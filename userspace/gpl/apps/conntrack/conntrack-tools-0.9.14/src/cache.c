/*
 * (C) 2006-2009 by Pablo Neira Ayuso <pablo@netfilter.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "cache.h"
#include "jhash.h"
#include "hash.h"
#include "log.h"
#include "conntrackd.h"

#include <libnetfilter_conntrack/libnetfilter_conntrack.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint32_t
__hash4(const struct nf_conntrack *ct, const struct hashtable *table)
{
	uint32_t a[4] = {
		[0]	= nfct_get_attr_u32(ct, ATTR_IPV4_SRC),
		[1]	= nfct_get_attr_u32(ct, ATTR_IPV4_DST),
		[2]	= nfct_get_attr_u8(ct, ATTR_L3PROTO) << 16 |
			  nfct_get_attr_u8(ct, ATTR_L4PROTO),
		[3]	= nfct_get_attr_u16(ct, ATTR_PORT_SRC) << 16 |
			  nfct_get_attr_u16(ct, ATTR_PORT_DST),
	};

	/*
	 * Instead of returning hash % table->hashsize (implying a divide)
	 * we return the high 32 bits of the (hash * table->hashsize) that will
	 * give results between [0 and hashsize-1] and same hash distribution,
	 * but using a multiply, less expensive than a divide. See:
	 * http://www.mail-archive.com/netdev@vger.kernel.org/msg56623.html
	 */
	return ((uint64_t)jhash2(a, 4, 0) * table->hashsize) >> 32;
}

static uint32_t
__hash6(const struct nf_conntrack *ct, const struct hashtable *table)
{
	uint32_t a[10];

	memcpy(&a[0], nfct_get_attr(ct, ATTR_IPV6_SRC), sizeof(uint32_t)*4);
	memcpy(&a[4], nfct_get_attr(ct, ATTR_IPV6_SRC), sizeof(uint32_t)*4);
	a[8] = nfct_get_attr_u8(ct, ATTR_ORIG_L3PROTO) << 16 |
	       nfct_get_attr_u8(ct, ATTR_ORIG_L4PROTO);
	a[9] = nfct_get_attr_u16(ct, ATTR_ORIG_PORT_SRC) << 16 |
	       nfct_get_attr_u16(ct, ATTR_ORIG_PORT_DST);

	return ((uint64_t)jhash2(a, 10, 0) * table->hashsize) >> 32;
}

static uint32_t hash(const void *data, const struct hashtable *table)
{
	int ret = 0;
	const struct nf_conntrack *ct = data;

	switch(nfct_get_attr_u8(ct, ATTR_L3PROTO)) {
		case AF_INET:
			ret = __hash4(ct, table);
			break;
		case AF_INET6:
			ret = __hash6(ct, table);
			break;
		default:
			dlog(LOG_ERR, "unknown layer 3 proto in hash");
			break;
	}

	return ret;
}

static int compare(const void *data1, const void *data2)
{
	const struct cache_object *obj = data1;
	const struct nf_conntrack *ct = data2;

	return nfct_cmp(obj->ct, ct, NFCT_CMP_ORIG) &&
	       nfct_get_attr_u32(obj->ct, ATTR_ID) ==
	       nfct_get_attr_u32(ct, ATTR_ID);
}

struct cache_feature *cache_feature[CACHE_MAX_FEATURE] = {
	[TIMER_FEATURE]		= &timer_feature,
};

struct cache *cache_create(const char *name, 
			   unsigned int features, 
			   struct cache_extra *extra)
{
	size_t size = sizeof(struct cache_object);
	int i, j = 0;
	struct cache *c;
	struct cache_feature *feature_array[CACHE_MAX_FEATURE] = {};
	unsigned int feature_offset[CACHE_MAX_FEATURE] = {};
	unsigned int feature_type[CACHE_MAX_FEATURE] = {};

	c = malloc(sizeof(struct cache));
	if (!c)
		return NULL;
	memset(c, 0, sizeof(struct cache));

	strcpy(c->name, name);

	for (i = 0; i < CACHE_MAX_FEATURE; i++) {
		if ((1 << i) & features) {
			feature_array[j] = cache_feature[i];
			feature_offset[j] = size;
			feature_type[i] = j;
			size += cache_feature[i]->size;
			j++;
		}
	}

	memcpy(c->feature_type, feature_type, sizeof(feature_type));

	c->features = malloc(sizeof(struct cache_feature) * j);
	if (!c->features) {
		free(c);
		return NULL;
	}
	memcpy(c->features, feature_array, sizeof(struct cache_feature) * j);
	c->num_features = j;

	c->extra_offset = size;
	c->extra = extra;
	if (extra)
		size += extra->size;

	c->feature_offset = malloc(sizeof(unsigned int) * j);
	if (!c->feature_offset) {
		free(c->features);
		free(c);
		return NULL;
	}
	memcpy(c->feature_offset, feature_offset, sizeof(unsigned int) * j);

	c->h = hashtable_create(CONFIG(hashsize),
				CONFIG(limit),
				hash,
				compare);

	if (!c->h) {
		free(c->features);
		free(c->feature_offset);
		free(c);
		return NULL;
	}
	c->object_size = size;

	return c;
}

void cache_destroy(struct cache *c)
{
	cache_flush(c);
	hashtable_destroy(c->h);
	free(c->features);
	free(c->feature_offset);
	free(c);
}

struct cache_object *cache_object_new(struct cache *c, struct nf_conntrack *ct)
{
	struct cache_object *obj;

	obj = calloc(c->object_size, 1);
	if (obj == NULL) {
		errno = ENOMEM;
		c->stats.add_fail_enomem++;
		return NULL;
	}
	obj->cache = c;

	if ((obj->ct = nfct_new()) == NULL) {
		free(obj);
		errno = ENOMEM;
		c->stats.add_fail_enomem++;
		return NULL;
	}
	memcpy(obj->ct, ct, nfct_sizeof(ct));
	obj->status = C_OBJ_NONE;
	c->stats.objects++;

	return obj;
}

void cache_object_free(struct cache_object *obj)
{
	obj->cache->stats.objects--;
	nfct_destroy(obj->ct);
	free(obj);
}

int cache_object_put(struct cache_object *obj)
{
	if (--obj->refcnt == 0) {
		cache_del(obj->cache, obj);
		cache_object_free(obj);
		return 1;
	}
	return 0;
}

void cache_object_get(struct cache_object *obj)
{
	obj->refcnt++;
}

void cache_object_set_status(struct cache_object *obj, int status)
{
	if (status == C_OBJ_DEAD) {
		obj->cache->stats.del_ok++;
		obj->cache->stats.active--;
	}
	obj->status = status;
}

static int __add(struct cache *c, struct cache_object *obj, int id)
{
	int ret;
	unsigned int i;
	char *data = obj->data;

	ret = hashtable_add(c->h, &obj->hashnode, id);
	if (ret == -1)
		return -1;

	for (i = 0; i < c->num_features; i++) {
		c->features[i]->add(obj, data);
		data += c->features[i]->size;
	}

	if (c->extra && c->extra->add)
		c->extra->add(obj, ((char *) obj) + c->extra_offset);

	c->stats.active++;
	obj->lifetime = obj->lastupdate = time_cached();
	obj->status = C_OBJ_NEW;
	obj->refcnt++;
	return 0;
}

int cache_add(struct cache *c, struct cache_object *obj, int id)
{
	int ret;

	ret = __add(c, obj, id);
	if (ret == -1) {
		c->stats.add_fail++;
		if (errno == ENOSPC)
			c->stats.add_fail_enospc++;
		return -1;
	}
	c->stats.add_ok++;
	return 0;
}

void cache_update(struct cache *c, struct cache_object *obj, int id,
		  struct nf_conntrack *ct)
{
	char *data = obj->data;
	unsigned int i;

	nfct_copy(obj->ct, ct, NFCT_CP_META);

	for (i = 0; i < c->num_features; i++) {
		c->features[i]->update(obj, data);
		data += c->features[i]->size;
	}

	if (c->extra && c->extra->update)
		c->extra->update(obj, ((char *) obj) + c->extra_offset);

	c->stats.upd_ok++;
	obj->lastupdate = time_cached();
	obj->status = C_OBJ_ALIVE;
}

static void __del(struct cache *c, struct cache_object *obj)
{
	unsigned i;
	char *data = obj->data;

	for (i = 0; i < c->num_features; i++) {
		c->features[i]->destroy(obj, data);
		data += c->features[i]->size;
	}

	if (c->extra && c->extra->destroy)
		c->extra->destroy(obj, ((char *) obj) + c->extra_offset);

	hashtable_del(c->h, &obj->hashnode);
}

void cache_del(struct cache *c, struct cache_object *obj)
{
	/*
	 * Do not increase stats if we are trying to
	 * kill an entry was previously deleted via
	 * __cache_del_timer.
	 */
	if (obj->status != C_OBJ_DEAD) {
		c->stats.del_ok++;
		c->stats.active--;
	}
	__del(c, obj);
}

struct cache_object *
cache_update_force(struct cache *c, struct nf_conntrack *ct)
{
	struct cache_object *obj;
	int id;

	obj = cache_find(c, ct, &id);
	if (obj) {
		if (obj->status != C_OBJ_DEAD) {
			cache_update(c, obj, id, ct);
			return obj;
		} else {
			cache_del(c, obj);
			cache_object_free(obj);
		}
	}
	obj = cache_object_new(c, ct);
	if (obj == NULL)
		return NULL;

	if (cache_add(c, obj, id) == -1) {
		cache_object_free(obj);
		return NULL;
	}

	return obj;
}

struct cache_object *
cache_find(struct cache *c, struct nf_conntrack *ct, int *id)
{
	*id = hashtable_hash(c->h, ct);
	return ((struct cache_object *) hashtable_find(c->h, ct, *id));
}

struct cache_object *cache_data_get_object(struct cache *c, void *data)
{
	return (struct cache_object *)((char*)data - c->extra_offset);
}

void *cache_get_extra(struct cache *c, void *data)
{
	return (char*)data + c->extra_offset;
}

void cache_stats(const struct cache *c, int fd)
{
	char buf[512];
	int size;

	size = sprintf(buf, "cache %s:\n"
			    "current active connections:\t%12u\n"
			    "connections created:\t\t%12u\tfailed:\t%12u\n"
			    "connections updated:\t\t%12u\tfailed:\t%12u\n"
			    "connections destroyed:\t\t%12u\tfailed:\t%12u\n\n",
			    			 c->name,
						 c->stats.active,
			    			 c->stats.add_ok, 
			    			 c->stats.add_fail,
						 c->stats.upd_ok,
						 c->stats.upd_fail,
						 c->stats.del_ok,
						 c->stats.del_fail);
	send(fd, buf, size, 0);
}

void cache_stats_extended(const struct cache *c, int fd)
{
	char buf[512];
	int size;

	size = snprintf(buf, sizeof(buf),
			    "cache:%s\tactive objects:\t\t%12u\n"
			    "\tactive/total entries:\t\t%12u/%12u\n"
			    "\tcreation OK/failed:\t\t%12u/%12u\n"
			    "\t\tno memory available:\t%12u\n"
			    "\t\tno space left in cache:\t%12u\n"
			    "\tupdate OK/failed:\t\t%12u/%12u\n"
			    "\t\tentry not found:\t%12u\n"
			    "\tdeletion created/failed:\t%12u/%12u\n"
			    "\t\tentry not found:\t%12u\n\n",
			    c->name, c->stats.objects,
			    c->stats.active, hashtable_counter(c->h),
			    c->stats.add_ok,
			    c->stats.add_fail,
			    c->stats.add_fail_enomem,
			    c->stats.add_fail_enospc,
			    c->stats.upd_ok,
			    c->stats.upd_fail,
			    c->stats.upd_fail_enoent,
			    c->stats.del_ok,
			    c->stats.del_fail,
			    c->stats.del_fail_enoent);

	send(fd, buf, size, 0);
}

void cache_iterate(struct cache *c, 
		   void *data, 
		   int (*iterate)(void *data1, void *data2))
{
	hashtable_iterate(c->h, data, iterate);
}

void cache_iterate_limit(struct cache *c, void *data,
			 uint32_t from, uint32_t steps,
			 int (*iterate)(void *data1, void *data2))
{
	hashtable_iterate_limit(c->h, data, from, steps, iterate);
}
