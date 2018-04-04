/************************************************************************
 *
 *      Copyright (C) 2004-2008 ZyXEL Communications, Corp.
 *      All Rights Reserved.
 *
 * ZyXEL Confidential; Need to Know only.
 * Protected as an unpublished work.
 *
 * The computer program listings, specifications and documentation
 * herein are the property of ZyXEL Communications, Corp. and shall
 * not be reproduced, copied, disclosed, or used in whole or in part
 * for any reason without the prior express written permission of
 * ZyXEL Communications, Corp.
 *
 *************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "zylist.h"

/*---------------------------------------------------------------------------------------------*/
/* added by bennewit@cs.tu-berlin.de */
#define list_iterator_has_elem( it ) ( 0 != (it).actual && (it).pos < (it).li->nb_elt )

/*---------------------------------------------------------------------------------------------*/
int list_init (list_t * li)
{
	if (li == NULL)
		return -1;
	
	memset (li, 0, sizeof (list_t));
	return 0 ; 
}

void list_special_free (list_t * li, void *(*free_func) (void *))
{
  int pos = 0;
  void *element;

  if (li == NULL)
    return;
  while (!list_eol (li, pos))
    {
      element = (void *) list_get (li, pos);
      list_remove (li, pos);
      if (free_func != NULL)
        free_func (element);
    }

	if(NULL != li)
		free(li) ;
  
}

void list_ofchar_free (list_t * li)
{
  int pos = 0;
  char *chain;

  if (li == NULL)
    return;
  while (!list_eol (li, pos))
    {
      chain = (char *) list_get (li, pos);
      list_remove (li, pos);
		if(NULL != chain)
			free(chain) ;
    }

  if(NULL != li)
  	free(li) ;
}

int list_size (const list_t * li)
{
  /* 
     Robin Nayathodan <roooot@softhome.net> 
     N.K Electronics INDIA

     NULL Checks  
   */

  if (li != NULL)
    return li->nb_elt;
  else
    return -1;
}

int list_eol (const list_t * li, int i)
{
	if (li == NULL)
		return -1;

	/* not end of list */
	if (i < li->nb_elt)
		return 0;                  

	/* end of list */
	return 1;                    
}

/* index starts from 0; */
int list_add (list_t * li, void *el, int pos)
{
	int i = 0 ;
	__node_t *ntmp ;
	__node_t *nextnode ;

	if(li == NULL)
		return -1;

	if (li->nb_elt == 0)
	{
		li->node = (__node_t *)malloc(sizeof(__node_t));
	  	if(NULL != li->node)
			memset(li->node, 0, sizeof(__node_t)) ;
	  
      if (li->node == NULL)
        return -1;
	  
      li->node->element = el;
      li->node->next = NULL;
      li->nb_elt++;
	  
      return li->nb_elt;
    }

	if (pos == -1 || pos >= li->nb_elt)
	{       
		/* insert at the end  */
		pos = li->nb_elt;
	}

	ntmp = li->node;              /* exist because nb_elt>0  */

	if (pos == 0)                 /* pos = 0 insert before first elt  */
    {
		li->node = (__node_t *)malloc(sizeof(__node_t));
		if (li->node == NULL)
		{
			/* leave the list unchanged */
			li->node = ntmp;
			return -1;
		}
		else
			memset(li->node, 0, sizeof(__node_t)) ;
		
      li->node->element = el;
      li->node->next = ntmp;
      li->nb_elt++;
      return li->nb_elt;
    }


	while(pos > i + 1)
    {
		i++;
		/* when pos>i next node exist  */
		ntmp = (__node_t *)ntmp->next ;
	}

	/* if pos==nb_elt next node does not exist  */
	if (pos == li->nb_elt)
    {
		ntmp->next = (__node_t *)malloc(sizeof(__node_t));
		if (ntmp->next == NULL)
			return -1;              /* leave the list unchanged */
		else
			memset(ntmp->next, 0, sizeof(__node_t)) ;

	  ntmp = (__node_t *) ntmp->next;
      ntmp->element = el;
      ntmp->next = NULL;
      li->nb_elt++;
      return li->nb_elt;
    }

	/* here pos==i so next node is where we want to insert new node */
	nextnode = (__node_t *)ntmp->next ;
    ntmp->next = (__node_t *)malloc(sizeof(__node_t));
	if (ntmp->next == NULL)
	{
		/* leave the list unchanged */
		ntmp->next = nextnode;
		return -1;
	}
	else
		memset(ntmp->next, 0, sizeof(__node_t)) ;
	
    ntmp = (__node_t *) ntmp->next;
    ntmp->element = el;
    ntmp->next = nextnode;
    li->nb_elt++;
  
	return li->nb_elt;
}

/* index starts from 0 */
void *list_get (const list_t * li, int pos)
{
	int i = 0 ;
  	__node_t *ntmp;

	if (li == NULL)
		return NULL;

	/* element does not exist */
	if (pos < 0 || pos >= li->nb_elt)
		return NULL;

	/* exist because nb_elt>0 */
	ntmp = li->node ;
	while (pos > i)
	{
		i++;
		ntmp = (__node_t *)ntmp->next ;
	}

	return ntmp->element;
}

/* added by bennewit@cs.tu-berlin.de */
void *list_get_first (list_t * li, list_iterator_t * iterator)
{

	if (0 >= li->nb_elt)
	{
		iterator->actual = 0;
		return 0;
	}

	iterator->actual = li->node;
	iterator->prev = &li->node;
	iterator->li = li;
	iterator->pos = 0;

	return li->node->element;
}

/* added by bennewit@cs.tu-berlin.de */
void *list_get_next (list_iterator_t * iterator)
{
	iterator->prev = (__node_t **) & (iterator->actual->next);
	iterator->actual = iterator->actual->next;
	++(iterator->pos);

	if(list_iterator_has_elem (*iterator))
	{
		return iterator->actual->element;
	}

	iterator->actual = 0;
	return 0;
}

/* added by bennewit@cs.tu-berlin.de */
void *list_iterator_remove (list_iterator_t * iterator)
{

	if (list_iterator_has_elem (*iterator))
	{
		--(iterator->li->nb_elt) ;
		*(iterator->prev) = iterator->actual->next;

		if(NULL != iterator->actual)
			free(iterator->actual) ;

		iterator->actual = *(iterator->prev);
	}

	if(list_iterator_has_elem (*iterator))
	{
		return iterator->actual->element;
	}

	return 0;
}

/* return -1 if failed */
int list_remove (list_t * li, int pos)
{

	int i = 0 ;
	__node_t *ntmp ;
	__node_t *remnode ;

	if (li == NULL)
		return -1;

	/* element does not exist */
	if(pos < 0 || pos >= li->nb_elt)
		return -1;

	/* exist because nb_elt>0 */
	ntmp = li->node; 

	/* special case  */
	if((pos == 0))
    {   
		li->node = (__node_t *) ntmp->next;
		li->nb_elt--;

		if(NULL != ntmp)
			free(ntmp) ;
		return li->nb_elt;
	}

	while(pos > i + 1)
	{
		i++;
		ntmp = (__node_t *) ntmp->next;
	}

	/* insert new node */
	remnode = (__node_t *) ntmp->next;
	ntmp->next = ((__node_t *) ntmp->next)->next;

	if(NULL != remnode)
		free(remnode) ;

	li->nb_elt--;
	
	return li->nb_elt;
}

