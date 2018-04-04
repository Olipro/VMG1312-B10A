/*
 * Common vendor IE list functions.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_bsscfg.c,v 1.240.8.41.2.5 2010-11-19 17:49:01 $
 */

/* common includes */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>

/* include wlioctl.h */
#include <siutils.h>
#include <proto/802.11.h>
#include <proto/wpa.h>
#include <wlioctl.h>	/* required for vndr_ie_info_t */

/* include wlc.h */
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc.h>	/* required for wlc_write_info_elt_safe */

#include <wlc_vndr_ie_list.h>

int
wlc_vndr_ie_list_getlen_ext(const vndr_ie_listel_t *vndr_ie_listp, vndr_ie_list_filter_fn_t filter,
	void *arg, uint32 pktflag, int *totie)
{
	const vndr_ie_listel_t *curr;
	int ie_count = 0;
	int tot_ielen = 0;

	for (curr = (const vndr_ie_listel_t *)vndr_ie_listp; curr != NULL; curr = curr->next_el) {
		if ((curr->vndr_ie_infoel.pktflag & pktflag) &&
		    (filter == NULL ||
		     (filter)(arg, &curr->vndr_ie_infoel.vndr_ie_data))) {
			ie_count++;
			tot_ielen += curr->vndr_ie_infoel.vndr_ie_data.len + VNDR_IE_HDR_LEN;
		}
	}

	if (totie) {
		*totie = ie_count;
	}

	return tot_ielen;
}

uint8 *
wlc_vndr_ie_list_write_ext(const vndr_ie_listel_t *vndr_ie_listp,
	vndr_ie_list_write_filter_fn_t filter, void *arg, uint type, uint8 *cp,
	int buflen, uint32 pktflag)
{
	const vndr_ie_listel_t *curr;
	uint8 *old_cp;

	for (curr = (const vndr_ie_listel_t *)vndr_ie_listp; curr != NULL; curr = curr->next_el) {
		if (curr->vndr_ie_infoel.pktflag & pktflag) {
			old_cp = cp;
			if (curr->vndr_ie_infoel.pktflag & VNDR_IE_CUSTOM_FLAG) {
				cp = wlc_write_info_elt_safe(cp, buflen,
					curr->vndr_ie_infoel.vndr_ie_data.id,
					curr->vndr_ie_infoel.vndr_ie_data.len,
					&curr->vndr_ie_infoel.vndr_ie_data.oui[0]);
			}
			else if (filter == NULL ||
			         (filter)(arg, type, &curr->vndr_ie_infoel.vndr_ie_data)) {
				cp = wlc_write_info_elt_safe(cp, buflen,
					DOT11_MNG_PROPR_ID,
					curr->vndr_ie_infoel.vndr_ie_data.len,
					&curr->vndr_ie_infoel.vndr_ie_data.oui[0]);
			}
			buflen -= (int)(cp - old_cp);
		}
	}

	return cp;
}

/*
 * Create a vendor IE information element object and add to the list.
 * Return value: address of the new object.
 */
vndr_ie_listel_t *
wlc_vndr_ie_list_add_elem(wlc_info_t *wlc, vndr_ie_listel_t **vndr_ie_listp,
	uint32 pktflag, vndr_ie_t *vndr_iep)
{
	vndr_ie_listel_t *new_list_el;
	int info_len, ie_len;
	vndr_ie_listel_t *last;

	ie_len = (int) (vndr_iep->len + VNDR_IE_HDR_LEN);
	info_len = (int) (VNDR_IE_INFO_HDR_LEN + ie_len);

	if ((new_list_el = (vndr_ie_listel_t *) MALLOC(wlc->osh, info_len +
		VNDR_IE_EL_HDR_LEN)) == NULL) {
		WL_ERROR(("wl%d: wlc_vndr_ie_add_elem: out of memory\n", wlc->pub->unit));
		return NULL;
	}

	new_list_el->vndr_ie_infoel.pktflag = pktflag;
	bcopy((char *)vndr_iep, (char *)&new_list_el->vndr_ie_infoel.vndr_ie_data, ie_len);

	/* Add to the tail of the list */
	for (last = *vndr_ie_listp;
	     last != NULL && last->next_el != NULL;
	     last = last->next_el)
		;
	new_list_el->next_el = NULL;
	if (last != NULL)
		last->next_el = new_list_el;
	else
		*vndr_ie_listp = new_list_el;

	return (new_list_el);
}

int
wlc_vndr_ie_list_add(wlc_info_t *wlc, vndr_ie_listel_t **vndr_ie_listp,
	const vndr_ie_buf_t *ie_buf, int len)
{
	vndr_ie_info_t *ie_info;
	vndr_ie_t *vndr_iep;
	int info_len;
	int totie, ieindex;
	uint32 currflag;
	char *bufaddr;

	bcopy(&ie_buf->iecount, &totie, sizeof(int));
	bufaddr = (char *) &ie_buf->vndr_ie_list;

	for (ieindex = 0; ieindex < totie; ieindex++, bufaddr += info_len) {
		ie_info = (vndr_ie_info_t *) bufaddr;
		bcopy((char *)&ie_info->pktflag, (char *)&currflag, (int) sizeof(uint32));

		vndr_iep = &ie_info->vndr_ie_data;

		if (!(ie_info->pktflag & VNDR_IE_CUSTOM_FLAG))
			vndr_iep->id = DOT11_MNG_PROPR_ID;

		info_len = (int) (VNDR_IE_INFO_HDR_LEN + VNDR_IE_HDR_LEN + vndr_iep->len);
		if (wlc_vndr_ie_list_add_elem(wlc, vndr_ie_listp, currflag, vndr_iep) == NULL) {
			return BCME_NORESOURCE;
		}
	}

	return 0;
}

int
wlc_vndr_ie_list_del(wlc_info_t *wlc, vndr_ie_listel_t **vndr_ie_listp,
	const vndr_ie_buf_t *ie_buf, int len)
{
	vndr_ie_listel_t *prev_list_el;
	vndr_ie_listel_t *curr_list_el;
	vndr_ie_listel_t *next_list_el;
	vndr_ie_info_t *ie_info;
	vndr_ie_t *vndr_iep;
	int ie_len, info_len;
	int totie, ieindex;
	uint32 currflag;
	char *bufaddr;
	bool found;
	int err = 0;

	bcopy(&ie_buf->iecount, &totie, sizeof(int));
	bufaddr = (char *) &ie_buf->vndr_ie_list;

	for (ieindex = 0; ieindex < totie; ieindex++, bufaddr += info_len) {
		ie_info = (vndr_ie_info_t *) bufaddr;
		bcopy((char *)&ie_info->pktflag, (char *)&currflag, (int) sizeof(uint32));

		vndr_iep = &ie_info->vndr_ie_data;
		if (!(ie_info->pktflag & VNDR_IE_CUSTOM_FLAG))
			vndr_iep->id = DOT11_MNG_PROPR_ID;

		ie_len = (int) (vndr_iep->len + VNDR_IE_HDR_LEN);
		info_len = (int) sizeof(uint32) + ie_len;

		curr_list_el = *vndr_ie_listp;
		prev_list_el = NULL;

		found = FALSE;

		while (curr_list_el != NULL) {
			next_list_el = curr_list_el->next_el;

			if (vndr_iep->len == curr_list_el->vndr_ie_infoel.vndr_ie_data.len) {
				if (!bcmp((char*)&curr_list_el->vndr_ie_infoel, (char*) ie_info,
					info_len)) {
					if (*vndr_ie_listp == curr_list_el) {
						*vndr_ie_listp = next_list_el;
					} else {
						prev_list_el->next_el = next_list_el;
					}
					MFREE(wlc->osh, curr_list_el, info_len +
					      VNDR_IE_EL_HDR_LEN);
					curr_list_el = NULL;
					found = TRUE;
					break;
				}
			}

			prev_list_el = curr_list_el;
			curr_list_el = next_list_el;
		}

		if (!found) {
			WL_ERROR(("wl%d: wlc_del_ie: IE not in list\n", wlc->pub->unit));
			err = BCME_NOTFOUND;
		}
	}

	return err;
}

void
wlc_vndr_ie_list_free(wlc_info_t *wlc, vndr_ie_listel_t **vndr_ie_listpp)
{
	vndr_ie_listel_t *curr_list_el;
	vndr_ie_listel_t *next_list_el;
	int freelen;

	/* nothing to do */
	if (vndr_ie_listpp == NULL)
		return;

	curr_list_el = *vndr_ie_listpp;

	while (curr_list_el != NULL) {
		next_list_el = curr_list_el->next_el;

		freelen =
			VNDR_IE_EL_HDR_LEN +
			sizeof(uint32) +
			VNDR_IE_HDR_LEN +
			curr_list_el->vndr_ie_infoel.vndr_ie_data.len;

		MFREE(wlc->osh, curr_list_el, freelen);
		curr_list_el = next_list_el;
	}

	*vndr_ie_listpp = NULL;
}

/* return the total length of the buffer when it >= 0
 * otherwise return value is error BCME_XXXX
 */
int
wlc_vndr_ie_buflen(const vndr_ie_buf_t *ie_buf, int len, int *bcn_ielen, int *prbrsp_ielen)
{
	int totie, ieindex;
	vndr_ie_info_t *ie_info;
	vndr_ie_t *vndr_iep;
	int ie_len, info_len;
	char *bufaddr;
	uint32 pktflag;
	int bcn_len, prbrsp_len, req_len;

	if (len < (int) sizeof(vndr_ie_buf_t) - 1) {
		return BCME_BUFTOOSHORT;
	}

	bcn_len = prbrsp_len = req_len = 0;

	bcopy(&ie_buf->iecount, &totie, (int) sizeof(int));

	bufaddr = (char *) &ie_buf->vndr_ie_list;
	len -= (int) sizeof(int);       /* reduce by the size of iecount */

	for (ieindex = 0; ieindex < totie; ieindex++) {
		if (len < (int) sizeof(vndr_ie_info_t) - 1) {
			return BCME_BUFTOOSHORT;
		}

		ie_info = (vndr_ie_info_t *) bufaddr;
		bcopy(&ie_info->pktflag, &pktflag, (int) sizeof(uint32));

		vndr_iep = &ie_info->vndr_ie_data;
		ie_len = (int) (vndr_iep->len + VNDR_IE_HDR_LEN);
		info_len = (int) sizeof(uint32) + ie_len;

		if (pktflag & VNDR_IE_BEACON_FLAG) {
			bcn_len += ie_len;
		}

		if (pktflag & VNDR_IE_PRBRSP_FLAG) {
			prbrsp_len += ie_len;
		}

		/* reduce the bufer length by the size of this vndr_ie_info */
		len -= info_len;

		/* point to the next vndr_ie_info */
		bufaddr += info_len;
	}

	if (len < 0) {
		return BCME_BUFTOOSHORT;
	}

	if (bcn_ielen) {
		*bcn_ielen = bcn_len;
	}

	if (prbrsp_ielen) {
		*prbrsp_ielen = prbrsp_len;
	}

	return (int)((uint8 *)bufaddr - (const uint8 *)ie_buf);
}

int
wlc_vndr_ie_list_set(wlc_info_t *wlc, const void *vndr_ie_setbuf,
	int vndr_ie_setbuf_len, vndr_ie_listel_t **vndr_ie_listp,
	bool *bcn_upd, bool *prbresp_upd)
{
	int err = BCME_OK;
	int rem = vndr_ie_setbuf_len;
	vndr_ie_setbuf_t *vndr_ie_bufp;
	int buf_len;
	int bcn_len, prbresp_len;

	/* Make a memory-aligned bcopy of the arguments in case they start
	 * on an unaligned address due to a "bsscfg:N" prefix.
	*/
	if (vndr_ie_setbuf_len < (int)sizeof(vndr_ie_setbuf_t) - 1)
		return BCME_BUFTOOSHORT;

	if (!(vndr_ie_bufp = MALLOC(wlc->osh, vndr_ie_setbuf_len))) {
		WL_ERROR(("wl%d: out of memory, malloced %d bytes\n",
			wlc->pub->unit, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}
	bcopy((const uint8*)vndr_ie_setbuf, vndr_ie_bufp, vndr_ie_setbuf_len);

	do {
		if ((buf_len = wlc_vndr_ie_buflen(&vndr_ie_bufp->vndr_ie_buffer,
			rem - VNDR_IE_CMD_LEN, &bcn_len, &prbresp_len)) < 0) {
			err = buf_len;
			break;
		}

		if (rem < VNDR_IE_CMD_LEN + buf_len) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		if (NULL != bcn_upd && bcn_len > 0)
			*bcn_upd = TRUE;
		if (NULL != prbresp_upd && prbresp_len > 0)
			*prbresp_upd = TRUE;

		if (!strcmp(vndr_ie_bufp->cmd, "add")) {
			err = wlc_vndr_ie_list_add(wlc, vndr_ie_listp,
				&vndr_ie_bufp->vndr_ie_buffer, rem - VNDR_IE_CMD_LEN);
		}
		else if (!strcmp(vndr_ie_bufp->cmd, "del")) {
			err = wlc_vndr_ie_list_del(wlc, vndr_ie_listp,
				&vndr_ie_bufp->vndr_ie_buffer, rem - VNDR_IE_CMD_LEN);
		}
		else {
			err = BCME_BADARG;
		}
		if ((err != BCME_OK))
			break;

		rem -= VNDR_IE_CMD_LEN + buf_len;
		if (rem == 0)
			break;

		memmove((uint8 *)vndr_ie_bufp,
			(uint8 *)vndr_ie_bufp + VNDR_IE_CMD_LEN + buf_len, rem);

		if ((rem < (int) sizeof(vndr_ie_setbuf_t) - 1) ||
			(strcmp(vndr_ie_bufp->cmd, "add") &&
			strcmp(vndr_ie_bufp->cmd, "del"))) {
			break;
		}

	}
	while (TRUE);

	if (vndr_ie_bufp != NULL)
		MFREE(wlc->osh, vndr_ie_bufp, vndr_ie_setbuf_len);

	return err;
}

int
wlc_vndr_ie_list_get(wlc_info_t *wlc, const vndr_ie_listel_t *vndr_ie_listp,
	vndr_ie_buf_t *ie_buf, int len, uint32 pktflag)
{
	int copylen;
	int totie;
	const vndr_ie_listel_t *curr_list_el;
	vndr_ie_info_t *ie_info;
	const vndr_ie_t *vndr_iep;
	char *bufaddr;
	int ie_len, info_len;

	BCM_REFERENCE(wlc);
	pktflag &= (~(VNDR_IE_CUSTOM_FLAG));
	/* Vendor IE data */
	copylen = wlc_vndr_ie_list_getlen(vndr_ie_listp, pktflag, &totie);

	if (totie != 0) {
		/* iecount */
		copylen += (int) sizeof(int);

		/* pktflag for each vndr_ie_info struct */
		copylen += (int) sizeof(uint32) * totie;
	} else {
		copylen = (int) sizeof(vndr_ie_buf_t) - sizeof(vndr_ie_info_t);
	}
	if (len < copylen) {
		WL_ERROR(("wl%d: %s: buf too small (copylen=%d, buflen=%d)\n",
			wlc->pub->unit, __FUNCTION__, copylen, len));
		/* Store the required buffer size value in the buffer provided */
		bcopy((char *) &copylen, (char *)ie_buf, sizeof(int));
		return BCME_BUFTOOSHORT;
	}

	bcopy(&totie, &ie_buf->iecount, sizeof(int));

	if (totie == 0)
		return BCME_OK;

	bufaddr = (char *) &ie_buf->vndr_ie_list;

	curr_list_el = (const vndr_ie_listel_t *)vndr_ie_listp;

	while (curr_list_el != NULL) {
		if (curr_list_el->vndr_ie_infoel.pktflag & pktflag) {
			ie_info = (vndr_ie_info_t *) bufaddr;
			vndr_iep = &curr_list_el->vndr_ie_infoel.vndr_ie_data;

			ie_len = (int) (vndr_iep->len + VNDR_IE_HDR_LEN);
			info_len = (int) sizeof(uint32) + ie_len;

			bcopy((const char*)&curr_list_el->vndr_ie_infoel, (char*)ie_info, info_len);

			/* point to the next vndr_ie_info */
			bufaddr += info_len;
		}
		curr_list_el = curr_list_el->next_el;
	}

	return BCME_OK;
}

/*
 * Modify the data in the previously added vendor IE info.
 */
vndr_ie_listel_t *
wlc_vndr_ie_list_mod_elem(wlc_info_t *wlc, vndr_ie_listel_t **vndr_ie_listp,
	vndr_ie_listel_t *old_listel, uint32 pktflag, vndr_ie_t *vndr_iep)
{
	vndr_ie_listel_t *curr_list_el, *prev_list_el;

	curr_list_el = *vndr_ie_listp;
	prev_list_el = NULL;

	while (curr_list_el != NULL) {
		/* found list element, update the vendor info */
		if (curr_list_el == old_listel) {
			/* reuse buffer if length of current elem is same as the new */
			if (curr_list_el->vndr_ie_infoel.vndr_ie_data.len == vndr_iep->len) {
				curr_list_el->vndr_ie_infoel.pktflag = pktflag;
				bcopy((char *)vndr_iep,
				      (char *)&curr_list_el->vndr_ie_infoel.vndr_ie_data,
				      (vndr_iep->len + VNDR_IE_HDR_LEN));

				return (curr_list_el);
			} else {
				/* Delete the old one from the list and free it */
				if (*vndr_ie_listp == curr_list_el) {
					*vndr_ie_listp = curr_list_el->next_el;
				} else {
					prev_list_el->next_el = curr_list_el->next_el;
				}
				MFREE(wlc->osh, curr_list_el,
				      (curr_list_el->vndr_ie_infoel.vndr_ie_data.len +
				       VNDR_IE_HDR_LEN + VNDR_IE_INFO_HDR_LEN +
				       VNDR_IE_EL_HDR_LEN));

				/* Add a new elem to the list */
				return wlc_vndr_ie_list_add_elem(wlc, vndr_ie_listp,
					pktflag, vndr_iep);
			}
		}

		prev_list_el = curr_list_el;
		curr_list_el = curr_list_el->next_el;
	}

	/* Should not come here */
	ASSERT(0);

	return 0;
}

int
wlc_vndr_ie_list_mod_elem_by_type(wlc_info_t *wlc, vndr_ie_listel_t **vndr_ie_listp,
	uint8 type, uint32 pktflag, vndr_ie_t *vndr_iep)
{
	vndr_ie_listel_t *curr_list_el, *prev_list_el;

	curr_list_el = *vndr_ie_listp;
	prev_list_el = NULL;

	while (curr_list_el != NULL) {
		/* found list element, update the IE */
		if (type == curr_list_el->vndr_ie_infoel.vndr_ie_data.id) {
			/* reuse buffer if length of current elem is same as the new */
			if (curr_list_el->vndr_ie_infoel.vndr_ie_data.len == vndr_iep->len) {
				curr_list_el->vndr_ie_infoel.pktflag = pktflag;
				bcopy((char *)vndr_iep,
					(char *)&curr_list_el->vndr_ie_infoel.vndr_ie_data,
					(vndr_iep->len + VNDR_IE_HDR_LEN));
				return BCME_OK;
			} else {
				/* Delete the old one from the list and free it */
				if (*vndr_ie_listp == curr_list_el) {
					*vndr_ie_listp = curr_list_el->next_el;
				} else {
					prev_list_el->next_el = curr_list_el->next_el;
				}
				MFREE(wlc->osh, curr_list_el,
					(curr_list_el->vndr_ie_infoel.vndr_ie_data.len +
					VNDR_IE_HDR_LEN + VNDR_IE_INFO_HDR_LEN +
					VNDR_IE_EL_HDR_LEN));
				break;
			}
		}
		prev_list_el = curr_list_el;
		curr_list_el = curr_list_el->next_el;
	}

	/* Add a new elem to the list */
	if (!wlc_vndr_ie_list_add_elem(wlc, vndr_ie_listp, pktflag, vndr_iep))
		return BCME_NOMEM;
	return BCME_OK;
}

int
wlc_vndr_ie_list_del_by_type(wlc_info_t *wlc, vndr_ie_listel_t **vndr_ie_listp, uint8 type)
{
	vndr_ie_listel_t *prev_list_el;
	vndr_ie_listel_t *curr_list_el;
	vndr_ie_listel_t *next_list_el;
	bool found = FALSE;
	int err = BCME_OK;

	curr_list_el = *vndr_ie_listp;
	prev_list_el = NULL;

	while (curr_list_el != NULL) {
		next_list_el = curr_list_el->next_el;
		if (type == curr_list_el->vndr_ie_infoel.vndr_ie_data.id) {
			if (*vndr_ie_listp == curr_list_el) {
				*vndr_ie_listp = next_list_el;

			} else {
				prev_list_el->next_el = next_list_el;
			}
			MFREE(wlc->osh, curr_list_el,
				(curr_list_el->vndr_ie_infoel.vndr_ie_data.len +
				VNDR_IE_HDR_LEN + VNDR_IE_INFO_HDR_LEN +
				VNDR_IE_EL_HDR_LEN));
			found = TRUE;
			break;
		}
		prev_list_el = curr_list_el;
		curr_list_el = next_list_el;
	}

	if (!found) {
		err = BCME_NOTFOUND;
	}

	return err;
}

uint8 *
wlc_vndr_ie_list_find_by_type(vndr_ie_listel_t *vndr_ie_listp, uint8 type)
{
	vndr_ie_listel_t *curr_list_el;

	curr_list_el = vndr_ie_listp;

	while (curr_list_el != NULL) {
		if (type == curr_list_el->vndr_ie_infoel.vndr_ie_data.id) {
			return (uint8 *)&curr_list_el->vndr_ie_infoel.vndr_ie_data;
		}
		curr_list_el = curr_list_el->next_el;
	}

	return NULL;
}
