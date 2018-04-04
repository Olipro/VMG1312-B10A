/*
 * (C) 2006-2008 by Pablo Neira Ayuso <pablo@netfilter.org>
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
#include <stdlib.h>
#include <string.h>
#include "fds.h"

struct fds *create_fds(void)
{
	struct fds *fds;

	fds = (struct fds *) calloc(sizeof(struct fds), 1);
	if (fds == NULL)
		return NULL;

	INIT_LIST_HEAD(&fds->list);

	return fds;
}

void destroy_fds(struct fds *fds)
{
	struct fds_item *this, *tmp;

	list_for_each_entry_safe(this, tmp, &fds->list, head) {
		list_del(&this->head);
		FD_CLR(this->fd, &fds->readfds);
		free(this);
	}
	free(fds);
}

int register_fd(int fd, struct fds *fds)
{
	struct fds_item *item;
	
	FD_SET(fd, &fds->readfds);

	if (fd > fds->maxfd)
		fds->maxfd = fd;

	item = calloc(sizeof(struct fds_item), 1);
	if (item == NULL)
		return -1;

	item->fd = fd;
	list_add(&item->head, &fds->list);

	return 0;
}

int unregister_fd(int fd, struct fds *fds)
{
	int found = 0, maxfd = -1;
	struct fds_item *this, *tmp;

	list_for_each_entry_safe(this, tmp, &fds->list, head) {
		if (this->fd == fd) {
			list_del(&this->head);
			FD_CLR(this->fd, &fds->readfds);
			free(this);
			found = 1;
			/* ... and recalculate maxfd, see below. */
		}
	}
	/* not found, report an error. */
	if (!found)
		return -1;

	/* calculate the new maximum fd. */
	list_for_each_entry(this, &fds->list, head) {
		if (maxfd < this->fd) {
			maxfd = this->fd;
		}
	}
	fds->maxfd = maxfd;

	return 0;
}

