/*
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id:	$
 */

#ifndef _WL_PLC_LINUX_H_
#define _WL_PLC_LINUX_H_
extern struct sk_buff *wl_plc_tx_prep(wl_if_t *wlif, struct sk_buff *skb);
extern void wl_plc_sendpkt(wl_if_t *wlif, struct sk_buff *skb, struct net_device *dev);
extern int32 wl_plc_recv(struct sk_buff *skb, struct net_device *dev, wl_plc_t *plc, uint16 if_in);
extern int32 wl_plc_init(wl_if_t *wlif);
extern void wl_plc_cleanup(wl_if_t *wlif);
#endif /* _WL_PLC_LINUX_H_ */
