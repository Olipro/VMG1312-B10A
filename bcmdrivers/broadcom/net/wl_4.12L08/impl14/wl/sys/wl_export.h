/*
 * Required functions exported by the port-specific (os-dependent) driver
 * to common (os-independent) driver code.
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
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
 * $Id: wl_export.h 356142 2012-09-11 09:54:49Z $
 */

#ifndef _wl_export_h_
#define _wl_export_h_

/* misc callbacks */
struct wl_info;
struct wl_if;
struct wlc_if;
extern void wl_init(struct wl_info *wl);
extern uint wl_reset(struct wl_info *wl);
extern void wl_intrson(struct wl_info *wl);
extern uint32 wl_intrsoff(struct wl_info *wl);
extern void wl_intrsrestore(struct wl_info *wl, uint32 macintmask);
extern void wl_event(struct wl_info *wl, char *ifname, wlc_event_t *e);
extern void wl_event_sync(struct wl_info *wl, char *ifname, wlc_event_t *e);
extern void wl_event_sendup(struct wl_info *wl, const wlc_event_t *e, uint8 *data, uint32 len);
extern int wl_up(struct wl_info *wl);
extern void wl_down(struct wl_info *wl);
extern void wl_dump_ver(struct wl_info *wl, struct bcmstrbuf *b);
extern void wl_txflowcontrol(struct wl_info *wl, struct wl_if *wlif, bool state, int prio);
extern bool wl_alloc_dma_resources(struct wl_info *wl, uint dmaddrwidth);
extern void wl_reclaim(void);
extern void wl_nocard_timer(void *arg);
extern void wl_recover_nocard(struct wl_info *wl);
#ifdef WLOFFLD
extern uint32 wl_pcie_bar1(struct wl_info *wl, uchar** addr);
#endif
/* timer functions */
struct wl_timer;
extern struct wl_timer *wl_init_timer(struct wl_info *wl, void (*fn)(void* arg), void *arg,
                                      const char *name);
extern void wl_free_timer(struct wl_info *wl, struct wl_timer *timer);
extern void wl_add_timer(struct wl_info *wl, struct wl_timer *timer, uint ms, int periodic);
extern bool wl_del_timer(struct wl_info *wl, struct wl_timer *timer);

/* proptxtstatus functions */
#ifdef PROP_TXSTATUS
struct wlfc_mac_desc_handle_map;
extern uint8 wlfc_allocate_MAC_descriptor_handle(struct wlfc_mac_desc_handle_map* map);
extern int wlfc_MAC_table_update(struct wl_info *wl, uint8* ea,
	uint8 add_del, uint8 mac_handle, uint8 ifidx);
extern void wlfc_release_MAC_descriptor_handle(struct wlfc_mac_desc_handle_map* map, uint8 handle);
extern int wlfc_psmode_request(struct wl_info *wl, uint8 mac_handle, uint8 credit,
	uint8 precedence_bitmap, uint8 request_type);
extern int wlfc_push_credit_data(struct wl_info *wl, void* p);
extern int wlfc_push_signal_data(struct wl_info *wl, void* data, uint8 len, bool hold);
extern int wlfc_sendup_ctl_info_now(struct wl_info *wl);
extern struct wlfc_info_state* wlfc_state_get(struct wl_info *wl);
#endif


/* data receive and interface management functions */
extern void wl_sendup(struct wl_info *wl, struct wl_if *wlif, void *p, int numpkt);
extern char *wl_ifname(struct wl_info *wl, struct wl_if *wlif);
extern struct wl_if *wl_add_if(struct wl_info *wl, struct wlc_if* wlcif, uint unit,
	struct ether_addr *remote);
extern void wl_del_if(struct wl_info *wl, struct wl_if *wlif);

/* pcie root complex operations
	op == 0: get link capability in configuration space
	op == 1: hot reset
*/
extern int wl_osl_pcie_rc(struct wl_info *wl, uint op, int param);

extern void wl_monitor(struct wl_info *wl, wl_rxsts_t *rxsts, void *p);
extern void wl_set_monitor(struct wl_info *wl, int val);
#ifdef WLTXMONITOR
extern void wl_tx_monitor(struct wl_info *wl, wl_txsts_t *txsts, void *p);
#endif

extern uint wl_buf_to_pktcopy(osl_t *osh, void *p, uchar *buf, int len, uint offset);
extern void * wl_get_pktbuffer(osl_t *osh, int len);
extern int wl_set_pktlen(osl_t *osh, void *p, int len);

/* contexts in wlif structure. Currently following are valid */
#define IFCTX_ARPI	(1)
#define IFCTX_NDI	(2)
#define IFCTX_NETDEV	(3)
extern void *wl_get_ifctx(struct wl_info *wl, int ctx_id, wl_if_t *wlif);


#if defined(MACOSX) && defined(WL_BSSLISTSORT)
extern bool wl_sort_bsslist(struct wl_info *wl, wlc_bss_info_t **bip);
#else
#define wl_sort_bsslist(a, b) FALSE
#endif

#ifdef LINUX_CRYPTO
extern int wl_tkip_miccheck(struct wl_info *wl, void *p, int hdr_len, bool group_key, int id);
extern int wl_tkip_micadd(struct wl_info *wl, void *p, int hdr_len);
extern int wl_tkip_encrypt(struct wl_info *wl, void *p, int hdr_len);
extern int wl_tkip_decrypt(struct wl_info *wl, void *p, int hdr_len, bool group_key);
extern void wl_tkip_printstats(struct wl_info *wl, bool group_key);
extern int wl_tkip_keyset(struct wl_info *wl, wsec_key_t *key);
#endif /* LINUX_CRYPTO */
#ifdef WL_WOWL_MEDIA
extern void wl_wowl_dngldown(struct wl_info *wl);
extern void wl_down_postwowlenab(struct wl_info *wl);
#endif
#if defined(NDIS) && (NDISVER == 0x0620)
extern int wl_fast_scan_enabled(struct wl_info *wl, void *p);
extern void wl_fast_scan_result_search(struct wl_info *wl, void *p);
#endif

#ifdef PLC_WET
struct wl_plc_node;
extern bool wl_plc_loop(struct wl_info *wl, void *p, struct wl_if *wlif);
extern struct wl_plc_node *wl_plc_node_find(struct wl_info *wl, uint8 *da);
extern int32 wl_plc_node_add(struct wl_info *wl, uint32 node_type, uint8 *ea,
                             uint8 *link_ea, struct wl_plc_node **node);
extern int32 wl_plc_node_delete(struct wl_info *wl, uint8 *ea, bool force);
extern void wl_plc_node_delete_all(struct wl_info *wl);
extern void wl_plc_node_list(struct wl_info *wl, struct wl_plc_nodelist *list);
extern int32 wl_plc_node_mac(struct wl_info *wl, struct wl_plc_node *node, uint8 *ea);
extern bool wl_plc_node_pref_plc(struct wl_info *wl, struct wl_plc_node *node);
extern void wl_plc_node_set_bsscfg(struct wl_info *wl, struct wl_plc_node *node,
                                   struct wlc_bsscfg *bsscfg);
extern void wl_plc_node_set_scb(struct wl_info *wl, struct wl_plc_node *node,
                                struct scb *scb);
extern struct scb * wl_plc_node_scb(struct wl_info *wl, struct wl_plc_node *node);
extern int32 wl_plc_node_get_cost(struct wl_info *wl, uint8 *ea, uint32 type);
extern int32 wl_plc_node_set_cost(struct wl_info *wl, uint8 *ea, uint32 type, uint32 cost);
#endif /* PLC_WET */

#endif	/* _wl_export_h_ */
