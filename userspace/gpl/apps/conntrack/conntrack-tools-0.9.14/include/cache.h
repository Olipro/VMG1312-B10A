#ifndef _CACHE_H_
#define _CACHE_H_

#include <stdint.h>
#include <stddef.h>
#include "hash.h"
#include "date.h"

/* cache features */
enum {
	NO_FEATURES = 0,

	TIMER_FEATURE = 0,
	TIMER = (1 << TIMER_FEATURE),

	__CACHE_MAX_FEATURE
};
#define CACHE_MAX_FEATURE __CACHE_MAX_FEATURE

enum {
	C_OBJ_NONE = 0,		/* not in the cache */
	C_OBJ_NEW,		/* just added to the cache */
	C_OBJ_ALIVE,		/* in the cache, alive */
	C_OBJ_DEAD		/* still in the cache, but dead */
};

struct cache;
struct cache_object {
	struct	hashtable_node hashnode;
	struct	nf_conntrack *ct;
	struct	cache *cache;
	int	status;
	int	refcnt;
	long	lifetime;
	long	lastupdate;
	char	data[0];
};

struct cache_feature {
	size_t size;
	void (*add)(struct cache_object *obj, void *data);
	void (*update)(struct cache_object *obj, void *data);
	void (*destroy)(struct cache_object *obj, void *data);
	int  (*dump)(struct cache_object *obj, void *data, char *buf, int type);
};

extern struct cache_feature timer_feature;

#define CACHE_MAX_NAMELEN 32

struct cache {
	char name[CACHE_MAX_NAMELEN];
	struct hashtable *h;

	unsigned int num_features;
	struct cache_feature **features;
	unsigned int feature_type[CACHE_MAX_FEATURE];
	unsigned int *feature_offset;
	struct cache_extra *extra;
	unsigned int extra_offset;
	size_t object_size;

        /* statistics */
	struct {
		uint32_t	active;
	
		uint32_t	add_ok;
		uint32_t	del_ok;
		uint32_t	upd_ok;
		
		uint32_t	add_fail;
		uint32_t	del_fail;
		uint32_t	upd_fail;

		uint32_t	add_fail_enomem;
		uint32_t	add_fail_enospc;
		uint32_t	del_fail_enoent;
		uint32_t	upd_fail_enoent;

		uint32_t	commit_ok;
		uint32_t	commit_fail;

		uint32_t	flush;

		uint32_t	objects;
	} stats;
};

struct cache_extra {
	unsigned int size;

	void (*add)(struct cache_object *obj, void *data);
	void (*update)(struct cache_object *obj, void *data);
	void (*destroy)(struct cache_object *obj, void *data);
};

struct nf_conntrack;

struct cache *cache_create(const char *name, unsigned int features, struct cache_extra *extra);
void cache_destroy(struct cache *e);

struct cache_object *cache_object_new(struct cache *c, struct nf_conntrack *ct);
void cache_object_free(struct cache_object *obj);
void cache_object_get(struct cache_object *obj);
int cache_object_put(struct cache_object *obj);
void cache_object_set_status(struct cache_object *obj, int status);

int cache_add(struct cache *c, struct cache_object *obj, int id);
void cache_update(struct cache *c, struct cache_object *obj, int id, struct nf_conntrack *ct);
struct cache_object *cache_update_force(struct cache *c, struct nf_conntrack *ct);
void cache_del(struct cache *c, struct cache_object *obj);
struct cache_object *cache_find(struct cache *c, struct nf_conntrack *ct, int *pos);
void cache_stats(const struct cache *c, int fd);
void cache_stats_extended(const struct cache *c, int fd);
struct cache_object *cache_data_get_object(struct cache *c, void *data);
void *cache_get_extra(struct cache *, void *);
void cache_iterate(struct cache *c, void *data, int (*iterate)(void *data1, void *data2));
void cache_iterate_limit(struct cache *c, void *data, uint32_t from, uint32_t steps, int (*iterate)(void *data1, void *data2));

/* iterators */
struct nfct_handle;

void cache_dump(struct cache *c, int fd, int type);
void cache_commit(struct cache *c, struct nfct_handle *h, int clientfd);
void cache_flush(struct cache *c);
void cache_bulk(struct cache *c);

#endif
