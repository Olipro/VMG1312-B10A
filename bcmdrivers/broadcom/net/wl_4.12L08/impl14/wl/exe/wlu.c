/*
 * Common code for wl command-line swiss-army-knife utility
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlu.c 375781 2012-12-20 02:48:55Z $
 */


#ifdef WIN32
#include <windows.h>
#endif

#if !defined(TARGETOS_nucleus)
#define CLMDOWNLOAD
#endif


#if defined(__NetBSD__)
#include <typedefs.h>
#endif

/* Because IL_BIGENDIAN was removed there are few warnings that need
 * to be fixed. Windows was not compiled earlier with IL_BIGENDIAN.
 * Hence these warnings were not seen earlier.
 * For now ignore the following warnings
 */
#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4761)
#endif

#include <typedefs.h>
#include <epivers.h>
#include <proto/ethernet.h>
#include <proto/802.11.h>
#include <proto/802.1d.h>
#include <proto/802.11e.h>
#include <proto/wpa.h>
#include <proto/bcmip.h>
#include <proto/wps.h>

#include <bcmwifi_rates.h>
#include "wlu_rates_matrix.h"

#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmwifi_channels.h>
#include <bcmsrom_fmt.h>
#include <bcmsrom_tbl.h>
#include "wlu_common.h"
#include "wlu.h"
#include <bcmcdc.h>
#if defined(WLPFN) && defined(linux)
#ifndef TARGETENV_android
#include <unistd.h>
#endif
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_packet.h>
#endif /* WLPFN */

#ifdef WLEXTLOG
#include <wlc_extlog_idstr.h>
#endif

#include <inttypes.h>
#include <miniopt.h>
#include <errno.h>

#if defined SERDOWNLOAD || defined CLMDOWNLOAD
#include <sys/stat.h>
#include <trxhdr.h>
#ifdef SERDOWNLOAD
#include <usbrdl.h>
#endif
#include <stdio.h>
#include <errno.h>

#ifndef WIN32
#include <fcntl.h>
#endif /* WIN32 */
#endif /* SERDOWNLOAD || defined CLMDOWNLOAD */

#if LCNCONF || SSLPNCONF
#define MAX_CHUNK_LEN 1456  /* 8 * 7 * 26 */
#else
#define MAX_CHUNK_LEN 1408 /* 8 * 8 * 22 */
#endif


#include <bcm_mpool_pub.h>
#include <proto/bcmipv6.h>
#include <wlc_ppr.h>

/* For backwards compatibility, the absense of the define 'NO_FILESYSTEM_SUPPORT'
 * implies that a filesystem is supported.
 */
#if !defined(BWL_NO_FILESYSTEM_SUPPORT)
#define BWL_FILESYSTEM_SUPPORT
#endif


cmd_func_t wl_int;

const ofdm_rates_t ofdm_rates[] = { /*	6b,   9,    12b,  18,   24b,  36,   48,   54 Mbps */
					0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c };

#ifdef ATE_BUILD
/* These are the only functions being used from this file by the ATE firmware */
static cmd_func_t wl_channel, wl_version, wl_void, wl_rate_mrate, wl_nrate;
static cmd_func_t wl_pkteng, wl_pkteng_stats, wl_phy_txpwrindex, wl_reg, wl_band;
int wl_seq_batch_in_client(bool enable);
int wlu_get(void *wl, int cmd, void *buf, int len);
int wlu_set(void *wl, int cmd, void *buf, int len);
static void wl_printrate(int val);
static int rate_string2int(char *s);
static char *rate_int2string(char *buf, int val);
int wlu_var_getbuf(void *wl, const char *iovar, void *param, int param_len, void **bufptr);
int wlu_var_getbuf_sm(void *wl, const char *iovar, void *param, int param_len, void **bufptr);
int wlu_var_getbuf_med(void *wl, const char *iovar, void *param, int param_len, void **bufptr);
int wlu_var_setbuf(void *wl, const char *iovar, void *param, int param_len);
int wlu_var_setbuf_sm(void *wl, const char *iovar, void *param, int param_len);
int wlu_var_setbuf_med(void *wl, const char *iovar, void *param, int param_len);
int wlu_iovar_get(void *wl, const char *iovar, void *outbuf, int len);
int wlu_iovar_set(void *wl, const char *iovar, void *param, int paramlen);
int wlu_iovar_getint(void *wl, const char *iovar, int *pval);
int wlu_iovar_setint(void *wl, const char *iovar, int val);
static cmd_func_t wl_gpioout;

#else /* ATE_BUILD */

static cmd_func_t wl_print_deprecate;
static cmd_func_t wl_void, wl_rssi, wl_rssi_event, wl_phy_rssi_ant, wl_gmode;
static cmd_func_t wlu_dump, wlu_mempool, wlu_srdump, wlu_srwrite, wlu_srvar, wl_nvsource;
static cmd_func_t wlu_ciswrite, wlu_cisupdate, wlu_cisdump;
static cmd_func_t wl_rate, wl_rate_mrate, wl_phy_rate, wl_bss_max;
static cmd_func_t wl_channel, wl_chanspec, wl_chanim_state, wl_chanim_mode;
static cmd_func_t wl_radio, wl_version, wl_list, wl_band, wl_bandlist, wl_phylist;
static cmd_func_t wl_join, wl_tssi, wl_txpwr, wl_atten, wl_evm, wl_country;
static cmd_func_t wl_out, wl_txpwr1, wl_country_ie_override;
static cmd_func_t wl_maclist, wl_get_pktcnt, wl_upgrade;
static cmd_func_t wl_maclist_1;
static cmd_func_t wl_rateset, wl_txbf_rateset, wl_interfere, wl_interfere_override;
static cmd_func_t wl_radar_args, wl_radar_thrs, wl_dfs_status;
static cmd_func_t wl_get_txpwr_limit, wl_get_current_power, wl_get_instant_power;
static cmd_func_t wl_get_current_txppr;
static cmd_func_t wl_var_get, wl_var_getint, wl_var_getinthex, wl_var_getandprintstr;
static cmd_func_t wl_var_setint, wl_addwep, wl_rmwep;
static cmd_func_t wl_nvdump, wl_nvget, wl_nvset, wl_sta_info, wl_chan_info;
static cmd_func_t wl_wme_ac_req, wl_add_ie, wl_del_ie, _wl_list_ie;
static cmd_func_t wl_wme_apsd_sta, wl_wme_dp, wl_lifetime;
static cmd_func_t wl_rand, wl_otpw, wl_counters, wl_delta_stats;
static cmd_func_t wl_assoc_info, wl_wme_counters, wl_devpath;
static cmd_func_t wl_management_info;
static cmd_func_t wl_bitvec128, wl_diag, wl_var_void;
static cmd_func_t wl_auto_channel_sel;
static cmd_func_t wl_bsscfg_int, wl_bsscfg_enable;
static cmd_func_t wl_msglevel, wl_plcphdr, wl_reg, wl_macreg, wl_band_elm;
static cmd_func_t wl_phymsglevel;
static cmd_func_t wl_rateparam, wl_wepstatus, wl_status, wl_spect;
static cmd_func_t wl_sup_rateset, wl_scan, wl_send_csa, wl_iscan, wl_escan;
#ifdef EXTENDED_SCAN
static cmd_func_t wl_extdscan;
#endif
static cmd_func_t wl_dump_chanlist, wl_primary_key, wl_measure_req, wl_send_quiet;
static cmd_func_t wl_dump_chanspecs, wl_cur_mcsset;
static cmd_func_t wl_wsec, wl_keys, wl_wsec_test;
static cmd_func_t wl_channels_in_country;
static cmd_func_t wl_wpa_auth, wl_tsc, wl_deauth_rc, wl_ssid, wl_bssid, wl_smfstats;
static cmd_func_t wl_wds_wpa_role_old, wl_wds_wpa_role, wl_set_pmk;
static cmd_func_t wl_rm_request, wl_rm_report;
static cmd_func_t wl_join_pref, wl_assoc_pref;
static cmd_func_t wl_dump_networks, wl_mac, wl_revinfo, wl_iov_mac, wl_iov_mac_params;
static cmd_func_t wl_cac, wl_tslist, wl_tspec, wl_tslist_ea, wl_tspec_ea, wl_cac_delts_ea;
static cmd_func_t wl_varstr, wl_var_setintandprintstr;
#if defined(BCMDBG)
static cmd_func_t wl_phy_debug_cmd;
#endif
static cmd_func_t wl_rifs;
static cmd_func_t wl_rifs_advert;
static cmd_func_t wl_test_tssi, wl_test_tssi_offs, wl_phy_rssiant, wl_rxiq;
static cmd_func_t wl_obss_scan, wl_obss_coex_action;
static cmd_func_t wl_dump_lq;
static cmd_func_t wl_monitor_lq;

#ifdef WLPFN
static cmd_func_t wl_pfn_set;
static cmd_func_t wl_pfn_add;
static cmd_func_t wl_pfn_add_bssid;
static cmd_func_t wl_pfn_cfg;
static cmd_func_t wl_pfn;
static cmd_func_t wl_pfnbest;
static cmd_func_t wl_pfn_suspend;
#if defined(linux)
static cmd_func_t wl_pfn_event_check;
static cmd_func_t wl_escan_event_check;
static cmd_func_t wl_escanresults;
#endif   /* linux */
static cmd_func_t wl_event_filter;
#endif /* WLPFN */
static cmd_func_t wl_wowl_pattern, wl_wowl_wakeind, wl_wowl_pkt, wl_wowl_status;
static cmd_func_t wl_wowl_wake_reason, wl_wowl_extended_magic;
static cmd_func_t wl_reassoc;


static cmd_func_t wl_overlay;

#ifdef DSLCPE
static cmd_func_t wl_assoclist_info;
#ifdef DSLCPE_SCBLIST
static cmd_func_t wl_readscb;
#endif


#if defined(DSLCPE) || defined(linux)
#include <unistd.h>
static void
sleep_ms(const unsigned int ms)
{
	usleep(1000*ms);
}
#else
#error "sleep_ms() not defined for this OS!!!"
#endif /* defined(linux) */
#endif /* DSLCPE */
static cmd_func_t wl_pmkid_info;


static void wl_rate_histo_print(wl_mac_ratehisto_res_t *rate_histo_res);
static cmd_func_t wl_rate_histo;
static cmd_func_t wl_mac_rate_histo;
static cmd_func_t wl_sample_collect;
static cmd_func_t wlu_reg3args;

static cmd_func_t wl_tpc_lm;
static cmd_func_t wlu_reg2args;
static cmd_func_t wme_tx_params;
static cmd_func_t wme_maxbw_params;
static cmd_func_t wl_ampdu_tid, wl_ampdu_activate_test;
static cmd_func_t wl_ampdu_retry_limit_tid;
static cmd_func_t wl_ampdu_rr_retry_limit_tid;
static cmd_func_t wl_ampdu_send_addba;
static cmd_func_t wl_ampdu_send_delba;

static cmd_func_t wl_dpt_deny;
static cmd_func_t wl_dpt_endpoint;
static cmd_func_t wl_dpt_pmk;
static cmd_func_t wl_dpt_fname;
static cmd_func_t wl_dpt_list;
#ifdef WLTDLS
static cmd_func_t wl_tdls_endpoint;
static cmd_func_t wl_tdls_wfd_ie;
#endif /* WLTDLS */
static cmd_func_t wl_actframe;

static cmd_func_t wl_gpioout;

static cmd_func_t wl_bw_cap;
static cmd_func_t wl_nrate, wl_antsel, wl_txcore;
static cmd_func_t wl_txcore_pwr_offset;
#ifdef PLC
static cmd_func_t wl_plc;
#endif /* PLC */
static cmd_func_t wl_txfifo_sz;
static cmd_func_t wl_pkteng, wl_pkteng_stats;

static cmd_func_t wl_offload_cmpnt;
static cmd_func_t wl_hostip, wl_arp_stats, wl_toe_stats, wl_mcast_ackmac, wl_mcast_status;
static cmd_func_t wl_ol_notify_bcn_ie;

#if defined(WLNDOE) || defined(WLOFFLD)
static cmd_func_t wl_hostipv6;
#endif
#ifdef WLNDOE
static cmd_func_t wl_ndstatus, wl_solicitipv6, wl_remoteipv6;
#endif
static cmd_func_t wl_phy_papdepstbl;

int wl_seq_batch_in_client(bool enable);
cmd_func_t wl_seq_start;
cmd_func_t wl_seq_stop;

static cmd_func_t wl_phy_txiqcc, wl_phy_txlocc;
static cmd_func_t wl_phytable, wl_phy_pavars, wl_phy_povars;
static cmd_func_t wl_phy_fem, wl_phy_maxpower, wl_antgain, wl_phy_txpwrindex;
static cmd_func_t wl_keep_alive;
static cmd_func_t wl_mkeep_alive;
static cmd_func_t wl_srchmem;

static cmd_func_t wl_pkt_filter_add;
static cmd_func_t wl_pkt_filter_enable;
static cmd_func_t wl_pkt_filter_list;
static cmd_func_t wl_pkt_filter_stats;

#ifdef TRAFFIC_MGMT
static cmd_func_t wl_trf_mgmt_config;
static cmd_func_t wl_trf_mgmt_filters_add;
static cmd_func_t wl_trf_mgmt_filters_addex;
static cmd_func_t wl_trf_mgmt_filters_remove;
static cmd_func_t wl_trf_mgmt_filters_removeex;
static cmd_func_t wl_trf_mgmt_filters_list;
static cmd_func_t wl_trf_mgmt_filters_clear;
static cmd_func_t wl_trf_mgmt_bandwidth;
static cmd_func_t wl_trf_mgmt_flags;
static cmd_func_t wl_trf_mgmt_stats;
static cmd_func_t wl_trf_mgmt_stats_clear;
static cmd_func_t wl_trf_mgmt_shaping_info;
#endif  /* TRAFFIC_MGMT */

#ifdef CLMDOWNLOAD
static cmd_func_t wl_clmload;
#endif /* CLMDOWNLOAD */
static cmd_func_t wl_ledbh;

#ifdef RWL_WIFI
/* Function added to support RWL_WIFI Transport */
static cmd_func_t wl_wifiserver;
#endif

static cmd_func_t wl_led_blink_sync;
static cmd_func_t wl_cca_get_stats;
static cmd_func_t wl_itfr_get_stats;
static cmd_func_t wl_rrm_nbr_req;
static cmd_func_t wl_rrm_bcn_req;
static cmd_func_t wl_rrm_chload_req;
static cmd_func_t wl_rrm_noise_req;
static cmd_func_t wl_rrm_frame_req;
static cmd_func_t wl_rrm_stat_req;
static cmd_func_t wl_rrm_lm_req;
static cmd_func_t wl_wnm;
static cmd_func_t wl_wnm_bsstq;
#ifdef WLWNM
static cmd_func_t wl_wnm_tclas_add;
static cmd_func_t wl_wnm_tclas_del;
static cmd_func_t wl_wnm_tclas_list;
static cmd_func_t wl_wnm_tfsreq_add;
static cmd_func_t wl_wnm_dmsdesc_add;
static cmd_func_t wl_wnm_dmsdesc_list;
#endif /* WLWNM */
static cmd_func_t wl_chanim_acs_record;
static cmd_func_t wl_chanim_stats;
static cmd_func_t wl_txdelay_params;

#if defined(WLTEST)
static cmd_func_t wl_sslpnphy_papd_debug;
static cmd_func_t wl_sslpnphy_debug;
static cmd_func_t wl_sslpnphy_spbdump;
static cmd_func_t wl_sslpnphy_percal_debug;
static cmd_func_t wl_phy_spbrange;
static cmd_func_t wl_phy_sslpnphy_cga_2g;
static cmd_func_t wl_phy_sslpnphy_cga_5g;
static cmd_func_t wl_sslpnphy_tx_iqcc;
static cmd_func_t wl_sslpnphy_tx_locc;
#endif 

#ifdef WLP2P
static cmd_func_t wl_p2p_state;
static cmd_func_t wl_p2p_scan;
static cmd_func_t wl_p2p_ifadd;
static cmd_func_t wl_p2p_ifdel;
static cmd_func_t wl_p2p_ifupd;
static cmd_func_t wl_p2p_if;
static cmd_func_t wl_p2p_ops;
static cmd_func_t wl_p2p_noa;
#endif

static cmd_func_t wl_rpmt;
static cmd_func_t wl_spatial_policy, wl_ratetbl_ppr;
static cmd_func_t wl_sarlimit;

static cmd_func_t wl_ie;
static cmd_func_t wl_wnm_url;


#ifdef SERDOWNLOAD
static cmd_func_t dhd_upload;
int debug = 0;
#endif

static void wl_txppr_print(ppr_t *ppr, int cck, uint flags);
static void wl_txppr_print_bw(ppr_t *ppr, int cck, uint flags, wl_tx_bw_t bw);
static void wl_txpwr_print_row(const char *label, uint8 chains, int8 pwr20,
	int8 pwr20in40, int8 pwr40, int8 pwr80, int8 pwr20in80, int8 pwr40in80,
	int8 unsupported_rate, int8 channel_bandwidth, bool verbose);
static int8  wl_ppr_get_pwr(ppr_t* pprptr, reg_rate_index_t rate_idx, wl_tx_bw_t bw);
static void wl_txpwr_array_row_print(ppr_t* pprptr, int8 channel_bandwidth,
	reg_rate_index_t rate_idx);
static void wl_txpwr_array_print(ppr_t* pprptr, int8 channel_bandwidth, bool verbose);
static void wl_txpwr_ppr_print(ppr_t* pprptr, int vb, ppr_rate_type_t type,
	clm_rate_group_id_t gid, int8 bw, reg_rate_index_t *rate_index);
static void wl_txpwr_regulatory_array_row_print(int8 *powers, int8 *powers_subchan1,
	int8 *powers_subchan2, int8 channel_bandwidth, reg_rate_index_t rate_index);
static void wl_txpwr_regulatory_array_print(int8 *powers, int8 *powers_subchan1,
	int8 *powers_subchan2, int8 channel_bandwidth, bool verbose);
static int wl_array_uniform(uint8 *pwr, int start, int count);
static int wl_array_check_val(int8 *pwr, uint count, int8 val);
static int wl_parse_rateset(void *wl, wl_rateset_args_t* rs, char **argv);
static int wl_parse_txbf_rateset(wl_txbf_rateset_t *rs, char **argv);
static void wl_print_mcsset(char *mcsset);
static void wl_print_vhtmcsset(uint16 *mcsset);
static void wl_print_txbf_mcsset(char *mcsset, char *prefix);
static void wl_print_txbf_vhtmcsset(uint16 *mcsset, char *prefix);

static void dump_networks(char *buf);
void dump_bss_info(wl_bss_info_t *bi);
static void wl_dump_wpa_rsn_ies(uint8* cp, uint len);
static void wl_rsn_ie_dump(bcm_tlv_t *ie);
static cmd_func_t wl_lpc_params;

int wlu_get(void *wl, int cmd, void *buf, int len);
int wlu_set(void *wl, int cmd, void *buf, int len);

static int _wl_dump_lq(void *wl);

/* 802.11i/WPA RSN IE parsing utilities */
typedef struct {
	uint16 version;
	wpa_suite_mcast_t *mcast;
	wpa_suite_ucast_t *ucast;
	wpa_suite_auth_key_mgmt_t *akm;
	uint8 *capabilities;
} rsn_parse_info_t;

static int wl_rsn_ie_parse_info(uint8* buf, uint len, rsn_parse_info_t *rsn);
static uint wl_rsn_ie_decode_cntrs(uint cntr_field);

static int wl_parse_assoc_params(char **argv, wl_assoc_params_t *params);
#define wl_parse_reassoc_params(argv, params) wl_parse_assoc_params(argv, \
						(wl_assoc_params_t *)(params))

static int wl_parse_channel_list(char* list_str, uint16* channel_list, int channel_num);
static int wl_parse_chanspec_list(char* list_str, chanspec_t *chanspec_list, int chanspec_num);
static chanspec_t wl_chspec_to_driver(chanspec_t chanspec);
static uint32 wl_chspec32_to_driver(chanspec_t chanspec);
static chanspec_t wl_chspec_from_driver(chanspec_t chanspec);
static chanspec_t wl_chspec32_from_driver(uint32 chanspec);

#ifdef EXTENDED_SCAN
static int wl_parse_extdchannel_list(char* list_str,
	chan_scandata_t* channel_list, int channel_num);
#endif

static uint16 wl_qdbm_to_mw(uint8 qdbm);
static uint8 wl_mw_to_qdbm(uint16 mw);

static int wl_cfg_option(char **argv, const char *fn_name, int *bsscfg_idx, int *consumed);
static int get_oui_bytes(uchar *oui_str, uchar *oui);
static int get_ie_data(uchar *data_str, uchar *ie_data, int len);
static void wl_printrate(int val);
static int rate_string2int(char *s);
static char *rate_int2string(char *buf, int val);

static int wl_get_iscan(void *wl, char *buf, uint buf_len);
int wlu_var_getbuf(void *wl, const char *iovar, void *param, int param_len, void **bufptr);
int wlu_var_getbuf_sm(void *wl, const char *iovar, void *param, int param_len, void **bufptr);
int wlu_var_getbuf_med(void *wl, const char *iovar, void *param, int param_len, void **bufptr);
int wlu_var_setbuf(void *wl, const char *iovar, void *param, int param_len);

int wlu_iovar_get(void *wl, const char *iovar, void *outbuf, int len);
int wlu_iovar_set(void *wl, const char *iovar, void *param, int paramlen);
int wlu_iovar_getint(void *wl, const char *iovar, int *pval);
int wlu_iovar_setint(void *wl, const char *iovar, int val);

static int wl_bssiovar_mkbuf(const char *iovar, int bssidx, void *param,
	int paramlen, void *bufptr, int buflen, int *perr);
int wlu_bssiovar_setbuf(void* wl, const char *iovar, int bssidx,
	void *param, int paramlen, void *bufptr, int buflen);
static int wl_bssiovar_getbuf(void* wl, const char *iovar, int bssidx,
	void *param, int paramlen, void *bufptr, int buflen);
static int wl_bssiovar_set(void *wl, const char *iovar, int bssidx, void *param, int paramlen);
int wlu_bssiovar_get(void *wl, const char *iovar, int bssidx, void *outbuf, int len);
static int wl_bssiovar_setint(void *wl, const char *iovar, int bssidx, int val);
static int wl_bssiovar_getint(void *wl, const char *iovar, int bssidx, int *pval);

static int wl_vndr_ie(void *wl, const char *command, uint32 pktflag_ok, char **argv);
static void wl_dump_ie_buf(vndr_ie_buf_t *ie_getbuf);
static int hexstrtobitvec(const char *cp, uchar *bitvec, int veclen);
static void wl_join_pref_print_ie(bcm_tlv_t *ie);
static void wl_join_pref_print_akm(uint8* suite);
static void wl_join_pref_print_cipher_suite(uint8* suite);
static void wl_print_tspec(tspec_arg_t *ts);
static void wl_cac_addts_usage(void);
static void wl_cac_delts_usage(void);

static cmd_func_t wl_txmcsset;
static cmd_func_t wl_rxmcsset;

static int wl_mimo_stf(void *wl, cmd_t *cmd, char **argv);

#ifdef WLEXTLOG
static int wl_extlog(void *wl, cmd_t *cmd, char **argv);
static int wl_extlog_cfg(void *wl, cmd_t *cmd, char **argv);
#endif

static int wl_assertlog(void *wl, cmd_t *cmd, char **argv);
static char *ver2str(unsigned int vms, unsigned int vls);
static int wl_tsf(void *wl, cmd_t *cmd, char **argv);
static int wl_mfp_config(void *wl, cmd_t *cmd, char **argv);
static int wl_mfp_sha256(void *wl, cmd_t *cmd, char **argv);
static int wl_mfp_sa_query(void *wl, cmd_t *cmd, char **argv);
static int wl_mfp_disassoc(void *wl, cmd_t *cmd, char **argv);
static int wl_mfp_deauth(void *wl, cmd_t *cmd, char **argv);
static int wl_mfp_assoc(void *wl, cmd_t *cmd, char **argv);
static int wl_mfp_auth(void *wl, cmd_t *cmd, char **argv);
static int wl_mfp_reassoc(void *wl, cmd_t *cmd, char **argv);
#endif /* ATE_BUILD */

/* some OSes (FC4) have trouble allocating (kmalloc) 128KB worth of memory,
 * hence keeping WL_DUMP_BUF_LEN below that
 */
#if defined(BWL_SMALL_WLU_DUMP_BUF)
#define WL_DUMP_BUF_LEN (4 * 1024)
#else
#define WL_DUMP_BUF_LEN (127 * 1024)
#endif 

#define OUI_STR_SIZE	8	/* OUI string size */
#define MAX_OUI_SIZE	3	/* MAX  OUI size */
#define MAX_BYTE_CHARS	2	/* MAX num chars */
#define MAX_DATA_COLS	16	/* MAX data cols */
#define DIV_QUO(num, div) ((num)/div)  /* Return the quotient of division to avoid floats */
#define DIV_REM(num, div) (((num%div) * 100)/div) /* Return the remainder of division */

#define RADIO_CORE_SYN                           (0x0 << 12)
#define RADIO_CORE_TX0                           (0x2 << 12)
#define RADIO_CORE_TX1                           (0x3 << 12)
#define RADIO_CORE_RX0                           (0x6 << 12)
#define RADIO_CORE_RX1                           (0x7 << 12)

#define RADIO_CORE_CR0                           (0x0 << 10)
#define RADIO_CORE_CR1                           (0x1 << 10)
#define RADIO_CORE_CR2                           (0x2 << 10)
#define RADIO_CORE_ALL                           (0x3 << 10)


/* IOCtl version read from targeted driver */
static int ioctl_version;

/* dword align allocation */
static union {
	char bufdata[WLC_IOCTL_MAXLEN];
	uint32 alignme;
} bufstruct_wlu;
static char *buf = (char*) &bufstruct_wlu.bufdata;

/* integer output format, default to signed integer */
static uint8 int_fmt;

#ifndef ATE_BUILD
/*
 * Country names and abbreviations from ISO 3166
 */
typedef struct {
	const char *name;	/* Long name */
	const char *abbrev;	/* Abbreviation */
} cntry_name_t;
cntry_name_t cntry_names[];	/* At end of this file */
#endif /* !ATE_BUILD */

typedef struct {
	uint value;
	const char *string;
} dbg_msg_t;

typedef struct {
	uint value;
	const char *string;
} phy_msg_t;

#define WL_SCAN_PARAMS_SSID_MAX 10
#define SCAN_USAGE	"" \
"\tDefault to an active scan across all channels for any SSID.\n" \
"\tOptional arg: SSIDs, list of [up to 10] SSIDs to scan (comma or space separated).\n" \
"\tOptions:\n" \
"\t-s S, --ssid=S\t\tSSIDs to scan\n" \
"\t-t ST, --scan_type=ST\t[active|passive|prohibit] scan type\n" \
"\t--bss_type=BT\t\t[bss/infra|ibss/adhoc] bss type to scan\n" \
"\t-b MAC, --bssid=MAC\tparticular BSSID MAC address to scan, xx:xx:xx:xx:xx:xx\n" \
"\t-n N, --nprobes=N\tnumber of probes per scanned channel\n" \
"\t-a N, --active=N\tdwell time per channel for active scanning\n" \
"\t-p N, --passive=N\tdwell time per channel for passive scanning\n" \
"\t-h N, --home=N\t\tdwell time for the home channel between channel scans\n" \
"\t-c L, --channels=L\tcomma or space separated list of channels to scan" \

#define RATE_2G_USAGE							\
"\tEither \"auto\", or a simple CCK/DSSS/OFDM rate value:\n"		\
"\t1 2 5.5 11 6 9 12 18 24 36 48 54\n\n"				\
"\tOr options to specify legacy, HT, or VHT rate:\n"			\
"\t-r R, --rate=R        : legacy rate (CCK, DSSS, OFDM)\n"		\
"\t-h M, --ht=M          : HT MCS index [0-23]\n"			\
"\t-v M[xS], --vht=M[xS] : VHT MCS index M [0-9],\n"			\
"\t                      : and optionally Nss S [1-8], eg. 5x2 is MCS=5, Nss=2\n" \
"\t-c cM[sS]             : VHT (c notation) MCS index M [0-9],\n"			\
"\t                      : and optionally Nss S [1-8], eg. c5s2 is MCS=5, Nss=2\n" \
"\t-s S, --ss=S          : VHT Nss [1-8], number of spatial streams, default 1.\n" \
"\t                      : Only used with -v/--vht when MxS format is not used\n" \
"\t-x T, --exp=T         : Tx Expansion, number of tx chains (NTx) beyond the minimum\n" \
"\t                      : required for the space-time-streams, exp = NTx - Nsts\n" \
"\t--stbc                : Use STBC expansion, otherwise no STBC\n"	\
"\t-l, --ldpc            : Use LDPC encoding, otherwise no LDPC\n"	\
"\t-g, --sgi             : SGI, Short Guard Interval, otherwise standard GI\n" \
"\t-b, --bandwidth       : transmit bandwidth MHz; 20, 40, 80"

#define RATE_5G_USAGE							\
"\tEither \"auto\", or a simple OFDM rate value:\n"			\
"\t6 9 12 18 24 36 48 54\n\n"						\
"\tOr options to specify legacy OFDM, HT, or VHT rate:\n"		\
"\t-r R, --rate=R        : legacy OFDM rate\n"				\
"\t-h M, --ht=M          : HT MCS index [0-23]\n"			\
"\t-v M[xS], --vht=M[xS] : VHT MCS index M [0-9],\n"			\
"\t                      : and optionally Nss S [1-8], eg. 5x2 is MCS=5, Nss=2\n" \
"\t-c cM[sS]             : VHT (c notation) MCS index M [0-9],\n"			\
"\t                      : and optionally Nss S [1-8], eg. c5s2 is MCS=5, Nss=2\n" \
"\t-s S, --ss=S          : VHT Nss [1-8], number of spatial streams, default 1.\n" \
"\t                      : Only used with -v/--vht when MxS format is not used\n" \
"\t-x T, --exp=T         : Tx Expansion, number of tx chains (NTx) beyond the minimum\n" \
"\t                      : required for the space-time-streams, exp = NTx - Nsts\n" \
"\t--stbc                : Use STBC expansion, otherwise no STBC\n"	\
"\t-l, --ldpc            : Use LDPC encoding, otherwise no LDPC\n"	\
"\t-g, --sgi             : SGI, Short Guard Interval, otherwise standard GI\n" \
"\t-b, --bandwidth       : transmit bandwidth MHz; 20, 40, 80"

#define TXBF_RATESET_USAGE						\
"Get rateset consisting of OFDM, HT and VHT rates, and Broadcom-to-Broadcom\n" \
"\tgroup of OFDM, HT and VHT rates by issuing command with no arguments.\n" \
"\tOFDM rates printed are in Mbps, and each Basic rate in OFDM list is marked\n" \
"\tby (b) behind it.  Example: full list of OFDM rates:\n" \
"\t\t6(b) 9 12(b) 18 24(b) 36 48 54\n" \
"\twhere 6, 12 and 24 are Basic rates.\n" \
"\n" \
"\tSet synopsis:\n" \
"\t\twl txbf_rateset < [ofdm_rate_list] [options ...] >\n" \
"\tOFDM rate specification does not need to mark Basic rates because Basic\n" \
"\trates are automatically recognized.\n" \
"\tOptions are processed in order; thus redundant instances of an option will\n" \
"\tresult in only the last instance taking effect for that option.\n" \
"\tOptions:\n" \
"\t-m <MCS_bitmask> ...\n" \
"\t\tSet HT rates by bitmask bytes, each ranges from 00 through ff, where\n" \
"\t\tthe least significant bit is MCS0.\n" \
"\t\tExample: '-m 3f 01' specifies HT rates MCS0 - MCS5 and MCS8.\n" \
"\n" \
"\t-v <VHT_bitmask> ...\n" \
"\t\tSet VHT rates for each supported count of spatial streams.\n" \
"\t\tExample: '-v 3ff 1ff ff' specifies VHT rates: MCS0 - MCS9 for 1 stream,\n" \
"\t\tMCS0 - MCS8 for 2 streams, and MCS0 - MCS7 for 3 streams.\n" \
"\n" \
"\t-b\n" \
"\t\tSet for Broadcom-to-Broadcom group of rates.  Otherwise without\n" \
"\t\tthe -b option, the standard group of rates are set accordingly.\n"

/* the default behavior is batching in driver,
 * to indicate client batching, users should specify --interactive and --clientbatch
 */
static bool batch_in_client;

/* If the new command needs to be part of 'wc.exe' tool used for WMM,
 * be sure to modify wc_cmds[] array as well
 *
 * If you add a command, please update wlu_cmd.c cmd2cat to categorize the command.
 */
cmd_t wl_cmds[] = {
	{ "ver", wl_version, -1, -1,
	"get version information" },
#ifndef ATE_BUILD
	{ "cmds", wl_list, -1, -1,
	"generate a short list of available commands"},
#endif
	{ "up",	wl_void, -1, WLC_UP,
	"reinitialize and mark adapter up (operational)" },
#ifndef ATE_BUILD
	{ "down", wl_void, -1, WLC_DOWN,
	"reset and mark adapter down (disabled)" },
	{ "out", wl_out, -1, WLC_OUT,
	"mark adapter down but do not reset hardware(disabled)\n"
	"\tOn dualband cards, cards must be bandlocked before use."},
	{ "clk", wl_int, WLC_GET_CLK, WLC_SET_CLK,
	"set board clock state. return error for set_clk attempt if the driver is not down\n"
	"\t0: clock off\n"
	"\t1: clock on" },
	{ "restart", wl_void, -1, WLC_RESTART,
	"Restart driver.  Driver must already be down."},
	{ "reboot", wl_void, -1, WLC_REBOOT,
	"Reboot platform"},
	{ "radio", wl_radio, WLC_GET_RADIO, WLC_SET_RADIO,
	"Set the radio on or off.\n"
	"\t\"on\" or \"off\"" },
	{ "dump", wlu_dump, WLC_GET_VAR, -1,
	"Give suboption \"list\" to list various suboptions" },
	{ "srclear", wlu_srwrite, -1, WLC_SET_SROM,
	"Clears first 'len' bytes of the srom, len in decimal or hex\n"
	"\tUsage: srclear <len>" },
	{ "srdump", wlu_srdump, WLC_GET_SROM, -1,
	"print contents of SPROM to stdout" },
	{ "srwrite", wlu_srwrite, -1, WLC_SET_SROM,
	"Write the srom: srwrite byteoffset value" },
	{ "srcrc", wlu_srwrite, WLC_GET_SROM, -1,
	"Get the CRC for input binary file" },
	{ "ciswrite", wlu_ciswrite, -1, WLC_SET_VAR,
	"Write specified <file> to the SDIO CIS source (either SROM or OTP)"},
	{ "cisupdate", wlu_cisupdate, -1, WLC_SET_VAR,
	"Write a hex byte stream to specified byte offset to the CIS source (either SROM or OTP)\n"
	"--preview option allows you to review the update without committing it\n"
	"\t<byte offset> <hex byte stream> [--preview]" },
	{ "cisdump", wlu_cisdump, WLC_GET_VAR, -1,
	"Display the content of the SDIO CIS source\n"
	"\t-b <file> -- also write raw bytes to <file>\n"
	"\t<len> -- optional count of bytes to display (must be even)"},
	{ "cis_source", wl_varint, WLC_GET_VAR, -1,
	"Display which source is used for the SDIO CIS"},
	{ "cisconvert", wlu_srvar, -1, -1,
	"Print CIS tuple for given name=value pair" },
	{ "rdvar", wlu_srvar, WLC_GET_SROM, -1,
	"Read a named variable to the srom" },
	{ "wrvar", wlu_srvar, WLC_GET_SROM, WLC_SET_SROM,
	"Write a named variable to the srom" },
	{ "nvram_source", wl_nvsource, WLC_GET_VAR, -1,
	"Display which source is used for nvram"},
	{ "nvram_dump", wl_nvdump, WLC_NVRAM_DUMP, -1,
	"print nvram variables to stdout" },
	{ "nvset", wl_nvset, -1, WLC_NVRAM_SET,
	"set an nvram variable\n"
	"\tname=value (no spaces around \'=\')" },
	{ "nvget", wl_nvget, WLC_NVRAM_GET, -1,
	"get the value of an nvram variable" },
	{ "nvram_get", wl_nvget, WLC_NVRAM_GET, -1,
	"get the value of an nvram variable" },
	{ "revinfo", wl_revinfo, WLC_GET_REVINFO, -1,
	"get hardware revision information" },
	{ "customvar1", wl_var_getinthex, -1, -1,
	"print the value of customvar1 in hex format" },
	{ "msglevel", wl_msglevel, WLC_GET_VAR, WLC_SET_VAR,
	"set driver console debugging message bitvector\n"
	"\ttype \'wl msglevel ?\' for values" },
	{ "phymsglevel", wl_phymsglevel, WLC_GET_VAR, WLC_SET_VAR,
	"set phy debugging message bitvector\n"
	"\ttype \'wl phymsglevel ?\' for values" },
	{ "PM", wl_int, WLC_GET_PM, WLC_SET_PM,
	"set driver power management mode:\n"
	"\t0: CAM (constantly awake)\n"
	"\t1: PS  (power-save)\n"
	"\t2: FAST PS mode" },
	{ "wake", wl_int, WLC_GET_WAKE, WLC_SET_WAKE,
	"set driver power-save mode sleep state:\n"
	"\t0: core-managed\n"
	"\t1: awake" },
	{ "promisc", wl_int, WLC_GET_PROMISC, WLC_SET_PROMISC,
	"set promiscuous mode ethernet address reception\n"
	"\t0 - disable\n"
	"\t1 - enable" },
	{ "monitor", wl_int, WLC_GET_MONITOR, WLC_SET_MONITOR,
	"set monitor mode\n"
	"\t0 - disable\n"
	"\t1 - enable active monitor mode (interface still operates)" },
	{ "frag", wl_print_deprecate, -1, -1, "Deprecated. Use fragthresh." },
	{ "rts", wl_print_deprecate, -1, -1, "Deprecated. Use rtsthresh." },
	{ "cwmin", wl_int, WLC_GET_CWMIN, WLC_SET_CWMIN,
	"Set the cwmin.  (integer [1, 255])" },
	{ "cwmax", wl_int, WLC_GET_CWMAX, WLC_SET_CWMAX,
	"Set the cwmax.  (integer [256, 2047])" },
	{ "srl", wl_int, WLC_GET_SRL, WLC_SET_SRL,
	"Set the short retry limit.  (integer [1, 255])" },
	{ "lrl", wl_int, WLC_GET_LRL, WLC_SET_LRL,
	"Set the long retry limit.  (integer [1, 255])" },
#endif /* !ATE_BUILD */
	{ "rate", wl_rate_mrate, WLC_GET_RATE, -1,
	"force a fixed rate:\n"
	"\tvalid values for 802.11a are (6, 9, 12, 18, 24, 36, 48, 54)\n"
	"\tvalid values for 802.11b are (1, 2, 5.5, 11)\n"
	"\tvalid values for 802.11g are (1, 2, 5.5, 6, 9, 11, 12, 18, 24, 36, 48, 54)\n"
	"\t-1 (default) means automatically determine the best rate" },
#ifndef ATE_BUILD
	{ "mrate", wl_rate_mrate, -1, -1, /* Deprecated. Use "bg_mrate" or "a_mrate" */
	"force a fixed multicast rate:\n"
	"\tvalid values for 802.11a are (6, 9, 12, 18, 24, 36, 48, 54)\n"
	"\tvalid values for 802.11b are (1, 2, 5.5, 11)\n"
	"\tvalid values for 802.11g are (1, 2, 5.5, 6, 9, 11, 12, 18, 24, 36, 48, 54)\n"
	"\t-1 (default) means automatically determine the best rate" },
	{ "a_rate", wl_phy_rate, WLC_GET_VAR, WLC_SET_VAR,
	"force a fixed rate for the A PHY:\n"
	"\tvalid values for 802.11a are (6, 9, 12, 18, 24, 36, 48, 54)\n"
	"\t-1 (default) means automatically determine the best rate" },
	{ "a_mrate", wl_phy_rate, WLC_GET_VAR, WLC_SET_VAR,
	"force a fixed multicast rate for the A PHY:\n"
	"\tvalid values for 802.11a are (6, 9, 12, 18, 24, 36, 48, 54)\n"
	"\t-1 (default) means automatically determine the best rate" },
	{ "bg_rate", wl_phy_rate, WLC_GET_VAR, WLC_SET_VAR,
	"force a fixed rate for the B/G PHY:\n"
	"\tvalid values for 802.11b are (1, 2, 5.5, 11)\n"
	"\tvalid values for 802.11g are (1, 2, 5.5, 6, 9, 11, 12, 18, 24, 36, 48, 54)\n"
	"\t-1 (default) means automatically determine the best rate" },
	{ "bg_mrate", wl_phy_rate, WLC_GET_VAR, WLC_SET_VAR,
	"force a fixed multicast rate for the B/G PHY:\n"
	"\tvalid values for 802.11b are (1, 2, 5.5, 11)\n"
	"\tvalid values for 802.11g are (1, 2, 5.5, 6, 9, 11, 12, 18, 24, 36, 48, 54)\n"
	"\t-1 (default) means automatically determine the best rate" },
	{ "2g_rate", wl_rate, WLC_GET_VAR, WLC_SET_VAR,
	"Force a fixed rate for data frames in the 2.4G band:\n\n"
	RATE_2G_USAGE
	},
	{ "2g_mrate", wl_rate, WLC_GET_VAR, WLC_SET_VAR,
	"Force a fixed rate for mulitcast/broadcast data frames in the 2.4G band:\n\n"
	RATE_2G_USAGE
	},
	{ "5g_rate", wl_rate, WLC_GET_VAR, WLC_SET_VAR,
	"Force a fixed rate for data frames in the 5G band:\n\n"
	RATE_5G_USAGE
	},
	{ "5g_mrate", wl_rate, WLC_GET_VAR, WLC_SET_VAR,
	"Force a fixed rate for mulitcast/broadcast data frames in the 5G band:\n\n"
	RATE_5G_USAGE
	},
	{ "infra", wl_int, WLC_GET_INFRA, WLC_SET_INFRA,
	"Set Infrastructure mode: 0 (IBSS) or 1 (Infra BSS)" },
	{ "ap", wl_int, WLC_GET_AP, WLC_SET_AP,
	"Set AP mode: 0 (STA) or 1 (AP)" },
	{ "bssid", wl_bssid, WLC_GET_BSSID, -1,
	"Get the BSSID value, error if STA and not associated"},
	{ "bssmax", wl_bss_max, WLC_GET_VAR, -1,
	"get number of BSSes " },
	{ "bw_cap", wl_bw_cap, WLC_GET_VAR, WLC_SET_VAR,
	"Get/set the per-band bandwidth.\n"
	"Usage: wl bw_cap <2g|5g> [<cap>]\n"
	"\t2g|5g - Band: 2.4GHz or 5GHz respectively\n"
	"cap:\n"
	"\t0x1 - 20MHz\n"
	"\t0x3 - 20/40MHz\n"
	"\t0x7 - 20/40/80MHz\n"
	"\t0xff - Unrestricted" },
#endif /* !ATE_BUILD */
	{ "channel", wl_channel, WLC_GET_CHANNEL, WLC_SET_CHANNEL,
	"Set the channel:\n"
	"\tvalid channels for 802.11b/g (2.4GHz band) are 1 through 14\n"
	"\tvalid channels for 802.11a  (5 GHz band) are:\n"
	"\t\t36, 40, 44, 48, 52, 56, 60, 64,\n"
	"\t\t100, 104, 108, 112, 116,120, 124, 128, 132, 136, 140,\n"
	"\t\t149, 153, 157, 161,\n"
	"\t\t184, 188, 192, 196, 200, 204, 208, 212, 216"},
#ifndef ATE_BUILD
	{ "cur_mcsset", wl_cur_mcsset, WLC_GET_VAR, -1,
	"Get the current mcs set"
	},
	{ "clmver", wl_var_getandprintstr, WLC_GET_VAR, -1,
	"Get version information for CLM data and tools"},
	{ "chanspecs", wl_dump_chanspecs, WLC_GET_VAR, -1,
	"Get all the valid chanspecs (default: all within current locale):\n"
	"\t-b band (5(a) or 2(b/g))\n"
	"\t-w bandwidth, 20, 40 or 80\n"
	"\t[-c country_abbrev]"
	},
	{ "chanspec", wl_chanspec, WLC_GET_VAR, WLC_SET_VAR,
	"Set current or configured channel:\n"
	"\t20MHz : [2g|5g]<channel>[/20]\n"
	"\t40MHz : [2g|5g]<channel>/40[u,l]\n"
	"\t80MHz :    [5g]<channel>/80\n"
	"\toptional band 2g or 5g, default to 2g if channel <= 14\n"
	"\tchannel number (0-200)\n"
	"\tbandwidth, 20, 40, or 80, default 20\n"
	"\tprimary sideband for 40MHz on 2g, l=lower, u=upper\n"
	"OR Set channel with legacy format:\n"
	"\t-c channel number (0-224)\n"
	"\t-b band (5(a) or 2(b/g))\n"
	"\t-w bandwidth 20 or 40\n"
	"\t-s ctl sideband, -1=lower, 0=none, 1=upper"},
	{ "dfs_channel_forced", wl_chanspec, WLC_GET_VAR, WLC_SET_VAR,
	"Set <channel>[a,b][n][u,l]\n"
	"\tchannel number (0-224)\n"
	"\tband a=5G, b=2G, default to 2G if channel <= 14\n"
	"\tbandwidth, n=10, non for 20 & 40\n"
	"\tctl sideband, l=lower, u=upper"},
	{ "tssi", wl_tssi, WLC_GET_TSSI, -1,
	"Get the tssi value from radio" },
	{ "txpwr", wl_txpwr, -1, -1, /* Deprecated. Use "txpwr1" */
	"Set tx power in milliwatts.  Range [1, 84]." },
	{ "txpwr1", wl_txpwr1, WLC_GET_VAR, WLC_SET_VAR,
	"Set tx power in in various units. Choose one of (default: dbm): \n"
	"\t-d dbm units\n"
	"\t-q quarter dbm units\n"
	"\t-m milliwatt units\n"
	"Can be combined with:\n"
	"\t-o turn on override to disable regulatory and other limitations\n"
	"Use wl txpwr -1 to restore defaults"},
	{ "txpathpwr", wl_int, WLC_GET_TX_PATH_PWR, WLC_SET_TX_PATH_PWR,
	"Turn the tx path power on or off on 2050 radios" },
	{ "txpwrlimit", wl_get_txpwr_limit, WLC_CURRENT_PWR, -1,
	"Return current tx power limit" },
	{ "powerindex", wl_int, WLC_GET_PWRIDX, WLC_SET_PWRIDX,
	"Set the transmit power for A band(0-63).\n"
	"\t-1 - default value" },
	{ "atten", wl_atten, WLC_GET_ATTEN, WLC_SET_ATTEN,
	"Set the transmit attenuation for B band. Args: bb radio txctl1.\n"
	"\tauto to revert to automatic control\n"
	"\tmanual to supspend automatic control" },
	{ "phyreg", wl_reg, WLC_GET_PHYREG, WLC_SET_PHYREG,
	"Get/Set a phy register:\n"
	"\toffset [ value ] [ band ]" },
#endif /* !ATE_BUILD */
	{ "radioreg", wl_reg, WLC_GET_RADIOREG, WLC_SET_RADIOREG,
	"Get/Set a radio register:\n"
	"\toffset [ value ] [ band/core ]\n"
	"HTPHY:\n"
	"\tGet a radio register: wl radioreg [ offset ] [ cr0/cr1/cr2 ]\n"
	"\tSet a radio register: wl radioreg [ offset ] [ value ] [ cr0/cr1/cr2/all ]"},
#ifndef ATE_BUILD
	{ "ucflags", wl_reg, WLC_GET_UCFLAGS, WLC_SET_UCFLAGS,
	"Get/Set ucode flags 1, 2, 3(16 bits each)\n"
	"\toffset [ value ] [ band ]" },
	{ "shmem", wl_reg, WLC_GET_SHMEM, WLC_SET_SHMEM,
	"Get/Set a shared memory location:\n"
	"\toffset [ value ] [band ]" },
	{ "macreg", wl_macreg, WLC_R_REG, WLC_W_REG,
	"Get/Set any mac registers(include IHR and SB):\n"
	"\tmacreg offset size[2,4] [ value ] [ band ]" },
	{ "ucantdiv", wl_int, WLC_GET_UCANTDIV, WLC_SET_UCANTDIV,
	"Enable/disable ucode antenna diversity (1/0 or on/off)" },
#endif /* !ATE_BUILD */
	{ "gpioout", wl_gpioout, -1, -1,
	"Set any GPIO pins to any value. Use with caution as GPIOs would be "
	"assigned to chipcommon\n"
	"\tUsage: gpiomask gpioval"},

#ifndef ATE_BUILD
	{ "devpath", wl_devpath, WLC_GET_VAR, -1,
	"print device path" },

	{ "pcieserdesreg", wlu_reg3args, WLC_GET_VAR, WLC_SET_VAR,
	"g/set SERDES registers: dev offset [val]"},
	{ "ampdu_activate_test", wl_ampdu_activate_test, -1, WLC_SET_VAR,
	"actiate" },
	/* nphy parameter setting is internal only for now */
	{ "ampdu_tid", wl_ampdu_tid, WLC_GET_VAR, WLC_SET_VAR,
	"enable/disable per-tid ampdu; usage: wl ampdu_tid <tid> [0/1]" },
	{ "ampdu_retry_limit_tid", wl_ampdu_retry_limit_tid, WLC_GET_VAR, WLC_SET_VAR,
	"Set per-tid ampdu retry limit; usage: wl ampdu_retry_limit_tid <tid> [0~31]" },
	{ "ampdu_rr_retry_limit_tid", wl_ampdu_rr_retry_limit_tid, WLC_GET_VAR, WLC_SET_VAR,
	"Set per-tid ampdu regular rate retry limit; usage: "
	"wl ampdu_rr_retry_limit_tid <tid> [0~31]" },
	{ "ampdu_send_addba", wl_ampdu_send_addba, WLC_GET_VAR, WLC_SET_VAR,
	"send addba to specified ea-tid; usage: wl ampdu_send_addba <tid> <ea>" },
	{ "ampdu_send_delba", wl_ampdu_send_delba, WLC_GET_VAR, WLC_SET_VAR,
	"send delba to specified ea-tid; usage: wl ampdu_send_delba <tid> <ea>" },
	{ "ampdu_clear_dump", wl_var_void, -1, WLC_SET_VAR,
	"clear ampdu counters"},
	{ "ampdu_txq_prof_start", wl_var_void, -1, WLC_SET_VAR,
	"start sample txq profiling data"},
	{ "ampdu_txq_prof_dump", wl_var_void, -1, WLC_SET_VAR,
	"show txq histogram"},
	{ "ampdu_txq_ss", wl_var_void, -1, WLC_SET_VAR,
	"take txq snapshot"},
	{ "dpt_deny", wl_dpt_deny, WLC_GET_VAR, WLC_SET_VAR,
	"adds/removes ea to dpt deny list\n"
	"\tusage: wl dpt_deny <add,remove> <ea>" },
	{ "dpt_endpoint", wl_dpt_endpoint, WLC_GET_VAR, WLC_SET_VAR,
	"creates/updates/deletes dpt endpoint for ea\n"
	"\tusage: wl dpt_endpoint <create, update, delete> <ea>" },
	{ "dpt_pmk", wl_dpt_pmk, -1, WLC_SET_VAR,
	"sets DPT pre-shared key" },
	{ "dpt_fname", wl_dpt_fname, WLC_GET_VAR, WLC_SET_VAR,
	"sets/gets DPT friendly name" },
	{ "dpt_list", wl_dpt_list, WLC_GET_VAR, -1,
	"gets status of all dpt peers" },
	{ "actframe", wl_actframe, -1, WLC_SET_VAR,
	"Send a Vendor specific Action frame to a channel\n"
	"\tusage: wl actframe <Dest Mac Addr> <data> channel dwell-time <BSSID>" },
	{ "antdiv", wl_int, WLC_GET_ANTDIV, WLC_SET_ANTDIV,
	"Set antenna diversity for rx\n"
	"\t0 - force use of antenna 0\n"
	"\t1 - force use of antenna 1\n"
	"\t3 - automatic selection of antenna diversity" },
	{ "txant", wl_int, WLC_GET_TXANT, WLC_SET_TXANT,
	"Set the transmit antenna\n"
	"\t0 - force use of antenna 0\n"
	"\t1 - force use of antenna 1\n"
	"\t3 - use the RX antenna selection that was in force during\n"
	"\t    the most recently received good PLCP header" },
	{ "plcphdr", wl_plcphdr, WLC_GET_PLCPHDR, WLC_SET_PLCPHDR,
	"Set the plcp header.\n"
	"\t\"long\" or \"auto\" or \"debug\"" },
	{ "phytype", wl_int, WLC_GET_PHYTYPE, -1,
	"Get phy type" },
	{ "rateparam", wl_rateparam, -1, WLC_SET_RATE_PARAMS,
	"set driver rate selection tunables\n"
	"\targ 1: tunable id\n"
	"\targ 2: tunable value" },
	{ "wepstatus", wl_wepstatus, -1, -1, /* Deprecated. Use "wsec" */
	"Set or Get WEP status\n"
	"\twepstatus [on|off]" },
	{ "primary_key", wl_primary_key, WLC_GET_KEY_PRIMARY, WLC_SET_KEY_PRIMARY,
	"Set or get index of primary key" },
	{ "addwep", wl_addwep, -1, WLC_SET_KEY,
	"Set an encryption key.  The key must be 5, 13 or 16 bytes long, or\n"
	"\t10, 26, 32, or 64 hex digits long.  The encryption algorithm is\n"
	"\tautomatically selected based on the key size. keytype is accepted\n"
	"\tonly when key length is 16 bytes/32 hex digits and specifies\n"
	"\twhether AES-OCB or AES-CCM encryption is used. Default is ccm.\n"
	"\tWAPI is selected if key len is 32 and arguments contain wapi.\n"
	"\taddwep <keyindex> <keydata> [ocb | ccm | wapi] [notx] [xx:xx:xx:xx:xx:xx]" },
	{ "rmwep", wl_rmwep, -1, WLC_SET_KEY,
	"Remove the encryption key at the specified key index." },
	{ "keys", wl_keys, WLC_GET_KEY, -1,
	"Prints a list of the current WEP keys" },
	{ "tsc", wl_tsc, WLC_GET_KEY_SEQ, -1,
	"Print Tx Sequence Couter for key at specified key index." },
	{ "wsec_test", wl_wsec_test, -1, WLC_SET_WSEC_TEST,
	"Generate wsec errors\n"
	"\twsec_test <test_type> <keyindex|xx:xx:xx:xx:xx:xx>\n"
	"\ttype \'wl wsec_test ?\' for test_types" },
	{ "tkip_countermeasures", wl_int, -1, WLC_TKIP_COUNTERMEASURES,
	"Enable or disable TKIP countermeasures (TKIP-enabled AP only)\n"
	"\t0 - disable\n"
	"\t1 - enable" },
	{ "wsec_restrict", wl_bsscfg_int, WLC_GET_WEP_RESTRICT, WLC_SET_WEP_RESTRICT,
	"Drop unencrypted packets if WSEC is enabled\n"
	"\t0 - disable\n"
	"\t1 - enable" },
	{ "eap", wl_int, WLC_GET_EAP_RESTRICT, WLC_SET_EAP_RESTRICT,
	"restrict traffic to 802.1X packets until 802.1X authorization succeeds\n"
	"\t0 - disable\n"
	"\t1 - enable" },
	{ "cur_etheraddr", wl_iov_mac, -1, -1,
	"Get/set the current hw address" },
	{ "perm_etheraddr", wl_iov_mac, -1, -1,
	"Get the permanent address from NVRAM" },
	{ "authorize", wl_mac, -1, WLC_SCB_AUTHORIZE,
	"restrict traffic to 802.1X packets until 802.1X authorization succeeds" },
	{ "deauthorize", wl_mac, -1, WLC_SCB_DEAUTHORIZE,
	"do not restrict traffic to 802.1X packets until 802.1X authorization succeeds" },
	{ "deauthenticate", wl_deauth_rc, -1, WLC_SCB_DEAUTHENTICATE_FOR_REASON,
	"deauthenticate a STA from the AP with optional reason code (AP ONLY)" },
	{ "wsec", wl_wsec, WLC_GET_WSEC, WLC_SET_WSEC,
	"wireless security bit vector\n"
	"\t1 - WEP enabled\n"
	"\t2 - TKIP enabled\n"
	"\t4 - AES enabled\n"
	"\t8 - WSEC in software\n"
	"\t0x80 - FIPS enabled\n"
	"\t0x100 - WAPI enabled\n"
	"\t0x200 - MFP capable\n"
	"\t0x400 - MFP required\n"
	"\t0x800 - MFP use KDF  (SHA256)"
	},
	{ "auth", wl_bsscfg_int, WLC_GET_AUTH, WLC_SET_AUTH,
	"set/get 802.11 authentication type. 0 = OpenSystem, 1= SharedKey, 3=Open/Shared" },
	{ "wpa_auth", wl_wpa_auth, WLC_GET_WPA_AUTH, WLC_SET_WPA_AUTH,
	"Bitvector of WPA authorization modes:\n"
	"\t1	WPA-NONE\n"
	"\t2	WPA-802.1X/WPA-Professional\n"
	"\t4	WPA-PSK/WPA-Personal\n"
	"\t64	WPA2-802.1X/WPA2-Professional\n"
	"\t128	WPA2-PSK/WPA2-Personal\n"
	"\t0	disable WPA"
	},
	{ "wpa_cap", wl_bsscfg_int, WLC_GET_VAR, WLC_SET_VAR,
	"set/get 802.11i RSN capabilities" },
	{ "set_pmk", wl_set_pmk, -1, WLC_SET_WSEC_PMK,
	"Set passphrase for PMK in driver-resident supplicant." },
	{ "scan", wl_scan, -1, WLC_SCAN,
	"Initiate a scan.\n" SCAN_USAGE
	},
	{ "iscan_s", wl_iscan, -1, WLC_SET_VAR,
	"Initiate an incremental scan.\n" SCAN_USAGE
	},
	{ "iscan_c", wl_iscan, -1, WLC_SET_VAR,
	"Continue an incremental scan.\n" SCAN_USAGE
	},
	{ "scancache_clear", wl_var_void, -1, WLC_SET_VAR,
	"clear the scan cache"},
	{ "escan", wl_escan, -1, WLC_SET_VAR,
	"Start an escan.\n" SCAN_USAGE
	},
	{ "escanabort", wl_escan, -1, WLC_SET_VAR,
	"Abort an escan.\n" SCAN_USAGE
	},
#ifdef EXTENDED_SCAN
	{ "extdscan", wl_extdscan, -1, WLC_SET_VAR,
	"Initiate an extended scan.\n"
	"\tDefault to an active scan across all channels for any SSID.\n"
	"\tOptional args: list of SSIDs to scan.\n"
	"\tOptions:\n"
	"\t-s S1 S2 S3, --ssid=S1 S2 S3\t\tSSIDs to scan, comma or space separated\n"
	"\t-x x, --split_scan=ST\t[split_scan] scan type\n"
	"\t-t ST, --scan_type=ST\t[background:0/forcedbackground:1/foreground:2] scan type\n"
	"\t-n N, --nprobes=N\tnumber of probes per scanned channel, per SSID\n"
	"\t-c L, --channels=L\tcomma or space separated list of channels to scan"},
#endif
	{ "passive", wl_int, WLC_GET_PASSIVE_SCAN, WLC_SET_PASSIVE_SCAN,
	"Puts scan engine into passive mode" },
	{ "regulatory", wl_int, WLC_GET_REGULATORY, WLC_SET_REGULATORY,
	"Get/Set regulatory domain mode (802.11d). Driver must be down." },
	{ "spect", wl_spect, WLC_GET_SPECT_MANAGMENT, WLC_SET_SPECT_MANAGMENT,
	"Get/Set 802.11h Spectrum Management mode.\n"
	"\t0 - Off\n"
	"\t1 - Loose interpretation of 11h spec - may join non-11h APs\n"
	"\t2 - Strict interpretation of 11h spec - may not join non-11h APs\n"
	"\t3 - Disable 11h and enable 11d\n"
	"\t4 - Loose interpretation of 11h+d spec - may join non-11h APs"
	},
	{ "scanabort", wl_var_void, -1, WLC_SET_VAR,
	"Abort a scan." },
	{ "scanresults", wl_dump_networks, WLC_SCAN_RESULTS, -1,
	"Return results from last scan." },
	{ "iscanresults", wl_dump_networks, WLC_GET_VAR, -1,
	"Return results from last iscan. Specify a buflen (max 8188)\n"
	"\tto artificially limit the size of the results buffer.\n"
	"\tiscanresults [buflen]"},
	{ "assoc",  wl_status, -1, -1,
	"Print information about current network association.\n"
	"\t(also known as \"status\")" },
	{ "status", wl_status, -1, -1,
	"Print information about current network association.\n"
	"\t(also known as \"assoc\")" },
	{ "disassoc", wl_void, -1, WLC_DISASSOC,
	"Disassociate from the current BSS/IBSS." },
	{ "chanlist", wl_print_deprecate, WLC_GET_VALID_CHANNELS, -1,
	"Deprecated. Use channels." },
	{ "channels", wl_dump_chanlist, WLC_GET_VALID_CHANNELS, -1,
	"Return valid channels for the current settings." },
	{ "channels_in_country", wl_channels_in_country, WLC_GET_CHANNELS_IN_COUNTRY, -1,
	"Return valid channels for the country specified.\n"
	"\tArg 1 is the country abbreviation\n"
	"\tArg 2 is the band(a or b)"},
	{ "curpower", wl_get_current_power, WLC_CURRENT_PWR, -1,
	"Return current tx power settings.\n"
	"\t-v, --verbose: display the power settings for every "
	"rate even when every rate in a rate group has the same power." },
	{ "curppr", wl_get_current_txppr, WLC_GET_VAR, -1,
	"Return current tx power per rate offset."},
	{ "txinstpwr", wl_get_instant_power, WLC_GET_VAR, -1,
	"Return tx power based on instant TSSI "},
	{ "scansuppress", wl_int, WLC_GET_SCANSUPPRESS, WLC_SET_SCANSUPPRESS,
	"Suppress all scans for testing.\n"
	"\t0 - allow scans\n"
	"\t1 - suppress scans" },
	{ "evm", wl_evm, -1, WLC_EVM,
	"Start an EVM test on the given channel, or stop EVM test.\n"
	"\tArg 1 is channel number 1-14, or \"off\" or 0 to stop the test.\n"
	"\tArg 2 is optional rate (1, 2, 5.5 or 11)"},
	{ "rateset", wl_rateset, WLC_GET_RATESET, WLC_SET_RATESET,
	"Returns or sets the supported and basic rateset, (b) indicates basic\n"
	"\tWith no args, returns the rateset. Args are\n"
	"\trateset \"default\" | \"all\" | <arbitrary rateset> [-m|-v <list of mcs masks>]\n"
	"\t\tdefault - driver defaults\n"
	"\t\tall - all rates are basic rates\n"
	"\t\tarbitrary rateset - list of rates\n"
	"\tList of rates are in Mbps and each rate is optionally followed\n"
	"\tby \"(b)\" or \"b\" for a Basic rate. Example: 1(b) 2b 5.5 11\n"
	"\tAt least one rate must be Basic for a legal rateset.\n\n"
	"\t-m  sets HT rates (bitmasks, 00-ff). Least significant bit is MCS0.\n"
	"\t    example: 'rateset -m 0x3f 0x01' limits rates to MCS0-MCS5 and MCS8\n\n"
	"\t-v  sets VHT MCS values for each supported count of spatial streams.\n"
	"\t    example: 'rateset -v 3ff 1ff ff' limits vht rates to MCS 0-9 for 1 stream,\n"
	"\t                             MCS 0-8 for 2 streams, and MCS 0-7 for 3 streams."
	},
	{"txbf_rateset", wl_txbf_rateset, WLC_GET_TXBF_RATESET, WLC_SET_TXBF_RATESET,
	TXBF_RATESET_USAGE},
	{ "roam_trigger", wl_band_elm, WLC_GET_ROAM_TRIGGER, WLC_SET_ROAM_TRIGGER,
	"Get or Set the roam trigger RSSI threshold:\n"
	"\tGet: roam_trigger [a|b]\n"
	"\tSet: roam_trigger <integer> [a|b|all]\n"
	"\tinteger -   0: default\n"
	"\t            1: optimize bandwidth\n"
	"\t            2: optimize distance\n"
	"\t    [-1, -99]: dBm trigger value"},
	{ "roam_delta",	wl_band_elm, WLC_GET_ROAM_DELTA, WLC_SET_ROAM_DELTA,
	"Set the roam candidate qualification delta. roam_delta [integer [, a/b]]" },
	{ "roam_scan_period", wl_int, WLC_GET_ROAM_SCAN_PERIOD, WLC_SET_ROAM_SCAN_PERIOD,
	"Set the roam candidate qualification delta.  (integer)" },
	{ "suprates", wl_sup_rateset, WLC_GET_SUP_RATESET_OVERRIDE, WLC_SET_SUP_RATESET_OVERRIDE,
	"Returns or sets the 11g override for the supported rateset\n"
	"\tWith no args, returns the rateset. Args are a list of rates,\n"
	"\tor 0 or -1 to specify an empty rateset to clear the override.\n"
	"\tList of rates are in Mbps, example: 1 2 5.5 11"},
	{ "scan_channel_time", wl_int, WLC_GET_SCAN_CHANNEL_TIME, WLC_SET_SCAN_CHANNEL_TIME,
	"Get/Set scan channel time"},
	{ "scan_unassoc_time", wl_int, WLC_GET_SCAN_UNASSOC_TIME, WLC_SET_SCAN_UNASSOC_TIME,
	"Get/Set unassociated scan channel dwell time"},
	{ "scan_home_time", wl_int, WLC_GET_SCAN_HOME_TIME, WLC_SET_SCAN_HOME_TIME,
	"Get/Set scan home channel dwell time"},
	{ "scan_passive_time", wl_int, WLC_GET_SCAN_PASSIVE_TIME, WLC_SET_SCAN_PASSIVE_TIME,
	"Get/Set passive scan channel dwell time"},
	{ "scan_nprobes", wl_int, WLC_GET_SCAN_NPROBES, WLC_SET_SCAN_NPROBES,
	"Get/Set scan parameter for number of probes to use per channel scanned"},
	{ "prb_resp_timeout", wl_int, WLC_GET_PRB_RESP_TIMEOUT, WLC_SET_PRB_RESP_TIMEOUT,
	"Get/Set probe response timeout"},
	{ "channel_qa", wl_int, WLC_GET_CHANNEL_QA, -1,
	"Get last channel quality measurment"},
	{ "channel_qa_start", wl_void, -1, WLC_START_CHANNEL_QA,
	"Start a channel quality measurment"},
	{ "country", wl_country, WLC_GET_COUNTRY, WLC_SET_COUNTRY,
	"Select Country Code for driver operational region\n"
	"\tFor simple country setting: wl country <country>\n"
	"\tWhere <country> is either a long name or country code from ISO 3166; "
	"for example \"Germany\" or \"DE\"\n"
	"\n\tFor a specific built-in country definition: "
	"wl country <built-in> [<advertised-country>]\n"
	"\tWhere <built-in> is a country country code followed by '/' and "
	"regulatory revision number.\n"
	"\tFor example, \"US/3\".\n"
	"\tAnd where <advertised-country> is either a long name or country code from ISO 3166.\n"
	"\tIf <advertised-country> is omitted, it will be the same as the built-in country code.\n"
	"\n\tUse 'wl country list [band(a or b)]' for the list of supported countries"},
	{ "country_ie_override", wl_country_ie_override, WLC_GET_VAR, WLC_SET_VAR,
	"To set/get country ie"},
	{ "autocountry_default", wl_varstr, WLC_GET_VAR, WLC_SET_VAR,
	"Select Country Code for use with Auto Contry Discovery"},
	{ "join", wl_join, -1, -1,
	"Join a specified network SSID.\n"
	"\tUsage: join <ssid> [key <0-3>:xxxxx] [imode bss|ibss] "
	"[amode open|shared|openshared|wpa|wpapsk|wpa2|wpa2psk|wpanone|ftpsk] [options]\n"
	"\tOptions:\n"
	"\t-b MAC, --bssid=MAC \tBSSID (xx:xx:xx:xx:xx:xx) to scan and join\n"
	"\t-c CL, --chanspecs=CL \tchanspecs (comma or space separated list)\n"
	"\t-p, -passive: force passive assoc scan (useful for P2P)"},
	{ "ssid", wl_ssid, WLC_GET_SSID, WLC_SET_SSID,
	"Set or get a configuration's SSID.\n"
	"\twl ssid [-C num]|[--cfg=num] [<ssid>]\n"
	"\tIf the configuration index 'num' is not given, configuraion #0 is assumed and\n"
	"\tsetting will initiate an assoication attempt if in infrastructure mode,\n"
	"\tor join/creation of an IBSS if in IBSS mode,\n"
	"\tor creation of a BSS if in AP mode."},
	{ "mac", wl_maclist, WLC_GET_MACLIST, WLC_SET_MACLIST,
	"Set or get the list of source MAC address matches.\n"
	"\twl mac xx:xx:xx:xx:xx:xx [xx:xx:xx:xx:xx:xx ...]\n"
	"\tTo Clear the list: wl mac none" },
	{ "macmode", wl_int, WLC_GET_MACMODE, WLC_SET_MACMODE,
	"Set the mode of the MAC list.\n"
	"\t0 - Disable MAC address matching.\n"
	"\t1 - Deny association to stations on the MAC list.\n"
	"\t2 - Allow association to stations on the MAC list."},
	{ "wds", wl_maclist, WLC_GET_WDSLIST, WLC_SET_WDSLIST,
	"Set or get the list of WDS member MAC addresses.\n"
	"\tSet using a space separated list of MAC addresses.\n"
	"\twl wds xx:xx:xx:xx:xx:xx [xx:xx:xx:xx:xx:xx ...]" },
	{ "lazywds", wl_int, WLC_GET_LAZYWDS, WLC_SET_LAZYWDS,
	"Set or get \"lazy\" WDS mode (dynamically grant WDS membership to anyone)."},
	{ "noise", wl_int, WLC_GET_PHY_NOISE, -1,
	"Get noise (moving average) right after tx in dBm" },
	{ "fqacurcy", wl_int, -1, WLC_FREQ_ACCURACY,
	"Manufacturing test: set frequency accuracy mode.\n"
	"\tfreqacuracy syntax is: fqacurcy <channel>\n"
	"\tArg is channel number 1-14, or 0 to stop the test." },
	{ "crsuprs", wl_int, -1, WLC_CARRIER_SUPPRESS,
	"Manufacturing test: set carrier suppression mode.\n"
	"\tcarriersuprs syntax is: crsuprs <channel>\n"
	"\tArg is channel number 1-14, or 0 to stop the test." },
	{ "longtrain", wl_int, -1, WLC_LONGTRAIN,
	"Manufacturing test: set longtraining mode.\n"
	"\tlongtrain syntax is: longtrain <channel>\n"
	"\tArg is A band channel number or 0 to stop the test." },
#endif /* !ATE_BUILD */
	{ "band", wl_band, WLC_GET_BAND, WLC_SET_BAND,
	"Returns or sets the current band\n"
	"\tauto - auto switch between available bands (default)\n"
	"\ta - force use of 802.11a band\n"
	"\tb - force use of 802.11b band" },
#ifndef ATE_BUILD
	{ "bands", wl_bandlist, WLC_GET_BANDLIST, -1,
	"Return the list of available 802.11 bands" },
	{ "phylist", wl_phylist, WLC_GET_PHYLIST, -1,
	"Return the list of available phytypes" },
	{ "shortslot", wl_int, WLC_GET_SHORTSLOT, -1,
	"Get current 11g Short Slot Timing mode. (0=long, 1=short)" },
	{ "shortslot_override", wl_int, WLC_GET_SHORTSLOT_OVERRIDE, WLC_SET_SHORTSLOT_OVERRIDE,
	"Get/Set 11g Short Slot Timing mode override. (-1=auto, 0=long, 1=short)" },
	{ "shortslot_restrict", wl_int, WLC_GET_SHORTSLOT_RESTRICT, WLC_SET_SHORTSLOT_RESTRICT,
	"Get/Set AP Restriction on associations for 11g Short Slot Timing capable STAs.\n"
	"\t0 - Do not restrict association based on ShortSlot capability\n"
	"\t1 - Restrict association to STAs with ShortSlot capability" },
	{ "ignore_bcns", wl_int, WLC_GET_IGNORE_BCNS, WLC_SET_IGNORE_BCNS,
	"AP only (G mode): Check for beacons without NONERP element"
	"(0=Examine beacons, 1=Ignore beacons)" },
	{ "pktcnt", wl_get_pktcnt, WLC_GET_PKTCNTS, -1,
	"Get the summary of good and bad packets." },
	{ "upgrade", wl_upgrade, -1, WLC_UPGRADE,
	"Upgrade the firmware on an embedded device" },
	{ "gmode", wl_gmode, WLC_GET_GMODE, WLC_SET_GMODE,
	"Set the 54g Mode (LegacyB|Auto||GOnly|BDeferred|Performance|LRS)" },
	{ "gmode_protection", wl_int, WLC_GET_GMODE_PROTECTION, -1,
	"Get G protection mode. (0=disabled, 1=enabled)" },
	{ "gmode_protection_control", wl_int, WLC_GET_PROTECTION_CONTROL,
	WLC_SET_PROTECTION_CONTROL,
	"Get/Set 11g protection mode control alg."
	"(0=always off, 1=monitor local association, 2=monitor overlapping BSS)" },
	{ "gmode_protection_override", wl_int, WLC_GET_GMODE_PROTECTION_OVERRIDE,
	WLC_SET_GMODE_PROTECTION_OVERRIDE,
	"Get/Set 11g protection mode override. (-1=auto, 0=disable, 1=enable)" },
	{ "protection_control", wl_int, WLC_GET_PROTECTION_CONTROL,
	WLC_SET_PROTECTION_CONTROL,
	"Get/Set protection mode control alg."
	"(0=always off, 1=monitor local association, 2=monitor overlapping BSS)" },
	{ "legacy_erp", wl_int, WLC_GET_LEGACY_ERP, WLC_SET_LEGACY_ERP,
	"Get/Set 11g legacy ERP inclusion (0=disable, 1=enable)" },
	{ "scb_timeout", wl_int, WLC_GET_SCB_TIMEOUT, WLC_SET_SCB_TIMEOUT,
	"AP only: inactivity timeout value for authenticated stas" },
	{ "assoclist", wl_maclist, WLC_GET_ASSOCLIST, -1,
	"AP only: Get the list of associated MAC addresses."},
	{ "isup", wl_int, WLC_GET_UP, -1,
	"Get driver operational state (0=down, 1=up)"},
	{ "rssi", wl_rssi, WLC_GET_RSSI, -1,
	"Get the current RSSI val, for an AP you must specify the mac addr of the STA" },
	{ "rssi_event", wl_rssi_event, WLC_GET_VAR, WLC_SET_VAR,
	"Set parameters associated with RSSI event notification\n"
	"\tusage: wl rssi_event <rate_limit> <rssi_levels>\n"
	"\trate_limit: Number of events posted to application will be limited"
	" to 1 per this rate limit. Set to 0 to disable rate limit.\n"
	"\trssi_levels: Variable number of RSSI levels (maximum 8) "
	" in increasing order (e.g. -85 -70 -60). An event will be posted"
	" each time the RSSI of received beacons/packets crosses a level."},
	{ "fasttimer", wl_print_deprecate, -1, -1,
	"Deprecated. Use fast_timer."},
	{ "slowtimer", wl_print_deprecate, -1, -1,
	"Deprecated. Use slow_timer."},
	{ "glacialtimer", wl_print_deprecate, -1, -1,
	"Deprecated. Use glacial_timer."},
	{ "radar", wl_int, WLC_GET_RADAR, WLC_SET_RADAR,
	"Enable/Disable radar"},
	{ "radarargs", wl_radar_args, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set Radar parameters in \n"
	"\torder as version, npulses, ncontig, min_pw, max_pw, thresh0,\n"
	"\tthresh1, blank, fmdemodcfg, npulses_lp, min_pw_lp, max_pw_lp,\n"
	"\tmin_fm_lp, max_span_lp, min_deltat, max_deltat,\n"
	"\tautocorr, st_level_time, t2_min, fra_pulse_err, npulses_fra,\n"
	"\tnpulses_stg2, npulses_stg3, percal_mask, quant, \n"
	"\tmin_burst_intv_lp, max_burst_intv_lp, nskip_rst_lp, max_pw_tol, feature_mask"},
	{ "radarargs40", wl_radar_args, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set Radar parameters for 40Mhz channel in \n"
	"\torder as version, npulses, ncontig, min_pw, max_pw, thresh0,\n"
	"\tthresh1, blank, fmdemodcfg, npulses_lp, min_pw_lp, max_pw_lp,\n"
	"\tmin_fm_lp, max_span_lp, min_deltat, max_deltat,\n"
	"\tautocorr, st_level_time, t2_min, fra_pulse_err, npulses_fra,\n"
	"\tnpulses_stg2, npulses_stg3, percal_mask, quant, \n"
	"\tmin_burst_intv_lp, max_burst_intv_lp, nskip_rst_lp, max_pw_tol, feature_mask"},
	{ "radarthrs", wl_radar_thrs, -1, WLC_SET_VAR,
	"Set Radar threshold for both 20 & 40MHz & 80MHz BW:\n"
	"\torder as thresh0_20_lo, thresh1_20_lo, thresh0_40_lo, thresh1_40_lo\n"
	"\tthresh0_80_lo, thresh1_80_lo, thresh0_20_hi, thresh1_20_hi\n"
	"\tthresh0_40_hi, thresh1_40_hi, thresh0_80_hi, thresh1_80_hi\n"},
	{ "dfs_status", wl_dfs_status, WLC_GET_VAR, -1,
	"Get dfs status"},
	{ "interference", wl_interfere, WLC_GET_INTERFERENCE_MODE, WLC_SET_INTERFERENCE_MODE,
	"Get/Set interference mitigation mode. Choices are:\n"
	"\t0 = none\n"
	"\t1 = non wlan\n"
	"\t2 = wlan manual\n"
	"\t3 = wlan automatic\n"
	"\t4 = wlan automatic with noise reduction"},
	{ "interference_override", wl_interfere_override,
	WLC_GET_INTERFERENCE_OVERRIDE_MODE,
	WLC_SET_INTERFERENCE_OVERRIDE_MODE,
	"Get/Set interference mitigation override. Choices are:\n"
	"\t0 = no interference mitigation\n"
	"\t1 = non wlan\n"
	"\t2 = wlan manual\n"
	"\t3 = wlan automatic\n"
	"\t4 = wlan automatic with noise reduction\n"
	"\t-1 = remove override, override disabled"},
	{ "frameburst", wl_int, WLC_GET_FAKEFRAG, WLC_SET_FAKEFRAG,
	"Disable/Enable frameburst mode" },
	{ "pwr_percent", wl_int, WLC_GET_PWROUT_PERCENTAGE, WLC_SET_PWROUT_PERCENTAGE,
	"Get/Set power output percentage"},
	{ "toe", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/Disable tcpip offload feature"},
	{ "toe_ol", wl_offload_cmpnt, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set tcpip offload components"},
	{ "toe_stats", wl_toe_stats, WLC_GET_VAR, -1,
	"Display checksum offload statistics"},
	{ "toe_stats_clear", wl_var_void, -1, WLC_SET_VAR,
	"Clear checksum offload statistics"},
	{ "arpoe", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/Disable arp agent offload feature"},
	{ "arp_ol", wl_offload_cmpnt, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set arp offload components"},
	{ "arp_peerage", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set age of the arp entry in minutes"},
	{ "arp_table_clear", wl_var_void, -1, WLC_SET_VAR,
	"Clear arp cache"},
	{ "ol_notify_bcn_ie", wl_ol_notify_bcn_ie, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/Disable IE ID notification"},
	{ "arp_hostip", wl_hostip, WLC_GET_VAR, WLC_SET_VAR,
	"Add a host-ip address or display them"},
	{ "arp_hostip_clear", wl_var_void, -1, WLC_SET_VAR,
	"Clear all host-ip addresses"},
	{ "arp_stats", wl_arp_stats, WLC_GET_VAR, -1,
	"Display ARP offload statistics"},
	{ "arp_stats_clear", wl_var_void, -1, WLC_SET_VAR,
	"Clear ARP offload statistics"},
	{ "wet", wl_int, WLC_GET_WET, WLC_SET_WET,
	"Get/Set wireless ethernet bridging mode"},
	{ "bi", wl_int, WLC_GET_BCNPRD, WLC_SET_BCNPRD,
	"Get/Set the beacon period (bi=beacon interval)"},
	{ "dtim", wl_int, WLC_GET_DTIMPRD, WLC_SET_DTIMPRD,
	"Get/Set DTIM"},
	{ "wds_remote_mac", wl_mac, WLC_WDS_GET_REMOTE_HWADDR, -1,
	"Get WDS link remote endpoint's MAC address"},
	{ "wds_wpa_role_old", wl_wds_wpa_role_old, WLC_WDS_GET_WPA_SUP, -1,
	"Get WDS link local endpoint's WPA role (old)"},
	{ "wds_wpa_role", wl_wds_wpa_role, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set WDS link local endpoint's WPA role"},
	{ "authe_sta_list", wl_maclist_1, WLC_GET_VAR, -1,
	"Get authenticated sta mac address list"},
	{ "autho_sta_list", wl_maclist_1, WLC_GET_VAR, -1,
	"Get authorized sta mac address list"},
	{ "measure_req", wl_measure_req, -1, WLC_MEASURE_REQUEST,
	"Send an 802.11h measurement request.\n"
	"\tUsage: wl measure_req <type> <target MAC addr>\n"
	"\tMeasurement types are: TPC, Basic, CCA, RPI\n"
	"\tTarget MAC addr format is xx:xx:xx:xx:xx:xx"},
	{ "quiet", wl_send_quiet, -1, WLC_SEND_QUIET,
	"Send an 802.11h quiet command.\n"
	"\tUsage: wl quiet <TBTTs until start>, <duration (in TUs)>, <offset (in TUs)>"},
	{ "csa", wl_send_csa, -1, WLC_SET_VAR,
	"Send an 802.11h channel switch anouncement with chanspec:\n"
	"\t<mode> <count> <channel>[a,b][n][u,l][frame type]\n"
	"\tmode (0 or 1)\n"
	"\tcount (0-254)\n"
	"\tchannel format:\n"
	"\t20MHz : [2g|5g]<channel>[/20]\n"
	"\t40MHz : [2g|5g]<channel>/40[u,l]\n"
	"\t80MHz :    [5g]<channel>/80\n"
	"\toptional band 2g or 5g, default to 2g if channel <= 14\n"
	"\tchannel number (0-200)\n"
	"\tbandwidth, 20, 40, or 80, default 20\n"
	"\tprimary sideband for 40MHz on 2g, l=lower, u=upper\n"
	"\tcsa frame type(optional), default is broadcast if not specified, u=unicast"},
	{ "constraint", wl_int, -1, WLC_SEND_PWR_CONSTRAINT,
	"Send an 802.11h Power Constraint IE\n"
	"\tUsage: wl constraint 1-255 db"},
	{ "rm_req", wl_rm_request, -1, WLC_SET_VAR,
	"Request a radio measurement of type basic, cca, or rpi\n"
	"\tspecify a series of measurement types each followed by options.\n"
	"\texample: wl rm_req cca -c 1 -d 50 cca -c 6 cca -c 11\n"
	"\tOptions:\n"
	"\t-t n  numeric token id for measurement set or measurement\n"
	"\t-c n  channel\n"
	"\t-d n  duration in TUs (1024 us)\n"
	"\t-p    parallel flag, measurement starts at the same time as previous\n"
	"\n"
	"\tEach measurement specified uses the same channel and duration as the\n"
	"\tprevious unless a new channel or duration is specified."},
	{ "rm_rep", wl_rm_report, WLC_GET_VAR, -1,
	"Get current radio measurement report"},
	{ "join_pref", wl_join_pref, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get join target preferences."},
	{ "assoc_pref", wl_assoc_pref, WLC_GET_ASSOC_PREFER, WLC_SET_ASSOC_PREFER,
	"Set/Get association preference.\n"
	"Usage: wl assoc_pref [auto|a|b|g]"},
	{ "wme", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Set WME (Wireless Multimedia Extensions) mode (0=off, 1=on, -1=auto)"},
	{ "wme_ac", wl_wme_ac_req, WLC_GET_VAR, WLC_SET_VAR,
	"wl wme_ac ap|sta [be|bk|vi|vo [ecwmax|ecwmin|txop|aifsn|acm <value>] ...]"},
	{ "wme_apsd", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Set APSD (Automatic Power Save Delivery) mode on AP (0=off, 1=on)" },
	{ "wme_apsd_sta", wl_wme_apsd_sta, WLC_GET_VAR, WLC_SET_VAR,
	"Set APSD parameters on STA. Driver must be down.\n"
	"Usage: wl wme_apsd_sta <max_sp_len> <be> <bk> <vi> <vo>\n"
	"   <max_sp_len>: number of frames per USP: 0 (all), 2, 4, or 6\n"
	"   <xx>: value 0 to disable, 1 to enable U-APSD per AC" },
	{ "wme_dp", wl_wme_dp, WLC_GET_VAR, WLC_SET_VAR,
	"Set AC queue discard policy.\n"
	"Usage: wl wme_dp <be> <bk> <vi> <vo>\n"
	"   <xx>: value 0 for newest-first, 1 for oldest-first" },
	{ "wme_counters", wl_wme_counters, WLC_GET_VAR, -1,
	"print WMM stats" },
	{ "wme_clear_counters", wl_var_void, -1, WLC_SET_VAR,
	"clear WMM counters"},
	{ "wme_tx_params", wme_tx_params, -1, -1,
	"wl wme_tx_params [be|bk|vi|vo [short|sfb|long|lfb|max_rate <value>] ...]"},
	{ "wme_maxbw_params", wme_maxbw_params, WLC_GET_VAR, WLC_SET_VAR,
	"wl wme_maxbw_params [be|bk|vi|vo <value> ....]"},
	{ "lifetime", wl_lifetime, WLC_GET_VAR, WLC_SET_VAR,
	"Set Lifetime parameter (milliseconds) for each ac.\n"
	"wl lifetime be|bk|vi|vo [<value>]"},
	{ "reinit", wl_void, -1, WLC_INIT,
	"Reinitialize device"},
	{ "sta_info", wl_sta_info, WLC_GET_VAR, -1,
	"wl sta_info <xx:xx:xx:xx:xx:xx>"},
	{ "pktq_stats", wl_iov_mac_params, WLC_GET_VAR, -1,
	"Dumps packet queue log info for [C] common, [A] AMPDU or [P] power save queues\n"
	"A: or P: are used to prefix a MAC address (a colon : separator is necessary),\n"
	"or else C: is used alone. The '+' option after the colon gives more details.\n"
	"Up to 4 parameters may be given, the common queue is default when no parameters\n"
	"are supplied\n"
	"wl pktq_stats [C:[+]]|[A:[+]|P:[+]<xx:xx:xx:xx:xx:xx>]..." },
	{ "cap", wl_var_getandprintstr, WLC_GET_VAR, -1, "driver capabilities"},
	{ "malloc_dump", wl_print_deprecate, -1, -1, "Deprecated. Folded under 'wl dump malloc"},
	{ "chan_info", wl_chan_info, WLC_GET_VAR, -1, "channel info"},
	{ "add_ie", wl_add_ie, -1, WLC_SET_VAR,
	"Add a vendor proprietary IE to 802.11 management packets\n"
	"Usage: wl add_ie <pktflag> length OUI hexdata\n"
	"<pktflag>: Bit 0 - Beacons\n"
	"           Bit 1 - Probe Rsp\n"
	"           Bit 2 - Assoc/Reassoc Rsp\n"
	"           Bit 3 - Auth Rsp\n"
	"           Bit 4 - Probe Req\n"
	"           Bit 5 - Assoc/Reassoc Req\n"
	"Example: wl add_ie 3 10 00:90:4C 0101050c121a03\n"
	"         to add this IE to beacons and probe responses" },
	{ "del_ie", wl_del_ie, -1, WLC_SET_VAR,
	"Delete a vendor proprietary IE from 802.11 management packets\n"
	"Usage: wl del_ie <pktflag> length OUI hexdata\n"
	"<pktflag>: Bit 0 - Beacons\n"
	"           Bit 1 - Probe Rsp\n"
	"           Bit 2 - Assoc/Reassoc Rsp\n"
	"           Bit 3 - Auth Rsp\n"
	"           Bit 4 - Probe Req\n"
	"           Bit 5 - Assoc/Reassoc Req\n"
	"Example: wl del_ie 3 10 00:90:4C 0101050c121a03" },
	{ "list_ie", _wl_list_ie, WLC_GET_VAR, -1,
	"Dump the list of vendor proprietary IEs" },
	{ "rand", wl_rand, WLC_GET_VAR, -1,
	"Get a 2-byte Random Number from the MAC's PRNG\n"
	"Usage: wl rand"},
	{ "otpw", wl_otpw, -1, WLC_OTPW,
	"Write an srom image to on-chip otp\n"
	"Usage: wl otpw file"},
	{ "nvotpw", wl_otpw, -1, WLC_NVOTPW,
	"Write nvram to on-chip otp\n"
	"Usage: wl nvotpw file"},
	{ "bcmerrorstr", wl_var_getandprintstr, WLC_GET_VAR, -1, "errorstring"},
	{ "freqtrack", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Set Frequency Tracking Mode (0=Auto, 1=On, 2=OFF)"},
	{ "eventing", wl_bitvec128, WLC_GET_VAR, WLC_SET_VAR,
	"set/get 128-bit hex filter bitmask for MAC event reporting up to application layer"},
	{ "event_msgs", wl_bitvec128, WLC_GET_VAR, WLC_SET_VAR,
	"set/get 128-bit hex filter bitmask for MAC event reporting via packet indications"},
	{ "counters", wl_counters, WLC_GET_VAR, -1,
	"Return driver counter values" },
	{ "delta_stats_interval", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"set/get the delta statistics interval in seconds (0 to disable)"},
	{ "delta_stats", wl_delta_stats, WLC_GET_VAR, -1,
	"get the delta statistics for the last interval" },
	{ "assoc_info", wl_assoc_info, WLC_GET_VAR, -1,
	"Returns the assoc req and resp information [STA only]" },
	{ "beacon_info", wl_management_info, WLC_GET_VAR, -1,
	"Returns the 802.11 management frame beacon information\n"
	"Usage: wl beacon_info [-f file] [-r]\n"
	"\t-f Write beacon data to file\n"
	"\t-r Raw hex dump of beacon data" },
	{ "probe_resp_info", wl_management_info, WLC_GET_VAR, -1,
	"Returns the 802.11 management frame probe response information\n"
	"Usage: wl probe_resp_info [-f file] [-r]\n"
	"\t-f Write probe response data to file\n"
	"\t-r Raw hex dump of probe response data" },
	{ "autochannel", wl_auto_channel_sel, WLC_GET_CHANNEL_SEL, WLC_START_CHANNEL_SEL,
	"auto channel selection: \n"
	"\t1 to issue a channel scanning;\n"
	"\t2 to set chanspec based on the channel scan result;\n"
	"\twithout argument to only show the chanspec selected; \n"
	"\tssid must set to null before this process, RF must be up"},
	{ "csscantimer", wl_int, WLC_GET_CS_SCAN_TIMER, WLC_SET_CS_SCAN_TIMER,
	"auto channel scan timer in minutes (0 to disable)" },
	{ "closed", wl_int, WLC_GET_CLOSED, WLC_SET_CLOSED,
	"hides the network from active scans, 0 or 1.\n"
	"\t0 is open, 1 is hide" },
	{ "pmkid_info", wl_pmkid_info, WLC_GET_VAR, WLC_SET_VAR,
	"Returns the pmkid table" },
	{ "bss", wl_bsscfg_enable, WLC_GET_VAR, WLC_SET_VAR,
	"set/get BSS enabled status: up/down"},
	{ "closednet", wl_bsscfg_int, WLC_GET_VAR, WLC_SET_VAR,
	"set/get BSS closed network attribute"},
	{ "ap_isolate", wl_bsscfg_int, WLC_GET_VAR, WLC_SET_VAR,
	"set/get AP isolation"},
	{ "eap_restrict", wl_bsscfg_int, WLC_GET_VAR, WLC_SET_VAR,
	"set/get EAP restriction"},
	{ "diag", wl_diag, WLC_GET_VAR, -1,
	"diag testindex(1-interrupt, 2-loopback, 3-memory, 4-led);"
	" precede by 'wl down' and follow by 'wl up'" },
	{ "reset_d11cnts", wl_var_void, -1, WLC_SET_VAR,
	"reset 802.11 MIB counters"},
	{ "staname", wl_varstr, WLC_GET_VAR, WLC_SET_VAR,
	"get/set station name: \n"
	"\tMaximum name length is 15 bytes"},
	{ "apname", wl_varstr, WLC_GET_VAR, -1,
	"get AP name"},
	{ "otpdump", wl_var_setintandprintstr, WLC_GET_VAR, -1,
	"Dump raw otp"},
	{ "otpstat", wl_var_setintandprintstr, WLC_GET_VAR, -1,
	"Dump OTP status"},
#endif /* !ATE_BUILD */
	{ "nrate", wl_nrate, WLC_GET_VAR, WLC_SET_VAR,
	"\"auto\" to clear a rate override, or:\n"
	"-r legacy rate (CCK, OFDM)\n"
	"-m HT MCS index\n"
	"-s stf mode (0=SISO,1=CDD,2=STBC,3=SDM)\n"
	"-w Override MCS only to support STA's with/without STBC capability"},
#ifndef ATE_BUILD
	{ "mimo_txbw", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"get/set mimo txbw (2=20Mhz(lower), 3=20Mhz upper, 4=40Mhz)"},
	{ "cac_addts", wl_cac, -1, WLC_SET_VAR,
	"add TSPEC, error if STA is not associated or WME is not enabled\n"
	"\targ: TSPEC parameter input list"},
	{ "cac_delts", wl_cac, -1, WLC_SET_VAR,
	"delete TSPEC, error if STA is not associated or WME is not enabled\n"
	"\targ: TSINFO for the target tspec"},
	{ "cac_delts_ea", wl_cac_delts_ea, -1, WLC_SET_VAR,
	"delete TSPEC, error if STA is not associated or WME is not enabled\n"
	"\targ1: Desired TSINFO for the target tspec\n"
	"\targ2: Desired MAC address"},
	{ "cac_tslist", wl_tslist, WLC_GET_VAR, -1,
	"Get the list of TSINFO in driver\n"
	"\teg. 'wl cac_tslist' get a list of TSINFO"},
	{ "cac_tslist_ea", wl_tslist_ea, WLC_GET_VAR, -1,
	"Get the list of TSINFO for given STA in driver\n"
	"\teg. 'wl cac_tslist_ea ea' get a list of TSINFO"},
	{ "cac_tspec", wl_tspec, WLC_GET_VAR, -1,
	"Get specific TSPEC with matching TSINFO\n"
	"\teg. 'wl cac_tspec 0xaa 0xbb 0xcc' where 0xaa 0xbb & 0xcc are TSINFO octets"},
	{ "cac_tspec_ea", wl_tspec_ea, WLC_GET_VAR, -1,
	"Get specific TSPEC for given STA with matching TSINFO\n"
	"\teg. 'wl cac_tspec 0xaa 0xbb 0xcc xx:xx:xx:xx:xx:xx'\n"
	"\t    where 0xaa 0xbb & 0xcc are TSINFO octets and xx is mac address"},


	{ "overlay", wl_overlay, WLC_GET_VAR, WLC_SET_VAR,
	"overlay virt_addr phy_addr size"},
#endif /* !ATE_BUILD */
	{ "phy_txpwrindex", wl_phy_txpwrindex, WLC_GET_VAR, WLC_SET_VAR,
	"usage: (set) phy_txpwrindex core0_idx core1_idx core2_idx core3_idx"
	"       (get) phy_txpwrindex, return format: core0_idx core1_idx core2_idx core3_idx"
	"Set/Get txpwrindex"
	},
#ifndef ATE_BUILD
	{ "phy_test_tssi", wl_test_tssi, WLC_GET_VAR, -1,
	"wl phy_test_tssi val"},
	{ "phy_test_tssi_offs", wl_test_tssi_offs, WLC_GET_VAR, -1,
	"wl phy_test_tssi_offs val"},
	{ "phy_rssiant", wl_phy_rssiant, WLC_GET_VAR, -1,
	"wl phy_rssiant antindex(0-3)"},
	{ "phy_rssi_ant", wl_phy_rssi_ant, WLC_GET_VAR, WLC_SET_VAR,
	"Get RSSI per antenna (only gives RSSI of current antenna for SISO PHY)"},
	{ "lpphy_papdepstbl", wl_phy_papdepstbl, -1, WLC_GET_VAR,
	"print papd eps table; Usage: wl lpphy_papdepstbl"
	},
	{ "lcnphy_papdepstbl", wl_phy_papdepstbl, -1, WLC_GET_VAR,
	"print papd eps table; Usage: wl lcnphy_papdepstbl"
	},
#if defined(BCMDBG)
	{ "phy_force_gainlevel", wl_phy_debug_cmd, WLC_GET_VAR, WLC_SET_VAR,
	"Force rxgain level \n"
	"\t 0 : force to init gain\n"
	"\t 1 : force to clip hi gain\n"
	"\t 2 : force to clip md gain\n"
	"\t 3 : force to clip lo gain\n"
	"\t 4 : force to adc clip gain\n"
	"\t 5 : force to nb clip gain\n"
	"\t 6 : force to wb clip gain\n"
	"\t -1 : disable\n"
	"\t usage: wl phy_force_gainlevel <int32 var>"
	},
#endif
#if defined(BCMDBG)
	{ "phy_force_fdiqi", wl_phy_debug_cmd, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/disable FDIQI Cal/Comp \n"
	"\t 0 : disable\n"
	"\t 1 : enable\n"
	"\t usage: wl phy_force_fdiqi <int32 var>"
	},
#endif
#if defined(BCMDBG)
	{ "phy_btcoex_desense", wl_phy_debug_cmd, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/disable btcoex desense\n"
	"\t 0 : disable\n"
	"\t 1 : mode 1\n"
	"\t usage: wl phy_btcoex_desense <int32 var>"
	},
#endif
	{ "rifs", wl_rifs, WLC_GET_VAR, WLC_SET_VAR,
	"set/get the rifs status; usage: wl rifs <1/0> (On/Off)"
	},
	{ "rifs_advert", wl_rifs_advert, WLC_GET_VAR, WLC_SET_VAR,
	"set/get the rifs mode advertisement status; usage: wl rifs_advert <-1/0> (Auto/Off)"
	},
	{ "phy_rxiqest", wl_rxiq, WLC_GET_VAR, -1,
	"Get phy RX IQ noise in dBm:\n"
	"\t-s # of samples (2^n)\n"
	"\t-a antenna select, 0,1 or 3\n"
	  "\t-r resolution select, 0 (coarse) or 1 (fine)\n"
	  "\t-f lpf hpc override select, 0 (hpc unchanged) or 1 (overridden to ltrn mode)\n"
	  "\t-w dig lpf override select, 0 (lpf unchanged) or 1 (overridden to ltrn_lpf mode)"
	  "\t or 2 (bypass)\n"
	  "\t-g gain-correction select, 0 (disable) or 1 (enable)\n"
	  "\t-e extra INITgain in dB on top of default. Valid values = {0, 3, 6, .., 21, 24}"
	},
	{ "phy_txiqcc", wl_phy_txiqcc, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_txiqcc [a b]\n"
	"Set/get the iqcc a, b values"
	},
	{ "phy_txlocc", wl_phy_txlocc, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_txlocc [di dq ei eq fi fq]\n"
	"Set/get locc di dq ei eq fi fq values"
	},
	{ "phytable", wl_phytable, WLC_GET_VAR, WLC_SET_VAR,
	"usage: wl phytable table_id offset width_of_table_element [table_element]\n"
	"Set/get table element of a table with the given ID at the given offset\n"
	"Note that table width supplied should be 8 or 16 or 32\n"
	"table ID, table offset can not be negative"
	},
	{ "pavars", wl_phy_pavars, WLC_GET_VAR, WLC_SET_VAR,
	"Set/get temp PA parameters\n"
	"usage: wl down\n"
	"       wl pavars pa2gw0a0=0x1 pa2gw1a0=0x2 pa2gw2a0=0x3 ... \n"
	"       wl pavars\n"
	"       wl up\n"
	"  override the PA parameters after driver attach(srom read), before diver up\n"
	"  These override values will be propogated to HW when driver goes up\n"
	"  PA parameters in one band range (2g, 5gl, 5g, 5gh) must all present if\n"
	"  one of them is specified in the command, otherwise it will be filled with 0"
	},
	{ "povars", wl_phy_povars, WLC_GET_VAR, WLC_SET_VAR,
	"Set/get temp power offset\n"
	"usage: wl down\n"
	"       wl povars cck2gpo=0x1 ofdm2gpo=0x2 mcs2gpo=0x3 ... \n"
	"       wl povars\n"
	"       wl up\n"
	"  override the power offset after driver attach(srom read), before diver up\n"
	"  These override values will be propogated to HW when driver goes up\n"
	"  power offsets in one band range (2g, 5gl, 5g, 5gh) must all present if\n"
	"  one of them is specified in the command, otherwise it will be filled with 0"
	"  cck(2g only), ofdm, and mcs(0-7) for NPHY are supported "
	},
	{ "fem", wl_phy_fem, WLC_GET_VAR, WLC_SET_VAR,
	"Set temp fem2g/5g value\n"
	"usage: wl fem (tssipos2g=0x1 extpagain2g=0x2 pdetrange2g=0x1 triso2g=0x1 antswctl2g=0)\n"
	"	(tssipos5g=0x1 extpagain5g=0x2 pdetrange5g=0x1 triso5g=0x1 antswctl5g=0)"
	},
	{ "antgain", wl_antgain, WLC_GET_VAR, WLC_SET_VAR,
	"Set temp ag0/1 value\n"
	"usage: wl antgain ag0=0x1 ag1=0x2"
	},
	{ "maxpower", wl_phy_maxpower, WLC_GET_VAR, WLC_SET_VAR,
	"Set temp maxp2g(5g)a0(a1) value\n"
	"usage: wl maxpower maxp2ga0=0x1 maxp2ga1=0x2 maxp5ga0=0xff maxp5ga1=0xff\n"
	"       maxp5gla0=0x3 maxp5gla1=0x4 maxp5gha0=0x5 maxp5gha1=0x6"
	},

	{ "phy_antsel", wl_antsel, WLC_GET_VAR, -1,
	"get/set antenna configuration \n"
	"\tset: -1(AUTO), 0xAB(fixed antenna selection)\n"
	"\t\twhere A and B is the antenna numbers used for RF chain 1 and 0 respectively\n"
	"\tquery: <utx>[AUTO] <urx>[AUTO] <dtx>[AUTO] <drx>[AUTO]\n"
	"\t\twhere utx = TX unicast antenna configuration\n"
	"\t\t\turx = RX unicast antenna configuration\n"
	"\t\t\tdtx = TX default (non-unicast) antenna configuration\n"
	"\t\t\tdrx = RX default (non-unicast) antenna configuration"
	},
	{ "txcore", wl_txcore, WLC_GET_VAR, WLC_SET_VAR,
	"Usage: wl txcore -k <CCK core mask> -o <OFDM core mask> -s <1..4> -c <core bitmap>\n"
	"\t-k CCK core mask\n"
	"\t-o OFDM core mask\n"
	"\t-s # of space-time-streams\n"
	"\t-c active core (bitmask) to be used when transmitting frames"
	},
	{ "txcore_override", wl_txcore, WLC_GET_VAR, -1,
	"Usage: wl txcore_override\n"
	"\tget the user override of txcore"
	},
	{ "txchain_pwr_offset", wl_txcore_pwr_offset, WLC_GET_VAR, WLC_SET_VAR,
	"Usage: wl txchain_pwr_offset [qdBm offsets]\n"
	"\tGet/Set the current offsets for each core in qdBm (quarter dBm)"
	},
	{ "sample_collect", wl_sample_collect, WLC_PHY_SAMPLE_COLLECT, -1,
	"Optional parameters HTPHY/(NPHY with NREV >= 7) are:\n"
	"\t-f File name to dump the sample buffer (default \"sample_collect.dat\")\n"
	"\t-t Trigger condition (default now)\n"
	"\t\t now, good_fcs, bad_fcs, bad_plcp, crs, crs_glitch, crs_deassert\n"
	"\t-b PreTrigger duration in us (default 10)\n"
	"\t-a PostTrigger duration in us (default 10) \n"
	"\t-m Sample collect mode (default 1) \n"
	"\t\t HTPHY: 0=adc, 1..3=adc+rssi, 4=gpio\n"
	"\t\t NPHY: 1=Dual-Core adc[9:2], 2=Core0 adc[9:0], 3=Core1 adc[9:0], gpio=gpio\n"
	"\t-g GPIO mux select (default 0)\n"
	"\t\t use only for gpio mode\n"
	"\t-d Downsample enable (default 0)\n"
	"\t\t use only for HTPHY\n"
	"\t-e BeDeaf enable (default 0)\n"
	"\t-i Timeout in units of 10us (default 1000)\n"
	"Optional parameters (NPHY with NREV < 7) are:\n"
	"\t-f File name to dump the sample buffer (binary format, default \"sample_collect.dat\")\n"
	"\t-u Sample collect duration in us (default 60)\n"
	"\t-c Cores to do sample collect, only if BW=40MHz (default both)\n"
	"For (NREV < 7), the NPHY buffer returned has the format:\n"
	"\tIn 20MHz [(uint16)num_bytes, <I(core0), Q(core0), I(core1), Q(core1)>]\n"
	"\tIn 40MHz [(uint16)num_bytes(core0), <I(core0), Q(core0)>,\n"
	"\t\t(uint16)num_bytes(core1), <I(core1), Q(core1)>]"},
#ifdef PLC
	{ "plc", wl_plc, WLC_GET_VAR, WLC_SET_VAR,
	"set/get the plc config params; usage: wl plc [[0|1] [node_list]"
	"[link_affinity|mac_affinity <etheraddr> [<affinity>]]]" },
#endif /* PLC */
	{ "txfifo_sz", wl_txfifo_sz, WLC_GET_VAR, WLC_SET_VAR,
	"set/get the txfifo size; usage: wl txfifo_sz <fifonum> <size_in_bytes>" },
#ifdef WLPFN
	{ "pfnset", wl_pfn_set, -1, -1,
	"Configures preferred network offload parameter\n"
	"\tpfnset syntax is: pfnset [scanfrq xxxxx(30 sec)] [netimeout xxxx(60 sec)]"
	"[slowfrq xxxx(180 sec)]"
	"[bestn (2)|[1-3]] [mscan (0)|[0-90]] [bdscan (0)|1] [adapt (off)|[smart, strict, slow]]"
	"[rssi_delta xxxx(30 dBm)] [sort (listorder)|rssi] [bkgscan (0)|1] [immediateevent (0)|1]"
	"[immediate 0|(1)] [repeat (10)|[1-20]] [exp (2)|[1-5]]"},
	{ "pfnadd", wl_pfn_add, -1, -1,
	"Adding SSID based preferred networks to monitor and connect\n"
	"\tpfnadd syntax is: pfnadd ssid <SSID> [hidden (0)|1]"
	"[imode (bss)|ibss]"
	"[amode (open)|shared] [wpa_auth (wpadisabled)|wpapsk|wpa2psk|wpanone|any]"
	"[wsec WEP|TKIP|AES|TKIPAES] [suppress (neither)|found|lost] [rssi <rssi>(0 dBm)]\n"
	"Up to 16 SSID networks can be added together in one pfnadd\n"
	"\tTo specify more than one WPA methods, use a number (same format as wpa_auth iovar) "
	"as the parameter of wpa_auth (e.g. 0x84 for wpapsk and wpa2psk.)"},
	{ "pfnadd_bssid", wl_pfn_add_bssid, -1, -1,
	"Adding BSSID based preferred networks to monitor and connect\n"
	"\tpfnadd_bssid syntax is: pfnadd_bssid bssid <BSSID> [suppress (neither)|found|lost]"
	"[rssi <rssi>(0 dBm)]\n"
	"\tUp to 150 BSSIDs can be added together in one pfnadd_bssid\n"},
	{ "pfncfg", wl_pfn_cfg, -1, -1,
	"Configures channel list and report type\n"
	"\tpfncfg syntax is: pfncfg [channel <channel list>] [report (both)|ssidonly|bssidonly]"},
	{ "pfn", wl_pfn, -1, -1,
	"Enable/disable preferred network off load monitoring\n"
	"\tpfn syntax is: pfn 0|1"},
	{ "pfnclear", wl_var_void, -1, WLC_SET_VAR,
	"Clear the preferred network list\n"
	"\tpfnclear syntax is: pfnclear"},
	{ "pfnbest", wl_pfnbest, -1, -1,
	"Get the best n networks in each of up to m scans\n"
	"\tpfnbest syntax is: pfnbest"},
	{ "pfnsuspend", wl_pfn_suspend, -1, -1,
	"Suspend/resume pno scan\n"
	"\tpfnsuspend syntax is: pfnsuspend 0|1"},
#if defined(linux)
	{ "pfneventchk", wl_pfn_event_check, -1, -1,
	"Listen and prints the preferred network off load event from dongle\n"
	"\tpfneventchk syntax is: pfneventchk [(eth1)ifname]"},
	{ "escan_event_check", wl_escan_event_check, -1, -1,
	"Listen and prints the escan events from the dongle\n"
	"\tescan_event_check syntax is: escan_event_check ifname flag\n"
	"\tflag 1 = sync_id info, 2 = bss info, 4 = state + bss info [default], "
	"8 = TLV check for IEs"},
	{ "escanresults", wl_escanresults, -1, WLC_SET_VAR,
	"Start escan and display results.\n" SCAN_USAGE
	},
#endif   /* linux */
	{ "event_filter", wl_event_filter, -1, -1,
	"Set/get event filter\n"
	"\tevent_filter syntax is: event_filter [value]"},
#endif /* WLPFN */
	{"rate_histo", wl_rate_histo, -1, WLC_GET_VAR,
	"Get rate hostrogram"
	},
#endif /* !ATE_BUILD */
	{ "pkteng_start", wl_pkteng, -1, WLC_SET_VAR,
	"start packet engine tx usage: wl pkteng_start <xx:xx:xx:xx:xx:xx>"
	" <tx|txwithack> [(async)|sync] [ipg] [len] [nframes] [src]\n"
	"\tstart packet engine rx usage: wl pkteng_start <xx:xx:xx:xx:xx:xx>"
	" <rx|rxwithack> [(async)|sync] [rxframes] [rxtimeout]\n"
	"\tsync: synchronous mode\n"
	"\tipg: inter packet gap in us\n"
	"\tlen: packet length\n"
	"\tnframes: number of frames; 0 indicates continuous tx test\n"
	"\tsrc: source mac address\n"
	"\trxframes: number of receive frames (sync mode only)\n"
	"\trxtimeout: maximum timout in msec (sync mode only)"},
	{ "pkteng_stop", wl_pkteng, -1, WLC_SET_VAR,
	"stop packet engine; usage: wl pkteng_stop <tx|rx>"},
	{ "pkteng_stats", wl_pkteng_stats, -1, WLC_GET_VAR,
	"packet engine stats; usage: wl pkteng_stats"},
#ifndef ATE_BUILD
	{ "wowl", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/disable WOWL events\n"
	"  0   - Clear all events\n"
	"Bit 0 - Wakeup on Magic Packet\n"
	"Bit 1 - Wakeup on NetPattern (use 'wl wowl_pattern' to configure pattern)\n"
	"Bit 2 - Wakeup on loss-of-link due to Disassociation/Deauth\n"
	"Bit 3 - Wakeup on retrograde tsf\n"
	"Bit 4 - Wakeup on loss of beacon (use 'wl wowl_bcn_loss' to configure time)"},
	{ "wowl_bcn_loss", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Set #of seconds of beacon loss for wakeup event"},
	{ "wowl_pattern", wl_wowl_pattern, -1, -1,
	"usage: wowl_pattern [ [clr | [[ add | del ] offset mask value ]]]\n"
	"No options -- lists existing pattern list\n"
	"add -- Adds the pattern to the list\n"
	"del -- Removes a pattern from the list\n"
	"clr -- Clear current list\n"
	"offset -- Starting offset for the pattern\n"
	"mask -- Mask to be used for pattern. Bit i of mask => byte i of the pattern\n"
	"value -- Value of the pattern"
	},
	/* wowl_keepalive shares structures and functions with "mkeep_alive" */
	{"wowl_keepalive", wl_mkeep_alive, WLC_GET_VAR, WLC_SET_VAR,
	"Send specified keep alive packet periodically in wowl mode.\n"
	"\tUsage: wl wowl_keepalive <index0-1> <period> <packet>\n"
	"\t\tindex: 0 - 1.\n"
	"\t\tperiod: Re-transmission period in milli-seconds. 0 to disable packet transmits.\n"
	"\t\tpacket: Hex packet contents to transmit. The packet contents should include "
	"the entire ethernet packet (ethernet header, IP header, UDP header, and UDP "
	"payload) specified in network byte order.\n"
	"\n\te.g. Send keep alive packet every 30 seconds using id-1:\n"
	"\twl wowl_keepalive 1 30000 0x0014a54b164f000f66f45b7e08004500001e000040004011c"
	"52a0a8830700a88302513c413c4000a00000a0d\n" },

	{ "wowl_wakeind", wl_wowl_wakeind, WLC_GET_VAR, WLC_SET_VAR,
	"usage: wowl_wakeind [clear]\n"
	"Shows last system wakeup event indications from PCI and D11 cores\n"
	"clear - Clear the indications"
	},
#if defined(WLTEST)
	{ "sslpnphy_papd_debug", wl_sslpnphy_papd_debug, -1, WLC_GET_VAR,
	"shows sslpnphy papd debug data"},
	{ "sslpnphy_papd_dumplut", wl_sslpnphy_debug, -1, WLC_GET_VAR,
	"shows sslpnphy papd compdelta table  data"},
	{ "sslpnphy_spbdump", wl_sslpnphy_spbdump, -1, WLC_GET_VAR,
	"shows sslpnphy spb dump"},
	{ "sslpnphy_percal_debug", wl_sslpnphy_percal_debug, -1, WLC_GET_VAR,
	"shows sslpnphy percal debug data"},
	{ "sslpnphy_spbrange", wl_phy_spbrange, WLC_GET_VAR, WLC_SET_VAR,
	"usage: sslpnphy_spbrange [start end}"},
	{ "sslpnphy_cga_5g", wl_phy_sslpnphy_cga_5g, WLC_GET_VAR, WLC_SET_VAR,
	"usage: sslpnphy_cga_5g cga1 cga2... cga24 }"},
	{ "sslpnphy_cga_2g", wl_phy_sslpnphy_cga_2g, WLC_GET_VAR, WLC_SET_VAR,
	"usage: sslpnphy_cga_2g cga1 cga2... cga14 }"},
	{ "sslpnphy_tx_iqcc", wl_sslpnphy_tx_iqcc, WLC_GET_VAR, WLC_SET_VAR,
	"usage: sslpnphy_tx_iqcc }"},
	{ "sslpnphy_tx_locc", wl_sslpnphy_tx_locc, WLC_GET_VAR, WLC_SET_VAR,
	"usage: slpnphy_tx_locc}"},
#endif 
	{ "wowl_status", wl_wowl_status, WLC_GET_VAR, -1,
	"usage: wowl_status [clear]\n"
	"Shows last system wakeup setting"
	},
	{"wowl_pkt", wl_wowl_pkt, -1, -1,
	"Send a wakeup frame to wakup a sleeping STA in WAKE mode\n"
	"Usage: wl wowl_pkt <len> <dst ea | bcast | ucast <STA ea>>"
	"[ magic [<STA ea>] | net <offset> <pattern> <reason code> ]\n"
	"e.g. To send bcast magic frame -- "
	"wl wowl_pkt 102 bcast magic 00:90:4c:AA:BB:CC\n"
	"     To send ucast magic frame -- "
	"wl wowl_pkt 102 ucast 00:90:4c:aa:bb:cc magic\n"
	"     To send a frame with L2 unicast - "
	"wl wowl_pkt 102 00:90:4c:aa:bb:cc net 0 0x00904caabbcc 0x03\n"
	" NOTE: offset for netpattern frame starts from \"Dest EA\" of ethernet frame."
	"So dest ea will be used only when offset is >= 6\n"
	"     To send a eapol identity frame with L2 unicast - "
	"wl wowl_pkt 102 00:90:4c:aa:bb:cc eapid id-string"},
	{"wowl_ext_magic", wl_wowl_extended_magic, WLC_GET_VAR, WLC_SET_VAR,
	"Set 6-byte extended magic pattern\n"
	"Usage: wl wowl_ext_magic 0x112233445566"},
	{ "wme_apsd_trigger", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Set Periodic APSD Trigger Frame Timer timeout in ms (0=off)"},
	{ "wme_autotrigger", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/Disable sending of APSD Trigger frame when all ac are delivery enabled"},
	{ "reassoc", wl_reassoc, -1, WLC_REASSOC,
	"Initiate a (re)association request.\n"
	"\tUsage: wl reassoc <bssid> [options]\n"
	"\tOptions:\n"
	"\t-c CL, --chanspecs=CL \tchanspecs (comma or space separated list)"},
	{ "send_nulldata", wl_iov_mac, -1, -1,
	"Sed a null frame to the specified hw address" },
	{ "btc_params", wlu_reg2args, WLC_GET_VAR, WLC_SET_VAR, "g/set BT Coex parameters"},
	{ "btc_flags", wlu_reg2args, WLC_GET_VAR, WLC_SET_VAR, "g/set BT Coex flags"},
	{ "obss_scan_params", wl_obss_scan, WLC_GET_VAR, WLC_SET_VAR,
	"set/get Overlapping BSS scan parameters\n"
	"Usage: wl obss_scan a b c d e ...; where\n"
	"\ta-Passive Dwell, {5-1000TU}, default = 100\n"
	"\tb-Active Dwell, {10-1000TU}, default = 20\n"
	"\tc-Width Trigger Scan Interval, {10-900sec}, default = 300\n"
	"\td-Passive Total per Channel, {200-10000TU}, default = 200\n"
	"\te-Active Total per Channel, {20-1000TU}, default = 20\n"
	"\tf-Channel Transition Delay Factor, {5-100}, default = 5\n"
	"\tg-Activity Threshold, {0-100%}, default = 25"},
	{"mkeep_alive", wl_mkeep_alive, WLC_GET_VAR, WLC_SET_VAR,
	"Send specified \"mkeep-alive\" packet periodically.\n"
	"\tUsage: wl mkeep_alive <index0-3> <period> <packet>\n"
	"\t\tindex: 0 - 3.\n"
	"\t\tperiod: Re-transmission period in milli-seconds. 0 to disable packet transmits.\n"
	"\t\tpacket: Hex packet contents to transmit. The packet contents should include "
	"the entire ethernet packet (ethernet header, IP header, UDP header, and UDP "
	"payload) specified in network byte order. If no packet is specified, a nulldata frame "
	"will be sent instead.\n"
	"\n\te.g. Send keep alive packet every 30 seconds using id-1:\n"
	"\twl mkeep_alive 1 30000 0x0014a54b164f000f66f45b7e08004500001e000040004011c"
	"52a0a8830700a88302513c413c4000a00000a0d" },
	{"keep_alive", wl_keep_alive, -1, -1,
	"Send specified \"keep-alive\" packet periodically.\n"
	"\tUsage: wl keep_alive <period> <packet>\n"
	"\t\tperiod: Re-transmission period in milli-seconds. 0 to disable packet transmits.\n"
	"\t\tpacket: Hex packet contents to transmit. The packet contents should include "
	"the entire ethernet packet (ethernet header, IP header, UDP header, and UDP "
	"payload) specified in network byte order.\n"
	"\n\te.g. Send keep alive packet every 30 seconds:\n"
	"\twl keep_alive 30000 0x0014a54b164f000f66f45b7e08004500001e000040004011c"
	"52a0a8830700a88302513c413c4000a00000a0d" },
	{ "srchmem", wl_srchmem, WLC_GET_VAR, WLC_SET_VAR,
	"g/set ucode srch engine memory"},
	{ "pkt_filter_add", wl_pkt_filter_add, -1, -1,
	"Install a packet filter.\n"
	"\tUsage: wl pkt_filter_add <id> <polarity> <type> <offset> <bitmask> <pattern>\n"
	"\tid:       Integer. User specified id.\n"
	"\ttype:     0 (Pattern matching filter).\n"
	"\toffset:   Integer. Offset within received packets to start matching.\n"
	"\tpolarity: Set to 1 to negate match result. 0 is default.\n"
	"\tbitmask:  Hex bitmask that indicates which bits of 'pattern' to match. Must be same\n"
	"\t\tsize as 'pattern'. Bit 0 of bitmask corresponds to bit 0 of pattern, etc.\n"
	"\t\tIf bit N of bitmask is 0, then do *not* match bit N of the pattern with\n"
	"\t\tthe received payload. If bit N of bitmask is 1, then perform match.\n"
	"\tpattern:  Hex pattern to match." },
	{ "pkt_filter_clear_stats", wl_varint, -1, WLC_SET_VAR,
	"Clear packet filter statistic counter values.\n"
	"\tUsage: wl pkt_filter_clear_stats <id>" },
	{ "pkt_filter_enable", wl_pkt_filter_enable, -1, -1,
	"Enable/disable a packet filter.\n"
	"\tUsage: wl pkt_filter_enable <id> <0|1>"},
	{ "pkt_filter_list", wl_pkt_filter_list, -1, -1,
	"List installed packet filters.\n"
	"\tUsage: wl pkt_filter_list [val]\n"
	"\tval: 0 (disabled filters) 1 (enabled filters)"},
	{ "pkt_filter_mode", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Set packet filter match action.\n"
	"\tUsage: wl pkt_filter_mode <value>\n"
	"\tvalue: 1 - Forward packet on match, discard on non-match (default).\n"
	"\t       0 - Discard packet on match, forward on non-match." },
	{ "pkt_filter_delete", wl_varint, -1, WLC_SET_VAR,
	"Uninstall a packet filter.\n"
	"\tUsage: wl pkt_filter_delete <id>"},
	{ "pkt_filter_stats", wl_pkt_filter_stats, -1, -1,
	"Retrieve packet filter statistic counter values.\n"
	"\tUsage: wl pkt_filter_stats <id>"},

#ifdef TRAFFIC_MGMT
	{"trf_mgmt_config", wl_trf_mgmt_config, WLC_GET_VAR, WLC_SET_VAR,
	"Sets/gets traffic management configuration.\n"
	"\tUsage: wl trf_mgmt_config [<enable> \n"
	"\t                          [<host IP address> <host IP subnet mask> \n"
	"\t                           <downlink kbps> <uplink kbps> [<flags>]]] \n"
	"\tenable: 0 - Disable traffic management\n"
	"\t        1 - Enables traffic management (host IP arguments required)\n"
	"\tFlag values are the following:\n"
	"\t0x0001 : Add DSCP values to tx packets\n"
	"\t0x0002 : Disable traffic shaping...just do priority classification\n"
	"\nIf no arguments are entered, the current traffic management configuration \n"
	"is displayed.\n"
	"\ne.g. Configure traffic management and specify local ip addr. and bandwidth data:\n"
	"\nwl trf_mgmt_config 1 12.0.0.1 255.0.0.0 5000 650\n"},

	{"trf_mgmt_filters_add", wl_trf_mgmt_filters_add, -1, WLC_SET_VAR,
	"Adds a traffic management filter.\n"
	"\tUsage: wl trf_mgmt_filter_add [dst_port src_port prot priority]\n"
	"\tdst_port    : Destination TCP or UDP port \n"
	"\tsrc_port    : Source TCP or UDP port (0 - wildcard for any source port)\n"
	"\tprot        : L4 protocol (6 - TCP, 17 - UDP)\n"
	"\tpriority    : Priority value (see trf_mgmt_priority_values enum) \n"
	"\ne.g. Add a tcp wildcard filter:\n"
	"\nwl trf_mgmt_filters_add 80 0 6 2\n"},

	{"trf_mgmt_filters_addex", wl_trf_mgmt_filters_addex, -1, WLC_SET_VAR,
	"Adds a traffic management filter.\n"
	"\tUsage: wl trf_mgmt_filter_add flag [dst_port src_port prot priority]\n"
	"\tUsage: wl trf_mgmt_filter_add flag [dst_mac priority] \n"
	"\tFlag values are the following:\n"
	"\t0x0000 : filter on tcp/udp src/dst port\n"
	"\t0x0001 : filter on destination MAC address\n"
	"\t0x0010 : do not update the packet priority \n"
	"\t0x0020 : Tag packets as Favored\n"
	"\tdst_mac    : Destination MAC address \n"
	"\tdst_port    : Destination TCP or UDP port \n"
	"\tsrc_port    : Source TCP or UDP port (0 - wildcard for any source port)\n"
	"\tprot        : L4 protocol (6 - TCP, 17 - UDP)\n"
	"\tpriority    : Priority value (see trf_mgmt_priority_values enum) \n"
	"\ne.g. Add a tcp wildcard filter for all src/dst ports:\n"
	"\nwl trf_mgmt_filters_addex 0 0 0 6 2\n"
	"\ne.g. Add a dst mac address filter\n"
	"\nwl trf_mgmt_filters_addex 0x31 aa:bb:cc:dd:ee:ff 2\n"},

	{"trf_mgmt_filters_remove", wl_trf_mgmt_filters_remove, -1, WLC_SET_VAR,
	"Removes a traffic management filter.\n"
	"\tUsage: wl trf_mgmt_filter_remove [dst_port src_port prot]\n"
	"\tdst_port    : Destination TCP or UDP port \n"
	"\tsrc_port    : Source TCP or UDP port (0 - wildcard for any source port)\n"
	"\tprot        : L4 protocol (6 - TCP, 17 - UDP)\n"
	"\ne.g. Remove a tcp wildcard filter:\n"
	"\nwl trf_mgmt_filters_remove 80 0 6\n"},

	{"trf_mgmt_filters_removeex", wl_trf_mgmt_filters_removeex, -1, WLC_SET_VAR,
	"Removes a traffic management filter.\n"
	"\tUsage: wl trf_mgmt_filter_remove flag [dst_port src_port prot]\n"
	"\tUsage: wl trf_mgmt_filter_remove flag [dst_mac]\n"
	"\tFlag values are the following:\n"
	"\t0x0000 : filter on tcp/udp src/dst port\n"
	"\t0x0001 : filter on destination MAC address\n"
	"\t0x0010 : do not update the packet priority \n"
	"\t0x0020 : Tag packets as Favored\n"
	"\tdst_mac    : Destination MAC address \n"
	"\tdst_port    : Destination TCP or UDP port \n"
	"\tsrc_port    : Source TCP or UDP port (0 - wildcard for any source port)\n"
	"\tprot        : L4 protocol (6 - TCP, 17 - UDP)\n"
	"\ne.g. Remove a tcp wildcard filter:\n"
	"\nwl trf_mgmt_filters_removeex 0 80 0 6\n"
	"\nwl trf_mgmt_filters_removeex 0x31 00:90:4c:52:a8:83\n"},

	{"trf_mgmt_filters_list", wl_trf_mgmt_filters_list, WLC_GET_VAR, -1,
	"Lists all traffic management filters.\n"
	"\tUsage: wl trf_mgmt_filter_list\n"},

	{"trf_mgmt_filters_clear", wl_trf_mgmt_filters_clear, -1, WLC_SET_VAR,
	"Clears all traffic management filters.\n"
	"\tUsage: wl trf_mgmt_filters_clear\n"},

	{"trf_mgmt_bandwidth", wl_trf_mgmt_bandwidth, WLC_GET_VAR, WLC_SET_VAR,
	"Sets/gets traffic management bandwidth configuration.\n"
	"\tUsage: wl trf_mgmt_bandwidth \n"
	"\t          [downlink uplink min_tx_bk min_tx_be min_tx_vi\n"
	"\t                          [min_rx_b min_rx_be min_rx_vi]]\n"
	"\tdownlink   : downlink bandwidth (kbps)\n"
	"\tuplink     : uplink bandwidth (kbps)\n"
	"\tmin_tx_bk  : min. guaranteed tx bandwidth percentage for BK (kbps)\n"
	"\tmin_tx_be  : min. guaranteed tx bandwidth percentage for BE (kbps)\n"
	"\tmin_tx_vi  : min. guaranteed tx bandwidth percentage for VI (kbps)\n"
	"\n(min_tx_bo + min_tx_be + min_tx_vi) must equal 100.\n"
	"\tmin_rx_bk  : min. guaranteed rx bandwidth percentage for BK (kbps)\n"
	"\tmin_rx_be  : min. guaranteed rx bandwidth percentage for BE (kbps)\n"
	"\tmin_rx_vi  : min. guaranteed rx bandwidth percentage for VI (kbps)\n"
	"\n(min_rx_bk + min_rx_be + min_rx_vi) must equal 100."
	"\nIf no rx gandwidth arguments are entered, tx bandwidth is used for rx."
	"\nIf no arguments are entered, the current bandwidth configuration is displayed."},

	{"trf_mgmt_flags", wl_trf_mgmt_flags, WLC_GET_VAR, WLC_SET_VAR,
	"Sets/gets traffic management operational flags.\n"
	"\tUsage: wl trf_mgmt_flags [flags]\n\n"
	"\tFlag values are the following:\n"
	"\t0x0001 : Add DSCP values to tx packets\n"
	"\t0x0002 : Disable traffic shaping...just do priority classification\n"
	"\nIf no arguments are entered, the current operational flags are displayed."},

	{"trf_mgmt_stats", wl_trf_mgmt_stats, WLC_GET_VAR, -1,
	"Gets traffic management statistics.\n"
	"\tUsage: wl trf_mgmt_stats [index]\n"
	"\tindex : Queue index \n"},

	{"trf_mgmt_stats_clear", wl_trf_mgmt_stats_clear, -1, WLC_SET_VAR,
	"Clears traffic management statistics.\n"
	"\tUsage: wl trf_mgmt_stats_clear\n"},

	{"trf_mgmt_shaping_info", wl_trf_mgmt_shaping_info, WLC_GET_VAR, -1,
	"Gets traffic management shaping parameters.\n"
	"\tUsage: wl trf_mgmt_shaping_info [index]\n"
	"\tindex : Queue index \n"},
#endif  /* TRAFFIC_MGMT */

	{ "seq_start", wl_seq_start, -1, WLC_SET_VAR,
	"Initiates command batching sequence. Subsequent IOCTLs will be queued until\n"
	"seq_stop is received."},
	{ "seq_stop", wl_seq_stop, -1, WLC_SET_VAR,
	"Defines the end of command batching sequence. Queued IOCTLs will be executed."},
	{ "seq_delay", wl_varint, -1, WLC_SET_VAR,
	"Driver should spin for the indicated amount of time.\n"
	"It is only valid within the context of batched commands."},
	{ "seq_error_index", wl_varint, WLC_GET_VAR, -1,
	"Used to retrieve the index (starting at 1) of the command that failed within a batch"},
#ifdef CLMDOWNLOAD
	{ "clmload", wl_clmload, -1, WLC_SET_VAR,
	"Used to download CLM data onto the dongle"},
#endif /* CLMDOWNLOAD */
	{ "bmac_reboot", wl_var_void, -1, WLC_SET_VAR,
	"Reboot BMAC"},
#ifdef RWL_WIFI
	{ "findserver", wl_wifiserver, -1, -1,
	"Used to find the remote server with proper mac address given by the user,this "
	"cmd is specific to wifi protocol."},
#endif
	{ "txmcsset", wl_txmcsset, WLC_GET_VAR, -1, "get Transmit MCS rateset for 11N device"},
	{ "rxmcsset", wl_rxmcsset, WLC_GET_VAR, -1, "get Receive MCS rateset for 11N device"},
	{ "mimo_ss_stf", wl_mimo_stf, WLC_GET_VAR, WLC_SET_VAR,
	"get/set SS STF mode.\n"
	"\tUsage: wl mimo_ss_stf <value> <-b a | b>\n"
	"\tvalue: 0 - SISO; 1 - CDD\n"
	"\t-b(band): a - 5G; b - 2.4G"},
#ifdef DSLCPE
	{ "assoclistinfo", wl_assoclist_info, -1, -1,
	"AP only: Get the list of yet another form of associated station info" },
#ifdef DSLCPE_SCBLIST
	{ "scblist", wl_readscb, -1, -1,
	"AP only: Get STA list" 
	},
#endif
#endif  /* DSLCPE */
#ifdef WLEXTLOG
	{ "extlog", wl_extlog, WLC_GET_VAR, -1,
	"get external logs\n"
	"\tUsage: wl extlog <from_last> <number>\n"
	"\tfrom_last: 1 - from the last log record; 0 - whole log recrods\n"
	"\tnumber: number of log records to get, MAX is 32."},
	{ "extlog_clr", wl_var_void, -1, WLC_SET_VAR, "clear external log records"},
	{ "extlog_cfg", wl_extlog_cfg, WLC_GET_VAR, WLC_SET_VAR,
	"get/set external log configuration"},
#endif
	{ "assertlog", wl_assertlog, WLC_GET_VAR, -1,
	"get external assert logs\n"
	"\tUsage: wl assertlog"},
	{ "assert_type", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"set/get the asset_bypass flag; usage: wl assert_type <1/0> (On/Off)"
	},
	{ "ledbh", wl_ledbh, WLC_GET_VAR, WLC_SET_VAR,
	"set/get led behavior\n"
	"\tUsage: wl ledbh [0-3] [0-15]"},
	{ "obss_coex_action", wl_obss_coex_action, -1, WLC_SET_VAR,
	"send OBSS 20/40 Coexistence Mangement Action Frame\n"
	"\tUsage: wl obss_coex_action -i <1/0> -w <1/0> -c <channel list>\n"
	"\t -i: 40MHz intolerate bit; -w: 20MHz width Req bit;\n"
	"\t -c: channel list, 1 - 14\n"
	"\t At least one option must be provided"
	},
	{"chanim_state", wl_chanim_state, WLC_GET_VAR, -1,
	"get channel interference state\n"
	"\tUsage: wl chanim_state channel\n"
	"\tValid channels: 1 - 14\n"
	"\treturns: 0 - Acceptable; 1 - Severe"
	},
	{"chanim_mode", wl_chanim_mode, WLC_GET_VAR, WLC_SET_VAR,
	"get/set channel interference measure (chanim) mode\n"
	"\tUsage: wl chanim_mode <value>\n"
	"\tvalue: 0 - disabled; 1 - detection only; 2 - detection and avoidance"
	},
	{ "ledbh", wl_ledbh, WLC_GET_VAR, WLC_SET_VAR, "set/get led behavior\n"
	"\tUsage: wl ledbh [0-3] [0-15]"},
	{ "led_blink_sync", wl_led_blink_sync, WLC_GET_VAR, WLC_SET_VAR, "set/get led_blink_sync\n"
	"\tUsage: wl led_blink_sync [0-3] [0/1]"},
#ifdef BCMWAPI_WAI
	{ "wai_restrict", wl_bsscfg_int, WLC_GET_VAR, WLC_SET_VAR,
	"set/get WAI restriction"},
	{ "wai_rekey", wl_iov_mac, -1, -1,
	"set rekey event"},
#endif /* BCMWAPI_WAI */

	{"cca_get_stats", wl_cca_get_stats, WLC_GET_VAR, -1,
	"Usage: wl cca_stats [-c channel] [-s num seconds][-n]\n"
	"\t -c channel: Optional. specify channel. 0 = All channels. Default = current channel \n"
	"\t -n: no analysis of results\n"
	"\t -s num_seconds: Optional. Default = 10, Max = 60\n"
	"\t -i: list individual measurements in addition to the averages\n"
	"\t -curband: Only recommend channels on current band"
	},
	{ "itfr_get_stats", wl_itfr_get_stats, WLC_GET_VAR, -1,
	"get interference source information"
	},
	{ "itfr_enab", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"get/set STA interference detection mode(STA only)\n"
	"\t 0  - disable\n"
	"\t 1  - enable maual detection\n"
	"\t 2  - enable auto detection"
	},
	{ "itfr_detect", wl_var_void, -1, WLC_SET_VAR,
	"issue an interference detection request"
	},
	{ "smfstats", wl_smfstats, WLC_GET_VAR, WLC_SET_VAR,
	"get/clear selected management frame (smf) stats"
	"\twl smfstats [-C num]|[--cfg=num] [auth]|[assoc]|[reassoc]|[clear]\n"
	"\tclear - to clear the stats" },
#ifdef RWL_DONGLE
	{ "dongleset", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Enable uart driver"
	},
#endif
	{ "manfinfo", wl_var_getandprintstr, WLC_GET_VAR, -1,
	"show chip package info in OTP"},
	{ "rrm_bcn_req", wl_rrm_bcn_req, -1, WLC_SET_VAR,
	"send 11k beacon measurement request\n"
	"\tUsage: wl rrm_bcn_req [bcn mode] [da] [duration] [random int] [channel] [ssid]"
	" [repetitions]"},
	{ "rrm_chload_req", wl_rrm_chload_req, -1, WLC_SET_VAR,
	"send 11k channel load measurement request\n"
	"\tUsage: wl rrm_chload_req [regulatory] [da] [duration] [random int] [channel]"
	" [repetitions]"},
	{ "rrm_noise_req", wl_rrm_noise_req, -1, WLC_SET_VAR,
	"send 11k noise measurement request\n"
	"\tUsage: wl rrm_noise_req [regulatory] [da] [duration] [random int] [channel]"
	" [repetitions] "},
	{ "rrm_frame_req", wl_rrm_frame_req, -1, WLC_SET_VAR,
	"send 11k frame measurement request\n"
	"\tUsage: wl rrm_frame_req [regulatory] [da] [duration] [random int] [channel] [ta]"
	" [repetitions]"},
	{ "rrm_stat_req", wl_rrm_stat_req, -1, WLC_SET_VAR,
	"send 11k stat measurement request\n"
	"\tUsage: wl rrm_stat_req [da] [random int] [duration] [peer] [group id] [repetitions]"},
	{ "rrm_lm_req", wl_rrm_lm_req, -1, WLC_SET_VAR,
	"send 11k link measurement request\n"
	"\tUsage: wl rrm_lm_req [da]"},
	{ "rrm_nbr_req", wl_rrm_nbr_req, -1, WLC_SET_VAR,
	"send 11k neighbor report measurement request\n"
	"\tUsage: wl rrm_nbr_req [ssid]"},
	{ "wnm", wl_wnm, WLC_GET_VAR, WLC_SET_VAR,
	"set driver wnm feature mask\n"
	"\ttype \'wl msglevel ?\' for values" },
	{ "wnm_bsstq", wl_wnm_bsstq, -1, WLC_SET_VAR,
	"send 11v BSS transition management query\n"
	"\tUsage: wl wnm_bsstq [ssid]"},
#ifdef WLWNM
	{ "wnm_tclas_add", wl_wnm_tclas_add, -1, WLC_SET_VAR,
	"add tclas frame classifier type entry\n"
	"\tUsage: wl wnm_tclas_add [dms | tfs | fms][user priority][frame classifier type]\n"
	"\tFC type 0 format: <type> <mask> <src mac> <dst mac> <ether type>\n"
	"\tFC type 1 ipv4 format: <type> <mask> <ver> <sip> <dip> <sp> <dp> <dscp> <prot>\n"
	"\tFC type 2 format: <type> <mask> <vlan tag>\n"
	"\tFC type 3 format: <type> <mask> <offset> <value> <mask>\n"
	"\tFC type 4 ipv4 format: <type> <mask> <ver> <sip> <dip> <sp> <dp> <dscp> <prot>\n"
	"\tFC type 5 format: <type> <mask> <802.1Q PCP> <802.1Q CFI> <802.1Q VID>\n"
	},
	{ "wnm_tclas_del", wl_wnm_tclas_del, -1, WLC_SET_VAR,
	"delete tclas frame classifier type entry\n"
	"\tUsage: wl wnm_tclas_del [dms | tfs | fms][start_idx | start_idx end_idx]\n"
	},
	{ "wnm_tclas_list", wl_wnm_tclas_list, WLC_GET_VAR, -1,
	"list the added tclas frame classifier type entry\n"
	"\tUsage: wl wnm_tclas_list [dms | tfs | fms]\n"
	},
	{ "wnm_tfsreq_add", wl_wnm_tfsreq_add, -1, WLC_SET_VAR,
	"add one tfs request element and send tfs request frame\n"
	"\tUsage: wl wnm_tfsreq_add <tfs_id> <tfs_action_code> <tfs_subelem_id> <send>\n"
	"\ttfs_id: a non-zero value (1 ~ 255)\n"
	"\ttfs_action_code bitfield: 1: delete after match, 2: notify\n"
	"\ttfs_subelem_id: TFS subelement (0 for none or 1 for previous wnm_tclas_add)\n"
	"\tsend: 0: store element, 1: send all stored elements\n"
	},
	{ "wnm_dmsdesc_add", wl_wnm_dmsdesc_add, -1, WLC_SET_VAR,
	"add one dms descriptor and send dms request frame\n"
	"\tUsage: wl wnm_dmsdesc_add <send>\n"
	"\tsend: 0: store element, 1: send all stored elements\n"
	},
	{ "wnm_dmsdesc_list", wl_wnm_dmsdesc_list, WLC_GET_VAR, -1,
	"list the request dms descriptors\n"
	"\tUsage: wl wnm_dmsdesc_list\n"
	},
#endif /* WLWNM */
#ifdef WLP2P
	{ "p2p_ssid", wl_ssid, -1, WLC_SET_VAR,
	"set WiFi P2P wildcard ssid.\n"
	"\tUsage: wl p2p_ssid <ssid>"
	},
	{ "p2p_state", wl_p2p_state, -1, WLC_SET_VAR,
	"set WiFi P2P discovery state.\n"
	"\tUsage: wl p2p_state <state> [<chanspec> <dwell time>]"
	},
	{ "p2p_scan", wl_p2p_scan, -1, WLC_SET_VAR,
	"initiate WiFi P2P scan.\n"
	"\tUsage: wl p2p_scan S|E <scan parms>\n"
	SCAN_USAGE
	},
	{ "p2p_ifadd", wl_p2p_ifadd, -1, WLC_SET_VAR,
	"add WiFi P2P interface\n"
	"\tUsage: wl p2p_ifadd <MAC-address> go|client|dyngo [chanspec]\n"
	"MAC-address: xx:xx:xx:xx:xx:xx"
	},
	{ "p2p_ifdel", wl_p2p_ifdel, -1, WLC_SET_VAR,
	"delete WiFi P2P interface\n"
	"\tUsage: wl p2p_ifdel <MAC-address>\n"
	"MAC-address: xx:xx:xx:xx:xx:xx"
	},
	{ "p2p_ifupd", wl_p2p_ifupd, -1, WLC_SET_VAR,
	"update an interface to WiFi P2P interface\n"
	"\tUsage: wl p2p_ifupd <MAC-address> go|client\n"
	"MAC-address: xx:xx:xx:xx:xx:xx"
	},
	{ "p2p_if", wl_p2p_if, WLC_GET_VAR, -1,
	"query WiFi P2P interface bsscfg index\n"
	"\tUsage: wl p2p_if <MAC-address>\n"
	"MAC-address: xx:xx:xx:xx:xx:xx"
	},
	{ "p2p_noa", wl_p2p_noa, WLC_GET_VAR, WLC_SET_VAR,
	"set/get WiFi P2P NoA schedule\n"
	"\tUsage: wl p2p_noa <type> <type-specific-params>\n"
	"\t\ttype 0: Scheduled Absence (on GO): <type> <action> <action-specific-params>\n"
	"\t\t\taction -1: Cancel the schedule: <type> <action>\n"
	"\t\t\taction 0,1,2: <type> <action> <option> <option-specific-params>\n"
	"\t\t\t\taction 0: Do nothing during absence periods\n"
	"\t\t\t\taction 1: Sleep during absence periods\n"
	"\t\t\t\toption 0: <start:tsf> <interval> <duration> <count> ...\n"
	"\t\t\t\toption 1 [<start-percentage>] <duration-percentage>\n"
	"\t\t\t\toption 2 <start:tsf-offset> <interval> <duration> <count>\n"
	"\t\ttype 1: Requested Absence (on GO): \n"
	"\t\t\taction -1: Cancel the schedule: <type> <action>\n"
	"\t\t\taction 2: <type> <action> <option> <option-specific-params>\n"
	"\t\t\t\taction 2: Turn off GO beacons and probe responses during absence period\n"
	"\t\t\t\toption 2 <start:tsf-offset> <interval> <duration> <count>"
	},
	{ "p2p_ops", wl_p2p_ops, WLC_GET_VAR, WLC_SET_VAR,
	"set/get WiFi P2P OppPS and CTWindow\n"
	"\tUsage: wl p2p_ops <ops> [<ctw>]\n"
	"\t\t<ops>:\n"
	"\t\t\t0: Disable OppPS\n"
	"\t\t\t1: Enable OppPS\n"
	"\t\t<ctw>:\n"
	"\t\t\t10 and up to beacon interval"
	},
	{ "p2p_da_override", wl_iov_mac, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set WiFi P2P device interface addr\n"
	"\tUsage: wl p2p_da_override <MAC-address>\n"
	"MAC-address: xx:xx:xx:xx:xx:xx\n"
	"(When MAC-address is set to 00:00:00:00:00:00, default da restored)"
	},
#endif /* WLP2P */
	{ "pm_dur", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Retrieve accumulated PM duration information (GET) or clear accumulator (SET)\n"
	"\tUsage: wl pm_dur <any-number-to-clear>"
	},
	{ "mpc_dur", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Retrieve accumulated MPC duration information in ms (GET) or clear accumulator (SET)\n"
	"\tUsage: wl mpc_dur <any-number-to-clear>"},
	{"chanim_acs_record", wl_chanim_acs_record, WLC_GET_VAR, -1,
	"get the auto channel scan record. \n"
	"\t Usage: wl acs_record"
	},
	{"chanim_stats", wl_chanim_stats, WLC_GET_VAR, -1,
	"get chanim stats \n"
	"\t Usage: wl chanim_stats"
	},
	{"txdelay_params", wl_txdelay_params, WLC_GET_VAR, -1,
	"get chanim stats \n"
	"\t Usage: wl txdelay_params ratio cnt period tune"
	},
	{ "dngl_wd", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"enable or disable dongle keep alive watchdog timer\n"
	"\tUsage: wl dngl_wd 0\\1 (to turn off\\on)"},
	{ "tsf", wl_tsf, WLC_GET_VAR, WLC_SET_VAR,
	"set/get tsf register\n"
	"\tUsage: wl tsf [<high> <low>]"},
	{ "tpc_mode", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/disable AP TPC.\n"
	"Usage: wl tpc_mode <mode> \n"
	"\t0 - disable, 1 - BSS power control, 2 - AP power control, 3 - Both (1) and (2)"},
	{ "tpc_period", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Set AP TPC periodicity in secs.\n"
	"Usage: wl tpc_period <secs> "},
	{ "tpc_lm", wl_tpc_lm, WLC_GET_VAR, -1,
	"Get current link margins."},
	{ "mfp_config", wl_mfp_config, -1, WLC_SET_VAR,
	"Config PMF capability\n"
	"\tusage: wl mfp 0/disable, 1/capable, 2/requred" },
	{ "mfp_sha256", wl_mfp_sha256, WLC_GET_VAR, WLC_SET_VAR,
	"Config SHA256 capability\n"
	"\tusage: wl sha256 0/disable, 1/enable" },
	{ "mfp_sa_query", wl_mfp_sa_query, -1, WLC_SET_VAR,
	"Send a sa query req/resp to a peer\n"
	"\tusage: wl mfp_sa_query flag action id" },
	{ "mfp_disassoc", wl_mfp_disassoc, WLC_GET_VAR, WLC_SET_VAR,
	"send bogus disassoc\n"
	"\tUsage: wl mfp_disassoc"},
	{ "mfp_deauth", wl_mfp_deauth, WLC_GET_VAR, WLC_SET_VAR,
	"send bogus deauth\n"
	"\tUsage: wl mfp_dedauth"},
	{ "mfp_assoc", wl_mfp_assoc, WLC_GET_VAR, WLC_SET_VAR,
	"send assoc\n"
	"\tUsage: wl mfp_assoc"},
	{ "mfp_auth", wl_mfp_auth, WLC_GET_VAR, WLC_SET_VAR,
	"send auth\n"
	"\tUsage: wl mfp_auth"},
	{ "mfp_reassoc", wl_mfp_reassoc, WLC_GET_VAR, WLC_SET_VAR,
	"send reassoc\n"
	"\tUsage: wl mfp_reassoc"},
	{ "monitor_lq", wl_monitor_lq, WLC_GET_VAR, WLC_SET_VAR,
	"Start/Stop monitoring link quality metrics - RSSI and SNR\n"
	"\tUsage: wl monitor_lq <0: turn off / 1: turn on"},
	{ "monitor_lq_status", wl_dump_lq, WLC_GET_VAR, -1 /* Set not reqd */,
	"Returns averaged link quality metrics - RSSI and SNR values"},
	{ "mac_rate_histo", wl_mac_rate_histo, WLC_GET_VAR, WLC_SET_VAR,
	"Usage: wl mac_rate_histo <mac address> <access category> <num_pkts>\n"
	"\t(MAC address e.g. 00:11:20:11:33:33)\n"
	"\t(Access Category(AC) - 0x10:for entire MAC or 0x4:for video AC for this MAC)\n"
	"\t(num_pkts (optional) - Number of packets to average - max 64 for AC 0x10,"
	" max 32 for AC 0x4)"
	},
#ifdef SERDOWNLOAD
	{ "init", dhd_init, WLC_GET_VAR, WLC_SET_VAR,
	"init <chip_id>\n"
	"\tInitialize the chip.\n"
	"\tCurrently only 4325, 4329, 43291, 4330a1 and 4330 (b1) are supported"
	},
	{ "download", dhd_download, WLC_GET_VAR, WLC_SET_VAR,
	"download  <binfile> <varsfile>\n"
	"\tdownload file to dongle ram and start CPU\n"
	"\tvars file will replace vars parsed from the CIS"
	},
	{ "hsic_download", hsic_download, WLC_GET_VAR, WLC_SET_VAR,
	"hsic_download  <binfile> <varsfile>\n"
	"\thsic_download file to dongle ram and start CPU\n"
	"\tvars file will replace vars parsed from the CIS\n"
	},
	{ "upload", dhd_upload, WLC_GET_VAR, WLC_SET_VAR,
	"upload <file> \n"
	"\tupload the entire memory and save it to the file"
	},
#endif /* SERDOWNLOAD */
	{ "rpmt", wl_rpmt, -1, WLC_SET_VAR, "rpmt <pm1-to> <pm0-to>\n"},
#ifdef WLTDLS
	{ "tdls_endpoint", wl_tdls_endpoint, WLC_GET_VAR, WLC_SET_VAR,
	"Available TDLS operations to each TDLS peer.\n"
	"\tusage: wl tdls_endpoint <disc, create, delete, PM, wake, cw> <ea> [chanspec]\n"
	"\t       [chanspec] only applies to 'cw' operaton.\n\n"
	"\t       addendum:\n"
	"\t       wl tdls_endpoint wfd_disc <ea> sends a WFD tunneled Probe Request"},
	{ "tdls_wfd_ie", wl_tdls_wfd_ie, WLC_GET_VAR, -1,
	"To set, get and clear additional WFD IE in setup_req and setup_resp\n"
	"\tTo set2, get2 and clear2 additional WFD IE in tunneled probe_req and probe_resp\n"
	"\tusage: wl tdls_wfd_ie get  <own|peer_eth_addr#> [ip] [port]\n"
	"\t       wl tdls_wfd_ie get2 <own|peer_eth_addr#> [alt_mac] [port] [PC_bit]\n"
	"\t\t       peer_eth_addr#: HH:HH:HH:HH:HH:HH\n"
	"\t\t       and peer must be TDLS connected (only in case of setup)\n\n"
	"\t       wl tdls_wfd_ie <clr|clr2> own\n\n"
	"\t       wl tdls_wfd_ie set  own wfd_ie_hexa_string [ip# [port# [type# [bssid#]]]]\n"
	"\t       wl tdls_wfd_ie set2 own wfd_ie_hexa_string [alt_mac# [port# [type#]]]\n"
	"\t\t       wfd_ie_hexa_string: should start with the full WFD IE header\n"
	"\t\t                           e.g. 0xDDXX506F9A0A...\n"
	"\t\t       ip#:      XXX.XXX.XXX.XXX\n"
	"\t\t       alt_mac#: HH:HH:HH:HH:HH:HH\n"
	"\t\t       port#:    0-65535\n"
	"\t\t       type#:    0 for source, 1 for primary sink\n"
	"\t\t       bssid#:   HH:HH:HH:HH:HH:HH"},
#endif /* WLTDLS */
	{ "spatial_policy", wl_spatial_policy, WLC_GET_VAR, WLC_SET_VAR,
	"set/get spatial_policy\n"
	"\tUsage: wl spatial_policy <-1: auto / 0: turn off / 1: turn on>\n"
	"\t       to control individual band/sub-band use\n"
	"\t       wl spatial_policy a b c d e\n"
	"\t       where a is 2.4G band setting\n"
	"\t       where b is 5G lower band setting\n"
	"\t       where c is 5G middle band setting\n"
	"\t       where d is 5G high band setting\n"
	"\t       where e is 5G upper band setting"},
	{ "ie", wl_ie, WLC_GET_VAR, WLC_SET_VAR,
	"set/get IE\n"
	"\tUsage: For set: wl ie type length hexdata\n"
	"\t     For get: wl ie type" },
	{ "wnm_url", wl_wnm_url, WLC_GET_VAR, WLC_SET_VAR,
	"set/get wnm session information url\n"
	"Usage for set: wl wnm_url length urlstring\n"
	"Usage for get: wl wnm_url" },
	{ "ratetbl_ppr", wl_ratetbl_ppr, WLC_GET_VAR, WLC_SET_VAR,
	"Usage: For get: wl ratetbl_ppr\n"
	"\t     For set: wl ratetbl_ppr <rate> <ppr>" },
	{ "wowl_wakeup_reason", wl_wowl_wake_reason, WLC_GET_VAR, -1 /* Set not reqd */,
	"Returns pattern id and associated wakeup reason"},
	{ "mempool", wlu_mempool, WLC_GET_VAR, -1,
	"Get memory pool statistics" },
#endif /* !ATE_BUILD */

#if defined(WLNDOE) || defined(WLOFFLD)
	{ "nd_hostip", wl_hostipv6, WLC_GET_VAR, WLC_SET_VAR,
	"Add a local host-ipv6 address or display them"},
#endif
#ifdef WLNDOE
	{ "nd_solicitip", wl_solicitipv6, WLC_GET_VAR, WLC_SET_VAR,
	"Add a local host solicit ipv6 address or display them"},
	{ "nd_remoteip", wl_remoteipv6, WLC_GET_VAR, WLC_SET_VAR,
	"Add a local remote ipv6 address or display them"},
	{ "nd_status", wl_ndstatus, WLC_GET_VAR, -1,
	"Displays Neighbor Discovery Status"},
	{ "nd_hostip_clear", wl_var_void, -1, WLC_SET_VAR,
	"Clear all host-ip addresses"},
	{ "nd_macaddr", wl_iov_mac, WLC_GET_VAR, WLC_SET_VAR,
	"Get/set the MAC address for offload" },
	{ "nd_status_clear", wl_var_void, -1, WLC_SET_VAR,
	"Clear neighbor discovery status"},
#endif
	{ "lpc_params", wl_lpc_params, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get Link Power Control params\n"
	"\tUsage: wl lpc_params <rate_stab_thresh>\n"
	"\t\t<pwr_stab_thresh> <lpc_exp_time>\n"
	"\t\t<pwrup_slow_step> <pwrup_fast_step> <pwrdn_slow_step>\n"},
	{ "mode_reqd", wl_bsscfg_int, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get operational capabilities required for STA to associate to the BSS "
	"supported by the interface.\n"
	"\tUsage: wl [-i ifname] mode_reqd [value]\n"
	"\t       wl mode_reqd [-C bss_idx ] [value]\n"
	"\t\t     <ifname> is the name of the interface corresponding to the BSS.\n"
	"\t\t\t   If the <ifname> is not given, the primary BSS is assumed.\n"
	"\t\t     <bss_idx> is the the BSS configuration index.\n"
	"\t\t\t   If the <bss_idx> is not given, configuraion #0 is assumed\n"
	"\t\t     <value> is the numeric values in the range [0..3]\n"
	"\t\t     0 - no requirements on joining devices.\n"
	"\t\t     1 - devices must advertise ERP (11g) capabilities to be allowed to associate\n"
	"\t\t\t   to a 2.4 GHz BSS.\n"
	"\t\t     2 - devices must advertise HT (11n) capabilities to be allowed to associate\n"
	"\t\t\t   to a BSS.\n"
	"\t\t     3 - devices must advertise VHT (11ac) capabilities to be allowed to associate\n"
	"\t\t\t   to a BSS.\n"
	"\tThe command returns an error if the BSS interface is up.\n"
	"\tThis configuration can only be changed while the BSS interface is down.\n"
	"\tNote that support for HT implies support for ERP,\n"
	"\tand support for VHT implies support for HT."},
	{ "nar_clear_dump", wl_var_void, -1, WLC_SET_VAR,
	"Clear non-aggregated regulation counters"},
	{ "sar_limit", wl_sarlimit, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get sar_limit\n"
	"\tusage: (set) sar_limit <2Gcore0 2Gcore1 2Gcore2 2Gcore3 5G[0]core0 5G[0]core1...>\n"
	"\t       (get) sar_limit, return sar limit table\n"
	"\tunit: all input/output values are absolute and in unit of qdbm\n"
	},
	{ "rmc_ackmac", wl_mcast_ackmac, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get ACK required multicast mac address\n"
	"\tusage: wl mcast_ackmac [multicast mac address]\n"
	},
	{ "rmc_txrate", wl_phy_rate, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get a fixed transmit rate for the reliable multicast:\n"
	"\tvalid values for 802.11ac are (6, 9, 12, 18, 24, 36, 48, 54)\n"
	"\t-1 (default) means automatically determine the best rate\n"
	},
	{ "rmc_status", wl_mcast_status, WLC_GET_VAR, -1,
	"Display reliable multicast client status\n"
	},
	{ NULL, NULL, 0, 0, NULL }
};

cmd_t wl_varcmd = {"var", wl_varint, -1, -1, "unrecognized name, type -h for help"};

#ifndef ATE_BUILD
#ifdef WC_TOOL
/* Include any commands for wc tool used for WMM
 * These need to be only the command names from port_cmds and wl_cmds array
 */
static const char *wc_cmds[] = {
	"ver", "cmds", "up", "down",
	"gmode", "listen", "wme", "wme_ac", "wme_apsd",
	"wme_apsd_sta", "wme_dp"
};
#else
static const char **wc_cmds = NULL;
#endif /* WC_TOOL */
#endif /* !ATE_BUILD */

/* initialize stuff needed before processing the command */
void
wl_cmd_init(void)
{
	int_fmt = INT_FMT_DEC;
}


void
wlu_init(void)
{
	/* Init global variables at run-time, not as part of the declaration.
	 * This is required to support init/de-init of the driver. Initialization
	 * of globals as part of the declaration results in non-deterministic
	 * behaviour since the value of the globals may be different on the
	 * first time that the driver is initialized vs subsequent initializations.
	 */
	int_fmt = INT_FMT_DEC;
	batch_in_client = FALSE;
	init_cmd_batchingmode();
}

int
wl_check(void *wl)
{
	int ret;
	int val;

	if ((ret = wlu_get(wl, WLC_GET_MAGIC, &val, sizeof(int))) < 0)
		return ret;

	/* Detect if IOCTL swapping is necessary */
	if (val == (int)bcmswap32(WLC_IOCTL_MAGIC))
	{
		val = bcmswap32(val);
		g_swap = TRUE;
	}
	if (val != WLC_IOCTL_MAGIC)
		return -1;
	if ((ret = wlu_get(wl, WLC_GET_VERSION, &val, sizeof(int))) < 0)
		return ret;
	ioctl_version = dtoh32(val);
	if (ioctl_version != WLC_IOCTL_VERSION &&
	    ioctl_version != 1) {
		fprintf(stderr, "Version mismatch, please upgrade. Got %d, expected %d or 1\n",
		        ioctl_version, WLC_IOCTL_VERSION);
		return -1;
	}
	return 0;
}

#ifndef ATE_BUILD
/* parse/validate the command line arguments */
/*
 * pargv is updated upon return if the first argument is an option.
 * It remains intact otherwise.
 */
int
wl_option(char ***pargv, char **pifname, int *phelp)
{
	char *ifname = NULL;
	int help = FALSE;
	int status = CMD_OPT;
	char **argv = *pargv;

	while (*argv) {
		/* select different adapter */
		if (!strcmp(*argv, "-a") || !strcmp(*argv, "-i")) {
			char *opt = *argv++;
			ifname = *argv;
			if (!ifname) {
				fprintf(stderr,
					"error: expected interface name after option %s\n", opt);
				status = CMD_ERR;
				break;
			}
		}

		/* integer output format */
		else if (!strcmp(*argv, "-d"))
			int_fmt = INT_FMT_DEC;
		else if (!strcmp(*argv, "-u"))
			int_fmt = INT_FMT_UINT;
		else if (!strcmp(*argv, "-x"))
			int_fmt = INT_FMT_HEX;

		/* command usage */
		else if (!strcmp(*argv, "-h") || !strcmp(*argv, "--help"))
			help = TRUE;

		else if (!strcmp(*argv, "--clientbatch")) {
			wl_seq_batch_in_client(TRUE);
		}
		/* To handle endian mis-matches between wl utility and wl driver */
		else if (!strcmp(*argv, "--es")) {
			g_swap = TRUE;
		}
		/* start of non wl options */
		else {
			status = CMD_WL;
			break;
		}

		/* consume the argument */
		argv ++;
		break;
	}

	*phelp = help;
	*pifname = ifname;
	*pargv = argv;

	return status;
}

void
wl_cmd_usage(FILE *fid, cmd_t *cmd)
{
	if (strlen(cmd->name) >= 8)
		fprintf(fid, "%s\n\t%s\n\n", cmd->name, cmd->help);
	else
		fprintf(fid, "%s\t%s\n\n", cmd->name, cmd->help);
}

static int
wl_print_deprecate(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(argv);

	wl_cmd_usage(stderr, cmd); /* warning string is in command table */
	return 0;
}

/* Dump out short list of commands */
static int
wl_list(void *wl, cmd_t *garb, char **argv)
{
	cmd_t *cmd;
	int nrows, i, len;
	char *list_buf;
	int letter, col, row, pad;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(garb);
	UNUSED_PARAMETER(argv);

	for (cmd = wl_cmds, nrows = 0; cmd->name; cmd++)
		/* Check for wc_cmd */
		if (wc_cmd_check(cmd->name))
		    nrows++;

	nrows /= 4;
	nrows++;

	len = nrows * 80 + 2;
	list_buf = malloc(len);
	if (list_buf == NULL) {
		fprintf(stderr, "Failed to allocate buffer of %d bytes\n", len);
		return -1;
	}
	for (i = 0; i < len; i++)
		*(list_buf+i) = 0;

	row = col = 0;
	for (letter = 'a'; letter < 'z'; letter++) {
		for (cmd = wl_cmds; cmd->name; cmd++) {
			/* Check for wc_cmd */
			if (!wc_cmd_check(cmd->name))
				continue;
			if (cmd->name[0] == letter || cmd->name[0] == letter - 0x20) {
				strcat(list_buf+row*80, cmd->name);
				pad = 18 * (col + 1) - strlen(list_buf+row*80);
				if (pad < 1)
					pad = 1;
				for (; pad; pad--)
					strcat(list_buf+row*80, " ");
				row++;
				if (row == nrows) {
					col++; row = 0;
				}
			}
		}
	}
	for (row = 0; row < nrows; row++)
		printf("%s\n", list_buf+row*80);

	printf("\n");

	free(list_buf);
	return (0);
}

void
wl_cmds_usage(FILE *fid, cmd_t *port_cmds)
{
	cmd_t *port_cmd;
	cmd_t *cmd;

	/* print usage of port commands */
	for (port_cmd = port_cmds; port_cmd && port_cmd->name; port_cmd++)
		/* Check for wc_cmd */
		if (wc_cmd_check(port_cmd->name))
			wl_cmd_usage(fid, port_cmd);

	/* print usage of common commands without port counterparts */
	for (cmd = wl_cmds; cmd->name; cmd++) {
		/* search if port counterpart exists */
		for (port_cmd = port_cmds; port_cmd && port_cmd->name; port_cmd++)
			if (!strcmp(port_cmd->name, cmd->name))
				break;
		/* Also, check for this being a wc_cmd */
		if ((!port_cmd || !port_cmd->name) && (wc_cmd_check(cmd->name)))
			wl_cmd_usage(fid, cmd);
	}
}

void
wl_usage(FILE *fid, cmd_t *port_cmds)
{
	fprintf(fid, "Usage: %s [-a|i <adapter>] [-h] [-d|u|x] <command> [arguments]\n", wlu_av0);

	fprintf(fid, "\n");
	fprintf(fid, "  -h        this message and command descriptions\n");
	fprintf(fid, "  -h [cmd]  command description for cmd\n");
	fprintf(fid, "  -a, -i    adapter name or number\n");
	fprintf(fid, "  -d        output format signed integer\n");
	fprintf(fid, "  -u        output format unsigned integer\n");
	fprintf(fid, "  -x        output format hexdecimal\n");
	fprintf(fid, "\n");

	wl_cmds_usage(fid, port_cmds);
}

void
wl_printint(int val)
{
	switch (int_fmt) {
	case INT_FMT_UINT:
		printf("%u\n", val);
		break;
	case INT_FMT_HEX:
		printf("0x%x\n", val);
		break;
	case INT_FMT_DEC:
	default:
		printf("%d\n", val);
		break;
	}
}


/* Common routine to check for an option arg specifying the configuration index.
 * Takes the syntax -C num, --cfg=num, --config=num, or --configuration=num
 * Returns -1 if there is a command line parsing error.
 * Returns 0 if no error, and sets *consumed to the number of argv strings
 * used. Sets *bsscfg_idx to the index to use. Will set *bsscfg_idx to zero if there
 * was no config arg.
 */
static int
wl_cfg_option(char **argv, const char *fn_name, int *bsscfg_idx, int *consumed)
{
	miniopt_t mo;
	int opt_err;

	*bsscfg_idx = 0;
	*consumed = 0;

	miniopt_init(&mo, fn_name, NULL, FALSE);

	/* process the first option */
	opt_err = miniopt(&mo, argv);

	/* check for no args or end of options */
	if (opt_err == -1)
		return 0;

	/* check for no options, just a positional arg encountered */
	if (mo.positional)
		return 0;

	/* check for error parsing options */
	if (opt_err == 1)
		return -1;

	/* check for -C, --cfg=X, --config=X, --configuration=X */
	if (mo.opt == 'C' ||
	    !strcmp(mo.key, "cfg") ||
	    !strcmp(mo.key, "config") ||
	    !strcmp(mo.key, "configuration")) {
		if (!mo.good_int) {
			fprintf(stderr,
			"%s: could not parse \"%s\" as an integer for the configuartion index\n",
			fn_name, mo.valstr);
			return -1;
		}
		*bsscfg_idx = mo.val;
		*consumed = mo.consumed;
	}

	return 0;
}
#endif /* !ATE_BUILD */

static int
wl_void(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(argv);

	if (cmd->set < 0)
		return -1;
	return wlu_set(wl, cmd->set, NULL, 0);
}

#ifndef ATE_BUILD
int
wl_int(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char *endptr = NULL;

	if (!*++argv) {
		if (cmd->get == -1)
			return -1;
		if ((ret = wlu_get(wl, cmd->get, &val, sizeof(int))) < 0)
			return ret;

		val = dtoh32(val);
		wl_printint(val);
	} else {
		if (cmd->set == -1)
			return -1;
		if (!stricmp(*argv, "on"))
			val = 1;
		else if (!stricmp(*argv, "off"))
			val = 0;
		else {
			val = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return -1;
			}
		}

		val = htod32(val);
		ret = wlu_set(wl, cmd->set, &val, sizeof(int));
	}

	return ret;
}

/* Return a new chanspec given a legacy chanspec
 * Returns INVCHANSPEC on error
 */
static chanspec_t
wl_chspec_from_legacy(chanspec_t legacy_chspec)
{
	chanspec_t chspec;

	/* get the channel number */
	chspec = LCHSPEC_CHANNEL(legacy_chspec);

	/* convert the band */
	if (LCHSPEC_IS2G(legacy_chspec)) {
		chspec |= WL_CHANSPEC_BAND_2G;
	} else {
		chspec |= WL_CHANSPEC_BAND_5G;
	}

	/* convert the bw and sideband */
	if (LCHSPEC_IS20(legacy_chspec)) {
		chspec |= WL_CHANSPEC_BW_20;
	} else {
		chspec |= WL_CHANSPEC_BW_40;
		if (LCHSPEC_CTL_SB(legacy_chspec) == WL_LCHANSPEC_CTL_SB_LOWER) {
			chspec |= WL_CHANSPEC_CTL_SB_L;
		} else {
			chspec |= WL_CHANSPEC_CTL_SB_U;
		}
	}

	if (wf_chspec_malformed(chspec)) {
		fprintf(stderr, "wl_chspec_from_legacy: output chanspec (0x%04X) malformed\n",
		        chspec);
		return INVCHANSPEC;
	}

	return chspec;
}

/* Return a legacy chanspec given a new chanspec
 * Returns INVCHANSPEC on error
 */
chanspec_t
wl_chspec_to_legacy(chanspec_t chspec)
{
	chanspec_t lchspec;

	if (wf_chspec_malformed(chspec)) {
		fprintf(stderr, "wl_chspec_to_legacy: input chanspec (0x%04X) malformed\n",
		        chspec);
		return INVCHANSPEC;
	}

	/* get the channel number */
	lchspec = CHSPEC_CHANNEL(chspec);

	/* convert the band */
	if (CHSPEC_IS2G(chspec)) {
		lchspec |= WL_LCHANSPEC_BAND_2G;
	} else {
		lchspec |= WL_LCHANSPEC_BAND_5G;
	}

	/* convert the bw and sideband */
	if (CHSPEC_IS20(chspec)) {
		lchspec |= WL_LCHANSPEC_BW_20;
		lchspec |= WL_LCHANSPEC_CTL_SB_NONE;
	} else if (CHSPEC_IS40(chspec)) {
		lchspec |= WL_LCHANSPEC_BW_40;
		if (CHSPEC_CTL_SB(chspec) == WL_CHANSPEC_CTL_SB_L) {
			lchspec |= WL_LCHANSPEC_CTL_SB_LOWER;
		} else {
			lchspec |= WL_LCHANSPEC_CTL_SB_UPPER;
		}
	} else {
		/* cannot express the bandwidth */
		char chanbuf[CHANSPEC_STR_LEN];
		fprintf(stderr,
		        "wl_chspec_to_legacy: unable to convert chanspec %s (0x%04X) "
		        "to pre-11ac format\n",
		        wf_chspec_ntoa(chspec, chanbuf), chspec);
		return INVCHANSPEC;
	}

	return lchspec;
}

/* given a chanspec value, do the endian and chanspec version conversion to
 * a chanspec_t value
 * Returns INVCHANSPEC on error
 */
static chanspec_t
wl_chspec_to_driver(chanspec_t chanspec)
{
	if (ioctl_version == 1) {
		chanspec = wl_chspec_to_legacy(chanspec);
		if (chanspec == INVCHANSPEC) {
			return chanspec;
		}
	}
	chanspec = htodchanspec(chanspec);

	return chanspec;
}

/* given a chanspec value, do the endian and chanspec version conversion to
 * a chanspec_t value in a 32 bit integer
 * Returns INVCHANSPEC on error
 */
static uint32
wl_chspec32_to_driver(chanspec_t chanspec)
{
	uint32 val;

	if (ioctl_version == 1) {
		chanspec = wl_chspec_to_legacy(chanspec);
		if (chanspec == INVCHANSPEC) {
			return chanspec;
		}
	}
	val = htod32((uint32)chanspec);

	return val;
}

/* given a chanspec value from the driver, do the endian and chanspec version conversion to
 * a chanspec_t value
 * Returns INVCHANSPEC on error
 */
static chanspec_t
wl_chspec_from_driver(chanspec_t chanspec)
{
	chanspec = dtohchanspec(chanspec);
	if (ioctl_version == 1) {
		chanspec = wl_chspec_from_legacy(chanspec);
	}

	return chanspec;
}

/* given a chanspec value from the driver in a 32 bit integer, do the endian and
 * chanspec version conversion to a chanspec_t value
 * Returns INVCHANSPEC on error
 */
static chanspec_t
wl_chspec32_from_driver(uint32 chanspec32)
{
	chanspec_t chanspec;

	chanspec = (chanspec_t)dtoh32(chanspec32);

	if (ioctl_version == 1) {
		chanspec = wl_chspec_from_legacy(chanspec);
	}

	return chanspec;
}


#ifdef CLMDOWNLOAD
/*
Generic interface for downloading required data onto the dongle
*/
int
download2dongle(void *wl, uint16 dload_type, unsigned char *dload_buf, int len)
{
	struct wl_dload_data *dload_ptr = (struct wl_dload_data *)dload_buf;
	int err = 0;
	int dload_data_offset;

	dload_data_offset = OFFSETOF(wl_dload_data_t, data);
	dload_ptr->flag = (DLOAD_HANDLER_VER << DLOAD_FLAG_VER_SHIFT);
	dload_ptr->dload_type = dload_type;
	dload_ptr->len = htod32(len - dload_data_offset);

	dload_ptr->crc = 0;

	len = len + 8 - (len%8);

	err = wlu_iovar_setbuf(wl, "generic_dload",
		dload_buf, len, buf, WLC_IOCTL_MEDLEN);
	return err;
}


static int
check_clm_file(unsigned char *headers)
{
	struct trx_header *trx;
	int content_len = -1;

	/* Extract trx header */
	trx = (struct trx_header *)headers;
	if (trx->magic != TRX_MAGIC) {
		printf("Error: trx bad hdr\n");
		goto err;
	}

	content_len = trx->offsets[0];
err:
	return content_len;
}


int
dload_clm(void *wl, uint32 datalen, unsigned char *org_buf, int ds_id)
{
	int num_chunks, chunk_len, cumulative_len = 0;
	int size2alloc;
	unsigned char *new_buf;
	wl_clm_dload_info_t *clm_info_ptr;
	int err = 0, clm_info_offset, chunk_offset;

	clm_info_offset = OFFSETOF(wl_dload_data_t, data);
	chunk_offset = OFFSETOF(wl_clm_dload_info_t, data_chunk);

	num_chunks = datalen/MAX_CHUNK_LEN;
	if (datalen % MAX_CHUNK_LEN != 0)
		num_chunks++;
	size2alloc = clm_info_offset + chunk_offset + MAX_CHUNK_LEN;

	if ((new_buf = (unsigned char *)malloc(size2alloc)) != NULL) {
		memset(new_buf, 0, size2alloc);
		clm_info_ptr = (wl_clm_dload_info_t*)((uint8 *)new_buf + clm_info_offset);
		clm_info_ptr->num_chunks = num_chunks;
		clm_info_ptr->clm_total_len = datalen;
		clm_info_ptr->ds_id = ds_id;
		do {
			if (datalen >= MAX_CHUNK_LEN)
				chunk_len = MAX_CHUNK_LEN;
			else
				chunk_len = datalen;
			memset(new_buf + clm_info_offset + chunk_offset, 0,
				size2alloc - clm_info_offset - chunk_offset);
			clm_info_ptr->chunk_len = htod32(chunk_len);

			memcpy(&clm_info_ptr->data_chunk[0], org_buf + cumulative_len, chunk_len);
			clm_info_ptr->chunk_offset = cumulative_len;
			cumulative_len += chunk_len;

			download2dongle(wl, DL_TYPE_CLM, new_buf,
				chunk_len + clm_info_offset + chunk_offset);

			datalen = datalen - chunk_len;
		} while (datalen > 0);
		free(new_buf);
	} else {
		err = BCME_NOMEM;
	}

	return err;
}


int
process_clm_data(void *wl, char *clmfn, int ds_id)
{
	int ret = 0;

	FILE *fp = NULL;

	int clm_filelen;
	int clm_data_len = 0;
	struct stat filest;
	unsigned char *new_buf = NULL, *new_ptr = NULL;
	unsigned long status = 0;
	int ifd;

	printf("Download CLM file: %s\n", clmfn);

	if (clmfn == NULL) {
		goto do_clm_load; /* Just erase (inc) or revert (ROM) data if nothing to load */
	}

	/* Open the firmware file */
	if (!(fp = fopen(clmfn, "rb"))) {
		fprintf(stderr, "Unable to open %s.\n", clmfn);
		ret = -EINVAL;
		goto error;
	}

	ifd = fileno(fp);

	if (fstat(ifd, &filest)) {
		fprintf(stderr, "process_clm_data: %s: %s\n", clmfn, strerror(errno));
		ret = -EINVAL;
		goto error;
	}

	clm_filelen = filest.st_size;

	if (clm_filelen < (int)sizeof(struct trx_header)) {
		fprintf(stderr, "%s not a valid TRX file.\n", clmfn);
		ret = -EINVAL;
		goto error;
	}

	if ((new_buf = malloc(clm_filelen)) == NULL) {
		fprintf(stderr, "Unable to allocate %d bytes!\n", clm_filelen);
		ret = -ENOMEM;
		goto error;
	}

	/* Read ONLY the firmware-file-header into the new_buffer */
	status = fread(new_buf, 1, sizeof(struct trx_header), fp);
	if ((ret = ferror(fp))) {
		fprintf(stderr, "error %d reading %s\n", ret, clmfn);
		ret = -EINVAL;
		goto error;
	}

	if (status < sizeof(struct trx_header)) {
		fprintf(stderr, "Bad hdr read for %s!\n", clmfn);
		ret = -EINVAL;
		goto error;
	}

	/* Validate the format/length etc of the file. Note: clm_data_len
	 * omits the trx header and trailing zeroes
	 */
	clm_data_len = check_clm_file(new_buf);
	if ((clm_data_len <= 0) ||
		((clm_data_len + (int)sizeof(struct trx_header)) > clm_filelen)) {
		fprintf(stderr, "process_clm_data:	Failed input file check\n");
		ret = -1;
		goto error;
	}
	status = fread(new_buf + status, 1, clm_data_len, fp);

	/* clm fw */
	new_ptr = (unsigned char *)(new_buf + sizeof(struct trx_header));

do_clm_load:
	dload_clm(wl, clm_data_len, new_ptr, ds_id);

error:
	if (new_buf)
		free(new_buf);
	if (fp)
		fclose(fp);

	return ret;
}


static int
wl_clmload(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 1;

	++argv;
	if (cmd->set == -1)
		return -1;
	{
		int ds_id = atoi(*argv);
		char* fname = *++argv;

		if ((ds_id == 0) || (ds_id == 1))
			process_clm_data(wl, fname, ds_id);
	}
	return ret;
}
#endif /* CLMDOWNLOAD */


static int
wl_bsscfg_int(void *wl, cmd_t *cmd, char **argv)
{
	char *endptr = NULL;
	char *val_name;
	int bsscfg_idx = 0;
	int val = 0;
	int consumed;
	int ret;

	val_name = *argv++;

	/* parse a bsscfg_idx option if present */
	if ((ret = wl_cfg_option(argv, val_name, &bsscfg_idx, &consumed)) != 0)
		return ret;

	/* handle a bsscfg int with a legacy ioctl */
	if (consumed == 0 && cmd->set != WLC_SET_VAR) {
		/* back up to the orig command and run as an ioctl int */
		argv--;
		return wl_int(wl, cmd, argv);
	}

	argv += consumed;

	if (!*argv) {
		/* This is a GET */
		if (cmd->get == -1)
			return -1;

		if (consumed == 0)
			ret = wlu_iovar_getint(wl, val_name, &val);
		else
			ret = wl_bssiovar_getint(wl, val_name, bsscfg_idx, &val);

		if (ret < 0)
			return ret;

		wl_printint(val);
	} else {
		/* This is a SET */
		if (cmd->set == -1)
			return -1;

		if (!stricmp(*argv, "on"))
			val = 1;
		else if (!stricmp(*argv, "off"))
			val = 0;
		else {
			val = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return -1;
			}
		}

		if (consumed == 0)
			ret = wlu_iovar_setint(wl, val_name, val);
		else
			ret = wl_bssiovar_setint(wl, val_name, bsscfg_idx, val);
	}

	return ret;
}

static int
wl_bsscfg_enable(void *wl, cmd_t *cmd, char **argv)
{
	char *endptr;
	const char *val_name = "bss";
	int bsscfg_idx = 0;
	int val;
	int consumed;
	int ret;

	UNUSED_PARAMETER(cmd);

	/* skip the command name */
	argv++;

	/* parse a bsscfg_idx option if present */
	if ((ret = wl_cfg_option(argv, val_name, &bsscfg_idx, &consumed)) != 0)
		return ret;

	argv += consumed;
	if (consumed == 0) { /* Use the -i parameter if that was present */
		bsscfg_idx = -1;
	}

	if (!*argv) {
		bsscfg_idx = htod32(bsscfg_idx);
		ret = wlu_iovar_getbuf(wl, val_name, &bsscfg_idx, sizeof(bsscfg_idx),
		                      buf, WLC_IOCTL_MAXLEN);
		if (ret < 0)
			return ret;
		val = *(int*)buf;
		val = dtoh32(val);
		if (val)
			printf("up\n");
		else
			printf("down\n");
		return 0;
	} else {
		struct {
			int cfg;
			int val;
		} bss_setbuf;
		if (!stricmp(*argv, "ap"))
			val = 3;
		else if (!stricmp(*argv, "sta"))
			val = 2;
		else if (!stricmp(*argv, "up"))
			val = 1;
		else if (!stricmp(*argv, "down"))
			val = 0;
		else if (!stricmp(*argv, "del"))
			val = -1;
		else {
			val = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return -1;
			}
		}
		bss_setbuf.cfg = htod32(bsscfg_idx);
		bss_setbuf.val = htod32(val);

		return wlu_iovar_set(wl, val_name, &bss_setbuf, sizeof(bss_setbuf));
	}
}

/* Get/Set the gmode config */
static int
wl_gmode(void *wl, cmd_t *cmd, char **argv)
{
	char *endptr = NULL;
	int ret = 0, val;

	if (!*++argv) {
		const char *gconfig;

		/* Get the current G mode */
		if ((ret = wlu_get(wl, cmd->get, &val, sizeof(val))) < 0)
			return ret;

		val = dtoh32(val);
		switch (val) {
		case GMODE_LEGACY_B:
			gconfig = "54g Legacy B";
			break;
		case GMODE_AUTO:
			gconfig = "54g Auto";
			break;
		case GMODE_ONLY:
			gconfig = "54g Only";
			break;
		case GMODE_PERFORMANCE:
			gconfig = "54g Performance";
			break;
		case GMODE_LRS:
			gconfig = "54g LRS";
			break;
		default:
			gconfig = "unknown";
			break;
		}

		printf("%s (%d)\n", gconfig, val);

	} else {
		/* Set the new G mode */

		if (!strnicmp(*argv, "legacy", 6))
			val = GMODE_LEGACY_B;
		else if (!strnicmp(*argv, "auto", 4))
			val = GMODE_AUTO;
		else if (!strnicmp(*argv, "gonly", 5))
			val = GMODE_ONLY;
		else if (!strnicmp(*argv, "perf", 4))
			val = GMODE_PERFORMANCE;
		else if (!strnicmp(*argv, "lrs", 3))
			val = GMODE_LRS;
		else {
			val = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return -1;
			}
		}

		/* Set the gmode configration */
		val = htod32(val);
		if ((ret = wlu_set(wl, cmd->set, &val, sizeof(val))))
			goto done;

	}

done:
	return (ret);
}


static int
wl_overlay(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int argc;
	char *endptr = NULL;
	void *ptr = NULL;
	int param[3];

	/* arg count */
	for (argc = 0; argv[argc]; argc++);
	argc--;

	if (argc < 1 || argc > 3) {
		printf("required args: virt_addr phy_addr size\n");
		return -1;
	}

	param[0] = strtol(argv[1], &endptr, 0);
	if (*endptr != '\0' || (param[0] & ~0x003FFE00) != 0) {
		printf("Invaild virtual address: %s\n", argv[1]);
		return -1;
	}

	if (argc == 1) {
		if ((ret = wlu_var_getbuf(wl, cmd->name, param, sizeof(int), &ptr)) >= 0) {
			wl_hexdump((uchar *)ptr, 512);
		}
		return (ret);
	}

	param[1] = strtol(argv[2], &endptr, 0);
	if (*endptr != '\0' || (param[1] & ~0x003FFE00) != 0) {
		printf("Invaild physical Address: %s\n", argv[2]);
		return -1;
	}

	if (argc == 3) {
		param[2] = strtol(argv[3], &endptr, 0);
		if (*endptr != '\0' || param[2] < 0 || param[2] > 7) {
			printf("Invaild size: %s\n", argv[3]);
			return -1;
		}
	} else {
		param[2] = 0;
	}

	printf("Setting virtual Address 0x%x to physical Address 0x%x, size is %d\n",
		param[0], param[1], param[2]);
	ret = wlu_var_setbuf(wl, cmd->name, param, sizeof(param));

	return (ret);
}
#endif /* !ATE_BUILD */

static int
wl_reg(void *wl, cmd_t *cmd, char **argv)
{
	int reg;
	int ret;
	struct {
		int val;
		int band;
	} x;
	char *endptr = NULL;
	uint argc;
	bool core_cmd;

	/* eat command name */
	argv++;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	/* required arg: reg offset */
	if (argc < 1)
		return -1;

	reg = strtol(argv[0], &endptr, 0);

	if (*endptr != '\0')
		return -1;

	x.val = 0;
	x.band = WLC_BAND_AUTO;
	core_cmd = FALSE;

	/* Second arg: value or band or "radio core" */
	if (argc >= 2) {
		if (!stricmp(argv[1], "a"))
			x.band = WLC_BAND_5G;
		else if (!stricmp(argv[1], "b"))
			x.band = WLC_BAND_2G;
		else {
			/* For NPHY Rev >= 3, the 2nd argument can be
			   the radio core
			 */
			if (strcmp(cmd->name, "radioreg") == 0) {
				if (strcmp(argv[1], "syn") == 0) {
					reg |= RADIO_CORE_SYN;
					core_cmd = TRUE;
				} else if (strcmp(argv[1], "tx0") == 0) {
					reg |= RADIO_CORE_TX0;
					core_cmd = TRUE;
				} else if (strcmp(argv[1], "tx1") == 0) {
					reg |= RADIO_CORE_TX1;
					core_cmd = TRUE;
				} else if (strcmp(argv[1], "rx0") == 0) {
					reg |= RADIO_CORE_RX0;
					core_cmd = TRUE;
				} else if (strcmp(argv[1], "rx1") == 0) {
					reg |= RADIO_CORE_RX1;
					core_cmd = TRUE;
				}
			}
			/* For HTPHY, the 2nd argument can be
			   the radio core
			 */
			if (strcmp(cmd->name, "radioreg") == 0) {
				if (strcmp(argv[1], "cr0") == 0) {
					reg |= RADIO_CORE_CR0;
					core_cmd = TRUE;
				} else if (strcmp(argv[1], "cr1") == 0) {
					reg |= RADIO_CORE_CR1;
					core_cmd = TRUE;
				} else if (strcmp(argv[1], "cr2") == 0) {
					reg |= RADIO_CORE_CR2;
					core_cmd = TRUE;
				}
			}
			/* If the second argument is a value */
			if (!core_cmd) {
				x.val = strtol(argv[1], &endptr, 0);
				if (*endptr != '\0')
					return (-1);
			}
		}
	}

	/* Third arg: band OR "radio core" */
	if (argc >= 3) {
		if (!stricmp(argv[2], "a"))
			x.band = WLC_BAND_5G;
		else if (!stricmp(argv[2], "b"))
			x.band = WLC_BAND_2G;
		else {
			/* For NPHY Rev >= 3, the 3rd argument can be
			   the radio core
			 */
			core_cmd = FALSE;
			if (strcmp(cmd->name, "radioreg") == 0) {
				if (strcmp(argv[2], "syn") == 0) {
					reg |= RADIO_CORE_SYN;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "tx0") == 0) {
					reg |= RADIO_CORE_TX0;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "tx1") == 0) {
					reg |= RADIO_CORE_TX1;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "rx0") == 0) {
					reg |= RADIO_CORE_RX0;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "rx1") == 0) {
					reg |= RADIO_CORE_RX1;
					core_cmd = TRUE;
				}
			}
			/* For HTPHY, the 3rd argument can be
			   the radio core
			 */
			if (strcmp(cmd->name, "radioreg") == 0) {
				if (strcmp(argv[2], "cr0") == 0) {
					reg |= RADIO_CORE_CR0;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "cr1") == 0) {
					reg |= RADIO_CORE_CR1;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "cr2") == 0) {
					reg |= RADIO_CORE_CR2;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "all") == 0) {
					reg |= RADIO_CORE_ALL;
					core_cmd = TRUE;
				}
			}

			if (!core_cmd) {
				return (-1);
			}
		}
	}

	x.val = (x.val << 16) | (reg & 0xffff);

	/* issue the get or set ioctl */
	if ((argc == 1) || ((argc == 2) && ((x.band != WLC_BAND_AUTO) || core_cmd))) {
		x.band = htod32(x.band);
		x.val = htod32(x.val);
		if ((ret = wlu_get(wl, cmd->get, &x, sizeof(x))) < 0)
			return (ret);
		printf("0x%04x\n", (uint16)(dtoh32(x.val)));
	} else {
		x.band = htod32(x.band);
		x.val = htod32(x.val);
		ret = wlu_set(wl, cmd->set, &x, sizeof(x));
	}

	return (ret);
}

static int
wl_gpioout(void *wl, cmd_t *cmd, char **argv)
{
	uint32 mask;
	uint32 val;
	char *endptr = NULL;
	uint argc;
	uint32 *int_ptr;
#ifdef ATE_BUILD
	uint32 params[2];
#endif

	UNUSED_PARAMETER(cmd);

	val = 0;

	/* eat command name */
	argv++;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	/* Get and print the values */
	if (argc == 0) {
		uint32 gpio_cntrl;
		uint32 gpio_out;
		uint32 gpio_outen;

		if (wlu_iovar_get(wl, "gpioout", buf, sizeof(uint32) *3))
			return -1;
		gpio_cntrl = dtoh32(((uint32 *)buf)[0]);
		gpio_out = dtoh32(((uint32 *)buf)[1]);
		gpio_outen = dtoh32(((uint32 *)buf)[2]);

		printf("gpiocontrol 0x%x gpioout 0x%x gpioouten 0x%x\n", gpio_cntrl,
		       gpio_out, gpio_outen);

		return 0;
	}

	/* required arg: mask value */
	if (argc < 2)
		return -1;

	mask = strtoul(argv[0], &endptr, 0);
	if (*endptr != '\0')
		return -1;

	val = strtoul(argv[1], &endptr, 0);
	if (*endptr != '\0')
		return -1;

	if ((~mask & val) != 0)
		return -1;

#ifdef ATE_BUILD
	int_ptr = (uint32 *)params;
#else
	int_ptr = (uint32 *)buf;
#endif
	mask = htod32(mask);
	memcpy(int_ptr, (const void *)&mask, sizeof(mask));
	int_ptr++;
	val = htod32(val);
	memcpy(int_ptr, (const void *)&val, sizeof(val));

#ifdef ATE_BUILD
	return (wlu_var_setbuf(wl, "gpioout", params, sizeof(uint32) *2));
#else
	return wlu_iovar_set(wl, "gpioout", buf, sizeof(uint32) *2);
#endif /* ATE_BUILD */
}

#ifndef ATE_BUILD
static int
wl_macreg(void *wl, cmd_t *cmd, char **argv)
{
	int reg;
	int size;
	uint32 val;
	int ret;
	char *endptr = NULL;
	rw_reg_t rwt;
	uint argc;

	val = 0;

	/* eat command name */
	argv++;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	/* required arg: reg offset */
	if (argc < 1)
		return -1;

	reg = strtol(argv[0], &endptr, 0);
	if (*endptr != '\0')
		return -1;

	/* required arg: reg size */
	if (argc < 2)
		return (-1);

	size = strtol(argv[1], &endptr, 0);
	if (*endptr != '\0')
		return -1;

	rwt.band = WLC_BAND_AUTO;

	/* Third arg: new value or band */
	if (argc >= 3) {
		if (!stricmp(argv[2], "a"))
			rwt.band = WLC_BAND_5G;
		else if (!stricmp(argv[2], "b"))
			rwt.band = WLC_BAND_2G;
		else {
			val = strtoul(argv[2], &endptr, 0);
			if (*endptr != '\0')
				return -1;
		}

	}

	/* Fourth arg: band */
	if (argc >= 4) {
		if (!stricmp(argv[3], "a"))
			rwt.band = WLC_BAND_5G;
		else if (!stricmp(argv[3], "b"))
			rwt.band = WLC_BAND_2G;
		else
			return (-1);
	}

	if ((argc == 2) || ((argc == 3) && (rwt.band != WLC_BAND_AUTO))) {
		rwt.band = htod32(rwt.band);
		rwt.byteoff = htod32(reg);
		rwt.size = htod32(size);
		if ((ret = wlu_get(wl, cmd->get, &rwt, sizeof(rw_reg_t))) < 0)
			return (ret);
		printf("0x%04x\n", dtoh32(rwt.val));
	}
	else {
		rwt.band = htod32(rwt.band);
		rwt.byteoff = htod32(reg);
		rwt.size = htod32(size);
		rwt.val = htod32(val);
		ret = wlu_set(wl, cmd->set, &rwt, sizeof(rw_reg_t));
	}

	return (ret);
}

/*
 * get or get a band specific variable
 * the band can be a/b/all or omitted. "all"(set only)
 * means all supported bands. blank means current band
 */
static int
wl_band_elm(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	struct {
		int val;
		int band;
	} x;
	char *endptr = NULL;
	uint argc;

	/* eat command name */
	argv++;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	x.val = 0;
	x.band = WLC_BAND_AUTO;

	/* First arg: value or band */
	if (argc >= 1) {
		if (!stricmp(argv[0], "a"))
			x.band = WLC_BAND_5G;
		else if (!stricmp(argv[0], "b"))
			x.band = WLC_BAND_2G;
		else if (!stricmp(argv[0], "all"))
			x.band = WLC_BAND_ALL;
		else {
			x.val = strtol(argv[0], &endptr, 0);
			if (*endptr != '\0')
				return (-1);
		}
	}

	/* Second arg: band */
	if (argc >= 2) {
		if (!stricmp(argv[1], "a"))
			x.band = WLC_BAND_5G;
		else if (!stricmp(argv[1], "b"))
			x.band = WLC_BAND_2G;
		else if (!stricmp(argv[1], "all"))
			x.band = WLC_BAND_ALL;
		else
			return (-1);
	}

	/* issue the get or set ioctl */
	if ((argc == 0) || ((argc == 1) && (x.band != WLC_BAND_AUTO))) {
		if (x.band == WLC_BAND_ALL) {
			printf("band option \"all\" is for set only, not get\n");
			return (-1);
		}

		x.band = htod32(x.band);
		if ((ret = wlu_get(wl, cmd->get, &x, sizeof(x))) < 0)
			return (ret);

		printf("%s is 0x%04x(%d)\n", cmd->name, (uint16)(dtoh32(x.val)), dtoh32(x.val));
	} else {
		x.band = htod32(x.band);
		x.val = htod32(x.val);
		ret = wlu_set(wl, cmd->set, &x, sizeof(x));
	}

	return (ret);
}

/* Command may or may not take a MAC address */
static int
wl_rssi(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	scb_val_t scb_val;
	int32 rssi;

	if (!*++argv) {
		if ((ret = wlu_get(wl, cmd->get, &rssi, sizeof(rssi))) < 0)
			return ret;
		printf("%d\n", dtoh32(rssi));
		return 0;
	} else {
		if (!wl_ether_atoe(*argv, &scb_val.ea))
			return -1;
		if ((ret = wlu_get(wl, cmd->get, &scb_val, sizeof(scb_val))) < 0)
			return ret;
		printf("%d\n", dtoh32(scb_val.val));
		return 0;
	}
}

static int
wl_rssi_event(void *wl, cmd_t *cmd, char **argv)
{
	int ret;

	if (!*++argv) {
		/* get */
		void *ptr = NULL;
		wl_rssi_event_t rssi;
		uint i;

		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;

		memcpy(&rssi, ptr, sizeof(rssi));
		rssi.rate_limit_msec = dtoh32(rssi.rate_limit_msec);

		printf("%d", rssi.rate_limit_msec);
		for (i = 0; i < rssi.num_rssi_levels; i++) {
			printf(" %d", rssi.rssi_levels[i]);
		}
		printf("\n");
	} else {
		/* set */
		wl_rssi_event_t rssi;

		memset(&rssi, 0, sizeof(wl_rssi_event_t));
		rssi.rate_limit_msec = atoi(*argv);

		while (*++argv && rssi.num_rssi_levels < MAX_RSSI_LEVELS) {
			rssi.rssi_levels[rssi.num_rssi_levels++] = atoi(*argv);
			if (rssi.num_rssi_levels > 1) {
				if (rssi.rssi_levels[rssi.num_rssi_levels - 1] <=
					rssi.rssi_levels[rssi.num_rssi_levels - 2]) {
					/* rssi levels must be in increasing order */
					return -1;
				}
			}
		}

		if (*argv) {
			/* too many parameters */
			return -1;
		}

		rssi.rate_limit_msec = htod32(rssi.rate_limit_msec);
		ret = wlu_var_setbuf(wl, cmd->name, &rssi, sizeof(rssi));
	}
	return ret;
}

static int
wl_phy_rssi_ant(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	uint i;
	wl_rssi_ant_t *rssi_ant_p;

	if (!*++argv) {
		/* Get */
		void *ptr = NULL;

		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;

		rssi_ant_p = (wl_rssi_ant_t *)ptr;
		rssi_ant_p->version = dtoh32(rssi_ant_p->version);
		rssi_ant_p->count = dtoh32(rssi_ant_p->count);

		if (rssi_ant_p->count == 0) {
			printf("not supported on this chip\n");
		} else {
			for (i = 0; i < rssi_ant_p->count; i++)
				printf("rssi[%d] %d  ", i, rssi_ant_p->rssi_ant[i]);
			printf("\n");
		}
	} else {
		ret = USAGE_ERROR;
	}
	return ret;
}

/* Commands that take a MAC address */
static int
wl_mac(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	struct ether_addr ea;

	if (!*++argv) {
		if ((ret = wlu_get(wl, cmd->get, &ea, ETHER_ADDR_LEN)) < 0)
			return ret;
		printf("%s\n", wl_ether_etoa(&ea));
		return 0;
	} else {
		if (!wl_ether_atoe(*argv, &ea))
			return -1;
		return wlu_set(wl, cmd->set, &ea, ETHER_ADDR_LEN);
	}
}

/* IO variables that take a MAC address */
static int
wl_iov_mac(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	struct ether_addr ea = {{0, 0, 0, 0, 0, 0}};

	if (argv[1]) { /* set */
		if (!wl_ether_atoe(argv[1], &ea)) {
			printf(" ERROR: no valid ether addr provided\n");
			return -1;
		}
		if ((ret = wlu_iovar_set(wl, cmd->name, &ea, ETHER_ADDR_LEN)) < 0) {
			printf("Error setting variable %s\n", argv[0]);
			return ret;
		}
		return 0;
	} else { /* get */
		if ((ret = wlu_iovar_get(wl, cmd->name, &ea, ETHER_ADDR_LEN)) < 0) {
			printf("Error getting variable %s\n", argv[0]);
			return ret;
		}
		printf("%s %s\n", argv[0], wl_ether_etoa(&ea));
	}

	return 0;
}

static void
wl_txq_prec_dump(wl_iov_pktq_log_t* iov_pktq_log)
{
	switch (iov_pktq_log->version)
	{
		case 0x02:
		{
			const char* title[2] = {"", ""};
			const char* form[2]  = {"", ""};
			char* headings = "";

			uint8 index, prec;
			pktq_log_format_v02_t* logv02 = &iov_pktq_log->pktq_log.v02;

			char  marker[4] = "[X]";

			headings = &logv02->headings[0];

			/* long form */
			title[0] = "prec:   rqstd,  stored,selfsave,   saved,"
					   "fulldrop, dropped,sacrficd, retried, rtsfail,"
					   "rtrydrop, psretry, acked, utlisatn,q length,"
					   "Mbits/s\n";
			form[0]  = "  %02u: %7u, %7u, %7u, %7u, %7u, %7u,"
					   " %7u, %7u, %7u, %7u, %7u, %7u, %7u, %7u, % 8.2f\n";

			/* short form */
			title[1] = "prec:   rqstd,  stored, dropped, retried,"
					   "rtsfail,rtrydrop, psretry,   acked, utlisatn,"
					   "q length,  Mbits/s\n";
			form[1]  = "  %02u: %7u, %7u, %7u, %7u, %7u, %7u, %7u, %7u,"
					   "%7u, %7u, % 8.2f\n";

			for (index = 0; index < (uint8)iov_pktq_log->params.num_addrs; index++) {

				char* heading_start;
				char* heading_end;
				uint32 num_prec = 0;

				/* search for string marker - the marker is of the form
				   "[<index>]" where index is a single ascii numeral
				*/
				marker[1] = '0' + index;
				heading_start = strstr(headings, marker);

				/* The driver may pass back an optional character
				 * string for additional info
				 */
				if (heading_start != NULL) {

					heading_start += strlen(marker);
					heading_end = strstr(heading_start, marker);

					if (heading_end == NULL) {
						heading_end = heading_start + strlen(heading_start);
					}
					while (heading_start < heading_end) {
						fputc(*heading_start++, stdout);
					}
				}

				num_prec = logv02->num_prec[index];

				/* Note that this is zero if the data is invalid */
				if (num_prec) {

					/* check for short form or long form (top bit set) */
					fputs(iov_pktq_log->params.addr_type[index] & 0x80 ?
					      title[0] : title [1], stdout);

					for (prec = 0; prec < num_prec; prec++) {
						float tput = 0.0;
						pktq_log_counters_v02_t* counters = 0;

						counters = &logv02->counters[index][prec];

						if (logv02->time_delta != 0) {
							/* convert bytes to bits */
							tput = (float)logv02->
								   throughput[index][prec];
							tput *= 8.0;

							/* converts to rate of bits per us,
							   because time_delta is in micro-seconds
							*/
							tput /= (float) logv02->time_delta;
						}

						if (iov_pktq_log->params.addr_type[index] & 0x80) {
							/* long form */
							fprintf(stdout, form[0], prec,
							        counters->requested,
							        counters->stored,
							        counters->selfsaved,
							        counters->saved,
							        counters->full_dropped,
							        counters->dropped,
							        counters->sacrificed,
							        counters->retry,
							        counters->rtsfail,
							        counters->retry_drop,
							        counters->ps_retry,
							        counters->acked,
							        counters->max_used,
							        counters->queue_capacity,
							        tput);
						}
						else {
							/* short form */
							fprintf(stdout, form[1], prec,
							        counters->requested,
							        counters->stored,
							        counters->dropped,
							        counters->retry,
							        counters->rtsfail,
							        counters->retry_drop,
							        counters->ps_retry,
							        counters->acked,
							        counters->max_used,
							        counters->queue_capacity,
							        tput);
						}
					}
					fputs("\n", stdout);
				}
				else {
					fprintf(stdout, "Parameter %c:%s not valid\n",
					        iov_pktq_log->params.addr_type[index] != 0 ?
					        iov_pktq_log->params.addr_type[index] & 0x7F : ' ',
					        wl_ether_etoa(&iov_pktq_log->params.ea[index]));
				}
			}
			break;
		}
		default:
			fprintf(stderr, "Unknown/unsupported binary format (%x)\n",
			        iov_pktq_log->version);
			break;
	}
}

/* IO variables that take MAC addresses (with optional single letter prefix)
 * and output a string buffer
 */
static int
wl_iov_mac_params(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	char** macaddrs = argv + 1;

	wl_iov_mac_params_t*  params = (wl_iov_mac_params_t*)buf;

	if (cmd->get < 0)
		return -1;

	memset(params, 0, sizeof(*params));

	/* only pass up to WL_IOV_MAC_PARAM_LEN parameters */
	while (params->num_addrs < WL_IOV_MAC_PARAM_LEN && *macaddrs) {

		/* is there a prefix character? */
		if ((*macaddrs)[1] == ':') {
			params->addr_type[params->num_addrs] = toupper((int)((*macaddrs)[0]));

			/* move ptr to skip over prefix */
			*macaddrs = & ((*macaddrs)[2]);

			/* is there the 'long form' option ? */
			if ((*macaddrs)[0] == '+') {
				/* check for + additional info option, set top bit */
				params->addr_type[params->num_addrs]  |= 0x80;
				*macaddrs = & ((*macaddrs)[1]);
			}
		}

		/* the prefix C: denotes no given MAC address (to refer to "common") */
		if (((params->addr_type[params->num_addrs] & 0x7F) == 'C') ||
		      wl_ether_atoe(*macaddrs, &params->ea[params->num_addrs])) {
			params->num_addrs ++;
		}
		else {
			params->addr_type[params->num_addrs] = 0;
			printf("Bad parameter '%s'\n", *macaddrs);
		}

		++macaddrs;
	}

	while (*macaddrs) {
		printf("Ignoring excess parameter '%s' (maximum number of params is %d)\n",
		       *macaddrs, WL_IOV_MAC_PARAM_LEN);
		++macaddrs;
	}

	/* if no valid params found, pass default prefix 'C' with no mac address */
	if (params->num_addrs == 0)
	{
		params->addr_type[0] = 'C';
		params->num_addrs = 1;
	}

	if ((ret = wlu_iovar_getbuf(wl, cmd->name, params,
	                            sizeof(*params), buf, WLC_IOCTL_MAXLEN)) < 0) {
		fprintf(stderr, "Error getting variable %s\n", argv[0]);
		return ret;
	}

	wl_txq_prec_dump((wl_iov_pktq_log_t*)buf);

	return 0;
}

static int
wlu_dump(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	char *dump_buf;
	int bcmerr;

	if (cmd->get < 0)
		return -1;

	dump_buf = malloc(WL_DUMP_BUF_LEN);
	if (dump_buf == NULL) {
		fprintf(stderr, "Failed to allocate dump buffer of %d bytes\n", WL_DUMP_BUF_LEN);
		return -1;
	}
	memset(dump_buf, 0, WL_DUMP_BUF_LEN);

	/* skip the command name */
	argv++;

	/* If no args given, get the subset of 'wl dump all'
	 * Otherwise, if args are given, they are the dump section names.
	 */
	if (*argv == NULL) {
		/* query for the 'dump' without any argument */
		ret = wlu_iovar_getbuf(wl, "dump", NULL, 0, dump_buf, WL_DUMP_BUF_LEN);

		/* if the query is successful, continue on and print the result. */

		/* if the query fails, check for a legacy driver that does not support
		 * the "dump" iovar, and instead issue a WLC_DUMP ioctl.
		 */
		if (ret) {
			wlu_iovar_getint(wl, "bcmerror", &bcmerr);
			if (bcmerr == BCME_UNSUPPORTED) {
				ret = wlu_get(wl, WLC_DUMP, dump_buf, WL_DUMP_BUF_LEN);
				if (ret) {
					fprintf(stderr, "dump: error on query of WLC_DUMP\n");
					ret = IOCTL_ERROR;
				}
			} else {
				fprintf(stderr, "dump: error on query of dump list\n");
				ret = IOCTL_ERROR;
			}
		}
	} else {
		/* create the dump section name list */
		while (*argv) {
			/* add space delimiter if this is not the first section name */
			if (dump_buf[0] != '\0')
				strcat(dump_buf, " ");

			strcat(dump_buf, *argv);

			argv++;
		}

		/* This is a "space" added at end of last argument */
		strcat(dump_buf, " ");

		ret = wlu_iovar_getbuf(wl, "dump", dump_buf, strlen(dump_buf),
		                      dump_buf, WL_DUMP_BUF_LEN);
	}

	if (!ret) {
		fputs(dump_buf, stdout);
	}

	free(dump_buf);

	return ret;
}


static int
wlu_srdump(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i, nw, nb = 0;
	uint16 *words = (uint16 *)&buf[8];

	srom_rw_t   *srt;

	/* srom has been expanded a few times, at the moment sromrev11 are the largest */
	nw = SROM11_WORDS;

	/* allow reading a larger (or any other-size one) if specified */
	if (*++argv != NULL) {
		nb = (int)strtol(*argv, NULL, 0);
		if (nb & 1) {
			printf("Byte count %d is odd\n", nb);
			return -1;
		}
		nw = nb / 2;
	}

	srt = (srom_rw_t *)buf;
	srt->byteoff = htod32(0);
	srt->nbytes = htod32(2 * nw);

	if (cmd->get < 0)
		return -1;
	if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return ret;

	if (words[SROM11_SIGN] == SROM11_SIGNATURE) {
		nw = SROM11_WORDS;
	} else if (words[SROM10_SIGN] == SROM4_SIGNATURE) {
		nw = SROM10_WORDS;
	} else {
		nw = SROM4_WORDS;
		if ((words[SROM4_SIGN] != SROM4_SIGNATURE) &&
			(words[SROM8_SIGN] != SROM4_SIGNATURE))
			nw = nb ? nw : SROM_WORDS;
	}
	for (i = 0; i < nw; i++) {
		if ((i % 8) == 0)
			printf("\n  srom[%03d]:  ", i);
		printf("0x%04x  ", words[i]);
	}
	printf("\n");

	return 0;
}

static int
wlu_srwrite(void *wl, cmd_t *cmd, char **argv)
{
#if !defined(BWL_FILESYSTEM_SUPPORT)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return (-1);
#elif	defined(_CFE_)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return CFE_ERR_UNSUPPORTED;
#else
	char *arg;
	char *endptr;
	FILE *fp = NULL;
	int ret = 0, erase, srcrc;
	uint i, len;
	srom_rw_t *srt = (srom_rw_t *)buf;

	erase = !strcmp(*argv, "srclear");
	srcrc = !strcmp(*argv, "srcrc");

	/* We need at least one arg */
	if (!*++argv)
		return USAGE_ERROR;

	arg = *argv++;

	if (erase) {
		if (*argv)
			return USAGE_ERROR;
		len = strtoul(arg, &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "error parsing value \"%s\" as an integer for byte count\n",
			        arg);
			return BAD_PARAM;
		}
		srt->byteoff = 0x55aa;
	} else if (!*argv) {  /* srwrite or srcrc */
		/* Only one arg, it better be a file name */
		if (!(fp = fopen(arg, "rb"))) {
			fprintf(stderr, "%s: No such file or directory\n", arg);
			return -2;
		}

		len = fread(srt->buf, 1, SROM_MAX + 1, fp);
		if ((ret = ferror(fp))) {
			printf("\nerror %d reading %s\n", ret, arg);
			ret = -3;
			goto out;
		}

		if (!feof(fp)) {
			printf("\nFile %s is too large\n", arg);
			ret = -4;
			goto out;
		}

		if (len == SROM4_WORDS * 2) {
			if ((srt->buf[SROM4_SIGN] != SROM4_SIGNATURE) &&
			    (srt->buf[SROM8_SIGN] != SROM4_SIGNATURE)) {
				printf("\nFile %s is %d bytes but lacks a REV4/ signature\n",
				       arg, SROM4_WORDS * 2);
				ret = -5;
				goto out;
			}
		} else if (len == SROM11_WORDS * 2) {
			if (srt->buf[SROM11_SIGN] != SROM11_SIGNATURE) {
				printf("\nFile %s is %d bytes but lacks a REV11/ signature\n",
				       arg, SROM11_WORDS * 2);
				ret = -5;
				goto out;
			}
		} else if ((len != SROM_WORDS * 2) && (len != SROM10_WORDS * 2) &&
			(len != SROM_MAX)) {
			printf("\nFile %s is %d bytes, not %d or %d or %d or %d bytes\n", arg, len,
				SROM_WORDS * 2, SROM4_WORDS * 2, SROM10_WORDS, SROM_MAX);
			ret = -6;
			goto out;
		}

		srt->byteoff = 0;
	} else {
		if (srcrc) {
			printf("srcrc only takes one arg\n");
			ret = BCME_BADARG;
			goto out;
		}

		/* More than 1 arg, first is offset, rest are data. */
		srt->byteoff = strtoul(arg, &endptr, 0);
		if (*endptr != '\0')
			goto nout;

		i = 0;
		while ((arg = *argv++) != NULL) {
			srt->buf[i++] = (uint16)strtoul(arg, &endptr, 0);
			if (*endptr != '\0') {
nout:
				printf("\n%s is not an integer\n", arg);
				ret = -7;
				goto out;
			}
		}

		if (srt->byteoff & 1) {
			printf("Byte offset (%d) is odd or negative\n", srt->byteoff);
			ret = -8;
			goto out;
		}

		len = 2 * i;
		if ((srt->byteoff + len) > SROM_MAX) {
			printf("Data extends past %d bytes\n", SROM_MAX);
			ret = -9;
			goto out;
		}
	}
	srt->nbytes = len;

	if (srcrc) {
		srt->byteoff = 0x55ab;	/* Hack for srcrc */
		ret = wlu_get(wl, cmd->get, buf, len + 8);
		printf("0x%x\n", (uint8)buf[0]);
	} else {
		printf("Writing srom. ioctl %d, iolen %d, sroff %d, len %d\n",
		        cmd->set, len + 8, srt->byteoff, srt->nbytes);

		ret = wlu_set(wl, cmd->set, buf, len + 8);
	}

out:
	fflush(stdout);
	if (fp)
		fclose(fp);
	return ret;
#endif   /* BWL_FILESYSTEM_SUPPORT */
}

static int
wlu_ciswrite(void *wl, cmd_t *cmd, char **argv)
{
#if !defined(BWL_FILESYSTEM_SUPPORT)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return (-1);
#elif	defined(_CFE_)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return CFE_ERR_UNSUPPORTED;
#else
	char *arg, *bufp;
	FILE *fp = NULL;
	int ret = 0;
	uint32 len;

	cis_rw_t cish;
	char *cisp, *cisdata;

	UNUSED_PARAMETER(cmd);

	/* We need extacly one arg */
	if (!*++argv || argv[1])
		return -1;

	/* initialize buffer with iovar */
	bufp = buf;
	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(bufp, "ciswrite");
	bufp += strlen("ciswrite") + 1;
	cisp = bufp;
	cisdata = cisp + sizeof(cish);

	cish.source = htod32(0);

	/* grab the filename arg */
	arg = *argv;
	if (!(fp = fopen(arg, "rb"))) {
		fprintf(stderr, "%s: No such file or directory\n", arg);
		return -2;
	}

	len = fread(cisdata, 1, SROM_MAX + 1, fp);
	if ((ret = ferror(fp))) {
		printf("\nerror %d reading %s\n", ret, arg);
		ret = -3;
		goto out;
	}

	if (!feof(fp)) {
		printf("\nFile %s is too large\n", arg);
		ret = -4;
		goto out;
	}

	/* fill in offset and length */
	cish.byteoff = htod32(0);
	cish.nbytes = htod32(len);
	memcpy(cisp, (char*)&cish, sizeof(cish));

	printf("len %d sizeof(cish) %d total %d\n", len, (int)sizeof(cish),
	       (int)(len + sizeof(cish)));
	ret = wl_set(wl, WLC_SET_VAR, buf, (cisp - buf) + sizeof(cish) + len);
	if (ret < 0) {
		fprintf(stderr, "ciswrite failed: %d\n", ret);
	}

out:
	if (fp)
		fclose(fp);

	return ret;
#endif   /* BWL_FILESYSTEM_SUPPORT */
}

static int
wlu_cisupdate(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#else
	char *bufp, *endptr;
	int ret = 0;
	int preview = 0;
	uint32 off;
	uint32 len;
	uint32 updatelen;
	uint32 i;
	char hexstr[3];
	char bytes[SROM_MAX];

	cis_rw_t cish;
	char *cisp;

	UNUSED_PARAMETER(cmd);

	/* validate arg count */
	if (!*++argv || !argv[1])
		return -1;

	if (argv[2] && !strcmp(argv[2], "--preview"))
		preview = 1;

	/* grab byte offset */
	off = (uint32)strtol(argv[0], &endptr, 0);
	if (*endptr != '\0')
		return -1;


	bufp = argv[1];
	updatelen = strlen(bufp);
	if (updatelen % 2) {
		fprintf(stderr, "cisupdate hex string must contain an even number of digits\n");
		goto done;
	}
	updatelen /= 2;

	/* convert and store hex byte values */
	for (i = 0; i < updatelen; i++) {
		hexstr[0] = *bufp;
		hexstr[1] = *(bufp + 1);
		if (!isxdigit((int)hexstr[0]) || !isxdigit((int)hexstr[1])) {
			fprintf(stderr, "cisupdate invalid hex digit(s) in %s\n", argv[1]);
			goto done;
		}
		hexstr[2] = '\0';
		bytes[i] = (char) strtol(hexstr, NULL, 16);
		bufp += 2;
	}

	/* Prepare the read info */
	cish.source = 0;
	cish.byteoff = 0;
	cish.nbytes = 0;

	/* set up the buffer and do the get (+9 allows space for "ciswrite" string later) */
	memset(buf + 9, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf + 9, "cisdump");
	bufp = buf + strlen("cisdump") + 1 + 9;
	memcpy(bufp, (char*)&cish, sizeof(cish));
	bufp += sizeof(cish);
	ret = wl_get(wl, WLC_GET_VAR, buf + 9, (bufp - (buf + 9)) + SROM_MAX);
	if (ret < 0) {
		fprintf(stderr, "cisupdate failed to read cis: %d\n", ret);
		goto done;
	}

	/* pull off the cis_rw_t */
	bufp = buf + 9;
	memcpy((char*)&cish, bufp, sizeof(cish));
	len = dtoh32(cish.nbytes);

	if ((off + updatelen) > len) {
		fprintf(stderr, "cisupdate offset %d plus update len %d exceeds CIS len %d\n",
		        off, updatelen, len);
		goto done;
	}

	/* move past to the data */
	bufp += sizeof(cish);

	/* update the bytes */
	if (cish.source == WLC_CIS_SROM) {
		for (i = 0; i < updatelen; ++i)
			bufp[off + i] = bytes[i] & 0xff;
	} else {
		for (i = 0; i < updatelen; ++i) {
			if (~bytes[i] & bufp[off + i]) {
				fprintf(stderr, "cisupdate: OTP update incompatible:"
				        " update[%d](0x%02x)->cis[%d](0x%02x)\n",
				        i,  bytes[i], off + i, bufp[off + i]);
				goto done;
			}
			bufp[off + i] |= bytes[i];
		}
	}

	/* initialize buffer with iovar */
	bufp = buf;
	strcpy(bufp, "ciswrite");
	bufp += strlen("ciswrite") + 1;
	cisp = bufp;

	/* fill in cis_rw_t fields */
	cish.source = 0;
	cish.byteoff = 0;
	cish.nbytes = htod32(len);
	memcpy(cisp, (char*)&cish, sizeof(cish));

	/* write the data back to the device */
	printf("offset %d data %s cislen %d\n", off, argv[1], len);
	if (preview) {
		bufp += sizeof(cish);
		for (i = 0; i < len; i++) {
			if ((i % 8) == 0)
				printf("\nByte %3d: ", i);
			printf("0x%02x ", (uint8)bufp[i]);
		}
		printf("\n");
	} else {
		ret = wl_set(wl, WLC_SET_VAR, buf, (cisp - buf) + sizeof(cish) + len);
		if (ret < 0) {
			fprintf(stderr, "cisupdate cis write failed: %d\n", ret);
		}
	}

done:
	return ret;
#endif /* _CFE_ */
}

static int
wlu_cisdump(void *wl, cmd_t *cmd, char **argv)
{
	char *bufp;
	int i, ret = 0;
	cis_rw_t cish;
	uint nbytes = 0;
	char *fname = NULL;

	UNUSED_PARAMETER(cmd);

	/* Grab and move past optional output file argument */
	if ((argv[1] != NULL) && (strcmp(argv[1], "-b") == 0)) {
		fname = argv[2];
		argv += 2;
	}

	/* check for a length argument */
	if (*++argv != NULL) {
		nbytes = (int)strtol(*argv, NULL, 0);
		if (nbytes & 1) {
			printf("Invalid byte count %d, must be even\n", nbytes);
			ret = -1;
			goto done;
		}
		if (nbytes > SROM_MAX) {
			printf("Count %d too large\n", nbytes);
			ret = -1;
			goto done;
		}
	}

	/* Prepare the read info */
	cish.source = 0;
	cish.byteoff = 0;
	cish.nbytes = htod32(nbytes);

	/* set up the buffer and do the get */
	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, "cisdump");
	bufp = buf + strlen("cisdump") + 1;
	memcpy(bufp, (char*)&cish, sizeof(cish));
	bufp += sizeof(cish);
	ret = wl_get(wl, WLC_GET_VAR, buf, (bufp - buf) + (nbytes ? nbytes : SROM_MAX));
	if (ret < 0) {
		fprintf(stderr, "Failed cisdump request: %d\n", ret);
		goto done;
	}

	/* pull off the cis_rw_t */
	bufp = buf;
	memcpy((char*)&cish, bufp, sizeof(cish));
	cish.source = dtoh32(cish.source);
	cish.byteoff = dtoh32(cish.byteoff);
	cish.nbytes = dtoh32(cish.nbytes);

	/* move past to the data */
	bufp += sizeof(cish);

	printf("Source: %d (%s)", cish.source,
	       (cish.source == WLC_CIS_DEFAULT) ? "Built-in default" :
	       (cish.source == WLC_CIS_SROM) ? "External SPROM" :
	       (cish.source == WLC_CIS_OTP) ? "Internal OTP" : "Unknown?");
	if (!nbytes)
		printf("\nMaximum length: %d bytes", cish.nbytes);
	for (i = 0; i < (int)cish.nbytes; i++) {
		if ((i % 8) == 0)
			printf("\nByte %3d: ", i);
		printf("0x%02x ", (uint8)bufp[i]);
	}
	printf("\n");

#if defined(BWL_FILESYSTEM_SUPPORT)
#if !defined(_CFE_)
	if (fname != NULL) {
		FILE *fp;

		if (!nbytes)
			nbytes = cish.nbytes;

		fp = fopen(fname, "wb");
		if (fp != NULL) {
			ret = fwrite(bufp, 1, nbytes, fp);
			if (ret != (int)nbytes) {
				fprintf(stderr, "Error writing %d bytes to file, rc %d!\n",
				        (int)nbytes, ret);
				ret = -1;
			} else {
				printf("Wrote %d bytes to %s\n", ret, fname);
				ret = 0;
			}
			fclose(fp);
		} else {
			fprintf(stderr, "Problem opening file %s\n", fname);
			ret = -1;
		}
	}
#endif /* !(CFE|HNDRTE|IOPOS) -- has stdio filesystem */
#endif   /* BWL_FILESYSTEM_SUPPORT */

done:
	return ret;
}

#if	defined(linux) || defined(MACOSX) || defined(_CFE_) || defined(__NetBSD__)
/* linux, MacOS, NetBSD: ffs is in the standard C library */
/* CFE, HNDRTE & IOPOS: Not needed, the code below is ifdef out */
#else
static int
ffs(int i)
{
	int j;

	if (i != 0)
		for (j = 0; j < 32; j++)
			if (i & (1 << j))
				return j + 1;
	return 0;
}
#endif	/* linux, MACOSX, CFE, HNDRTE, IOPOS, NetBSD */

#if	!defined(_CFE_)

/* VX wants prototypes even for static functions. */
static char* find_pattern(char **argv, const char *pattern, uint *val);
static char* find_pattern2(char **argv, const char *pattern, uint *val, int vnum);
static int newtuple(char *b, int *cnt, uint8 tag, const cis_tuple_t *srv);
static int parsecis(char *b, char **argv, int sromrev);
static const sromvar_t *srvlookup(const sromvar_t *tab, char *name, int nlen, int sromrev);

/* Find an entry in argv[][] in this form
 *	name=value, could be pattern=(0x)1234 or pattern=ABC
 *
 * If *val is NULL, return the pointer to value.
 * If *val is not NULL, fill the value into val, return the pointer to name if found,
 * return NULL if no match found.
 */
static char*
find_pattern(char **argv, const char *pattern, uint *val)
{
	char *ret = NULL, *name = NULL, **pargv = argv;

	/* clear val first */
	if (val)	*val = 0;

	while ((name = *pargv++)) {
		if ((ret = strstr(name, pattern))) {
			char *p = ret, *q = NULL;

			/* Extracting the content */
			p += strlen(pattern);

			/* var name could have same prefix */
			if (*p++ != '=') {
				ret = NULL;
				continue;
			}
			if (!val)	return (ret+strlen(pattern)+1);

			*val = strtoul(p, &q, 0);
			if (p == q) {
				printf("Bad value: %s\n", ret);
				return NULL;
			}

			break;
		}
	}
	return ret;
}

/* Find an entry in argv[][] in this form
 *	name=value1,value2,...,value(n)
 *		n is indicated by vnum
 * 		could be pattern=(0x)1234,... or pattern=ABC,...
 *
 * If *val is NULL, return the pointer to value.
 * If *val is not NULL, fill the value into val, return the pointer to name if found,
 * return NULL if no match found.
 */
static char*
find_pattern2(char **argv, const char *pattern, uint *val, int vnum)
{
	char *ret = NULL, *name = NULL, **pargv = argv;
	int i;

	while ((name = *pargv++)) {
		if ((ret = strstr(name, pattern))) {
			char *p = ret, *q = NULL;

			/* Extracting the content */
			p += strlen(pattern);

			/* var name could have same prefix */
			if (*p++ != '=') {
				ret = NULL;
				continue;
			}
			if (!val)	return (ret+strlen(pattern)+1);

			for (i = 0; i < vnum; i ++)
			{
				val[i] = strtoul(p, &q, 0);

				if (p == q) {
					printf("Bad value: %s\n", ret);
					return NULL;
				}
				p = q + 1;	/* skip ',' */
			}
			break;
		}
	}
	return ret;
}

static int
newtuple(char *b, int *cnt, uint8 tag, const cis_tuple_t *srv)
{
	memset(b, 0, srv->len + 2);

	b[0] = tag;
	b[1] = (char)srv->len;
	b[2] = (char)srv->tag;

	if (cnt)
		*cnt += 3;
	return 0;
}

static int
parsecis(char *b, char **argv, int sromrev)
{
	const cis_tuple_t *srv = cis_hnbuvars;
	char	*cpar = NULL, *p = NULL;
	char	*par;
	char	delimit[2] = " \0";
	int	cnt = 0, i = 0;
	uint 	sromrev_mask = 0xffffffff;

	if (sromrev > 0 && sromrev <= 31) {
		sromrev_mask = 1 << sromrev;
	} else {
		printf("Invalid sromrev %d.\n", sromrev);
		return BCME_BADARG;
	}

	/* Walk through all the tuples, create append buffer */
	while (srv->tag != 0xFF) {
		uint val = 0;

		/* Skip srv if not supported in sromrev */
		if (!(sromrev_mask & srv->revmask)) {
			srv++;
			continue;
		}

		/* Special cases (Raw Data / macaddr / ccode / fem) */
		if (srv->tag == OTP_RAW || srv->tag == OTP_RAW1) {
			if ((p = find_pattern(argv, "RAW", &val))) {
				p += (strlen("RAW") + 1);	/* RAW= */
				for (;;) {
					b[cnt++] = (unsigned char) strtoul(p, &p, 16);
					if (!*p++)
						break;
				}
			}
		} else if (srv->tag == OTP_VERS_1) {
			uint l1 = 1, l2 = 1;
			char *p2 = NULL;

			if ((p = find_pattern(argv, "manf", NULL)))
				l1 += strlen(p);

			if ((p2 = find_pattern(argv, "productname", NULL)))
				l2 += strlen(p2);

			if ((p != NULL) | (p2 != NULL)) {
				b[cnt++] = CISTPL_VERS_1;
				b[cnt++] = 2 + l1 + l2;
				b[cnt++] = 8;
				b[cnt++] = 0;
				if (p) {
					char *q = p;
					/* Replace '_' by space */
					while ((q = strchr(q, '_')))
						*q = ' ';
					memcpy(&b[cnt], p, l1);
				} else
					b[cnt] = '\0';
				cnt += l1;

				if (p2) {
					char *q = p2;
					/* Replace '_' by space */
					while ((q = strchr(q, '_')))
						*q = ' ';
					memcpy(&b[cnt], p2, l2);
				} else
					b[cnt] = '\0';
				cnt += l2;
			}
		} else if (srv->tag == OTP_MANFID) {
			bool found = FALSE;
			uint manfid = 0, prodid = 0;

			if ((p = find_pattern(argv, "manfid", &manfid)))
				found = TRUE;

			if ((p = find_pattern(argv, "prodid", &prodid)))
				found = TRUE;

			if (found) {
				b[cnt++] = CISTPL_MANFID;
				b[cnt++] = srv->len;
				b[cnt++] = (uint8)(manfid & 0xff);
				b[cnt++] = (uint8)((manfid >> 8) & 0xff);
				b[cnt++] = (uint8)(prodid & 0xff);
				b[cnt++] = (uint8)((prodid >> 8) & 0xff);
			}
		} else if (srv->tag == HNBU_MACADDR) {
			if ((p = find_pattern(argv, "macaddr", NULL))) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				if (!wl_ether_atoe(p, (struct ether_addr*)&b[cnt]))
					printf("Argument does not look like a MAC "
					"address: %s\n", p);
				cnt += sizeof(struct ether_addr);
			}
		} else if (srv->tag == HNBU_CCODE) {
			bool found = FALSE;
			char tmp[3] = "\0\0\0";

			if ((p = find_pattern(argv, "ccode", NULL))) {
				found = TRUE;
				tmp[0] = *p++;
				tmp[1] = *p++;
			}
			if ((p = find_pattern(argv, "cctl", &val))) {
				found = TRUE;
				tmp[2] = (uint8)val;
			}
			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				memcpy(&b[cnt], tmp, 3);
				cnt += 3;	/* contents filled already */
			}
		} else if (srv->tag == HNBU_RSSISMBXA2G) {
			bool found = FALSE;
			char tmp[2] = "\0\0";

			if ((p = find_pattern(argv, "rssismf2g", &val))) {
				found = TRUE;
				tmp[0] |= val & 0xf;
			}
			if ((p = find_pattern(argv, "rssismc2g", &val))) {
				found = TRUE;
				tmp[0] |= (val & 0xf) << 4;
			}
			if ((p = find_pattern(argv, "rssisav2g", &val))) {
				found = TRUE;
				tmp[1] |= val & 0x7;
			}
			if ((p = find_pattern(argv, "bxa2g", &val))) {
				found = TRUE;
				tmp[1] |= (val & 0x3) << 3;
			}
			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				memcpy(&b[cnt], tmp, 2);
				cnt += 2;	/* contents filled already */
			}
		} else if (srv->tag == HNBU_RSSISMBXA5G) {
			bool found = FALSE;
			char tmp[2] = "\0\0";

			if ((p = find_pattern(argv, "rssismf5g", &val))) {
				found = TRUE;
				tmp[0] |= val & 0xf;
			}
			if ((p = find_pattern(argv, "rssismc5g", &val))) {
				found = TRUE;
				tmp[0] |= (val & 0xf) << 4;
			}
			if ((p = find_pattern(argv, "rssisav5g", &val))) {
				found = TRUE;
				tmp[1] |= val & 0x7;
			}
			if ((p = find_pattern(argv, "bxa5g", &val))) {
				found = TRUE;
				tmp[1] |= (val & 0x3) << 3;
			}
			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				memcpy(&b[cnt], tmp, 2);
				cnt += 2;	/* contents filled already */
			}
		} else if (srv->tag == HNBU_FEM) {
			bool	found = FALSE;
			uint16	tmp2g = 0, tmp5g = 0;

			if ((p = find_pattern(argv, "antswctl2g", &val))) {
				found = TRUE;
				tmp2g |= ((val << SROM8_FEM_ANTSWLUT_SHIFT) &
					SROM8_FEM_ANTSWLUT_MASK);
			}
			if ((p = find_pattern(argv, "triso2g", &val))) {
				found = TRUE;
				tmp2g |= ((val << SROM8_FEM_TR_ISO_SHIFT) &
					SROM8_FEM_TR_ISO_MASK);
			}
			if ((p = find_pattern(argv, "pdetrange2g", &val))) {
				found = TRUE;
				tmp2g |= ((val << SROM8_FEM_PDET_RANGE_SHIFT) &
					SROM8_FEM_PDET_RANGE_MASK);
			}
			if ((p = find_pattern(argv, "extpagain2g", &val))) {
				found = TRUE;
				tmp2g |= ((val << SROM8_FEM_EXTPA_GAIN_SHIFT) &
					SROM8_FEM_EXTPA_GAIN_MASK);
			}
			if ((p = find_pattern(argv, "tssipos2g", &val))) {
				found = TRUE;
				tmp2g |= ((val << SROM8_FEM_TSSIPOS_SHIFT) &
					SROM8_FEM_TSSIPOS_MASK);
			}
			if ((p = find_pattern(argv, "antswctl5g", &val))) {
				found = TRUE;
				tmp5g |= ((val << SROM8_FEM_ANTSWLUT_SHIFT) &
					SROM8_FEM_ANTSWLUT_MASK);
			}
			if ((p = find_pattern(argv, "triso5g", &val))) {
				found = TRUE;
				tmp5g |= ((val << SROM8_FEM_TR_ISO_SHIFT) &
					SROM8_FEM_TR_ISO_MASK);
			}
			if ((p = find_pattern(argv, "pdetrange5g", &val))) {
				found = TRUE;
				tmp5g |= ((val << SROM8_FEM_PDET_RANGE_SHIFT) &
					SROM8_FEM_PDET_RANGE_MASK);
			}
			if ((p = find_pattern(argv, "extpagain5g", &val))) {
				found = TRUE;
				tmp5g |= ((val << SROM8_FEM_EXTPA_GAIN_SHIFT) &
					SROM8_FEM_EXTPA_GAIN_MASK);
			}
			if ((p = find_pattern(argv, "tssipos5g", &val))) {
				found = TRUE;
				tmp5g |= ((val << SROM8_FEM_TSSIPOS_SHIFT) &
					SROM8_FEM_TSSIPOS_MASK);
			}

			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				b[cnt++] = (uint8)(tmp2g & 0xff);
				b[cnt++] = (uint8)((tmp2g >> 8) & 0xff);
				b[cnt++] = (uint8)(tmp5g & 0xff);
				b[cnt++] = (uint8)((tmp5g >> 8) & 0xff);
			}
		} else if (srv->tag == HNBU_UUID) {

			char *uuidstr = NULL;
			char nibble[3] = {0, 0, 0};

			if ((uuidstr = find_pattern(argv, "uuid", NULL)) != NULL) {

				/* uuid format 12345678-1234-5678-1234-567812345678 */

				if (strlen(uuidstr) == 36) {
					newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
					while (*uuidstr != '\0') {
						if (*uuidstr == '-') {
							uuidstr++;
							continue;
						}
						nibble[0] = *uuidstr++;
						nibble[1] = *uuidstr++;
						b[cnt ++] = (char)strtoul(nibble, NULL, 16);
					}
				}
			}

		} else if (srv->tag == HNBU_TEMPTHRESH) {
			bool found = FALSE;
			char tmp[6] = "\0\0\0\0\0\0";

			if ((p = find_pattern(argv, "tempthresh", &val))) {
				found = TRUE;
				tmp[0] = val;
			}
			if ((p = find_pattern(argv, "temps_period", &val))) {
				found = TRUE;
				tmp[1] |= ((val << SROM11_TEMPS_PERIOD_SHIFT) &
					SROM11_TEMPS_PERIOD_MASK);
			}
			if ((p = find_pattern(argv, "temps_hysteresis", &val))) {
				found = TRUE;
				tmp[1] |= ((val << SROM11_TEMPS_HYSTERESIS_SHIFT) &
					SROM11_TEMPS_HYSTERESIS_MASK);
			}
			if ((p = find_pattern(argv, "tempoffset", &val))) {
				found = TRUE;
				tmp[2] = val;
			}
			if ((p = find_pattern(argv, "tempsense_slope", &val))) {
				found = TRUE;
				tmp[3] = val;
			}
			if ((p = find_pattern(argv, "tempcorrx", &val))) {
				found = TRUE;
				tmp[4] |= ((val << SROM11_TEMPCORRX_SHIFT) &
					SROM11_TEMPCORRX_MASK);
			}
			if ((p = find_pattern(argv, "tempsense_option", &val))) {
				found = TRUE;
				tmp[4] |= ((val << SROM11_TEMPSENSE_OPTION_SHIFT) &
					SROM11_TEMPSENSE_OPTION_MASK);
			}
			if ((p = find_pattern(argv, "phycal_tempdelta", &val))) {
				found = TRUE;
				tmp[5] = val;
			}

			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				memcpy(&b[cnt], tmp, 6);
				cnt += 6;	/* contents filled already */
			}
		} else if (srv->tag == HNBU_FEM_CFG) {
			bool found = FALSE;
			uint16	fem_cfg1 = 0, fem_cfg2 = 0;

			if ((p = find_pattern(argv, "femctrl", &val))) {
				found = TRUE;
				fem_cfg1 |= ((val << SROM11_FEMCTRL_SHIFT) &
					SROM11_FEMCTRL_MASK);
			}
			if ((p = find_pattern(argv, "papdcap2g", &val))) {
				found = TRUE;
				fem_cfg1 |= ((val << SROM11_PAPDCAP_SHIFT) &
					SROM11_PAPDCAP_MASK);
			}
			if ((p = find_pattern(argv, "tworangetssi2g", &val))) {
				found = TRUE;
				fem_cfg1 |= ((val << SROM11_TWORANGETSSI_SHIFT) &
					SROM11_TWORANGETSSI_MASK);
			}
			if ((p = find_pattern(argv, "pdgain2g", &val))) {
				found = TRUE;
				fem_cfg1 |= ((val << SROM11_PDGAIN_SHIFT) &
					SROM11_PDGAIN_MASK);
			}
			if ((p = find_pattern(argv, "epagain2g", &val))) {
				found = TRUE;
				fem_cfg1 |= ((val << SROM11_EPAGAIN_SHIFT) &
					SROM11_EPAGAIN_MASK);
			}
			if ((p = find_pattern(argv, "tssiposslope2g", &val))) {
				found = TRUE;
				fem_cfg1 |= ((val << SROM11_TSSIPOSSLOPE_SHIFT) &
					SROM11_TSSIPOSSLOPE_MASK);
			}
			if ((p = find_pattern(argv, "gainctrlsph", &val))) {
				found = TRUE;
				fem_cfg2 |= ((val << SROM11_GAINCTRLSPH_SHIFT) &
					SROM11_GAINCTRLSPH_MASK);
			}
			if ((p = find_pattern(argv, "papdcap5g", &val))) {
				found = TRUE;
				fem_cfg2 |= ((val << SROM11_PAPDCAP_SHIFT) &
					SROM11_PAPDCAP_MASK);
			}
			if ((p = find_pattern(argv, "tworangetssi5g", &val))) {
				found = TRUE;
				fem_cfg2 |= ((val << SROM11_TWORANGETSSI_SHIFT) &
					SROM11_TWORANGETSSI_MASK);
			}
			if ((p = find_pattern(argv, "pdgain5g", &val))) {
				found = TRUE;
				fem_cfg2 |= ((val << SROM11_PDGAIN_SHIFT) &
					SROM11_PDGAIN_MASK);
			}
			if ((p = find_pattern(argv, "epagain5g", &val))) {
				found = TRUE;
				fem_cfg2 |= ((val << SROM11_EPAGAIN_SHIFT) &
					SROM11_EPAGAIN_MASK);
			}
			if ((p = find_pattern(argv, "tssiposslope5g", &val))) {
				found = TRUE;
				fem_cfg2 |= ((val << SROM11_TSSIPOSSLOPE_SHIFT) &
					SROM11_TSSIPOSSLOPE_MASK);
			}

			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				b[cnt++] = (uint8)(fem_cfg1 & 0xff);
				b[cnt++] = (uint8)((fem_cfg1 >> 8) & 0xff);
				b[cnt++] = (uint8)(fem_cfg2 & 0xff);
				b[cnt++] = (uint8)((fem_cfg2 >> 8) & 0xff);
			}
		} else if (srv->tag == HNBU_ACRXGAINS_C0) {
			bool found = FALSE;
			uint16	rxgains = 0, rxgains1 = 0;

			if ((p = find_pattern(argv, "rxgains5gtrelnabypa0", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gtrisoa0", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GTRISOA_SHIFT) &
					SROM11_RXGAINS5GTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gelnagaina0", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GELNAGAINA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gtrelnabypa0", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GTRELNABYPA_SHIFT) &
					SROM11_RXGAINS2GTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gtrisoa0", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GTRISOA_SHIFT) &
					SROM11_RXGAINS2GTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gelnagaina0", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GELNAGAINA_SHIFT) &
					SROM11_RXGAINS2GELNAGAINA_MASK);
			}

			if ((p = find_pattern(argv, "rxgains5ghtrelnabypa0", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GHTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5ghtrisoa0", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHTRISOA_SHIFT) &
					SROM11_RXGAINS5GHTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5ghelnagaina0", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GHELNAGAINA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmtrelnabypa0", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GMTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmtrisoa0", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMTRISOA_SHIFT) &
					SROM11_RXGAINS5GMTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmelnagaina0", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GMELNAGAINA_MASK);
			}

			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				b[cnt++] = (uint8)(rxgains & 0xff);
				b[cnt++] = (uint8)((rxgains >> 8) & 0xff);
				b[cnt++] = (uint8)(rxgains1 & 0xff);
				b[cnt++] = (uint8)((rxgains1 >> 8) & 0xff);
			}
		} else if (srv->tag == HNBU_ACRXGAINS_C1) {
			bool found = FALSE;
			uint16	rxgains = 0, rxgains1 = 0;

			if ((p = find_pattern(argv, "rxgains5gtrelnabypa1", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gtrisoa1", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GTRISOA_SHIFT) &
					SROM11_RXGAINS5GTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gelnagaina1", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GELNAGAINA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gtrelnabypa1", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GTRELNABYPA_SHIFT) &
					SROM11_RXGAINS2GTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gtrisoa1", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GTRISOA_SHIFT) &
					SROM11_RXGAINS2GTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gelnagaina1", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GELNAGAINA_SHIFT) &
					SROM11_RXGAINS2GELNAGAINA_MASK);
			}

			if ((p = find_pattern(argv, "rxgains5ghtrelnabypa1", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GHTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5ghtrisoa1", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHTRISOA_SHIFT) &
					SROM11_RXGAINS5GHTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5ghelnagaina1", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GHELNAGAINA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmtrelnabypa1", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GMTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmtrisoa1", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMTRISOA_SHIFT) &
					SROM11_RXGAINS5GMTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmelnagaina1", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GMELNAGAINA_MASK);
			}

			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				b[cnt++] = (uint8)(rxgains & 0xff);
				b[cnt++] = (uint8)((rxgains >> 8) & 0xff);
				b[cnt++] = (uint8)(rxgains1 & 0xff);
				b[cnt++] = (uint8)((rxgains1 >> 8) & 0xff);
			}
		} else if (srv->tag == HNBU_ACRXGAINS_C2) {
			bool found = FALSE;
			uint16	rxgains = 0, rxgains1 = 0;

			if ((p = find_pattern(argv, "rxgains5gtrelnabypa2", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gtrisoa2", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GTRISOA_SHIFT) &
					SROM11_RXGAINS5GTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gelnagaina2", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GELNAGAINA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gtrelnabypa2", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GTRELNABYPA_SHIFT) &
					SROM11_RXGAINS2GTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gtrisoa2", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GTRISOA_SHIFT) &
					SROM11_RXGAINS2GTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gelnagaina2", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GELNAGAINA_SHIFT) &
					SROM11_RXGAINS2GELNAGAINA_MASK);
			}

			if ((p = find_pattern(argv, "rxgains5ghtrelnabypa2", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GHTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5ghtrisoa2", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHTRISOA_SHIFT) &
					SROM11_RXGAINS5GHTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5ghelnagaina2", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GHELNAGAINA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmtrelnabypa2", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GMTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmtrisoa2", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMTRISOA_SHIFT) &
					SROM11_RXGAINS5GMTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmelnagaina2", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GMELNAGAINA_MASK);
			}

			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				b[cnt++] = (uint8)(rxgains & 0xff);
				b[cnt++] = (uint8)((rxgains >> 8) & 0xff);
				b[cnt++] = (uint8)(rxgains1 & 0xff);
				b[cnt++] = (uint8)((rxgains1 >> 8) & 0xff);
			}
		} else if (srv->tag == HNBU_PDOFF_2G) {
			bool	found = FALSE;
			uint16	tmppdoff2g = 0;

			if ((p = find_pattern(argv, "pdoffset2g40ma0", &val))) {
				found = TRUE;
				tmppdoff2g |= ((val << SROM11_PDOFF_2G_40M_A0_SHIFT) &
					SROM11_PDOFF_2G_40M_A0_MASK);
			}

			if ((p = find_pattern(argv, "pdoffset2g40ma1", &val))) {
				found = TRUE;
				tmppdoff2g |= ((val << SROM11_PDOFF_2G_40M_A1_SHIFT) &
					SROM11_PDOFF_2G_40M_A1_MASK);
			}

			if ((p = find_pattern(argv, "pdoffset2g40ma2", &val))) {
				found = TRUE;
				tmppdoff2g |= ((val << SROM11_PDOFF_2G_40M_A2_SHIFT) &
					SROM11_PDOFF_2G_40M_A2_MASK);
			}

			if ((p = find_pattern(argv, "pdoffset2g40mvalid", &val))) {
				found = TRUE;
				tmppdoff2g |= ((val << SROM11_PDOFF_2G_40M_VALID_SHIFT) &
					SROM11_PDOFF_2G_40M_VALID_MASK);
			}

			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				b[cnt++] = (uint8)(tmppdoff2g & 0xff);
				b[cnt++] = (uint8)((tmppdoff2g >> 8) & 0xff);
			}
		} else {	/* All other tuples */
			int	found = FALSE, varlen = 0;
			char	*cur = &b[cnt];
			uint	newtp = TRUE;

			par = malloc(strlen(srv->params)+1);
			if (!par)
				return BCME_NOMEM;

			/* Walk through each parameters in one tuple */
			strcpy(par, srv->params);

			cpar = strtok (par, delimit);	/* current param */
			while (cpar) {
				int	array_sz = 1;
				val = 0;

				/* Fill the CIS tuple to b but don't commit cnt yet */
				if (newtp) {
					newtuple(cur, NULL, CISTPL_BRCM_HNBU, srv);
					cur += 3;
					newtp = FALSE;
				}

				/* the first byte of each parameter indicates its length */
				varlen = (*cpar++) - '0';

				/* parse array size if any */
				if (*cpar == '*') {
					array_sz = 0;
					while (((*++cpar) >= '0') && (*cpar <= '9'))
						array_sz = (array_sz * 10) + *cpar - '0';
				}

				/* Find the parameter in the input argument list */
				if ((p = find_pattern(argv, cpar, &val)))
					found = TRUE;
				else
					val = 0;

				while (found && array_sz--) {
					*cur++ = (uint8)(val & 0xff);
					if (varlen >= 2)
						*cur++ = (uint8)((val >> 8) & 0xff);
					if (varlen >= 4) {
						*cur++ = (uint8)((val >> 16) & 0xff);
						*cur++ = (uint8)((val >> 24) & 0xff);
					}

					/* skip the "," if more array elements */
					if (p && array_sz) {
						char *q = NULL;

						p = strstr (p, ",");	/* current param */
						if (p) {
							p++;
							val = strtoul(p, &q, strncmp(p, "0x", 2) ?
								10 : 16);
						} else {
							printf("Input array size error!");
							free(par);
							return BCME_BADARG;
						}
					}
				}

				/* move to the next parameter string */
				cpar = strtok(NULL, delimit);
			}
			free(par);

			/* commit the tuple if its valid */
			if (found)
				cnt += (cur - &b[cnt]);
		}

		srv++;
	}

	printf("sromrev %d buffer size %d bytes:\n", sromrev, cnt);
	for (i = 0; i < cnt; i++) {
		printf("0x%.02x ", b[i] & 0xff);
		if (i%8 == 7)	printf("\n");
	}
	printf("\n");

	return cnt;
}

static const sromvar_t *
srvlookup(const sromvar_t *tab, char *name, int nlen, int sromrev)
{
	uint32 srrmask;
	const sromvar_t *srv = tab;

	srrmask = 1 << sromrev;

	while (srv->name) {
		if ((strncmp(name, srv->name, nlen) == 0) &&
		    ((srrmask & srv->revmask) != 0))
			break;
		while (srv->flags & SRFL_MORE)
			srv++;
		srv++;
	}

	return srv;
}
#endif	

static int
wl_nvsource(void *wl, cmd_t *cmd, char **argv)
{
	int32 val, err;

	if ((err = wl_var_get(wl, cmd, argv)))
		return (err);

	val = dtoh32(*(int32*)buf);

	switch (val) {
	case 0:
		printf("SROM\n");
		break;
	case 1:
		printf("OTP\n");
		break;
	case 2:
		printf("NVRAM\n");
		break;
	default:
		printf("Unrecognized source %d\n", val);
		break;
	}

	return 0;
}

static int
wlu_srvar(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#else
	int ret, nw, nlen, ro, co, wr, sromrev, shift = 0;
	bool otp = FALSE;
	uint32 val32 = 0;
	char *name, *p, *newval;
	const sromvar_t *srv;
	uint16 w, *words = (uint16 *)&buf[8];
	srom_rw_t   *srt;
	struct ether_addr ea;

	ro = !strcmp(*argv, "rdvar");
	wr = !strcmp(*argv, "wrvar");
	co = !strcmp(*argv, "cisconvert");

	if (!*++argv)
		return -1;

	/* Check the cis source */
	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, "cis_source");
	ret = wl_get(wl, WLC_GET_VAR, buf, strlen(buf)+1);
	if (ret < 0) {
		; /* printf("Error %x: cannot get cis_source\n", ret); */
	}

	if (buf[0] == WLC_CIS_OTP)
		otp = TRUE;
	if (otp && ro) {
		/* Do the same thing as nvget */
		wl_nvget(wl, cmd, argv);
		return ret;
	}

	if ((otp && wr) || co) {
		int cnt = 0, err = 0;
		uint sromrev = 8;
		void *p = NULL;

		/* Find out sromrev from nvram.txt or fakenvram */
		if ((err = wlu_var_getbuf(wl, "nvram_dump", NULL, 0, &p)) < 0) {
			err = wlu_get(wl, WLC_NVRAM_DUMP, &buf[0], WLC_IOCTL_MAXLEN);
		}

		if (err) {
			printf("Fail to get sromrev from nvram file!\n");
			return err;
		}

		if ((p = strstr(p, "sromrev"))) {
			char *q = NULL;

			p = (void*)((char*)p + 8);
			sromrev = strtoul(p, &q, strncmp(p, "0x", 2) ? 10 : 16);
		} else {
			printf("sromrev not defined in nvram file!\n");
			return BCME_ERROR;
		}

		/* leave an empty srom_rw_t at the front for backward
		 * compatibility
		 */
		if ((cnt = parsecis(buf, argv, sromrev)) <= 0) {
			printf("CIS parse failure!\n");
			return BCME_ERROR;
		}

		if (!co)
			/* Pass the append buffer to driver */
			ret = wlu_iovar_set(wl, "cisvar", buf, cnt);
		return ret;
	}

	/* First read the srom and find out the sromrev */
	srt = (srom_rw_t *)buf;
	srt->byteoff = htod32(0);
	srt->nbytes = htod32(2 * SROM4_WORDS);

	if (cmd->get < 0)
		return -1;
	if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return ret;

	if (words[SROM11_SIGN] == SROM11_SIGNATURE) {
		sromrev = 11;

		srt->byteoff = htod32(0);
		srt->nbytes = htod32(2 * SROM11_WORDS);

		if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
			return ret;

	} else {
		if ((words[SROM4_SIGN] != SROM4_SIGNATURE) &&
		    (words[SROM8_SIGN] != SROM4_SIGNATURE))
			nw = SROM_CRCREV;
		else
			nw = SROM4_CRCREV;
		sromrev = words[nw] & 0xff;
	}

	if ((sromrev < 2) || (sromrev > SROM_MAXREV)) {
		return -2;
	}

	nw = 0;
	while ((name = *argv++) != NULL) {
		int off, off_base;

		newval = strchr(name, '=');
		if (newval)
			*newval++ = '\0';
		nlen = strlen(name);
		if ((nlen == 0) || (nlen > 16)) {
			printf("Bad variable name: %s\n", name);
			continue;
		}
		off = 0;
		srv = srvlookup(pci_sromvars, name, nlen + 1, sromrev);
		if (srv->name == NULL) {
			int path;

			srv = srvlookup(perpath_pci_sromvars, name, nlen - 1, sromrev);
			path = name[nlen - 1] - '0';
			if ((srv->name == NULL) || (path < 0) || (path >= MAX_PATH_SROM)) {
				printf("Variable %s does not exist in sromrev %d\n",
				       name, sromrev);
				continue;
			}
			if (sromrev >= 11) {
				if (path == 0) {
					off = SROM11_PATH0;
				} else if (path == 1) {
					off = SROM11_PATH1;
				} else if (path == 2) {
					off = SROM11_PATH2;
				}
			} else if (sromrev >= 8) {
				if (path == 0) {
					off = SROM8_PATH0;
				} else if (path == 1) {
					off = SROM8_PATH1;
				} else if (path == 2) {
					off = SROM8_PATH2;
				} else if (path == 3) {
					off = SROM8_PATH3;
				}
			} else
				off = (path == 0) ? SROM4_PATH0 : SROM4_PATH1;
		}
		off_base = off;
		off += srv->off;

		if (ro) {
			/* This code is cheating a bit: it knows that SRFL_ETHADDR means three
			 * whole words, and SRFL_MORE means 2 whole words (i.e. the masks for
			 * them are all 0xffff).
			 */
			if (srv->flags & SRFL_ETHADDR) {
				w = words[off];
				ea.octet[0] = w >> 8;
				ea.octet[1] = w & 0xff;
				w = words[off + 1];
				ea.octet[2] = w >> 8;
				ea.octet[3] = w & 0xff;
				w = words[off + 2];
				ea.octet[4] = w >> 8;
				ea.octet[5] = w & 0xff;
			} else if (srv->flags & SRFL_MORE) {
				val32 = words[off];
				val32 |= words[srv[1].off] << 16;
			} else {
				shift = ffs(srv->mask) - 1;
				val32 = (words[off] & srv->mask) >> shift;
			}

			/* OK, print it */
			if (srv->flags & SRFL_ETHADDR)
				printf("%s=%s", name, wl_ether_etoa(&ea));
			else if (srv->flags & SRFL_PRHEX)
				printf("%s=0x%x", name, val32);
			else if (srv->flags & SRFL_PRSIGN)
				printf("%s=%d", name, val32);
			else
				printf("%s=%u", name, val32);

			if (srv->flags & SRFL_ARRAY) {
				do {
					srv ++;
					off = off_base + srv->off;

					if (srv->name == NULL)
						break;

					shift = ffs(srv->mask) - 1;
					val32 = (words[off] & srv->mask) >> shift;

					if (srv->flags & SRFL_PRHEX)
						printf(",0x%x", val32);
					else if (srv->flags & SRFL_PRSIGN)
						printf(",%d", val32);
					else
						printf(",%u", val32);
				} while (srv->flags & SRFL_ARRAY);
			}
			printf("\n");

		} else {	/* wr */

			/* Make the change in the image we read */
			if (!newval) {
				printf("wrvar missing value to write for variable %s\n", name);
				ro = 1;
				break;
			}

			/* Cheating again as above */
			if (srv->flags & SRFL_ETHADDR) {
				if (!wl_ether_atoe(newval, &ea)) {
					printf("Argument does not look like a MAC address: %s\n",
						newval);
					ro = 1;
					break;
				}
				words[off] = (ea.octet[0] << 8) | ea.octet[1];
				words[off + 1] = (ea.octet[2] << 8) | ea.octet[3];
				words[off + 2] = (ea.octet[4] << 8) | ea.octet[5];
				off += 2;
			} else {
				val32 = strtoul(newval, &p, 0);
				if (p == newval) {
					printf("Bad value: %s for variable %s\n", newval, name);
					ro = 1;
					break;
				}

				if (srv->flags & SRFL_MORE) {
					words[off] = val32 & 0xffff;
					words[off + 1] = val32 >> 16;
					off++;
				} else {
					shift = ffs(srv->mask) - 1;
					words[off] = (((val32 << shift) & srv->mask) |
						(words[off] & ~srv->mask));

					if (srv->flags & SRFL_ARRAY) {
						do {
							srv ++;
							off = off_base + srv->off;

							if (srv->name == NULL)
								break;

							newval = p + 1;
							val32 = strtoul(newval, &p, 0);
							if (p == newval) {
								printf(
								 "Bad value: %s for variable %s\n",
								 newval, name);
								ro = 1;
								break;
							}

							shift = ffs(srv->mask) - 1;
							words[off] =
							 (((val32 << shift) & srv->mask) |
							 (words[off] & ~srv->mask));
						} while (srv->flags & SRFL_ARRAY);
					}
				}
			}

			if (off > nw)
				nw = off;
		}
	}

	if (!ro) {
		/* Now write all the changes */
		nw++;
		srt->byteoff = 0;
		srt->nbytes = htod32(2 * nw);
		ret = wlu_set(wl, cmd->set, buf, (2 * nw) + 8);
		if (ret < 0)
			printf("Error %d writing the srom\n", ret);
	}

	return ret;
#endif /* _CFE_ */
}

/* All 32bits are used. Please populate wl_msgs2[] with further entries */
static dbg_msg_t wl_msgs[] = {
	{WL_ERROR_VAL, 	"error"},
	{WL_ERROR_VAL, 	"err"},
	{WL_TRACE_VAL, 	"trace"},
	{WL_PRHDRS_VAL,	"prhdrs"},
	{WL_PRPKT_VAL,	"prpkt"},
	{WL_INFORM_VAL,	"inform"},
	{WL_INFORM_VAL,	"info"},
	{WL_INFORM_VAL,	"inf"},
	{WL_TMP_VAL,	"tmp"},
	{WL_OID_VAL,	"oid"},
	{WL_RATE_VAL,	"rate"},
	{WL_ASSOC_VAL,	"assoc"},
	{WL_ASSOC_VAL,	"as"},
	{WL_PRUSR_VAL,	"prusr"},
	{WL_PS_VAL,	"ps"},
	{WL_TXPWR_VAL,	"txpwr"},
	{WL_TXPWR_VAL,	"pwr"},
	{WL_PORT_VAL,	"port"},
	{WL_DUAL_VAL,	"dual"},
	{WL_WSEC_VAL,	"wsec"},
	{WL_WSEC_DUMP_VAL,	"wsec_dump"},
	{WL_LOG_VAL,	"log"},
	{WL_NRSSI_VAL,	"nrssi"},
	{WL_LOFT_VAL,	"loft"},
	{WL_REGULATORY_VAL,	"regulatory"},
	{WL_RADAR_VAL,	"radar"},
	{WL_MPC_VAL,	"mpc"},
	{WL_APSTA_VAL,	"apsta"},
	{WL_DFS_VAL,	"dfs"},
	{WL_MBSS_VAL,	"mbss"},
	{WL_CAC_VAL,	"cac"},
	{WL_AMSDU_VAL,	"amsdu"},
	{WL_AMPDU_VAL,	"ampdu"},
	{WL_FFPLD_VAL,  "ffpld"},
	{0,		NULL}
};

/* msglevels which use wl_msg_level2 should go here */
static dbg_msg_t wl_msgs2[] = {
	{WL_DPT_VAL,  "dpt"},
	{WL_SCAN_VAL,	"scan"},
	{WL_WOWL_VAL,	"wowl"},
	{WL_COEX_VAL,	"coex"},
	{WL_RTDC_VAL,	"rtdc"},
	{WL_PROTO_VAL,	"proto"},
	{WL_CHANINT_VAL,	"chanim"},
	{WL_THERMAL_VAL,	"thermal"},
#ifdef WLP2P
	{WL_P2P_VAL,	"p2p"},
#endif
	{WL_ITFR_VAL,	"itfr"},
#ifdef WLMCHAN
	{WL_MCHAN_VAL,	"mchan"},
#endif
#ifdef WLTDLS
	{ WL_TDLS_VAL, "tdls"},
#endif
	{WL_MCNX_VAL,	"mcnx"},
	{WL_PSTA_VAL,	"psta"},
	{WL_TSO_VAL,	"tso"},
	{WL_TIMESTAMP_VAL, "time"},
	{WL_LPC_VAL, "lpc"},
	{WL_TXBF_VAL, "txbf"},
	{0,		NULL}
};

static int
wl_msglevel(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i;
	uint hval = 0, len, val = 0, found, last_val = 0, msglevel = 0, msglevel2_add = 0;
	uint msglevel2_del = 0, msglevel_add = 0, msglevel_del = 0, supported = 1;
	char *endptr = NULL;
	dbg_msg_t *dbg_msg = wl_msgs, *dbg_msg2 = wl_msgs2;
	void *ptr = NULL;
	struct wl_msglevel2 msglevel64, *reply;
	const char *cmdname = "msglevel";

	UNUSED_PARAMETER(cmd);

	/* but preseve older IOCTL call for older drivers */
	if ((ret = wlu_var_getbuf_sm(wl, cmdname, &msglevel64, sizeof(msglevel64), &ptr) < 0)) {
		if ((ret = wlu_get(wl, WLC_GET_MSGLEVEL, &msglevel, sizeof(int))) < 0)
			return (ret);
		supported = 0;
		msglevel = dtoh32(msglevel);
		if (!*++argv) {
			printf("0x%x ", msglevel);
			for (i = 0; (val = dbg_msg[i].value); i++) {
			if ((msglevel & val) && (val != last_val))
				printf(" %s", dbg_msg[i].string);
			last_val = val;
			}
		printf("\n");
		return (0);
		}
		while (*argv) {
			char *s = *argv;
			if (*s == '+' || *s == '-')
				s++;
			else
				msglevel_del = ~0; /* make the whole list absolute */
			val = strtoul(s, &endptr, 0);
			if (val == 0xFFFFFFFF) {
				fprintf(stderr,
					"Bits >32 are not supported on this driver version\n");
				val = 1;
			}
			/* not an integer if not all the string was parsed by strtoul */
			if (*endptr != '\0') {
				for (i = 0; (val = dbg_msg[i].value); i++)
					if (stricmp(dbg_msg[i].string, s) == 0)
						break;
					if (!val)
						goto usage;
			}
			if (**argv == '-')
				msglevel_del |= val;
			else
				msglevel_add |= val;
			++argv;
		}
		msglevel &= ~msglevel_del;
		msglevel |= msglevel_add;
		msglevel = htod32(msglevel);
		return (wlu_set(wl, WLC_SET_MSGLEVEL, &msglevel, sizeof(int)));
	} else { /* 64bit message level */
		reply = (struct wl_msglevel2 *)ptr;
		reply->low = dtoh32(reply->low);
		reply->high = dtoh32(reply->high);
		if (!*++argv) {
			if (reply->high != 0)
				printf("0x%x%08x", reply->high, reply->low);
			else
				printf("0x%x ", reply->low);
			for (i = 0; (val = dbg_msg2[i].value); i++) {
				if (((reply->high & val)) && (val != last_val))
					printf(" %s", dbg_msg2[i].string);
				last_val = val;
				}
			last_val = 0;
			for (i = 0; (val = dbg_msg[i].value); i++) {
				if (((reply->low & val)) && (val != last_val))
					printf(" %s", dbg_msg[i].string);
				last_val = val;
			}
			printf("\n");
			return (0);
		}
		while (*argv) {
			char* s = *argv;
			char t[32];
			found = 0;
			if (*s == '+' || *s == '-')
				s++;
			else {
				msglevel_del = ~0;	/* make the whole list absolute */
				msglevel2_del = ~0;
			}
			val = strtoul(s, &endptr, 0);
			if (val == 0xFFFFFFFF){ /* Assume >32 bit hex passed in */
				if (!(*s == '0' && *(s+1) == 'x')) {
					fprintf(stderr,
					"Msg bits >32 take only numerical input in hex\n");
					val = 0;
				} else {
					char c[32] = "0x";
					len = strlen(s);
					hval = strtoul(strncpy(t, s, len-8), &endptr, 0);
					*endptr = 0;
					s = s+strlen(t);
					s = strcat(c, s);
					val = strtoul(s, &endptr, 0);
					if (hval == 0xFFFFFFFF) {
						fprintf(stderr, "Invalid entry for msglevel\n");
						hval = 0;
						val = 0;
					}
				}
			}
			if (*endptr != '\0') {
				for (i = 0; (val = dbg_msg[i].value); i++) {
					if (stricmp(dbg_msg[i].string, s) == 0) {
						found = 1;
						break;
					}
				}
				if (!found) {
				for (i = 0; (hval = dbg_msg2[i].value); i++)
					if (stricmp(dbg_msg2[i].string, s) == 0)
						break;
				}
				if (!val && !hval)
				      goto usage;
		      }
		      if (**argv == '-') {
				msglevel_del |= val;
				if (!found)
					msglevel2_del |= hval;
		      }
		      else {
				msglevel_add |= val;
				if (!found)
					msglevel2_add |= hval;
		      }
		      ++argv;
		}
		reply->low &= ~msglevel_del;
		reply->high &= ~msglevel2_del;
		reply->low |= msglevel_add;
		reply->high |= msglevel2_add;
		reply->low = htod32(reply->low);
		reply->high = htod32(reply->high);
		msglevel64.low = reply->low;
		msglevel64.high = reply->high;
		return (wlu_var_setbuf(wl, cmdname, &msglevel64, sizeof(msglevel64)));
	}

usage:
	fprintf(stderr, "msg values may be a list of numbers or names from the following set.\n");
	fprintf(stderr, "Use a + or - prefix to make an incremental change.");

	for (i = 0; (val = dbg_msg[i].value); i++) {
		if (val != last_val)
			fprintf(stderr, "\n0x%04x %s", val, dbg_msg[i].string);
		else
			fprintf(stderr, ", %s", dbg_msg[i].string);
		last_val = val;
	}
	if (supported) {
		for (i = 0; (val = dbg_msg2[i].value); i++) {
			if (val != last_val)
				fprintf(stderr, "\n0x%x00000000 %s", val, dbg_msg2[i].string);
			else
				fprintf(stderr, ", %s", dbg_msg2[i].string);
			last_val = val;
		}
	}
	fprintf(stderr, "\n");
	return 0;
}

/* make sure the PHY msg level here are in sync with wlc_phy_int.h */
#define WL_PHYHAL_ERROR		0x0001
#define WL_PHYHAL_TRACE		0x0002
#define WL_PHYHAL_INFORM	0x0004
#define WL_PHYHAL_TMP		0x0008
#define WL_PHYHAL_TXPWR		0x0010
#define WL_PHYHAL_CAL		0x0020
#define WL_PHYHAL_ACI		0x0040
#define WL_PHYHAL_RADAR		0x0080
#define WL_PHYHAL_THERMAL	0x0100
#define WL_PHYHAL_PAPD		0x0200
#define WL_PHYHAL_FCBS		0x0400
#define WL_PHYHAL_TIMESTAMP	0x8000

static phy_msg_t wl_phy_msgs[] = {
	{WL_PHYHAL_ERROR,	"error"},
	{WL_PHYHAL_ERROR, 	"err"},
	{WL_PHYHAL_TRACE,	"trace"},
	{WL_PHYHAL_INFORM,	"inform"},
	{WL_PHYHAL_TMP,		"tmp"},
	{WL_PHYHAL_TXPWR,	"txpwr"},
	{WL_PHYHAL_CAL,		"cal"},
	{WL_PHYHAL_ACI,         "aci"},
	{WL_PHYHAL_RADAR,	"radar"},
	{WL_PHYHAL_THERMAL,	"thermal"},
	{WL_PHYHAL_PAPD,	"papd"},
	{WL_PHYHAL_FCBS,	"fcbs"},
	{WL_PHYHAL_TIMESTAMP,	"time"},
	{0,		NULL}
	};

static int
wl_phymsglevel(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i;
	uint val = 0, last_val = 0;
	uint phymsglevel = 0, phymsglevel_add = 0, phymsglevel_del = 0;
	char *endptr;
	phy_msg_t *phy_msg = wl_phy_msgs;
	const char *cmdname = "phymsglevel";

	UNUSED_PARAMETER(cmd);
	if ((ret = wlu_iovar_getint(wl, cmdname, (int *)&phymsglevel) < 0)) {
		return ret;
	}
	phymsglevel = dtoh32(phymsglevel);
	if (!*++argv) {
		printf("0x%x ", phymsglevel);
		for (i = 0; (val = phy_msg[i].value); i++) {
			if ((phymsglevel & val) && (val != last_val))
				printf(" %s", phy_msg[i].string);
			last_val = val;
		}
		printf("\n");
		return (0);
	}
	while (*argv) {
		char *s = *argv;
		if (*s == '+' || *s == '-')
			s++;
		else
			phymsglevel_del = ~0; /* make the whole list absolute */
		val = strtoul(s, &endptr, 0);
		if (val == 0xFFFFFFFF) {
			fprintf(stderr,
				"Bits >32 are not supported on this driver version\n");
			val = 1;
		}
		/* not an integer if not all the string was parsed by strtoul */
		if (*endptr != '\0') {
			for (i = 0; (val = phy_msg[i].value); i++)
				if (stricmp(phy_msg[i].string, s) == 0)
					break;
				if (!val)
					goto usage;
		}
		if (**argv == '-')
			phymsglevel_del |= val;
		else
			phymsglevel_add |= val;
		++argv;
	}
	phymsglevel &= ~phymsglevel_del;
	phymsglevel |= phymsglevel_add;
	phymsglevel = htod32(phymsglevel);
	return (wlu_iovar_setint(wl, cmdname, (int)phymsglevel));

usage:
	fprintf(stderr, "msg values may be a list of numbers or names from the following set.\n");
	fprintf(stderr, "Use a + or - prefix to make an incremental change.");
	for (i = 0; (val = phy_msg[i].value); i++) {
		if (val != last_val)
			fprintf(stderr, "\n0x%04x %s", val, phy_msg[i].string);
		else
			fprintf(stderr, ", %s", phy_msg[i].string);
		last_val = val;
	}
	return 0;
}
#endif /* !ATE_BUILD */

struct d11_mcs_rate_info {
	uint8 constellation_bits;
	uint8 coding_q;
	uint8 coding_d;
};

static const struct d11_mcs_rate_info wlu_mcs_info[] = {
	{ 1, 1, 2 }, /* MCS  0: MOD: BPSK,   CR 1/2 */
	{ 2, 1, 2 }, /* MCS  1: MOD: QPSK,   CR 1/2 */
	{ 2, 3, 4 }, /* MCS  2: MOD: QPSK,   CR 3/4 */
	{ 4, 1, 2 }, /* MCS  3: MOD: 16QAM,  CR 1/2 */
	{ 4, 3, 4 }, /* MCS  4: MOD: 16QAM,  CR 3/4 */
	{ 6, 2, 3 }, /* MCS  5: MOD: 64QAM,  CR 2/3 */
	{ 6, 3, 4 }, /* MCS  6: MOD: 64QAM,  CR 3/4 */
	{ 6, 5, 6 }, /* MCS  7: MOD: 64QAM,  CR 5/6 */
	{ 8, 3, 4 }, /* MCS  8: MOD: 256QAM, CR 3/4 */
	{ 8, 5, 6 }  /* MCS  9: MOD: 256QAM, CR 5/6 */
};

static uint
wl_mcs2rate(uint mcs, uint nss, uint bw, int sgi)
{
	const int ksps = 250; /* kilo symbols per sec, 4 us sym */
	const int Nsd_20MHz = 52;
	const int Nsd_40MHz = 108;
	const int Nsd_80MHz = 234;
	const int Nsd_160MHz = 468;
	uint rate;

	if (mcs == 32) {
		/* just return fixed values for mcs32 instead of trying to parametrize */
		rate = (sgi == 0) ? 6000 : 6700;
	} else if (mcs <= 9) {
		/* This calculation works for 11n HT and 11ac VHT if the HT mcs values
		 * are decomposed into a base MCS = MCS % 8, and Nss = 1 + MCS / 8.
		 * That is, HT MCS 23 is a base MCS = 7, Nss = 3
		 */

		/* find the number of complex numbers per symbol */
		if (bw == 20) {
			rate = Nsd_20MHz;
		} else if (bw == 40) {
			rate = Nsd_40MHz;
		} else if (bw == 80) {
			rate = Nsd_80MHz;
		} else if (bw == 160) {
			rate = Nsd_160MHz;
		} else {
			rate = 1;
		}

		/* multiply by bits per number from the constellation in use */
		rate = rate * wlu_mcs_info[mcs].constellation_bits;

		/* adjust for the number of spatial streams */
		rate = rate * nss;

		/* adjust for the coding rate given as a quotient and divisor */
		rate = (rate * wlu_mcs_info[mcs].coding_q) / wlu_mcs_info[mcs].coding_d;

		/* multiply by Kilo symbols per sec to get Kbps */
		rate = rate * ksps;

		/* adjust the symbols per sec for SGI
		 * symbol duration is 4 us without SGI, and 3.6 us with SGI,
		 * so ratio is 10 / 9
		 */
		if (sgi) {
			/* add 4 for rounding of division by 9 */
			rate = ((rate * 10) + 4) / 9;
		}
	} else {
		rate = 0;
	}

	return rate;
}

/* take a wl_ratespec arg and return phy rate in 500Kbps units */
static int
wl_ratespec2rate(uint32 rspec)
{
	const char* fn_name = "wl_ratespec2rate";
	int rate = -1;
	int sgi = ((rspec & WL_RSPEC_SGI) != 0);

	if ((rspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_RATE) {
		rate = (rspec & WL_RSPEC_RATE_MASK);
	} else if ((rspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_HT) {
		uint mcs = (rspec & WL_RSPEC_RATE_MASK);

		if (mcs > 32) {
			fprintf(stderr, "%s: MCS %u out of range (>32) in ratespec 0x%X\n",
				fn_name, mcs, rspec);
		} else if (mcs == 32) {
			rate = wl_mcs2rate(mcs, 1, 40, sgi) / 500;
		} else {
			uint nss = 1 + (mcs / 8);
			mcs = mcs % 8;

			rate = wl_mcs2rate(mcs, nss, 20, sgi) / 500;
		}
	} else if ((rspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_VHT) {
		uint mcs = (rspec & WL_RSPEC_VHT_MCS_MASK);
		uint nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;

		rate = wl_mcs2rate(mcs, nss, 20, sgi) / 500;
	} else {
		fprintf(stderr, "%s: expected rate encoding in ratespec 0x%X\n",
			fn_name, (uint)rspec);
	}

	return rate;
}

/* take rate arg in units of 500Kbits/s and print it in units of Mbit/s */
static void
wl_printrate(int val)
{
	char rate_buf[32];

	printf("%s\n", rate_int2string(rate_buf, val));
}

/* convert rate string in Mbit/s format, like "11", "5.5", to internal 500 Kbit/s units */
static int
rate_string2int(char *s)
{
	if (!stricmp(s, "-1"))
		return (0);
	if (!stricmp(s, "5.5"))
		return (11);
	return (atoi(s) * 2);
}

/* convert rate internal 500 Kbits/s units to string in Mbits/s format, like "11", "5.5" */
static char*
rate_int2string(char *rate_buf, int val)
{
	if ((val == -1) || (val == 0))
		sprintf(rate_buf, "auto");
	else
		sprintf(rate_buf, "%d%s Mbps", (val / 2), (val & 1) ? ".5" : "");
	return (rate_buf);
}

/* Format a ratespec for "nrate" output
 * Will handle both current wl_ratespec and legacy (ioctl_version 1) nrate ratespec
 */
static void
wl_nrate_print(uint32 rspec)
{
	const char * rspec_auto = "auto";
	uint encode, rate, txexp = 0, bw_val;
	const char* stbc = "";
	const char* ldpc = "";
	const char* sgi = "";
	const char* txbf = "";
	const char* bw = "";
	int stf;

	if (rspec == 0) {
		encode = WL_RSPEC_ENCODE_RATE;
	} else if (ioctl_version == 1) {
		encode = (rspec & OLD_NRATE_MCS_INUSE) ? WL_RSPEC_ENCODE_HT : WL_RSPEC_ENCODE_RATE;
		stf = (int)((rspec & OLD_NRATE_STF_MASK) >> OLD_NRATE_STF_SHIFT);
		rate = (rspec & OLD_NRATE_RATE_MASK);

		if (rspec & OLD_NRATE_OVERRIDE) {
			if (rspec & OLD_NRATE_OVERRIDE_MCS_ONLY)
				rspec_auto = "fixed mcs only";
			else
				rspec_auto = "fixed";
		}
	} else {
		int siso;
		encode = (rspec & WL_RSPEC_ENCODING_MASK);
		rate = (rspec & WL_RSPEC_RATE_MASK);
		txexp = (rspec & WL_RSPEC_TXEXP_MASK) >> WL_RSPEC_TXEXP_SHIFT;
		stbc  = ((rspec & WL_RSPEC_STBC) != 0) ? " stbc" : "";
		ldpc  = ((rspec & WL_RSPEC_LDPC) != 0) ? " ldpc" : "";
		sgi   = ((rspec & WL_RSPEC_SGI)  != 0) ? " sgi"  : "";
		txbf  = ((rspec & WL_RSPEC_TXBF) != 0) ? " txbf" : "";
		bw_val = (rspec & WL_RSPEC_BW_MASK);

		if (bw_val == WL_RSPEC_BW_20MHZ) {
			bw = "bw20";
		} else if (bw_val == WL_RSPEC_BW_40MHZ) {
			bw = "bw40";
		} else if (bw_val == WL_RSPEC_BW_80MHZ) {
			bw = "bw80";
		} else if (bw_val == WL_RSPEC_BW_160MHZ) {
			bw = "bw160";
		}

		/* initialize stf mode to an illegal value and
		 * fix to a backward compatable value if possible
		 */
		stf = -1;
		/* for stf calculation, determine if the rate is single stream.
		 * Legacy rates WL_RSPEC_ENCODE_RATE are single stream, and
		 * HT rates for mcs 0-7 are single stream
		 */
		siso = (encode == WL_RSPEC_ENCODE_RATE) ||
			((encode == WL_RSPEC_ENCODE_HT) && rate < 8);

		/* calc a value for nrate stf mode */
		if (txexp == 0) {
			if ((rspec & WL_RSPEC_STBC) && siso) {
				/* STF mode STBC */
				stf = OLD_NRATE_STF_STBC;
			} else {
				/* STF mode SISO or SDM */
				stf = (siso) ? OLD_NRATE_STF_SISO : OLD_NRATE_STF_SDM;
			}
		} else if (txexp == 1 && siso) {
			/* STF mode CDD */
			stf = OLD_NRATE_STF_CDD;
		}

		if (rspec & WL_RSPEC_OVERRIDE_RATE) {
			rspec_auto = "fixed";
		}
	}

	if (encode == WL_RSPEC_ENCODE_RATE) {
		if (rspec == 0) {
			printf("auto\n");
		} else {
			printf("legacy rate %d%s Mbps stf mode %d %s\n",
			       rate/2, (rate % 2)?".5":"", stf, rspec_auto);
		}
	} else if (encode == WL_RSPEC_ENCODE_HT) {
		printf("mcs index %d stf mode %d %s\n",
		       rate, stf, rspec_auto);
	} else {
		uint vht = (rspec & WL_RSPEC_VHT_MCS_MASK);
		uint Nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
		printf("vht mcs %d Nss %d Tx Exp %d %s%s%s%s%s %s\n",
		       vht, Nss, txexp, bw, stbc, txbf, ldpc, sgi, rspec_auto);
	}
}

/*
 * Format a ratespec for output of any of the wl_rate() iovars
 */
static char*
wl_rate_print(char *rate_buf, uint32 rspec)
{
	uint encode, rate, txexp, bw_val;
	const char* stbc;
	const char* ldpc;
	const char* sgi;
	const char* bw;
	const char* txbf;

	encode = (rspec & WL_RSPEC_ENCODING_MASK);
	rate = (rspec & WL_RSPEC_RATE_MASK);
	txexp = (rspec & WL_RSPEC_TXEXP_MASK) >> WL_RSPEC_TXEXP_SHIFT;
	bw_val = (rspec & WL_RSPEC_BW_MASK);
	stbc  = ((rspec & WL_RSPEC_STBC) != 0) ? " stbc" : "";
	ldpc  = ((rspec & WL_RSPEC_LDPC) != 0) ? " ldpc" : "";
	sgi   = ((rspec & WL_RSPEC_SGI)  != 0) ? " sgi"  : "";
	txbf  = ((rspec & WL_RSPEC_TXBF) != 0) ? " txbf" : "";

	if (bw_val == WL_RSPEC_BW_UNSPECIFIED) {
		bw = "auto";
	} else if (bw_val == WL_RSPEC_BW_20MHZ) {
		bw = "20";
	} else if (bw_val == WL_RSPEC_BW_40MHZ) {
		bw = "40";
	} else if (bw_val == WL_RSPEC_BW_80MHZ) {
		bw = "80";
	} else if (bw_val == WL_RSPEC_BW_160MHZ) {
		bw = "160";
	} else {
		bw = "???";
	}

	if (rspec == 0) {
		sprintf(rate_buf, "auto");
	} else if (encode == WL_RSPEC_ENCODE_RATE) {
		sprintf(rate_buf, "rate %d%s Mbps Tx Exp %d",
			rate/2, (rate % 2)?".5":"", txexp);
	} else if (encode == WL_RSPEC_ENCODE_HT) {
		sprintf(rate_buf, "ht mcs %d Tx Exp %d BW %s%s%s%s%s",
		        rate, txexp, bw, stbc, txbf, ldpc, sgi);
	} else if (encode == WL_RSPEC_ENCODE_VHT) {
		uint mcs = (rspec & WL_RSPEC_VHT_MCS_MASK);
		uint Nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
		sprintf(rate_buf, "vht mcs %d Nss %d Tx Exp %d BW %s%s%s%s%s",
		        mcs, Nss, txexp, bw, stbc, txbf, ldpc, sgi);
	} else {
		sprintf(rate_buf, "<unknown encoding for ratespec 0x%08X>", rspec);
	}

	return rate_buf;
}

/* handles both "rate" and "mrate", which makes the flow a bit complex */
static int
wl_rate_mrate(void *wl, cmd_t *cmd, char **argv)
{
	const char* fn_name = "wl_rate_mrate";
	int error;
	int val;
	int band;
	int list[3];
	char aname[sizeof("5g_mrate") + 1];
	char bgname[sizeof("2g_mrate") + 1];
	char *name;

	sprintf(aname, "5g_%s", cmd->name);
	sprintf(bgname, "2g_%s", cmd->name);

	if ((error = wlu_get(wl, WLC_GET_BAND, &band, sizeof(uint))) < 0)
		return error;
	band = dtoh32(band);

	if ((error = wlu_get(wl, WLC_GET_BANDLIST, list, sizeof(list))) < 0)
		return error;
	list[0] = dtoh32(list[0]);
	list[1] = dtoh32(list[1]);
	list[2] = dtoh32(list[2]);

	if (!list[0])
		return IOCTL_ERROR;
	else if (list[0] > 2)
		list[0] = 2;

	/* toss the command name from the args */
	argv++;

	if ((!strcmp(cmd->name, "rate"))) {
		/* it is "rate" */
		if (!*argv) {
			/* it is "rate" get. handle it here */
			/* WLC_GET_RATE processing */
			if ((error = wlu_get(wl, cmd->get, &val, sizeof(int))) < 0)
				return error;

			val = dtoh32(val);
			wl_printrate(val);
			return 0;
		}
	}

	switch (band) {
	case WLC_BAND_AUTO :
		if (list[0] > 1) {
			fprintf(stderr,
			        "%s: driver band must be locked to %s %s, use %s/%s instead\n",
			        fn_name, (*argv ? "set" : "get"), cmd->name,
			        bgname, aname);
			return BAD_PARAM;
		} else if (list[1] == WLC_BAND_5G)
			name = (char *)aname;
		else if (list[1] == WLC_BAND_2G)
			name = (char *)bgname;
		else
			return IOCTL_ERROR;

		break;

	case WLC_BAND_5G :
		name = (char *)aname;
		break;

	case WLC_BAND_2G :
		name = (char *)bgname;
		break;

	default :
		return IOCTL_ERROR;
	}

	if (!*argv) {
		/* it is "mrate" get */
		if ((error = wlu_iovar_getint(wl, name, &val) < 0))
			return error;

		val = dtoh32(val);

		if (ioctl_version == 1) {
			wl_printrate(val);
		} else {
			wl_printrate(wl_ratespec2rate((uint32)val));
		}
	} else {
		/* create the ioctl value based on the major ioctl version */
		if (ioctl_version == 1) {
			/* for ver=1 ioctl interfaces, override values for 2g_(m)rate/5g_(m)rate
			 * are just given as 500 Kbps units
			 */
			val = rate_string2int(*argv);
		} else {
			/* for ver>1 ioctl interfaces, override values for 2g_(m)rate/5g_(m)rate
			 * are a wl_ratespec of a legacy rate.
			 */
			val = WL_RSPEC_ENCODE_RATE | rate_string2int(*argv);
		}

		val = htod32(val);

		error = wlu_iovar_setint(wl, name, val);
	}

	return error;
}
#ifndef ATE_BUILD
/* parse the -v/--vht or -c argument for the wl_rate() command.
 * return FALSE if the arg does not look like MxS or cMsS, where M and S are single digits
 * return TRUE if the arg does look like MxS or cMsS, setting mcsp to M, and nssp to S
 */
static int
wl_parse_vht_spec(const char* cp, int* mcsp, int* nssp)
{
	char *startp, *endp;
	char c;
	int mcs, nss;
	char sx;

	if (cp == NULL || cp[0] == '\0') {
		return FALSE;
	}

	if (cp[0] == 'c') {
		startp = (char*)cp + 1;
		sx = 's';
	}
	else {
		startp = (char*)cp;
		sx = 'x';
	}

	mcs = (int)strtol(startp, &endp, 10);
	/* verify MCS 0-9, and next char is 's' or 'x' */
	if (mcs < 0 || mcs > 9 || endp[0] != sx) {
		return FALSE;
	}

	/* grab the char after the 'x'/'s' and convert to value */
	c = endp[1];
	nss = 0;
	if (isdigit((int)c)) {
		nss = c - '0';
	}

	/* consume trailing space after digit */
	cp = &endp[2];
	while (isspace((int)(*cp))) {
		cp++;
	}

	/* check for trailing garbage after digit */
	if (cp[0] != '\0') {
		return FALSE;
	}

	if (nss < 1 || nss > 8) {
		return FALSE;
	}

	*mcsp = mcs;
	*nssp = nss;

	return TRUE;
}

static int
wl_rate(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t mo;
	char *option_name = NULL;
	char* endp;
	const int option_name_len = 64;
	const char* iov_name;
	const char* fn_name = "wl_rate";
	bool options = FALSE;
	bool auto_set = FALSE;
	bool legacy_set = FALSE, ht_set = FALSE, vht_set = FALSE;
	int rate, mcs, Nss, tx_exp, bw;
	bool stbc, ldpc, sgi;
	int err, opt_err;
	uint32 rspec = 0;

	/* set default values */
	rate = 0;
	mcs = 0;
	Nss = 0;
	tx_exp = 0;
	stbc = FALSE;
	ldpc = FALSE;
	sgi = FALSE;
	bw = 0;

	/* save the command name */
	iov_name = *argv++;

	if (ioctl_version == 1) {
		fprintf(stderr,
			"cmd %s not supported in this version of the driver, ioctl version 1.\n",
			iov_name);
		return USAGE_ERROR;
	}

	/* process a GET */
	if (!*argv) {
		uint32 val = 0;
		char * rate_str;
		const int rate_str_len = 64;

		if (cmd->get < 0)
			return -1;
		if ((err = wlu_iovar_getint(wl, iov_name, (int*)&val)) < 0)
			return err;

		rate_str = malloc(rate_str_len);
		if (rate_str == NULL) {
			fprintf(stderr, "%s: malloc failure for rate string buffer.\n", fn_name);
			return -1;
		}
		memset(rate_str, 0, rate_str_len);

		wl_rate_print(rate_str, val);

		printf("%s\n", rate_str);

		free(rate_str);

		return 0;
	}

	/* process a SET */

	/* initialze to common error for the miniopt processing */
	err = USAGE_ERROR;

	/* alloc option name for error messages */
	option_name = malloc(option_name_len);
	if (option_name == NULL) {
		fprintf(stderr, "%s: malloc failure for option_name buffer.\n", fn_name);
		return -1;
	}
	memset(option_name, 0, option_name_len);

	miniopt_init(&mo, fn_name, "lg", TRUE);
	while ((opt_err = miniopt(&mo, argv)) != -1) {
		if (opt_err == 1) {
			err = -1;
			goto exit;
		}
		argv += mo.consumed;

		/* track whether or not the command line is just a single positional
		 * parameter of "auto" or a legacy rate, or is using options
		 */
		if (!mo.positional) {
			options = TRUE;	/* command line is using options */
		}
		if (mo.positional) {
			/* command line is using a positional parameter,
			 * complain if options are also being used
			 */
			if (options) {
				fprintf(stderr,
				        "%s: cannot mix positional args and options. "
				        "Got positional arg \"%s\" after options.\n",
				        fn_name, mo.valstr);
				goto exit;
			}
			/* complain if there are any more parameters since there should only
			 * be one positional param, "auto" or a legacy rate.
			 */
			if (*argv != NULL) {
				fprintf(stderr,
				        "%s: unexpected parameter \"%s\" after rate value.\n",
				        fn_name, *argv);
				goto exit;
			}
			/* test for "auto" to clear the rate override */
			if (!strcmp(mo.valstr, "auto")) {
				auto_set = TRUE;
			} else {
				/* pretend there was a '-r' option */
				mo.opt = 'r';
			}
		}

		/* format the option name for error messages */
		if (mo.key[0] != '\0') {
			/* would like to do the following, but snprintf() is not availble in
			 * all target builds. Fails in win_mfgtest_wl build.
			 *
			 * snprintf(option_name, option_name_len, "--%s", mo.key);
			 * option_name[option_name_len - 1] = '\0';
			 */
			size_t key_len;

			key_len = strlen(mo.key);
			/* limit key_len to space in str buffer minus the '--' and null */
			key_len = MIN((uint)(option_name_len - 3), key_len);

			memcpy(option_name, "--", 2);
			memcpy(option_name + 2, mo.key, key_len);
			option_name[2 + key_len] = '\0';
		} else {
			sprintf(option_name, "-%c", mo.opt);
		}

		/* Option: -r or --rate */
		if (mo.opt == 'r' ||
		    !strcmp(mo.key, "rate")) {
			if (mo.valstr == NULL) {
				fprintf(stderr,
				        "%s: expected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			/* special case check for "-r 5.5" */
			if (!strcmp(mo.valstr, "5.5")) {
				rate = 11;
			} else if (!mo.good_int) {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			} else {
				rate = mo.val*2;
			}

			legacy_set = TRUE;
		}
		/* Option: -h or --ht */
		if (mo.opt == 'h' ||
		    !strcmp(mo.key, "ht")) {
			if (mo.valstr == NULL) {
				fprintf(stderr,
				        "%s: expected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			if (!mo.good_int) {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			}

			mcs = mo.val;
			ht_set = TRUE;
		}
		/* Option: -v or --vht */
		if (mo.opt == 'v' ||
		    !strcmp(mo.key, "vht")) {
			if (mo.valstr == NULL || mo.valstr[0] == 'c') {
				fprintf(stderr,
				        "%s: expected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			mcs = (int)strtol(mo.valstr, &endp, 10);
			if (*endp == '\0') {
				mcs = mo.val;
				vht_set = TRUE;
			} else if (wl_parse_vht_spec(mo.valstr, &mcs, &Nss)) {
				vht_set = TRUE;
			} else {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			}
		}
		/* Option: -c (system team's c notiation: c<MCS>s<Nss>) */
		if (mo.opt == 'c') {
			if (mo.valstr == NULL || mo.valstr[0] != 'c') {
				fprintf(stderr,
				        "%s: expected a value start with c for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			mcs = (int)strtol(mo.valstr + 1, &endp, 10);
			if (*endp == '\0') {
				vht_set = TRUE;
			} else if (wl_parse_vht_spec(mo.valstr, &mcs, &Nss)) {
				vht_set = TRUE;
			} else {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			}
		}
		/* Option: -s or --ss */
		if (mo.opt == 's' ||
		    !strcmp(mo.key, "ss")) {
			if (mo.valstr == NULL) {
				fprintf(stderr,
				        "%s: expected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			if (!mo.good_int) {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			}
			Nss = mo.val;
		}
		/* Option: -x or --exp */
		if (mo.opt == 'x' ||
		    !strcmp(mo.key, "exp")) {
			if (mo.valstr == NULL) {
				fprintf(stderr,
				        "%s: expected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			if (!mo.good_int) {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			}
			tx_exp = mo.val;
			if (tx_exp < 0 || tx_exp > 3) {
				fprintf(stderr,
				        "%s: tx expansion %d out of range [0-3]\n",
				        fn_name, tx_exp);
				err = BCME_RANGE;
				goto exit;
			}
		}
		/* Option: --stbc */
		if (!strcmp(mo.key, "stbc")) {
			if (mo.valstr != NULL) {
				fprintf(stderr,
				        "%s: unexpected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}
			stbc = TRUE;
		}
		/* Option: -l or --ldpc */
		if (mo.opt == 'l' ||
		    !strcmp(mo.key, "ldpc")) {
			if (mo.valstr != NULL) {
				fprintf(stderr,
				        "%s: unexpected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}
			ldpc = TRUE;
		}
		/* Option: -g or --sgi */
		if (mo.opt == 'g' ||
		    !strcmp(mo.key, "sgi")) {
			if (mo.valstr != NULL) {
				fprintf(stderr,
				        "%s: unexpected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}
			sgi = TRUE;
		}
		/* Option: -b or --bandwidth */
		if (mo.opt == 'b' ||
		    !strcmp(mo.key, "bandwidth")) {
			if (mo.valstr == NULL) {
				fprintf(stderr,
				        "%s: expected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			if (!mo.good_int) {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			}
			if (mo.val == 20) {
				bw = WL_RSPEC_BW_20MHZ;
			} else if (mo.val == 40) {
				bw = WL_RSPEC_BW_40MHZ;
			} else if (mo.val == 80) {
				bw = WL_RSPEC_BW_80MHZ;
			} else if (mo.val == 160) {
				bw = WL_RSPEC_BW_160MHZ;
			} else {
				fprintf(stderr,
				        "%s: unexpected bandwidth specified \"%s\", "
				        "expected 20, 40, 80, or 160\n",
				        fn_name, mo.valstr);
				goto exit;
			}
		}
	}

	/*
	 * check for necessary use of one of -r/-h/-v or auto
	 */

	if (!auto_set && !legacy_set && !ht_set && !vht_set) {
		fprintf(stderr, "%s: must specify one of \"auto\", legacy rate -r/--rate, "
		        "HT (11n) rate -h/--ht, or VHT (11ac) rate -v/--vht\n",
		        fn_name);
		goto exit;
	}

	/*
	 * check for incompatible use of -r/-h/-v
	 */

	if (legacy_set && ht_set) {
		fprintf(stderr, "%s: cannot use legacy rate -r/--rate and "
		        "HT rate -h/--ht at the same time\n",
		        fn_name);
		goto exit;
	}

	if (legacy_set && vht_set) {
		fprintf(stderr, "%s: cannot use legacy rate -r/--rate and "
		        "HT rate -v/--vht at the same time\n",
		        fn_name);
		goto exit;
	}

	if (ht_set && vht_set) {
		fprintf(stderr, "%s: cannot use HT rate -h/--ht and "
		        "HT rate -v/--vht at the same time\n",
		        fn_name);
		goto exit;
	}

	/* Nss can only be used with VHT */
	if (!vht_set && Nss != 0) {
		fprintf(stderr, "%s: cannot use -s/--ss option with non VHT rate\n",
		        fn_name);
		goto exit;
	}

	/* STBC, LDPC, SGI can only be used with HT/VHT rates */
	if ((stbc || ldpc || sgi) && !(ht_set || vht_set)) {
		fprintf(stderr, "%s: cannot use STBC/LDPC/SGI options with non HT/VHT rates\n",
		        fn_name);
		goto exit;
	}

	/* set the ratespec encoding type and basic rate value */
	if (auto_set) {
		rspec = 0;
	} else if (legacy_set) {
		rspec = WL_RSPEC_ENCODE_RATE;	/* 11abg */
		rspec |= rate;
	} else if (ht_set) {
		rspec = WL_RSPEC_ENCODE_HT;	/* 11n HT */
		rspec |= mcs;
	} else {
		rspec = WL_RSPEC_ENCODE_VHT;	/* 11ac VHT */
		if (Nss == 0) {
			Nss = 1; /* default Nss = 1 if --ss option not given */
		}
		rspec |= (Nss << WL_RSPEC_VHT_NSS_SHIFT) | mcs;
	}

	/* set the other rspec fields */
	rspec |= (tx_exp << WL_RSPEC_TXEXP_SHIFT);
	rspec |= bw;
	rspec |= (stbc ? WL_RSPEC_STBC : 0);
	rspec |= (ldpc ? WL_RSPEC_LDPC : 0);
	rspec |= (sgi  ? WL_RSPEC_SGI  : 0);

	err = wlu_iovar_setint(wl, iov_name, (int)rspec);

exit:
	if (option_name != NULL) {
		free(option_name);
	}

	return err;
}

static int
wl_wepstatus(void *wl, cmd_t *cmd, char **argv)
{
	int val, error;
	const char *name = "wsec";
	int wsec;

	UNUSED_PARAMETER(cmd);

	if (!*++argv) {
		if ((error = wlu_iovar_getint(wl, name, &val) < 0))
			return error;

		printf("%d\n", val);
		return 0;
	} else {
		val = atoi(*argv);
		if ((error = wlu_iovar_getint(wl, name, &wsec) < 0))
			return error;

		if (val)
			wsec |= WEP_ENABLED;
		else
			wsec &= ~WEP_ENABLED;

		return wlu_iovar_setint(wl, name, wsec);
	}
}

static int
wl_bss_max(void *wl, cmd_t *cmd, char **argv)
{
	int val = 1;
	int error;

	UNUSED_PARAMETER(argv);

	/* Get the CAP variable; search for mbss4 or mbss16 */
	strcpy(buf, "cap");
	if ((error = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MEDLEN)) < 0)
		return (error);
	if (strstr(buf, "mbss16")) val = 16;
	else if (strstr(buf, "mbss4")) val = 4;

	printf("%d\n", val);
	return (0);
}

static int
wl_phy_rate(void *wl, cmd_t *cmd, char **argv)
{
	int val, error;
	const char *name = cmd->name;
	static const struct {
		const char* orig;
		const char* new;
	} aliases[] = {
		{"bg_rate",  "2g_rate"},
		{"bg_mrate", "2g_mrate"},
		{"a_rate",   "5g_rate"},
		{"a_mrate",  "5g_mrate"},
	};

	/* toss the command name from the args */
	argv++;

	/* if we are not using the legacy ioctl driver, translate the
	 * bg_* prefix to 2g_* and a_* to 5g_* iovars
	 */
	if (ioctl_version > 1) {
		int i;
		for (i = 0; i < 4; i++) {
			if (!strcmp(name, aliases[i].orig)) {
				name = aliases[i].new;
				break;
			}
		}
	}

	if (!*argv) {
		error = wlu_iovar_getint(wl, name, &val);
		if (error < 0)
			return (error);

		val = dtoh32(val);
		if (ioctl_version > 1) {
			wl_printrate(wl_ratespec2rate((uint32)val));
		} else {
			wl_printrate(val);
		}
	} else {
		/* create the ioctl value based on the major ioctl version */
		if (ioctl_version == 1) {
			/* for ver=1 ioctl interfaces, override values for 2g_(m)rate/5g_(m)rate
			 * are just given as 500 Kbps units
			 */
			val = rate_string2int(*argv);
		} else {
			/* for ver>1 ioctl interfaces, override values for 2g_(m)rate/5g_(m)rate
			 * are a wl_ratespec of a legacy rate.
			 */
			val = WL_RSPEC_ENCODE_RATE | rate_string2int(*argv);
		}


		val = htod32(val);
		error = wlu_iovar_setint(wl, name, val);
	}

	return error;
}
#endif /* !ATE_BUILD */

static int
wl_nrate(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t to;
	const char* fn_name = "wl_nrate";
	bool mcs_set = FALSE, legacy_set = FALSE, stf_set = FALSE;
	bool mcs_only = FALSE;
	int err, opt_err;
	uint32 val = 0;
	uint32 rate = 0;
	uint32 nrate = 0;
	uint32 mcs = 0;
	uint stf = 0;

	/* toss the command name */
	argv++;

	if (!*argv) {
		if (cmd->get < 0)
			return -1;
		if ((err = wlu_iovar_getint(wl, "nrate", (int*)&val)) < 0)
			return err;

		/* print a user readable line for the nrate rspec */
		wl_nrate_print(val);

		return 0;
	}

	/* check for a single argument of "auto" or -1 */
	if ((!strcmp(argv[0], "auto") || !strcmp(argv[0], "-1")) &&
	    argv[1] == NULL) {
		/* clear the nrate override */
		err = wlu_iovar_setint(wl, "nrate", 0);
		goto exit;
	}

	miniopt_init(&to, fn_name, "w", FALSE);
	while ((opt_err = miniopt(&to, argv)) != -1) {
		if (opt_err == 1) {
			err = -1;
			goto exit;
		}
		argv += to.consumed;

		if (to.opt == 'r') {
			if (!to.good_int) {
				/* special case check for "-r 5.5" */
				if (!strcmp(to.valstr, "5.5")) {
					to.val = 11;
				} else {
					fprintf(stderr,
						"%s: could not parse \"%s\" as a rate value\n",
						fn_name, to.valstr);
					err = -1;
					goto exit;
				}
			} else
			      to.val = to.val*2;
			if (mcs_set) {
				fprintf(stderr, "%s: cannot use -r and -m\n", fn_name);
				err = -1;
				goto exit;
			}

			legacy_set = TRUE;
			rate = to.val;
		}
		if (to.opt == 'm') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for mcs\n",
					fn_name, to.valstr);
				err = -1;
				goto exit;
			}
			if (legacy_set) {
				fprintf(stderr, "%s: cannot use -r and -m\n", fn_name);
				err = -1;
				goto exit;
			}
			mcs_set = TRUE;
			mcs = to.val;
		}
		if (to.opt == 's') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for stf mode\n",
					fn_name, to.valstr);
				err = -1;
				goto exit;
			}
			stf = to.val;
			stf_set = TRUE;
		}
		if (to.opt == 'w') {
			mcs_only = TRUE;
		}
	}

	if ((mcs_only && !mcs_set) || (mcs_only && (stf_set || legacy_set))) {
		fprintf(stderr, "%s: can use -w only with -m\n", fn_name);
		err = -1;
		goto exit;
	}

	if (!stf_set) {
		if (legacy_set)
			stf = OLD_NRATE_STF_SISO;	/* SISO */
		else if (mcs_set) {
			if ((nrate & OLD_NRATE_RATE_MASK) <= HIGHEST_SINGLE_STREAM_MCS ||
			    (nrate & OLD_NRATE_RATE_MASK) == 32)
				stf = OLD_NRATE_STF_SISO;	/* SISO */
			else
				stf = OLD_NRATE_STF_SDM;	/* SDM */
		}
	}

	if (!legacy_set && !mcs_set) {
		fprintf(stderr, "%s: you need to set a legacy or mcs rate\n", fn_name);
		err = USAGE_ERROR;
		goto exit;
	}

	if (ioctl_version == 1) {
		if (legacy_set) {
			nrate = rate;
		} else {
			nrate = mcs;
			nrate |= OLD_NRATE_MCS_INUSE;
			if (mcs_only) {
				nrate |= OLD_NRATE_OVERRIDE_MCS_ONLY;
			}
		}

		nrate |= (stf << OLD_NRATE_STF_SHIFT) & OLD_NRATE_STF_MASK;
	} else {
		uint tx_exp = 0;

		/* set the ratespec encoding type and basic rate value */
		if (legacy_set) {
			nrate = WL_RSPEC_ENCODE_RATE;	/* 11abg */
			nrate |= rate;
		} else {
			nrate = WL_RSPEC_ENCODE_HT;	/* 11n HT */
			nrate |= mcs;
		}

		/* decode nrate stf value into tx expansion and STBC */
		if (stf == OLD_NRATE_STF_CDD) {
			tx_exp = 1;
		} else if (stf == OLD_NRATE_STF_STBC) {
			nrate |= WL_RSPEC_STBC;
		}

		nrate |= (tx_exp << WL_RSPEC_TXEXP_SHIFT);
	}

	err = wlu_iovar_setint(wl, "nrate", (int)nrate);

exit:
	return err;
}

#ifndef ATE_BUILD
static int
wl_assoc_info(void *wl, cmd_t *cmd, char **argv)
{
	uint i, req_ies_len = 0, resp_ies_len = 0;
	wl_assoc_info_t assoc_info;
	int ret;
	uint8 *pbuf;

	UNUSED_PARAMETER(argv);

	/* get the generic association information */
	strcpy(buf, cmd->name);
	if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_SMLEN)) < 0)
		return ret;

	memcpy(&assoc_info, buf, sizeof(wl_assoc_info_t));
	assoc_info.req_len = htod32(assoc_info.req_len);
	assoc_info.resp_len = htod32(assoc_info.resp_len);
	assoc_info.flags = htod32(assoc_info.flags);

	printf("Assoc req:\n");
	printf("\tlen 0x%x\n", assoc_info.req_len);
	if (assoc_info.req_len) {
		printf("\tcapab  0x%x\n", ltoh16(assoc_info.req.capability));
		printf("\tlisten 0x%x\n", ltoh16(assoc_info.req.listen));
		req_ies_len = assoc_info.req_len - sizeof(struct dot11_assoc_req);
		if (assoc_info.flags & WLC_ASSOC_REQ_IS_REASSOC) {
			printf("\treassoc bssid %s\n",
				wl_ether_etoa(&assoc_info.reassoc_bssid));
			req_ies_len -= ETHER_ADDR_LEN;
		}
	}

	/* get the association req IE's if there are any */
	if (req_ies_len) {
		strcpy(buf, "assoc_req_ies");
		if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_SMLEN)) < 0)
			return ret;

		printf("assoc req IEs:\n\t");
		for (i = 1, pbuf = (uint8*)buf; i <= req_ies_len; i++) {
			printf("0x%02x ", *pbuf++);
			if (!(i%8))
				printf("\n\t");
		}
	}

	printf("\nAssoc resp:\n");
	printf("\tlen 0x%x\n", assoc_info.resp_len);
	if (assoc_info.resp_len) {
		printf("\tcapab  0x%x\n", ltoh16(assoc_info.resp.capability));
		printf("\tstatus 0x%x\n", ltoh16(assoc_info.resp.status));
		printf("\taid    0x%x\n", ltoh16(assoc_info.resp.aid));
		resp_ies_len = assoc_info.resp_len - sizeof(struct dot11_assoc_resp);
	}

	/* get the association resp IE's if there are any */
	if (resp_ies_len) {
		strcpy(buf, "assoc_resp_ies");
		if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_SMLEN)) < 0)
			return ret;

		printf("assoc resp IEs:\n\t");
		for (i = 1, pbuf = (uint8*)buf; i <= resp_ies_len; i++) {
			printf(" 0x%02x ", *pbuf++);
			if (!(i%8))
				printf("\n\t");

		}
	}
	printf("\n");

	return 0;
}

static void dump_management_fields(uint8 *data, int len)
{
	int i, tag_len;
	uint8 tag;
	char temp[64];
	uint8 *p;

	while (len > 0) {
		/* Get the tag */
		tag = *data;
		data++; len--;

		/* Get the tag length */
		tag_len = (int) *data;
		data++; len--;

		printf("Tag:%d Len:%d - ", tag, tag_len);

		switch (tag) {
		case DOT11_MNG_SSID_ID:
			for (i = 0; i < tag_len; i++) {
				temp[i] = data[i];
			}
			if (i < 64) {
				temp[i] = '\0';
			}
			printf("SSID: '%s'\n", temp);
			break;
		case DOT11_MNG_FH_PARMS_ID:
			printf("FH Parameter Set\n");
			break;
		case DOT11_MNG_DS_PARMS_ID:
			printf("DS Parameter Set\n");
			break;
		case DOT11_MNG_CF_PARMS_ID:
			printf("CF Parameter Set\n");
			break;
		case DOT11_MNG_RATES_ID:
			printf("Supported Rates\n");
			break;
		case DOT11_MNG_TIM_ID:
			printf("Traffic Indication Map (TIM)\n");
			break;
		case DOT11_MNG_IBSS_PARMS_ID:
			printf("IBSS Parameter Set\n");
			break;
		case DOT11_MNG_COUNTRY_ID:
			p = data;
			printf("Country '%c%c%c'\n",
			       data[0], data[1], data[2]);
			p += DOT11_MNG_COUNTRY_ID_LEN;
			while (((data+tag_len) - p) >= DOT11_MNG_COUNTRY_ID_LEN) {
				printf("Start Channel: %d, Channels: %d, "
				       "Max TX Power: %d dBm\n",
				       p[0], p[1], p[2]);
				p += DOT11_MNG_COUNTRY_ID_LEN;
			}
			break;
		case DOT11_MNG_HOPPING_PARMS_ID:
			printf("Hopping Pattern Parameters\n");
			break;
		case DOT11_MNG_HOPPING_TABLE_ID:
			printf("Hopping Pattern Table\n");
			break;
		case DOT11_MNG_REQUEST_ID:
			printf("Request\n");
			break;
		case DOT11_MNG_QBSS_LOAD_ID:
			printf("QBSS Load\n");
			break;
		case DOT11_MNG_EDCA_PARAM_ID:
			printf("EDCA Parameter\n");
			break;
		case DOT11_MNG_CHALLENGE_ID:
			printf("Challenge text\n");
			break;
		case DOT11_MNG_PWR_CONSTRAINT_ID:
			printf("Power Constraint\n");
			break;
		case DOT11_MNG_PWR_CAP_ID:
			printf("Power Capability\n");
			break;
		case DOT11_MNG_TPC_REQUEST_ID:
			printf("Transmit Power Control (TPC) Request\n");
			break;
		case DOT11_MNG_TPC_REPORT_ID:
			printf("Transmit Power Control (TPC) Report\n");
			break;
		case DOT11_MNG_SUPP_CHANNELS_ID:
			printf("Supported Channels\n");
			break;
		case DOT11_MNG_CHANNEL_SWITCH_ID:
			printf("Channel Switch Announcement\n");
			break;
		case DOT11_MNG_MEASURE_REQUEST_ID:
			printf("Measurement Request\n");
			break;
		case DOT11_MNG_MEASURE_REPORT_ID:
			printf("Measurement Report\n");
			break;
		case DOT11_MNG_QUIET_ID:
			printf("Quiet\n");
			break;
		case DOT11_MNG_IBSS_DFS_ID:
			printf("IBSS DFS\n");
			break;
		case DOT11_MNG_ERP_ID:
			printf("ERP Information\n");
			break;
		case DOT11_MNG_TS_DELAY_ID:
			printf("TS Delay\n");
			break;
		case DOT11_MNG_HT_CAP:
			printf("HT Capabilities\n");
			break;
		case DOT11_MNG_QOS_CAP_ID:
			printf("QoS Capability\n");
			break;
		case DOT11_MNG_NONERP_ID:
			printf("NON-ERP\n");
			break;
		case DOT11_MNG_RSN_ID:
			printf("RSN\n");
			break;
		case DOT11_MNG_EXT_RATES_ID:
			printf("Extended Supported Rates\n");
			break;
		case DOT11_MNG_AP_CHREP_ID:
			printf("AP Channel Report\n");
			break;
		case DOT11_MNG_NBR_REP_ID:
			printf("Neighbor Report\n");
			break;
		case DOT11_MNG_MDIE_ID:
			printf("Mobility Domain\n");
			break;
		case DOT11_MNG_FTIE_ID:
			printf("Fast BSS Transition\n");
			break;
		case DOT11_MNG_FT_TI_ID:
			printf("802.11R Timeout Interval\n");
			break;
		case DOT11_MNG_REGCLASS_ID:
			printf("Regulatory Class\n");
			break;
		case DOT11_MNG_EXT_CSA_ID:
			printf("Extended CSA\n");
			break;
		case DOT11_MNG_HT_ADD:
			printf("HT Information\n");
			break;
		case DOT11_MNG_EXT_CHANNEL_OFFSET:
			printf("Ext Channel\n");
			break;
#ifdef BCMWAPI_WAI
		case DOT11_MNG_WAPI_ID:
			printf("WAPI\n");
			break;
#endif
		case DOT11_MNG_RRM_CAP_ID:
			printf("Radio Measurement\n");
			break;
		case DOT11_MNG_HT_BSS_COEXINFO_ID:
			printf("OBSS Coexistence INFO\n");
			break;
		case DOT11_MNG_HT_BSS_CHANNEL_REPORT_ID:
			printf("OBSS Intolerant Channel List\n");
			break;
		case DOT11_MNG_HT_OBSS_ID:
			printf("OBSS HT Info\n");
			break;
#ifdef DOT11_MNG_CHANNEL_USAGE
		case DOT11_MNG_CHANNEL_USAGE:
			printf("Channel Usage\n");
			break;
#endif
		case DOT11_MNG_LINK_IDENTIFIER_ID:
			printf("TDLS Link Identifier\n");
			break;
		case DOT11_MNG_WAKEUP_SCHEDULE_ID:
			printf("TDLS Wakeup Schedule\n");
			break;
		case DOT11_MNG_CHANNEL_SWITCH_TIMING_ID:
			printf("TDLS Channel Switch Timing\n");
			break;
		case DOT11_MNG_PTI_CONTROL_ID:
			printf("TDLS PTI Control\n");
			break;
		case DOT11_MNG_PU_BUFFER_STATUS_ID:
			printf("TDLS PU Buffer Status\n");
			break;
		case DOT11_MNG_EXT_CAP_ID:
			printf("Management Ext Capability\n");
			break;
		case DOT11_MNG_PROPR_ID:
			printf("Proprietary\n");
			break;
		default:
			if (tag_len <= len) {
				printf("Unsupported tag\n");
			} else {
				/* Just dump the remaining data */
				printf("Unsupported tag error/malformed\n");
				tag_len = len;
			}
			break;
		} /* switch */

		wl_hexdump(data, tag_len);

		data += tag_len;
		len -= tag_len;
	} /* while */
}

static void dump_management_info(uint8 *data, int len)
{
	struct dot11_management_header hdr;
	struct dot11_bcn_prb parms;

	if (len <= (int) (sizeof(hdr)+sizeof(parms))) {
		/* Management packet invalid */
		return;
	}

	memcpy(&hdr, data, sizeof(hdr));
	data += sizeof(hdr);
	len -= sizeof(hdr);

	memcpy(&parms, data, sizeof(parms));
	data += sizeof(parms);
	len -= sizeof(parms);

	/* 802.11 MAC header */
	printf("Frame Ctl: 0x%04x\n", ltoh16(hdr.fc));
	printf("Duration : 0x%04x\n", ltoh16(hdr.durid));
	printf("Dest addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
	       hdr.da.octet[0],
	       hdr.da.octet[1],
	       hdr.da.octet[2],
	       hdr.da.octet[3],
	       hdr.da.octet[4],
	       hdr.da.octet[5]);
	printf("Src addr : %02x:%02x:%02x:%02x:%02x:%02x\n",
	       hdr.sa.octet[0],
	       hdr.sa.octet[1],
	       hdr.sa.octet[2],
	       hdr.sa.octet[3],
	       hdr.sa.octet[4],
	       hdr.sa.octet[5]);
	printf("BSSID    : %02x:%02x:%02x:%02x:%02x:%02x\n",
	       hdr.bssid.octet[0],
	       hdr.bssid.octet[1],
	       hdr.bssid.octet[2],
	       hdr.bssid.octet[3],
	       hdr.bssid.octet[4],
	       hdr.bssid.octet[5]);
	printf("Seq ctl  : 0x%04x\n", hdr.seq);

	/* 802.11 management frame */
	printf("Timestamp: 0x%08x%08x\n",
	       ltoh32(parms.timestamp[0]), ltoh32(parms.timestamp[1]));
	printf("Beacon Interval: 0x%04x\n", ltoh16(parms.beacon_interval));
	printf("Capabilities: 0x%04x\n", ltoh32(parms.capability));

	dump_management_fields(data, len);
}

static int
wl_management_info(void *wl, cmd_t *cmd, char**argv)
{
	int ret = 0;
	int len;
	uint8 *data;
	FILE *fp = NULL;
	char *fname = NULL;
	int raw = 0;

	/* Skip the command name */
	argv++;

	while (*argv) {
		char *s = *argv;

		if (!strcmp(s, "-f") && argv[1] != NULL) {
			/* Write packet to a file */
			fname = argv[1];
			argv += 2;
		} else if (!strcmp(s, "-r")) {
			/* Do a hex dump to console */
			raw = 1;
			argv++;
		} else
			return -1;
	}

	/* Get the beacon information */
	strcpy(buf, cmd->name);
	if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return ret;

	/*
	 * Dump out the beacon data. The first word (4 bytes) is the
	 * length of the management packet followed by the data itself.
	 */
	len = dtoh32(*(int *)buf);

	if (len <= 0) {
		/* Nothing to do */
		return ret;
	}

	data = (uint8 *) (buf + sizeof(int));
	printf("Data: %p Len: %d bytes\n", data, len);

	if (fname != NULL) {
		/* Write the packet to a file */
		if ((fp = fopen(fname, "wb")) == NULL) {
			fprintf(stderr, "Failed to open file %s\n",
			        fname);
			ret = -1;
		} else {
			ret = fwrite(data, 1, len, fp);

			if (ret != len) {
				fprintf(stderr,
				        "Error write %d bytes to file %s, rc %d!\n",
				        len, fname, ret);
				ret = -1;
			}
		}
	} else if (raw) {
		/* Hex dump */
		wl_hexdump(data, len);
	} else {
		/* Print management (w/some decode) */
		dump_management_info(data, len);
	}

	if (fp) fclose(fp);

	return ret;
}

static int
wl_pmkid_info(void *wl, cmd_t *cmd, char**argv)
{
	int i, j, ret;
	pmkid_list_t *pmkid_info;

	if (!*++argv) {
		strcpy(buf, cmd->name);
		if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_SMLEN)) < 0)
			return ret;

		pmkid_info = (pmkid_list_t *)buf;
		pmkid_info->npmkid = dtoh32(pmkid_info->npmkid);
		printf("\npmkid entries : %d\n", pmkid_info->npmkid);

		for (i = 0; i < (int)pmkid_info->npmkid; i++) {
			printf("\tPMKID[%d]: %s =",
				i, wl_ether_etoa(&pmkid_info->pmkid[i].BSSID));
			for (j = 0; j < WPA2_PMKID_LEN; j++)
				printf("%02x ", pmkid_info->pmkid[i].PMKID[j]);
			printf("\n");
		}
	}
	else {
#ifdef test_pmkid_info
		char eaddr[6] = {0x0, 0x0, 0x1, 0x2, 0x3, 0x5};
		char eaddr1[6] = {0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
		char id[WPA2_PMKID_LEN], id1[WPA2_PMKID_LEN];
		int i, len = (sizeof(uint32) + 2*(sizeof(pmkid_t)));

		/* check that the set uses to "test" cmd */
		if (strcmp(*argv, "test")) {
			printf("\t wl pmkid_info only supports `test` a test specific set\n");
			return -1;
		}
		if ((pmkid_info = (pmkid_list_t *)malloc(len)) == NULL)	{
			printf("\tfailed to allocate buffer\n");
			return -1;
		}

		printf("\toverwriting pmkid table with test pattern\n");
		for (i = 0; i < (int)sizeof(id); i++) {
			id[i] = i;
			id1[i] = (i*2);
		}

		/* "test" - creates two PMKID entries and sets the table to that */
		pmkid_info->npmkid = htod32(2);
		memcpy(&pmkid_info->pmkid[0].BSSID.octet[0], &eaddr[0], ETHER_ADDR_LEN);
		memcpy(&pmkid_info->pmkid[0].PMKID[0], &id[0], WPA2_PMKID_LEN);
		memcpy(&pmkid_info->pmkid[1].BSSID.octet[0], &eaddr1[0], ETHER_ADDR_LEN);
		memcpy(&pmkid_info->pmkid[1].PMKID[0], &id1[0], WPA2_PMKID_LEN);

		ret = wlu_var_setbuf(wl, cmd->name, pmkid_info, len);

		free(pmkid_info);

		return ret;
#else
		printf("\tset cmd ignored\n");
#endif /* test_pmkid_info */
	}

	return 0;
}

static int
wl_rateset(void *wl, cmd_t *cmd, char **argv)
{
	wl_rateset_args_t rs, defrs;
	int error;
	uint i;

	UNUSED_PARAMETER(cmd);

	error = 0;

	argv++;

	if (*argv == NULL) {
		/* get current rateset */
		if ((error = wlu_iovar_get(wl, "cur_rateset", &rs, sizeof(rs))) < 0)
			return (error);

		dump_rateset(rs.rates, dtoh32(rs.count));
		printf("\n");
		wl_print_mcsset((char *)rs.mcs);
		wl_print_vhtmcsset((uint16 *)rs.vht_mcs);
	} else {
		/* get default rateset and mcsset */
		if ((error = wlu_iovar_get(wl, "rateset", &defrs,
		                           sizeof(wl_rateset_args_t))) < 0)
			return (error);
		defrs.count = dtoh32(defrs.count);

		if (!stricmp(*argv, "all")) {
			for (i = 0; i < defrs.count; i++)
				defrs.rates[i] |= 0x80;
			defrs.count = htod32(defrs.count);
			error = wlu_iovar_set(wl, "rateset", &defrs,
			                      sizeof(wl_rateset_args_t));
		}
		else if (!stricmp(*argv, "default")) {
			defrs.count = htod32(defrs.count);
			error = wlu_iovar_set(wl, "rateset", &defrs,
			                      sizeof(wl_rateset_args_t));
		}
		else {	/* arbitrary list */
			error = wl_parse_rateset(wl, &defrs, argv);
			if (!error) {

				/* check for common error of no basic rates */
				for (i = 0; i < defrs.count; i++) {
					if (defrs.rates[i] & 0x80)
						break;
				}
				if (i < defrs.count) {
					defrs.count = htod32(defrs.count);
					error = wlu_iovar_set(wl, "rateset", &defrs,
						sizeof(wl_rateset_args_t));
				} else {
					error = BAD_PARAM;
					fprintf(stderr,
					    "Bad Args: at least one rate must be marked Basic\n");
				}
			}
		}

	}
	return (error);
}

static int
wl_txbf_rateset(void *wl, cmd_t *cmd, char **argv)
{
	wl_txbf_rateset_t rs;
	int error = 0;

	argv++;

	if ((error = wlu_iovar_get(wl, cmd->name, &rs, sizeof(rs))) < 0)
		return (error);
	/* Get txbf rateset */
	if (*argv == NULL) {
		printf("    OFDM: ");
		dump_rateset(rs.txbf_rate_ofdm, dtoh32(rs.txbf_rate_ofdm_cnt));
		printf("\n");
		wl_print_txbf_mcsset((char *)rs.txbf_rate_mcs, "   ");
		wl_print_txbf_vhtmcsset((uint16 *)rs.txbf_rate_vht, "   ");

		printf("BCM OFDM: ");
		dump_rateset(rs.txbf_rate_ofdm_bcm, dtoh32(rs.txbf_rate_ofdm_cnt_bcm));
		printf("\n");
		wl_print_txbf_mcsset((char *)rs.txbf_rate_mcs_bcm, "BCM");
		wl_print_txbf_vhtmcsset((uint16 *)rs.txbf_rate_vht_bcm, "BCM");
	} else {	/* Set txbf rateset */
		error = wl_parse_txbf_rateset(&rs, argv);
		if (!error) {
			rs.txbf_rate_ofdm_cnt     = htod32(rs.txbf_rate_ofdm_cnt);
			rs.txbf_rate_ofdm_cnt_bcm = htod32(rs.txbf_rate_ofdm_cnt_bcm);
			error = wlu_iovar_set(wl, cmd->name, &rs, sizeof(rs));
		}
	}
	return (error);
}

static int
wl_sup_rateset(void *wl, cmd_t *cmd, char **argv)
{
	wl_rateset_args_t rs;
	bool got_basic;
	int error;
	uint i;

	error = 0;
	memset((char*)&rs, 0, sizeof(wl_rateset_args_t));

	argv++;

	if (*argv == NULL) {
		/* get rateset */
		if ((error = wlu_get(wl, cmd->get, &rs, sizeof(wl_rateset_t))) < 0)
			return (error);

		dump_rateset(rs.rates, dtoh32(rs.count));
		printf("\n");
	} else {
		if (!stricmp(*argv, "-1") || !stricmp(*argv, "0")) {
			/* set an empty rateset */
			error = wlu_set(wl, cmd->set, &rs, sizeof(wl_rateset_t));
		}
		else {	/* set the specified rateset */
			wl_parse_rateset(wl, &rs, argv);
			/* check for common error of including a basic rate */
			got_basic = FALSE;
			for (i = 0; i < rs.count; i++) {
				if (rs.rates[i] & 0x80) {
					rs.rates[i] &= 0x7F;
					got_basic = TRUE;
				}
			}
			if (got_basic) {
				fprintf(stderr,
				"Warning: Basic rate attribute ignored for \"%s\" command\n",
				cmd->name);
			}
			rs.count = htod32(rs.count);
			error = wlu_set(wl, cmd->set, &rs, sizeof(wl_rateset_t));
		}

	}
	return (error);
}

/*
 * Parse the rateset command arguments into the passed wl_rateset_args_t structure.
 *
 * Returns 0 on success, or an appropriate error code (USAGE_ERROR, BAD_PARAM).
 *
 */
static int
wl_parse_rateset(void *wl, wl_rateset_args_t* rs, char **argv)
{
	char* endp = NULL;
	char* arg;
	int r;
	int mcs_index = 0;
	uint32 mcs_mask;
	int error = 0;
	bool mcs_args, vht_args;
	wl_rateset_args_t cur_rs;

	mcs_args = vht_args = FALSE;

	memset(rs, 0, sizeof(*rs));

	while ((arg = *argv++) != NULL) {
		/* mcs rates */
		if (!stricmp(arg, "-m")) {
			mcs_args = TRUE;
			break;
		}

		/* vht rates */
		if (!stricmp(arg, "-v")) {
			vht_args = TRUE;
			break;
		}

		/* Parse legacy rates */

		if (rs->count >= WL_MAXRATES_IN_SET) {
			fprintf(stderr,
			"parsing \"%s\", too many rates specified, max is %d rates\n",
			arg, WL_MAXRATES_IN_SET);
			error = USAGE_ERROR;
			break;
		}

		/* convert the rate number to a 500kbps rate by multiplying by 2 */
		r = (int)(strtoul(arg, &endp, 0) * 2);
		if (endp == arg) {
			fprintf(stderr, "unable to convert the rate parameter \"%s\"\n", arg);
			error = USAGE_ERROR;
			break;
		}

		/* parse a .5 specially */
		if (!strncmp(endp, ".5", 2)) {
			r += 1;
			endp += 2;
		}

		/* strip trailing space */
		while (isspace((int)endp[0]))
			endp++;

		/* check for a basic rate specifier */
		if (!stricmp(endp, "b") || !stricmp(endp, "(b)")) {
			r |= 0x80;
		} else if (endp[0] != '\0') {
			fprintf(stderr,
				"unable to convert trailing characters"
				" \"%s\" in the rate parameter \"%s\"\n",
				endp, arg);
			error = BAD_PARAM;
			break;
		}

		/* no legacy rates specified */
		if ((rs->count == 0) && (r == 0)) {
			fprintf(stderr, "empty legacy rateset not supported\n");
			error = USAGE_ERROR;
			break;
		}

		rs->rates[rs->count++] = r;
	}

	if (error)
		return error;

	if (!mcs_args && !vht_args && !rs->count)
		return USAGE_ERROR; /* Cannot happen, really */

	/*
	 * If one of the rate sets was not specified, keep its current setting.
	 */

	error = wlu_iovar_get(wl, "cur_rateset", &cur_rs, sizeof(cur_rs));
	if (error)
		return error;

	if (!rs->count) { /* No legacy rates specified -- keep what we have */
		rs->count = cur_rs.count;
		memcpy(&rs->rates, &cur_rs.rates, rs->count);
	}

	if (!mcs_args) { /* No MCS rates specified */
		memcpy(rs->mcs, cur_rs.mcs, MCSSET_LEN);
	}

	if (!vht_args) { /* No VHT rates specified, keep current values */
		memcpy(rs->vht_mcs, cur_rs.vht_mcs,
		       VHT_CAP_MCS_MAP_NSS_MAX * sizeof(rs->vht_mcs[0]));
	}

	/* If no more args, return. */

	if (!arg) {
		return error;
	}

	/* Parse mcs or VHT rateset values */

	while ((arg = *argv++) != NULL) {

	    if (mcs_args) {

		    if (mcs_index >= MCSSET_LEN) {
			fprintf(stderr, "parsing \"%s\", too many mcs rates "
			        "specified, max is %d rates\n", arg, MCSSET_LEN);
			error = USAGE_ERROR;
			break;
		}

		mcs_mask = strtoul(arg, &endp, 16);

		if (endp == arg) {
			fprintf(stderr, "unable to convert the mcs parameter \"%s\"\n", arg);
			error = BAD_PARAM;
			break;
		}

		/* clear the mcs rates */
		if (mcs_mask == 0) {
			memset(rs->mcs, 0, MCSSET_LEN);
			break;
		}

		/* copy the mcs rates bitmap octets */
		rs->mcs[mcs_index++] = mcs_mask;

	    } else { /* vht_args */

		    /*
		     * Specified as rate masks for Nss=0, Nss=1, etc.
		     */
		    if (mcs_index >= VHT_CAP_MCS_MAP_NSS_MAX) {
			fprintf(stderr,
				"Error: Too many VHT rate masks specified, max %d\n",
				VHT_CAP_MCS_MAP_NSS_MAX);
			error = USAGE_ERROR;
			break;
		    }

		    mcs_mask = strtoul( arg, &endp, 16 ); /* Base 16 for consistency with -m */

		    if ((*arg == '\0') || (*endp != '\0')) {
			    fprintf(stderr, "Error converting VHT rate mask value '%s'\n", arg);
			    error = BAD_PARAM;
			    break;
		    }

		    /*
		     * Can only specify 0, 0xff, 0x1ff, 0x3ff because of the way the rates
		     * are encoded in the driver (0-3).
		     */
		    if ((mcs_mask != 0x0000) &&   /* vht disabled */
			(mcs_mask != 0x00ff) &&   /* vht mcs0-7 */
			(mcs_mask != 0x01ff) &&   /* vht mcs0-8 */
			(mcs_mask != 0x03ff)) {   /* vht mcs0-9 */
			    fprintf(stderr, "Error: VHT rate mask must be 0 (disabled),"
				" 0xff (MCS0-7), 0x1ff (MCS0-8), or 0x3ff (MCS0-9).\n");
			    error = BAD_PARAM;
			    break;
		    }

		    rs->vht_mcs[mcs_index++] = mcs_mask;
	       }
	}

	return error;
}

static int
wl_parse_txbf_rateset(wl_txbf_rateset_t *rs, char **argv)
{
#define TXBF_RATE_OFDM	0
#define TXBF_RATE_MCS	1
#define TXBF_RATE_VHT	2
	char* endp = NULL;
	char* arg;
	int i;
	int error = 0;
	int rate_type = TXBF_RATE_OFDM;
	uint8	txbf_rate_mcs[TXBF_RATE_MCS_ALL];
	uint8	txbf_rate_mcs_cnt = 0;
	uint8	mcs_mask;
	uint16	txbf_rate_vht[TXBF_RATE_VHT_ALL];
	uint8	txbf_rate_vht_cnt = 0;
	uint16	vht_mask;
	uint8	txbf_rate_ofdm[TXBF_RATE_OFDM_ALL];
	uint8	txbf_rate_ofdm_cnt;
	int r;
	bool specified_ofdm = FALSE;
	bool specified_mcs  = FALSE;
	bool specified_vht  = FALSE;
	bool specified_bcm  = FALSE;

	/* First see if ofdm list */
	arg = *argv;
	if (stricmp(arg, "-m") != 0 && stricmp(arg, "-v") != 0) {
		rate_type = TXBF_RATE_OFDM;
		specified_ofdm = TRUE;
		txbf_rate_ofdm_cnt = 0;
	}

	while ((arg = *argv++) != NULL) {
		/* mcs rates */
		if (!stricmp(arg, "-m")) {
			rate_type = TXBF_RATE_MCS;
			specified_mcs = TRUE;
			txbf_rate_mcs_cnt = 0;
			continue;
		}
		/* vht rates */
		if (!stricmp(arg, "-v")) {
			rate_type = TXBF_RATE_VHT;
			specified_vht = TRUE;
			txbf_rate_vht_cnt = 0;
			continue;
		}
		/* Broadcom-to-Broadcom group */
		if (!stricmp(arg, "-b")) {
			specified_bcm = TRUE;
			continue;
		}

		if (rate_type == TXBF_RATE_OFDM) {
			/* Check if too many specified ofdm rates */
			if (txbf_rate_ofdm_cnt >= TXBF_RATE_OFDM_ALL) {
				fprintf(stderr, "ERR: more than max. of %d ofdm rates\n",
					TXBF_RATE_OFDM_ALL);
				return (USAGE_ERROR);
			}
			/* Convert user typed number to a 500kbps rate by multiplying by 2 */
			r = (int)(strtoul(arg, &endp, 0) * 2);
			if (endp == arg) {
				fprintf(stderr, "ERR: failed to convert %s\n", arg);
				return (USAGE_ERROR);
			}
			/* Check if valid ofdm rate */
			for (i = 0; i < (int)sizeof(ofdm_rates); i++) {
				/* match: it is a valid ofdm rate */
				if (r == (ofdm_rates[i] & OFDM_RATE_MASK)) {
					r = ofdm_rates[i]; /* will pick up basic rate bit too */
					break;
				}
			}
			if (i >= (int)sizeof(ofdm_rates)) {
				fprintf(stderr, "ERR: %s is an invalid ofdm rate\n", arg);
				return (USAGE_ERROR);
			}

			/* Strip trailing space */
			while (isspace((int)endp[0]))
				endp++;
			/* No ofdm rate specified */
			if ((rs->txbf_rate_ofdm_cnt == 0) && (r == 0)) {
				fprintf(stderr, "ERR: no ofdm rate specified\n");
				return (USAGE_ERROR);
			}
			txbf_rate_ofdm[txbf_rate_ofdm_cnt++] = r;
		} else if (rate_type == TXBF_RATE_MCS) {
			/* Check if exceeding max */
			if (txbf_rate_mcs_cnt >= TXBF_RATE_MCS_ALL) {
				fprintf(stderr, "ERR: exceed max. %d bitmask bytes; parsing %s\n",
					TXBF_RATE_MCS_ALL, arg);
				return (BAD_PARAM);
			}
			mcs_mask = (uint8)strtoul(arg, &endp, 16);
			if (endp == arg) {
				fprintf(stderr, "ERR: failed to convert %s\n", arg);
				return (USAGE_ERROR);
			}
			txbf_rate_mcs[txbf_rate_mcs_cnt++] = mcs_mask;
		} else if (rate_type == TXBF_RATE_VHT) {
			/* Check if exceeding max */
			if (txbf_rate_vht_cnt >= TXBF_RATE_VHT_ALL) {
				fprintf(stderr, "ERR: exceed max. %d bitmasks; parsing %s\n",
					TXBF_RATE_VHT_ALL, arg);
				return (BAD_PARAM);
			}
			vht_mask = (uint16)strtoul(arg, &endp, 16);
			if (endp == arg) {
				fprintf(stderr, "ERR: failed to convert %s\n", arg);
				return (USAGE_ERROR);
			}

			/* Validate bitmask value */
			if (vht_mask > 0x03ff) {
				fprintf(stderr, "ERR: vht bitmask must be 0 (disabled)"
						" or up to a maximum of 3ff (MCS0-9).\n");
				return (BAD_PARAM);
			}
			txbf_rate_vht[txbf_rate_vht_cnt++] = vht_mask;
		}

	} /* while */

	if (specified_mcs && !specified_bcm) {
		for (i = 0; i < txbf_rate_mcs_cnt; i++)
			rs->txbf_rate_mcs[i] = txbf_rate_mcs[i];
		if (txbf_rate_mcs_cnt)
			for (i = txbf_rate_mcs_cnt; i < TXBF_RATE_MCS_ALL; i++)	/* clear trailer */
				rs->txbf_rate_mcs[i] = 0;
	}

	if (specified_vht && !specified_bcm) {
		for (i = 0; i < txbf_rate_vht_cnt; i++)
			rs->txbf_rate_vht[i] = txbf_rate_vht[i];
		if (txbf_rate_vht_cnt)
			for (i = txbf_rate_vht_cnt; i < TXBF_RATE_VHT_ALL; i++)	/* clear trailer */
				rs->txbf_rate_vht[i] = 0;
	}

	if (specified_ofdm && !specified_bcm) {
		for (i = 0; i < txbf_rate_ofdm_cnt; i++)
			rs->txbf_rate_ofdm[i] = txbf_rate_ofdm[i];
		rs->txbf_rate_ofdm_cnt = txbf_rate_ofdm_cnt;
	}

	if (specified_mcs && specified_bcm) {
		for (i = 0; i < txbf_rate_mcs_cnt; i++)
			rs->txbf_rate_mcs_bcm[i] = txbf_rate_mcs[i];
		if (txbf_rate_mcs_cnt)
			for (i = txbf_rate_mcs_cnt; i < TXBF_RATE_MCS_ALL; i++)	/* clear trailer */
				rs->txbf_rate_mcs_bcm[i] = 0;
	}

	if (specified_vht && specified_bcm) {
		for (i = 0; i < txbf_rate_vht_cnt; i++)
			rs->txbf_rate_vht_bcm[i] = txbf_rate_vht[i];
		if (txbf_rate_vht_cnt)
			for (i = txbf_rate_vht_cnt; i < TXBF_RATE_VHT_ALL; i++)	/* clear trailer */
				rs->txbf_rate_vht_bcm[i] = 0;
	}

	if (specified_ofdm && specified_bcm) {
		for (i = 0; i < txbf_rate_ofdm_cnt; i++)
			rs->txbf_rate_ofdm_bcm[i] = txbf_rate_ofdm[i];
		rs->txbf_rate_ofdm_cnt_bcm = txbf_rate_ofdm_cnt;
	}

	return (error);
}

/*
 * Get or Set LPC Params
 *	wl lpc_params \
 *		<rate_stab_thresh> <pwr_stab_thresh> <lpc_exp_time> <pwrup_slow_step>
 *		<pwrup_fast_step> <pwrdn_slow_step>
 */
static int
wl_lpc_params(void *wl, cmd_t *cmd, char **argv)
{
	int err, argc;
	lpc_params_t lpc_params;

	UNUSED_PARAMETER(cmd);

	argv++;

	if (*argv == NULL) {
		/* get current LPC params */
		if ((err = wlu_iovar_get(wl, cmd->name, (void *) &lpc_params,
			(sizeof(lpc_params_t)))) < 0)
			return (err);

		printf("- Link Power Control parameters -\n");
		printf("rate_stab_thresh\t= %d\n", lpc_params.rate_stab_thresh);
		printf("pwr_stab_thresh\t\t= %d\nlpc_exp_time\t\t= %d\n",
			lpc_params.pwr_stab_thresh, lpc_params.lpc_exp_time);
		printf("pwrup_slow_step\t\t= %d\npwrup_fast_step\t\t= %d\n",
			lpc_params.pwrup_slow_step, lpc_params.pwrup_fast_step);
		printf("pwrdn_slow_step\t\t= %d\n",	lpc_params.pwrdn_slow_step);
	} else {
		char *endptr;
		/* Validate num of entries */
		for (argc = 0; argv[argc]; argc++);
		if (argc != 6)
			return USAGE_ERROR;

		argc = 0;
		lpc_params.rate_stab_thresh = strtol(argv[argc], &endptr, 0);
		argc++;
		lpc_params.pwr_stab_thresh = strtol(argv[argc], &endptr, 0);
		argc++;
		lpc_params.lpc_exp_time = strtol(argv[argc], &endptr, 0);
		argc++;
		lpc_params.pwrup_slow_step = strtol(argv[argc], &endptr, 0);
		argc++;
		lpc_params.pwrup_fast_step = strtol(argv[argc], &endptr, 0);
		argc++;
		lpc_params.pwrdn_slow_step = strtol(argv[argc], &endptr, 0);

		/* Set LPC params */
		err = wlu_iovar_set(wl, cmd->name, (void *) &lpc_params,
			(sizeof(lpc_params_t)));
	}

	return err;
}
#endif /* !ATE_BUILD */

/* Set per-band bandwidth */
static int wl_bw_cap(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	struct {
		uint32 band;
		uint32 bw_cap;
	} param = { 0, 0 };
	char *s = NULL;
	void *ptr = NULL;

	/* Skip the command name */
	argv++;

	if (*argv) {
		if (!strcmp(*argv, "a") || !strcmp(*argv, "5") || !strcmp(*argv, "5g")) {
			param.band = WLC_BAND_5G;
		} else if (!strcmp(*argv, "b") || !strcmp(*argv, "2") || !strcmp(*argv, "2g")) {
			param.band = WLC_BAND_2G;
		} else {
			fprintf(stderr,
			        "%s: invalid band %s\n",
			        cmd->name, *argv);
			err = -1;
			goto exit;
		}

		argv++;

		if (*argv) {
			/* Optional 2nd arg is used to set the bandwidth cap */
			s = NULL;

			param.bw_cap = (uint32) strtoul(*argv, &s, 0);
			if (s && *s != '\0') {
				fprintf(stderr, "%s: invalid bandwidth '%s'\n",
				        cmd->name, *argv);
				err = -1;
				goto exit;
			}
		}
	} else {
		fprintf(stderr, "%s: band unspecified\n", cmd->name);
		err = -1;
		goto exit;
	}

	if (param.bw_cap == 0) {
		if ((err = wlu_var_getbuf(wl, cmd->name, &param, sizeof(param), &ptr)) < 0)
			return err;

		printf("0x%x\n", *((uint32 *)ptr));
	} else {
		err = wlu_var_setbuf(wl, cmd->name, &param, sizeof(param));
	}

exit:
	return err;
}

static int
wl_channel(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	channel_info_t ci;

	if (!*++argv) {
		memset(&ci, 0, sizeof(ci));
		if ((ret = wlu_get(wl, cmd->get, &ci, sizeof(channel_info_t))) < 0)
			return ret;
		ci.hw_channel = dtoh32(ci.hw_channel);
		ci.scan_channel = dtoh32(ci.scan_channel);
		ci.target_channel = dtoh32(ci.target_channel);
		if (ci.scan_channel) {
			printf("Scan in progress.\n");
			printf("current scan channel\t%d\n", ci.scan_channel);
		} else {
			printf("No scan in progress.\n");
		}
		printf("current mac channel\t%d\n", ci.hw_channel);
		printf("target channel\t%d\n", ci.target_channel);
		return 0;
	} else {
		ci.target_channel = htod32(atoi(*argv));
		ret =  wlu_set(wl, cmd->set, &ci.target_channel, sizeof(int));
		return ret;
	}
}

#ifndef ATE_BUILD
static int
wl_chanspec(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t to;
	const char* fn_name = "wl_chanspec";
	bool band_set = FALSE, ch_set = FALSE, bw_set = FALSE, ctl_sb_set = FALSE;
	int err, opt_err;
	uint32 val = 0;
	chanspec_t chanspec = 0;

	/* toss the command name */
	argv++;

	if (!*argv) {
		if (cmd->get < 0)
			return -1;
		if ((err = wlu_iovar_getint(wl, cmd->name, (int*)&val)) < 0)
			return err;

		chanspec = wl_chspec32_from_driver(val);
		wf_chspec_ntoa(chanspec, buf);
		printf("%s (0x%x)\n", buf, chanspec);
		return 0;
	}


	chanspec = wf_chspec_aton(*argv);
	if (chanspec != 0) {
		val = wl_chspec32_to_driver(chanspec);
		if (val != INVCHANSPEC) {
			err = wlu_iovar_setint(wl, cmd->name, val);
		} else {
			err = USAGE_ERROR;
		}
	} else {
		miniopt_init(&to, fn_name, NULL, FALSE);
		while ((opt_err = miniopt(&to, argv)) != -1) {
			if (opt_err == 1) {
				err = -1;
				goto exit;
			}
			argv += to.consumed;

			if (to.opt == 'c') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for"
						" the channel\n", fn_name, to.valstr);
					err = -1;
					goto exit;
				}
				if (to.val > 224) {
					fprintf(stderr, "%s: invalid channel %d\n",
						fn_name, to.val);
					err = -1;
					goto exit;
				}
				chanspec |= to.val;
				ch_set = TRUE;
			}
			if (to.opt == 'b') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for band\n",
						fn_name, to.valstr);
					err = -1;
					goto exit;
				}
				if ((to.val != 5) && (to.val != 2)) {
					fprintf(stderr,
						"%s: invalid band %d\n",
						fn_name, to.val);
					err = -1;
					goto exit;
				}
				if (to.val == 5)
					chanspec |= WL_CHANSPEC_BAND_5G;
				else
					chanspec |= WL_CHANSPEC_BAND_2G;
				band_set = TRUE;
			}
			if (to.opt == 'w') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for"
						" bandwidth\n", fn_name, to.valstr);
					err = -1;
					goto exit;
				}
				if ((to.val != 20) && (to.val != 40)) {
					fprintf(stderr,
						"%s: invalid bandwidth %d\n",
						fn_name, to.val);
					err = -1;
					goto exit;
				}
				if (to.val == 20)
					chanspec |= WL_CHANSPEC_BW_20;
				else
					chanspec |= WL_CHANSPEC_BW_40;
				bw_set = TRUE;
			}
			if (to.opt == 's') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for"
						" ctl sideband\n", fn_name, to.valstr);
					err = -1;
					goto exit;
				}
				if ((to.val != 1) && (to.val != 0) && (to.val != -1)) {
					fprintf(stderr,
						"%s: invalid ctl sideband %d\n",
						fn_name, to.val);
					err = -1;
					goto exit;
				}
				if (to.val == -1)
					chanspec |= WL_CHANSPEC_CTL_SB_LOWER;
				else if (to.val == 1)
					chanspec |= WL_CHANSPEC_CTL_SB_UPPER;
				ctl_sb_set = TRUE;
			}
		}

		/* set ctl sb to 20 if not set and 20mhz is selected */
		if (!ctl_sb_set && CHSPEC_IS20(chanspec)) {
			ctl_sb_set = TRUE;
		}

		if (ch_set && band_set && bw_set && ctl_sb_set) {
			val = wl_chspec32_to_driver(chanspec);
			if (val != INVCHANSPEC) {
				err = wlu_iovar_setint(wl, cmd->name, val);
			} else {
				err = USAGE_ERROR;
			}
		} else {
			if (!ch_set)
				fprintf(stderr, "%s: you need to set a channel,"
					" '-c <ch>'\n", fn_name);
			if (!band_set)
				fprintf(stderr, "%s: you need to set a band,"
					" '-b <5|2>'\n", fn_name);
			if (!bw_set)
				fprintf(stderr, "%s: you need to set a bandwidth,"
					" '-w <20|40>'\n", fn_name);
			if (!ctl_sb_set)
				fprintf(stderr, "%s: you need to set a ctl sideband,"
					  " '-s <-1|0|1>'\n", fn_name);
			err = -1;
		}
	}

	if (!err)
		printf("Chanspec set to 0x%x\n", chanspec);

exit:
	return err;
}

static int
wl_chanim_state(void *wl, cmd_t *cmd, char **argv)
{
	uint32 chanspec;
	int argc = 0;
	int ret, val;

	argv++;

	/* find the arg count */
	while (argv[argc])
		argc++;

	if (argc != 1)
		return USAGE_ERROR;

	chanspec = wf_chspec_aton(*argv);
	chanspec = wl_chspec32_to_driver(chanspec);
	if (chanspec == INVCHANSPEC) {
		return USAGE_ERROR;
	}

	ret = wlu_iovar_getbuf(wl, cmd->name, &chanspec, sizeof(chanspec),
	                       buf, WLC_IOCTL_SMLEN);
	if (ret < 0)
		return ret;
	val = *(int*)buf;
	val = dtoh32(val);

	printf("%d\n", val);
	return 0;
}

static int
wl_chanim_mode(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char *endptr;
	int mode;

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;
		if ((ret = wlu_iovar_getint(wl, cmd->name, &mode)) < 0)
			return ret;

		switch (mode) {
		case CHANIM_DISABLE:
			printf("CHANIM mode: disabled.\n");
			break;
		case CHANIM_DETECT:
			printf("CHANIM mode: detect only.\n");
			break;
		case CHANIM_EXT:
			printf("CHANIM mode: external (acsd).\n");
			break;
		case CHANIM_ACT:
			printf("CHANIM mode: detect + act.\n");
			break;
		}
		return 0;
	} else {
		mode = CHANIM_DETECT;
		val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0')
			return (-1);

		switch (val) {
			case 0:
				mode = CHANIM_DISABLE;
				break;
			case 1:
				mode = CHANIM_DETECT;
				break;
			case 2:
				mode = CHANIM_EXT;
				break;
			case 3:
				mode = CHANIM_ACT;
				break;
			default:
				return (-1);
		}

		mode = htod32(mode);
		return wlu_iovar_setint(wl, cmd->name, mode);
	}
}
#endif /* !ATE_BUILD */

int
wl_ether_atoe(const char *a, struct ether_addr *n)
{
	char *c = NULL;
	int i = 0;

	memset(n, 0, ETHER_ADDR_LEN);
	for (;;) {
		n->octet[i++] = (uint8)strtoul(a, &c, 16);
		if (!*c++ || i == ETHER_ADDR_LEN)
			break;
		a = c;
	}
	return (i == ETHER_ADDR_LEN);
}
#ifndef ATE_BUILD
char *
wl_ether_etoa(const struct ether_addr *n)
{
	static char etoa_buf[ETHER_ADDR_LEN * 3];
	char *c = etoa_buf;
	int i;

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		if (i)
			*c++ = ':';
		c += sprintf(c, "%02X", n->octet[i] & 0xff);
	}
	return etoa_buf;
}

int
wl_atoip(const char *a, struct ipv4_addr *n)
{
	char *c = NULL;
	int i = 0;

	for (;;) {
		n->addr[i++] = (uint8)strtoul(a, &c, 0);
		if (*c++ != '.' || i == IPV4_ADDR_LEN)
			break;
		a = c;
	}
	return (i == IPV4_ADDR_LEN);
}

int
wl_atoipv6(const char *a, struct ipv6_addr *n)
{
	char *c = NULL;
	int i = 0;
	uint16 *addr16;
	for (;;) {
		addr16 = (uint16 *)&n->addr[i];
		*addr16 = hton16((uint16)strtoul((char *)a, &c, 16));
		i += 2;
		if (*c++ != ':' || i == IPV6_ADDR_LEN)
			break;
		a = c;
	}

	return (i == IPV6_ADDR_LEN);
}

char *
wl_iptoa(const struct ipv4_addr *n)
{
	static char iptoa_buf[IPV4_ADDR_LEN * 4];

	sprintf(iptoa_buf, "%u.%u.%u.%u",
	        n->addr[0], n->addr[1], n->addr[2], n->addr[3]);

	return iptoa_buf;
}

int
wl_format_ssid(char* ssid_buf, uint8* ssid, int ssid_len)
{
	int i, c;
	char *p = ssid_buf;

	if (ssid_len > 32) ssid_len = 32;

	for (i = 0; i < ssid_len; i++) {
		c = (int)ssid[i];
		if (c == '\\') {
			*p++ = '\\';
			*p++ = '\\';
		} else if (isprint((uchar)c)) {
			*p++ = (char)c;
		} else {
			p += sprintf(p, "\\x%02X", c);
		}
	}
	*p = '\0';

	return p - ssid_buf;
}

/* pretty hex print a contiguous buffer */
void
wl_hexdump(uchar *dump_buf, uint nbytes)
{
	char line[256];
	char* p;
	uint i;

	if (nbytes == 0) {
		printf("\n");
		return;
	}

	p = line;
	for (i = 0; i < nbytes; i++) {
		if (i % 16 == 0 && nbytes > 16) {
			p += sprintf(p, "%04d: ", i);	/* line prefix */
		}
		p += sprintf(p, "%02x ", dump_buf[i]);
		if (i % 16 == 15) {
			printf("%s\n", line);		/* flush line */
			p = line;
		}
	}

	/* flush last partial line */
	if (p != line)
		printf("%s\n", line);
}

static int
wl_plcphdr(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;

	if (!*++argv) {
		if ((ret = wlu_get(wl, cmd->get, &val, sizeof(int))) < 0)
			return ret;
		val = dtoh32(val);
		if (val == WLC_PLCP_AUTO)
			printf("long");
		else if (val == WLC_PLCP_SHORT)
			printf("auto");
		else if (val == WLC_PLCP_LONG)
			printf("debug");
		else
			printf("unknown");
		printf("\n");
		return 0;
	} else {
		if (!stricmp(*argv, "long"))
			val = WLC_PLCP_AUTO;
		else if (!stricmp(*argv, "auto"))
			val = WLC_PLCP_SHORT;
		else if (!stricmp(*argv, "debug"))
			val = WLC_PLCP_LONG;
		else
			return -1;
		val = htod32(val);
		return wlu_set(wl, cmd->set, &val, sizeof(int));
	}
}

/* WLC_GET_RADIO and WLC_SET_RADIO in driver operate on radio_disabled which
 * is opposite of "wl radio [1|0]".  So invert for user.
 * In addition, display WL_RADIO_SW_DISABLE and WL_RADIO_HW_DISABLE bits.
 */
static int
wl_radio(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	uint val;
	char *endptr = NULL;

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;
		if ((ret = wlu_get(wl, cmd->get, &val, sizeof(int))) < 0)
			return ret;
		val = dtoh32(val);
		printf("0x%04x\n", val);
		return 0;
	} else {
		if (cmd->set < 0)
			return -1;
		if (!stricmp(*argv, "on"))
			val = WL_RADIO_SW_DISABLE << 16;
		else if (!stricmp(*argv, "off"))
			val = WL_RADIO_SW_DISABLE << 16 | WL_RADIO_SW_DISABLE;
		else {
			val = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return -1;
			}

			/* raw bits setting, add the mask if not provided */
			if ((val >> 16) == 0) {
				val |= val << 16;
			}
		}
		val = htod32(val);
		return wlu_set(wl, cmd->set, &val, sizeof(int));
	}
}
#endif /* !ATE_BUILD */

static char *
ver2str(unsigned int vms, unsigned int vls)
{
	static char verstr[100];
	unsigned int maj, year, month, day, build;

	maj = (vms >> 16) & 0xFFFF;
	if (maj > 1000) {
		/* it is probably a date... */
		year = (vms >> 16) & 0xFFFF;
		month = vms & 0xFFFF;
		day = (vls >> 16) & 0xFFFF;
		build = vls & 0xFFFF;
		sprintf(verstr, "%d/%d/%d build %d",
			month, day, year, build);
	} else {
		/* it is a tagged release. */
		sprintf(verstr, "%d.%d RC%d.%d",
			(vms>>16)&0xFFFF, vms&0xFFFF,
			(vls>>16)&0xFFFF, vls&0xFFFF);
	}
	return verstr;
}


static int
wl_version(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int bcmerr = 0;
	char *p = NULL;
	char *dump_buf;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	printf("%s\n",
		ver2str(((EPI_MAJOR_VERSION) << 16) | EPI_MINOR_VERSION,
		(EPI_RC_NUMBER << 16) | EPI_INCREMENTAL_NUMBER));
	dump_buf = malloc(WLC_IOCTL_SMLEN);
	if (dump_buf == NULL) {
		fprintf(stderr, "Failed to allocate dump buffer of %d bytes\n", WLC_IOCTL_SMLEN);
		return -1;
	}
	memset(dump_buf, 0, WLC_IOCTL_SMLEN);

	/* query for 'ver' to get version info */
	ret = wlu_iovar_get(wl, "ver", dump_buf, WLC_IOCTL_SMLEN);

	/* if the query is successful, continue on and print the result. */

	/* if the query fails, check for a legacy driver that does not support
	 * the "dump" iovar, and instead issue a WLC_DUMP ioctl.
	 */
	if (ret) {
		wlu_iovar_getint(wl, "bcmerror", &bcmerr);
		if (bcmerr == BCME_UNSUPPORTED) {
			ret = wlu_get(wl, WLC_DUMP, dump_buf, WLC_IOCTL_SMLEN);
		}
	}

	if (ret) {
		fprintf(stderr, "Error %d on query of driver dump\n", (int)ret);
		free(dump_buf);
		return IOCTL_ERROR;
	}

	/* keep only the first line from the dump buf output */
	p = strchr(dump_buf, '\n');
	if (p)
		*p = '\0';
	printf("%s\n", dump_buf);

	free(dump_buf);

	return 0;
}

#ifndef ATE_BUILD
static int
wl_rateparam(void *wl, cmd_t *cmd, char **argv)
{
	int val[2];

	if (!*++argv)
		return -1;
	val[0] = htod32(atoi(*argv));
	if (!*++argv)
		return -1;
	val[1] = htod32(atoi(*argv));
	return wlu_set(wl, cmd->set, val, 2 * sizeof(val));
}

/* wl scan
 * -s --ssid=ssid_list
 * -t T --scan_type=T : [active|passive]
 * --bss_type=T : [infra|bss|adhoc|ibss]
 * -b --bssid=
 * -n --nprobes=
 * -a --active=
 * -p --passive=
 * -h --home=
 * -c --channels=
 * ssid_list
 */

/* Parse a comma-separated list from list_str into ssid array, starting
 * at index idx.  Max specifies size of the ssid array.  Parses ssids
 * and returns updated idx; if idx >= max not all fit, the excess have
 * not been copied.  Returns -1 on empty string, or on ssid too long.
 */
static int
wl_parse_ssid_list(char* list_str, wlc_ssid_t* ssid, int idx, int max)
{
	char *str, *ptr;

	if (list_str == NULL)
		return -1;

	for (str = list_str; str != NULL; str = ptr) {
		if ((ptr = strchr(str, ',')) != NULL)
			*ptr++ = '\0';

		if (strlen(str) > DOT11_MAX_SSID_LEN) {
			fprintf(stderr, "ssid <%s> exceeds %d\n", str, DOT11_MAX_SSID_LEN);
			return -1;
		}
		if (strlen(str) == 0)
			ssid[idx].SSID_len = 0;

		if (idx < max) {
			strcpy((char*)ssid[idx].SSID, str);
			ssid[idx].SSID_len = strlen(str);
		}
		idx++;
	}

	return idx;
}

static int
wl_scan_prep(void *wl, cmd_t *cmd, char **argv, wl_scan_params_t *params, int *params_size)
{
	int val = 0;
	char key[64];
	int keylen;
	char *p, *eq, *valstr, *endptr = NULL;
	char opt;
	bool positional_param;
	bool good_int;
	bool opt_end;
	int err = 0;
	int i;

	int nchan = 0;
	int nssid = 0;
	wlc_ssid_t ssids[WL_SCAN_PARAMS_SSID_MAX];

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->bss_type = DOT11_BSSTYPE_ANY;
	params->scan_type = 0;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;
	memset(ssids, 0, WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t));

	/* skip the command name */
	argv++;

	opt_end = FALSE;
	while ((p = *argv) != NULL) {
		argv++;
		positional_param = FALSE;
		memset(key, 0, sizeof(key));
		opt = '\0';
		valstr = NULL;
		good_int = FALSE;

		if (opt_end) {
			positional_param = TRUE;
			valstr = p;
		}
		else if (!strcmp(p, "--")) {
			opt_end = TRUE;
			continue;
		}
		else if (!strncmp(p, "--", 2)) {
			eq = strchr(p, '=');
			if (eq == NULL) {
				fprintf(stderr,
				"wl_scan: missing \" = \" in long param \"%s\"\n", p);
				err = -1;
				goto exit;
			}
			keylen = eq - (p + 2);
			if (keylen > 63) keylen = 63;
			memcpy(key, p + 2, keylen);

			valstr = eq + 1;
			if (*valstr == '\0') {
				fprintf(stderr,
				"wl_scan: missing value after \" = \" in long param \"%s\"\n", p);
				err = -1;
				goto exit;
			}
		}
		else if (!strncmp(p, "-", 1)) {
			opt = p[1];
			if (strlen(p) > 2) {
				fprintf(stderr,
				"wl_scan: only single char options, error on param \"%s\"\n", p);
				err = -1;
				goto exit;
			}
			if (*argv == NULL) {
				fprintf(stderr,
				"wl_scan: missing value parameter after \"%s\"\n", p);
				err = -1;
				goto exit;
			}
			valstr = *argv;
			argv++;
		} else {
			positional_param = TRUE;
			valstr = p;
		}

		/* parse valstr as int just in case */
		if (valstr) {
			val = (int)strtol(valstr, &endptr, 0);
			if (*endptr == '\0') {
				/* not all the value string was parsed by strtol */
				good_int = TRUE;
			}
		}

		if (opt == 's' || !strcmp(key, "ssid") || positional_param) {
			nssid = wl_parse_ssid_list(valstr, ssids, nssid, WL_SCAN_PARAMS_SSID_MAX);
			if (nssid < 0) {
				err = -1;
				goto exit;
			}
		}
		if (opt == 't' || !strcmp(key, "scan_type")) {
			if (!strcmp(valstr, "active")) {
				params->scan_type = 0;
			} else if (!strcmp(valstr, "passive")) {
				params->scan_type = WL_SCANFLAGS_PASSIVE;
			} else if (!strcmp(valstr, "prohibit")) {
				params->scan_type = WL_SCANFLAGS_PROHIBITED;
			} else {
				fprintf(stderr,
				"scan_type value should be \"active\" "
				"or \"passive\", but got \"%s\"\n", valstr);
				err = -1;
				goto exit;
			}
		}
		if (!strcmp(key, "bss_type")) {
			if (!strcmp(valstr, "bss") || !strcmp(valstr, "infra")) {
				params->bss_type = DOT11_BSSTYPE_INFRASTRUCTURE;
			} else if (!strcmp(valstr, "ibss") || !strcmp(valstr, "adhoc")) {
				params->bss_type = DOT11_BSSTYPE_INDEPENDENT;
			} else if (!strcmp(valstr, "any")) {
				params->bss_type = DOT11_BSSTYPE_ANY;
			} else {
				fprintf(stderr,
				"bss_type value should be "
				"\"bss\", \"ibss\", or \"any\", but got \"%s\"\n", valstr);
				err = -1;
				goto exit;
			}
		}
		if (opt == 'b' || !strcmp(key, "bssid")) {
			if (!wl_ether_atoe(valstr, &params->bssid)) {
				fprintf(stderr,
				"could not parse \"%s\" as an ethernet MAC address\n", valstr);
				err = -1;
				goto exit;
			}
		}
		if (opt == 'n' || !strcmp(key, "nprobes")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as an int for value nprobes\n", valstr);
				err = -1;
				goto exit;
			}
			params->nprobes = val;
		}
		if (opt == 'a' || !strcmp(key, "active")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as an int for active dwell time\n",
					valstr);
				err = -1;
				goto exit;
			}
			params->active_time = val;
		}
		if (opt == 'p' || !strcmp(key, "passive")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as an int for passive dwell time\n",
					valstr);
				err = -1;
				goto exit;
			}
			params->passive_time = val;
		}
		if (opt == 'h' || !strcmp(key, "home")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as an int for home channel dwell time\n",
					valstr);
				err = -1;
				goto exit;
			}
			params->home_time = val;
		}
		if (opt == 'c' || !strcmp(key, "channels")) {
			nchan = wl_parse_channel_list(valstr, params->channel_list,
			                              WL_NUMCHANNELS);
			if (nchan == -1) {
				fprintf(stderr, "error parsing channel list arg\n");
				err = -1;
				goto exit;
			}
		}
	}

	if (nssid > WL_SCAN_PARAMS_SSID_MAX) {
		fprintf(stderr, "ssid count %d exceeds max of %d\n",
		        nssid, WL_SCAN_PARAMS_SSID_MAX);
		err = -1;
		goto exit;
	}

	params->nprobes = htod32(params->nprobes);
	params->active_time = htod32(params->active_time);
	params->passive_time = htod32(params->passive_time);
	params->home_time = htod32(params->home_time);

	for (i = 0; i < nchan; i++) {
		params->channel_list[i] = htodchanspec(params->channel_list[i]);
	}

	for (i = 0; i < nssid; i++) {
		ssids[i].SSID_len = htod32(ssids[i].SSID_len);
	}

	/* For a single ssid, use the single fixed field */
	if (nssid == 1) {
		nssid = 0;
		memcpy(&params->ssid, &ssids[0], sizeof(ssids[0]));
	}

	/* Copy ssid array if applicable */
	if (nssid > 0) {
		i = OFFSETOF(wl_scan_params_t, channel_list) + nchan * sizeof(uint16);
		i = ROUNDUP(i, sizeof(uint32));
		if (i + nssid * sizeof(wlc_ssid_t) > (uint)*params_size) {
			fprintf(stderr, "additional ssids exceed params_size\n");
			err = -1;
			goto exit;
		}

		p = (char*)params + i;
		memcpy(p, ssids, nssid * sizeof(wlc_ssid_t));
		p += nssid * sizeof(wlc_ssid_t);
	} else {
		p = (char*)params->channel_list + nchan * sizeof(uint16);
	}

	params->channel_num = htod32((nssid << WL_SCAN_PARAMS_NSSID_SHIFT) |
	                             (nchan & WL_SCAN_PARAMS_COUNT_MASK));
	*params_size = p - (char*)params + nssid * sizeof(wlc_ssid_t);
exit:
	return err;
}


static int
wl_scan(void *wl, cmd_t *cmd, char **argv)
{
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + WL_NUMCHANNELS * sizeof(uint16);
	wl_scan_params_t *params;
	int err = 0;

	params_size += WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t);
	params = (wl_scan_params_t*)malloc(params_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan params\n", params_size);
		return -1;
	}
	memset(params, 0, params_size);

	err = wl_scan_prep(wl, cmd, argv, params, &params_size);

	if (!err) {
		err = wlu_set(wl, cmd->set, params, params_size);
	}

	free(params);
	return err;
}

#if defined(linux)
extern time_t time(time_t *ptr);
#endif
static int
wl_escan(void *wl, cmd_t *cmd, char **argv)
{
	int params_size = (WL_SCAN_PARAMS_FIXED_SIZE + OFFSETOF(wl_escan_params_t, params)) +
	    (WL_NUMCHANNELS * sizeof(uint16));
	wl_escan_params_t *params;
	int err = 0;
	uint16 action = WL_SCAN_ACTION_START;

	if (!stricmp(*argv, "escan"))
		/* start an escan */
		action = WL_SCAN_ACTION_START;
	else if (!stricmp(*argv, "escanabort"))
		/* abort an escan */
		action = WL_SCAN_ACTION_ABORT;
	else {
		printf("unknown escan command: %s\n", *argv);
		return 0;
	}

	params_size += WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t);
	params = (wl_escan_params_t*)malloc(params_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan params\n", params_size);
		return -1;
	}
	memset(params, 0, params_size);

	err = wl_scan_prep(wl, cmd, argv, &params->params, &params_size);

	if (!err) {
		params->version = htod32(ESCAN_REQ_VERSION);
		params->action = htod16(action);

#if defined(linux)
		srand((unsigned)time(NULL));
		params->sync_id = htod16(rand() & 0xffff);
#else
		params->sync_id = htod16(4321);
#endif /* #if defined(linux) */

		params_size += OFFSETOF(wl_escan_params_t, params);
		err = wlu_iovar_setbuf(wl, "escan", params, params_size, buf, WLC_IOCTL_MAXLEN);
	}

	free(params);
	return err;
}

static int
wl_iscan(void *wl, cmd_t *cmd, char **argv)
{
	int params_size = (WL_SCAN_PARAMS_FIXED_SIZE + OFFSETOF(wl_iscan_params_t, params)) +
	    (WL_NUMCHANNELS * sizeof(uint16));
	wl_iscan_params_t *params;
	int err = 0;
	uint16 action = WL_SCAN_ACTION_START;
	char **p;
	uint16 iscan_duration = 0;

	if (!stricmp(*argv, "iscan_s"))
		action = WL_SCAN_ACTION_START;
	else if (!stricmp(*argv, "iscan_c"))
		action = WL_SCAN_ACTION_CONTINUE;
	else {
		printf("unknown iscan command: %s\n", *argv);
		return 0;
	}

	/* look for iscan_duration parameter */
	p = argv;
	while (*p != NULL) {
		if (!strcmp(*p, "-d") || !strncmp(*p, "--duration=", 11)) {
			char *valptr;
			int val;
			char *endptr;
			if (!strcmp(*p, "-d"))
				valptr = *(++p);
			else
				valptr = *p + 11;
			val = (int)strtol(valptr, &endptr, 0);
			if (*endptr != '\0') {
				fprintf(stderr,
					"could not parse \"%s\" as an int for duration\n",
					valptr);
				err = -1;
				goto exit;
			}
			iscan_duration = (uint16) val;
			break;
		}
		++p;
	}

	params_size += WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t);
	params = (wl_iscan_params_t*)malloc(params_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan params\n", params_size);
		return -1;
	}
	memset(params, 0, params_size);

	err = wl_scan_prep(wl, cmd, argv, &params->params, &params_size);

	if (!err) {
		params->version = htod32(ISCAN_REQ_VERSION);
		params->action = htod16(action);
		params->scan_duration = htod16(iscan_duration);
		params_size += OFFSETOF(wl_iscan_params_t, params);
		err = wlu_iovar_setbuf(wl, "iscan", params, params_size, buf, WLC_IOCTL_MAXLEN);
	}

	free(params);
exit:
	return err;
}

static int
wl_parse_assoc_params(char **argv, wl_assoc_params_t *params)
{
	int err = BCME_OK;
	char *p, *eq, *valstr;
	char opt;
	bool opt_end;
	int keylen;
	char key[64];
	int i;

	opt_end = FALSE;

	while ((p = *argv) != NULL) {
		argv++;
		memset(key, 0, sizeof(key));
		opt = '\0';
		valstr = NULL;

		if (opt_end) {
			valstr = p;
		}
		else if (!strcmp(p, "--")) {
			opt_end = TRUE;
			continue;
		}
		else if (!strncmp(p, "--", 2)) {
			eq = strchr(p, '=');
			if (eq == NULL) {
				fprintf(stderr, "wl_parse_assoc_params: missing \" = \" in "
				        "long param \"%s\"\n", p);
				err = BCME_BADARG;
				goto exit;
			}
			keylen = eq - (p + 2);
			if (keylen > 63) keylen = 63;
			memcpy(key, p + 2, keylen);

			valstr = eq + 1;
			if (*valstr == '\0') {
				fprintf(stderr, "wl_parse_assoc_params: missing value after "
				        "\" = \" in long param \"%s\"\n", p);
				err = BCME_BADARG;
				goto exit;
			}
		}
		else if (!strncmp(p, "-", 1)) {
			opt = p[1];
			if (strlen(p) > 2) {
				fprintf(stderr, "wl_parse_assoc_params: only single char options, "
				        "error on param \"%s\"\n", p);
				err = BCME_BADARG;
				goto exit;
			}
			if (*argv == NULL) {
				fprintf(stderr, "wl_parse_assoc_params: missing value parameter "
				        "after \"%s\"\n", p);
				err = BCME_BADARG;
				goto exit;
			}
			valstr = *argv++;
		} else {
			valstr = p;
		}

		/* handle -o v or --option=val */
		if (opt == 'b' || !stricmp(key, "bssid")) {
			if (!wl_ether_atoe(valstr, &params->bssid)) {
				fprintf(stderr, "could not parse as an ethernet MAC address\n");
				err = BCME_BADARG;
				goto exit;
			}
		}
		else if (opt == 'c' || !strcmp(key, "chanspecs")) {
			params->chanspec_num =
			     wl_parse_chanspec_list(valstr, params->chanspec_list, WL_NUMCHANNELS);
			if (params->chanspec_num == -1) {
				fprintf(stderr, "error parsing chanspec list arg\n");
				err = BCME_BADARG;
				goto exit;
			}
		}
	}

	/* prepare the chanspec using the channel number and the user provided options */
	for (i = 0; i < params->chanspec_num; i++) {
		chanspec_t chanspec = wl_chspec_to_driver(params->chanspec_list[i]);
		if (chanspec == INVCHANSPEC) {
			err = USAGE_ERROR;
			goto exit;
		}
		params->chanspec_list[i] = chanspec;
	}
	params->chanspec_num = htod32(params->chanspec_num);

exit:
	return err;
}

/* wl reassoc <bssid>
 * Options:
 * -c CL, --chanspecs=CL, where CL is a comma or space separated list of chanspecs
 */
static int
wl_reassoc(void *wl, cmd_t *cmd, char **argv)
{
	int params_size = WL_REASSOC_PARAMS_FIXED_SIZE + WL_NUMCHANNELS * sizeof(chanspec_t);
	wl_reassoc_params_t *params;
	int err = 0;

	UNUSED_PARAMETER(cmd);

	if (*++argv == NULL) {
		fprintf(stderr, "no arguments to wl_reassoc\n");
		return BCME_ERROR;
	}

	params = (wl_reassoc_params_t *)malloc(params_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan params\n", params_size);
		return BCME_NOMEM;
	}
	memset(params, 0, params_size);

	if (!wl_ether_atoe(*argv, &params->bssid)) {
		fprintf(stderr, "could not parse %s as an Ethernet MAC address\n", *argv);
		err = BCME_BADARG;
		goto exit;
	}
	/* default to plain old ioctl */
	params_size = ETHER_ADDR_LEN;

	if (*++argv != NULL) {
		if ((err = wl_parse_reassoc_params(argv, params)) != BCME_OK) {
			fprintf(stderr, "could not parse reassociation parameters\n");
			goto exit;
		}
		params_size = WL_REASSOC_PARAMS_FIXED_SIZE +
		        dtoh32(params->chanspec_num) * sizeof(chanspec_t);
	}

	err = wlu_set(wl, WLC_REASSOC, params, params_size);

exit:
	free(params);
	return err;
}

#ifdef EXTENDED_SCAN
/* wl extdscan
 * -s --ssid=ssid1 ssid2 ssid3
 * -b --split_scan=0 : [split_scan]
 * -t --scan_type=0 : [background/forcedbackground/foreground]
 * -n --nprobes=
 * -c --channels=
 */
static int
wl_extdscan(void *wl, cmd_t *cmd, char **argv)
{
	wl_extdscan_params_t *params;
	int params_size = WL_EXTDSCAN_PARAMS_FIXED_SIZE +
		(WL_NUMCHANNELS * sizeof(chan_scandata_t));
	int val = 0;
	char *p, *eq, *valstr, *endptr;
	char opt;
	bool positional_param;
	bool good_int;
	bool opt_end;
	int err = 0;
	int keylen;
	char key[64];
	int i;
	int nssid = 0;

	fprintf(stderr, "params alloc size is %d\n", params_size);
	params = (wl_extdscan_params_t *)malloc(params_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan params\n", params_size);
		return -1;
	}
	memset(params, 0, params_size);

	params->scan_type = EXTDSCAN_FORCEDBACKGROUND_SCAN;
	params->nprobes = 3;
	params->band = WLC_BAND_2G;
	params->split_scan = 0;

	/* skip the command name */
	argv++;

	if (*argv == NULL) {
		fprintf(stderr, "no arguments to wl_extdscan\n");
		err = -1;
		goto exit;
	}
	opt_end = FALSE;
	while ((p = *argv) != NULL) {
		argv++;
		positional_param = FALSE;
		memset(key, 0, sizeof(key));
		opt = '\0';
		valstr = NULL;
		good_int = FALSE;

		if (opt_end) {
			positional_param = TRUE;
			valstr = p;
		}
		else if (!strcmp(p, "--")) {
			opt_end = TRUE;
			continue;
		}
		else if (!strncmp(p, "--", 2)) {
			eq = strchr(p, '=');
			if (eq == NULL) {
				fprintf(stderr,
				"wl_extdscan: missing \" = \" in long param \"%s\"\n", p);
				err = -1;
				goto exit;
			}
			keylen = eq - (p + 2);
			if (keylen > 63) keylen = 63;
			memcpy(key, p + 2, keylen);

			valstr = eq + 1;
			if (*valstr == '\0') {
				fprintf(stderr,
				"extdscan: missing value after \" = \" in long param \"%s\"\n", p);
				err = -1;
				goto exit;
			}
		}
		else if (!strncmp(p, "-", 1)) {
			opt = p[1];
			if (strlen(p) > 2) {
				fprintf(stderr,
				"extdscan: only single char options, error on param \"%s\"\n", p);
				err = -1;
				goto exit;
			}
			if (*argv == NULL) {
				fprintf(stderr,
				"extdscan: missing value parameter after \"%s\"\n", p);
				err = -1;
				goto exit;
			}
			valstr = *argv;
			argv++;
		} else {
			positional_param = TRUE;
			valstr = p;
		}

		/* parse valstr as int just in case */
		if (valstr) {
			val = (int)strtol(valstr, &endptr, 0);
			if (*endptr == '\0') {
				/* not all the value string was parsed by strtol */
				good_int = TRUE;
			}
		}

		if (opt == 's' || !strcmp(key, "ssid") || positional_param) {
			nssid = wl_parse_ssid_list(valstr, params->ssid,
			                           nssid, WLC_EXTDSCAN_MAX_SSID);
			if (nssid < 0) {
				err = -1;
				goto exit;
			}
		}
		if (opt == 'b' || !strcmp(key, "band")) {
			if (!strcmp(valstr, "5G")) {
				params->band = WLC_BAND_5G;
			}
			else if (!strcmp(valstr, "2.4G")) {
				params->band = WLC_BAND_2G;
			}
			else if (!strcmp(valstr, "all")) {
				params->band = WLC_BAND_ALL;
			} else {
				fprintf(stderr,
				"scan_type value should be \"5G\" "
				"or \"2.4G\" " "or \"all\" but got \"%s\"\n", valstr);
				err = -1;
				goto exit;
			}
		}
		if (opt == 't' || !strcmp(key, "scan_type")) {
			if (!strcmp(valstr, "background")) {
				params->scan_type = EXTDSCAN_BACKGROUND_SCAN;
			} else if (!strcmp(valstr, "fbackground")) {
				params->scan_type = EXTDSCAN_FORCEDBACKGROUND_SCAN;
			} else if (!strcmp(valstr, "foreground")) {
				params->scan_type = EXTDSCAN_FOREGROUND_SCAN;
			} else {
				fprintf(stderr,
				"scan_type value should be \"background\" "
				"or \"fbackground\" " "or \"foreground\" but got \"%s\"\n", valstr);
				err = -1;
				goto exit;
			}
		}
		if (opt == 'n' || !strcmp(key, "nprobes")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as an int for value nprobes\n", valstr);
				err = -1;
				goto exit;
			}
			params->nprobes = val;
		}
		if (opt == 'x' || !strcmp(key, "split_scan")) {
			if (val != 0)
				params->split_scan = 1;
		}
		if (opt == 'c' || !strcmp(key, "channels")) {
			params->channel_num = wl_parse_extdchannel_list(valstr,
				params->channel_list, WL_NUMCHANNELS);
			if (params->channel_num == -1) {
				fprintf(stderr, "error parsing channel list arg\n");
				err = -1;
				goto exit;
			}
		}
	}

	if (nssid > WLC_EXTDSCAN_MAX_SSID) {
		fprintf(stderr, "ssid count %d exceeds max of %d\n",
		        nssid, WLC_EXTDSCAN_MAX_SSID);
		err = -1;
		goto exit;
	}

	params_size = WL_EXTDSCAN_PARAMS_FIXED_SIZE +
	    (params->channel_num * sizeof(chan_scandata_t));

	fprintf(stderr, "ssid list is %s(%d) %s(%d) %s(%d) %s(%d) %s(%d)\n",
		(char *)&params->ssid[0].SSID, params->ssid[0].SSID_len,
		(char *)&params->ssid[1].SSID, params->ssid[1].SSID_len,
		(char *)&params->ssid[2].SSID, params->ssid[2].SSID_len,
		(char *)&params->ssid[3].SSID, params->ssid[3].SSID_len,
		(char *)&params->ssid[4].SSID, params->ssid[4].SSID_len);
	if (params->split_scan)
	    fprintf(stderr, "split scan is enabled\n");
	else
	   fprintf(stderr, "split scan is not enabled\n");

	fprintf(stderr, "scan type is %d, nprobes are %d, band is %d, channels are %d\n",
		params->scan_type, params->nprobes, params->band, params->channel_num);

	fprintf(stderr, "params size is %d\n", params_size);
	params->scan_type = htodenum(params->scan_type);
	for (i = 0; i < WLC_EXTDSCAN_MAX_SSID; i++) {
		params->ssid[i].SSID_len = htod32(params->ssid[i].SSID_len);
	}
	for (i = 0; i < params->channel_num; i++) {
		chanspec_t chanspec = params->channel_list[i].channel;
		chanspec = wl_chspec_to_driver(chanspec);
		if (chanspec == INVCHANSPEC) {
			err = USAGE_ERROR;
			goto exit;
		}
		params->channel_list[i].channel = chanspec;
		params->channel_list[i].channel_mintime =
		        htod32(params->channel_list[i].channel_mintime);
		params->channel_list[i].channel_maxtime =
		        htod32(params->channel_list[i].channel_maxtime);
	}
	params->channel_num = htod32(params->channel_num);
	err =  wlu_var_setbuf(wl, cmd->name, params, params_size);

exit:
	free(params);
	return err;
}

static int
wl_parse_extdchannel_list(char* list_str, chan_scandata_t* channel_list, int channel_num)
{
	int num;
	int val;
	char* str;
	char* endptr;

	if (list_str == NULL)
		return -1;

	str = list_str;
	num = 0;
	while (*str != '\0') {
		val = (int)strtol(str, &endptr, 0);
		if (endptr == str) {
			fprintf(stderr,
				"could not parse channel number starting at"
				" substring \"%s\" in list:\n%s\n",
				str, list_str);
			return -1;
		}
		str = endptr + strspn(endptr, " ,");

		if (num == channel_num) {
			fprintf(stderr, "too many channels (more than %d) in channel list:\n%s\n",
				channel_num, list_str);
			return -1;
		}
		channel_list->channel = (uint16)val;
		channel_list++;
		num++;
	}

	return num;
}
#endif /* EXTENDED_SCAN */

static int
wl_parse_channel_list(char* list_str, uint16* channel_list, int channel_num)
{
	int num;
	int val;
	char* str;
	char* endptr = NULL;

	if (list_str == NULL)
		return -1;

	str = list_str;
	num = 0;
	while (*str != '\0') {
		val = (int)strtol(str, &endptr, 0);
		if (endptr == str) {
			fprintf(stderr,
				"could not parse channel number starting at"
				" substring \"%s\" in list:\n%s\n",
				str, list_str);
			return -1;
		}
		str = endptr + strspn(endptr, " ,");

		if (num == channel_num) {
			fprintf(stderr, "too many channels (more than %d) in channel list:\n%s\n",
				channel_num, list_str);
			return -1;
		}

		channel_list[num++] = (uint16)val;
	}

	return num;
}

static int
wl_parse_chanspec_list(char *list_str, chanspec_t *chanspec_list, int chanspec_num)
{
	int num = 0;
	chanspec_t chanspec;
	char *next, str[8];
	size_t len;

	if ((next = list_str) == NULL)
		return BCME_ERROR;

	while ((len = strcspn(next, " ,")) > 0) {
		if (len >= sizeof(str)) {
			fprintf(stderr, "string \"%s\" before ',' or ' ' is too long\n", next);
			return BCME_ERROR;
		}
		strncpy(str, next, len);
		str[len] = 0;
		chanspec = wf_chspec_aton(str);
		if (chanspec == 0) {
			fprintf(stderr, "could not parse chanspec starting at "
			        "\"%s\" in list:\n%s\n", str, list_str);
			return BCME_ERROR;
		}
		if (num == chanspec_num) {
			fprintf(stderr, "too many chanspecs (more than %d) in chanspec list:\n%s\n",
				chanspec_num, list_str);
			return BCME_ERROR;
		}
		chanspec_list[num++] = chanspec;
		next += len;
		next += strspn(next, " ,");
	}

	return num;
}

/* channel info structure */
typedef struct {
	uint	chan;		/* channel number */
	uint	freq;		/* in Mhz */
} chan_info_t;

static chan_info_t chan_info[] = {
	/* B channels */
	{ 1,	2412},
	{ 2,	2417},
	{ 3,	2422},
	{ 4,	2427},
	{ 5,	2432},
	{ 6,	2437},
	{ 7,	2442},
	{ 8,	2447},
	{ 9,	2452},
	{ 10,	2457},
	{ 11,	2462},
	{ 12,	2467},
	{ 13,	2472},
	{ 14,	2484},

	/* A channels */
	/* 11a usa low */
	{ 36,	5180},
	{ 40,	5200},
	{ 44,	5220},
	{ 48,	5240},
	{ 52,	5260},
	{ 56,	5280},
	{ 60,	5300},
	{ 64,	5320},

	/* 11a Europe */
	{ 100,	5500},
	{ 104,	5520},
	{ 108,	5540},
	{ 112,	5560},
	{ 116,	5580},
	{ 120,	5600},
	{ 124,	5620},
	{ 128,	5640},
	{ 132,	5660},
	{ 136,	5680},
	{ 140,	5700},

	/* 11a usa high */
	{ 149,	5745},
	{ 153,	5765},
	{ 157,	5785},
	{ 161,	5805},

	/* 11a japan */
	{ 184,	4920},
	{ 188,	4940},
	{ 192,	4960},
	{ 196,	4980},
	{ 200,	5000},
	{ 204,	5020},
	{ 208,	5040},
	{ 212,	5060},
	{ 216,	5080}
};

uint
freq2channel(uint freq)
{
	int i;

	for (i = 0; i < (int)ARRAYSIZE(chan_info); i++) {
		if (chan_info[i].freq == freq)
			return (chan_info[i].chan);
	}
	return (0);
}

void
dump_rateset(uint8 *rates, uint count)
{
	uint i;
	uint r;
	bool b;

	printf("[ ");
	for (i = 0; i < count; i++) {
		r = rates[i] & 0x7f;
		b = rates[i] & 0x80;
		if (r == 0)
			break;
		printf("%d%s%s ", (r / 2), (r % 2)?".5":"", b?"(b)":"");
	}
	printf("]");
}

/* Helper routine to print the infrastructure mode while pretty printing the BSS list */
static const char *
capmode2str(uint16 capability)
{
	capability &= (DOT11_CAP_ESS | DOT11_CAP_IBSS);

	if (capability == DOT11_CAP_ESS)
		return "Managed";
	else if (capability == DOT11_CAP_IBSS)
		return "Ad Hoc";
	else
		return "<unknown>";
}

/*
 * Traverse a string of 1-byte tag/1-byte length/variable-length value
 * triples, returning a pointer to the substring whose first element
 * matches tag
 */
static uint8 *
wlu_parse_tlvs(uint8 *tlv_buf, int buflen, uint key)
{
	uint8 *cp;
	int totlen;

	cp = tlv_buf;
	totlen = buflen;

	/* find tagged parameter */
	while (totlen >= 2) {
		uint tag;
		int len;

		tag = *cp;
		len = *(cp +1);

		/* validate remaining totlen */
		if ((tag == key) && (totlen >= (len + 2)))
			return (cp);

		cp += (len + 2);
		totlen -= (len + 2);
	}

	return NULL;
}

static int
wlu_bcmp(const void *b1, const void *b2, int len)
{
	return (memcmp(b1, b2, len));
}

/* Is this body of this tlvs entry a WPA entry? If */
/* not update the tlvs buffer pointer/length */
static bool
wlu_is_wpa_ie(uint8 **wpaie, uint8 **tlvs, uint *tlvs_len)
{
	uint8 *ie = *wpaie;

	/* If the contents match the WPA_OUI and type=1 */
	if ((ie[1] >= 6) && !wlu_bcmp(&ie[2], WPA_OUI "\x01", 4)) {
		return TRUE;
	}

	/* point to the next ie */
	ie += ie[1] + 2;
	/* calculate the length of the rest of the buffer */
	*tlvs_len -= (int)(ie - *tlvs);
	/* update the pointer to the start of the buffer */
	*tlvs = ie;

	return FALSE;
}

static void
wl_dump_wpa_rsn_ies(uint8* cp, uint len)
{
	uint8 *parse = cp;
	uint parse_len = len;
	uint8 *wpaie;
	uint8 *rsnie;

	while ((wpaie = wlu_parse_tlvs(parse, parse_len, DOT11_MNG_WPA_ID)))
		if (wlu_is_wpa_ie(&wpaie, &parse, &parse_len))
			break;
	if (wpaie)
		wl_rsn_ie_dump((bcm_tlv_t*)wpaie);

	rsnie = wlu_parse_tlvs(cp, len, DOT11_MNG_RSN_ID);
	if (rsnie)
		wl_rsn_ie_dump((bcm_tlv_t*)rsnie);

	return;
}

static void
wl_rsn_ie_dump(bcm_tlv_t *ie)
{
	int i;
	int rsn;
	wpa_ie_fixed_t *wpa = NULL;
	rsn_parse_info_t rsn_info;
	wpa_suite_t *suite;
	uint8 std_oui[3];
	int unicast_count = 0;
	int akm_count = 0;
	uint16 capabilities;
	uint cntrs;
	int err;

	if (ie->id == DOT11_MNG_RSN_ID) {
		rsn = TRUE;
		memcpy(std_oui, WPA2_OUI, WPA_OUI_LEN);
		err = wl_rsn_ie_parse_info(ie->data, ie->len, &rsn_info);
	} else {
		rsn = FALSE;
		memcpy(std_oui, WPA_OUI, WPA_OUI_LEN);
		wpa = (wpa_ie_fixed_t*)ie;
		err = wl_rsn_ie_parse_info((uint8*)&wpa->version, wpa->length - WPA_IE_OUITYPE_LEN,
		                           &rsn_info);
	}
	if (err || rsn_info.version != WPA_VERSION)
		return;

	if (rsn)
		printf("RSN:\n");
	else
		printf("WPA:\n");

	/* Check for multicast suite */
	if (rsn_info.mcast) {
		printf("\tmulticast cipher: ");
		if (!wlu_bcmp(rsn_info.mcast->oui, std_oui, 3)) {
			switch (rsn_info.mcast->type) {
			case WPA_CIPHER_NONE:
				printf("NONE\n");
				break;
			case WPA_CIPHER_WEP_40:
				printf("WEP64\n");
				break;
			case WPA_CIPHER_WEP_104:
				printf("WEP128\n");
				break;
			case WPA_CIPHER_TKIP:
				printf("TKIP\n");
				break;
			case WPA_CIPHER_AES_OCB:
				printf("AES-OCB\n");
				break;
			case WPA_CIPHER_AES_CCM:
				printf("AES-CCMP\n");
				break;
			default:
				printf("Unknown-%s(#%d)\n", rsn ? "RSN" : "WPA",
				       rsn_info.mcast->type);
				break;
			}
		}
		else {
			printf("Unknown-%02X:%02X:%02X(#%d) ",
			       rsn_info.mcast->oui[0], rsn_info.mcast->oui[1],
			       rsn_info.mcast->oui[2], rsn_info.mcast->type);
		}
	}

	/* Check for unicast suite(s) */
	if (rsn_info.ucast) {
		unicast_count = ltoh16_ua(&rsn_info.ucast->count);
		printf("\tunicast ciphers(%d): ", unicast_count);
		for (i = 0; i < unicast_count; i++) {
			suite = &rsn_info.ucast->list[i];
			if (!wlu_bcmp(suite->oui, std_oui, 3)) {
				switch (suite->type) {
				case WPA_CIPHER_NONE:
					printf("NONE ");
					break;
				case WPA_CIPHER_WEP_40:
					printf("WEP64 ");
					break;
				case WPA_CIPHER_WEP_104:
					printf("WEP128 ");
					break;
				case WPA_CIPHER_TKIP:
					printf("TKIP ");
					break;
				case WPA_CIPHER_AES_OCB:
					printf("AES-OCB ");
					break;
				case WPA_CIPHER_AES_CCM:
					printf("AES-CCMP ");
					break;
				default:
					printf("WPA-Unknown-%s(#%d) ", rsn ? "RSN" : "WPA",
					       suite->type);
					break;
				}
			}
			else {
				printf("Unknown-%02X:%02X:%02X(#%d) ",
					suite->oui[0], suite->oui[1], suite->oui[2],
					suite->type);
			}
		}
		printf("\n");
	}
	/* Authentication Key Management */
	if (rsn_info.akm) {
		akm_count = ltoh16_ua(&rsn_info.akm->count);
		printf("\tAKM Suites(%d): ", akm_count);
		for (i = 0; i < akm_count; i++) {
			suite = &rsn_info.akm->list[i];
			if (!wlu_bcmp(suite->oui, std_oui, 3)) {
				switch (suite->type) {
				case RSN_AKM_NONE:
					printf("None ");
					break;
				case RSN_AKM_UNSPECIFIED:
					printf("WPA ");
					break;
				case RSN_AKM_PSK:
					printf("WPA-PSK ");
					break;
				case RSN_AKM_FBT_1X:
					printf("FT-802.1X ");
					break;
				case RSN_AKM_FBT_PSK:
					printf("FT-PSK ");
					break;
				default:
					printf("Unknown-%s(#%d)  ",
					       rsn ? "RSN" : "WPA", suite->type);
					break;
				}
			}
			else {
				printf("Unknown-%02X:%02X:%02X(#%d)  ",
					suite->oui[0], suite->oui[1], suite->oui[2],
					suite->type);
			}
		}
		printf("\n");
	}

	/* Capabilities */
	if (rsn_info.capabilities) {
		capabilities = ltoh16_ua(rsn_info.capabilities);
		printf("\tCapabilities(0x%04x): ", capabilities);
		if (rsn)
			printf("%sPre-Auth, ", (capabilities & RSN_CAP_PREAUTH) ? "" : "No ");

		printf("%sPairwise, ", (capabilities & RSN_CAP_NOPAIRWISE) ? "No " : "");

		cntrs = wl_rsn_ie_decode_cntrs((capabilities & RSN_CAP_PTK_REPLAY_CNTR_MASK) >>
		                               RSN_CAP_PTK_REPLAY_CNTR_SHIFT);

		printf("%d PTK Replay Ctr%s", cntrs, (cntrs > 1)?"s":"");

		if (rsn) {
			cntrs = wl_rsn_ie_decode_cntrs(
				(capabilities & RSN_CAP_GTK_REPLAY_CNTR_MASK) >>
				RSN_CAP_GTK_REPLAY_CNTR_SHIFT);

			printf("%d GTK Replay Ctr%s\n", cntrs, (cntrs > 1)?"s":"");
		} else {
			printf("\n");
		}
	} else {
		printf("\tNo %s Capabilities advertised\n", rsn ? "RSN" : "WPA");
	}

}

/* Validates and parses the RSN or WPA IE contents into a rsn_parse_info_t structure
 * Returns 0 on success, or 1 if the information in the buffer is not consistant with
 * an RSN IE or WPA IE.
 * The buf pointer passed in should be pointing at the version field in either an RSN IE
 * or WPA IE.
 */
static int
wl_rsn_ie_parse_info(uint8* rsn_buf, uint len, rsn_parse_info_t *rsn)
{
	uint16 count;

	memset(rsn, 0, sizeof(rsn_parse_info_t));

	/* version */
	if (len < sizeof(uint16))
		return 1;

	rsn->version = ltoh16_ua(rsn_buf);
	len -= sizeof(uint16);
	rsn_buf += sizeof(uint16);

	/* Multicast Suite */
	if (len < sizeof(wpa_suite_mcast_t))
		return 0;

	rsn->mcast = (wpa_suite_mcast_t*)rsn_buf;
	len -= sizeof(wpa_suite_mcast_t);
	rsn_buf += sizeof(wpa_suite_mcast_t);

	/* Unicast Suite */
	if (len < sizeof(uint16))
		return 0;

	count = ltoh16_ua(rsn_buf);

	if (len < (sizeof(uint16) + count * sizeof(wpa_suite_t)))
		return 1;

	rsn->ucast = (wpa_suite_ucast_t*)rsn_buf;
	len -= (sizeof(uint16) + count * sizeof(wpa_suite_t));
	rsn_buf += (sizeof(uint16) + count * sizeof(wpa_suite_t));

	/* AKM Suite */
	if (len < sizeof(uint16))
		return 0;

	count = ltoh16_ua(rsn_buf);

	if (len < (sizeof(uint16) + count * sizeof(wpa_suite_t)))
		return 1;

	rsn->akm = (wpa_suite_auth_key_mgmt_t*)rsn_buf;
	len -= (sizeof(uint16) + count * sizeof(wpa_suite_t));
	rsn_buf += (sizeof(uint16) + count * sizeof(wpa_suite_t));

	/* Capabilites */
	if (len < sizeof(uint16))
		return 0;

	rsn->capabilities = rsn_buf;

	return 0;
}

static uint
wl_rsn_ie_decode_cntrs(uint cntr_field)
{
	uint cntrs;

	switch (cntr_field) {
	case RSN_CAP_1_REPLAY_CNTR:
		cntrs = 1;
		break;
	case RSN_CAP_2_REPLAY_CNTRS:
		cntrs = 2;
		break;
	case RSN_CAP_4_REPLAY_CNTRS:
		cntrs = 4;
		break;
	case RSN_CAP_16_REPLAY_CNTRS:
		cntrs = 16;
		break;
	default:
		cntrs = 0;
		break;
	}

	return cntrs;
}


void
wl_dump_raw_ie(bcm_tlv_t *ie, uint len)
{
	uint dump_len;

	if (len == 0) {
		return;
	} else if (len == 1) {
		printf("IE header truncated: ID: 0x%02X\n", ie->id);
		return;
	} else if (len < (uint)(ie->len + TLV_HDR_LEN)) {
		printf("IE data truncated: ID: 0x%02X Len: %d\n", ie->id, ie->len);
		dump_len = len - TLV_HDR_LEN;
	} else {
		printf("ID: 0x%02X Len: %d\n", ie->id, ie->len);
		dump_len = ie->len;
	}

	/* choose how to format the data based on data len */
	if (dump_len > 16)
		printf("Data:\n");
	else if (dump_len > 0)
		printf("Data: ");

	if (dump_len > 0)
		wl_hexdump(ie->data, dump_len);

	if (dump_len < ie->len)
		printf("<missing %d bytes>\n", ie->len - dump_len);

	return;
}


/* Pretty print the BSS list */
static void
dump_networks(char *network_buf)
{
	wl_scan_results_t *list = (wl_scan_results_t*)network_buf;
	wl_bss_info_t *bi;
	uint i;

	if (list->count == 0)
		return;
	else if (list->version != WL_BSS_INFO_VERSION &&
	         list->version != LEGACY2_WL_BSS_INFO_VERSION &&
	         list->version != LEGACY_WL_BSS_INFO_VERSION) {
		fprintf(stderr, "Sorry, your driver has bss_info_version %d "
			"but this program supports only version %d.\n",
			list->version, WL_BSS_INFO_VERSION);
		return;
	}

	bi = list->bss_info;
	for (i = 0; i < list->count; i++, bi = (wl_bss_info_t*)((int8*)bi + dtoh32(bi->length))) {
		dump_bss_info(bi);
	}
}

static void
bcm_wps_version(uint8 *wps_ie)
{
	uint16 wps_len;
	uint16 wps_off, wps_suboff;
	uint16 wps_key;
	uint8 wps_field_len;

	wps_len = (uint16)*(wps_ie+TLV_LEN_OFF); /* Get the length of the WPS OUI header */
	wps_off = WPS_OUI_FIXED_HEADER_OFF; /* Skip the fixed headers */
	wps_field_len = 1;

	/* Parsing the OUI header looking for version number */
	while ((wps_len >= wps_off + 2) && (wps_field_len))
	{
		wps_key = (((uint8)wps_ie[wps_off]*256) + (uint8)wps_ie[wps_off+1]);
		if (wps_key == WPS_ID_VENDOR_EXT) {
			/* Key found */
			wps_suboff = wps_off + WPS_OUI_HEADER_SIZE;

			/* Looking for the Vendor extension code 0x00 0x37 0x2A
			 * and the Version 2 sudId 0x00
			 * if found then the next byte is the len of field which is always 1
			 * for version field the byte after is the version number
			 */
			if (!wlu_bcmp(&wps_ie[wps_suboff],  WFA_VENDOR_EXT_ID, WPS_OUI_LEN)&&
				(wps_ie[wps_suboff+WPS_WFA_SUBID_V2_OFF] == WPS_WFA_SUBID_VERSION2))
			{
				printf("V%d.%d ", (wps_ie[wps_suboff+WPS_WFA_V2_OFF]>>4),
				(wps_ie[wps_suboff+WPS_WFA_V2_OFF] & 0x0f));
				return;
			}
		}
		/* Jump to next field */
		wps_field_len = wps_ie[wps_off+WPS_OUI_HEADER_LEN+1];
		wps_off += WPS_OUI_HEADER_SIZE + wps_field_len;
	}

	/* If nothing found from the parser then this is the WPS version 1.0 */
	printf("V1.0 ");
}

static void
bcm_is_wps_configured(uint8 *wps_ie)
{
	/* Before calling this function the test of WPS_OUI type 4 should be already done
	 * If the contents match the WPS_OUI_SC_STATE
	 */
	uint16 wps_key;
	wps_key = (wps_ie[WPS_SCSTATE_OFF]*256) + wps_ie[WPS_SCSTATE_OFF+1];
	if ((wps_ie[TLV_LEN_OFF] > (WPS_SCSTATE_OFF+5))&&
		(wps_key == WPS_ID_SC_STATE))
	{
		switch (wps_ie[WPS_SCSTATE_OFF+WPS_OUI_HEADER_SIZE])
		{
			case WPS_SCSTATE_UNCONFIGURED:
				printf("Unconfigured\n");
				break;
			case WPS_SCSTATE_CONFIGURED:
				printf("Configured\n");
				break;
			default:
				printf("Unknown State\n");
		}
	}
}

/* Looking for WPS OUI in the propriatary_ie */
static bool
bcm_is_wps_ie(uint8 *ie, uint8 **tlvs, uint32 *tlvs_len)
{
	bool retval = FALSE;
	/* If the contents match the WPS_OUI and type=4 */
	if ((ie[TLV_LEN_OFF] > (WPS_OUI_LEN+1)) &&
		!wlu_bcmp(&ie[TLV_BODY_OFF], WPS_OUI "\x04", WPS_OUI_LEN + 1)) {
		retval = TRUE;
	}

	/* point to the next ie */
	ie += ie[TLV_LEN_OFF] + TLV_HDR_LEN;
	/* calculate the length of the rest of the buffer */
	*tlvs_len -= (int)(ie - *tlvs);
	/* update the pointer to the start of the buffer */
	*tlvs = ie;

	return retval;
}

static void
wl_dump_wps(uint8* cp, uint len)
{
	uint8 *parse = cp;
	uint32 parse_len = len;
	uint8 *proprietary_ie;

	while ((proprietary_ie = wlu_parse_tlvs(parse, parse_len, DOT11_MNG_WPA_ID))) {
		if (bcm_is_wps_ie(proprietary_ie, &parse, &parse_len)) {
			/* Print WPS status */
			printf("WPS: ");
			/* Print the version get from vendor extension field */
			bcm_wps_version(proprietary_ie);
			/* Print the WPS configure or Unconfigure option */
			bcm_is_wps_configured(proprietary_ie);
			break;
		}
	}
}

void
dump_bss_info(wl_bss_info_t *bi)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	char chspec_str[CHANSPEC_STR_LEN];
	wl_bss_info_107_t *old_bi;
	int mcs_idx = 0;

	/* Convert version 107 to 109 */
	if (dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION) {
		old_bi = (wl_bss_info_107_t *)bi;
		bi->chanspec = CH20MHZ_CHSPEC(old_bi->channel);
		bi->ie_length = old_bi->ie_length;
		bi->ie_offset = sizeof(wl_bss_info_107_t);
	} else {
		/* do endian swap and format conversion for chanspec if we have
		 * not created it from legacy bi above
		 */
		bi->chanspec = wl_chspec_from_driver(bi->chanspec);
	}

	wl_format_ssid(ssidbuf, bi->SSID, bi->SSID_len);

	printf("SSID: \"%s\"\n", ssidbuf);

	printf("Mode: %s\t", capmode2str(dtoh16(bi->capability)));
	printf("RSSI: %d dBm\t", (int16)(dtoh16(bi->RSSI)));

	/*
	 * SNR has valid value in only 109 version.
	 * So print SNR for 109 version only.
	 */
	if (dtoh32(bi->version) == WL_BSS_INFO_VERSION) {
		printf("SNR: %d dB\t", (int16)(dtoh16(bi->SNR)));
	}

	printf("noise: %d dBm\t", bi->phy_noise);
	if (bi->flags) {
		bi->flags = dtoh16(bi->flags);
		printf("Flags: ");
		if (bi->flags & WL_BSS_FLAGS_FROM_BEACON) printf("FromBcn ");
		if (bi->flags & WL_BSS_FLAGS_FROM_CACHE) printf("Cached ");
		if (bi->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) printf("RSSI on-channel ");
		printf("\t");
	}
#ifdef DSLCPE	
	if (dtoh32(bi->version) != LEGACY_WL_BSS_INFO_VERSION && bi->n_cap)
		printf("Channel: %d\n", bi->ctl_ch);
	else
#endif
	printf("Channel: %s\n", wf_chspec_ntoa(bi->chanspec, chspec_str));

	printf("BSSID: %s\t", wl_ether_etoa(&bi->BSSID));

	printf("Capability: ");
	bi->capability = dtoh16(bi->capability);
	if (bi->capability & DOT11_CAP_ESS) printf("ESS ");
	if (bi->capability & DOT11_CAP_IBSS) printf("IBSS ");
	if (bi->capability & DOT11_CAP_POLLABLE) printf("Pollable ");
	if (bi->capability & DOT11_CAP_POLL_RQ) printf("PollReq ");
	if (bi->capability & DOT11_CAP_PRIVACY) printf("WEP ");
	if (bi->capability & DOT11_CAP_SHORT) printf("ShortPre ");
	if (bi->capability & DOT11_CAP_PBCC) printf("PBCC ");
	if (bi->capability & DOT11_CAP_AGILITY) printf("Agility ");
	if (bi->capability & DOT11_CAP_SHORTSLOT) printf("ShortSlot ");
	if (bi->capability & DOT11_CAP_CCK_OFDM) printf("CCK-OFDM ");
	printf("\n");

	printf("Supported Rates: ");
	dump_rateset(bi->rateset.rates, dtoh32(bi->rateset.count));
	printf("\n");
	if (dtoh32(bi->ie_length))
		wl_dump_wpa_rsn_ies((uint8 *)(((uint8 *)bi) + dtoh16(bi->ie_offset)),
		                    dtoh32(bi->ie_length));

	if (dtoh32(bi->version) != LEGACY_WL_BSS_INFO_VERSION && bi->n_cap) {
		if (bi->vht_cap)
			printf("VHT Capable:\n");
		else
			printf("HT Capable:\n");
		printf("\tChanspec: %sGHz channel %d %dMHz (0x%x)\n",
			CHSPEC_IS2G(bi->chanspec)?"2.4":"5", CHSPEC_CHANNEL(bi->chanspec),
		       (CHSPEC_IS80(bi->chanspec) ?
		        80 : (CHSPEC_IS40(bi->chanspec) ?
		              40 : (CHSPEC_IS20(bi->chanspec) ? 20 : 10))),
			bi->chanspec);
		printf("\tPrimary channel: %d\n", bi->ctl_ch);
		printf("\tHT Capabilities: ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_40MHZ)
			printf("40Mhz ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_SHORT_GI_20)
			printf("SGI20 ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_SHORT_GI_40)
			printf("SGI40 ");
		printf("\n\tSupported MCS : [ ");
		for (mcs_idx = 0; mcs_idx < (MCSSET_LEN * 8); mcs_idx++)
			if (isset(bi->basic_mcs, mcs_idx))
				printf("%d ", mcs_idx);
		printf("]\n");

		if (bi->vht_cap) {
			int i;
			uint mcs;
			printf("\tVHT Capabilities: \n");
			printf("\tSupported VHT (tx) Rates:\n");
			for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
				mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, dtoh16(bi->vht_txmcsmap));
				if (mcs != VHT_CAP_MCS_MAP_NONE)
					printf("\t\tNSS: %d MCS: %s\n", i,
						(mcs == VHT_CAP_MCS_MAP_0_9 ? "0-9" :
						(mcs == VHT_CAP_MCS_MAP_0_8 ? "0-8" : "0-7")));
			}
			printf("\tSupported VHT (rx) Rates:\n");
			for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
				mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, dtoh16(bi->vht_rxmcsmap));
				if (mcs != VHT_CAP_MCS_MAP_NONE)
					printf("\t\tNSS: %d MCS: %s\n", i,
						(mcs == VHT_CAP_MCS_MAP_0_9 ? "0-9" :
						(mcs == VHT_CAP_MCS_MAP_0_8 ? "0-8" : "0-7")));
			}
		}
	}

	if (dtoh32(bi->ie_length))
	{
		wl_dump_wps((uint8 *)(((uint8 *)bi) + dtoh16(bi->ie_offset)),
			dtoh32(bi->ie_length));
	}

	printf("\n");
}

static int
_wl_dump_lq(void *wl)
{
	int ret = BCME_OK, noise = 0;
	wl_lq_t *plq = NULL;
	void *ptr = NULL;

	memset(buf, 0, sizeof(wl_lq_t));

	/* Display stats when disabled */
	if ((ret = wlu_get(wl, WLC_GET_PHY_NOISE, &noise, sizeof(int))) < 0) {
		printf("wlc_get noise failed with retcode:%d\n", ret);
		return ret;
	}

	if ((ret = wlu_var_getbuf_sm (wl, "monitor_lq_status", NULL, 0, &ptr)) < 0) {
		printf("wlc_get lq_status failed with retcode:%d\n", ret);
		return ret;
	}

	plq = (wl_lq_t *)ptr;

	if (!plq->isvalid) {
		printf("Stats collection currently disabled"
	               "['wl monitor_lq 1' to enable statistics collection]\n");
		return ret;
	}

	noise = dtoh32(noise);
	plq->rssi[LQ_IDX_MIN] = dtoh32(plq->rssi[LQ_IDX_MIN]);
	plq->rssi[LQ_IDX_MAX] = dtoh32(plq->rssi[LQ_IDX_MAX]);
	plq->rssi[LQ_IDX_AVG] = dtoh32(plq->rssi[LQ_IDX_AVG]);

	printf("rss: %d, %d, %d\nsnr: %d, %d, %d\n",
		plq->rssi[LQ_IDX_MIN],
		plq->rssi[LQ_IDX_AVG],
		plq->rssi[LQ_IDX_MAX],
		plq->rssi[LQ_IDX_MIN]-noise,
		plq->rssi[LQ_IDX_AVG]-noise,
		plq->rssi[LQ_IDX_MAX]-noise);

	return ret;
} /* _wl_dump_lq */

static int
wl_dump_lq(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;

	UNUSED_PARAMETER(cmd);

	if (!*++argv)
		ret = _wl_dump_lq(wl);

	return ret;
} /* wl_dump_lq */

static int
wl_monitor_lq(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	char *endptr = NULL;
	char **startptr = argv;

	if (!*++startptr) { /* Get */
		wl_varint(wl, cmd, argv);
	}
	else {
		int val = *startptr[0];
		val = strtol(*startptr, &endptr, 0);

		if (*endptr != '\0') {
			return -1;
		}

		val = htod32(val);

		if (val == LQ_STOP_MONITOR) {
			ret = _wl_dump_lq(wl);
		}

		wl_varint(wl, cmd, argv); /* Standard set call after getting stats */
	}

	return ret;
} /* wl_monitor_lq */

static int
wl_dump_networks(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	char *dump_buf, *dump_buf_orig;
	uint32 status = 0;
	bool iscan = FALSE;

	dump_buf_orig = dump_buf = malloc(WL_DUMP_BUF_LEN);
	if (dump_buf == NULL) {
		fprintf(stderr, "Failed to allocate dump buffer of %d bytes\n", WL_DUMP_BUF_LEN);
		return -1;
	}

	iscan = (cmd->get != WLC_SCAN_RESULTS);
	if (iscan) {
		int buflen = 1920;	/* usually fits about 10 BSS infos */

		if (*(++argv)) {
			char *endptr = NULL;
			buflen = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				ret = -1;
				goto exit;
			}
		}
		ret = wl_get_iscan(wl, dump_buf, buflen);
	} else
		ret = wl_get_scan(wl, WLC_SCAN_RESULTS, dump_buf, WL_DUMP_BUF_LEN);

	if (ret == 0) {
		if (iscan) {
			status = dtoh32(((wl_iscan_results_t *)dump_buf)->status);
			dump_buf += OFFSETOF(wl_iscan_results_t, results);
		}
		dump_networks(dump_buf);
		if (iscan) {
			switch (status) {
			case WL_SCAN_RESULTS_PARTIAL:
				printf("iscanresults incomplete\n");
				break;
			case WL_SCAN_RESULTS_SUCCESS:
				printf("iscanresults complete\n");
				break;
			case WL_SCAN_RESULTS_PENDING:
				printf("iscanresults pending\n");
				break;
			case WL_SCAN_RESULTS_ABORTED:
				printf("iscanresults aborted\n");
				break;
			default:
				printf("iscanresults returned unknown status %d\n", status);
				break;
			}
		}
	}

exit:
	free(dump_buf_orig);
	return ret;
}

static int
wl_dump_chanlist(void *wl, cmd_t *cmd, char **argv)
{
	uint32 chan_buf[WL_NUMCHANNELS + 1];
	wl_uint32_list_t *list;
	int ret;
	uint i;

	UNUSED_PARAMETER(argv);

	list = (wl_uint32_list_t *)(void *)chan_buf;
	list->count = htod32(WL_NUMCHANNELS);
	ret = wlu_get(wl, cmd->get, chan_buf, sizeof(chan_buf));
	if (ret < 0)
		return ret;

	for (i = 0; i < dtoh32(list->count); i++)
		printf("%d ", dtoh32(list->element[i]));
	printf("\n");
	return ret;
}

static int
wl_cur_mcsset(void *wl, cmd_t *cmd, char **argv)
{
	int ret;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	memset(buf, 0, WLC_IOCTL_SMLEN);
	ret = wlu_iovar_get(wl, "cur_mcsset", &buf[0], MCSSET_LEN);
	if (ret < 0)
		return ret;

	wl_print_mcsset(buf);

	return ret;
}


static int
wl_dump_chanspecs(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t to;
	const char* fn_name = "wl_dump_chanspecs";
	wl_uint32_list_t *list;
	chanspec_t c = 0, *chanspec;
	int ret, buflen;
	uint i;
	int err, opt_err;
	bool band_set = FALSE, bw_set = FALSE;
	char abbrev[WLC_CNTRY_BUF_SZ] = ""; /* default.. current locale */
	char chspec_str[CHANSPEC_STR_LEN];

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	memset(buf, 0, WLC_IOCTL_MAXLEN);

	strcpy(buf, "chanspecs");
	buflen = strlen(buf) + 1;

	/* toss the command name */
	argv++;

	/* Validate arguments if any */
	if (*argv) {
		miniopt_init(&to, fn_name, NULL, FALSE);
		while ((opt_err = miniopt(&to, argv)) != -1) {
			if (opt_err == 1) {
				err = -1;
				goto exit;
			}
			argv += to.consumed;

			if (to.opt == 'b') {
				if (!to.good_int) {
					fprintf(stderr,
					        "%s: could not parse \"%s\" as an int for band\n",
					        fn_name, to.valstr);
					err = -1;
					goto exit;
				}
				if ((to.val != 5) && (to.val != 2)) {
					fprintf(stderr,
					        "%s: invalid band %d\n",
					        fn_name, to.val);
					err = -1;
					goto exit;
				}
				if (to.val == 5)
					c |= WL_CHANSPEC_BAND_5G;
				else
					c |= WL_CHANSPEC_BAND_2G;
				band_set = TRUE;
			}
			if (to.opt == 'w') {
				if (!to.good_int) {
					fprintf(stderr,
					        "%s: could not parse \"%s\" as an int for"
					        " bandwidth\n",
					        fn_name, to.valstr);
					err = -1;
					goto exit;
				}
				if ((to.val != 20) && (to.val != 40) && (to.val != 80)) {
					fprintf(stderr,
					        "%s: invalid bandwidth %d\n",
					        fn_name, to.val);
					err = -1;
					goto exit;
				}
				if (to.val == 20)
					c |= WL_CHANSPEC_BW_20;
				else if (to.val == 40)
					c |= WL_CHANSPEC_BW_40;
				else {
					if (ioctl_version == 1) {
						fprintf(stderr,
						        "%s: bandwidth 80 MHz is not supported by "
						        "this version driver.\n",
						        fn_name);
						err = USAGE_ERROR;
						goto exit;

					}
					c |= WL_CHANSPEC_BW_80;
				}

				bw_set = TRUE;
			}
			if (to.opt == 'c') {
				if (!to.valstr) {
					fprintf(stderr,
					        "%s: please provide country abbrev \n", fn_name);
					err = -1;
					goto exit;
				}
#ifdef DSLCPE
				if(strlen(to.valstr)<WLC_CNTRY_BUF_SZ+4)
					strcpy(abbrev, to.valstr);
				else
					strncpy(abbrev, to.valstr, WLC_CNTRY_BUF_SZ+3);
				abbrev[WLC_CNTRY_BUF_SZ +4] = '\0';
#else
				strncpy(abbrev, to.valstr, WLC_CNTRY_BUF_SZ - 1);
				abbrev[WLC_CNTRY_BUF_SZ - 1] = '\0';
#endif
			}
		}
		if (!bw_set || !band_set) {
			if (!band_set)
				fprintf(stderr, "%s: you need to set a band, '-b <5|2>'\n",
				        fn_name);
			if (!bw_set)
				fprintf(stderr,
				        "%s: you need to set a bandwidth, '-w <20|40|80>'\n",
				        fn_name);
			err = -1;
			goto exit;
		}
	}

	/* convert chanspec to legacy if needed */
	if (c != 0) {
		c = wl_chspec_to_driver(c);
		if (c == INVCHANSPEC) {
			err = USAGE_ERROR;
			goto exit;
		}
	}

	/* Add chanspec argument */
	chanspec = (chanspec_t *) (buf + buflen);
	*chanspec = c;
	buflen += (sizeof(chanspec_t));

	/* Add country abbrev */
#ifdef DSLCPE
	strncpy(buf + buflen, abbrev, WLC_CNTRY_BUF_SZ+4);
	buflen += WLC_CNTRY_BUF_SZ+4;
#else
	strncpy(buf + buflen, abbrev, WLC_CNTRY_BUF_SZ);
	buflen += WLC_CNTRY_BUF_SZ;
#endif

	/* Add list */
	list = (wl_uint32_list_t *)(buf + buflen);
	list->count = htod32(WL_NUMCHANSPECS);
	buflen += sizeof(uint32)*(WL_NUMCHANSPECS + 1);

	ret = wlu_get(wl, WLC_GET_VAR, &buf[0], buflen);
	if (ret < 0)
		return ret;

	list = (wl_uint32_list_t *)buf;
	for (i = 0; i < dtoh32(list->count); i++) {
		c = wl_chspec32_from_driver(list->element[i]);
		wf_chspec_ntoa(c, chspec_str);
		printf("%s (0x%04x)\n", chspec_str, c);
	}
	printf("\n");
	return ret;

exit:
	return err;
}

static int
wl_channels_in_country(void *wl, cmd_t *cmd, char **argv)
{
	wl_channels_in_country_t *cic;
	int ret;
	uint i, len;

	cic = (wl_channels_in_country_t *)buf;
	cic->buflen = WLC_IOCTL_MAXLEN;
	cic->count = 0;

	/* country abbrev must follow */
	if (!*++argv) {
		fprintf(stderr, "missing country abbrev\n");
		return -1;
	}

	len = strlen(*argv);
	if ((len > 3) || (len < 2)) {
		fprintf(stderr, "invalid country abbrev: %s\n", *argv);
		return -1;
	}

	strcpy(cic->country_abbrev, *argv);

	/* band must follow */
	if (!*++argv) {
		fprintf(stderr, "missing band\n");
		return -1;
	}

	if (!stricmp(*argv, "a"))
		cic->band = WLC_BAND_5G;
	else if (!stricmp(*argv, "b"))
		cic->band = WLC_BAND_2G;
	else {
		fprintf(stderr, "unsupported band: %s\n", *argv);
		return -1;
	}

	cic->buflen = htod32(cic->buflen);
	cic->band = htod32(cic->band);
	cic->count = htod32(cic->count);
	ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN);
	if (ret < 0)
		return ret;

	for (i = 0; i < dtoh32(cic->count); i++)
		printf("%d ", dtoh32(cic->channel[i]));
	printf("\n");

	return ret;
}

int
wl_get_scan(void *wl, int opc, char *scan_buf, uint buf_len)
{
	wl_scan_results_t *list = (wl_scan_results_t*)scan_buf;
	int ret;

	list->buflen = htod32(buf_len);
	ret = wlu_get(wl, opc, scan_buf, buf_len);
	if (ret < 0)
		return ret;
	ret = 0;

	list->buflen = dtoh32(list->buflen);
	list->version = dtoh32(list->version);
	list->count = dtoh32(list->count);
	if (list->buflen == 0) {
		list->version = 0;
		list->count = 0;
	} else if (list->version != WL_BSS_INFO_VERSION &&
	           list->version != LEGACY2_WL_BSS_INFO_VERSION &&
	           list->version != LEGACY_WL_BSS_INFO_VERSION) {
		fprintf(stderr, "Sorry, your driver has bss_info_version %d "
			"but this program supports only version %d.\n",
			list->version, WL_BSS_INFO_VERSION);
		list->buflen = 0;
		list->count = 0;
	}

	return ret;
}

static int
wl_get_iscan(void *wl, char *scan_buf, uint buf_len)
{
	wl_iscan_results_t list;
	wl_scan_results_t *results;
	int ret;

	memset(&list, '\0', sizeof(list));
	list.results.buflen = htod32(buf_len);
	ret = wlu_iovar_getbuf(wl, "iscanresults", &list, WL_ISCAN_RESULTS_FIXED_SIZE,
	                      scan_buf, WLC_IOCTL_MAXLEN);

	if (ret < 0)
		return ret;

	ret = 0;

	results = &((wl_iscan_results_t*)scan_buf)->results;
	results->buflen = dtoh32(results->buflen);
	results->version = dtoh32(results->version);
	results->count = dtoh32(results->count);
	if (results->buflen == 0) {
		printf("wl_get_iscan buflen 0\n");
		results->version = 0;
		results->count = 0;
	} else if (results->version != WL_BSS_INFO_VERSION &&
	           results->version != LEGACY2_WL_BSS_INFO_VERSION &&
	           results->version != LEGACY_WL_BSS_INFO_VERSION) {
		fprintf(stderr, "Sorry, your driver has bss_info_version %d "
			"but this program supports only version %d.\n",
			results->version, WL_BSS_INFO_VERSION);
		results->buflen = 0;
		results->count = 0;
	}

	return ret;
}

static int
wl_spect(void *wl, cmd_t *cmd, char **argv)
{
	int ret, spect;
	char *endptr = NULL;

	if (!*++argv) {
		if ((ret = wlu_get(wl, cmd->get, &spect, sizeof(spect))) < 0) {
			return ret;
		}

		spect = dtoh32(spect);
		switch (spect) {
		case SPECT_MNGMT_OFF:
			printf("Off\n");
			break;

		case SPECT_MNGMT_LOOSE_11H:
			printf("Loose interpretation of 11h spec - may join non 11h AP.\n");
			break;

		case SPECT_MNGMT_STRICT_11H:
			printf("Strict interpretation of 11h spec - may not join non 11h AP.\n");
			break;

		case SPECT_MNGMT_STRICT_11D:
			printf("802.11d mode\n");
			break;

		case SPECT_MNGMT_LOOSE_11H_D:
			printf("Loose interpretation of 11h+d spec - may join non-11h APs\n");
			break;

		default:
			printf("invalid value 0x%x\n", spect);
			return -1;
		}
		return (0);
	} else {
		spect = strtol(*argv, &endptr, 0);
		if (*endptr != '\0')
			return (-1);

		if (spect < SPECT_MNGMT_OFF || spect > SPECT_MNGMT_LOOSE_11H_D)
			return (-1);

		spect = htod32(spect);
		return wlu_set(wl, cmd->set, &spect, sizeof(spect));
	}
}

static int
wl_status(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	struct ether_addr bssid;
	wlc_ssid_t ssid;
	char ssidbuf[SSID_FMT_BUF_LEN];
	wl_bss_info_t *bi;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	if ((ret = wlu_get(wl, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN)) == 0) {
		/* The adapter is associated. */
		*(uint32*)buf = htod32(WLC_IOCTL_MAXLEN);
		if ((ret = wlu_get(wl, WLC_GET_BSS_INFO, buf, WLC_IOCTL_MAXLEN)) < 0)
			return ret;

		bi = (wl_bss_info_t*)(buf + 4);
		if (dtoh32(bi->version) == WL_BSS_INFO_VERSION ||
		    dtoh32(bi->version) == LEGACY2_WL_BSS_INFO_VERSION ||
		    dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION)
			dump_bss_info(bi);
		else
			fprintf(stderr, "Sorry, your driver has bss_info_version %d "
				"but this program supports only version %d.\n",
				bi->version, WL_BSS_INFO_VERSION);
	} else {
		printf("Not associated. Last associated with ");

		if ((ret = wlu_get(wl, WLC_GET_SSID, &ssid, sizeof(wlc_ssid_t))) < 0) {
			printf("\n");
			return ret;
		}

		wl_format_ssid(ssidbuf, ssid.SSID, dtoh32(ssid.SSID_len));
		printf("SSID: \"%s\"\n", ssidbuf);
	}

	return 0;
}

static int
wl_deauth_rc(void *wl, cmd_t *cmd, char **argv)
{
	scb_val_t scb_val;
	int ret;

	if (!*++argv) {
		fprintf(stderr, "STA MAC not specified, deauth all\n");
		ret = wlu_set(wl, WLC_SCB_DEAUTHENTICATE, (void *)&ether_bcast,
			ETHER_ADDR_LEN);
		return 0;

	} else if (!wl_ether_atoe(*argv, &scb_val.ea)) {
		fprintf(stderr, "Malformed STA MAC parameter\n");
		ret = -1;

	} else if (!*++argv) {
		/* No reason code furnished, so driver will use its default */
		ret = wlu_set(wl, WLC_SCB_DEAUTHENTICATE, &scb_val.ea,
			ETHER_ADDR_LEN);

	} else {
		scb_val.val = htod32((uint32)strtoul(*argv, NULL, 0));
		ret = wlu_set(wl, cmd->set, &scb_val, sizeof(scb_val));
	}
	return ret;
}

static int
wl_wpa_auth(void *wl, cmd_t *cmd, char **argv)
{
	int bsscfg_idx = 0;
	int consumed;
	int wpa_auth = 0;
	int ret = 0;
	int i;
	static struct {
		int val;
		const char *name;
	} auth_mode[] =
		  /* Keep the numeric values in the staticly initialized
		   * help string consistent.  Unfortunately, there isn't
		   * an automatic way for that.
		   */
		{{WPA_AUTH_NONE,	"WPA-NONE"},
		 {WPA_AUTH_UNSPECIFIED,	"WPA-802.1x"},
		 {WPA_AUTH_PSK,		"WPA-PSK"},
		 {WPA2_AUTH_UNSPECIFIED, "WPA2-802.1x"},
		 {WPA2_AUTH_PSK,	"WPA2-PSK"},
#ifdef BCMWAPI_WAI
		{WAPI_AUTH_UNSPECIFIED, "WAPI-AS"},
		{WAPI_AUTH_PSK,        "WAPI-PSK"},
#endif /* BCMWAPI_WAI */
		 {WPA2_AUTH_FT,		"FT"},
		 {WPA_AUTH_DISABLED,	"disabled"}};

	/* skip the command name */
	argv++;

	/* parse a bsscfg_idx option if present */
	if ((ret = wl_cfg_option(argv, cmd->name, &bsscfg_idx, &consumed)) != 0)
		return ret;

	argv += consumed;

	if (!*argv) {
		/* no arg, so this is a GET. */

		if (!consumed)
			ret = wlu_iovar_getint(wl, "wpa_auth", &wpa_auth);
		else
			ret = wl_bssiovar_getint(wl, "wpa_auth", bsscfg_idx, &wpa_auth);

		if (ret < 0)
			return ret;

		/* Show all AKM suites enabled */
		printf("0x%x", wpa_auth);

		if (wpa_auth == WPA_AUTH_DISABLED)
			printf(" Disabled");

		for (i = 0; i < (int)ARRAYSIZE(auth_mode); i++) {
			if (wpa_auth & auth_mode[i].val)
				printf(" %s", auth_mode[i].name);
		}

		printf("\n");
		return ret;


	} else {
		/* there's an arg, so this is a SET. */
		ret = 1;

		/* Validate the user input range */
		if (isdigit((int)*argv[0])) {
			unsigned int range = 0;

			/* param is a number; look for value in the list */
			wpa_auth = strtoul(*argv, NULL, 0);

			/* Validate the user input range */

			for (i = 0; i < (int)ARRAYSIZE(auth_mode); i++)
				range |= auth_mode[i].val;

			range = (~range) & 0xFFFF;

			if (range & wpa_auth) {
				ret = 1;
				goto usage;
			} else {
				ret = 0;
			}

		} else {

			int arg_count = 0;
			char** p_argv;
			int j = 0;
			unsigned int range = 0;

			wpa_auth = 0;
			p_argv = argv;

			for (i = 0; i < (int)ARRAYSIZE(auth_mode); i++)
				range |= auth_mode[i].val;

			range = (~range) & (0xFFFF);

			while (*p_argv) {
				arg_count++;
				p_argv++;
				}

			p_argv = argv;

			for (j = 0; j < arg_count; j++) {
				bool found = FALSE;

				argv = p_argv + j;

				/* treat param as string to be matched in list */
				for (i = 0; i < (int)ARRAYSIZE(auth_mode); i++) {
					if (!stricmp(auth_mode[i].name, *argv)) {

						found = TRUE;
						wpa_auth |= auth_mode[i].val;
						ret = 0;

						/* traverse the list */
						argv++;
						if (!*argv)
							break;
					}
				}

				if ((found == FALSE) || (range & wpa_auth))
					goto usage;
			}

		}
		if (ret)
			fprintf(stderr, "%s is not a valid WPA auth mode\n", *argv);
		else {
			if (!consumed)
				ret = wlu_iovar_setint(wl, "wpa_auth", wpa_auth);
			else
				ret = wl_bssiovar_setint(wl, "wpa_auth", bsscfg_idx, wpa_auth);
		}
	}

	return ret;

usage:
	fprintf(stderr, "Inavlid user argument.\n");
	fprintf(stderr, "Values may be a bitvector or list of names from the set.\n");

	for (i = 0; i < (int)ARRAYSIZE(auth_mode); i++) {
		fprintf(stderr, "\n0x%04x  %s", auth_mode[i].val, auth_mode[i].name);
	}

	printf("\n");
	return ret;
}

static int
wl_set_pmk(void *wl, cmd_t *cmd, char **argv)
{
	wsec_pmk_t psk;
	size_t key_len;

	if (!*++argv) {
		return -1;
	}
	key_len = strlen(*argv);
	if (key_len < WSEC_MIN_PSK_LEN || key_len > WSEC_MAX_PSK_LEN) {
		fprintf(stderr, "passphrase must be between %d and %d characters long\n",
		       WSEC_MIN_PSK_LEN, WSEC_MAX_PSK_LEN);
		return -1;
	}
	psk.key_len = htod16((ushort) key_len);
	psk.flags = htod16(WSEC_PASSPHRASE);
	memcpy(psk.key, *argv, key_len);
	return wlu_set(wl, cmd->set, &psk, sizeof(psk));
}

static int
wl_wsec(void *wl, cmd_t *cmd, char **argv)
{
	int wsec;
	int bsscfg_idx = 0;
	int consumed;
	char *endptr = NULL;
	int error;

	UNUSED_PARAMETER(cmd);

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((error = wl_cfg_option(argv, "wsec", &bsscfg_idx, &consumed)) != 0)
		return error;

	argv += consumed;

	if (!*argv) {
		/* This is a GET */
		if (consumed == 0) {
			error = wlu_get(wl, WLC_GET_WSEC, &wsec, sizeof(uint32));
			wsec = dtoh32(wsec);
		}
		else
			error = wl_bssiovar_getint(wl, "wsec", bsscfg_idx, &wsec);

		if (!error)
			wl_printint(wsec);
	} else {
		/* This is a SET */
		if (!stricmp(*argv, "off"))
			wsec = 0;
		else {
			wsec = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return -1;
			}
		}

		if (consumed == 0) {
			wsec = htod32(wsec);
			error = wlu_set(wl, WLC_SET_WSEC, &wsec, sizeof(uint32));
		}
		else
			error = wl_bssiovar_setint(wl, "wsec", bsscfg_idx, wsec);
	}

	return error;
}

static int
parse_wep(char **argv, wl_wsec_key_t *key, bool options)
{
	char hex[] = "XX";
	unsigned char *data = key->data;
	char *keystr = *argv;

	switch (strlen(keystr)) {
	case 5:
	case 13:
	case 16:
		key->len = strlen(keystr);
		memcpy(data, keystr, key->len + 1);
		break;
	case 12:
	case 28:
	case 34:
	case 66:
		/* strip leading 0x */
		if (!strnicmp(keystr, "0x", 2))
			keystr += 2;
		else
			return -1;
		/* fall through */
	case 10:
	case 26:
	case 32:
	case 64:
		key->len = strlen(keystr) / 2;
		while (*keystr) {
			strncpy(hex, keystr, 2);
			*data++ = (char) strtoul(hex, NULL, 16);
			keystr += 2;
		}
		break;
	default:
		return -1;
	}

	switch (key->len) {
	case 5:
		key->algo = CRYPTO_ALGO_WEP1;
		break;
	case 13:
		key->algo = CRYPTO_ALGO_WEP128;
		break;
	case 16:
		/* default to AES-CCM */
		key->algo = CRYPTO_ALGO_AES_CCM;
		break;
	case 32:
		key->algo = CRYPTO_ALGO_TKIP;
		break;
	default:
		return -1;
	}

	/* Set as primary key by default */
	key->flags |= WL_PRIMARY_KEY;

	if (options) {
		/* Get options */
		while (*++argv) {
			if (!strnicmp("ccm", *argv, 3) && key->len == 16)
				key->algo = CRYPTO_ALGO_AES_CCM;
#ifdef BCMWAPI_WPI
			else if (!strnicmp("wapi", *argv, 4) && key->len == 32)
				key->algo = CRYPTO_ALGO_SMS4;
#endif /* BCMWAPI_WPI */
			else if (!strnicmp("ocb", *argv, 3) && key->len == 16)
				key->algo = CRYPTO_ALGO_AES_OCB_MPDU;
			else if (!strnicmp("notx", *argv, 4))
				key->flags &= ~WL_PRIMARY_KEY;
			else if (!wl_ether_atoe(*argv, &key->ea))
				memset(&key->ea, 0, ETHER_ADDR_LEN);
		}
	}

	return 0;
}

static int
wl_primary_key(void *wl, cmd_t *cmd, char **argv)
{
	int i, val, ret = 0;

	if (!*++argv) {
		i = 0;
		do {
			val = htod32(i);
			if (wlu_get(wl, cmd->get, &val, sizeof(val)) < 0) {
				return -1;
			}
			if (dtoh32(val)) {
				printf("Key %d is primary\n", i);
				return 0;
			}
		} while (++i < DOT11_MAX_DEFAULT_KEYS);
		printf("No primary key set\n");

	} else {
		val = htod32(atoi(*argv));
		ret = wlu_set(wl, cmd->set, &val, sizeof(val));
	}
	return ret;
}

static int
wl_addwep(void *wl, cmd_t *cmd, char **argv)
{
	wl_wsec_key_t key;
	int bsscfg_idx = 0;
	int consumed;
	int error;

	memset(&key, 0, sizeof(key));

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((error = wl_cfg_option(argv, "addwep", &bsscfg_idx, &consumed)) != 0)
		return error;

	argv += consumed;

	/* GET operation not allowed */
	if (!*argv)
		return -1;

	key.index = atoi(*argv++);

	if (!*argv) {
		fprintf(stderr, "No key specified\n");
		return -1;
	}
	if (parse_wep(argv, &key, TRUE))
		return -1;

	key.index = htod32(key.index);
	key.len = htod32(key.len);
	key.algo = htod32(key.algo);
	key.flags = htod32(key.flags);

	if (consumed == 0) {
		error = wlu_set(wl, cmd->set, &key, sizeof(key));
	} else {
		error = wlu_bssiovar_setbuf(wl, "wsec_key", bsscfg_idx,
			&key, sizeof(key), buf, WLC_IOCTL_MAXLEN);
	}

	return error;
}

static int
wl_rmwep(void *wl, cmd_t *cmd, char **argv)
{
	wl_wsec_key_t key;
	int bsscfg_idx = 0;
	int consumed;
	int error;

	memset(&key, 0, sizeof(key));

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((error = wl_cfg_option(argv, "rmwep", &bsscfg_idx, &consumed)) != 0)
		return error;

	argv += consumed;

	/* GET operation not allowed */
	if (!*argv)
		return -1;

	key.index = htod32(atoi(*argv++));

	if (*argv) {
		if (!(wl_ether_atoe(*argv, &key.ea)))
			return -1;
	}

	if (consumed == 0) {
		error = wlu_set(wl, cmd->set, &key, sizeof(key));
	} else {
		error = wlu_var_setbuf(wl, "wsec_key", &key, sizeof(key));
	}

	return error;
}

static struct {
	uint value;
	const char *string;
} wsec_test[] = {
	{WSEC_GEN_MIC_ERROR, 	"mic_error"},
	{WSEC_GEN_REPLAY, 	"replay"},
	{WSEC_GEN_ICV_ERROR, "icv_error"},
	{WSEC_GEN_MFP_ACT_ERROR, "act_error"},
	{WSEC_GEN_MFP_DISASSOC_ERROR, "disassoc_error"},
	{WSEC_GEN_MFP_DEAUTH_ERROR, "deauth_error"},
	{0,		NULL}
};


static int
wl_wsec_test(void *wl, cmd_t *cmd, char **argv)
{
	wl_wsec_key_t *key;
	int i, len;
	char *endptr = NULL, *wsec_buf = NULL;
	uint32 val, last_val;
	int err = 0;

	if (!*++argv)
		goto usage;

	val = strtol(*argv, &endptr, 0);
	if (endptr == *argv) {
		/* the value string was not parsed by strtol */
		for (i = 0; wsec_test[i].value; i++)
			if (stricmp(wsec_test[i].string, *argv) == 0) {
				val = wsec_test[i].value;
				break;
			}
		if (wsec_test[i].value == 0)
			goto usage;
	}
	++argv;

	switch (val) {
	case WSEC_GEN_REPLAY:
	case WSEC_GEN_MIC_ERROR:
	case WSEC_GEN_ICV_ERROR:
	case WSEC_GEN_MFP_ACT_ERROR:
	case WSEC_GEN_MFP_DISASSOC_ERROR:
	case WSEC_GEN_MFP_DEAUTH_ERROR:
		if (!*argv) {
			fprintf(stderr, "insufficient arguments\n");
			return -1;
		}
		len = sizeof(wl_wsec_key_t) + 4;
		wsec_buf = malloc(len);
		*(uint32 *)wsec_buf = htod32(val);
		key = (wl_wsec_key_t *)&wsec_buf[4];
		memset(key, 0, sizeof(wl_wsec_key_t));
		/* If it doesn't look like an ether addr, suppose it's a key index */
		if (!(wl_ether_atoe(*argv, &key->ea))) {
			memset(&key->ea, 0, ETHER_ADDR_LEN);
			key->index = htod32(atoi(*argv));
		}
		break;
	default:
		goto usage;
	}

	err = wlu_set(wl, cmd->set, wsec_buf, len);
	free(wsec_buf);
	goto exit;

usage:
	fprintf(stderr, "wsec test_type may be a number or name from the following set:");
	last_val = 0xffffffff;
	for (i = 0; (val = wsec_test[i].value); i++) {
		if (val != last_val)
			fprintf(stderr, "\n0x%04x %s", val, wsec_test[i].string);
		else
			fprintf(stderr, ", %s", wsec_test[i].string);
		last_val = val;
	}
	fprintf(stderr, "\n");

exit:
	return err;
}

static int
wl_keys(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	uint i, j;
	union {
		int index;
		wl_wsec_key_t key;
	} u;
	int wep_is_on = 0;
	const char *addr;
	int empty_first, empty_last;
	bool empty;

	UNUSED_PARAMETER(argv);

	if ((ret = wlu_iovar_getint(wl, "wsec", &wep_is_on)) < 0)
		fprintf(stderr, "Could not query wsec status.\n");

	empty_first = empty_last = -1;

	for (i = 0; ; i++) {	/* The upper limit is not known here. */
		u.index = htod32(i);
		ret = wlu_get(wl, cmd->get, &u, sizeof(u));
		empty = (ETHER_ISNULLADDR(&u.key.ea) && dtoh32(u.key.algo) == CRYPTO_ALGO_OFF);

		if (empty_first != -1 &&
		    (ret < 0 || !empty)) {
			if (empty_first == empty_last)
				printf("%3d: <empty>\n", empty_first);
			else
				printf("%3d - %3d: <empty>\n", empty_first, empty_last);
			empty_first = empty_last = -1;
		}

		if (ret < 0) {
			/* If at least one beyond the defaults could be read,
			 * not knowing the limit must have been the error.
			 */
			return (i >= DOT11_MAX_DEFAULT_KEYS) ? 0 : ret;
		}

		/* Key may not have been set yet */
		if (i < DOT11_MAX_DEFAULT_KEYS)
			addr = "(default)";
		else if (empty) {
			if (empty_first == -1) empty_first = i;
			empty_last = i;
			continue;
		} else
			addr = wl_ether_etoa(&u.key.ea);

		printf("%3d: %-17s Key %d: %s ", i, addr, dtoh32(u.key.index),
		       bcm_crypto_algo_name(dtoh32(u.key.algo)));

		if (wep_is_on && dtoh32(u.key.flags) & WL_PRIMARY_KEY)
			printf("*");
		printf("\t");

		if (dtoh32(u.key.len) == 0)
			printf("No key present");
		else {
			if (dtoh32(u.key.flags) & WL_SOFT_KEY)
				printf("soft ");
			printf("len %d, data 0x", dtoh32(u.key.len));
			for (j = 0; j < dtoh32(u.key.len); j++)
				printf("%02X", u.key.data[j]);

			for (j = 0; j < dtoh32(u.key.len); j++)
				if (!isprint(u.key.data[j]))
					break;
			if (j == dtoh32(u.key.len))
				printf(" (%.*s)", (int)dtoh32(u.key.len), u.key.data);

		}

		printf("\n");
	}
}

static int
wl_tsc(void *wl, cmd_t *cmd, char **argv)
{
	union {
		int32 index;
		uint8 tsc[DOT11_WPA_KEY_RSC_LEN];
	} u;
	uint32 hi, lo;
	int idx, ret;

	if (!*++argv)
		return -1;
	idx = atoi(*argv);
	if (idx < 0) {
		fprintf(stderr, "Key index %d out of range. Should be positive.\n", idx);
		return -1;
	}
	u.index = htod32(idx);
	if ((ret = wlu_get(wl, cmd->get, &u, sizeof(u))) < 0)
		return ret;
	lo = u.tsc[0] | (u.tsc[1] << 8) | (u.tsc[2] << 16) | (u.tsc[3] << 24);
	hi = u.tsc[4] | (u.tsc[5] << 8) | (u.tsc[6] << 16) | (u.tsc[7] << 24);

	printf("Key %d TSC: 0x%04x:%08x\n", idx, hi, lo);
	return 0;
}

/* print power offset for for a given bandwidth */
static void
wl_txppr_print_bw(ppr_t *ppr, int cck, uint flags, wl_tx_bw_t bw)
{
	uint i;
	uint n = WL_NUM_2x2_ELEMENTS;
	uint offset = 0;
	int8 *ptr, *vhtptr;
	const char *str = "";
	bool siso = ((flags & WL_TX_POWER_F_MIMO) == 0);
	bool vht = ((flags & WL_TX_POWER_F_VHT) != 0);
	ppr_ofdm_rateset_t ofdm_rate;
	ppr_ofdm_rateset_t ofdm_cdd_rate;
	ppr_vht_mcs_rateset_t vhtrate;

	if (!siso) {
		offset = n = WL_NUM_3x3_ELEMENTS;
	}

	if (cck) {
		ppr_dsss_rateset_t rate;
		ppr_get_dsss(ppr, bw, WL_TX_CHAINS_1, &rate);
		printf("CCK          %2d %2d %2d %2d\n", rate.pwr[0], rate.pwr[1],
		       rate.pwr[2], rate.pwr[3]);
		if (!siso) {
			ppr_get_dsss(ppr, bw, WL_TX_CHAINS_2, &rate);
			printf("CCK CDD 1x2  %2d %2d %2d %2d\n",
			       rate.pwr[0], rate.pwr[1],
			       rate.pwr[2], rate.pwr[3]);
			ppr_get_dsss(ppr, bw, WL_TX_CHAINS_3, &rate);
			printf("CCK CDD 1x3  %2d %2d %2d %2d\n",
			       rate.pwr[0], rate.pwr[1],
			       rate.pwr[2], rate.pwr[3]);
		}
	}
	ppr_get_ofdm(ppr, bw, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_rate);
	printf("OFDM         %2d %2d %2d %2d %2d %2d %2d %2d\n",
	       ofdm_rate.pwr[0], ofdm_rate.pwr[1], ofdm_rate.pwr[2], ofdm_rate.pwr[3],
	       ofdm_rate.pwr[4], ofdm_rate.pwr[5], ofdm_rate.pwr[6], ofdm_rate.pwr[7]);
	ppr_get_ofdm(ppr, bw, WL_TX_MODE_CDD, WL_TX_CHAINS_2, &ofdm_cdd_rate);
	printf("OFDM-CDD     %2d %2d %2d %2d %2d %2d %2d %2d\n",
	       ofdm_cdd_rate.pwr[0], ofdm_cdd_rate.pwr[1], ofdm_cdd_rate.pwr[2],
	       ofdm_cdd_rate.pwr[3],
	       ofdm_cdd_rate.pwr[4], ofdm_cdd_rate.pwr[5], ofdm_cdd_rate.pwr[6],
	       ofdm_cdd_rate.pwr[7]);
	for (i = 0; i < n; i++) {
		wl_tx_nss_t nss;
		wl_tx_mode_t mode;
		wl_tx_chains_t chains;
		switch (i + offset) {
			case 0:
				str = "MCS-SISO    ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_1;
				ptr = vhtrate.pwr;
				vhtptr = NULL;
				break;
			case 1:
				str = "MCS-CDD     ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_CDD;
				chains = WL_TX_CHAINS_2;
				ptr = vhtrate.pwr;
				vhtptr = NULL;
				break;
			case 2:
				str = "MCS STBC    ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_STBC;
				chains = WL_TX_CHAINS_2;
				ptr = vhtrate.pwr;
				vhtptr = NULL;
				break;
			case 3:
				str = "MCS 8~15    ";
				nss = WL_TX_NSS_2;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_2;
				ptr = vhtrate.pwr;
				vhtptr = NULL;
				break;
			case 4:
			case 5:
				ptr = NULL;
				vhtptr = NULL;
				break;
			case 6:
				str = "1 Nsts 1 Tx ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_1;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			case 7:
				str = "1 Nsts 2 Tx ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_CDD;
				chains = WL_TX_CHAINS_2;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			case 8:
				str = "1 Nsts 3 Tx ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_CDD;
				chains = WL_TX_CHAINS_3;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			case 9:
				str = "2 Nsts 2 Tx ";
				nss = WL_TX_NSS_2;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_2;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			case 10:
				str = "2 Nsts 3 Tx ";
				nss = WL_TX_NSS_2;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_3;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			case 11:
				str = "3 Nsts 3 Tx ";
				nss = WL_TX_NSS_3;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_3;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			default:
				ptr = NULL;
				vhtptr = NULL;
				break;
		}
		if (ptr == NULL)
			continue;
		ppr_get_vht_mcs(ppr, bw, nss, mode, chains, &vhtrate);
		printf("%s %2d %2d %2d %2d %2d %2d %2d %2d",
		       str, ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7]);
		if (vht && vhtptr)
			printf(" %2d %2d", vhtptr[0], vhtptr[1]);
		printf("\n");
	}
}

static void
wl_txppr_print(ppr_t *ppr, int cck, uint flags)
{
	printf("20MHz:\n");
	wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_20);
	printf("\n40MHz:\n");
	wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_40);

	/* MCS32 value is obsoleted */
	/* printf("MCS32        %2d\n", ppr->mcs32); */

	printf("\n20 in 40MHz:\n");
	wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_20IN40);
	printf("\n");
}

static int
wl_get_current_txppr(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	uint flags;
	chanspec_t chanspec;
	char chanspec_str[CHANSPEC_STR_LEN];
	uint32 pprsize = ppr_ser_size_by_bw(ppr_get_max_bw());
	wl_txppr_t *wl_txppr;
	ppr_t *pprptr = NULL;

	wl_txppr = (wl_txppr_t *)malloc(sizeof(*wl_txppr) + pprsize);
	memset(wl_txppr, 0, sizeof(*wl_txppr));
	wl_txppr->buflen = pprsize;
	if (ppr_init_ser_mem_by_bw(wl_txppr->pprbuf, ppr_get_max_bw(), pprsize) != BCME_OK) {
		free(wl_txppr);
		return IOCTL_ERROR;
	}

	if (WLC_IOCTL_MAXLEN < sizeof(wl_txppr_t) + pprsize) {
		free(wl_txppr);
		return IOCTL_ERROR;
	}

	argv++;
	if (*argv)
		fprintf(stderr, "Ignoring arguments for %s\n", cmd->name);

	wl_txppr->ver = WL_TXPPR_VERSION;
	wl_txppr->len = WL_TXPPR_LENGTH;
	if ((err = wlu_iovar_getbuf(wl, "curppr", wl_txppr, sizeof(*wl_txppr) + pprsize,
		buf, WLC_IOCTL_MAXLEN)) < 0) {
		free(wl_txppr);
		return err;
	}

	/* the input buffer is no longer needed, output results are in buf */
	free(wl_txppr);
	wl_txppr = (wl_txppr_t *)buf;

	/* parse */
	wl_txppr->flags = dtoh32(wl_txppr->flags);
	wl_txppr->chanspec = wl_chspec_from_driver(wl_txppr->chanspec);
	wl_txppr->local_chanspec = wl_chspec_from_driver(wl_txppr->local_chanspec);

	chanspec = wl_txppr->chanspec;
	flags = (wl_txppr->flags & WL_TX_POWER_F_VHT) |
	       (wl_txppr->flags & WL_TX_POWER_F_HT) |
	       (wl_txppr->flags & WL_TX_POWER_F_MIMO) |
	       (wl_txppr->flags & WL_TX_POWER_F_SISO);

	/* dump */
	printf("Current channel:\t %s\n",
	       wf_chspec_ntoa(wl_txppr->chanspec, chanspec_str));
	printf("BSS channel:\t\t %s\n",
	       wf_chspec_ntoa(wl_txppr->local_chanspec, chanspec_str));
	printf("Power/Rate Dump (in 1/4dB): Channel %d\n", CHSPEC_CHANNEL(chanspec));
	if (ppr_deserialize_create(NULL, wl_txppr->pprbuf, pprsize, &pprptr) == BCME_OK) {
		wl_txppr_print(pprptr, CHSPEC_IS2G(chanspec), flags);
		ppr_delete(NULL, pprptr);
	}
	return err;
}

/* This version number must be incremented for every
 * modification to the curpower output format. Minor changes
 * warrant a decimal point increment. Major (potential
 * script-breaking) changes should be met with a major increment.
 */
#define CURPOWER_OUTPUT_FORMAT_VERSION "5.0"

#define RATE_STR_LEN 64

static int
wl_get_current_power(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int mimo;
	int i;
	chanspec_t chanspec;
	char chanspec_str[CHANSPEC_STR_LEN];
	bool verbose = FALSE;
	bool brief = FALSE;
	int16 power_target;
	char rate_str[RATE_STR_LEN];

	size_t pprsize = ppr_ser_size_by_bw(ppr_get_max_bw());
	tx_pwr_rpt_t *ppr_wl = (tx_pwr_rpt_t *)malloc(sizeof(tx_pwr_rpt_t) + pprsize*3);
	uint8 *ppr_ser  = ppr_wl->pprdata;
	memset(ppr_wl, 0, sizeof(tx_pwr_rpt_t) + pprsize*3);
	ppr_wl->board_limit_len  = pprsize;
	ppr_wl->target_len       = pprsize;
	ppr_wl->version          = TX_POWER_T_VERSION;
	/* init allocated mem for serialisation */
	ppr_init_ser_mem_by_bw(ppr_ser, ppr_get_max_bw(), ppr_wl->board_limit_len);
	ppr_ser += ppr_wl->board_limit_len;
	ppr_init_ser_mem_by_bw(ppr_ser, ppr_get_max_bw(), ppr_wl->target_len);


	if (argv[1] && (!strcmp(argv[1], "--verbose") || !strcmp(argv[1], "-v"))) {
		verbose = TRUE;
		argv++;
	}
	if (argv[1] && (!strcmp(argv[1], "--brief") || !strcmp(argv[1], "-b"))) {
		brief = TRUE;
		argv++;
	}
	argv++;
	if (*argv)
		fprintf(stderr, "Ignoring arguments for %s\n", cmd->name);

	if ((err = wlu_get(wl, cmd->get, ppr_wl,  sizeof(tx_pwr_rpt_t) + pprsize*3)) < 0) {
		fprintf(stderr, "Error: Curpower failed. ");
		fprintf(stderr, "Bring up interface and disable mpc if necessary (wl mpc 0)\n");
		free(ppr_wl);
		return err;
	}
	/* parse */
	if (ppr_wl->version != TX_POWER_T_VERSION) {
		printf("error: version mismatch - driver %d, wl executable was expecting %d\n",
			ppr_wl->version, TX_POWER_T_VERSION);
		err = IOCTL_ERROR;
	} else {
		ppr_t* ppr_board = NULL;
		ppr_t* ppr_target = NULL;
		ppr_wl->flags = dtoh32(ppr_wl->flags);
		ppr_wl->chanspec = wl_chspec_from_driver(ppr_wl->chanspec);
		ppr_wl->local_chanspec = wl_chspec_from_driver(ppr_wl->local_chanspec);

		chanspec = ppr_wl->chanspec;
		mimo = (ppr_wl->flags & WL_TX_POWER_F_HT) |
		       (ppr_wl->flags & WL_TX_POWER_F_MIMO) |
		       (ppr_wl->flags & WL_TX_POWER_F_SISO);
		if (ppr_deserialize_create(NULL, ppr_wl->pprdata,
			ppr_wl->board_limit_len, &ppr_board) != BCME_OK) {
			err = IOCTL_ERROR;
			goto exit;
		}

		if (ppr_deserialize_create(NULL, ppr_ser, ppr_wl->target_len, &ppr_target)
			!= BCME_OK) {
			err = IOCTL_ERROR;
			goto exit;
		}

		/* dump */
		if (verbose)
			printf("%-23s%s\n", "Output Format Version:",
				CURPOWER_OUTPUT_FORMAT_VERSION);

		printf("%-23s%s, %s\n", "Power Control:",
		       (ppr_wl->flags & WL_TX_POWER_F_ENABLED) ? "On" : "Off",
		       (ppr_wl->flags & WL_TX_POWER_F_HW) ? "HW" : "SW");
		printf("%-23s%s\n", "Current Channel:",
		       wf_chspec_ntoa(ppr_wl->chanspec, chanspec_str));
		printf("%-23s%s\n", "BSS Channel:",
		       wf_chspec_ntoa(ppr_wl->local_chanspec, chanspec_str));
		printf("%-23s%d.%d dBm\n", "BSS Local Max:",
		       DIV_QUO(ppr_wl->local_max, 4), DIV_REM(ppr_wl->local_max, 4));
		printf("%-23s%d.%d dB\n", "BSS Local Constraint:",
		       DIV_QUO(ppr_wl->local_constraint, 4), DIV_REM(ppr_wl->local_constraint, 4));
		printf("%-23s%s\n", "Channel Width:",
			((ppr_wl->channel_bandwidth == WL_BW_20MHZ) ? "20MHz" :
			((ppr_wl->channel_bandwidth == WL_BW_40MHZ) ? "40MHz" : "80MHz")));
		printf("%-23s%d.%d dBm\n", "User Target:",
		       DIV_QUO(ppr_wl->user_target, 4), DIV_REM(ppr_wl->user_target, 4));
		printf("%-23s%d.%d dB\n", "SROM Antgain 2G:",
		       DIV_QUO(ppr_wl->antgain[0], 4), DIV_REM(ppr_wl->antgain[0], 4));
		printf("%-23s%d.%d dB\n", "SROM Antgain 5G:",
		       DIV_QUO(ppr_wl->antgain[1], 4), DIV_REM(ppr_wl->antgain[1], 4));
		printf("%-23s", "SAR:");
		if (ppr_wl->sar != WLC_TXPWR_MAX)
			printf("%d.%d dB\n", DIV_QUO(ppr_wl->sar, 4), DIV_REM(ppr_wl->sar, 4));
		else
			printf("-\n");
		printf("%-23s", "Current rate:");
		wl_rate_print(rate_str, ppr_wl->last_tx_ratespec);
		printf("[%s] %s\n", get_reg_rate_string_from_ratespec(ppr_wl->last_tx_ratespec),
			rate_str);
		printf("\n");

		printf("Regulatory Limits:\n");
		if (brief)
		{
			wl_txpwr_regulatory_array_row_print(ppr_wl->clm_limits,
				ppr_wl->clm_limits_subchan1, ppr_wl->clm_limits_subchan2,
				ppr_wl->channel_bandwidth,
				get_reg_rate_index_from_ratespec(ppr_wl->last_tx_ratespec));
		}
		else
		{
			wl_txpwr_regulatory_array_print(ppr_wl->clm_limits,
				ppr_wl->clm_limits_subchan1, ppr_wl->clm_limits_subchan2,
				ppr_wl->channel_bandwidth, verbose);
		}
		printf("\n");

		printf("%-23s%d\n", "Core Index:", ppr_wl->display_core);
		printf("Board Limits:\n");
		if (brief)
		{
			wl_txpwr_array_row_print(ppr_board, ppr_wl->channel_bandwidth,
				get_reg_rate_index_from_ratespec(ppr_wl->last_tx_ratespec));
		}
		else
		{
			wl_txpwr_array_print(ppr_board, ppr_wl->channel_bandwidth, verbose);
			printf("\n");
		}

		printf("Power Targets:\n");
		if (brief)
		{
			wl_txpwr_array_row_print(ppr_target, ppr_wl->channel_bandwidth,
				get_reg_rate_index_from_ratespec(ppr_wl->last_tx_ratespec));
		}
		else
		{
			wl_txpwr_array_print(ppr_target, ppr_wl->channel_bandwidth, verbose);
		}
		printf("\n");

		/* print the different power estimate combinations */
		if (mimo) {
			printf("Maximum Power Target among all rates:\t");
			for (i = 0; i < ppr_wl->rf_cores; i++)
				printf("%2d.%02d  ",
				       DIV_QUO(ppr_wl->tx_power_max[i], 4),
				       DIV_REM(ppr_wl->tx_power_max[i], 4));
			printf("\n");

			printf("Last est. power                     :\t");
			for (i = 0; i < ppr_wl->rf_cores; i++)
				printf("%2d.%02d  ",
				       DIV_QUO(ppr_wl->est_Pout[i], 4),
				       DIV_REM(ppr_wl->est_Pout[i], 4));
			printf("\n");

			printf("Power Target for the current rate   :\t");
			for (i = 0; i < ppr_wl->rf_cores; i++)
			{
				if (ppr_wl->target_offsets[i] != WL_RATE_DISABLED)
				{
					power_target = ppr_wl->tx_power_max[i] -
						ppr_wl->target_offsets[i];
					/*	for ACPHY, clip the power_target if it
						is larger than the SAR limit for the
						current path. For non-ACPHY or
						WLC_SARLIMIT disabled, this threshold is
						set to be MAX pwr, ie. 127
					 */
					if (power_target > ppr_wl->SARLIMIT[i])
						power_target = ppr_wl->SARLIMIT[i];
					printf("%2d.%02d  ",
						DIV_QUO(power_target, 4),
						DIV_REM(power_target, 4));
				}
				else
				{
					printf("-    ");
				}
			}
			printf("\n");

			printf("Last adjusted est. power            :\t");
			for (i = 0; i < ppr_wl->rf_cores; i++)
				printf("%2d.%02d  ",
				       DIV_QUO(ppr_wl->est_Pout_act[i], 4),
				       DIV_REM(ppr_wl->est_Pout_act[i], 4));
			printf("\n");
		} else {
			printf("Last est. power:\t%2d.%02d dBm\n",
			       DIV_QUO(ppr_wl->est_Pout[0], 4),
			       DIV_REM(ppr_wl->est_Pout[0], 4));
		}

		if (!mimo && CHSPEC_IS2G(chanspec)) {
			printf("Last CCK est. power:\t%2d.%02d dBm\n",
			       DIV_QUO(ppr_wl->est_Pout_cck, 4),
			       DIV_REM(ppr_wl->est_Pout_cck, 4));
		}
exit:
		if (ppr_board != NULL) {
			ppr_delete(NULL, ppr_board);
		}
		if (ppr_target != NULL) {
			ppr_delete(NULL, ppr_target);
		}
	}

	free(ppr_wl);
	return err;
}

/*	print a single row of the power data.
	convert data from dB to qdB;
	decide if the pwr data is 20 or 40MHz data;
	print "-" in the other channels
 */
static void
wl_txpwr_print_row(const char *label, uint8 chains, int8 pwr20,
	int8 pwr20in40, int8 pwr40, int8 pwr80, int8 pwr20in80, int8 pwr40in80,
	int8 unsupported_rate, int8 channel_bandwidth, bool verbose)
{
	/* homebrew conversion to qdB, pseudo-floating point representation. */
	int pwr20_q = DIV_QUO(pwr20, 4);
	int pwr20_r = DIV_REM(pwr20, 4);

	int pwr20in40_q = DIV_QUO(pwr20in40, 4);
	int pwr20in40_r = DIV_REM(pwr20in40, 4);

	int pwr40_q = DIV_QUO(pwr40, 4);
	int pwr40_r = DIV_REM(pwr40, 4);

	int pwr80_q = DIV_QUO(pwr80, 4);
	int pwr80_r = DIV_REM(pwr80, 4);

	int pwr20in80_q = DIV_QUO(pwr20in80, 4);
	int pwr20in80_r = DIV_REM(pwr20in80, 4);

	int pwr40in80_q = DIV_QUO(pwr40in80, 4);
	int pwr40in80_r = DIV_REM(pwr40in80, 4);


	char rate20[]     = "-    ";
	char rate20in40[] = "-    ";
	char rate40[]     = "-    ";
	char rate80[]     = "-    ";
	char rate20in80[] = "-    ";
	char rate40in80[] = "-    ";
	char pad[] = "  ";

	if (pwr20 != unsupported_rate)
		sprintf(rate20, "%2d.%-2d", pwr20_q, pwr20_r);
	if (pwr20in40 != unsupported_rate)
		sprintf(rate20in40, "%2d.%-2d", pwr20in40_q, pwr20in40_r);
	if (pwr40 != unsupported_rate)
		sprintf(rate40, "%2d.%-2d", pwr40_q, pwr40_r);
	if (pwr80 != unsupported_rate)
		sprintf(rate80, "%2d.%-2d", pwr80_q, pwr80_r);
	if (pwr20in80 != unsupported_rate)
		sprintf(rate20in80, "%2d.%-2d", pwr20in80_q, pwr20in80_r);
	if (pwr40in80 != unsupported_rate)
		sprintf(rate40in80, "%2d.%-2d", pwr40in80_q, pwr40in80_r);

	printf("%-23s%d     ", label, chains);
	if (!verbose) {
		switch (channel_bandwidth) {
		case WL_BW_20MHZ:
			printf("%s\n", rate20);
			break;
		case WL_BW_40MHZ:
			printf("%s%s%s\n", rate20in40, pad, rate40);
			break;
		case WL_BW_80MHZ:
			printf("%s%s%s%s%s\n", rate20in80, pad, rate40in80, pad, rate80);
			break;
		}
	} else {
		printf("%s%s%s%s%s%s%s%s%s%s%s\n", rate20, pad, rate20in40, pad,
			rate40, pad, rate20in80, pad, rate40in80, pad, rate80);
	}

}

static void
wl_txpwr_print_header(int8 channel_bandwidth, bool verbose)
{
	printf("Rate                  Chains ");
	if (!verbose)
	{
		switch (channel_bandwidth) {
		case WL_BW_20MHZ:
			printf("20MHz\n");
			break;
		case WL_BW_40MHZ:
			printf("20in40 40MHz\n");
			break;
		case WL_BW_80MHZ:
			printf("20in80 40in80 80MHz\n");
			break;
		}
	} else {
		printf("20MHz  20in40 40MHz  20in80 40in80 80MHz\n");
	}
}

/* get a power value from the opaque ppr structure */
static int8 wl_ppr_get_pwr(ppr_t* pprptr, reg_rate_index_t rate_idx, wl_tx_bw_t bw)
{
	clm_rate_group_id_t group_id = ppr_table[rate_idx].id;
	int8 power = WL_RATE_DISABLED;
	switch (ppr_group_table[group_id].rate_type) {
		case PPR_RATE_DSSS:
			{
				ppr_dsss_rateset_t rateset;
				ppr_get_dsss(pprptr, bw, ppr_group_table[group_id].chain, &rateset);
				power = rateset.pwr[rate_idx-ppr_group_table[group_id].first_rate];
			}
			break;
		case PPR_RATE_OFDM:
			{
				ppr_ofdm_rateset_t rateset;
				ppr_get_ofdm(pprptr, bw, ppr_group_table[group_id].mode,
					ppr_group_table[group_id].chain, &rateset);
				power = rateset.pwr[rate_idx-ppr_group_table[group_id].first_rate];
			}
			break;
		case PPR_RATE_HT:
			{
				ppr_ht_mcs_rateset_t rateset;
				ppr_get_ht_mcs(pprptr, bw, ppr_group_table[group_id].nss,
					ppr_group_table[group_id].mode,
					ppr_group_table[group_id].chain, &rateset);
				power = rateset.pwr[rate_idx-ppr_group_table[group_id].first_rate];
			}
			break;
		case PPR_RATE_VHT:
			{
				ppr_vht_mcs_rateset_t rateset;
				ppr_get_vht_mcs(pprptr, bw, ppr_group_table[group_id].nss,
					ppr_group_table[group_id].mode,
					ppr_group_table[group_id].chain, &rateset);
				power = rateset.pwr[rate_idx-ppr_group_table[group_id].first_rate];
			}
			break;
		default:
			break;
	}

	return power;
}

static void
wl_txpwr_array_row_print(ppr_t *pprptr, int8 channel_bandwidth,	reg_rate_index_t rate_index)
{
	const char *label;
	int8 pwr20     = WL_RATE_DISABLED;
	int8 pwr20in40 = WL_RATE_DISABLED;
	int8 pwr40     = WL_RATE_DISABLED;
	int8 pwr80     = WL_RATE_DISABLED;
	int8 pwr20in80 = WL_RATE_DISABLED;
	int8 pwr40in80 = WL_RATE_DISABLED;

	if (rate_index == NO_RATE)
	{
		printf("(NO_RATE)               -     -      -      -      -      -      -\n");
	}
	else
	{
		clm_rate_group_id_t group_id = ppr_table[rate_index].id;
		label = ppr_table[rate_index].label;

		switch (channel_bandwidth) {
		case WL_BW_20MHZ:
			pwr20     = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_20);
			break;

		case WL_BW_40MHZ:
			pwr20in40 = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_20IN40);
			pwr40     = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_40);
			break;

		case WL_BW_80MHZ:
			pwr80     = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_80);
			pwr20in80 = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_20IN80);
			pwr40in80 = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_40IN80);
			break;
		}

		wl_txpwr_print_row(label, ppr_group_table[group_id].chain,
			pwr20, pwr20in40, pwr40, pwr80, pwr20in80, pwr40in80,
			WL_RATE_DISABLED, channel_bandwidth, TRUE);
	}
}

/* print power value for each group of rates, if not in verbose mode and rates
 * are uniform, one power value is printed out for the whole group
 */
static void
wl_txpwr_ppr_print(ppr_t* pprptr, int vb, ppr_rate_type_t type,
	clm_rate_group_id_t gid, int8 bw, reg_rate_index_t *rate_index)
{
	uint i;
	const char *label;
	int  buniform  = FALSE;
	int8 pwr20     = WL_RATE_DISABLED;
	int8 pwr20in40 = WL_RATE_DISABLED;
	int8 pwr40     = WL_RATE_DISABLED;
	int8 pwr80     = WL_RATE_DISABLED;
	int8 pwr20in80 = WL_RATE_DISABLED;
	int8 pwr40in80 = WL_RATE_DISABLED;
	uint len       = 0;
	if (pprptr == NULL) {
		fprintf(stderr, "illegal ppr data!\n");
		return;
	}
	switch (type) {
	case PPR_RATE_DSSS:
		{
			len = sizeof(ppr_dsss_rateset_t);
			if (bw == WL_BW_20MHZ) {
				ppr_dsss_rateset_t rate;
				ppr_get_dsss(pprptr, WL_TX_BW_20, ppr_group_table[gid].chain,
					&rate);
				buniform = wl_array_check_val(rate.pwr,
					sizeof(rate), rate.pwr[0]) && !vb;
				if (buniform) {
					pwr20 = rate.pwr[0];
					label = get_clm_rate_group_label(gid);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);

				} else {
					for (i = 0; i < len; i++) {
						pwr20 = rate.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
			} else if (bw == WL_BW_40MHZ) {
				ppr_dsss_rateset_t rate20in40;
				ppr_dsss_rateset_t rate40;

				ppr_get_dsss(pprptr, WL_TX_BW_20IN40, ppr_group_table[gid].chain,
					&rate20in40);
				ppr_get_dsss(pprptr, WL_TX_BW_40, ppr_group_table[gid].chain,
					&rate40);
				buniform = wl_array_check_val(rate40.pwr,
					sizeof(ppr_dsss_rateset_t), rate40.pwr[0]) && !vb &&
					wl_array_check_val(rate20in40.pwr,
					sizeof(ppr_dsss_rateset_t), rate20in40.pwr[0]);
				if (buniform) {
					pwr20in40 = rate20in40.pwr[0];
					pwr40     = rate40.pwr[0];
					label = get_clm_rate_group_label(gid);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);

				} else {
					for (i = 0; i < len; i++) {
						pwr20in40 = rate20in40.pwr[i];
						pwr40     = rate40.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
			} else if (bw == WL_BW_80MHZ) {
				ppr_dsss_rateset_t rate20in80;
				ppr_dsss_rateset_t rate40in80;
				ppr_dsss_rateset_t rate80;
				ppr_get_dsss(pprptr, WL_TX_BW_20IN80, ppr_group_table[gid].chain,
					&rate20in80);
				ppr_get_dsss(pprptr, WL_TX_BW_40IN80, ppr_group_table[gid].chain,
					&rate40in80);
				ppr_get_dsss(pprptr, WL_TX_BW_80, ppr_group_table[gid].chain,
					&rate80);
				buniform = wl_array_check_val(rate80.pwr,
					sizeof(ppr_dsss_rateset_t), rate80.pwr[0]) && !vb &&
					wl_array_check_val(rate20in80.pwr,
					sizeof(ppr_dsss_rateset_t), rate20in80.pwr[0]) &&
					wl_array_check_val(rate40in80.pwr,
					sizeof(ppr_dsss_rateset_t), rate40in80.pwr[0]);
				if (buniform) {
					pwr20in80 = rate20in80.pwr[0];
					pwr40in80 = rate40in80.pwr[0];
					pwr80     = rate80.pwr[0];
					label     = get_clm_rate_group_label(gid);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);

				} else {
					for (i = 0; i < len; i++) {
						pwr20in80 = rate20in80.pwr[i];
						pwr40in80 = rate40in80.pwr[i];
						pwr80     = rate80.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
			}
			*rate_index += len;
		}
		break;
	case PPR_RATE_OFDM:
		{
			len = sizeof(ppr_ofdm_rateset_t);
			if (bw == WL_BW_20MHZ) {
				ppr_ofdm_rateset_t rate;
				ppr_get_ofdm(pprptr, WL_TX_BW_20, ppr_group_table[gid].mode,
					ppr_group_table[gid].chain, &rate);
				buniform = wl_array_check_val(rate.pwr,
					sizeof(rate), rate.pwr[0]) && !vb;
				if (buniform) {
					pwr20 = rate.pwr[0];
					label = get_clm_rate_group_label(gid);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);

				} else {
					for (i = 0; i < len; i++) {
						pwr20 = rate.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
			} else if (bw == WL_BW_40MHZ) {
				ppr_ofdm_rateset_t rate20in40;
				ppr_ofdm_rateset_t rate40;

				ppr_get_ofdm(pprptr, WL_TX_BW_20IN40, ppr_group_table[gid].mode,
					ppr_group_table[gid].chain, &rate20in40);
				ppr_get_ofdm(pprptr, WL_TX_BW_40, ppr_group_table[gid].mode,
					ppr_group_table[gid].chain, &rate40);
				buniform = wl_array_check_val(rate40.pwr,
					sizeof(ppr_ofdm_rateset_t), rate40.pwr[0]) && !vb &&
					wl_array_check_val(rate20in40.pwr,
					sizeof(ppr_ofdm_rateset_t), rate20in40.pwr[0]);
				if (buniform) {
					pwr20in40 = rate20in40.pwr[0];
					pwr40     = rate40.pwr[0];
					label = get_clm_rate_group_label(gid);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);

				} else {
					for (i = 0; i < len; i++) {
						pwr20in40 = rate20in40.pwr[i];
						pwr40     = rate40.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
			} else if (bw == WL_BW_80MHZ) {
				ppr_ofdm_rateset_t rate20in80;
				ppr_ofdm_rateset_t rate40in80;
				ppr_ofdm_rateset_t rate80;
				ppr_get_ofdm(pprptr, WL_TX_BW_20IN80, ppr_group_table[gid].mode,
					ppr_group_table[gid].chain, &rate20in80);
				ppr_get_ofdm(pprptr, WL_TX_BW_40IN80, ppr_group_table[gid].mode,
					ppr_group_table[gid].chain, &rate40in80);
				ppr_get_ofdm(pprptr, WL_TX_BW_80, ppr_group_table[gid].mode,
					ppr_group_table[gid].chain, &rate80);
				buniform = wl_array_check_val(rate80.pwr,
					sizeof(ppr_ofdm_rateset_t), rate80.pwr[0]) && !vb &&
					wl_array_check_val(rate20in80.pwr,
					sizeof(ppr_ofdm_rateset_t), rate20in80.pwr[0]) &&
					wl_array_check_val(rate40in80.pwr,
					sizeof(ppr_ofdm_rateset_t), rate40in80.pwr[0]);
				if (buniform) {
					pwr20in80 = rate20in80.pwr[0];
					pwr40in80 = rate40in80.pwr[0];
					pwr80     = rate80.pwr[0];
					label     = get_clm_rate_group_label(gid);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);

				} else {
					for (i = 0; i < len; i++) {
						pwr20in80 = rate20in80.pwr[i];
						pwr40in80 = rate40in80.pwr[i];
						pwr80     = rate80.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
			}
			*rate_index += len;
		}
		break;
	case PPR_RATE_HT:
		{
			len = sizeof(ppr_ht_mcs_rateset_t);
			if (bw == WL_BW_20MHZ) {
				ppr_ht_mcs_rateset_t rate;
				ppr_get_ht_mcs(pprptr, WL_TX_BW_20, ppr_group_table[gid].nss,
					ppr_group_table[gid].mode, ppr_group_table[gid].chain,
					&rate);
				buniform = wl_array_check_val(rate.pwr,
					sizeof(rate), rate.pwr[0]) && !vb;
				if (buniform) {
					pwr20 = rate.pwr[0];
					label = get_clm_rate_group_label(gid);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);

				} else {
					for (i = 0; i < len; i++) {
						pwr20 = rate.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
			} else if (bw == WL_BW_40MHZ) {
				ppr_ht_mcs_rateset_t rate20in40;
				ppr_ht_mcs_rateset_t rate40;

				ppr_get_ht_mcs(pprptr, WL_TX_BW_20IN40, ppr_group_table[gid].nss,
					ppr_group_table[gid].mode, ppr_group_table[gid].chain,
					&rate20in40);
				ppr_get_ht_mcs(pprptr, WL_TX_BW_40, ppr_group_table[gid].nss,
					ppr_group_table[gid].mode, ppr_group_table[gid].chain,
					&rate40);
				buniform = wl_array_check_val(rate40.pwr,
					sizeof(ppr_ht_mcs_rateset_t), rate40.pwr[0]) && !vb &&
					wl_array_check_val(rate20in40.pwr,
					sizeof(ppr_ht_mcs_rateset_t), rate20in40.pwr[0]);
				if (buniform) {
					pwr20in40 = rate20in40.pwr[0];
					pwr40     = rate40.pwr[0];
					label = get_clm_rate_group_label(gid);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);

				} else {
					for (i = 0; i < len; i++) {
						pwr20in40 = rate20in40.pwr[i];
						pwr40     = rate40.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
			} else if (bw == WL_BW_80MHZ) {
				ppr_ht_mcs_rateset_t rate20in80;
				ppr_ht_mcs_rateset_t rate40in80;
				ppr_ht_mcs_rateset_t rate80;
				ppr_get_ht_mcs(pprptr, WL_TX_BW_20IN80, ppr_group_table[gid].nss,
					ppr_group_table[gid].mode, ppr_group_table[gid].chain,
					&rate20in80);
				ppr_get_ht_mcs(pprptr, WL_TX_BW_40IN80, ppr_group_table[gid].nss,
					ppr_group_table[gid].mode, ppr_group_table[gid].chain,
					&rate40in80);
				ppr_get_ht_mcs(pprptr, WL_TX_BW_80, ppr_group_table[gid].nss,
					ppr_group_table[gid].mode, ppr_group_table[gid].chain,
					&rate80);
				buniform = wl_array_check_val(rate80.pwr,
					sizeof(ppr_ht_mcs_rateset_t), rate80.pwr[0]) && !vb &&
					wl_array_check_val(rate20in80.pwr,
					sizeof(ppr_ht_mcs_rateset_t), rate20in80.pwr[0]) &&
					wl_array_check_val(rate40in80.pwr,
					sizeof(ppr_ht_mcs_rateset_t), rate40in80.pwr[0]);
				if (buniform) {
					pwr20in80 = rate20in80.pwr[0];
					pwr40in80 = rate40in80.pwr[0];
					pwr80     = rate80.pwr[0];
					label     = get_clm_rate_group_label(gid);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);

				} else {
					for (i = 0; i < len; i++) {
						pwr20in80 = rate20in80.pwr[i];
						pwr40in80 = rate40in80.pwr[i];
						pwr80     = rate80.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
			}
			*rate_index += len;
		}
		break;
	case PPR_RATE_VHT:
		{
			uint bvhtuniform = FALSE;
			uint lenht = sizeof(ppr_ht_mcs_rateset_t);
			len = sizeof(ppr_vht_mcs_rateset_t);
			/* VHT RATES are printed as two parts: MCS0-7 and VTH8-9 */
			if (bw == WL_BW_20MHZ) {
				ppr_vht_mcs_rateset_t rate;
				ppr_get_vht_mcs(pprptr, WL_TX_BW_20, ppr_group_table[gid].nss,
					ppr_group_table[gid].mode, ppr_group_table[gid].chain,
					&rate);
				buniform = wl_array_check_val(rate.pwr, lenht, rate.pwr[0])&& !vb;
				bvhtuniform = wl_array_check_val(&rate.pwr[lenht], len-lenht,
					rate.pwr[lenht]) && !vb;
				if (buniform) {
					pwr20 = rate.pwr[0];
					label = get_clm_rate_group_label(gid);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);

				} else {
					for (i = 0; i < lenht; i++) {
						pwr20 = rate.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
				if (bvhtuniform) {
					pwr20 = rate.pwr[lenht];
					/* print VHT8-9 label */
					label = get_clm_rate_group_label(gid+1);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);
				} else {
					for (i = lenht; i < len; i++) {
						pwr20 = rate.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
			} else if (bw == WL_BW_40MHZ) {
				ppr_vht_mcs_rateset_t rate20in40;
				ppr_vht_mcs_rateset_t rate40;

				ppr_get_vht_mcs(pprptr, WL_TX_BW_20IN40, ppr_group_table[gid].nss,
					ppr_group_table[gid].mode, ppr_group_table[gid].chain,
					&rate20in40);
				ppr_get_vht_mcs(pprptr, WL_TX_BW_40, ppr_group_table[gid].nss,
					ppr_group_table[gid].mode, ppr_group_table[gid].chain,
					&rate40);

				buniform = wl_array_check_val(rate40.pwr,
					sizeof(ppr_ht_mcs_rateset_t), rate40.pwr[0]) && !vb &&
					wl_array_check_val(rate20in40.pwr,
					sizeof(ppr_ht_mcs_rateset_t), rate20in40.pwr[0]);
				bvhtuniform = wl_array_check_val(&rate40.pwr[lenht], len-lenht,
					rate40.pwr[lenht]) && !vb &&
					wl_array_check_val(&rate20in40.pwr[lenht], len-lenht,
					rate20in40.pwr[lenht]);
				if (buniform) {
					pwr20in40 = rate20in40.pwr[0];
					pwr40     = rate40.pwr[0];
					label = get_clm_rate_group_label(gid);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);

				} else {
					for (i = 0; i < lenht; i++) {
						pwr20in40 = rate20in40.pwr[i];
						pwr40     = rate40.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
				if (bvhtuniform) {
					pwr20in40 = rate20in40.pwr[lenht];
					pwr40     = rate40.pwr[lenht];
					/* print VHT8-9 label */
					label = get_clm_rate_group_label(gid+1);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);
				} else {
					for (i = lenht; i < len; i++) {
						pwr20in40 = rate20in40.pwr[i];
						pwr40     = rate40.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
			} else if (bw == WL_BW_80MHZ) {
				ppr_vht_mcs_rateset_t rate20in80;
				ppr_vht_mcs_rateset_t rate40in80;
				ppr_vht_mcs_rateset_t rate80;
				ppr_get_vht_mcs(pprptr, WL_TX_BW_20IN80, ppr_group_table[gid].nss,
					ppr_group_table[gid].mode, ppr_group_table[gid].chain,
					&rate20in80);
				ppr_get_vht_mcs(pprptr, WL_TX_BW_40IN80, ppr_group_table[gid].nss,
					ppr_group_table[gid].mode, ppr_group_table[gid].chain,
					&rate40in80);
				ppr_get_vht_mcs(pprptr, WL_TX_BW_80, ppr_group_table[gid].nss,
					ppr_group_table[gid].mode, ppr_group_table[gid].chain,
					&rate80);
				buniform = wl_array_check_val(rate80.pwr,
					sizeof(ppr_ht_mcs_rateset_t), rate80.pwr[0]) && !vb &&
					wl_array_check_val(rate20in80.pwr,
					sizeof(ppr_ht_mcs_rateset_t), rate20in80.pwr[0]) &&
					wl_array_check_val(rate40in80.pwr,
					sizeof(ppr_ht_mcs_rateset_t), rate40in80.pwr[0]);
				bvhtuniform = wl_array_check_val(&rate80.pwr[lenht], len-lenht,
					rate80.pwr[lenht]) && !vb &&
					wl_array_check_val(&rate20in80.pwr[lenht], len-lenht,
					rate20in80.pwr[lenht]) &&
					wl_array_check_val(&rate40in80.pwr[lenht], len-lenht,
					rate40in80.pwr[lenht]);

				if (buniform) {
					pwr20in80 = rate20in80.pwr[0];
					pwr40in80 = rate40in80.pwr[0];
					pwr80     = rate80.pwr[0];
					label     = get_clm_rate_group_label(gid);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);

				} else {
					for (i = 0; i < lenht; i++) {
						pwr20in80 = rate20in80.pwr[i];
						pwr40in80 = rate40in80.pwr[i];
						pwr80     = rate80.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
				if (bvhtuniform) {
					pwr20in80 = rate20in80.pwr[lenht];
					pwr40in80 = rate40in80.pwr[lenht];
					pwr80     = rate80.pwr[lenht];
					/* print VHT8-9 label */
					label = get_clm_rate_group_label(gid+1);
					wl_txpwr_print_row(label, ppr_group_table[gid].chain,
						pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
						pwr40in80, WL_RATE_DISABLED, bw, vb);
				} else {
					for (i = lenht; i < len; i++) {
						pwr20in80 = rate20in80.pwr[i];
						pwr40in80 = rate40in80.pwr[i];
						pwr80     = rate80.pwr[i];
						label = ppr_table[*rate_index+i].label;
						wl_txpwr_print_row(label,
							ppr_group_table[gid].chain,
							pwr20, pwr20in40, pwr40, pwr80, pwr20in80,
							pwr40in80, WL_RATE_DISABLED,
							bw, vb);
					}
				}
			}
			*rate_index += len;
		}
		break;
	}
}

static void
wl_txpwr_array_print(ppr_t* pprptr, int8 bw, bool verbose)
{
	clm_rate_group_id_t i;
	reg_rate_index_t rate_index = DSSS1;
	wl_txpwr_print_header(bw, verbose);
	for (i = RATE_GROUP_ID_DSSS; i < RATE_GROUP_ID_COUNT; i++) {
		wl_txpwr_ppr_print(pprptr, verbose, ppr_group_table[i].rate_type, i, bw,
			&rate_index);
		/* VHT rates are printed in two parts: MCS + VHT8,9 */
		if (ppr_group_table[i].rate_type == PPR_RATE_VHT)
			i++;
	}
}

static void
wl_txpwr_regulatory_array_row_print(int8 *powers, int8 *powers_subchan1,
	int8 *powers_subchan2, int8 channel_bandwidth,  reg_rate_index_t rate_index)
{
	const int RATE_UNSUPPORTED = -128; /* CLM uses -128 as the unsupported rate */
	const char* label;
	uint8 chains;
	int pwr20 = RATE_UNSUPPORTED;
	int pwr20in40 = RATE_UNSUPPORTED;
	int pwr40 = RATE_UNSUPPORTED;
	int pwr80 = RATE_UNSUPPORTED;
	int pwr20in80 = RATE_UNSUPPORTED;
	int pwr40in80 = RATE_UNSUPPORTED;

	wl_txpwr_print_header(channel_bandwidth, TRUE);

	if (rate_index == NO_RATE)
	{
		printf("(NO_RATE)               -     -      -      -      -      -      -\n");
	}
	else
	{
		label = ppr_table[rate_index].label;
		chains = ppr_group_table[ppr_table[rate_index].id].chain;

		switch (channel_bandwidth) {
		case WL_BW_20MHZ:
			pwr20 = powers[ppr_table[rate_index].rate];
			break;
		case WL_BW_40MHZ:
			pwr40 = powers[ppr_table[rate_index].rate];
			pwr20in40 = powers_subchan1[ppr_table[rate_index].rate];
			break;
		case WL_BW_80MHZ:
			pwr80 = powers[ppr_table[rate_index].rate];
			pwr40in80 = powers_subchan1[ppr_table[rate_index].rate];
			pwr20in80 = powers_subchan2[ppr_table[rate_index].rate];
			break;
		}
		wl_txpwr_print_row(label, chains, pwr20, pwr20in40,
			pwr40, pwr80, pwr20in80, pwr40in80, RATE_UNSUPPORTED,
			channel_bandwidth, TRUE);
	}
}

/* Essentially the same as wl_txpwr_array_print, but the
 * format of the regulatory data is now diffreent enough that we need a separate function.
 * The data format should be unified, and this code removed.
 */
static void
wl_txpwr_regulatory_array_print(int8 *powers, int8 *powers_subchan1,
	int8 *powers_subchan2, int8 channel_bandwidth, bool verbose)
{
	int i, j;
	const int RATE_UNSUPPORTED = -128; /* CLM uses -128 as the unsupported rate */
	const char* label;

	/* This code assumes that all of the rates in a group will be contiguous */
	wl_txpwr_print_header(channel_bandwidth, verbose);
	for (i = 0; i < RATE_GROUP_ID_COUNT; i++) {
		int first_rate = 0;
		int num_rates = 0;
		uint8 chains;
		bool compress_rate = FALSE;
		int pwr20 = RATE_UNSUPPORTED;
		int pwr20in40 = RATE_UNSUPPORTED;
		int pwr40 = RATE_UNSUPPORTED;
		int pwr80 = RATE_UNSUPPORTED;
		int pwr20in80 = RATE_UNSUPPORTED;
		int pwr40in80 = RATE_UNSUPPORTED;

		/* find the first rate index in this group. */
		while ((int)ppr_table[first_rate].id != i)
			first_rate++;
		/* how many rates in that group? */
		while ((first_rate + num_rates < WL_NUMRATES) &&
			((int)ppr_table[first_rate + num_rates].id == i))
			num_rates++;

		/* are the regulatory power settings the same? */
		switch (channel_bandwidth) {
		case WL_BW_20MHZ:
			compress_rate = !verbose &&
				wl_array_uniform((uint8*)&powers[ppr_table[first_rate].rate], 0,
				num_rates);
			break;
		case WL_BW_40MHZ:
			compress_rate = !verbose &&
				wl_array_uniform((uint8*)&powers[ppr_table[first_rate].rate], 0,
				num_rates) &&
				wl_array_uniform((uint8*)&powers_subchan1[ppr_table
				[first_rate].rate], 0, num_rates);
			break;
		case WL_BW_80MHZ:
			compress_rate = !verbose &&
				wl_array_uniform((uint8*)&powers[ppr_table[first_rate].rate], 0,
				num_rates) &&
				wl_array_uniform((uint8*)&powers_subchan1[ppr_table
				[first_rate].rate], 0, num_rates) &&
				wl_array_uniform((uint8*)&powers_subchan2[ppr_table
				[first_rate].rate], 0, num_rates);
			break;
		}

		if (compress_rate)
			j = num_rates - 1;
		else
			j = 0;

		for (; j < num_rates; j++) {
			if (compress_rate)
				label = get_clm_rate_group_label(i);
			else
				label = ppr_table[first_rate + j].label;

			chains = ppr_group_table[ppr_table[first_rate + j].id].chain;

			switch (channel_bandwidth) {
			case WL_BW_20MHZ:
				pwr20 = powers[ppr_table[first_rate + j].rate];
				break;
			case WL_BW_40MHZ:
				pwr40 = powers[ppr_table[first_rate + j].rate];
				pwr20in40 = powers_subchan1[ppr_table[first_rate + j].rate];
				break;
			case WL_BW_80MHZ:
				pwr80 = powers[ppr_table[first_rate + j].rate];
				pwr40in80 = powers_subchan1[ppr_table[first_rate + j].rate];
				pwr20in80 = powers_subchan2[ppr_table[first_rate + j].rate];
				break;
			}
			wl_txpwr_print_row(label, chains, pwr20, pwr20in40,
				pwr40, pwr80, pwr20in80, pwr40in80, RATE_UNSUPPORTED,
				channel_bandwidth, verbose);
		}
	}
}

/* return TRUE if all the values in the array are uniformly the same */
static int
wl_array_uniform(uint8 *pwr, int start, int count)
{
	int i, rate;

	for (i = 1, rate = start + 1; i < count; i++, rate++)
		if (pwr[rate] != pwr[rate - 1])
			return FALSE;

	return TRUE;
}

/* helper function to check if the array are uniformly same as the input value */
static int wl_array_check_val(int8 *pwr, uint count, int8 val)
{
	uint i;
	for (i = 0; i < count; i++) {
		if (pwr[i] != val)
			return FALSE;
	}
	return TRUE;
}

static int
wl_get_instant_power(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	tx_inst_power_t *power;
	uint band_list[3];

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	strcpy(buf, "txinstpwr");
	if ((ret = wlu_get(wl, WLC_GET_VAR, &buf[0], WLC_IOCTL_MAXLEN)) < 0) {
		return ret;
	}

	power = (tx_inst_power_t *)buf;
	/* Make the most of the info returned in band_list!
	 * b/g and a
	 * b/g-uni
	 * a-uni
	 * NOTE: NO a and b/g case ...
	 */
	if ((ret = wlu_get(wl, WLC_GET_BANDLIST, band_list, sizeof(band_list))) < 0)
		return (ret);
	band_list[0] = dtoh32(band_list[0]);
	band_list[1] = dtoh32(band_list[1]);
	band_list[2] = dtoh32(band_list[2]);

	/* If B/G is present it's always the lower index */
	if (band_list[1] == WLC_BAND_2G) {
			printf("Last B phy CCK est. power:\t%2d.%d dBm\n",
			       DIV_QUO(power->txpwr_est_Pout[0], 4),
			       DIV_REM(power->txpwr_est_Pout[0], 4));
			printf("Last B phy OFDM est. power:\t%2d.%d dBm\n",
			       DIV_QUO(power->txpwr_est_Pout_gofdm, 4),
			       DIV_REM(power->txpwr_est_Pout_gofdm, 4));

			printf("\n");
	}

	/* A band */
	if (band_list[1] == WLC_BAND_5G || (band_list[0] > 1 && band_list[2] == WLC_BAND_5G)) {
		printf("Last A phy est. power:\t\t%2d.%d dBm\n",
		       DIV_QUO(power->txpwr_est_Pout[1], 4),
		       DIV_REM(power->txpwr_est_Pout[1], 4));
	}

	return ret;
}

static int
wl_evm(void *wl, cmd_t *cmd, char **argv)
{
	int val[3];

	/* Get channel */
	if (!*++argv) {
		fprintf(stderr, "Need to specify at least one parameter\n");
		return -1;
	}

	if (!stricmp(*argv, "off"))
		val[0] = 0;
	else
		val[0] = atoi(*argv);

	/* set optional parameters to default */
	val[1] = 4;	/* rate in 500Kb units */
	val[2] = 0;	/* This is ignored */

	/* Get optional rate and convert to 500Kb units */
	if (*++argv)
		val[1] = rate_string2int(*argv);

	val[0] = htod32(val[0]);
	val[1] = htod32(val[1]);
	val[2] = htod32(val[2]);
	return wlu_set(wl, cmd->set, val, sizeof(val));
}

/* wl join <ssid> [key <0-3>:xxxxx]
 *                [imode bss|ibss]
 *                [amode open|shared|openshared|wpa|wpapsk|wpa2|wpa2psk|wpanone|ftpsk]
 *                [options]
 * Options:
 * -b MAC, --bssid=MAC, where MAC is in xx:xx:xx:xx:xx:xx format
 * -c CL, --chanspecs=CL, where CL is a comma or space separated list of chanspecs
 * -p, -passive: uses  join iovar instead of SET_SSID ioctl to force passive assoc scan
 */
static int
wl_join(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_OK, idx = 0;
	wl_join_params_t *join_params;
	int join_params_size;
	wl_wsec_key_t key;
	int wsec = 0, auth = 0, infra = 1;
	int wpa_auth = WPA_AUTH_DISABLED;
	char* cmd_name;
	int passive = 0;

	UNUSED_PARAMETER(cmd);

	cmd_name = *argv++;

	/* allocate the max storage */
	join_params_size = WL_JOIN_PARAMS_FIXED_SIZE + WL_NUMCHANNELS * sizeof(chanspec_t);
	if ((join_params = malloc(join_params_size)) == NULL) {
		fprintf(stderr, "Error allocating %d bytes for assoc params\n", join_params_size);
		return BCME_NOMEM;
	}
	memset(join_params, 0, join_params_size);
	memcpy(&join_params->params.bssid, &ether_bcast, ETHER_ADDR_LEN);

	/* verify that SSID was specified and is a valid length */
	if (!*argv || (strlen(*argv) > DOT11_MAX_SSID_LEN)) {
		ret = BCME_BADARG;
		goto exit;
	}

	join_params->ssid.SSID_len = strlen(*argv);
	memcpy(join_params->ssid.SSID, *argv, join_params->ssid.SSID_len);
	/* default to plain old ioctl */
	join_params_size = sizeof(wlc_ssid_t);

	/* get current wsec */
	if (wlu_iovar_getint(wl, "wsec", &wsec) < 0)
		wsec = 0;

	while (*++argv) {
		if (!stricmp(*argv, "wepkey") || !stricmp(*argv, "wep") || !stricmp(*argv, "key")) {
			/* specified wep key */
			memset(&key, 0, sizeof(key));
			if (!*++argv) {
				ret = BCME_BADARG;
				goto exit;
			}
			/* WEP index specified */
			if (*(argv[0]+1) == ':') {
				idx = *argv[0] - 0x30;
				if (idx < 0 || idx > 3) {
					fprintf(stderr, "Invalid key index %d specified\n", idx);
					ret = BCME_BADARG;
					goto exit;
				}
				argv[0] += 2; /* colon + digit */
			}
			key.index = idx;

			if (parse_wep(argv, &key, FALSE)) {
				ret = BCME_BADARG;
				goto exit;
			}

			key.index = htod32(key.index);
			key.len = htod32(key.len);
			key.algo = htod32(key.algo);
			key.flags = htod32(key.flags);

			if ((ret = wlu_set(wl, WLC_SET_KEY, &key, sizeof(wl_wsec_key_t))) < 0) {
				ret = BCME_BADARG;
				goto exit;
			}
			wsec |= WEP_ENABLED;
		}
		/* specified infrastructure mode */
		else if (!stricmp(*argv, "imode") ||
		         !stricmp(*argv, "infra") ||
		         !stricmp(*argv, "mode")) {
			if (!*++argv) {
				fprintf(stderr, "%s %s: expected argument after \"infra\" keyword "
				        "but command line ended.\n", wlu_av0, cmd_name);
				ret = BCME_BADARG;
				goto exit;
			} else if (!stricmp(*argv, "ibss") ||
			           !stricmp(*argv, "adhoc") ||
			           !stricmp(*argv, "ad-hoc")) {
				infra = 0;
			} else if (!stricmp(*argv, "bss") ||
			           !stricmp(*argv, "managed") ||
			           !strnicmp(*argv, "infra", 5)) {
				infra = 1;
			} else {
				fprintf(stderr, "%s %s: unrecongnized parameter \"%s\" after "
				        "\"infra\" keyword\n", wlu_av0, cmd_name, *argv);
				ret = BCME_BADARG;
				goto exit;
			}
		}
		/* specified authentication mode */
		else if (!stricmp(*argv, "amode") || !strnicmp(*argv, "auth", 4)) {
			if (!*++argv) {
				ret = BCME_BADARG;
				goto exit;
			}
			if (!stricmp(*argv, "open"))
				auth = WL_AUTH_OPEN_SYSTEM;
			else if (!stricmp(*argv, "shared"))
				auth = WL_AUTH_SHARED_KEY;
			else if (!stricmp(*argv, "openshared"))
				auth = WL_AUTH_OPEN_SHARED;
			else if (!stricmp(*argv, "wpanone"))
				wpa_auth = WPA_AUTH_NONE;
			else if (!stricmp(*argv, "wpa"))
				wpa_auth = WPA_AUTH_UNSPECIFIED;
			else if (!stricmp(*argv, "wpapsk"))
				 wpa_auth = WPA_AUTH_PSK;
			else if (!stricmp(*argv, "wpa2"))
				wpa_auth = WPA2_AUTH_UNSPECIFIED;
			else if (!stricmp(*argv, "wpa2psk"))
				wpa_auth = WPA2_AUTH_PSK;
#ifdef BCMWAPI_WAI
			else if (!stricmp(*argv, "wapi"))
				wpa_auth = WAPI_AUTH_UNSPECIFIED;
			else if (!stricmp(*argv, "wapipsk"))
				wpa_auth = WAPI_AUTH_PSK;
#endif /* BCMWAPI_WAI */
			else if (!stricmp(*argv, "ftpsk"))
				wpa_auth = WPA2_AUTH_PSK | WPA2_AUTH_FT;
			else {
				ret = BCME_BADARG;
				goto exit;
			}
		}
		else if (!stricmp(*argv, "-passive") || !stricmp(*argv, "-p")) {
			/* Use extended join iovar to assoc_scan passively */
			passive = 1;
		}
		/* optional assoc params */
		else if ((ret = wl_parse_assoc_params(argv, &join_params->params)) == BCME_OK) {
			join_params_size = WL_JOIN_PARAMS_FIXED_SIZE +
			        dtoh32(join_params->params.chanspec_num) * sizeof(chanspec_t);
			break;
		}
		else {
			fprintf(stderr, "%s %s: unable to parse parameter \"%s\"\n",
			        wlu_av0, cmd_name, *argv);
			goto exit;
		}
	}

	/* set infrastructure mode */
	infra = htod32(infra);
	if ((ret = wlu_set(wl, WLC_SET_INFRA, &infra, sizeof(int))) < 0)
		goto exit;

	/* set authentication mode */
	auth = htod32(auth);
	if ((ret = wlu_set(wl, WLC_SET_AUTH, &auth, sizeof(int))) < 0)
		goto exit;

	/* set wsec mode */
	if ((ret = wlu_iovar_setint(wl, "wsec", wsec)) < 0)
		goto exit;

	/* set WPA_auth mode */
	wpa_auth = htod32(wpa_auth);
	if ((ret = wlu_set(wl, WLC_SET_WPA_AUTH, &wpa_auth, sizeof(wpa_auth))) < 0)
		goto exit;

	if (passive) {
		wl_extjoin_params_t *extjoin_params;
		int extjoin_size;
		int i;
		printf("Using passive assoc scan\n");
		extjoin_size = WL_EXTJOIN_PARAMS_FIXED_SIZE +
			join_params->params.chanspec_num * sizeof(chanspec_t);
		if ((extjoin_params = malloc(extjoin_size)) == NULL) {
			fprintf(stderr, "Error allocating %d bytes for extjoin \n", extjoin_size);
			ret = BCME_NOMEM;
			goto exit;
		}

		/* Copy assoc params from legacy struct into extended struct */
		memset(extjoin_params, 0, extjoin_size);
		memcpy(&extjoin_params->ssid.SSID, &join_params->ssid.SSID, DOT11_MAX_SSID_LEN);
		extjoin_params->ssid.SSID_len = htod32(join_params->ssid.SSID_len);

		memcpy(&extjoin_params->assoc.bssid, &join_params->params.bssid, ETHER_ADDR_LEN);
		extjoin_params->assoc.chanspec_num = join_params->params.chanspec_num;
		for (i = 0; i < join_params->params.chanspec_num; i++) {
			extjoin_params->assoc.chanspec_list[i] =
				join_params->params.chanspec_list[i];
		}

		extjoin_params->scan.scan_type = WL_SCANFLAGS_PASSIVE;
		extjoin_params->scan.nprobes = -1;
		extjoin_params->scan.active_time = -1;
		extjoin_params->scan.passive_time = -1;
		extjoin_params->scan.home_time = -1;


		ret = wlu_var_setbuf(wl, "join", extjoin_params, extjoin_size);
		free(extjoin_params);
	} else {
		/* set ssid with extend assoc params (if any) */
		join_params->ssid.SSID_len = htod32(join_params->ssid.SSID_len);
		ret = wlu_set(wl, WLC_SET_SSID, join_params, join_params_size);
	}

exit:
	free(join_params);
		return ret;
}

/* Set or Get the "bssid" iovar, with an optional config index argument:
 *	wl bssid [-C N]|[--cfg=N] bssid
 *
 * Option:
 *	-C N
 *	--cfg=N
 *	--config=N
 *	--configuration=N
 *		specify the config index N
 * If cfg index not given on a set, the WLC_SET_BSSID ioctl will be used
 */
static int
wl_bssid(void *wl, cmd_t *cmd, char **argv)
{
	struct ether_addr ea;
	int bsscfg_idx = 0;
	int consumed;
	int error;

	UNUSED_PARAMETER(cmd);

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((error = wl_cfg_option(argv, "bssid", &bsscfg_idx, &consumed)) != 0)
		return error;

	argv += consumed;

	if (*argv == NULL) {
		if (consumed == 0) {
			/* no config index, use WLC_GET_BSSID on the interface */
			error = wlu_get(wl, WLC_GET_BSSID, &ea, ETHER_ADDR_LEN);
		} else {
			/* use "bssid" iovar since a config option was given */
			error = wlu_bssiovar_get(wl, "bssid", bsscfg_idx, &ea, ETHER_ADDR_LEN);
		}
		if (error < 0)
			return error;
		printf("%s\n", wl_ether_etoa(&ea));

	} else {

		if (!wl_ether_atoe(*argv, &ea))
			return -1;

		if (consumed == 0) {
			/* no config index given, use WLC_SET_BSSID */
			error = wlu_set(wl, WLC_SET_BSSID, &ea, ETHER_ADDR_LEN);
		} else {
			/* use "bssid" iovar since a config option was given */
			error = wl_bssiovar_set(wl, "bssid", bsscfg_idx, &ea, ETHER_ADDR_LEN);
		}
	}
	return error;
}

/* Set or Get the "ssid" iovar, with an optional config index argument:
 *	wl ssid [-C N]|[--cfg=N] ssid
 *
 * Option:
 *	-C N
 *	--cfg=N
 *	--config=N
 *	--configuration=N
 *		specify the config index N
 * If cfg index not given on a set, the WLC_SET_SSID ioctl will be used
 */
static int
wl_ssid(void *wl, cmd_t *cmd, char **argv)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	wlc_ssid_t ssid = { 0, {0} };
	int bsscfg_idx = 0;
	int consumed;
	int error;

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((error = wl_cfg_option(argv, "ssid", &bsscfg_idx, &consumed)) != 0)
		return error;

	argv += consumed;

	if (*argv == NULL) {
		if (consumed == 0) {
			/* no config index, use WLC_GET_SSID on the interface */
			if (cmd->get == WLC_GET_SSID)
				error = wlu_get(wl, WLC_GET_SSID, &ssid, sizeof(ssid));
			else
				error = wlu_iovar_get(wl, cmd->name, &ssid, sizeof(ssid));
		} else {
			if (cmd->get == WLC_GET_SSID) {
				/* use "ssid" iovar since a config option was given */
				error = wlu_bssiovar_get(wl, "ssid", bsscfg_idx, &ssid,
				                        sizeof(ssid));
			} else {
				error = wlu_bssiovar_get(wl, cmd->name, bsscfg_idx, &ssid,
				                        sizeof(ssid));
			}
		}
		if (error < 0)
			return error;

		ssid.SSID_len = dtoh32(ssid.SSID_len);
		wl_format_ssid(ssidbuf, ssid.SSID, ssid.SSID_len);
		printf("Current %s: \"%s\"\n",
		       (cmd->get == WLC_GET_SSID)? "SSID": cmd->name,
		       ssidbuf);
	} else {
		if (strlen(argv[0]) > DOT11_MAX_SSID_LEN) {
			fprintf(stderr, "SSID arg \"%s\" must be 32 chars or less\n", argv[0]);
			return -1;
		}
		ssid.SSID_len = strlen(argv[0]);
		memcpy(ssid.SSID, argv[0], ssid.SSID_len);

		wl_format_ssid(ssidbuf, ssid.SSID, ssid.SSID_len);
		printf("Setting %s: \"%s\"\n", (cmd->set == WLC_SET_SSID)? "SSID": cmd->name,
		       ssidbuf);

		ssid.SSID_len = htod32(ssid.SSID_len);
		if (consumed == 0) {
			/* no config index given, use WLC_SET_SSID */
			if (cmd->set == WLC_SET_SSID) {
				error = wlu_set(wl, WLC_SET_SSID, &ssid, sizeof(wlc_ssid_t));
			} else {
				error = wlu_iovar_set(wl, cmd->name, &ssid, sizeof(wlc_ssid_t));
			}
		} else {
			if (cmd->set == WLC_SET_SSID) {
				/* use "ssid" iovar since a config option was given */
				error = wl_bssiovar_set(wl, "ssid", bsscfg_idx, &ssid,
				                        sizeof(wlc_ssid_t));
			} else
				error = wl_bssiovar_set(wl, cmd->name, bsscfg_idx, &ssid,
				                        sizeof(wlc_ssid_t));
		}
	}
	return error;
}

static const char*
wl_smfs_map_type(uint8 type)
{
	static const struct {uint8 type; char name[32];} type_names[] = {
		{SMFS_TYPE_AUTH, "Authentication_Request"},
		{SMFS_TYPE_ASSOC, "Association_Request"},
		{SMFS_TYPE_REASSOC, "Reassociation_Request"},
		{SMFS_TYPE_DISASSOC_TX, "Disassociation_Request_TX"},
		{SMFS_TYPE_DISASSOC_RX, "Disassociation_Request_RX"},
		{SMFS_TYPE_DEAUTH_TX, "Deauthentication_Request_TX"},
		{SMFS_TYPE_DEAUTH_RX, "Deauthentication_Request_RX"}
	};

	const char *tname = "UNKNOWN";
	uint i;

	for (i = 0; i < ARRAYSIZE(type_names); i++) {
		if (type_names[i].type == type)
		    tname = type_names[i].name;
	}
	return tname;
}

static int
wl_disp_smfs(char *inbuf)
{
	static const char *codename[] = {"Status_code", "Reason_code"};
	wl_smf_stats_t *smf_stats;
	wl_smfs_elem_t *elemt = NULL;
	const char *namebuf;
	uint32 version;
	int count;

	smf_stats = (wl_smf_stats_t *) inbuf;
	namebuf = wl_smfs_map_type(smf_stats->type);

	version = dtoh32(smf_stats->version);
	if (version != SMFS_VERSION) {
		fprintf(stderr, "Sorry, your driver has smfs_version %d "
			"but this program supports only version %d.\n",
			version, SMFS_VERSION);
		return -1;
	}

	printf("Frame type: %s\n", namebuf);
	printf("\tIgnored Count: %d\n", dtoh32(smf_stats->ignored_cnt));
	printf("\tMalformed Count: %d\n", dtoh32(smf_stats->malformed_cnt));

	count = dtoh32(smf_stats->count_total);

	if (count) {
		namebuf = codename[dtoh32(smf_stats->codetype)];
		printf("\tSuccessful/Failed Count:\n");
		elemt = &smf_stats->elem[0];
	}

	while (count) {
		printf("\t\t%s %d Count: %d\n",  namebuf, dtoh16(elemt->code),
		  dtoh32(elemt->count));
		elemt ++;
		count --;
	}

	return 0;

}

/*
 * Check for the smfstats parameters. One of defined parameters can be passed in.
 */
static int
wl_smfs_option(char **argv, int* idx, int *consumed, int* clear)
{
	int err = 0;
	char *p;
	char const * smfs_opt[] = {"auth", "assoc", "reassoc", "disassoc_tx",
							   "disassoc_rx", "deauth_tx", "deauth_rx"};
	char const * clear_opt = "clear";
	int i;
	char const * cur_opt;

	if (*argv == NULL) {
		goto exit;
	}

	p = *argv++;

	for (i = 0; i < SMFS_TYPE_MAX; i++) {
		cur_opt = smfs_opt[i];
		if (!strcmp(p, cur_opt)) {
			*idx = i;
			*consumed += 1;
			goto exit;
		}
	}

	if (!strcmp(p, clear_opt))
		*clear = 1;

exit:
	return err;
}

/* Get or Clear (set)  the "smfstats" iovar, with an optional config index argument:
 *	wl smfstats [-C N]|[--cfg=N] 0
 *
 * Option:
 *	-C N
 *	--cfg=N
 *	--config=N
 *	--configuration=N
 *		specify the config index N
 * If cfg index not given on a set, the WLC_SET_SMF_STATS ioctl will be used
 */
static int
wl_smfstats(void *wl, cmd_t *cmd, char **argv)
{
	int bsscfg_idx = 0;
	int cfg_consumed = 0, smfs_consumed = 0;
	int err;
	int i, val;
	int smf_index = 0;
	int smfs_clear = 0;

	BCM_REFERENCE(cmd);

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((err = wl_cfg_option(argv, "smfstats", &bsscfg_idx, &cfg_consumed)) != 0)
		return err;

	argv += cfg_consumed;

	if ((err = wl_smfs_option(argv, &smf_index, &smfs_consumed, &smfs_clear)) != 0)
		return err;

	if (!smfs_clear) {
		if (cfg_consumed == 0) {
			if (smfs_consumed) {
				err = wlu_iovar_getbuf(wl, "smfstats", &smf_index, sizeof(int),
				   buf, WLC_IOCTL_SMLEN);
				if (!err)
					wl_disp_smfs(buf);
			}
			else {
				for (i = 0; i < SMFS_TYPE_MAX; i++) {
					smf_index = i;
					err = wlu_iovar_getbuf(wl, "smfstats", &smf_index,
					   sizeof(int), buf, WLC_IOCTL_SMLEN);
					if (!err)
						wl_disp_smfs(buf);
				}
			}
		} else {
			/* use "stats" iovar since a config option was given */
			if (smfs_consumed) {
				err = wl_bssiovar_getbuf(wl, "smfstats", bsscfg_idx, &smf_index,
				  sizeof(int), buf, WLC_IOCTL_SMLEN);
				if (!err)
					wl_disp_smfs(buf);
			}
			else {
				for (i = 0; i < SMFS_TYPE_MAX; i++) {
					smf_index = i;
					err = wl_bssiovar_getbuf(wl, "smfstats", bsscfg_idx,
						&smf_index, sizeof(int), buf, WLC_IOCTL_SMLEN);
					if (!err)
						wl_disp_smfs(buf);
				}
			}
		}
		if (err < 0)
			return err;
	} else {
		val = 0;

		if (cfg_consumed == 0)
			err = wlu_iovar_setint(wl, "smfstats", val);
		else
			err = wl_bssiovar_setint(wl, "smfstats", bsscfg_idx, val);

	}
	return err;
}


static int
wl_tssi(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;

	UNUSED_PARAMETER(argv);

	if (cmd->get < 0)
		return -1;
	if ((ret = wlu_get(wl, cmd->get, &val, sizeof(int))) < 0)
		return ret;

	val = dtoh32(val);
	printf("CCK %d OFDM %d\n", (val & 0xff), (val >> 8) & 0xff);
	return 0;
}

/* Quarter dBm units to mW
 * Table starts at QDBM_OFFSET, so the first entry is mW for qdBm=153
 * Table is offset so the last entry is largest mW value that fits in
 * a uint16.
 */

#define QDBM_OFFSET 153 /* QDBM_OFFSET */
#define QDBM_TABLE_LEN 40 /* QDBM_TABLE_LEN */

/* Smallest mW value that will round up to the first table entry, QDBM_OFFSET.
 * Value is ( mW(QDBM_OFFSET - 1) + mW(QDBM_OFFSET) ) / 2
 */
#define QDBM_TABLE_LOW_BOUND 6493 /* QDBM_TABLE_LOW_BOUND */

/* Largest mW value that will round down to the last table entry,
 * QDBM_OFFSET + QDBM_TABLE_LEN-1.
 * Value is ( mW(QDBM_OFFSET + QDBM_TABLE_LEN - 1) + mW(QDBM_OFFSET + QDBM_TABLE_LEN) ) / 2.
 */
#define QDBM_TABLE_HIGH_BOUND 64938 /* QDBM_TABLE_HIGH_BOUND */

static const uint16 nqdBm_to_mW_map[QDBM_TABLE_LEN] = {
/* qdBm:        +0	+1	+2	+3	+4	+5	+6	+7	*/
/* 153: */      6683,	7079,	7499,	7943,	8414,	8913,	9441,	10000,
/* 161: */      10593,	11220,	11885,	12589,	13335,	14125,	14962,	15849,
/* 169: */      16788,	17783,	18836,	19953,	21135,	22387,	23714,	25119,
/* 177: */      26607,	28184,	29854,	31623,	33497,	35481,	37584,	39811,
/* 185: */      42170,	44668,	47315,	50119,	53088,	56234,	59566,	63096
};

static uint16
wl_qdbm_to_mw(uint8 qdbm)
{
	uint factor = 1;
	int idx = qdbm - QDBM_OFFSET;

	if (idx >= QDBM_TABLE_LEN) {
		/* clamp to max uint16 mW value */
		return 0xFFFF;
	}

	/* scale the qdBm index up to the range of the table 0-40
	 * where an offset of 40 qdBm equals a factor of 10 mW.
	 */
	while (idx < 0) {
		idx += 40;
		factor *= 10;
	}

	/* return the mW value scaled down to the correct factor of 10,
	 * adding in factor/2 to get proper rounding.
	 */
	return ((nqdBm_to_mW_map[idx] + factor/2) / factor);
}

static uint8
wl_mw_to_qdbm(uint16 mw)
{
	uint8 qdbm;
	int offset;
	uint mw_uint = mw;
	uint boundary;

	/* handle boundary case */
	if (mw_uint <= 1)
		return 0;

	offset = QDBM_OFFSET;

	/* move mw into the range of the table */
	while (mw_uint < QDBM_TABLE_LOW_BOUND) {
		mw_uint *= 10;
		offset -= 40;
	}

	for (qdbm = 0; qdbm < QDBM_TABLE_LEN-1; qdbm++) {
		boundary = nqdBm_to_mW_map[qdbm] +
			(nqdBm_to_mW_map[qdbm+1] - nqdBm_to_mW_map[qdbm])/2;
		if (mw_uint < boundary) break;
	}

	qdbm += (uint8)offset;

	return (qdbm);
}

#define UNIT_MW		1 /* UNIT_MW */
#define UNIT_QDBM	2 /* UNIT_QDBM */
#define UNIT_DBM	3 /* UNIT_DBM */
static int
wl_txpwr1(void *wl, cmd_t *cmd, char **argv)
{
	int ret, val, new_val = 0, unit;
	const char *name = "qtxpower";
	bool override = FALSE;

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;
		if ((ret = wlu_iovar_getint(wl, name, &val)) < 0)
			return ret;

		override = ((val & WL_TXPWR_OVERRIDE) != 0);
		val &= ~WL_TXPWR_OVERRIDE;
		printf("TxPower is %d qdbm, %d.%d dbm, %d mW  Override is %s\n",
		       val, DIV_QUO(val, 4), DIV_REM(val, 4),
		       wl_qdbm_to_mw((uint8)(MIN(val, 0xff))),
		       override ? "On" : "Off");
		return 0;
	} else {
		/* for set */
		unit = UNIT_DBM;	/* default units */

		/* override can be used in combo with any unit */
		if (!strcmp(*argv, "-o")) {
			override = TRUE;
			if (!*++argv)
				return (-1);
		}

		if (!strcmp(*argv, "-d")) {
			unit = UNIT_DBM;
			argv++;
		}
		else if (!strcmp(*argv, "-q")) {
			unit = UNIT_QDBM;
			argv++;
		}
		else if (!strcmp(*argv, "-m")) {
			unit = UNIT_MW;
			argv++;
		}

		/* override can be used in combo with any unit */
		if (!strcmp(*argv, "-o")) {
			override = TRUE;
			argv++;
		}

		if (!*argv)
			return (-1);

		val = atoi(*argv);

		if (val == -1) {
			val = 127;		/* Max val of 127 qdbm */
			unit = UNIT_QDBM;
		}

		if (val <= 0) {
			return -1;
		}

		switch (unit) {
		case UNIT_MW:
			new_val = wl_mw_to_qdbm((uint16)MIN(val, 0xffff));
			break;
		case UNIT_DBM:
			new_val = val * 4;
			break;
		case UNIT_QDBM:
			new_val = val;
			break;
		}

		if (override)
			new_val |= WL_TXPWR_OVERRIDE;

		return wlu_iovar_setint(wl, name, new_val);
	}
}

static int
wl_txpwr(void *wl, cmd_t *cmd, char **argv)
{
	int error;
	uint32 val;
	char *endptr = NULL;
	uint32 override;
	const char *name = "qtxpower";

	UNUSED_PARAMETER(cmd);

	if (!*++argv) {
		if ((error = wlu_iovar_getint(wl, name, (int *)&val)) < 0)
			return error;

		/* Report power in mw with WL_TXPWR_OVERRIDE
		 * bit indicating the status
		 */
		override = ((val & WL_TXPWR_OVERRIDE) != 0);
		val &= ~WL_TXPWR_OVERRIDE;
		printf("%d.%d dBm = %d mw.  %s\n", DIV_QUO(val, 4), DIV_REM(val, 4),
			wl_qdbm_to_mw((uint8)(MIN(val, 0xff))), (override ? "(Override ON)" : ""));
		return 0;
	} else {
		if (!strcmp(*argv, "-u")) {
			override = 0;
			argv++;
		} else
			override = WL_TXPWR_OVERRIDE;

		val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0') {
			/* not all the value string was parsed by strtol */
			return -1;
		}

		val = wl_mw_to_qdbm((uint16)MIN(val, 0xffff));

		/* wl command input power will override current power set if told so */
		val |= override;

		return wlu_iovar_setint(wl, name, val);
	}
}

static int
wl_get_txpwr_limit(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	uint8 val_qdbm;
	uint16 val_mw;
	tx_power_legacy_t power;

	UNUSED_PARAMETER(argv);

	ret = wlu_get(wl, cmd->get, &power, sizeof(power));
	if (ret < 0)
		return ret;

	val_qdbm = MIN(power.txpwr_band_max[0], power.txpwr_local_max);
	val_mw = wl_qdbm_to_mw((uint8)(MIN(val_qdbm, 0xff)));

	printf("%d mW (%d.%d dBm)\n", val_mw, DIV_QUO(val_qdbm, 4), DIV_REM(val_qdbm, 4));

	return ret;
}

static int
wl_atten(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	atten_t atten;
	char *endptr;

	memset(&atten, 0, sizeof(atten_t));

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;

		if ((ret = wlu_get(wl, cmd->get, &atten, sizeof(atten_t))) < 0)
			return ret;

		printf("tx %s bb/radio/ctl1 %d/%d/%d\n",
		       (dtoh16(atten.auto_ctrl) ? "auto" : ""),
			dtoh16(atten.bb), dtoh16(atten.radio), dtoh16(atten.txctl1));

		return 0;
	} else {
		if (cmd->set < 0)
			return -1;

		if (!stricmp(*argv, "auto")) {
			atten.auto_ctrl = WL_ATTEN_PCL_ON;
			atten.auto_ctrl = htod16(atten.auto_ctrl);
		}
		else if (!stricmp(*argv, "manual")) {
			atten.auto_ctrl = WL_ATTEN_PCL_OFF;
			atten.auto_ctrl = htod16(atten.auto_ctrl);
		}
		else {
			atten.auto_ctrl = WL_ATTEN_APP_INPUT_PCL_OFF;
			atten.auto_ctrl = htod16(atten.auto_ctrl);

			atten.bb = (uint16)strtoul(*argv, &endptr, 0);
			atten.bb = htod16(atten.bb);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return -1;
			}

			if (!*++argv)
				return -1;

			atten.radio = (uint16)strtoul(*argv, &endptr, 0);
			atten.radio = htod16(atten.radio);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return -1;
			}

			if (!*++argv)
				return -1;

			atten.txctl1 = (uint16)strtoul(*argv, &endptr, 0);
			atten.txctl1 = htod16(atten.txctl1);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return -1;
			}

		}

		return wlu_set(wl, cmd->set, &atten, sizeof(atten_t));
	}
}

static int
wl_maclist(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	struct maclist *maclist = (struct maclist *) buf;
	struct ether_addr *ea;
	uint i, max = (WLC_IOCTL_MAXLEN - sizeof(int)) / ETHER_ADDR_LEN;
	uint len;

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;
		maclist->count = htod32(max);
		if ((ret = wlu_get(wl, cmd->get, maclist, WLC_IOCTL_MAXLEN)) < 0)
			return ret;
		maclist->count = dtoh32(maclist->count);
		for (i = 0, ea = maclist->ea; i < maclist->count && i < max; i++, ea++)
			printf("%s %s\n", cmd->name, wl_ether_etoa(ea));
		return 0;
	} else {
		if (cmd->set < 0)
			return -1;
		/* Clear list */
		maclist->count = htod32(0);
		if (!stricmp(*argv, "none") || !stricmp(*argv, "clear"))
			return wlu_set(wl, cmd->set, maclist, sizeof(int));
		/* Get old list */
		maclist->count = htod32(max);
		if ((ret = wlu_get(wl, cmd->get, maclist, WLC_IOCTL_MAXLEN)) < 0)
			return ret;
		/* Append to old list */
		maclist->count = dtoh32(maclist->count);
		ea = &maclist->ea[maclist->count];
		while (*argv && maclist->count < max) {
			if (!wl_ether_atoe(*argv, ea)) {
				printf("Problem parsing MAC address \"%s\".\n", *argv);
				return -1;
			}
			maclist->count++;
			ea++;
			argv++;
		}
		/* Set new list */
		len = sizeof(maclist->count) + maclist->count * sizeof(maclist->ea);
		maclist->count = htod32(maclist->count);
		return wlu_set(wl, cmd->set, maclist, len);
	}
}

static int
wl_maclist_1(void *wl, cmd_t *cmd, char **argv)
{
	struct maclist *maclist;
	struct ether_addr *ea;
	uint i;
	int ret;

	strcpy(buf, argv[0]);

	if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return ret;

	maclist = (struct maclist *)buf;

	for (i = 0, ea = maclist->ea; i < dtoh32(maclist->count); i++, ea++)
		printf("%s %s\n", cmd->name, wl_ether_etoa(ea));
	return 0;
}

static int
wl_out(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	return wlu_set(wl, WLC_OUT, NULL, 0);
}
#endif /* !ATE_BUILD */

static int
wl_band(void *wl, cmd_t *cmd, char **argv)
{
	uint band;
	int error;

	UNUSED_PARAMETER(cmd);

	error = 0;

	argv++;

	if (*argv == NULL) {	/* get current band */
		if ((error = wlu_get(wl, WLC_GET_BAND, &band, sizeof(uint))) < 0)
			return (error);
		band = dtoh32(band);

		if (band == WLC_BAND_AUTO)
			printf("auto\n");
		else if (band == WLC_BAND_5G)
			printf("a\n");
		else if (band == WLC_BAND_2G)
			printf("b\n");
		else
			printf("unrecognized band value %d\n", band);
	} else {		/* set the band */
		if (!stricmp(*argv, "auto"))
			band = WLC_BAND_AUTO;
		else if (!stricmp(*argv, "a"))
			band = WLC_BAND_5G;
		else if (!stricmp(*argv, "b"))
			band = WLC_BAND_2G;
		else {
			printf("unsupported band: %s\n", *argv);
			return (-1);
		}

		band = htod32(band);
		error = wlu_set(wl, WLC_SET_BAND, &band, sizeof(uint));

	}
	return (error);
}

#ifndef ATE_BUILD
static int
wl_bandlist(void *wl, cmd_t *cmd, char **argv)
{
	uint list[3];
	int error;
	uint i;

	UNUSED_PARAMETER(cmd);

	error = 0;

	argv++;

	if ((error = wlu_get(wl, WLC_GET_BANDLIST, list, sizeof(list))) < 0)
		return (error);
	list[0] = dtoh32(list[0]);
	list[1] = dtoh32(list[1]);
	list[2] = dtoh32(list[2]);

	/* list[0] is count, followed by 'count' bands */

	if (list[0] > 2)
		list[0] = 2;

	for (i = 1; i <= list[0]; i++)
		if (list[i] == WLC_BAND_5G)
			printf("a ");
		else if (list[i] == WLC_BAND_2G)
			printf("b ");
		else
			printf("? ");
	printf("\n");

	return (0);
}

static int
wl_phylist(void *wl, cmd_t *cmd, char **argv)
{
	char phylist_buf[128];
	int error;
	char *cp;

	UNUSED_PARAMETER(cmd);

	error = 0;

	argv++;

	if ((error = wlu_get(wl, WLC_GET_PHYLIST, phylist_buf, sizeof(phylist_buf))) < 0)
		return (error);

	cp = phylist_buf;

	for (; *cp; cp++)
		printf("%c ", *cp);
	printf("\n");

	return (0);
}

#ifdef linux
#define UPGRADE_BUFSIZE	512 /* upgrade buffer size */
#else
#define UPGRADE_BUFSIZE	1024 /* upgrade buffer size */
#endif /* linux */

static int
wl_upgrade(void *wl, cmd_t *cmd, char **argv)
{
#if !defined(BWL_FILESYSTEM_SUPPORT)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return (-1);
#elif	defined(_CFE_)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return CFE_ERR_UNSUPPORTED;
#else
	FILE *fp;
	int ret = 0;
	struct {
		uint32 offset;
		char buf[UPGRADE_BUFSIZE];
	} block;
	uint32 offset;
	uint len;

	if (!*++argv)
		return -1;

	if (!(fp = fopen(*argv, "rb"))) {
		fprintf(stderr, "%s: No such file or directory\n", *argv);
		return -2;
	}

	printf("Programming %s...", *argv);
	fflush(stdout);
	offset = 0;
	block.offset = htod32(offset);
	while ((len = fread(block.buf, 1, sizeof(block.buf), fp))) {
		if ((ret = wlu_set(wl, cmd->set, &block, 4 + len)) < 0)
			break;
		offset += len;
		block.offset = htod32(offset);
		printf(".");
		fflush(stdout);
	}

	if (ferror(fp)) {
		ret = ferror(fp);
		printf("\nerror reading %s\n", *argv);
	} else {
		long status = WLC_UPGRADE_PENDING;
		int retries;

		printf("\nCommitting image to flash...\n");
		while (status == WLC_UPGRADE_PENDING) {
			retries = 10;
retry:
			if ((ret = wlu_get(wl, WLC_UPGRADE_STATUS,
				&status, sizeof(status))) < 0) {
				/* the first attempt to get status will
				 * likely fail due to dev reset
				 */
				if (retries--)
					goto retry;
				break;
			}
			status = dtoh32(status);
		}
		if (status == WLC_UPGRADE_SUCCESS)
			printf("\nDone\n\nSuccessfully downloaded %d bytes\n", block.offset);
		else
			fprintf(stderr, "\n*** UPGRADE FAILED! *** (status %ld)\n", status);
	}

	fclose(fp);
	return ret;
#endif   /* BWL_FILESYSTEM_SUPPORT */
}

#include <bcmnvram.h>

static int
wl_otpw(void *wl, cmd_t *cmd, char **argv)
{
#if !defined(BWL_FILESYSTEM_SUPPORT)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return (-1);
#elif	defined(_CFE_)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return CFE_ERR_UNSUPPORTED;
#else
	FILE *fp;
	int ret = 0;
	struct nvram_header *nvr;
	char *p, otpw_buf[1024 - 128];
	const char *msg;
	int len;

	if (!*++argv)
		return -1;

	if (!(fp = fopen(*argv, "rb"))) {
		fprintf(stderr, "%s: No such file or directory\n", *argv);
		return -2;
	}

	len = fread(otpw_buf, 1, sizeof(otpw_buf) - 1, fp);
	if ((ret = ferror(fp))) {
		printf("\nerror %d reading %s\n", ret, *argv);
		ret = -3;
		goto out;
	}
	if (!feof(fp)) {
		printf("\nFile %s too large\n", *argv);
		ret = -4;
		goto out;
	}

	/* Got the bits, do they look like the output of nvserial? */
	nvr = (struct nvram_header *)otpw_buf;
	if (nvr->magic == NVRAM_MAGIC) {
		if (cmd->set == WLC_OTPW) {
			printf("File %s looks like an nvserial file, use nvotpw\n", *argv);
			fflush(stdout);
			ret = -5;
			goto out;
		}
		len  = nvr->len - sizeof(struct nvram_header);
		if (len <= 0) {
			printf("Invalid length (%d)\n", len);
			ret = -8;
			goto out;
		}
		if (len & 1) {
			otpw_buf[len++] = '\0';
		}
		p = (char *)(nvr + 1);
		msg = "nvserial";
	} else {
		if (cmd->set == WLC_NVOTPW) {
			printf("File %s is not an nvserial file\n", *argv);
			ret = -6;
			goto out;
		}
		if (len & 1) {
			printf("File %s has an odd length (%d)\n", *argv, len);
			ret = -7;
			goto out;
		}
		p = otpw_buf;
		msg = "raw";
	}

	printf("Writing %d bytes from %s file %s to otp ...\n", len, msg, *argv);
	fflush(stdout);

	if ((ret = wlu_set(wl, cmd->set, p, len)) < 0) {
		printf("\nError %d writing %s to otp\n", ret, *argv);
	}

out:
	fclose(fp);
	return ret;
#endif /* BWL_FILESYSTEM_SUPPORT */
}

static int
wl_get_pktcnt(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	get_pktcnt_t pktcnt;

	UNUSED_PARAMETER(argv);

	memset(&pktcnt, 0, sizeof(pktcnt));
	if ((ret = wlu_get(wl, cmd->get, &pktcnt, sizeof(pktcnt))) < 0)
		return ret;

	printf("Receive: good packet %d, bad packet %d, othercast good packet %d\n",
		dtoh32(pktcnt.rx_good_pkt), dtoh32(pktcnt.rx_bad_pkt),
		dtoh32(pktcnt.rx_ocast_good_pkt));
	printf("Transmit: good packet %d, bad packet %d\n",
		dtoh32(pktcnt.tx_good_pkt), dtoh32(pktcnt.tx_bad_pkt));

	return ret;
}

static int
wl_interfere(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char *endptr = NULL;
	int mode;

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;
		if ((ret = wlu_get(wl, cmd->get, &mode, sizeof(mode))) < 0)
			return ret;
		mode = dtoh32(mode);
		switch (mode & 0x7f) {
		case INTERFERE_NONE:
			printf("All interference mitigation is disabled. (mode 0)\n");
			break;
		case NON_WLAN:
			printf("Non-wireless LAN Interference mitigation is enabled. (mode 1)\n");
			break;
		case WLAN_MANUAL:
			printf("Wireless LAN Interference mitigation is enabled. (mode 2)\n");
			break;
		case WLAN_AUTO:
			printf("Auto Wireless LAN Interference mitigation is enabled and ");
			if (mode & AUTO_ACTIVE)
				printf("active. (mode 3)\n");
			else
				printf("not active. (mode 3)\n");

			break;
		case WLAN_AUTO_W_NOISE:
			printf("Auto Wireless LAN Interference mitigation is enabled and ");
			if (mode & AUTO_ACTIVE)
				printf("active, ");
			else
				printf("not active, ");

			printf("and noise reduction is enabled. (mode 4)\n");
			break;
		}
		return 0;
	} else {
		mode = INTERFERE_NONE;
		val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0')
			return (-1);

		switch (val) {
			case 0:
				mode = INTERFERE_NONE;
				break;
			case 1:
				mode = NON_WLAN;
				break;
			case 2:
				mode = WLAN_MANUAL;
				break;
			case 3:
				mode = WLAN_AUTO;
				break;
			case 4:
				mode = WLAN_AUTO_W_NOISE;
				break;
			default:
				return (-1);
		}

		mode = htod32(mode);
		return wlu_set(wl, cmd->set, &mode, sizeof(mode));
	}
}

static int
wl_interfere_override(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char *endptr;
	int mode;

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;
		if ((ret = wlu_get(wl, cmd->get, &mode, sizeof(mode))) < 0) {
			return ret;
		}
		mode = dtoh32(mode);
		switch (mode) {
		case INTERFERE_NONE:
			printf("Interference override NONE, "
			"all mitigation disabled. (mode 0)\n");
			break;
		case NON_WLAN:
			printf("Interference override enabled. "
				" Non-wireless LAN Interference mitigation is enabled. (mode 1)\n");
			break;
		case WLAN_MANUAL:
			printf("Interference override enabled.  "
			" Wireless LAN Interference mitigation is enabled. (mode 2)\n");
			break;
		case WLAN_AUTO:
			printf("Interference override enabled. "
				" Interference mitigation is enabled and ");
			if (mode & AUTO_ACTIVE)
				printf("active. (mode 3)\n");
			else
				printf("not active. (mode 3)\n");

			break;
		case WLAN_AUTO_W_NOISE:
			printf("Interference override enabled. "
				" Interference mitigation is enabled and ");
			if (mode & AUTO_ACTIVE)
				printf("active, ");
			else
				printf("not active, ");

			printf("and noise reduction is enabled. (mode 4)\n");
			break;
		case INTERFERE_OVRRIDE_OFF:
			printf("Interference override disabled. \n");
			break;
		}
		return 0;
	} else {
		mode = INTERFERE_NONE;
		val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0')
			return (-1);

		switch (val) {
			case 0:
				mode = INTERFERE_NONE;
				break;
			case 1:
				mode = NON_WLAN;
				break;
			case 2:
				mode = WLAN_MANUAL;
				break;
			case 3:
				mode = WLAN_AUTO;
				break;
			case 4:
				mode = WLAN_AUTO_W_NOISE;
				break;
			case INTERFERE_OVRRIDE_OFF:
				mode = INTERFERE_OVRRIDE_OFF;
				break;
			default:
				return (-1);
		}

		mode = htod32(mode);
		return wlu_set(wl, cmd->set, &mode, sizeof(mode));
	}
}

static cntry_name_t *
wlc_cntry_name_to_country(char *long_name)
{
	cntry_name_t *cntry;
	for (cntry = cntry_names; cntry->name &&
		stricmp(long_name, cntry->name); cntry++);
	return (!cntry->name ? NULL : cntry);
}

static cntry_name_t *
wlc_cntry_abbrev_to_country(const char *abbrev)
{
	cntry_name_t *cntry;
	if (!*abbrev || strlen(abbrev) > 3 || strlen(abbrev) < 2)
		return (NULL);
	for (cntry = cntry_names; cntry->name &&
		strnicmp(abbrev, cntry->abbrev, strlen(abbrev)); cntry++);
	return (!cntry->name ? NULL : cntry);
}

static int
wl_parse_country_spec(const char *spec, char *ccode, int *regrev)
{
	char *revstr;
	char *endptr = NULL;
	int ccode_len;
	int rev = -1;

	revstr = strchr(spec, '/');

	if (revstr) {
		rev = strtol(revstr + 1, &endptr, 10);
		if (*endptr != '\0') {
			/* not all the value string was parsed by strtol */
			fprintf(stderr,
				"Could not parse \"%s\" as a regulatory revision "
				"in the country string \"%s\"\n",
				revstr + 1, spec);
			return USAGE_ERROR;
		}
	}

	if (revstr)
		ccode_len = (int)(uintptr)(revstr - spec);
	else
		ccode_len = (int)strlen(spec);

	if (ccode_len > 3) {
		fprintf(stderr,
			"Could not parse a 2-3 char country code "
			"in the country string \"%s\"\n",
			spec);
		return USAGE_ERROR;
	}

	memcpy(ccode, spec, ccode_len);
	ccode[ccode_len] = '\0';
	*regrev = rev;

	return 0;
}

int
wl_country(void *wl, cmd_t *cmd, char **argv)
{
	cntry_name_t *cntry;
	wl_country_t cspec = {{0}, 0, {0}};
	int argc = 0;
	int err;
	int bcmerr = 1;

	/* skip the command name */
	argv++;

	/* find the arg count */
	while (argv[argc])
		argc++;

	/* check arg list count */
	if (argc > 2) {
		fprintf(stderr, "Too many arguments (%d) for command %s\n", argc, cmd->name);
		return USAGE_ERROR;
	}

	buf[0] = 0;
	if (argc == 0) {
		const char* name = "<unknown>";

		/* first try the country iovar */
		err = wlu_iovar_get(wl, "country", &cspec, sizeof(cspec));

		if (!err) {
			cntry = wlc_cntry_abbrev_to_country(cspec.country_abbrev);
			if (cntry)
				name = cntry->name;
			cspec.rev = dtoh32(cspec.rev);

			printf("%s (%s/%d) %s\n",
			       cspec.country_abbrev, cspec.ccode, cspec.rev, name);

			return 0;
		}

		/* if there was an error other than BCME_UNSUPPORTED, fail now */
		wlu_iovar_getint(wl, "bcmerror", &bcmerr);
		if (bcmerr != BCME_UNSUPPORTED)
			return err;

		/* if the "country" iovar is unsupported, try the WLC_SET_COUNTRY ioctl */
		if ((err = wlu_get(wl, cmd->get, &buf[0], WLC_IOCTL_SMLEN)) < 0)
			return err;
		if (strlen(buf) == 0) {
			printf("No country set\n");
			return 0;

		}
		cntry = wlc_cntry_abbrev_to_country(buf);
		if (cntry != NULL)
			name = cntry->name;

		printf("%s () %s\n", buf, name);
		return 0;
	}

	if (!stricmp(*argv, "list")) {
		uint i;
		const char* abbrev;
		wl_country_list_t *cl = (wl_country_list_t *)buf;

		cl->buflen = WLC_IOCTL_MAXLEN;
		cl->count = 0;

		/* band may follow */
		if (*++argv) {
			cl->band_set = TRUE;
			if (!stricmp(*argv, "a"))
				cl->band = WLC_BAND_5G;
			else if (!stricmp(*argv, "b") || !stricmp(*argv, "g"))
				cl->band = WLC_BAND_2G;
			else {
				printf("unsupported band: %s\n", *argv);
				return -1;
			}
		} else {
			cl->band_set = FALSE;
		}

		cl->buflen = htod32(cl->buflen);
		cl->band_set = htod32(cl->band_set);
		cl->band = htod32(cl->band);
		cl->count = htod32(cl->count);
		err = wlu_get(wl, WLC_GET_COUNTRY_LIST, buf, WLC_IOCTL_MAXLEN);
		if (err < 0)
			return err;

		printf("Supported countries: country code and long name\n");
		for (i = 0; i < dtoh32(cl->count); i++) {
			abbrev = &cl->country_abbrev[i*WLC_CNTRY_BUF_SZ];
			cntry = wlc_cntry_abbrev_to_country(abbrev);
			printf("%s\t%s\n", abbrev, cntry ? cntry->name : "");
		}
		return 0;
	}

	memset(&cspec, 0, sizeof(cspec));
	cspec.rev = -1;

	if (argc == 1) {
		/* check for the first arg being a country name, e.g. "United States",
		 * or country spec, "US/1", or just a country code, "US"
		 */
		if ((cntry = wlc_cntry_name_to_country(argv[0])) != NULL) {
			/* arg matched a country name */
			memcpy(cspec.country_abbrev, cntry->abbrev, WLC_CNTRY_BUF_SZ);
			err = 0;
		} else {
			/* parse a country spec, e.g. "US/1", or a country code.
			 * cspec.rev will be -1 if not specified.
			 */
			err = wl_parse_country_spec(argv[0], cspec.country_abbrev, &cspec.rev);
		}

		if (err) {
			fprintf(stderr,
				"Argument \"%s\" could not be parsed as a country name, "
				"country code, or country code and regulatory revision.\n",
				argv[0]);
			return USAGE_ERROR;
		}

		/* if the arg was a country spec, then fill out ccdoe and rev,
		 * and leave country_abbrev defaulted to the ccode
		 */
		if (cspec.rev != -1)
			memcpy(cspec.ccode, cspec.country_abbrev, WLC_CNTRY_BUF_SZ);
	} else {
		/* for two args, the first needs to be a country code or country spec */
		err = wl_parse_country_spec(argv[0], cspec.ccode, &cspec.rev);
		if (err) {
			fprintf(stderr,
				"Argument 1 \"%s\" could not be parsed as a country code, or "
				"country code and regulatory revision.\n",
				argv[0]);
			return USAGE_ERROR;
		}

		/* the second arg needs to be a country name or country code */
		if ((cntry = wlc_cntry_name_to_country(argv[1])) != NULL) {
			/* arg matched a country name */
			memcpy(cspec.country_abbrev, cntry->abbrev, WLC_CNTRY_BUF_SZ);
		} else {
			int rev;
			err = wl_parse_country_spec(argv[1], cspec.country_abbrev, &rev);
			if (rev != -1) {
				fprintf(stderr,
					"Argument \"%s\" had a revision. Arg 2 must be "
					"a country name or country code without a revision\n",
					argv[1]);
				return USAGE_ERROR;
			}
		}

		if (err) {
			fprintf(stderr,
				"Argument 2 \"%s\" could not be parsed as "
				"a country name or country code\n",
				argv[1]);
			return USAGE_ERROR;
		}
	}

	/* first try the country iovar */
	if (cspec.rev == -1 && cspec.ccode[0] == '\0')
		err = wlu_iovar_set(wl, "country", &cspec, WLC_CNTRY_BUF_SZ);
	else {
		cspec.rev = htod32(cspec.rev);
		err = wlu_iovar_set(wl, "country", &cspec, sizeof(cspec));
	}

	if (err == 0)
		return 0;

	/* if there was an error other than BCME_UNSUPPORTED, fail now */
	wlu_iovar_getint(wl, "bcmerror", &bcmerr);
	if (bcmerr != BCME_UNSUPPORTED)
		return err;

	/* if the "country" iovar is unsupported, try the WLC_SET_COUNTRY ioctl if possible */

	if (cspec.rev != -1 || cspec.ccode[0] != '\0') {
		fprintf(stderr,
			"Driver does not support full country spec interface, "
			"only a country name or code may be sepcified\n");
		return err;
	}

	/* use the legacy ioctl */
	err = wlu_set(wl, WLC_SET_COUNTRY, cspec.country_abbrev, WLC_CNTRY_BUF_SZ);

	return err;
}

int
wl_country_ie_override(void *wl, cmd_t *cmd, char **argv)
{
	int argc = 0;
	int error, i;

	/* skip the command name */
	argv++;

	/* find the arg count */
	while (argv[argc])
		argc++;

	if (argc == 0) {
		void *ptr;
		bcm_tlv_t *ie;

		if ((error = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return (error);

		ie = (bcm_tlv_t *)ptr;

		printf("ie tag:0x%x ie len:0x%x ie data:", ie->id, ie->len);

		for (i = 0; i < ie->len; i++)
			printf("0x%x ", ie->data[i]);

		printf("\n");

		return error;

	} else {
		/* Set */
		char *endptr = NULL;
		uchar *valsp;
		int8 ie_len, pad = 0;

		/* retrieve the ie len in advance to check for padding */
		ie_len = (int8)strtol(*(argv + 1), NULL, 0);
		if (ie_len & 1) {
			fprintf(stderr, "country ie len is odd(%d), padding by 1 octet\n", ie_len);
			pad = 1;
		}

		valsp = (uchar*)malloc(argc + pad);
		if (valsp == NULL) {
			fprintf(stderr, "Error allocating %d bytes country ie\n", argc);
			return -1;
		}
		memset(valsp, 0, argc + pad);

		for (i = 0; i < argc; i++, argv++) {

			valsp[i] = (uchar)strtol(*argv, &endptr, 0);

			/* make sure all the value string was parsed by strtol */
			if (*endptr != '\0') {
				free(valsp);
				return -1;
			}
		}

		/* update ie len if padded */
		if (pad) {
			valsp[1] += 1;
			valsp[ie_len + TLV_HDR_LEN] = 0;
		}

		error = wlu_var_setbuf(wl, cmd->name, valsp, argc + pad);

		free(valsp);

		return error;
	}
}

#define ACI_SPIN	"spin"
#define ACI_ENTER	"enter"
#define ACI_EXIT	"exit"
#define ACI_GLITCH	"glitch"

#define NPHY_ACI_ADCPWR_ENTER "adcpwr_enter"
#define NPHY_ACI_ADCPWR_EXIT "adcpwr_exit"
#define NPHY_ACI_REPEAT_CTR "repeat"
#define NPHY_ACI_NUM_SAMPLES "samples"
#define NPHY_ACI_UNDETECT "undetect_sz"
#define NPHY_ACI_LOPWR "loaci"
#define NPHY_ACI_MDPWR "mdaci"
#define NPHY_ACI_HIPWR "hiaci"
#define NPHY_ACI_NOISE_NOASSOC_GLITCH_TH_UP "nphy_noise_noassoc_glitch_th_up"
#define NPHY_ACI_NOISE_NOASSOC_GLITCH_TH_DN "nphy_noise_noassoc_glitch_th_dn"
#define NPHY_ACI_NOISE_ASSOC_GLITCH_TH_UP "nphy_noise_assoc_glitch_th_up"
#define NPHY_ACI_NOISE_ASSOC_GLITCH_TH_DN "nphy_noise_assoc_glitch_th_dn"
#define NPHY_ACI_NOISE_ASSOC_ACI_GLITCH_TH_UP "nphy_noise_assoc_aci_glitch_th_up"
#define NPHY_ACI_NOISE_ASSOC_ACI_GLITCH_TH_DN "nphy_noise_assoc_aci_glitch_th_dn"
#define NPHY_ACI_NOISE_NOASSOC_ENTER_TH "nphy_noise_noassoc_enter_th"
#define NPHY_ACI_NOISE_ASSOC_ENTER_TH "nphy_noise_assoc_enter_th"
#define NPHY_ACI_NOISE_ASSOC_RX_GLITCH_BADPLCP_ENTER_TH \
"nphy_noise_assoc_rx_glitch_badplcp_enter_th"
#define NPHY_ACI_NOISE_ASSOC_CRSIDX_INCR "nphy_noise_assoc_crsidx_incr"
#define NPHY_ACI_NOISE_NOASSOC_CRSIDX_INCR "nphy_noise_noassoc_crsidx_incr"
#define NPHY_ACI_NOISE_CRSIDX_DECR "nphy_noise_crsidx_decr"


#if defined(BWL_FILESYSTEM_SUPPORT)
#if !defined(_CFE_)
static int
wl_do_samplecollect_n(void *wl, wl_samplecollect_args_t *collect, uint8 *buff, FILE *fp)
{
	uint16 nbytes;
	int ret = 0;

	ret = wlu_iovar_getbuf(wl, "sample_collect", collect, sizeof(wl_samplecollect_args_t),
		buff, WLC_SAMPLECOLLECT_MAXLEN);

	if (ret)
		return ret;

	/* bytes 1:0 indicate capture length */
	while ((nbytes = ltoh16_ua(buff))) {
		nbytes += 2;
		ret = fwrite(buff, 1, nbytes, fp);
		if (ret != nbytes) {
			fprintf(stderr, "Error writing %d bytes to file, rc %d!\n",
				nbytes, ret);
			ret = -1;
			break;
		} else {
			fprintf(stderr, "Wrote %d bytes\n", nbytes);
		}
		buff += nbytes;
	}
	return (ret);
}

static int
wl_do_samplecollect(void *wl, wl_samplecollect_args_t *collect, int sampledata_version,
	uint32 *buff, FILE *fp)
{
	uint16 nbytes, tag;
	uint32 flag, *header, sync;
	uint8 *ptr;
	int err;
	wl_sampledata_t *sample_collect;
	wl_sampledata_t sample_data, *psample;

	err = wlu_iovar_getbuf(wl, "sample_collect", collect, sizeof(wl_samplecollect_args_t),
		buff, WLC_SAMPLECOLLECT_MAXLEN);

	if (err)
		return err;

	sample_collect = (wl_sampledata_t *)buff;
	header = (uint32 *)&sample_collect[1];
	tag = ltoh16_ua(&sample_collect->tag);
	if (tag != WL_SAMPLEDATA_HEADER_TYPE) {
		fprintf(stderr, "Expect SampleData Header type %d, receive type %d\n",
			WL_SAMPLEDATA_HEADER_TYPE, tag);
		return -1;
	}

	nbytes = ltoh16_ua(&sample_collect->length);
	flag = ltoh32_ua(&sample_collect->flag);
	sync = ltoh32_ua(&header[0]);
	if (sync != 0xACDC2009) {
		fprintf(stderr, "Header sync word mismatch (0x%08x)\n", sync);
		return -1;
	}

	err = fwrite((uint8 *)header, 1, nbytes, fp);
	if (err != (int)nbytes)
		  fprintf(stderr, "Failed write file-header to file %d\n", err);

	memset(&sample_data, 0, sizeof(wl_sampledata_t));
	sample_data.version = sampledata_version;
	sample_data.size = htol16(sizeof(wl_sampledata_t));
	flag = 0;
	/* new format, used in htphy */
	do {
		sample_data.tag = htol16(WL_SAMPLEDATA_TYPE);
		sample_data.length = htol16(WLC_SAMPLECOLLECT_MAXLEN);
		/* mask seq# */
	        sample_data.flag = htol32((flag & 0xff));

		err = wlu_iovar_getbuf(wl, "sample_data", &sample_data, sizeof(wl_sampledata_t),
			buff, WLC_SAMPLECOLLECT_MAXLEN);
		if (err) {
			fprintf(stderr, "Error reading back sample collected data\n");
			err = -1;
			break;
		}

		ptr = (uint8 *)buff + sizeof(wl_sampledata_t);
		psample = (wl_sampledata_t *)buff;
		tag = ltoh16_ua(&psample->tag);
		nbytes = ltoh16_ua(&psample->length);
		flag = ltoh32_ua(&psample->flag);
		if (tag != WL_SAMPLEDATA_TYPE) {
			fprintf(stderr, "Expect SampleData type %d, receive type %d\n",
				WL_SAMPLEDATA_TYPE, tag);
			err = -1;
			break;
		}
		if (nbytes == 0) {
			fprintf(stderr, "Done retrieving sample data\n");
			err = -1;
			break;
		}
		err = fwrite(ptr, 1, nbytes, fp);
		if (err != (int)nbytes) {
			fprintf(stderr, "Error writing %d bytes to file, rc %d!\n",
				(int)nbytes, err);
			err = -1;
			break;
		} else {
			printf("Wrote %d bytes\n", err);
		}
	} while (flag & WL_SAMPLEDATA_MORE_DATA);
	return err;
}
#endif 
#endif /* defined(BWL_FILESYSTEM_SUPPORT) */

static int
wl_sample_collect(void *wl, cmd_t *cmd, char **argv)
{
#if !defined(BWL_FILESYSTEM_SUPPORT)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return (-1);
#elif defined(_CFE_)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return CFE_ERR_UNSUPPORTED;
#else
	int ret = -1;
	uint8 *buff = NULL;
	wl_samplecollect_args_t collect;
	const char *fname = "sample_collect.dat";
	wlc_rev_info_t revinfo;
	uint32 phytype, phyrev;
	FILE *fp = NULL;

	/* Default setting for sampledata_version */
	int sampledata_version = htol16(WL_SAMPLEDATA_T_VERSION);

	UNUSED_PARAMETER(cmd);

	memset(&revinfo, 0, sizeof(revinfo));
	if ((ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo))) < 0)
		return ret;

	phytype = dtoh32(revinfo.phytype);
	phyrev = dtoh32(revinfo.phyrev);

	/* Assign some default params first */
	/* 60us is roughly the max we can store (for NPHY with NREV < 7). */
	collect.coll_us = 60;
	collect.cores = -1;
	collect.bitStart = -1;
	/* extended settings */
	collect.trigger = TRIGGER_NOW;
		collect.mode = 1;
		collect.post_dur = 10;
	collect.pre_dur = 10;
	collect.gpio_sel = 0;
	collect.downsamp = FALSE;
	collect.be_deaf = FALSE;
	collect.timeout = 1000;
	collect.agc = FALSE;
	collect.filter = FALSE;
	collect.version = WL_SAMPLECOLLECT_T_VERSION;
	collect.length = sizeof(wl_samplecollect_args_t);

	/* Skip the command name */
	argv++;
	ret = -1;
	while (*argv) {
		char *s = *argv;

		if (argv[1] == NULL)
			goto exit;
		if (!strcmp(s, "-f"))
			fname = argv[1];
		else if (!strcmp(s, "-u"))
			collect.coll_us = atoi(argv[1]);
		else if (!strcmp(s, "-c"))
			collect.cores = atoi(argv[1]);
		/* extended settings */
		else if (!strcmp(s, "-t")) {
			/* event trigger */
			if (!strcmp(argv[1], "crs"))
				collect.trigger = TRIGGER_CRS;
			else if (!strcmp(argv[1], "crs_deassert"))
				collect.trigger = TRIGGER_CRSDEASSERT;
			else if (!strcmp(argv[1], "good_fcs"))
				collect.trigger = TRIGGER_GOODFCS;
			else if (!strcmp(argv[1], "bad_fcs"))
				collect.trigger = TRIGGER_BADFCS;
			else if (!strcmp(argv[1], "bad_plcp"))
				collect.trigger = TRIGGER_BADPLCP;
			else if (!strcmp(argv[1], "crs_glitch"))
				collect.trigger = TRIGGER_CRSGLITCH;
		}
		else if (!strcmp(s, "-m")) {
			if (!strcmp(argv[1], "gpio")) {
				if (phytype == WLC_PHY_TYPE_HT) {
					collect.mode = 4;
				} else {
					/* MIMOPHY */
					collect.mode = 0xff;
				}
			} else {
			collect.mode = atoi(argv[1]);
			}
		}
		else if (!strcmp(s, "-k"))
			collect.gpioCapMask = atoi(argv[1]);
		else if (!strcmp(s, "-s"))
			collect.bitStart = atoi(argv[1]);
		else if (!strcmp(s, "-b"))
			collect.pre_dur = atoi(argv[1]);
		else if (!strcmp(s, "-a"))
			collect.post_dur = atoi(argv[1]);
		else if (!strcmp(s, "-g"))
			collect.gpio_sel = atoi(argv[1]);
		else if (!strcmp(s, "-d"))
			collect.downsamp = atoi(argv[1]);
		else if (!strcmp(s, "-e"))
			collect.be_deaf = atoi(argv[1]);
		else if (!strcmp(s, "-i"))
			collect.timeout = atoi(argv[1]);
		else if (!strcmp(s, "--agc")) {
			/* perform software agc for sample collect */
			collect.agc = atoi(argv[1]);
		}
		else if (!strcmp(s, "--filter")) {
			/* Set HPC for LPF to lowest possible value (0x1) */
			collect.filter = atoi(argv[1]);
		}
		else if (!strcmp(s, "-v"))
			 sampledata_version = atoi(argv[1]);
		else
			goto exit;

		argv += 2;
	}

	buff = malloc(WLC_SAMPLECOLLECT_MAXLEN);
	if (buff == NULL) {
		fprintf(stderr, "Failed to allocate dump buffer of %d bytes\n",
			WLC_SAMPLECOLLECT_MAXLEN);
		return -1;
	}
	memset(buff, 0, WLC_SAMPLECOLLECT_MAXLEN);

	if ((fp = fopen(fname, "wb")) == NULL) {
		fprintf(stderr, "Problem opening file %s\n", fname);
		ret = -1;
		goto exit;
	}

	if ((phytype == WLC_PHY_TYPE_HT) || (phytype == WLC_PHY_TYPE_AC)) {
		ret = wl_do_samplecollect(wl, &collect, sampledata_version, (uint32 *)buff, fp);
	} else if (phytype == WLC_PHY_TYPE_N) {
		if (phyrev < 7) {
		ret = wl_do_samplecollect_n(wl, &collect, buff, fp);
		} else {
			ret = wl_do_samplecollect(wl, &collect, sampledata_version,
				(uint32 *)buff, fp);
		}
	}

exit:
	if (buff) free(buff);
	if (fp) fclose(fp);
	return ret;
#endif /* !BWL_FILESYSTEM_SUPPORT */
}

static int
wl_ampdu_activate_test(void *wl, cmd_t *cmd, char **argv)
{
	char *param;
	const char *cmdname = "ampdu_activate_test";
	struct agg {
		bool val1;
		bool val2;
	} x;
	int err = 0;

	UNUSED_PARAMETER(cmd);

	if ((param = *++argv) == NULL)
		return USAGE_ERROR;

	x.val1 = atoi(param);
	if ((param = *++argv)) {
		x.val2 = atoi(param);
		printf("%d %d\n", x.val1, x.val2);
		err = wlu_var_setbuf(wl, cmdname, &x, sizeof(x));
	}
	return err;
}

static int
wl_ampdu_tid(void *wl, cmd_t *cmd, char **argv)
{
	char *param;
	const char *cmdname = "ampdu_tid";
	struct ampdu_tid_control atc, *reply;
	uint8 tid;
	int err;
	void *ptr = NULL;

	UNUSED_PARAMETER(cmd);

	if ((param = *++argv) == NULL)
		return USAGE_ERROR;

	tid = atoi(param);
	if (tid > MAXPRIO)
		return USAGE_ERROR;
	atc.tid = tid;

	if ((param = *++argv)) {
		atc.enable = atoi(param);
		err = wlu_var_setbuf(wl, cmdname, &atc, sizeof(atc));
	} else {
		if ((err = wlu_var_getbuf_sm(wl, cmdname, &atc, sizeof(atc), &ptr) < 0))
			return err;
		reply = (struct ampdu_tid_control *)ptr;
		printf("AMPDU for tid %d: %d\n", tid, reply->enable);
	}
	return err;
}

static int
wl_ampdu_retry_limit_tid(void *wl, cmd_t *cmd, char **argv)
{
	char *param;
	const char *cmdname = "ampdu_retry_limit_tid";
	struct ampdu_retry_tid retry_limit, *reply;
	uint8 tid;
	int err;
	void *ptr = NULL;

	UNUSED_PARAMETER(cmd);

	if ((param = *++argv) == NULL)
		return USAGE_ERROR;

	tid = atoi(param);
	if (tid > MAXPRIO)
		return USAGE_ERROR;
	retry_limit.tid = tid;

	if ((param = *++argv)) {
		retry_limit.retry = atoi(param);
		err = wlu_var_setbuf(wl, cmdname, &retry_limit, sizeof(retry_limit));
	} else {
		if ((err = wlu_var_getbuf(wl, cmdname, &retry_limit,
			sizeof(retry_limit), &ptr)) < 0)
			return err;
		reply = (struct ampdu_retry_tid *)ptr;
		printf("AMPDU retry limit for tid %d: %d\n", tid, reply->retry);
	}
	return err;
}

static int
wl_ampdu_rr_retry_limit_tid(void *wl, cmd_t *cmd, char **argv)
{
	char *param;
	const char *cmdname = "ampdu_rr_retry_limit_tid";
	struct ampdu_retry_tid retry_limit, *reply;
	uint8 tid;
	int err;
	void *ptr = NULL;

	UNUSED_PARAMETER(cmd);

	if ((param = *++argv) == NULL)
		return USAGE_ERROR;

	tid = atoi(param);
	if (tid > MAXPRIO)
		return USAGE_ERROR;
	retry_limit.tid = tid;

	if ((param = *++argv)) {
		retry_limit.retry = atoi(param);
		err = wlu_var_setbuf(wl, cmdname, &retry_limit, sizeof(retry_limit));
	} else {
		if ((err = wlu_var_getbuf(wl, cmdname, &retry_limit,
			sizeof(retry_limit), &ptr)) < 0)
			return err;
		reply = (struct ampdu_retry_tid *)ptr;
		printf("AMPDU regular rate retry limit for tid %d: %d\n", tid, reply->retry);
	}
	return err;
}


static int
wl_ampdu_send_addba(void *wl, cmd_t *cmd, char **argv)
{
	char *param;
	const char *cmdname = "ampdu_send_addba";
	struct ampdu_ea_tid aet;
	uint8 tid;

	UNUSED_PARAMETER(cmd);

	if ((param = *++argv) == NULL)
		return USAGE_ERROR;

	tid = atoi(param);
	if (tid > MAXPRIO)
		return USAGE_ERROR;
	aet.tid = tid;

	argv++;
	if (!*argv) {
		printf("error: missing address\n");
		return USAGE_ERROR;
	}

	if (!wl_ether_atoe(*argv, &aet.ea)) {
		printf("error: could not parse MAC address %s\n", *argv);
		return -1;
	}

	return wlu_var_setbuf(wl, cmdname, &aet, sizeof(aet));
}

static int
wl_ampdu_send_delba(void *wl, cmd_t *cmd, char **argv)
{
	char *param;
	const char *cmdname = "ampdu_send_delba";
	struct ampdu_ea_tid aet;
	uint8 tid;

	UNUSED_PARAMETER(cmd);

	if ((param = *++argv) == NULL)
		return USAGE_ERROR;

	tid = atoi(param);
	if (tid > MAXPRIO)
		return USAGE_ERROR;
	aet.tid = tid;

	argv++;
	if (!*argv) {
		printf("error: missing address\n");
		return USAGE_ERROR;
	}

	if (!wl_ether_atoe(*argv, &aet.ea)) {
		printf("error: could not parse MAC address %s\n", *argv);
		return -1;
	}

	return wlu_var_setbuf(wl, cmdname, &aet, sizeof(aet));
}

static int
wl_dpt_deny(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "dpt_deny";
	dpt_iovar_t info;

	UNUSED_PARAMETER(cmd);

	if (!*++argv)
		return USAGE_ERROR;

	info.pad = 0;

	if (!strcmp("add", *argv))
		info.mode = DPT_DENY_LIST_ADD;
	else if (!strcmp("remove", *argv))
		info.mode = DPT_DENY_LIST_REMOVE;
	else {
		printf("error: invalid mode string\n");
		return USAGE_ERROR;
	}

	argv++;
	if (!*argv) {
		printf("error: missing mode value\n");
		return USAGE_ERROR;
	}

	if (!wl_ether_atoe(*argv, &info.ea)) {
		printf("error: could not parse MAC address %s\n", *argv);
		return -1;
	}

	return wlu_var_setbuf(wl, cmdname, &info, sizeof(info));
}

static int
wl_dpt_endpoint(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "dpt_endpoint";
	dpt_iovar_t info;

	UNUSED_PARAMETER(cmd);

	if (!*++argv)
		return USAGE_ERROR;

	info.pad = 0;

	if (!strcmp("create", *argv))
		info.mode = DPT_MANUAL_EP_CREATE;
	else if (!strcmp("modify", *argv))
		info.mode = DPT_MANUAL_EP_MODIFY;
	else if (!strcmp("delete", *argv))
		info.mode = DPT_MANUAL_EP_DELETE;
	else {
		printf("error: invalid mode string\n");
		return USAGE_ERROR;
	}

	argv++;
	if (!*argv) {
		printf("error: missing ea\n");
		return USAGE_ERROR;
	}

	if (!wl_ether_atoe(*argv, &info.ea)) {
		printf("error: could not parse MAC address %s\n", *argv);
		return -1;
	}

	return wlu_var_setbuf(wl, cmdname, &info, sizeof(info));
}

#ifdef WLTDLS
static int
wl_tdls_endpoint(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname_tdls = "tdls_endpoint";
	tdls_iovar_t info;
	chanspec_t chanspec;

	if (strcmp(cmd->name, cmdname_tdls)) {
		printf("error: invalid command name.\n");
		return USAGE_ERROR;
	}

	if (!*++argv)
		return USAGE_ERROR;

	memset(&info, 0, sizeof(tdls_iovar_t));

	if (!strcmp("create", *argv))
		info.mode = TDLS_MANUAL_EP_CREATE;
	else if (!strcmp("modify", *argv))
		info.mode = TDLS_MANUAL_EP_MODIFY;
	else if (!strcmp("delete", *argv))
		info.mode = TDLS_MANUAL_EP_DELETE;
	else if (!strcmp("PM", *argv))
		info.mode = TDLS_MANUAL_EP_PM;
	else if (!strcmp("wake", *argv))
		info.mode = TDLS_MANUAL_EP_WAKE;
	else if (!strcmp("disc", *argv))
		info.mode = TDLS_MANUAL_EP_DISCOVERY;
	else if (!strcmp("cw", *argv)) {
		info.mode = TDLS_MANUAL_EP_CHSW;
	}
	else if (!strcmp("wfd_disc", *argv))
		info.mode = TDLS_MANUAL_EP_WFD_TPQ;
	else {
		printf("error: invalid mode string\n");
		return USAGE_ERROR;
	}

	argv++;
	if (!*argv) {
		printf("error: missing ea\n");
		return USAGE_ERROR;
	}

	if (!wl_ether_atoe(*argv, &info.ea)) {
		printf("error: could not parse MAC address %s\n", *argv);
		return -1;
	}

	if (info.mode == TDLS_MANUAL_EP_CHSW) {
		argv++;
		if (!*argv) {
			printf("error: missing target channel number\n");
			return USAGE_ERROR;
		}
		if (atoi(*argv) != 0) {
			chanspec = wf_chspec_aton(*argv);
			if (chanspec == 0) {
				printf("error: bad chanspec \"%s\".\n", *argv);
				return USAGE_ERROR;
			}
			chanspec = wl_chspec_to_driver(chanspec);
			if (chanspec == INVCHANSPEC) {
				return USAGE_ERROR;
			}
			info.chanspec = chanspec;
		}
	}

	return wlu_var_setbuf(wl, cmd->name, &info, sizeof(info));
}

static int
wl_pattern_atoh(char *, char *);

#define WFD_DEV			0
#define WFD_DEV_LEN		6
#define WFD_IP				8
#define WFD_IP_LEN		5
#define WFD_ALT_MAC		10
#define WFD_ALT_MAC_LEN	6

static int
wl_tdls_wfd_ie(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname_tdls = "tdls_wfd_ie";
	tdls_wfd_ie_iovar_t info;
	tdls_wfd_ie_iovar_t* buf_info = (tdls_wfd_ie_iovar_t*) buf;
	int ret;
	uint8* ptr;
	uint8 element, subelement;
	uint16 offset;
	uint8 buffer[TDLS_WFD_IE_SIZE - (WFA_OUI_LEN + 3)];
	uint16 length, element_length, current_length;
	bcm_tlv_t * ie;
	unsigned long value;
	struct ether_addr ea;
	struct ipv4_addr ipa_set;

	if (strcmp(cmd->name, cmdname_tdls)) {
		printf("error: invalid command name.\n");
		return USAGE_ERROR;
	}

	if (!*++argv)
		return USAGE_ERROR;

	if (!strcmp(*argv, "clr")) {
		memset(&info, 0, sizeof(tdls_wfd_ie_iovar_t));

		if (!*++argv)
			return USAGE_ERROR;

		if (!strcmp("own", *argv))
			info.mode = TDLS_WFD_IE_TX;
		else {
			printf("error: invalid mode string\n");
			return USAGE_ERROR;
		}

		return wlu_var_setbuf(wl, cmd->name, &info, sizeof(info));

	} else if (!strcmp(*argv, "get")) {
		memset(buf_info, 0, sizeof(*buf_info));

		if (!*++argv)
			return USAGE_ERROR;

		if (!strcmp("own", *argv))
			buf_info->mode = TDLS_WFD_IE_TX;
		else if (wl_ether_atoe(*argv, &buf_info->ea))
			buf_info->mode = TDLS_WFD_IE_RX;
		else {
			printf("error: invalid mode string\n");
			return USAGE_ERROR;
		}

		if ((ret = wlu_iovar_getbuf(wl, cmd->name, buf_info,
			sizeof(*buf_info), buf, WLC_IOCTL_MAXLEN)) < 0) {
			return ret;
		}

		/* empty */
		if (!buf_info->length) return ret;

		if (!*++argv) wl_hexdump((uchar *)buf_info->data, buf_info->length);

		else {
			if (!strcmp("ip", *argv)) {
				element = WFD_IP;
				element_length = WFD_IP_LEN;
			} else if (!strcmp("port", *argv)) {
				element = WFD_DEV;
				element_length = WFD_DEV_LEN;
			} else {
				printf("error: unknown element\n");
				return USAGE_ERROR;
			}

			/* Reassemble the WFD IE (without header) */

			ptr = buf_info->data;
			length = buf_info->length;
			offset = 0;
			current_length = 0;

			while (length - offset > WFA_OUI_LEN + 3) {
				if ((ie = bcm_parse_tlvs(ptr + offset,
					length - offset, DOT11_MNG_VS_ID)) != NULL) {
					if (ie->len > WFA_OUI_LEN + 1) {
						if ((!memcmp(ie->data, WFA_OUI, WFA_OUI_LEN)) &&
						    (*(ie->data + WFA_OUI_LEN) ==
						    WFA_OUI_TYPE_WFD)) {
							/* WFD */
							memcpy(buffer + current_length,
								ie->data + WFA_OUI_LEN + 1,
								ie->len - WFA_OUI_LEN - 1);
							current_length += ie->len - WFA_OUI_LEN - 1;
						}
					}
					offset = (uint16)((uint8*)ie - ptr + ie->len + 2);
				}
				else break;
			}

			/* Find the elements */

			ptr = buffer;
			length = current_length;

			while (length > 3) {
				current_length = (ptr[1] << 8) + ptr[2];
				if ((ptr[0] == element) && (current_length == element_length) &&
				    (current_length <= length - 3)) {

					switch (element) {
					case WFD_IP:
	/* we do not care about the IP version i.e. ptr[3] */
	printf("%u.%u.%u.%u\n", ptr[4], ptr[5], ptr[6], ptr[7]);
						break;

					case WFD_DEV:
						/* just get the RTSP TCP valid port */
						printf("%u\n", (ptr[5] << 8) + ptr[6]);
						break;
					}
					break;

				} else {
					if (current_length + 3 < length) {
						length -= current_length + 3;
						ptr += current_length + 3;
					} else break;
				}
			}
		}

		return ret;

	} else if (!strcmp(*argv, "set")) {
		memset(&info, 0, sizeof(tdls_wfd_ie_iovar_t));

		if (!*++argv)
			return USAGE_ERROR;

		if (!strcmp("own", *argv))
			info.mode = TDLS_WFD_IE_TX;
		else {
			printf("error: invalid mode string\n");
			return USAGE_ERROR;
		}

		argv++;
		if (!*argv) {
			printf("error: missing IE string\n");
			return USAGE_ERROR;
		}

		if (strlen((char*)*argv) - 2 > sizeof(info.data) * 2) {
			printf("error: IE string too long; max is %u bytes\n",
			    (unsigned int)sizeof(info.data));
			return USAGE_ERROR;
		}

		ret = wl_pattern_atoh(*argv, (char*)info.data);

		if (ret <= 0) {
			printf("error: could not parse IE string address %s\n", *argv);
			return USAGE_ERROR;
		}

		info.length = ret;

		if (*++argv) {
		/* IP specified */

			/* watchdog */
			if (info.length != 32) {
				printf(
				"if one or several set fields are used, "
				"the following the IE string must be\n"
				"exactly 32 bytes and must have the following order:\n"
				"\t6-byte header (0xDD1E506F9A0A)\n"
				"\t9-byte subelement 0 (WFD device information)\n"
				"\t9-byte subelement 1 (BSSID)\n"
				"\t8-byte subelement 8 (IP address)\n");
				return USAGE_ERROR;
			}

			if (!wl_atoip(*argv, &ipa_set))
			return USAGE_ERROR;

			memcpy(&info.data[28], (uint8*) &ipa_set, sizeof(ipa_set));

			if (*++argv) {
			/* port specified */

				value = strtoul(*argv, NULL, 0);
				info.data[11] = (uint8) (0xFF & (value >> 8));
				info.data[12] = (uint8) (0xFF & value);

				if (*++argv) {
					/* WFD type (Source or Primary Sink) specified */

					element = (uint8) (0x01 & strtoul(*argv, NULL, 0));
					if (element) info.data[10] |= 0x01;
					else info.data[10] &= ~0x01;

					if (*++argv) {
					/* BSSID specified */

						if (!wl_ether_atoe(*argv, &ea))
							return USAGE_ERROR;

						memcpy(&info.data[18], (uint8*) &ea, sizeof(ea));
					}
				}
			}
		}

		return wlu_var_setbuf(wl, cmd->name, &info, sizeof(info));

	} else if (!strcmp(*argv, "clr2")) {
		memset(&info, 0, sizeof(tdls_wfd_ie_iovar_t));

		if (!*++argv)
			return USAGE_ERROR;

		if (!strcmp("own", *argv))
			info.mode = TDLS_WFD_PROBE_IE_TX;
		else {
			printf("error: invalid mode string\n");
			return USAGE_ERROR;
		}

		return wlu_var_setbuf(wl, cmd->name, &info, sizeof(info));

	} else if (!strcmp(*argv, "get2")) {
		memset(buf_info, 0, sizeof(*buf_info));

		if (!*++argv)
			return USAGE_ERROR;

		if (!strcmp("own", *argv))
			buf_info->mode = TDLS_WFD_PROBE_IE_TX;
		else if (wl_ether_atoe(*argv, &buf_info->ea))
			buf_info->mode = TDLS_WFD_PROBE_IE_RX;
		else {
			printf("error: invalid mode string\n");
			return USAGE_ERROR;
		}

		if ((ret = wlu_iovar_getbuf(wl, cmd->name, buf_info,
			sizeof(*buf_info), buf, WLC_IOCTL_MAXLEN)) < 0) {
			return ret;
		}

		/* empty */
		if (!buf_info->length) return ret;

		if (!*++argv) wl_hexdump((uchar *)buf_info->data, buf_info->length);

		else {
			if (!strcmp("alt_mac", *argv)) {
				element = WFD_ALT_MAC;
				element_length = WFD_ALT_MAC_LEN;
			} else if (!strcmp("port", *argv)) {
				element = WFD_DEV;
				element_length = WFD_DEV_LEN;
				subelement = 1;
			} else if (!strcmp("PC_bit", *argv)) {
				element = WFD_DEV;
				element_length = WFD_DEV_LEN;
				subelement = 0;
			} else {
				printf("error: unknown element\n");
				return USAGE_ERROR;
			}

			/* Reassemble the WFD IE (without header) */

			ptr = buf_info->data;
			length = buf_info->length;
			offset = 0;
			current_length = 0;

			while (length - offset > WFA_OUI_LEN + 3) {
				if ((ie = bcm_parse_tlvs(ptr + offset,
					length - offset, DOT11_MNG_VS_ID)) != NULL) {
					if (ie->len > WFA_OUI_LEN + 1) {
						if ((!memcmp(ie->data, WFA_OUI, WFA_OUI_LEN)) &&
							(*(ie->data + WFA_OUI_LEN) ==
							WFA_OUI_TYPE_WFD)) {
							/* WFD */
							memcpy(buffer + current_length,
								ie->data + WFA_OUI_LEN + 1,
								ie->len - WFA_OUI_LEN - 1);
							current_length += ie->len - WFA_OUI_LEN - 1;
						}
					}
					offset = (uint16)((uint8*)ie - ptr + ie->len + 2);
				}
				else break;
			}

			/* Find the elements */

			ptr = buffer;
			length = current_length;

			while (length > 3) {
				current_length = (ptr[1] << 8) + ptr[2];
				if ((ptr[0] == element) && (current_length == element_length) &&
					(current_length <= length - 3)) {

					switch (element) {
	case WFD_ALT_MAC:
		printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
			ptr[3], ptr[4], ptr[5], ptr[6], ptr[7], ptr[8]);
		break;
	case WFD_DEV:
			if (subelement)
				/* just get the RTSP TCP valid port */
				printf("%u\n", (ptr[5] << 8) + ptr[6]);
			else
				/* just get the Preferred Connection bit */
				printf("%u\n", ptr[4] >> 7);
		break;
					}
					break;

				} else {
					if (current_length + 3 < length) {
						length -= current_length + 3;
						ptr += current_length + 3;
					} else break;
				}
			}
		}

		return ret;

	} else if (!strcmp(*argv, "set2")) {
		memset(&info, 0, sizeof(tdls_wfd_ie_iovar_t));

		if (!*++argv)
			return USAGE_ERROR;

		if (!strcmp("own", *argv))
			info.mode = TDLS_WFD_PROBE_IE_TX;
		else {
			printf("error: invalid mode string\n");
			return USAGE_ERROR;
		}

		argv++;
		if (!*argv) {
			printf("error: missing IE string\n");
			return USAGE_ERROR;
		}

		if (strlen((char*)*argv) - 2 > sizeof(info.data) * 2) {
			printf("error: IE string too long; max is %u bytes\n",
				(unsigned int)sizeof(info.data));
			return USAGE_ERROR;
		}

		ret = wl_pattern_atoh(*argv, (char*)info.data);

		if (ret <= 0) {
			printf("error: could not parse IE string address %s\n", *argv);
			return USAGE_ERROR;
		}

		info.length = ret;

		if (*++argv) {
		/* alt MAC specified */

			/* watchdog */
			if (info.length != 24) {
				printf(
				"if one or several set2 fields are used, "
				"the following the IE string must be\n"
				"exactly 24 bytes and must have the following order:\n"
				"\t6-byte header (0xDD16506F9A0A)\n"
				"\t9-byte subelement 0  (WFD device information)\n"
				"\t9-byte subelement 10 (alternate MAC address)\n");
				return USAGE_ERROR;
			}

			if (!wl_ether_atoe(*argv, &ea)) return USAGE_ERROR;
			memcpy(&info.data[18], (uint8*) &ea, sizeof(ea));

			if (*++argv) {
			/* port specified */

				value = strtoul(*argv, NULL, 0);
				info.data[11] = (uint8) (0xFF & (value >> 8));
				info.data[12] = (uint8) (0xFF & value);

				if (*++argv) {
					/* WFD type (Source or Primary Sink) specified */

					element = (uint8) (0x01 & strtoul(*argv, NULL, 0));
					if (element) info.data[10] |= 0x01;
					else info.data[10] &= ~0x01;
				}
			}
		}

		return wlu_var_setbuf(wl, cmd->name, &info, sizeof(info));

	} else {
		printf("error: unknown operation\n");
		return USAGE_ERROR;
	}
}
#endif /* WLTDLS */

static int
wl_actframe(void *wl, cmd_t *cmd, char **argv)
{
	wl_action_frame_t * action_frame;
	wl_af_params_t * af_params;
	struct ether_addr ea;
	int argc;
	int err = 0;

	UNUSED_PARAMETER(cmd);

	if (!argv[1] || !argv[2]) {
		fprintf(stderr, "Too few arguments\n");
		return -1;
	}

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if ((af_params = (wl_af_params_t *) malloc(WL_WIFI_AF_PARAMS_SIZE)) == NULL) {
		printf("wl_actframe: unable to allocate frame \n");
		return -1;
	}
	af_params->channel = 0;
	af_params->dwell_time = -1;
	action_frame = &af_params->action_frame;

	/* Add the packet Id */
	action_frame->packetId = (uint32)(uintptr)action_frame;

	/* convert the ea string into an ea struct */
	if (!wl_ether_atoe(argv[1], &ea)) {
		free(action_frame);
		printf(" ERROR: no valid ether addr provided\n");
		return -1;
	}
	memcpy(&action_frame->da, (char*)&ea, ETHER_ADDR_LEN);
	/* set default BSSID */
	memcpy(&af_params->BSSID, (char*)&ea, ETHER_ADDR_LEN);

	/* add the length */
	if (argv[2]) {
		action_frame->len  = htod16(strlen(argv[2])) / 2;
	}

	/* add the channel */
	if (argc > 3 && argv[3]) {
		af_params->channel = htod32(atoi(argv[3]));
	}

	/* add the dwell_time */
	if (argc > 4 && argv[4]) {
		af_params->dwell_time = htod32(atoi(argv[4]));
	}

	/* add the BSSID */
	if (argc > 5 && argv[5]) {
		if (!wl_ether_atoe(argv[5], &ea)) {
			free(action_frame);
			printf(" ERROR: no valid ether addr provided\n");
			return -1;
		}
		memcpy(&af_params->BSSID, (char*)&ea, ETHER_ADDR_LEN);
	}

	if ((err = get_ie_data ((uchar *)argv[2],
		&action_frame->data[0],
		action_frame->len))) {
		free(af_params);
		fprintf(stderr, "Error parsing data arg\n");
		return err;
	}
	err = wlu_var_setbuf(wl, "actframe", af_params, WL_WIFI_AF_PARAMS_SIZE);

	free(af_params);

	return (err);

}

static int
wl_dpt_pmk(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "dpt_pmk";
	wsec_pmk_t psk;
	size_t key_len;

	UNUSED_PARAMETER(cmd);

	if (!*++argv)
		return USAGE_ERROR;

	key_len = strlen(*argv);
	if (key_len < WSEC_MIN_PSK_LEN || key_len > WSEC_MAX_PSK_LEN) {
		fprintf(stderr, "passphrase must be between %d and %d characters long\n",
		       WSEC_MIN_PSK_LEN, WSEC_MAX_PSK_LEN);
		return -1;
	}
	psk.key_len = htod16((ushort) key_len);
	psk.flags = htod16(WSEC_PASSPHRASE);
	memcpy(psk.key, *argv, key_len);

	return wlu_var_setbuf(wl, cmdname, &psk, sizeof(wsec_pmk_t));
}

static int
wl_dpt_fname(void *wl, cmd_t *cmd, char **argv)
{
	char *param;
	const char *cmdname = "dpt_fname";
	int err;
	dpt_fname_t fname, *reply;
	void *ptr = NULL;

	UNUSED_PARAMETER(cmd);

	if ((param = *++argv)) {
		fname.len = strlen(param);
		if (fname.len >= (DPT_FNAME_LEN - 1)) {
			fprintf(stderr, "Name must be less than 32 characters\n");
			return -1;
		}
		memcpy(fname.name, param, fname.len);
		fname.name[fname.len] = '\0';
		err = wlu_var_setbuf(wl, cmdname, &fname, sizeof(fname));
	} else {
		if ((err = wlu_var_getbuf(wl, cmdname, &fname, sizeof(fname), &ptr) < 0))
			return err;
		reply = (dpt_fname_t *)ptr;
		printf("%s\n", reply->name);
	}
	return err;
}

static int
wl_dpt_list(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	uint32 i;
	dpt_list_t *list;

	if (*++argv)
		return USAGE_ERROR;

	strcpy(buf, cmd->name);
	if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return err;

	list = (dpt_list_t *) buf;

	printf("List of DPT connections:\n");
	for (i = 0; i < list->num; i++) {
		printf("%s: status 0x%x rx %d tx %d rssi %d\n",
			list->status[i].name,
			list->status[i].status,
			list->status[i].sta.rx_ucast_pkts,
			list->status[i].sta.tx_pkts,
			list->status[i].rssi);
	}
	return err;
}


/*
 *  RADAR detection parameter control
 */
static int
wl_radar_args(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	wl_radar_args_t ra;

	/* Skip the command name */
	argv++;

	if (*argv == NULL) {
		/* Get */

		if ((ret = wlu_iovar_get(wl, cmd->name, &ra, sizeof(ra))) < 0)
			return ret;

		if (ra.version != WL_RADAR_ARGS_VERSION) {
			printf("\tIncorrect version of RADAR_ARGS struct: expected %d; got %d\n",
			       WL_RADAR_ARGS_VERSION, ra.version);
			return -1;
		}
		printf("version %d npulses %d ncontig %d min_pw %d max_pw %d thresh0 0x%x "
		       "thresh1 0x%x\n",
		       ra.version, ra.npulses, ra.ncontig, ra.min_pw,
		       ra.max_pw, ra.thresh0, ra.thresh1);
		printf("blank 0x%x fmdemodcfg 0x%x npulses_lp %d min_pw_lp %d "
		       "max_pw_lp %d\n",
		       ra.blank, ra.fmdemodcfg, ra.npulses_lp, ra.min_pw_lp,
		       ra.max_pw_lp);
		printf("min_fm_lp %d max_span_lp %d min_deltat %d max_deltat %d\n",
		       ra.min_fm_lp, ra.max_span_lp, ra.min_deltat, ra.max_deltat);

		printf("autocorr 0x%x st_level_time 0x%x  t2_min %d fra_pulse_err %d\n",
		       ra.autocorr, ra.st_level_time, ra.t2_min, ra.fra_pulse_err);
		printf("npulses_fra %d npulses_stg2 %d npulses_stg3 %d percal_mask 0x%x quant %d\n",
			ra.npulses_fra, ra.npulses_stg2, ra.npulses_stg3, ra.percal_mask,
			ra.quant);
		printf("min_burst_intv_lp %d max_burst_intv_lp %d nskip_rst_lp %d max_pw_tol %d "
				"feature_mask 0x%x\n",
				ra.min_burst_intv_lp, ra.max_burst_intv_lp, ra.nskip_rst_lp,
				ra.max_pw_tol, ra.feature_mask);
		/* this part prints only param values */
		printf("%d %d %d %d %d 0x%x "
		       "0x%x",
		       ra.version, ra.npulses, ra.ncontig, ra.min_pw,
		       ra.max_pw, ra.thresh0, ra.thresh1);
		printf(" 0x%x 0x%x %d %d "
		       "%d",
		       ra.blank, ra.fmdemodcfg, ra.npulses_lp, ra.min_pw_lp,
		       ra.max_pw_lp);
		printf(" %d %d %d %d",
		       ra.min_fm_lp, ra.max_span_lp, ra.min_deltat, ra.max_deltat);

		printf(" 0x%x 0x%x %d %d",
		       ra.autocorr, ra.st_level_time, ra.t2_min, ra.fra_pulse_err);
		printf(" %d %d %d 0x%x %d",
			ra.npulses_fra, ra.npulses_stg2, ra.npulses_stg3, ra.percal_mask,
			ra.quant);
		printf(" %d %d %d %d "
				"0x%x\n",
				ra.min_burst_intv_lp, ra.max_burst_intv_lp, ra.nskip_rst_lp,
				ra.max_pw_tol, ra.feature_mask);

	} else {
		/* Set */
		char *endptr = NULL;
		int val_count = 30;
		long vals[30];
		long *pval;
		int i;

		for (i = 0; i < val_count; i++, argv++) {
			/* verify that there is another arg */
			if (*argv == NULL)
				return -1;

			vals[i] = strtol(*argv, &endptr, 0);

			/* make sure all the value string was parsed by strtol */
			if (*endptr != '\0')
				return -1;
		}

		pval = vals;

		ra.version       = *pval++;
		ra.npulses       = *pval++;
		ra.ncontig       = *pval++;
		ra.min_pw        = *pval++;
		ra.max_pw        = *pval++;
		ra.thresh0       = (uint16)*pval++;
		ra.thresh1       = (uint16)*pval++;
		ra.blank         = (uint16)*pval++;
		ra.fmdemodcfg    = (uint16)*pval++;
		ra.npulses_lp    = *pval++;
		ra.min_pw_lp     = *pval++;
		ra.max_pw_lp     = *pval++;
		ra.min_fm_lp     = *pval++;
		ra.max_span_lp   = *pval++;
		ra.min_deltat    = *pval++;
		ra.max_deltat    = *pval++;
		ra.autocorr      = (uint16)*pval++;
		ra.st_level_time = (uint16)*pval++;
		ra.t2_min        = (uint16)*pval++;
		ra.fra_pulse_err = (uint32)*pval++;
		ra.npulses_fra   = (int)*pval++;
		ra.npulses_stg2  = (int)*pval++;
		ra.npulses_stg3  = (int)*pval++;
		ra.percal_mask   = (int)*pval++;
		ra.quant         = (int)*pval++;
		ra.min_burst_intv_lp = (uint32)*pval++;
		ra.max_burst_intv_lp = (uint32)*pval++;
		ra.nskip_rst_lp  = (int)*pval++;
		ra.max_pw_tol    = (int)*pval++;
		ra.feature_mask  = (uint16)*pval++;

		return wlu_var_setbuf(wl, cmd->name, &ra, sizeof(wl_radar_args_t));
	}
	return ret;
}

static int
wl_radar_thrs(void *wl, cmd_t *cmd, char **argv)
{
	int ret = -1;
	wl_radar_thr_t radar_thrs;

	if (*++argv) {
		/* Set */
		char *endptr;
		int val_count = 12;
		uint16 vals[12];
		uint16 *pval;
		int i;

		for (i = 0; i < val_count; i++, argv++) {
			/* verify that there is another arg */
			if (*argv == NULL)
				return -1;

			vals[i] = (uint16)strtol(*argv, &endptr, 0);

			/* make sure all the value string was parsed by strtol */
			if (*endptr != '\0')
				return -1;
		}

		radar_thrs.version = WL_RADAR_THR_VERSION;

		/* Order thresh0_20_lo, thresh1_20_lo, thresh0_40_lo, thresh1_40_lo
		 * thresh0_20_hi, thresh1_20_hi, thresh0_40_hi, thresh1_40_hi
		 */
		pval = vals;
		radar_thrs.thresh0_20_lo = (uint16)*pval++;
		radar_thrs.thresh1_20_lo = (uint16)*pval++;
		radar_thrs.thresh0_40_lo = (uint16)*pval++;
		radar_thrs.thresh1_40_lo = (uint16)*pval++;
		radar_thrs.thresh0_80_lo = (uint16)*pval++;
		radar_thrs.thresh1_80_lo = (uint16)*pval++;
		radar_thrs.thresh0_20_hi = (uint16)*pval++;
		radar_thrs.thresh1_20_hi = (uint16)*pval++;
		radar_thrs.thresh0_40_hi = (uint16)*pval++;
		radar_thrs.thresh1_40_hi = (uint16)*pval++;
		radar_thrs.thresh0_80_hi = (uint16)*pval++;
		radar_thrs.thresh1_80_hi = (uint16)*pval++;
		return wlu_var_setbuf(wl, cmd->name, &radar_thrs, sizeof(wl_radar_thr_t));
	}
	return ret;
}

static int
wl_dfs_status(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	wl_dfs_status_t *dfs_status_ptr;
	char chanspec_str[CHANSPEC_STR_LEN];

	const char *dfs_cacstate_str[WL_DFS_CACSTATES] = {
		"IDLE",
		"PRE-ISM Channel Availability Check(CAC)",
		"In-Service Monitoring(ISM)",
		"Channel Switching Announcement(CSA)",
		"POST-ISM Channel Availability Check",
		"PRE-ISM Ouf Of Channels(OOC)",
		"POST-ISM Out Of Channels(OOC)"
	};

	void *ptr;

	UNUSED_PARAMETER(argv);

	if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return ret;

	dfs_status_ptr = (wl_dfs_status_t *)ptr;

	dfs_status_ptr->state = dtoh32(dfs_status_ptr->state);
	dfs_status_ptr->duration = dtoh32(dfs_status_ptr->duration);
	dfs_status_ptr->chanspec_cleared = wl_chspec_from_driver(dfs_status_ptr->chanspec_cleared);

	if (dfs_status_ptr->state >= WL_DFS_CACSTATES) {
		printf("Unknown dfs state %d.\n", dfs_status_ptr->state);
		return -1;
	}

	printf("state %s time elapsed %dms radar channel cleared by dfs ",
		dfs_cacstate_str[dfs_status_ptr->state], dfs_status_ptr->duration);

	if (dfs_status_ptr->chanspec_cleared) {
		printf("channel %s (0x%04X)\n",
		       wf_chspec_ntoa(dfs_status_ptr->chanspec_cleared, chanspec_str),
		       dfs_status_ptr->chanspec_cleared);
	}
	else {
		printf("none\n");
	}

	return ret;
}

static int
wl_wds_wpa_role_old(void *wl, cmd_t *cmd, char **argv)
{
	uint remote[2];
	uint *sup = remote;

	UNUSED_PARAMETER(argv);

	if (wlu_get(wl, WLC_WDS_GET_REMOTE_HWADDR, remote, sizeof(remote)) < 0) {
		printf("Unable to get remote endpoint's hwaddr\n");
		return -1;
	}
	if (wlu_get(wl, cmd->get, remote, sizeof(remote)) < 0) {
		printf("Unable to get local endpoint's WPA role\n");
		return -1;
	}
	printf("Local endpoing's WPA role: %s\n", dtoh32(*sup) ? "supplicant" : "authenticator");
	return 0;
}


/*
 * wlu_reg2args is a generic function that is used for setting/getting
 * WL_IOVAR variables that require address for read, and
 * address + data for write.
 */
static int
wlu_reg2args(void *wl, cmd_t *cmd, char **argv)
{
	char var[256];
	uint32 int_val;
	bool get = TRUE;
	uint32 len;
	void *ptr = NULL;
	char *endptr;

	if (argv[1]) {
		len = sizeof(int_val);
		int_val = htod32(strtoul(argv[1], &endptr, 0));
		memcpy(var, (char *)&int_val, sizeof(int_val));
	}
	else
		return -1;

	if (argv[2]) {
		get = FALSE;
		int_val = htod32(strtoul(argv[2], &endptr, 0));
		memcpy(&var[len], (char *)&int_val, sizeof(int_val));
		len += sizeof(int_val);
	}
	if (get) {
		if (wlu_var_getbuf(wl, cmd->name, var, sizeof(var), &ptr) < 0)
			return -1;

		printf("0x%x\n", dtoh32(*(int *)ptr));
	}
	else
		wlu_var_setbuf(wl, cmd->name, &var, sizeof(var));
	return 0;
}

/*
 * wlu_reg3args is a generic function that is used for setting/getting
 * WL_IOVAR variables that require address + offset for read, and
 * address + offset + data for write.
 */
static int
wlu_reg3args(void *wl, cmd_t *cmd, char **argv)
{
	char var[256];
	uint32 int_val;
	bool get = TRUE;
	uint32 len, i;
	void *ptr = NULL;
	char *endptr;
	uint numargs;

	len = 0;

	if (!argv[1] || !argv[2]) {
		printf("Wrong syntax => dev offset [val]\n");
		return -1;
	}

	if (argv[3]) {
		numargs = 3;
		get = FALSE;
	} else
		numargs = 2;

	for (i = 1; i <= numargs; i++) {
		int_val = htod32(strtoul(argv[i], &endptr, 0));
		memcpy(&var[len], (char *)&int_val, sizeof(int_val));
		len += sizeof(int_val);
	}

	if (get) {
		if (wlu_var_getbuf(wl, cmd->name, var, sizeof(var), &ptr) < 0)
			return -1;

		printf("0x%x\n", dtoh32(*(int *)ptr));
	}
	else
		wlu_var_setbuf(wl, cmd->name, &var, sizeof(var));
	return 0;
}


static int
wl_tpc_lm(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	uint16 val;
	int8 aplm, stalm;

	UNUSED_PARAMETER(argv);

	if ((ret = wlu_iovar_getint(wl, cmd->name, (int *)(uintptr)&val)) < 0)
		return ret;

	stalm = val & 0xff;
	aplm = (val >> 8) & 0xff;

	printf("TPC: APs link margin:%d\t STAs link margin:%d\n", aplm, stalm);

	return 0;
}


static int
wl_wds_wpa_role(void *wl, cmd_t *cmd, char **argv)
{
	char var[256];
	char *mac;
	char *sup;
	int len;
	if (strlen("wds_wpa_role") + 1 + ETHER_ADDR_LEN + 1 > sizeof(var))
		return -1;
	/* build var required by WLC_GET|SET_VAR */
	len = sprintf(var, "%s", "wds_wpa_role") + 1;
	mac = var + len;
	if (wlu_get(wl, WLC_WDS_GET_REMOTE_HWADDR, mac, ETHER_ADDR_LEN) < 0) {
		printf("Unable to get remote endpoint's hwaddr\n");
		return -1;
	}
	len += ETHER_ADDR_LEN + 1;
	if (argv[1]) {
		sup = mac + ETHER_ADDR_LEN;
		switch ((uchar)(*sup = atoi(argv[1]))) {
		case WL_WDS_WPA_ROLE_AUTH:
		case WL_WDS_WPA_ROLE_SUP:
		case WL_WDS_WPA_ROLE_AUTO:
			if (wlu_set(wl, cmd->set, var, len) < 0)
				printf("Unable to set local endpoint's WPA role\n");
			break;
		default:
			printf("Invalid WPA role %s. %u:authenticator, %u:supplicant, %u:auto\n",
				argv[1], WL_WDS_WPA_ROLE_AUTH,
				WL_WDS_WPA_ROLE_SUP, WL_WDS_WPA_ROLE_AUTO);
			break;
		}
	}
	else if (wlu_get(wl, cmd->get, var, len) < 0) {
		printf("Unable to get local endpoint's WPA role\n");
		return -1;
	}
	else {
		sup = var;
		printf("Local endpoint's WPA role: %s\n", *sup ? "supplicant" : "authenticator");
	}
	return 0;
}

static int
wl_measure_req(void *wl, cmd_t *cmd, char **argv)
{
	uint32 val;
	struct ether_addr ea;

	if (!*++argv) {
		printf("error: missing arguments\n");
		return -1;
	}

	if (!stricmp(*argv, "tpc"))
		val = WLC_MEASURE_TPC;
	else if (!stricmp(*argv, "basic"))
		val = WLC_MEASURE_CHANNEL_BASIC;
	else if (!stricmp(*argv, "cca"))
		val = WLC_MEASURE_CHANNEL_CCA;
	else if (!stricmp(*argv, "rpi"))
		val = WLC_MEASURE_CHANNEL_RPI;
	else {
		printf("error: unknown measurement type %s\n", *argv);
		return -1;
	}
	argv++;

	if (!*argv) {
		printf("error: missing target address\n");
		return -1;
	}

	if (!wl_ether_atoe(*argv, &ea)) {
		printf("error: could not parse MAC address %s\n", *argv);
		return -1;
	}

	val = htod32(val);
	memcpy(&buf[0], &val, sizeof(uint32));
	memcpy(&buf[4], ea.octet, ETHER_ADDR_LEN);

	return wlu_set(wl, cmd->set, buf, sizeof(uint32) + ETHER_ADDR_LEN);
}

static int
wl_send_quiet(void *wl, cmd_t *cmd, char **argv)
{
	dot11_quiet_t quiet;

	if (!*++argv) {
		printf("error: missing arguments\n");
		return -1;
	}
	/* Order is count, duration, offset */
	quiet.count = atoi(*argv);
	if (!*++argv) {
		printf("error: missing arguments\n");
		return -1;
	}
	quiet.duration = atoi(*argv);
	if (!*++argv) {
		printf("error: missing arguments\n");
		return -1;
	}
	quiet.offset = atoi(*argv);
	quiet.period = 0;

	quiet.duration = htod16(quiet.duration);
	quiet.offset = htod16(quiet.offset);
	return (wlu_set(wl, cmd->set, &quiet, sizeof(quiet)));
}

static int
wl_send_csa(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	wl_chan_switch_t csa_arg;

	/* Order is mode, count channel */
	if (!*++argv) {
		printf("error: missing arguments\n");
		return -1;
	}
	csa_arg.mode = atoi(*argv) ? 1 : 0;
	if (!*++argv) {
		printf("error: missing count\n");
		return -1;
	}
	csa_arg.count = atoi(*argv);
	if (!*++argv) {
		printf("error: missing channel\n");
		return -1;
	}
	csa_arg.reg = 0;

	if ((csa_arg.chspec = wf_chspec_aton(*argv))) {
		csa_arg.chspec = wl_chspec_to_driver(csa_arg.chspec);
		if (csa_arg.chspec == INVCHANSPEC) {
			return USAGE_ERROR;
		}
		/* csa action frame type */
		if (*++argv) {
			if (strcmp(*argv, "u") == 0)
				csa_arg.frame_type = CSA_UNICAST_ACTION_FRAME;
			else {
				printf("error: invalid frame type: %s\n", *argv);
				return USAGE_ERROR;
			}
		} else
			csa_arg.frame_type = CSA_BROADCAST_ACTION_FRAME;

		err = wlu_var_setbuf(wl, cmd->name, &csa_arg, sizeof(csa_arg));
	} else {
		printf("Error: bad parameters \"%s\"\n", *argv);
		return -1;
	}

	return err;
}
#endif /* !ATE_BUILD */

static int
wl_var_setint(void *wl, cmd_t *cmd, char **argv)
{
	int32 val;
	char *varname;
	char *endptr = NULL;

	UNUSED_PARAMETER(cmd);

	if (!*argv) {
		printf("set: missing arguments\n");
		return -1;
	}

	varname = *argv++;

	if (!*argv) {
		printf("set: missing value argument for set of \"%s\"\n", varname);
		return -1;
	}

	val = strtol(*argv, &endptr, 0);
	if (*endptr != '\0') {
		/* not all the value string was parsed by strtol */
		printf("set: error parsing value \"%s\" as an integer for set of \"%s\"\n",
			*argv, varname);
		return -1;
	}

	return wlu_iovar_setint(wl, varname, val);
}

#ifndef ATE_BUILD
static int
wl_var_get(void *wl, cmd_t *cmd, char **argv)
{
	char *varname;
	char *p;

	UNUSED_PARAMETER(cmd);

	if (!*argv) {
		printf("get: missing arguments\n");
		return -1;
	}

	varname = *argv++;

	if (*argv) {
		printf("get: error, extra arg \"%s\"\n", *argv);
		return -1;
	}

	strcpy(buf, varname);
	p = buf;
	while (*p != '\0') {
		*p = tolower((int)*p);
		p++;
	}
	return (wlu_get(wl, WLC_GET_VAR, &buf[0], WLC_IOCTL_MAXLEN));
}

static int
wl_var_getinthex(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int32 val;

	if ((err = wl_var_get(wl, cmd, argv)))
		return (err);

	val = dtoh32(*(int32*)buf);

	printf("0x%08x\n", val);

	return 0;
}
#endif /* !ATE_BUILD */
static int
wl_var_getint(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int32 val;
	char *varname;

	UNUSED_PARAMETER(cmd);

	if (!*argv) {
		printf("get: missing arguments\n");
		return -1;
	}

	varname = *argv++;

	if ((err = wlu_iovar_getint(wl, varname, &val)))
		return (err);

	if (val < 10)
		printf("%d\n", val);
	else
		printf("%d (0x%x)\n", val, val);

	return (0);
}

#ifndef ATE_BUILD
static int
wl_var_getandprintstr(void *wl, cmd_t *cmd, char **argv)
{
	int err;

	if ((err = wl_var_get(wl, cmd, argv)))
		return (err);

	printf("%s\n", buf);
	return (0);
}

/* Variation: Like getandprint, but allow an int arg to be passed */
static int
wl_var_setintandprintstr(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int32 val;
	char *varname;
	char *endptr = NULL;

	UNUSED_PARAMETER(cmd);

	if (!*argv) {
		printf("set: missing arguments\n");
		return -1;
	}

	varname = *argv++;

	if (!*argv) {
		val = 0;
	} else {
		val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0') {
			/* not all the value string was parsed by strtol */
			printf("set: error parsing value \"%s\" as an integer for set of \"%s\"\n",
			       *argv, varname);
			return -1;
		}
	}

	val = htod32(val);
	err = wlu_iovar_getbuf(wl, varname, &val, sizeof(int), buf, WLC_IOCTL_MAXLEN);

	if (err)
		return (err);

	printf("%s\n", buf);
	return (0);
}

void
wl_printlasterror(void *wl)
{
	char error_str[128];

	if (wlu_iovar_get(wl, "bcmerrorstr", error_str, sizeof(error_str)) != 0) {
		fprintf(stderr, "%s: \nError getting the last error\n", wlu_av0);
	} else {
		fprintf(stderr, "%s: %s\n", wlu_av0, error_str);
	}
}
#endif /* !ATE_BUILD */

/* just issue a wl_var_setint() or a wl_var_getint() if there is a 2nd arg */
int
wl_varint(void *wl, cmd_t *cmd, char *argv[])
{
	if (argv[1])
		return (wl_var_setint(wl, cmd, argv));
	else
		return (wl_var_getint(wl, cmd, argv));
}

int
wlu_var_getbuf(void *wl, const char *iovar, void *param, int param_len, void **bufptr)
{
	int len;

	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, iovar);

	/* include the null */
	len = strlen(iovar) + 1;

	if (param_len)
		memcpy(&buf[len], param, param_len);

	*bufptr = buf;

	return wlu_get(wl, WLC_GET_VAR, &buf[0], WLC_IOCTL_MAXLEN);
}

/* get buffer for smaller sizes upto 256 bytes */
int
wlu_var_getbuf_sm(void *wl, const char *iovar, void *param, int param_len, void **bufptr)
{
	int len;

	memset(buf, 0, WLC_IOCTL_SMLEN);
	strcpy(buf, iovar);

	/* include the null */
	len = strlen(iovar) + 1;

	if (param_len)
		memcpy(&buf[len], param, param_len);

	*bufptr = buf;

	return wlu_get(wl, WLC_GET_VAR, &buf[0], WLC_IOCTL_SMLEN);
}

/* Get buffer for medium sizes upto 1500 bytes */
int
wlu_var_getbuf_med(void *wl, const char *iovar, void *param, int param_len, void **bufptr)
{
	int len;

	memset(buf, 0, WLC_IOCTL_MEDLEN);
	strcpy(buf, iovar);

	/* include the null */
	len = strlen(iovar) + 1;

	if (param_len)
		memcpy(&buf[len], param, param_len);

	*bufptr = buf;

	return wlu_get(wl, WLC_GET_VAR, &buf[0], WLC_IOCTL_MEDLEN);
}


int
wlu_var_setbuf(void *wl, const char *iovar, void *param, int param_len)
{
	int len;

	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, iovar);

	/* include the null */
	len = strlen(iovar) + 1;

	if (param_len)
		memcpy(&buf[len], param, param_len);

	len += param_len;

	return wlu_set(wl, WLC_SET_VAR, &buf[0], len);
}

int
wlu_var_setbuf_sm(void *wl, const char *iovar, void *param, int param_len)
{
	int len;

	memset(buf, 0, WLC_IOCTL_SMLEN);
	strcpy(buf, iovar);

	/* include the null */
	len = strlen(iovar) + 1;

	if (param_len)
		memcpy(&buf[len], param, param_len);

	len += param_len;

	return wlu_set(wl, WLC_SET_VAR, &buf[0], WLC_IOCTL_SMLEN);
}

int
wlu_var_setbuf_med(void *wl, const char *iovar, void *param, int param_len)
{
	int len;

	memset(buf, 0, WLC_IOCTL_MEDLEN);
	strcpy(buf, iovar);

	/* include the null */
	len = strlen(iovar) + 1;

	if (param_len)
		memcpy(&buf[len], param, param_len);

	len += param_len;

	return wlu_set(wl, WLC_SET_VAR, &buf[0], WLC_IOCTL_MEDLEN);
}

#ifndef ATE_BUILD
static int
wl_var_void(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(argv);

	if (cmd->set < 0)
		return -1;

	return wlu_var_setbuf(wl, cmd->name, NULL, 0);
}

/*
 * format a bsscfg indexed iovar buffer
 */
static int
wl_bssiovar_mkbuf(const char *iovar, int bssidx, void *param,
	int paramlen, void *bufptr, int buflen, int *perr)
{
	const char *prefix = "bsscfg:";
	int8* p;
	uint prefixlen;
	uint namelen;
	uint iolen;

	prefixlen = strlen(prefix);	/* length of bsscfg prefix */
	namelen = strlen(iovar) + 1;	/* length of iovar name + null */
	iolen = prefixlen + namelen + sizeof(int) + paramlen;

	/* check for overflow */
	if (buflen < 0 || iolen > (uint)buflen) {
		*perr = BCME_BUFTOOSHORT;
		return 0;
	}

	p = (int8*)bufptr;

	/* copy prefix, no null */
	memcpy(p, prefix, prefixlen);
	p += prefixlen;

	/* copy iovar name including null */
	memcpy(p, iovar, namelen);
	p += namelen;

	/* bss config index as first param */
	bssidx = htod32(bssidx);
	memcpy(p, &bssidx, sizeof(int32));
	p += sizeof(int32);

	/* parameter buffer follows */
	if (paramlen)
		memcpy(p, param, paramlen);

	*perr = 0;
	return iolen;
}

/*
 * set named & bss indexed driver iovar providing both parameter and i/o buffers
 */
int
wlu_bssiovar_setbuf(void* wl, const char *iovar, int bssidx,
	void *param, int paramlen, void *bufptr, int buflen)
{
	int err;
	int iolen;

	iolen = wl_bssiovar_mkbuf(iovar, bssidx, param, paramlen, bufptr, buflen, &err);
	if (err)
		return err;

	return wlu_set(wl, WLC_SET_VAR, bufptr, iolen);
}

/*
 * get named & bss indexed driver iovar providing both parameter and i/o buffers
 */
static int
wl_bssiovar_getbuf(void* wl, const char *iovar, int bssidx,
	void *param, int paramlen, void *bufptr, int buflen)
{
	int err;

	wl_bssiovar_mkbuf(iovar, bssidx, param, paramlen, bufptr, buflen, &err);
	if (err)
		return err;

	return wlu_get(wl, WLC_GET_VAR, bufptr, buflen);
}

/*
 * get named & bss indexed driver variable to buffer value
 */
int
wlu_bssiovar_get(void *wl, const char *iovar, int bssidx, void *outbuf, int len)
{
	char smbuf[WLC_IOCTL_SMLEN];
	int err;

	/* use the return buffer if it is bigger than what we have on the stack */
	if (len > (int)sizeof(smbuf)) {
		err = wl_bssiovar_getbuf(wl, iovar, bssidx, NULL, 0, outbuf, len);
	} else {
		memset(smbuf, 0, sizeof(smbuf));
		err = wl_bssiovar_getbuf(wl, iovar, bssidx, NULL, 0, smbuf, sizeof(smbuf));
		if (err == 0)
			memcpy(outbuf, smbuf, len);
	}

	return err;
}

/*
 * set named & bss indexed driver variable to buffer value
 */
static int
wl_bssiovar_set(void *wl, const char *iovar, int bssidx, void *param, int paramlen)
{
	char smbuf[WLC_IOCTL_SMLEN];

	memset(smbuf, 0, sizeof(smbuf));

	return wlu_bssiovar_setbuf(wl, iovar, bssidx, param, paramlen, smbuf, sizeof(smbuf));
}

/*
 * get named & bsscfg indexed driver variable as an int value
 */
static int
wl_bssiovar_getint(void *wl, const char *iovar, int bssidx, int *pval)
{
	int ret;

	ret = wlu_bssiovar_get(wl, iovar, bssidx, pval, sizeof(int));
	if (ret == 0)
	{
		*pval = dtoh32(*pval);
	}
	return ret;
}

/*
 * set named & bsscfg indexed driver variable to int value
 */
static int
wl_bssiovar_setint(void *wl, const char *iovar, int bssidx, int val)
{
	val = htod32(val);
	return wl_bssiovar_set(wl, iovar, bssidx, &val, sizeof(int));
}

static int
wl_nvdump(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	const char *iovar = "nvram_dump";
	void *p = NULL;

	UNUSED_PARAMETER(cmd);

	/* skip the "nvdump/nvram_dump" command name */
	argv++;

	if (*argv) {
		printf("nvdump error: extra arg \"%s\"\n", *argv);
		return -1;
	}

	if ((err = wlu_var_getbuf(wl, iovar, NULL, 0, &p)) < 0) {
		if ((err = wlu_get(wl, WLC_NVRAM_DUMP, &buf[0], WLC_IOCTL_MAXLEN)) < 0)
		    return err;
		p = (void *)buf;
	}
	printf("%s\n", (char *)p);

	return err;
}

static int
wl_nvget(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	char *varname;
	const char *iovar = "nvram_get";
	void *p;

	UNUSED_PARAMETER(cmd);

	/* skip the "nvget/nvram_get" command name */
	argv++;

	if (!*argv) {
		printf("nvget: missing arguments\n");
		return -1;
	}

	varname = *argv++;

	if (*argv) {
		printf("nvget error: extra arg \"%s\"\n", *argv);
		return -1;
	}

	if ((err = wlu_var_getbuf(wl, iovar, varname, strlen(varname) + 1, &p)) < 0) {

		strcpy(buf, varname);
		if ((err = wlu_get(wl, WLC_NVRAM_GET, &buf[0], WLC_IOCTL_MAXLEN)) < 0)
		    return err;
	}

	printf("%s\n", buf);

	return err;
}

static int
wl_nvset(void *wl, cmd_t *cmd, char **argv)
{
	char *varname;

	UNUSED_PARAMETER(cmd);

	/* skip the "nvset" command name if present */
	if (!strcmp("nvset", *argv))
		argv++;

	if (!*argv) {
		printf("nvset: missing arguments\n");
		return -1;
	}

	varname = *argv++;

	if (*argv) {
		fprintf(stderr,
		"nvset error: extra arg \"%s\"; format is name=value (no spaces around '=')\n",
			*argv);
		return -1;
	}

	if (!strchr(varname, '=')) {
		fprintf(stderr,
		"nvset error: no '=' in \"%s\", format is name=value (no spaces around '=')\n",
			*argv);
		return -1;
	}

	strcpy(buf, varname);

	return (wlu_set(wl, WLC_NVRAM_SET, &buf[0], strlen(buf) + 1));
}

static int
wl_chan_info(void *wl, cmd_t *cmd, char **argv)
{
	uint bitmap;
	uint channel;
	uint32 chanspec_arg;
	int buflen, err, first, last, minutes;
	char *param;
	bool all;

	if (!*++argv) {
		first = 0;
		last = MAXCHANNEL;
		all = TRUE;
	} else {
		last = first = atoi(*argv);
		if (last <= 0) {
			printf(" Usage: %s [channel | All ]\n", cmd->name);
			return -1;
		}
		all = FALSE;
	}

	for (; first <= last; first++) {
		channel = first;
		chanspec_arg = CH20MHZ_CHSPEC(channel);

		strcpy(buf, "per_chan_info");
		buflen = strlen(buf) + 1;
		param = (char *)(buf + buflen);
		/* there should be no problem if converting to a legacy chanspec
		 * since chanspec_arg is created as 20MHz
		 */
		chanspec_arg = wl_chspec32_to_driver(chanspec_arg);
		memcpy(param, (char*)&chanspec_arg, sizeof(chanspec_arg));

		if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
			return err;

		bitmap = dtoh32(*(uint *)buf);
		minutes = (bitmap >> 24) & 0xff;

		if (!(bitmap & WL_CHAN_VALID_HW)) {
			if (!all)
				printf("Invalid Channel\n");
			continue;
		}

		if (!(bitmap & WL_CHAN_VALID_SW)) {
			if (!all)
				printf("Not supported in current locale\n");
			continue;
		}

		printf("Channel %d\t", channel);

		if (bitmap & WL_CHAN_BAND_5G)
			printf("A Band");
		else
			printf("B Band");

		if (bitmap & WL_CHAN_RADAR) {
			printf(", RADAR Sensitive");
		}
		if (bitmap & WL_CHAN_RESTRICTED) {
			printf(", Restricted");
		}
		if (bitmap & WL_CHAN_PASSIVE) {
			printf(", Passive");
		}
		if (bitmap & WL_CHAN_INACTIVE) {
			printf(", Temporarily Out of Service for %d minutes", minutes);
		}
		printf("\n");
	}

	return (0);
}

static int
wl_test_tssi(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char* endptr = NULL;

	/* toss the command name */
	argv++;

	if (!*argv)
		return -1;

	val = htod32(strtol(*argv, &endptr, 0));
	if (*endptr != '\0') {
		/* not all the value string was parsed by strtol */
		printf("set: error parsing value \"%s\" as an integer\n", *argv);
		return -1;
	}

	ret = wlu_iovar_getbuf(wl, cmd->name, &val, sizeof(val),
	                      buf, WLC_IOCTL_MAXLEN);

	if (ret)
		return ret;

	val = dtoh32(*(int*)buf);

	wl_printint(val);

	return ret;
}

static int
wl_test_tssi_offs(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char* endptr = NULL;

	/* toss the command name */
	argv++;

	if (!*argv)
		return -1;

	val = htod32(strtol(*argv, &endptr, 0));
	if (*endptr != '\0') {
		/* not all the value string was parsed by strtol */
		printf("set: error parsing value \"%s\" as an integer\n", *argv);
		return -1;
	}

	ret = wlu_iovar_getbuf(wl, cmd->name, &val, sizeof(val),
	                      buf, WLC_IOCTL_MAXLEN);

	if (ret)
		return ret;

	val = dtoh32(*(int*)buf);

	wl_printint(val);

	return ret;
}

static int
wl_sta_info(void *wl, cmd_t *cmd, char **argv)
{
	sta_info_t *sta;
	struct ether_addr ea;
	char *param;
	int buflen, err;

	/* convert the ea string into an ea struct */
	if (!*++argv || !wl_ether_atoe(*argv, &ea)) {
		printf(" ERROR: no valid ether addr provided\n");
		return -1;
	}

	strcpy(buf, "sta_info");
	buflen = strlen(buf) + 1;
	param = (char *)(buf + buflen);
	memcpy(param, (char*)&ea, ETHER_ADDR_LEN);

	if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MEDLEN)) < 0)
		return err;

	/* display the sta info */
	sta = (sta_info_t *)buf;
	sta->ver = dtoh16(sta->ver);

	/* Report unrecognized version */
	if (sta->ver > WL_STA_VER) {
		printf(" ERROR: unknown driver station info version %d\n", sta->ver);
		return -1;
	}

	sta->len = dtoh16(sta->len);
	sta->cap = dtoh16(sta->cap);
	sta->flags = dtoh32(sta->flags);
	sta->idle = dtoh32(sta->idle);
	sta->rateset.count = dtoh32(sta->rateset.count);
	sta->in = dtoh32(sta->in);
	sta->listen_interval_inms = dtoh32(sta->listen_interval_inms);

	printf(" STA %s:\n", *argv);
	printf("\t rateset ");
	dump_rateset(sta->rateset.rates, sta->rateset.count);
	printf("\n\t idle %d seconds\n", sta->idle);
	printf("\t in network %d seconds\n", sta->in);
	printf("\t state:%s%s%s\n",
	       (sta->flags & WL_STA_AUTHE) ? " AUTHENTICATED" : "",
	       (sta->flags & WL_STA_ASSOC) ? " ASSOCIATED" : "",
	       (sta->flags & WL_STA_AUTHO) ? " AUTHORIZED" : "");

	printf("\t flags 0x%x:%s%s%s%s%s%s%s%s\n",
	       sta->flags,
	       (sta->flags & WL_STA_BRCM) ? " BRCM" : "",
	       (sta->flags & WL_STA_WME) ? " WME" : "",
	       (sta->flags & WL_STA_PS) ? " PS" : "",
	       (sta->flags & WL_STA_APSD_BE) ? " APSD_BE" : "",
	       (sta->flags & WL_STA_APSD_BK) ? " APSD_BK" : "",
	       (sta->flags & WL_STA_APSD_VI) ? " APSD_VI" : "",
	       (sta->flags & WL_STA_APSD_VO) ? " APSD_VO" : "",
	       (sta->flags & WL_STA_N_CAP) ? " N_CAP" : "");

	/* Driver didn't return extended station info */
	if (sta->len < sizeof(sta_info_t))
		return 0;

	if (sta->flags & WL_STA_SCBSTATS)
	{
		printf("\t tx pkts: %d\n", dtoh32(sta->tx_pkts));
		printf("\t tx failures: %d\n", dtoh32(sta->tx_failures));
		printf("\t rx ucast pkts: %d\n", dtoh32(sta->rx_ucast_pkts));
		printf("\t rx mcast/bcast pkts: %d\n", dtoh32(sta->rx_mcast_pkts));
		printf("\t rate of last tx pkt: %d kbps\n", dtoh32(sta->tx_rate));
		printf("\t rate of last rx pkt: %d kbps\n", dtoh32(sta->rx_rate));
		printf("\t rx decrypt succeeds: %d\n", dtoh32(sta->rx_decrypt_succeeds));
		printf("\t rx decrypt failures: %d\n", dtoh32(sta->rx_decrypt_failures));
	}

	return (0);
}

static int
wl_revinfo(void *wl, cmd_t *cmd, char **argv)
{
	char b[8];
	int err;
	wlc_rev_info_t revinfo;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	memset(&revinfo, 0, sizeof(revinfo));

	if ((err = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo))) < 0)
		return err;

	printf("vendorid 0x%x\n", dtoh32(revinfo.vendorid));
	printf("deviceid 0x%x\n", dtoh32(revinfo.deviceid));
	printf("radiorev 0x%x\n", dtoh32(revinfo.radiorev));
	printf("chipnum 0x%x\n", dtoh32(revinfo.chipnum));
	printf("chiprev 0x%x\n", dtoh32(revinfo.chiprev));
	printf("chippackage 0x%x\n", dtoh32(revinfo.chippkg));
	printf("corerev 0x%x\n", dtoh32(revinfo.corerev));
	printf("boardid 0x%x\n", dtoh32(revinfo.boardid));
	printf("boardvendor 0x%x\n", dtoh32(revinfo.boardvendor));
	printf("boardrev %s\n", bcm_brev_str(dtoh32(revinfo.boardrev), b));
	printf("driverrev 0x%x\n", dtoh32(revinfo.driverrev));
	printf("ucoderev 0x%x\n", dtoh32(revinfo.ucoderev));
	printf("bus 0x%x\n", dtoh32(revinfo.bus));
	printf("phytype 0x%x\n", dtoh32(revinfo.phytype));
	printf("phyrev 0x%x\n", dtoh32(revinfo.phyrev));
	printf("anarev 0x%x\n", dtoh32(revinfo.anarev));
	printf("nvramrev %d\n", dtoh32(revinfo.nvramrev));

	return 0;
}

static int
wl_rm_request(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t to;
	const char* fn_name = "wl_rm_request";
	wl_rm_req_t *rm_ptr;
	wl_rm_req_t rm;
	wl_rm_req_elt_t req;
	int buflen = 0;
	int err, opt_err;
	int type;
	bool in_measure = FALSE;

	UNUSED_PARAMETER(cmd);

	memset(buf, 0, WLC_IOCTL_MAXLEN);
	memset(&rm, 0, WL_RM_REQ_FIXED_LEN);
	memset(&req, 0, sizeof(wl_rm_req_elt_t));

	strcpy(buf, "rm_req");
	buflen = strlen(buf) + 1;

	rm_ptr = (wl_rm_req_t*)(buf + buflen);
	buflen += WL_RM_REQ_FIXED_LEN;

	/* toss the command name */
	argv++;

	miniopt_init(&to, fn_name, "p", FALSE);
	while ((opt_err = miniopt(&to, argv)) != -1) {
		if (opt_err == 1) {
			err = -1;
			goto exit;
		}
		argv += to.consumed;

		if (to.opt == 't') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for the token\n",
					fn_name, to.valstr);
				err = -1;
				goto exit;
			}

			if (!in_measure)
				rm.token = to.val;
			else
				req.token = to.val;
		}
		if (to.opt == 'c') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for channel\n",
					fn_name, to.valstr);
				err = -1;
				goto exit;
			}

			req.chanspec = CH20MHZ_CHSPEC(to.val);
		}
		if (to.opt == 'd') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for duration\n",
					fn_name, to.valstr);
				err = -1;
				goto exit;
			}
			req.dur = to.val;
		}

		if (to.opt == 'p') {
			req.flags = WL_RM_FLAG_PARALLEL;
		}

		if (to.positional) {
			if (!strcmp(to.valstr, "basic")) {
				type = WL_RM_TYPE_BASIC;
			} else if (!strcmp(to.valstr, "cca")) {
				type = WL_RM_TYPE_CCA;
			} else if (!strcmp(to.valstr, "rpi")) {
				type = WL_RM_TYPE_RPI;
			} else {
				fprintf(stderr,
					"%s: could not parse \"%s\" as a measurement type\n",
					fn_name, to.valstr);
				err = -1;
				goto exit;
			}
			/* complete the previous measurement */
			if (in_measure) {
				req.chanspec = wl_chspec_to_driver(req.chanspec);
				req.token = htod32(req.token);
				req.tsf_h = htod32(req.tsf_h);
				req.tsf_l = htod32(req.tsf_l);
				req.dur = htod32(req.dur);
				memcpy(buf + buflen, &req, sizeof(wl_rm_req_elt_t));
				buflen += sizeof(wl_rm_req_elt_t);
				rm.count++;
				req.chanspec = wl_chspec_from_driver(req.chanspec);
				req.token = dtoh32(req.token);
				req.tsf_h = dtoh32(req.tsf_h);
				req.tsf_l = dtoh32(req.tsf_l);
				req.dur = dtoh32(req.dur);
				/* measure to measure default param update */
				req.token++;	/* each measure gets a new token */
				req.flags = 0;	/* measure flags are cleared between measures */
			}
			in_measure = TRUE;
			req.type = (int8)type;
		}
	}

	/* complete the last measurement */
	if (in_measure) {
		req.chanspec = wl_chspec_to_driver(req.chanspec);
		req.token = htod32(req.token);
		req.tsf_h = htod32(req.tsf_h);
		req.tsf_l = htod32(req.tsf_l);
		req.dur = htod32(req.dur);
		memcpy(buf + buflen, &req, sizeof(wl_rm_req_elt_t));
		buflen += sizeof(wl_rm_req_elt_t);
		rm.count++;
	}

	if (rm.count == 0) {
		fprintf(stderr, "%s: no measurement requests specified\n",
			fn_name);
		err = -1;
		goto exit;
	}

	rm.token = htod32(rm.token);
	rm.count = htod32(rm.count);
	memcpy(rm_ptr, &rm, WL_RM_REQ_FIXED_LEN);

	err = wlu_set(wl, WLC_SET_VAR, &buf[0], buflen);

exit:
	return err;
}

static int
wl_rm_report(void *wl, cmd_t *cmd, char **argv)
{
	wl_rm_rep_t *rep_set;
	wl_rm_rep_elt_t rep;
	char extra[128];
	char* p;
	const char* name;
	uint8* data;
	int err, bin;
	uint32 val;
	uint16 channel;
	bool aband;
	int len;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	strcpy(buf, "rm_rep");

	if ((err = wlu_get(wl, WLC_GET_VAR, &buf[0], WLC_IOCTL_MAXLEN)) < 0)
		return err;

	rep_set = (wl_rm_rep_t *)buf;
	rep_set->token = dtoh32(rep_set->token);
	rep_set->len = dtoh32(rep_set->len);

	printf("Measurement Report: token %d, length %d\n", rep_set->token, rep_set->len);

	len = rep_set->len;
	data = (uint8*)rep_set->rep;
	for (; len > 0; (len -= rep.len), (data += rep.len)) {
		if (len >= WL_RM_REP_ELT_FIXED_LEN)
			memcpy(&rep, data, WL_RM_REP_ELT_FIXED_LEN);
		else
			break;

		rep.chanspec = wl_chspec_from_driver(rep.chanspec);
		rep.token = dtoh32(rep.token);
		rep.tsf_h = dtoh32(rep.tsf_h);
		rep.tsf_l = dtoh32(rep.tsf_l);
		rep.dur = dtoh32(rep.dur);
		rep.len = dtoh32(rep.len);

		data += WL_RM_REP_ELT_FIXED_LEN;
		len -= WL_RM_REP_ELT_FIXED_LEN;

		if (rep.type == WL_RM_TYPE_BASIC)
			name = "Basic";
		else if (rep.type == WL_RM_TYPE_CCA)
			name = "CCA";
		else if (rep.type == WL_RM_TYPE_RPI)
			name = "RPI";
		else
			name = NULL;

		if (name)
			printf("\nReport   : %s\n", name);
		else
			printf("\nReport   : %d <unknown>\n", rep.type);

		p = extra;
		if (rep.flags & WL_RM_FLAG_PARALLEL) {
			if (p != extra) p += sprintf(p, " | ");
			p += sprintf(p, "Parallel");
		}
		if (rep.flags & WL_RM_FLAG_LATE) {
			if (p != extra) p += sprintf(p, " | ");
			p += sprintf(p, "Late");
		}
		if (rep.flags & WL_RM_FLAG_INCAPABLE) {
			if (p != extra) p += sprintf(p, " | ");
			p += sprintf(p, "Incapable");
		}
		if (rep.flags & WL_RM_FLAG_REFUSED) {
			if (p != extra) p += sprintf(p, " | ");
			p += sprintf(p, "Refused");
		}

		if (p != extra) {
			printf("flags    : 0x%02x (%s)\n", rep.flags, extra);
		} else {
			printf("flags    : 0x%02x\n", rep.flags);
		}
		printf("token    : %4d\n", rep.token);

		if (rep.flags & (WL_RM_FLAG_LATE |
			WL_RM_FLAG_INCAPABLE |
			WL_RM_FLAG_REFUSED)) {
			continue;
		}

		channel = CHSPEC_CHANNEL(rep.chanspec);
		aband = CHSPEC_IS5G(rep.chanspec);

		printf("channel  : %4d %s\n", channel,
		       aband ? "(a)":"(b)");
		printf("start tsf: 0x%x:%08x\n", rep.tsf_h, rep.tsf_l);
		printf("duration : %4d TU\n", rep.dur);

		if (len < (int)rep.len) {
			printf("Error: partial report element, %d report bytes "
			       "remain, element claims %d\n",
			       len, rep.len);
			break;
		}

		if (rep.type == WL_RM_TYPE_BASIC) {
			if (rep.len >= 4) {
				memcpy(&val, data, sizeof(uint32));
				val = dtoh32(val);
				printf("Basic bits: 0x%08x\n", val);
			}
		} else if (rep.type == WL_RM_TYPE_CCA) {
			if (rep.len >= 4) {
				memcpy(&val, data, sizeof(uint32));
				val = dtoh32(val);
				printf("Carrier Fraction: %d / 255\n", val);
			}
		} else if (rep.type == WL_RM_TYPE_RPI) {
			if (rep.len >= sizeof(wl_rm_rpi_rep_t)) {
				wl_rm_rpi_rep_t rpi_rep;
				int8 min = -128;
				int8 max;

				memcpy(&rpi_rep, data, sizeof(wl_rm_rpi_rep_t));

				for (bin = 0; bin < 8; bin++) {
					max = rpi_rep.rpi_max[bin];
					if (bin == 0)
						printf("       Power <= %3d: ",
						       max);
					else if (bin < 7)
						printf(" %3d < Power <= %3d: ",
						       min, max);
					else
						printf(" %3d < Power       : ",
						       min);
					min = max;
					printf("%3d\n", rpi_rep.rpi[bin]);
				}
			}
		}
	}

	return err;
}

static int
wl_join_pref(void *wl, cmd_t *cmd, char **argv)
{
	char* data;
	int err;
	int len;
	int remaining_bytes;
	int i;
	bcm_tlv_t *ie;

	UNUSED_PARAMETER(cmd);

	strcpy(buf, "join_pref");

	/* set */
	if (argv[1]) {
		len = strlen(buf);
		data = argv[1];
		for (i = len + 1, len += 1 + strlen(data) / 2;
		    (i < len) && (i < (int)WLC_IOCTL_MAXLEN); i ++) {
			char hex[] = "XX";
			hex[0] = *data++;
			hex[1] = *data++;
			buf[i] = (uint8)strtoul(hex, NULL, 16);
		}
		err = wlu_set(wl, WLC_SET_VAR, buf, i);
	}
	/* get */
	else if (!(err = wlu_get(wl, WLC_GET_VAR, buf, WLC_IOCTL_MAXLEN))) {
		len = dtoh32(*(int *)buf);
		data = buf + sizeof(int);
		for (i = 0; i < len; i ++)
			printf("%02x", data[i]);
		printf("\n");
		/* pretty print the join pref elements */
		remaining_bytes = len;
		ie = (bcm_tlv_t*)data;
		if (!bcm_valid_tlv(ie, remaining_bytes))
		    ie = NULL;
		while (ie) {
			wl_join_pref_print_ie(ie);
			ie = bcm_next_tlv(ie, &remaining_bytes);
		}
	}
	return err;
}

static void
wl_join_pref_print_ie(bcm_tlv_t *ie)
{
	int i;
	uint8 band;
	uint8 count;
	int suite_len;
	uint8 *suite;
	int data_bytes;


	switch (ie->id) {
	case WL_JOIN_PREF_RSSI:
		printf("Pref RSSI\n");
		if (ie->len > 2)
			printf("\t<%d extra bytes in pref data>\n", ie->len);
		break;
	case WL_JOIN_PREF_BAND:
		printf("Pref BAND: ");
		if (ie->len < 2) {
			printf("len = %d <band pref data truncated>\n", ie->len);
			break;
		}

		band = ie->data[1];
		if (band == WLC_BAND_AUTO)
			printf("0x%x AUTO (no preference)\n", band);
		else if (band == WLC_BAND_5G)
			printf("0x%x 5 GHz\n", band);
		else if (band == WLC_BAND_2G)
			printf("0x%x 2.4 GHz\n", band);
		else if (band == WLJP_BAND_ASSOC_PREF)
			printf("0x%x Use ASSOC_PREFER value\n", band);
		else
			printf("0x%x\n", band);

		if (ie->len > 2)
			printf("\t<%d extra bytes in pref data>\n", ie->len - 1);

		break;
	case WL_JOIN_PREF_WPA:
		printf("Pref WPA: ");
		if (ie->len < 2) {
			printf("len = %d <WPA pref data truncated>\n", ie->len);
			break;
		}
		count = ie->data[1];
		printf("%d ACP Specs\n", count);

		data_bytes = ie->len - 2;
		suite_len = 4; /* WPA Suite Selector length, OUI + type */
		suite = ie->data + 2;

		for (i = 0; i < (int)count; i++) {
			if (data_bytes < 3 * suite_len)
				break;
			printf("\t");
			/* AKM Suite */
			wl_join_pref_print_akm(suite);
			printf(",");

			suite = suite + suite_len;
			/* Unicast Cipher Suite */
			printf("U:");
			wl_join_pref_print_cipher_suite(suite);
			printf(",");

			suite = suite + suite_len;
			/* Multicast Cipher Suite */
			printf("M:");
			if (!memcmp(suite, WL_WPA_ACP_MCS_ANY, suite_len))
				printf("Any");
			else
				wl_join_pref_print_cipher_suite(suite);
			printf("\n");

			suite = suite + suite_len;
			data_bytes -= 3 * suite_len;
		}

		if (i != count)
			printf("\t<expected %d more specs, %d bytes>\n",
			count - i, suite_len * (count - i));
		if (data_bytes > 0)
			printf("\t<%d extra bytes>\n", data_bytes);
		break;
	case WL_JOIN_PREF_RSSI_DELTA:
		printf("RSSI Delta for Pref BAND: ");
		if (ie->len < 2) {
			printf("len = %d <rssi delta pref data truncated>\n", ie->len);
			break;
		}

		band = ie->data[1];
		if (band == WLC_BAND_AUTO)
			printf("0x%x AUTO (no preference)\n", band);
		else if (band == WLC_BAND_5G)
			printf("0x%x 5 GHz\n", band);
		else if (band == WLC_BAND_2G)
			printf("0x%x 2.4 GHz\n", band);
		else
			printf("0x%x\n", band);

		printf("RSSI boost %ddb\n", ie->data[0]);

		break;
	default:
		printf("Pref 0x%x: len = %d\n", ie->id, ie->len);
		for (i = 0; i < ie->len; i++)
			printf("%02x", ie->data[i]);
		printf("\n");
		break;

	}

}

static void
wl_join_pref_print_akm(uint8* suite)
{
	uint8 type = suite[3];
	const char *oui_name;

	if (!memcmp(suite, WPA_OUI, 3))
		oui_name = "WPA";
	else if (!memcmp(suite, WPA2_OUI, 3))
		oui_name = "WPA2";
	else
		oui_name = NULL;

	if (oui_name) {
		if (type == RSN_AKM_NONE)
			printf("%s-NONE", oui_name);
		else if (type == RSN_AKM_UNSPECIFIED)
			printf("%s", oui_name);
		else if (type == RSN_AKM_UNSPECIFIED)
			printf("%s-PSK", oui_name);
		else
			printf("%s/0x%x", oui_name, type);
	} else {
		printf("0x%02x%02x%02x/0x%02x", suite[0], suite[1], suite[2], suite[3]);
	}
}

static void
wl_join_pref_print_cipher_suite(uint8* suite)
{
	uint8 type = suite[3];
	const char *oui_name;

	if (!memcmp(suite, WPA_OUI, 3))
		oui_name = "WPA";
	else if (!memcmp(suite, WPA2_OUI, 3))
		oui_name = "WPA2";
	else
		oui_name = NULL;

	if (oui_name) {
		if (type == WPA_CIPHER_NONE)
			printf("%s/NONE", oui_name);
		else if (type == WPA_CIPHER_WEP_40)
			printf("%s/WEP40", oui_name);
		else if (type == WPA_CIPHER_TKIP)
			printf("%s/TKIP", oui_name);
		else if (type == WPA_CIPHER_AES_CCM)
			printf("%s/AES", oui_name);
		else if (type == WPA_CIPHER_WEP_104)
			printf("%s/WEP104", oui_name);
		else
			printf("%s/0x%x", oui_name, type);
	} else {
		printf("0x%02x%02x%02x/0x%02x", suite[0], suite[1], suite[2], suite[3]);
	}
}

static int
wl_assoc_pref(void *wl, cmd_t *cmd, char **argv)
{
	uint assoc_pref;
	int err;

	/* set */
	if (argv[1]) {
		if (!strcmp(argv[1], "auto") || !strcmp(argv[1], "0"))
			assoc_pref = WLC_BAND_AUTO;
		else if (!strcmp(argv[1], "a") || !strcmp(argv[1], "1"))
			assoc_pref = WLC_BAND_5G;
		else if (!strcmp(argv[1], "b") || !strcmp(argv[1], "g") || !strcmp(argv[1], "2"))
			assoc_pref = WLC_BAND_2G;
		else
			return -1;
		assoc_pref = htod32(assoc_pref);
		err = wlu_set(wl, cmd->set, &assoc_pref, sizeof(assoc_pref));
	}
	/* get */
	else if (!(err = wlu_get(wl, cmd->get, &assoc_pref, sizeof(assoc_pref)))) {
		assoc_pref = dtoh32(assoc_pref);
		switch (assoc_pref) {
		case WLC_BAND_AUTO:
			printf("auto\n");
			break;
		case WLC_BAND_5G:
			printf("a\n");
			break;
		case WLC_BAND_2G:
			printf("b/g\n");
			break;
		}
	}
	return err;
}

static const char ac_names[AC_COUNT][6] = {"AC_BE", "AC_BK", "AC_VI", "AC_VO"};

/*
 * Get or set WME per-AC transmit parameters
 */
static int
wme_tx_params(void *wl, cmd_t *cmd, char **argv)
{
	char *val_p, *ac_str, *param;
	int buflen;
	int aci;
	wme_tx_params_t cur_params[AC_COUNT], new_params[AC_COUNT];
	int err;
	int val;

	UNUSED_PARAMETER(cmd);

	argv++;

	buflen = WLC_IOCTL_MAXLEN;

	/*
	 * Get current acparams, using buf as an input buffer.
	 * Return data is array of 4 ACs of wme params.
	 */

	strcpy(buf, "wme_tx_params");
	if ((err = wlu_get(wl, WLC_GET_VAR, &buf[0], buflen)) < 0) {
		return err;
	}
	memcpy(&cur_params, buf, WL_WME_TX_PARAMS_IO_BYTES);

	if ((ac_str = *argv++) == NULL) {
		printf("WME TX params: \n");
		for (aci = 0; aci < AC_COUNT; aci++) {
			printf("%s: short %d. sfb %d. long %d. lfb %d. max %d\n", ac_names[aci],
				cur_params[aci].short_retry,
				cur_params[aci].short_fallback,
				cur_params[aci].long_retry,
				cur_params[aci].long_fallback,
				cur_params[aci].max_rate);
		}
	} else {
		int chk_lim;
		if (strcmp(ac_str, "be") == 0) {
			aci = AC_BE;
		} else if (strcmp(ac_str, "bk") == 0) {
			aci = AC_BK;
		} else if (strcmp(ac_str, "vi") == 0) {
			aci = AC_VI;
		} else if (strcmp(ac_str, "vo") == 0) {
			aci = AC_VO;
		} else {
			printf("Unknown access class: %s\n", ac_str);
			return USAGE_ERROR;
		}

		/* Preload new values with current values */
		memcpy(&new_params, &cur_params, sizeof(new_params));
		while ((param = *argv++) != NULL) {
			if ((val_p = *argv++) == NULL) {
				printf("Need value following %s\n", param);
				return USAGE_ERROR;
			}
			chk_lim = 15;
			val = (int)strtoul(val_p, NULL, 0);
			/* All values must fit in uint8 */
			if (!strcmp(param, "short")) {
				new_params[aci].short_retry = (uint8)val;
			} else if (!strcmp(param, "sfb")) {
				new_params[aci].short_fallback = (uint8)val;
			} else if (!strcmp(param, "long")) {
				new_params[aci].long_retry = (uint8)val;
			} else if (!strcmp(param, "lfb")) {
				new_params[aci].long_fallback = (uint8)val;
			} else if ((!strcmp(param, "max_rate")) || (!strcmp(param, "max")) ||
				(!strcmp(param, "rate"))) {
				chk_lim = 255;
				new_params[aci].max_rate = (uint8)val;
			} else {
				printf("Unknown parameter: %s\n", param);
				return USAGE_ERROR;
			}
			if (val > chk_lim) {
				printf("Value for %s must be < %d\n", param, chk_lim + 1);
				return USAGE_ERROR;
			}
		}
		strcpy(buf, "wme_tx_params");
		memcpy(buf + strlen(buf) + 1, new_params, WL_WME_TX_PARAMS_IO_BYTES);
		err = wlu_set(wl, WLC_SET_VAR, &buf[0], buflen);
	}

	return 0;
}

/*
 * Get or Set WME Access Class (AC) parameters
 *	wl wme_ac ap|sta [be|bk|vi|vo [ecwmax|ecwmin|txop|aifsn|acm <value>] ...]
 * Without args past ap|sta, print current values
 */
static int
wl_wme_ac_req(void *wl, cmd_t *cmd, char **argv)
{
	const char *iovar_name;
	int err;
	edcf_acparam_t acparam_cur[AC_COUNT], acparam_new[AC_COUNT], *acp;
	char *ac_str, *param, *val;
	bool acm;
	int aci, aifsn, ecwmin, ecwmax, txop;

	UNUSED_PARAMETER(cmd);

	argv++;

	if ((param = *argv++) == NULL)
		return USAGE_ERROR;

	if (!strcmp(param, "ap"))
		iovar_name = "wme_ac_ap";
	else if (!strcmp(param, "sta"))
		iovar_name = "wme_ac_sta";
	else
		return USAGE_ERROR;

	/*
	 * Get current acparams into an array of 4 ACs of wme params.
	 */
	err = wlu_iovar_get(wl, iovar_name, &acparam_cur, sizeof(acparam_cur));
	if (err < 0)
		return err;

	if ((ac_str = *argv++) == NULL) {
		printf("AC Parameters\n");

		for (aci = 0; aci < AC_COUNT; aci++) {
			acp = &acparam_cur[aci];
			acp->TXOP = dtoh16(acp->TXOP);
			if (((acp->ACI & EDCF_ACI_MASK) >> EDCF_ACI_SHIFT) != aci)
				printf("Warning: AC params out of order\n");
			acm = (acp->ACI & EDCF_ACM_MASK) ? 1 : 0;
			aifsn = acp->ACI & EDCF_AIFSN_MASK;
			ecwmin = acp->ECW & EDCF_ECWMIN_MASK;
			ecwmax = (acp->ECW & EDCF_ECWMAX_MASK) >> EDCF_ECWMAX_SHIFT;
			txop = acp->TXOP;
			printf("%s: raw: ACI 0x%x ECW 0x%x TXOP 0x%x\n",
			       ac_names[aci],
			       acp->ACI, acp->ECW, acp->TXOP);
			printf("       dec: aci %d acm %d aifsn %d "
			       "ecwmin %d ecwmax %d txop 0x%x\n",
			       aci, acm, aifsn, ecwmin, ecwmax, txop);
			/* CWmin = 2^(ECWmin) - 1 */
			/* CWmax = 2^(ECWmax) - 1 */
			/* TXOP = number of 32 us units */
			printf("       eff: CWmin %d CWmax %d TXop %dusec\n",
			       EDCF_ECW2CW(ecwmin), EDCF_ECW2CW(ecwmax), EDCF_TXOP2USEC(txop));
		}

		err = 0;
	} else {
		if (strcmp(ac_str, "be") == 0)
			aci = AC_BE;
		else if (strcmp(ac_str, "bk") == 0)
			aci = AC_BK;
		else if (strcmp(ac_str, "vi") == 0)
			aci = AC_VI;
		else if (strcmp(ac_str, "vo") == 0)
			aci = AC_VO;
		else
			return USAGE_ERROR;

		/* Preload new values with current values */
		memcpy(&acparam_new, &acparam_cur, sizeof(acparam_new));

		acp = &acparam_new[aci];

		while ((param = *argv++) != NULL) {
			if ((val = *argv++) == NULL)
				return USAGE_ERROR;

			if (!strcmp(param, "acm")) {
				if (!stricmp(val, "on") || !stricmp(val, "1"))
					acp->ACI |= EDCF_ACM_MASK;
				else if (!stricmp(val, "off") || !stricmp(val, "0"))
					acp->ACI &= ~EDCF_ACM_MASK;
				else {
					fprintf(stderr, "acm value must be 1|0\n");
					return USAGE_ERROR;
				}
			} else if (!strcmp(param, "aifsn")) {
				aifsn = (int)strtol(val, NULL, 0);
				if (aifsn >= EDCF_AIFSN_MIN && aifsn <= EDCF_AIFSN_MAX)
					acp->ACI =
					        (acp->ACI & ~EDCF_AIFSN_MASK) |
					        (aifsn & EDCF_AIFSN_MASK);
				else {
					fprintf(stderr, "aifsn %d out of range (%d-%d)\n",
					        aifsn, EDCF_AIFSN_MIN, EDCF_AIFSN_MAX);
					return USAGE_ERROR;
				}
			} else if (!strcmp(param, "ecwmax")) {
				ecwmax = (int)strtol(val, NULL, 0);
				if (ecwmax >= EDCF_ECW_MIN && ecwmax <= EDCF_ECW_MAX)
					acp->ECW =
					        ((ecwmax << EDCF_ECWMAX_SHIFT) & EDCF_ECWMAX_MASK) |
					        (acp->ECW & EDCF_ECWMIN_MASK);
				else {
					fprintf(stderr, "ecwmax %d out of range (%d-%d)\n",
					        ecwmax, EDCF_ECW_MIN, EDCF_ECW_MAX);
					return USAGE_ERROR;
				}
			} else if (!strcmp(param, "ecwmin")) {
				ecwmin = (int)strtol(val, NULL, 0);
				if (ecwmin >= EDCF_ECW_MIN && ecwmin <= EDCF_ECW_MAX)
					acp->ECW =
					        ((acp->ECW & EDCF_ECWMAX_MASK) |
					         (ecwmin & EDCF_ECWMIN_MASK));
				else {
					fprintf(stderr, "ecwmin %d out of range (%d-%d)\n",
					        ecwmin, EDCF_ECW_MIN, EDCF_ECW_MAX);
					return USAGE_ERROR;
				}
			} else if (!strcmp(param, "txop")) {
				txop = (int)strtol(val, NULL, 0);
				if (txop >= EDCF_TXOP_MIN && txop <= EDCF_TXOP_MAX)
					acp->TXOP = htod16(txop);
				else {
					fprintf(stderr, "txop %d out of range (%d-%d)\n",
					        txop, EDCF_TXOP_MIN, EDCF_TXOP_MAX);
					return USAGE_ERROR;
				}
			} else {
				fprintf(stderr, "unexpected param %s\n", param);
				return USAGE_ERROR;
			}
		}

		/*
		 * Now set the new acparams
		 * NOTE: only one of the four ACs can be set at a time.
		 */
		err = wlu_iovar_set(wl, iovar_name, acp, sizeof(edcf_acparam_t));
	}

	return err;
}

/*
 * Get or Set WME APSD control parameters
 *	wl wme_apsd_sta <max_sp_len> <be> <bk> <vi> <vo>
 *	  <max_sp_len> is 0 (all), 2, 4, or 6
 *        <be>, <bk>, <vi>, <vo> are each 0 or 1 for APSD enable
 *  with no args, print current values
 */
static int
wl_wme_apsd_sta(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int buflen;
	char *param;
	int ap_mode;
	int qosinfo;
	int msp, max_sp_len, be, bk, vi, vo;

	if ((err = wlu_get(wl, WLC_GET_AP, &ap_mode, sizeof(ap_mode))) < 0)
		return err;

	if (ap_mode) {
		printf("%s: STA only\n", cmd->name);
		return -1;
	}

	/* Display current params if no args, else set params */

	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, "wme_qosinfo");
	buflen = WLC_IOCTL_MAXLEN;

	param = *++argv;

	if (param == NULL) {
		if ((err = wlu_get(wl, cmd->get, &buf[0], buflen)) < 0)
			return err;

		memcpy(&qosinfo, buf, sizeof(qosinfo));
		qosinfo = dtoh32(qosinfo);

		msp = (qosinfo & WME_QI_STA_MAXSPLEN_MASK) >> WME_QI_STA_MAXSPLEN_SHIFT;
		be = (qosinfo & WME_QI_STA_APSD_BE_MASK) >> WME_QI_STA_APSD_BE_SHIFT;
		bk = (qosinfo & WME_QI_STA_APSD_BK_MASK) >> WME_QI_STA_APSD_BK_SHIFT;
		vi = (qosinfo & WME_QI_STA_APSD_VI_MASK) >> WME_QI_STA_APSD_VI_SHIFT;
		vo = (qosinfo & WME_QI_STA_APSD_VO_MASK) >> WME_QI_STA_APSD_VO_SHIFT;

		max_sp_len = msp * 2;

		printf("Max SP Length = %d, APSD: BE=%d BK=%d VI=%d VO=%d\n",
		       max_sp_len, be, bk, vi, vo);
	} else {
		max_sp_len = (int)strtol(param, 0, 0);
		if ((param = *++argv) == NULL)
			return -1;
		be = (int)strtol(param, 0, 0);
		if ((param = *++argv) == NULL)
			return -1;
		bk = (int)strtol(param, 0, 0);
		if ((param = *++argv) == NULL)
			return -1;
		vi = (int)strtol(param, 0, 0);
		if ((param = *++argv) == NULL)
			return -1;
		vo = (int)strtol(param, 0, 0);

		if (((be | bk | vi | vo) & ~1) | (max_sp_len & ~6)) {
			printf("%s: Invalid parameter\n", cmd->name);
			return -1;
		}

		msp = max_sp_len / 2;

		qosinfo = (msp << WME_QI_STA_MAXSPLEN_SHIFT) & WME_QI_STA_MAXSPLEN_MASK;
		qosinfo |= (be << WME_QI_STA_APSD_BE_SHIFT) & WME_QI_STA_APSD_BE_MASK;
		qosinfo |= (bk << WME_QI_STA_APSD_BK_SHIFT) & WME_QI_STA_APSD_BK_MASK;
		qosinfo |= (vi << WME_QI_STA_APSD_VI_SHIFT) & WME_QI_STA_APSD_VI_MASK;
		qosinfo |= (vo << WME_QI_STA_APSD_VO_SHIFT) & WME_QI_STA_APSD_VO_MASK;

		qosinfo = htod32(qosinfo);
		memcpy(&buf[strlen(buf) + 1], &qosinfo, sizeof(qosinfo));

		err = wlu_set(wl, cmd->set, &buf[0], buflen);
	}

	return err;
}

/*
 * Get or Set WME discard policy
 *	wl wme_dp <be> <bk> <vi> <vo>
 *        <be>, <bk>, <vi>, <vo> are each 0/1 for discard newest/oldest first
 *  with no args, print current values
 */
static int
wl_wme_dp(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int buflen;
	char *param;
	int dp;
	int be, bk, vi, vo;

	/* Display current params if no args, else set params */

	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, "wme_dp");
	buflen = WLC_IOCTL_MAXLEN;

	param = *++argv;

	if (param == NULL) {
		if ((err = wlu_get(wl, cmd->get, &buf[0], buflen)) < 0)
			return err;

		memcpy(&dp, buf, sizeof(dp));
		dp = dtoh32(dp);

		be = (dp >> AC_BE) & 1;
		bk = (dp >> AC_BK) & 1;
		vi = (dp >> AC_VI) & 1;
		vo = (dp >> AC_VO) & 1;

		printf("Discard oldest first: BE=%d BK=%d VI=%d VO=%d\n", be, bk, vi, vo);
	} else {
		be = (int)strtol(param, 0, 0);
		if ((param = *++argv) == NULL)
			return -1;
		bk = (int)strtol(param, 0, 0);
		if ((param = *++argv) == NULL)
			return -1;
		vi = (int)strtol(param, 0, 0);
		if ((param = *++argv) == NULL)
			return -1;
		vo = (int)strtol(param, 0, 0);

		if ((be | bk | vi | vo) & ~1) {
			printf("%s: Invalid parameter\n", cmd->name);
			return -1;
		}

		dp = (be << AC_BE) | (bk << AC_BK) | (vi << AC_VI) | (vo << AC_VO);

		dp = htod32(dp);
		memcpy(&buf[strlen(buf) + 1], &dp, sizeof(dp));

		err = wlu_set(wl, cmd->set, &buf[0], buflen);
	}

	return err;
}

/*
 * Get or Set WME lifetime parameter
 *	"wl lifetime be|bk|vi|vo [<value>]"},
 *  with no args, print current values
 */
static int
wl_lifetime(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	uint8 ac;
	char *param, *val;
	const char *cmdname = "lifetime";
	wl_lifetime_t lifetime, *reply;
	void *ptr = NULL;

	UNUSED_PARAMETER(cmd);

	if ((param = *++argv) == NULL)
		return USAGE_ERROR;

	if (strcmp(param, "be") == 0)
		ac = AC_BE;
	else if (strcmp(param, "bk") == 0)
		ac = AC_BK;
	else if (strcmp(param, "vi") == 0)
		ac = AC_VI;
	else if (strcmp(param, "vo") == 0)
		ac = AC_VO;
	else {
		fprintf(stderr, "unexpected param %s\n", param);
		return USAGE_ERROR;
	}

	if ((val = *++argv) == NULL) {
		lifetime.ac = htod32(ac);
		if ((err = wlu_var_getbuf(wl, cmdname, &lifetime, sizeof(lifetime),
		                         &ptr)) < 0)
			return err;
		reply = (wl_lifetime_t *) ptr;
		reply->ac = dtoh32(reply->ac);
		reply->lifetime = dtoh32(reply->lifetime);
		printf("Lifetime for access class '%s' is %dms\n", param, reply->lifetime);
	}
	else {
		lifetime.ac = htod32(ac);
		lifetime.lifetime = htod32((uint)strtol(val, 0, 0));
		err = wlu_var_setbuf(wl, cmdname, &lifetime, sizeof(lifetime));
	}

	return err;
}

#define VNDR_IE_OK_FLAGS \
	(VNDR_IE_BEACON_FLAG | VNDR_IE_PRBRSP_FLAG | VNDR_IE_ASSOCRSP_FLAG | \
	 VNDR_IE_AUTHRSP_FLAG | VNDR_IE_PRBREQ_FLAG | VNDR_IE_ASSOCREQ_FLAG | \
	 VNDR_IE_IWAPID_FLAG)

static int
wl_add_ie(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(cmd);

	return (wl_vndr_ie(wl, "add", VNDR_IE_OK_FLAGS, argv));
}

static int
wl_del_ie(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(cmd);

	return (wl_vndr_ie(wl, "del", VNDR_IE_OK_FLAGS, argv));
}

int
wl_mk_ie_setbuf(const char *command, uint32 pktflag_ok, char **argv,
	vndr_ie_setbuf_t **buf, int *buf_len)
{
	vndr_ie_setbuf_t *ie_setbuf;
	uint32 pktflag;
	int ielen, datalen, buflen, iecount;
	int err = 0;

	if (!argv[1] || !argv[2] || !argv[3]) {
		fprintf(stderr, "Too few arguments\n");
		return -1;
	}

	pktflag = (uint)strtol(argv[1], 0, 0);

	if (pktflag & ~pktflag_ok) {
		fprintf(stderr, "Invalid packet flag 0x%x (%d)\n", pktflag, pktflag);
		return -1;
	}

	ielen = atoi(argv[2]);
	if (ielen > VNDR_IE_MAX_LEN) {
		fprintf(stderr, "IE length is %d, should be <= %d\n", ielen, VNDR_IE_MAX_LEN);
		return -1;
	}
	else if (ielen < VNDR_IE_MIN_LEN) {
		fprintf(stderr, "IE length is %d, should be >= %d\n", ielen, VNDR_IE_MIN_LEN);
		return -1;
	}

	if (strlen(argv[3]) != OUI_STR_SIZE) {
		fprintf(stderr, "Invalid OUI length %d\n", (int)strlen(argv[3]));
		return -1;
	}

	datalen = ielen - VNDR_IE_MIN_LEN;
	if (datalen > 0) {
		if (!argv[4]) {
			fprintf(stderr, "Data bytes should be specified for IE of length %d",
			        ielen);
			return -1;
		}
		else {
			/* Ensure each data byte is 2 characters long */
			if ((int)strlen (argv[4]) < (datalen * 2)) {
				fprintf(stderr, "Please specify all the data bytes for this IE\n");
				return -1;
			}
		}
	}

	if (datalen == 0 && (argv[4] != NULL))
		fprintf(stderr, "Ignoring data bytes for IE of length %d", ielen);

	buflen = sizeof(vndr_ie_setbuf_t) + datalen - 1;

	ie_setbuf = (vndr_ie_setbuf_t *) malloc(buflen);

	if (ie_setbuf == NULL) {
		fprintf(stderr, "memory alloc failure\n");
		return -1;
	}

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strncpy(ie_setbuf->cmd, command, VNDR_IE_CMD_LEN - 1);
	ie_setbuf->cmd[VNDR_IE_CMD_LEN - 1] = '\0';


	/* Buffer contains only 1 IE */
	iecount = htod32(1);
	memcpy((void *)&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	/*
	 * The packet flag bit field indicates the packets that will
	 * contain this IE
	 */
	pktflag = htod32(pktflag);
	memcpy((void *)&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag,
	       &pktflag, sizeof(uint32));

	/* Now, add the IE to the buffer */
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.id = (uchar) DOT11_MNG_PROPR_ID;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len = (uchar) ielen;

	if ((err = get_oui_bytes ((uchar *)argv[3],
		&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[0]))) {
		free(ie_setbuf);
		fprintf(stderr, "Error parsing OUI arg\n");
		return err;
	}

	if (datalen > 0) {
		if ((err = get_ie_data ((uchar *)argv[4],
			&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[0],
			datalen))) {
			free(ie_setbuf);
			fprintf(stderr, "Error parsing data arg\n");
			return err;
		}
	}

	/* Copy-out */
	if (buf) {
		*buf = ie_setbuf;
		ie_setbuf = NULL;
	}
	if (buf_len)
		*buf_len = buflen;

	/* Clean-up */
	if (ie_setbuf)
		free(ie_setbuf);

	return (err);
}

static int
wl_vndr_ie(void *wl, const char *command, uint32 pktflag_ok, char **argv)
{
	vndr_ie_setbuf_t *ie_setbuf;
	int buflen;
	int err = 0;
	int ret;
	int bsscfg_idx = 0;
	int consumed = 0;

	/* parse a bsscfg_idx option if present */
	if ((ret = wl_cfg_option(argv + 1, argv[0], &bsscfg_idx, &consumed)) != 0)
		return ret;
	if (consumed)
		argv = argv + consumed;
	else
		bsscfg_idx = -1;

	if ((err = wl_mk_ie_setbuf(command, pktflag_ok, argv, &ie_setbuf, &buflen)) != 0)
		return err;

	if (bsscfg_idx == -1)
		err = wlu_var_setbuf(wl, "ie", ie_setbuf, buflen);
	else
		err = wlu_bssiovar_setbuf(wl, "ie", bsscfg_idx,
			ie_setbuf, buflen, buf, WLC_IOCTL_MAXLEN);

	free(ie_setbuf);

	return (err);
}

int
wl_list_ie(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	void *ptr;
	ie_getbuf_t param;

	BCM_REFERENCE(argv);

	param.pktflag = (uint32) -1;
	param.id = (uint8) DOT11_MNG_PROPR_ID;
	err = wlu_var_getbuf(wl, cmd->name, &param, sizeof(param), &ptr);
	if (err == 0) {
		wl_dump_ie_buf((vndr_ie_buf_t *)ptr);
	} else {
		fprintf(stderr, "Error %d getting IOVar\n", err);
	}

	return err;
}

static int
_wl_list_ie(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	const char *old = cmd->name;

	cmd->name = "ie";
	err = wl_list_ie(wl, cmd, argv);
	cmd->name = old;

	return err;
}

static void
wl_dump_ie_buf(vndr_ie_buf_t *ie_getbuf)
{
	uchar *iebuf;
	uchar *data;
	int tot_ie, pktflag, iecount, count, datalen, col;
	vndr_ie_info_t *ie_info;
	vndr_ie_t *ie;

	memcpy(&tot_ie, (void *)&ie_getbuf->iecount, sizeof(int));
	tot_ie = dtoh32(tot_ie);
	printf("Total IEs %d\n", tot_ie);

	iebuf = (uchar *)&ie_getbuf->vndr_ie_list[0];

	for (iecount = 0; iecount < tot_ie; iecount++) {
		ie_info = (vndr_ie_info_t *) iebuf;
		memcpy(&pktflag, (void *)&ie_info->pktflag, sizeof(uint32));
		pktflag = dtoh32(pktflag);
		iebuf += sizeof(uint32);

		printf("\n");

		ie = &ie_info->vndr_ie_data;
		printf("IE index = %d\n", iecount);
		printf("-----------------\n");
		printf("Pkt Flg = 0x%x\n", pktflag);
		printf("Length  = %d\n", ie->len);
		printf("OUI     = %02x:%02x:%02x\n",
			ie->oui[0], ie->oui[1], ie->oui[2]);
		printf("Data:\n");

		data = &ie->data[0];
		datalen = ie->len - VNDR_IE_MIN_LEN;
		for (count = 0; (count < datalen);) {
			for (col = 0; (col < MAX_DATA_COLS) &&
				(count < datalen); col++, count++) {
				printf("%02x ", *data++);
			}
			printf("\n");
		}

		iebuf += ie->len + VNDR_IE_HDR_LEN;
	}
}

static int
wl_rand(void *wl, cmd_t *cmd, char **argv)
{
	char *randbuf;
	uint16 randnum;
	int err;
	void *ptr;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf (wl, cmd->name, NULL, 0, &ptr)))
		return (err);

	randbuf = (char *)ptr;
	memcpy(&randnum, randbuf, sizeof(uint16));
	printf("%d\n", randnum);

	return (0);
}

#define	PRVAL(name)	pbuf += sprintf(pbuf, "%s %u ", #name, dtoh32(cnt->name))
#define	PRVALSIX(name)	pbuf += sprintf(pbuf, "%s %u ", #name, dtoh32(cnt_six->name))
#define	PRNL()		pbuf += sprintf(pbuf, "\n")

#define WL_CNT_VERSION_SIX 6
static int
wl_counters(void *wl, cmd_t *cmd, char **argv)
{
	char *statsbuf;
	wl_cnt_t *cnt;
	wl_cnt_ver_six_t *cnt_six;
	int err;
	uint i;
	char *pbuf = buf;
	void *ptr;
	uint16 ver;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf_med (wl, cmd->name, NULL, 0, &ptr)))
		return (err);

	statsbuf = (char *)ptr;

	ver = *(uint16*)statsbuf;

	if (ver > WL_CNT_T_VERSION) {
		printf("\tIncorrect version of counters struct: expected %d; got %d\n",
		       WL_CNT_T_VERSION, ver);
		return -1;
	}
	else if (ver == WL_CNT_VERSION_SIX) {
		printf("\tUse version 6 counters struct\n");
	}
	else {
		if (ver != WL_CNT_T_VERSION) {
			printf("\tIncorrect version of counters struct: expected %d; got %d\n",
			       WL_CNT_T_VERSION, ver);
			printf("\tDisplayed values may be incorrect\n");
		}
	}

	cnt_six = (wl_cnt_ver_six_t*)malloc(sizeof(wl_cnt_ver_six_t));
	if (cnt_six == NULL) {
		printf("\tCan not allocate %d bytes for counters six struct\n",
		       (int)sizeof(wl_cnt_ver_six_t));
		return (-1);
	} else
		memcpy(cnt_six, statsbuf, sizeof(wl_cnt_ver_six_t));

	cnt = (wl_cnt_t*)malloc(sizeof(wl_cnt_t));
	if (cnt == NULL) {
		printf("\tCan not allocate %d bytes for counters struct\n",
		       (int)sizeof(wl_cnt_t));
		return (-1);
	} else
		memcpy(cnt, statsbuf, sizeof(wl_cnt_t));

	/* summary stat counter line */
	PRVAL(txframe); PRVAL(txbyte); PRVAL(txretrans); PRVAL(txerror);
	PRVAL(rxframe); PRVAL(rxbyte); PRVAL(rxerror); PRNL();

	PRVAL(txprshort); PRVAL(txdmawar); PRVAL(txnobuf); PRVAL(txnoassoc);
	PRVAL(txchit); PRVAL(txcmiss); PRNL();

	PRVAL(reset); PRVAL(txserr); PRVAL(txphyerr); PRVAL(txphycrs);
	PRVAL(txfail); PRVAL(tbtt); PRNL();

	pbuf += sprintf(pbuf, "d11_txfrag %d d11_txmulti %d d11_txretry %d d11_txretrie %d\n",
		dtoh32(cnt->txfrag), dtoh32(cnt->txmulti), dtoh32(cnt->txretry),
		dtoh32(cnt->txretrie));

	pbuf += sprintf(pbuf, "d11_txrts %d d11_txnocts %d d11_txnoack %d d11_txfrmsnt %d\n",
		dtoh32(cnt->txrts), dtoh32(cnt->txnocts), dtoh32(cnt->txnoack),
		dtoh32(cnt->txfrmsnt));

	PRVAL(rxcrc); PRVAL(rxnobuf); PRVAL(rxnondata); PRVAL(rxbadds);
	PRVAL(rxbadcm); PRVAL(rxdup); PRVAL(rxfragerr); PRNL();

	PRVAL(rxrunt); PRVAL(rxgiant); PRVAL(rxnoscb); PRVAL(rxbadproto);
	PRVAL(rxbadsrcmac); PRNL();

	pbuf += sprintf(pbuf, "d11_rxfrag %d d11_rxmulti %d d11_rxundec %d\n",
		dtoh32(cnt->rxfrag), dtoh32(cnt->rxmulti), dtoh32(cnt->rxundec));

	PRVAL(rxctl); PRVAL(rxbadda); PRVAL(rxfilter); PRNL();

	pbuf += sprintf(pbuf, "rxuflo: ");
	for (i = 0; i < NFIFO; i++)
		pbuf += sprintf(pbuf, "%d ", dtoh32(cnt->rxuflo[i]));
	pbuf += sprintf(pbuf, "\n");
	PRVAL(txallfrm); PRVAL(txrtsfrm); PRVAL(txctsfrm); PRVAL(txackfrm); PRNL();
	PRVAL(txdnlfrm); PRVAL(txbcnfrm); PRVAL(txtplunfl); PRVAL(txphyerr); PRNL();
	pbuf += sprintf(pbuf, "txfunfl: ");
	for (i = 0; i < NFIFO; i++)
		pbuf += sprintf(pbuf, "%d ", dtoh32(cnt->txfunfl[i]));
	pbuf += sprintf(pbuf, "\n");

	/* WPA2 counters */
	PRNL();
	if ((cnt->version == WL_CNT_VERSION_SIX) && (cnt->version != WL_CNT_T_VERSION)) {
		PRVALSIX(tkipmicfaill); PRVALSIX(tkipicverr); PRVALSIX(tkipcntrmsr); PRNL();
		PRVALSIX(tkipreplay); PRVALSIX(ccmpfmterr); PRVALSIX(ccmpreplay); PRNL();
		PRVALSIX(ccmpundec); PRVALSIX(fourwayfail); PRVALSIX(wepundec); PRNL();
		PRVALSIX(wepicverr); PRVALSIX(decsuccess); PRVALSIX(rxundec); PRNL();
	} else {
		PRVAL(tkipmicfaill); PRVAL(tkipicverr); PRVAL(tkipcntrmsr); PRNL();
		PRVAL(tkipreplay); PRVAL(ccmpfmterr); PRVAL(ccmpreplay); PRNL();
		PRVAL(ccmpundec); PRVAL(fourwayfail); PRVAL(wepundec); PRNL();
		PRVAL(wepicverr); PRVAL(decsuccess); PRVAL(rxundec); PRNL();
	}
	PRNL();
	PRVAL(rxfrmtoolong); PRVAL(rxfrmtooshrt);
	PRVAL(rxinvmachdr); PRVAL(rxbadfcs); PRNL();
	PRVAL(rxbadplcp); PRVAL(rxcrsglitch);
	PRVAL(bphy_rxcrsglitch);
	PRVAL(bphy_badplcp);
	PRVAL(rxstrt); PRVAL(rxdfrmucastmbss); PRNL();
	PRVAL(rxmfrmucastmbss); PRVAL(rxcfrmucast);
	PRVAL(rxrtsucast); PRVAL(rxctsucast); PRNL();
	PRVAL(rxackucast); PRVAL(rxdfrmocast);
	PRVAL(rxmfrmocast); PRVAL(rxcfrmocast); PRNL();
	PRVAL(rxrtsocast); PRVAL(rxctsocast);
	PRVAL(rxdfrmmcast); PRVAL(rxmfrmmcast); PRNL();
	PRVAL(rxcfrmmcast); PRVAL(rxbeaconmbss);
	PRVAL(rxdfrmucastobss); PRVAL(rxbeaconobss); PRNL();
	PRVAL(rxrsptmout); PRVAL(bcntxcancl);
	PRVAL(rxf0ovfl); PRVAL(rxf1ovfl); PRNL();
	PRVAL(rxf2ovfl); PRVAL(txsfovfl); PRVAL(pmqovfl); PRNL();
	PRVAL(rxcgprqfrm); PRVAL(rxcgprsqovfl);
	PRVAL(txcgprsfail); PRVAL(txcgprssuc); PRNL();
	PRVAL(prs_timeout); PRVAL(rxnack); PRVAL(frmscons);
	PRVAL(txnack); PRVAL(txglitch_nack); PRNL();
	PRVAL(txburst); PRVAL(txphyerror); PRNL();

	if ((cnt->version == WL_CNT_VERSION_SIX) && (cnt->version != WL_CNT_T_VERSION)) {
		PRVALSIX(txchanrej); PRNL();
		/* per-rate receive counters */
		PRVALSIX(rx1mbps); PRVALSIX(rx2mbps); PRVALSIX(rx5mbps5); PRNL();
		PRVALSIX(rx6mbps); PRVALSIX(rx9mbps); PRVALSIX(rx11mbps); PRNL();
		PRVALSIX(rx12mbps); PRVALSIX(rx18mbps); PRVALSIX(rx24mbps); PRNL();
		PRVALSIX(rx36mbps); PRVALSIX(rx48mbps); PRVALSIX(rx54mbps); PRNL();

		PRVALSIX(pktengrxducast); PRVALSIX(pktengrxdmcast); PRNL();

		PRVALSIX(txmpdu_sgi); PRVALSIX(rxmpdu_sgi); PRVALSIX(txmpdu_stbc);
		PRVALSIX(rxmpdu_stbc); PRNL();
	} else {
		PRVAL(txchanrej); PRNL();
		if (cnt->version >= 4) {
			/* per-rate receive counters */
			PRVAL(rx1mbps); PRVAL(rx2mbps); PRVAL(rx5mbps5); PRNL();
			PRVAL(rx6mbps); PRVAL(rx9mbps); PRVAL(rx11mbps); PRNL();
			PRVAL(rx12mbps); PRVAL(rx18mbps); PRVAL(rx24mbps); PRNL();
			PRVAL(rx36mbps); PRVAL(rx48mbps); PRVAL(rx54mbps); PRNL();
		}

		if (cnt->version >= 5) {
			PRVAL(pktengrxducast); PRVAL(pktengrxdmcast); PRNL();
		}

		if (cnt->version >= 6) {
			PRVAL(txmpdu_sgi); PRVAL(rxmpdu_sgi); PRVAL(txmpdu_stbc);
			PRVAL(rxmpdu_stbc); PRNL();
		}

		if (cnt->version >= 8) {
			PRVAL(reinit); PRNL();
			if (cnt->length >= OFFSETOF(wl_cnt_t, cso_passthrough) + sizeof(uint32)) {
				PRVAL(cso_normal);
				PRVAL(cso_passthrough);
				PRNL();
			}
			PRVAL(chained); PRVAL(chainedsz1); PRVAL(unchained); PRVAL(maxchainsz);
			PRVAL(currchainsz); PRNL();
#if defined(DSLCPE) && defined(PKTC)
			PRVAL(enet); PRVAL(fcache); PRVAL(proto); PRNL();
			PRVAL(maxtxchainsz); PRVAL(txcurrchainsz); PRNL();
#if defined(BCMDBG)
			PRVAL(txchainsz5); PRVAL(txchainsz16); PRVAL(txchainsz32); PRVAL(txchainsz64); PRVAL(txchainsz128); PRVAL(txchainsz128up); PRNL();
{
			uint32 total_pkts=0, txchainsz5per=0, txchainsz16per=0, txchainsz32per=0, txchainsz64per=0, txchainsz128per=0, txchainsz128up_per=0;
			total_pkts = cnt->txchainsz5 + cnt->txchainsz16 + cnt->txchainsz32 + cnt->txchainsz64 + cnt->txchainsz128 + cnt->txchainsz128up;
			if (total_pkts != 0) {
				txchainsz5per = (cnt->txchainsz5 * 100) / total_pkts;
				txchainsz16per = (cnt->txchainsz16 * 100) / total_pkts;
				txchainsz32per = (cnt->txchainsz32 * 100) / total_pkts;
				txchainsz64per = (cnt->txchainsz64 * 100) / total_pkts;
				txchainsz128per = (cnt->txchainsz128 * 100) / total_pkts;
				txchainsz128up_per = (cnt->txchainsz128up * 100) / total_pkts;
				printf("dblchain5per %d  dblchain16per %d dblchain32per %d dblchain64per %d dblchain128per %d dblchain128up_per %d\n", 
					txchainsz5per, txchainsz16per, txchainsz32per, txchainsz64per, txchainsz128per, txchainsz128up_per);
			}
}
#endif
#endif
		}
	}

	pbuf += sprintf(pbuf, "\n");
	fputs(buf, stdout);

	if (cnt)
		free(cnt);

	if (cnt_six)
		free(cnt_six);

	return (0);
}

static int
wl_delta_stats(void *wl, cmd_t *cmd, char **argv)
{
	char *statsbuf;
	wl_delta_stats_t *cnt;
	int err;
	char *pbuf = buf;
	void *ptr;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf_med (wl, cmd->name, NULL, 0, &ptr)))
		return (err);

	statsbuf = (char *)ptr;

	cnt = (wl_delta_stats_t*)malloc(sizeof(wl_delta_stats_t));
	if (cnt == NULL) {
		printf("\tCan not allocate %d bytes for wl delta stats struct\n",
		       (int)sizeof(wl_delta_stats_t));
		return (-1);
	}
	memcpy(cnt, statsbuf, sizeof(wl_delta_stats_t));
	cnt->version = dtoh16(cnt->version);
	cnt->length = dtoh16(cnt->length);

	if (cnt->version != WL_DELTA_STATS_T_VERSION) {
		printf("\tIncorrect version of delta stats struct: expected %d; got %d\n",
			WL_DELTA_STATS_T_VERSION, cnt->version);
		free(cnt);
		return -1;
	}

	PRVAL(txframe); PRVAL(txbyte); PRVAL(txretrans); PRVAL(txfail); PRNL();

	PRVAL(rxframe); PRVAL(rxbyte); PRNL();

	PRVAL(rx1mbps); PRVAL(rx2mbps); PRVAL(rx5mbps5); PRVAL(rx6mbps); PRNL();
	PRVAL(rx9mbps); PRVAL(rx11mbps); PRVAL(rx12mbps); PRVAL(rx18mbps); PRNL();
	PRVAL(rx24mbps); PRVAL(rx36mbps); PRVAL(rx48mbps); PRVAL(rx54mbps); PRNL();

	pbuf += sprintf(pbuf, "\n");
	fputs(buf, stdout);

	if (cnt != NULL)
		free(cnt);

	return (0);
}

static int
wl_wme_counters(void *wl, cmd_t *cmd, char **argv)
{
	char *statsbuf;
	wl_wme_cnt_t cnt;
	int err;
	void *ptr;
	char *pbuf = buf;
	uint ac;
	int ap_mode = 0;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf_sm (wl, cmd->name, NULL, 0, &ptr)))
		return (err);

	statsbuf = (char *)ptr;
	memcpy(&cnt, statsbuf, sizeof(cnt));
	cnt.version = dtoh16(cnt.version);
	cnt.length = dtoh16(cnt.length);

	if (cnt.version != WL_WME_CNT_VERSION) {
		printf("\tIncorrect version of counters struct: expected %d; got %d\n",
			WL_WME_CNT_VERSION, cnt.version);
		return -1;
	}

	if ((err = wlu_get(wl, WLC_GET_AP, &ap_mode, sizeof(ap_mode))) < 0) {
		return -1;
	}
	ap_mode = dtoh32(ap_mode);

	/* summary stat counter line */
	for (ac = AC_BE; ac < AC_COUNT; ac++) {
		pbuf += sprintf(pbuf, "\n%s: tx frames: %d bytes: %d failed frames: %d "
		                "failed bytes: %d\n",
		                ac_names[ac], dtoh32(cnt.tx[ac].packets), dtoh32(cnt.tx[ac].bytes),
		                dtoh32(cnt.tx_failed[ac].packets), dtoh32(cnt.tx_failed[ac].bytes));
		pbuf += sprintf(pbuf, "       rx frames: %d bytes: %d failed frames: %d "
		                "failed bytes: %d\n", dtoh32(cnt.rx[ac].packets),
		                dtoh32(cnt.rx[ac].bytes), dtoh32(cnt.rx_failed[ac].packets),
		                dtoh32(cnt.rx_failed[ac].bytes));

		if (ap_mode)
			pbuf += sprintf(pbuf, "       foward frames: %d bytes: %d \n",
			                dtoh32(cnt.forward[ac].packets),
			                dtoh32(cnt.forward[ac].bytes));

		pbuf += sprintf(pbuf, "       tx frames time expired: %d \n",
		                dtoh32(cnt.tx_expired[ac].packets));
	}
	pbuf += sprintf(pbuf, "\n");
	fputs(buf, stdout);
	return (0);
}

static int
wl_devpath(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	void *ptr;
	char *pbuf = buf;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf_sm (wl, cmd->name, NULL, 0, &ptr)))
		return (err);

	pbuf += strlen(buf);
	sprintf(pbuf, "\n");
	fputs(buf, stdout);
	return (0);
}

static int
wl_diag(void *wl, cmd_t *cmd, char **argv)
{
	uint testindex;
	int buflen, err;
	char *param;
	uint32 testresult;

	if (!*++argv) {
		printf(" Usage: %s testindex[1-4]\n", cmd->name);
		return -1;
	}

	testindex = atoi(*argv);

	strcpy(buf, "diag");
	buflen = strlen(buf) + 1;
	param = (char *)(buf + buflen);
	testindex = htod32(testindex);
	memcpy(param, (char*)&testindex, sizeof(testindex));

	if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return err;

	testresult = *(uint32 *)buf;
	testindex = dtoh32(testindex);
	testresult = dtoh32(testresult);
	if (testresult != 0) {
		printf("\ndiag test %d failed(error code %d)\n", testindex, testresult);
	} else
		printf("\ndiag test %d passed\n", testindex);

	return (0);
}

static int
wl_phy_rssiant(void *wl, cmd_t *cmd, char **argv)
{
	uint32 antindex;
	int buflen, err;
	char *param;
	int16 antrssi;

	if (!*++argv) {
		printf(" Usage: %s antenna_index[0-3]\n", cmd->name);
		return -1;
	}

	antindex = htod32(atoi(*argv));

	strcpy(buf, "nphy_rssiant");
	buflen = strlen(buf) + 1;
	param = (char *)(buf + buflen);
	memcpy(param, (char*)&antindex, sizeof(antindex));

	if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return err;

	antindex = dtoh32(antindex);
	antrssi = dtoh16(*(int16 *)buf);
	printf("\nnphy_rssiant ant%d = %d\n", antindex, antrssi);

	return (0);
}

static int
get_oui_bytes(uchar *oui_str, uchar *oui)
{
	int idx;
	uchar val;
	uchar *src, *dest;
	char hexstr[3];

	src = oui_str;
	dest = oui;

	for (idx = 0; idx < MAX_OUI_SIZE; idx++) {
		hexstr[0] = src[0];
		hexstr[1] = src[1];
		hexstr[2] = '\0';

		val = (uchar) strtoul(hexstr, NULL, 16);

		*dest++ = val;
		src += 2;

		if ((idx < (MAX_OUI_SIZE - 1)) && (*src++ != ':'))
			return -1;
	}

	return 0;
}

static int
get_ie_data(uchar *data_str, uchar *ie_data, int len)
{
	uchar *src, *dest;
	uchar val;
	int idx;
	char hexstr[3];

	src = data_str;
	dest = ie_data;

	for (idx = 0; idx < len; idx++) {
		hexstr[0] = src[0];
		hexstr[1] = src[1];
		hexstr[2] = '\0';

		val = (uchar) strtoul(hexstr, NULL, 16);

		*dest++ = val;
		src += 2;
	}

	return 0;
}

static int
hexstrtobitvec(const char *cp, uchar *bitvec, int veclen)
{
	uchar value = 0;
	int nibble;		/* index of current hex-format nibble to process */
	int even;		/* 1 if even number of nibbles, 0 if odd number */
	int i = 0;

	if (cp[0] == '0' && cp[1] == 'x')
		cp += 2;

	memset(bitvec, '\0', veclen);
	nibble = strlen(cp);
	if (!nibble)
		return -1;
	even = ((nibble % 2) == 0);

	/* convert from right to left (lsb is rightmost byte) */
	--nibble;
	while (nibble >= 0 && i < veclen && (isxdigit((int)cp[nibble]) &&
		(value = isdigit((int)cp[nibble]) ? cp[nibble]-'0' :
		(islower((int)cp[nibble]) ? toupper((int)cp[nibble]) : cp[nibble])-'A'+10) < 16)) {
		if (even == ((nibble+1) % 2)) {
			bitvec[i] += value*16;
			++i;
		} else
			bitvec[i] = value;
		--nibble;
	}

	return ((nibble == -1 && i <= veclen) ? 0 : -1);
}

static int
wl_bitvec128(void *wl, cmd_t *cmd, char **argv)
{
	char *vbuf;
	int err;
	uchar bitvec[16];
	bool skipzeros;
	int i;

	err = 0;
	skipzeros = TRUE;

	/* set */
	if (argv[1]) {
		memset(bitvec, '\0', sizeof(bitvec));
		if (!(err = hexstrtobitvec(argv[1], bitvec, sizeof(bitvec))))
			err = wlu_var_setbuf(wl, cmd->name, bitvec, sizeof(bitvec));
	}
	/* get */
	else {
		void *ptr;

		memset(buf, '\0', WLC_IOCTL_MAXLEN);
		if (!(err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr))) {
			vbuf = (char *)ptr;
			printf("0x");
			for (i = (sizeof(bitvec) - 1); i >= 0; i--) {
				if (vbuf[i] || (i == 0))
					skipzeros = FALSE;
				if (skipzeros)
					continue;
				printf("%02x", vbuf[i] & 0xff);
			}
			printf("\n");
		}
	}

	return (err);
}

static int
wl_auto_channel_sel(void *wl, cmd_t *cmd, char **argv)
{
	/*
	* The following condition(s) must be met when Auto Channel Selection
	* is enabled.
	*  - the I/F is up (change radio channel requires it is up?)
	*  - the AP must not be associated (setting SSID to empty should
	*    make sure it for us)
	*/
	int chosen = 0;
	wl_uint32_list_t request;
	int ret = 0;
#ifdef DSLCPE
	int i;
#endif

	if (!*++argv) {
		ret = wlu_get(wl, cmd->get, &chosen, sizeof(chosen));
		chosen = wl_chspec32_from_driver(chosen);
		if (ret >= 0 && chosen != 0) {
			wf_chspec_ntoa((chanspec_t)chosen, buf);
			printf("%s (0x%x)\n", buf, chosen);
			return 0;
		}
		else {
			if (chosen == 0)
				printf("invalid chanspec (0x%x)\n", chosen);
		}
	} else {
		if (atoi(*argv) == 1) {
			request.count = htod32(0);
			ret = wlu_set(wl, cmd->set, &request, sizeof(request));
#ifdef DSLCPE

			if (!ret) {
				sleep_ms(1000);
				for (i = 0; i < 100; i++) {
						ret = wlu_get(wl, cmd->get, &chosen, sizeof(chosen));
					if (!ret)
						break;
					sleep_ms(100);
				}
			}
			printf("chanspec 0x%4x selected \n", chosen);
#endif
		} else if (atoi(*argv) == 2) {
			ret = wlu_get(wl, cmd->get, &chosen, sizeof(chosen));
			if (ret >= 0 && chosen != 0)
				ret = wlu_iovar_setint(wl, "chanspec", (int)chosen);
		} else {
			ret = -1;
		}
	}
	return ret;
}

static int
wl_varstr(void *wl, cmd_t *cmd, char **argv)
{
	int error;
	char *str;

	if (!*++argv) {
		void *ptr;

		if ((error = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return (error);

		str = (char *)ptr;
		printf("%s\n", str);
		return (0);
	} else {
		str = *argv;
		/* str length include NULL */
		return wlu_var_setbuf(wl, cmd->name, str, (strlen(str)+1));
	}
}

/* Return TRUE if it's one of the wc cmds. If WC_TOOL is not defined,
 * it'll return TRUE by default so all the commands are allowed.
 */
bool wc_cmd_check(const char *cmd_name)
{
	uint j;
	if (wc_cmds == NULL)
		return TRUE;

	for (j = 0; j < ARRAYSIZE(wc_cmds); j++)
		if (strcmp(wc_cmds[j], cmd_name) == 0)
			return TRUE;
	return FALSE;
}

#define NUM_TSLIST_ARG 3		/* minimum number of arguments required for TSLIST */
#define NUM_TSLIST_PER_EA_ARG 3	/* minimum number of arguments required for TSLIST */
#define MIN_NUM_DELTS_ARG 4		/* minimum number of arguments required for DELTS */
#define MIN_NUM_DELTS_EA_ARG 5	/* minimum number of arguments required for DELTS */
#define MIN_NUM_ADDTS_ARG 20	/* minimum number of arguments required for ADDTS */
#define PERIODIC_TRAFFIC 1		/* Periodic traffic type */
#define VO_TID (0 << 1)			/* voice TID */
#define VI_TID (1 << 1)			/* signal TID */
#define UPLINK_DIRECTION (0 << 5)	/* uplink direction traffic stream */
#define DOWNLINK_DIRECTION (1 << 5)	/* downlink direction traffic stream */
#define BI_DIRECTION (3 << 5)	/* bi direction traffic stream */
#define EDCA_ACCESS (1 << 7)	/* EDCA access policy */
#define UAPSD_PSB (1 << 2)		/* U-APSD power saving behavior */
#define VO_USER_PRIO (6 << 3)	/* voice user priority */
#define VI_USER_PRIO (4 << 3)	/* signal user priority */
#define TID_SHIFT 1				/* TID Shift */
#define UP_SHIFT 3				/* UP Shift */

static void
wl_cac_format_tspec_htod(tspec_arg_t *tspec_arg)
{
	tspec_arg->version = htod16(tspec_arg->version);
	tspec_arg->length = htod16(tspec_arg->length);
	tspec_arg->flag = htod32(tspec_arg->flag);
	tspec_arg->nom_msdu_size = htod16(tspec_arg->nom_msdu_size);
	tspec_arg->max_msdu_size = htod16(tspec_arg->max_msdu_size);
	tspec_arg->min_srv_interval = htod32(tspec_arg->min_srv_interval);
	tspec_arg->max_srv_interval = htod32(tspec_arg->max_srv_interval);
	tspec_arg->inactivity_interval = htod32(tspec_arg->inactivity_interval);
	tspec_arg->suspension_interval = htod32(tspec_arg->suspension_interval);
	tspec_arg->srv_start_time = htod32(tspec_arg->srv_start_time);
	tspec_arg->min_data_rate = htod32(tspec_arg->min_data_rate);
	tspec_arg->mean_data_rate = htod32(tspec_arg->mean_data_rate);
	tspec_arg->peak_data_rate = htod32(tspec_arg->peak_data_rate);
	tspec_arg->max_burst_size = htod32(tspec_arg->max_burst_size);
	tspec_arg->delay_bound = htod32(tspec_arg->delay_bound);
	tspec_arg->min_phy_rate = htod32(tspec_arg->min_phy_rate);
	tspec_arg->surplus_bw = htod16(tspec_arg->surplus_bw);
	tspec_arg->medium_time = htod16(tspec_arg->medium_time);
}

static void
wl_cac_format_tspec_dtoh(tspec_arg_t *tspec_arg)
{
	tspec_arg->version = dtoh16(tspec_arg->version);
	tspec_arg->length = dtoh16(tspec_arg->length);
	tspec_arg->flag = dtoh32(tspec_arg->flag);
	tspec_arg->nom_msdu_size = dtoh16(tspec_arg->nom_msdu_size);
	tspec_arg->max_msdu_size = dtoh16(tspec_arg->max_msdu_size);
	tspec_arg->min_srv_interval = dtoh32(tspec_arg->min_srv_interval);
	tspec_arg->max_srv_interval = dtoh32(tspec_arg->max_srv_interval);
	tspec_arg->inactivity_interval = dtoh32(tspec_arg->inactivity_interval);
	tspec_arg->suspension_interval = dtoh32(tspec_arg->suspension_interval);
	tspec_arg->srv_start_time = dtoh32(tspec_arg->srv_start_time);
	tspec_arg->min_data_rate = dtoh32(tspec_arg->min_data_rate);
	tspec_arg->mean_data_rate = dtoh32(tspec_arg->mean_data_rate);
	tspec_arg->peak_data_rate = dtoh32(tspec_arg->peak_data_rate);
	tspec_arg->max_burst_size = dtoh32(tspec_arg->max_burst_size);
	tspec_arg->delay_bound = dtoh32(tspec_arg->delay_bound);
	tspec_arg->min_phy_rate = dtoh32(tspec_arg->min_phy_rate);
	tspec_arg->surplus_bw = dtoh16(tspec_arg->surplus_bw);
	tspec_arg->medium_time = dtoh16(tspec_arg->medium_time);

}

static void wl_cac_addts_usage(void)
{
	fprintf(stderr, "Too few arguments\n");
	fprintf(stderr, "wl cac_addts ver dtoken tid dir psb up a b c d e ...\n");
	fprintf(stderr, "\twhere ver is the structure version\n");
	fprintf(stderr, "\twhere dtoken is the dialog token [range 1-255]\n");
	fprintf(stderr, "\twhere tid is the tspec identifier [range 0-7]\n");
	fprintf(stderr, "\twhere dir is direction [uplink | downlink | bi-directional]\n");
	fprintf(stderr, "\twhere psb is power save mode [legacy|U-APSD]\n");
	fprintf(stderr, "\twhere up is user priority [range 0-7]\n");
	fprintf(stderr, "\twhere a is the nominal MSDU size\n");
	fprintf(stderr, "\twhere b is bool for fixed size msdu [ 0 and 1]\n");
	fprintf(stderr, "\twhere c is the maximum MSDU size\n");
	fprintf(stderr, "\twhere d is the minimum service interval\n");
	fprintf(stderr, "\twhere e is the maximum service interval\n");
	fprintf(stderr, "\twhere f is the inactivity interval\n");
	fprintf(stderr, "\twhere g is the suspension interval\n");
	fprintf(stderr, "\twhere h is the minimum data rate\n");
	fprintf(stderr, "\twhere i is the mean data rate\n");
	fprintf(stderr, "\twhere j is the peak data rate\n");
	fprintf(stderr, "\twhere k is the max burst size\n");
	fprintf(stderr, "\twhere l is the delay bound\n");
	fprintf(stderr, "\twhere m is the surplus bandwidth [fixed point notation]\n");
	fprintf(stderr, "\twhere n is the minimum PHY rate\n");
}

static void wl_cac_delts_usage(void)
{
	fprintf(stderr, "Too few arguments\n");
	fprintf(stderr, "wl cac_delts ver a b c \n");
	fprintf(stderr, "\twhere ver is the tspec version\n");
	fprintf(stderr, "\twhere a is byte[0] of tsinfo (bits 0-7)\n");
	fprintf(stderr, "\twhere b is byte[1] of tsinfo (bits 8-15)\n");
	fprintf(stderr, "\twhere c is byte[2] of tsinfo (bits 16-23)\n");
}

static int
wl_cac(void *wl, cmd_t *cmd, char **argv)
{
	int err = -1;
	int ap_mode = 0;
	int cmd_type = 0;
	tspec_arg_t tspec_arg;
	char *endptr = NULL;
	uint buflen;
	char *arg1, *user_argv;
	uint8 direction = BI_DIRECTION;
	uint8 user_tid, user_prio, user_psb;
	uint fixed;

	if ((wlu_get(wl, WLC_GET_AP, &ap_mode, sizeof(ap_mode))))
		return err;
	ap_mode = dtoh32(ap_mode);

	if (ap_mode) {
		fprintf(stderr, "This command can only be executed on the STA\n");
		return err;
	}

	if (!strcmp(*argv, "cac_addts"))
		cmd_type = 1;
	else if (!strcmp(*argv, "cac_delts"))
		cmd_type = 2;
	else {
		fprintf(stderr, "unknown command\n");
		return BCME_BADARG;
	}

	/* eat command name */
	if (!*++argv) {
		(cmd_type == 1) ? wl_cac_addts_usage():wl_cac_delts_usage();
		return BCME_BADARG;
	}

	buflen = sizeof(tspec_arg_t);
	memset((uint8 *)&tspec_arg, 0, buflen);

	/* get direction option */
	arg1 = *argv;

	/* Unidirectional DL/UL */
	if (!strcmp(arg1, "UDL") || (!strcmp(arg1, "UUL")))
		direction = DOWNLINK_DIRECTION;

	if (cmd_type == 1) {
			uint argc = 0;

			/* arg count */
			while (argv[argc])
				argc++;

			/* required argments */
			if (argc < MIN_NUM_ADDTS_ARG) {
				wl_cac_addts_usage();
				return BCME_BADARG;
			}

			tspec_arg.length = sizeof(tspec_arg_t) - (2 * sizeof(uint16));
			tspec_arg.version = (uint16)strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			tspec_arg.dialog_token = (uint8)strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			user_tid = (uint8)strtol(*argv++, &endptr, 0);
			user_tid <<= TID_SHIFT;
			if (*endptr != '\0')
				goto error;

			/* store the pointer for parsing */
			user_argv = *argv++;

			if (!strcmp(user_argv, "uplink"))
				direction = UPLINK_DIRECTION;
			else if (!strcmp(user_argv, "downlink"))
				direction = DOWNLINK_DIRECTION;
			else if (!strcmp(user_argv, "bi-directional"))
				direction = BI_DIRECTION;
			else
				goto error;

			/* store the pointer for parsing */
			user_argv = *argv++;

			if (!strcmp(user_argv, "legacy"))
				user_psb = 0;
			else if (!strcmp(user_argv, "U-APSD"))
				user_psb = UAPSD_PSB;
			else
				goto error;

			user_prio = (uint8)strtol(*argv++, &endptr, 0);
			user_prio <<= UP_SHIFT;
			if (*endptr != '\0')
				goto error;

			tspec_arg.tsinfo.octets[0] = (uint8)(user_tid |
				direction | EDCA_ACCESS);

			tspec_arg.tsinfo.octets[1] = (uint8)(user_prio | user_psb);
			tspec_arg.tsinfo.octets[2] = 0x00;

			tspec_arg.nom_msdu_size = (uint16)strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			fixed = (uint)strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			if (fixed == 1)
				tspec_arg.nom_msdu_size |= 0x8000;

			tspec_arg.max_msdu_size = (uint16)strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			tspec_arg.min_srv_interval = strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			tspec_arg.max_srv_interval = strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			tspec_arg.inactivity_interval = strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			tspec_arg.suspension_interval = strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			tspec_arg.min_data_rate = strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			tspec_arg.mean_data_rate = strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			tspec_arg.peak_data_rate = strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			tspec_arg.max_burst_size = strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			tspec_arg.delay_bound = strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			tspec_arg.surplus_bw = (uint16)strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			tspec_arg.min_phy_rate = strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;
			printf("Setting min_phy_rate to 0x%x\n", tspec_arg.min_phy_rate);
	} else {
			uint argc = 0;

			/* arg count */
			while (argv[argc])
				argc++;

			/* required argments */
			if (argc < MIN_NUM_DELTS_ARG) {
				wl_cac_delts_usage();
				return BCME_BADARG;
			}

			tspec_arg.length = sizeof(tspec_arg_t) - (2 * sizeof(uint16));
			tspec_arg.version = (uint16)strtol(*argv++, &endptr, 0);

			if (*endptr != '\0')
				goto error;

			tspec_arg.tsinfo.octets[0] = (uint8)strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			tspec_arg.tsinfo.octets[1] = (uint8)strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;

			tspec_arg.tsinfo.octets[2] = (uint8)strtol(*argv++, &endptr, 0);
			if (*endptr != '\0')
				goto error;
	}

	wl_cac_format_tspec_htod(&tspec_arg);
	err = wlu_var_setbuf(wl, cmd->name, &tspec_arg, buflen);

error:
	return err;
}

/* get a list of traffic stream (TSINFO) in driver */
static int
wl_tslist(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr;
	int i;
	int ap_mode, err = -1;
	struct tslist *tslist;

	UNUSED_PARAMETER(argv);

	if ((wlu_get(wl, WLC_GET_AP, &ap_mode, sizeof(ap_mode))))
		return err;
	ap_mode = dtoh32(ap_mode);

	if (ap_mode) {
		fprintf(stderr, "This command can only be executed on the STA\n");
		return err;
	}

	if ((err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return err;

	tslist = (struct tslist *)ptr;
	tslist->count = dtoh32(tslist->count);
	for (i = 0; i < tslist->count; i++)
		printf("tsinfo 0x%02X 0x%02X 0x%02X  TID %d  User Prio %d  Direction %d\n",
		       tslist->tsinfo[i].octets[0],
		       tslist->tsinfo[i].octets[1],
		       tslist->tsinfo[i].octets[2],
		       WLC_CAC_GET_TID(tslist->tsinfo[i]),
		       WLC_CAC_GET_USER_PRIO(tslist->tsinfo[i]),
		       WLC_CAC_GET_DIR(tslist->tsinfo[i]));

	return 0;
}

/* get specific TSPEC in driver */
static int
wl_tspec(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr;
	int ap_mode, err = -1;
	tspec_arg_t *ts, tspec_arg;
	char *temp = NULL;
	uint argc = 0;

	if ((wlu_get(wl, WLC_GET_AP, &ap_mode, sizeof(ap_mode))))
		return err;
	ap_mode = dtoh32(ap_mode);

	if (ap_mode) {
		fprintf(stderr, "This command can only be executed on the STA\n");
		return err;
	}

	/* eat command name */
	argv++;

	/* arg count */
	while (argv[argc])
		argc++;

	/* required argments */
	if (argc < NUM_TSLIST_ARG) {
		fprintf(stderr, "Too few arguments\n");
		fprintf(stderr, "wl cac_tspec 0xaa 0xbb 0xcc \n");
		fprintf(stderr, "\twhere 0xaa is byte[0] of tsinfo (bits 0-7)\n");
		fprintf(stderr, "\twhere 0xbb is byte[1] of tsinfo (bits 8-15)\n");
		fprintf(stderr, "\twhere 0xcc is byte[2] of tsinfo (bits 16-23)\n");
		return BCME_BADARG;
	}

	memset((uint8 *)&tspec_arg, 0, sizeof(tspec_arg_t));

	tspec_arg.tsinfo.octets[0] = (uint8)strtol(*argv++, &temp, 0);
	if (*temp != '\0')
		goto error;

	tspec_arg.tsinfo.octets[1] = (uint8)strtol(*argv++, &temp, 0);
	if (*temp != '\0')
		goto error;

	tspec_arg.tsinfo.octets[2] = (uint8)strtol(*argv++, &temp, 0);
	if (*temp != '\0')
		goto error;

	if ((err = wlu_var_getbuf(wl, cmd->name, &tspec_arg, sizeof(tspec_arg_t), &ptr)) < 0)
		return err;

	ts = (tspec_arg_t *)ptr;
	wl_cac_format_tspec_dtoh(ts);
	wl_print_tspec(ts);
	return 0;
error:
	return err;
}

/* get/set max bandwidth for each access category in ap */
static int
wme_maxbw_params(void *wl, cmd_t *cmd, char **argv)
{
	wme_max_bandwidth_t cur_params, new_params;
	char *val_p, *ac_str, *param;
	int buflen;
	int aci;
	int err;
	int val;
	int ap_mode = 0;

	argv++;

	if ((err = wlu_get(wl, WLC_GET_AP, &ap_mode, sizeof(ap_mode))) < 0)
		return err;

	if (!ap_mode) {
		printf("%s: AP only\n", cmd->name);
		return -1;
	}

	buflen = WLC_IOCTL_MAXLEN;

	/* get the current max bandwidth, using buf as an input buffer. */
	strcpy(buf, "wme_maxbw_params");
	if ((err = wlu_get(wl, WLC_GET_VAR, &buf[0], buflen)) < 0) {
		return err;
	}

	/* cache the current values */
	memcpy(&cur_params, buf, sizeof(wme_max_bandwidth_t));

	if ((ac_str = *argv) == NULL) {
		printf("WME bandwidth limit: \n");
		for (aci = 0; aci < AC_COUNT; aci++) {
			printf("%s: bandwidth limit %d\n", ac_names[aci],
				cur_params.ac[aci]);
		}
	} else {
		/* preload new values with current values */
		memcpy(&new_params, &cur_params, sizeof(new_params));
		while ((param = *argv++) != NULL) {
			if ((val_p = *argv++) == NULL) {
				printf("Need value following %s\n", param);
				return USAGE_ERROR;
			}

			val = (int)strtoul(val_p, NULL, 0);

			if (!strcmp(param, "be")) {
				new_params.ac[AC_BE] = (uint32)val;
			} else if (!strcmp(param, "bk")) {
				new_params.ac[AC_BK] = (uint32)val;
			} else if (!strcmp(param, "vi")) {
				new_params.ac[AC_VI] = (uint32)val;
			} else if (!strcmp(param, "vo")) {
				new_params.ac[AC_VO] = (uint32)val;
			} else {
				printf("Unknown access category: %s\n", param);
				return USAGE_ERROR;
			}
		}

		strcpy(buf, "wme_maxbw_params");
		memcpy(buf + strlen(buf) + 1, &new_params, sizeof(wme_max_bandwidth_t));
		err = wlu_set(wl, WLC_SET_VAR, &buf[0], buflen);

	}

	return 0;
}

/* get the tspec list for the given station */
static int
wl_tslist_ea(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr;
	int i;
	int ap_mode, err = -1;
	struct tslist *tslist;
	scb_val_t scb_val;
	int ret = 0;


	if (!*++argv) {
		printf("MAC address must be specified\n");
		ret = -1;
	} else if (!wl_ether_atoe(*argv, &scb_val.ea)) {
		printf("Malformed MAC address parameter\n");
		ret = -1;
	}
	if ((ret != 0) || (wlu_get(wl, WLC_GET_AP, &ap_mode, sizeof(ap_mode))))
		return err;

	ap_mode = dtoh32(ap_mode);

	if ((err = wlu_var_getbuf(wl, cmd->name, &scb_val.ea, ETHER_ADDR_LEN, &ptr)) < 0)
		return err;

	tslist = (struct tslist *)ptr;

	for (i = 0; i < tslist->count; i++)
		printf("tsinfo 0x%02X 0x%02X 0x%02X  TID %d  User Prio %d  Direction %d\n",
		       tslist->tsinfo[i].octets[0],
		       tslist->tsinfo[i].octets[1],
		       tslist->tsinfo[i].octets[2],
		       WLC_CAC_GET_TID(tslist->tsinfo[i]),
		       WLC_CAC_GET_USER_PRIO(tslist->tsinfo[i]),
		       WLC_CAC_GET_DIR(tslist->tsinfo[i]));

	return 0;

}

/* get specific TSPEC for a STA */
static int
wl_tspec_ea(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr;
	int err = -1;
	tspec_per_sta_arg_t tsea;
	tspec_arg_t *ts;
	char *temp;
	uint argc = 0;

	/* eat command name */
	argv++;

	while (argv[argc])
		argc++;

	/* required argments */
	if (argc < (NUM_TSLIST_PER_EA_ARG + 1)) {
		fprintf(stderr, "Too few arguments\n");
		fprintf(stderr, "wl cac_tspec 0xaa 0xbb 0xcc xx:xx:xx:xx:xx:xx\n");
		fprintf(stderr, "\twhere 0xaa is byte[0] of tsinfo (bits 0-7)\n");
		fprintf(stderr, "\twhere 0xbb is byte[1] of tsinfo (bits 8-15)\n");
		fprintf(stderr, "\twhere 0xcc is byte[2] of tsinfo (bits 16-23)\n");
		fprintf(stderr, "\twhere xx:xx:xx:xx:xx:xx is mac address )\n");
		return BCME_BADARG;
	}

	memset((uint8 *)&tsea, 0, sizeof(tspec_per_sta_arg_t));

	ts = &tsea.ts;

	ts->tsinfo.octets[0] = (uint8)strtol(*argv++, &temp, 0);
	if (*temp != '\0')
		goto error;

	ts->tsinfo.octets[1] = (uint8)strtol(*argv++, &temp, 0);
	if (*temp != '\0')
		goto error;

	ts->tsinfo.octets[2] = (uint8)strtol(*argv++, &temp, 0);
	if (*temp != '\0')
		goto error;

	/* add the ether address after tsinfo */
	if (!*argv) {
		printf("MAC address must be specified\n");
		return -1;
	} else if (!wl_ether_atoe(*argv, &tsea.ea)) {
		printf("Malformed MAC address parameter\n");
		return -1;
	}

	if ((err = wlu_var_getbuf(wl, cmd->name, &tsea, sizeof(tspec_per_sta_arg_t), &ptr)) < 0)
		return err;

	ts = (tspec_arg_t *)ptr;
	wl_cac_format_tspec_dtoh(ts);
	wl_print_tspec(ts);
	return 0;

error:
	return err;
}

static const uint8 wlu_wme_fifo2ac[] = { AC_BK, AC_BE, AC_VI, AC_VO, AC_BE,
             AC_BE };
static const uint8 wlu_prio2fifo[NUMPRIO] = {
	0,	/* 0	BE	AC_BE	Best Effort */
	1,	/* 1	BK	AC_BK	Background */
	2,	/* 2	--	AC_BK	Background */
	3,	/* 3	EE	AC_BE	Best Effort */
	4,	/* 4	CL	AC_VI	Video */
	5,	/* 5	VI	AC_VI	Video */
	6,	/* 6	VO	AC_VO	Voice */
	7	/* 7	NC	AC_VO	Voice */
};
#define WME_PRIO2AC(prio)	wlu_wme_fifo2ac[wlu_prio2fifo[(prio)]]

static void
wl_print_tspec(tspec_arg_t *ts)
{
	const char *str;
	if (ts->version != TSPEC_ARG_VERSION) {
		printf("\tIncorrect version of TSPEC struct: expected %d; got %d\n",
		       TSPEC_ARG_VERSION, ts->version);
		return;
	}

	if (ts->length < (sizeof(tspec_arg_t) - (2 * sizeof(uint16)))) {
		printf("\tTSPEC arg length too short: expected %d; got %d\n",
		       (int)(sizeof(tspec_arg_t) - (2 * sizeof(uint16))), ts->length);
		return;
	}

	switch (ts->flag & TSPEC_STATUS_MASK) {
		case TSPEC_PENDING:
			str = "PENDING";
			break;
		case TSPEC_ACCEPTED:
			str = "ACCEPTED";
			break;
		case TSPEC_REJECTED:
			str = "REJECTED";
			break;
		default:
			str = "UNKNOWN";
			break;
	}

	printf("version %d\n", ts->version);
	printf("length %d\n", ts->length);

	printf("TID %d %s\n", WLC_CAC_GET_TID(ts->tsinfo), str);
	printf("tsinfo 0x%02x 0x%02x 0x%02x\n", ts->tsinfo.octets[0],
	       ts->tsinfo.octets[1], ts->tsinfo.octets[2]);

	/* breakout bitfields for apsd */
	if (WLC_CAC_GET_PSB(ts->tsinfo)) {
		int ac = WME_PRIO2AC(WLC_CAC_GET_USER_PRIO(ts->tsinfo));
		switch (WLC_CAC_GET_DIR(ts->tsinfo)) {
			case (TS_INFO_UPLINK >> TS_INFO_DIRECTION_SHIFT):
				printf("AC[%d] : Trigger enabled\n", ac);
				break;

			case (TS_INFO_DOWNLINK >> TS_INFO_DIRECTION_SHIFT):
				printf("AC[%d] : Delivery enabled\n", ac);
				break;

			case (TS_INFO_BIDIRECTIONAL >>
				TS_INFO_DIRECTION_SHIFT):
				printf("AC[%d] : Trig & Delv enabled\n", ac);
				break;
		}
	} else {
		int ac;
		ac = WME_PRIO2AC(WLC_CAC_GET_USER_PRIO(ts->tsinfo));
		printf("AC [%d] : Legacy Power save\n", ac);
	}


	printf("nom_msdu_size %d %s\n", (ts->nom_msdu_size & 0x7fff),
	       ((ts->nom_msdu_size & 0x8000) ? "fixed size" : ""));
	printf("max_msdu_size %d\n", ts->max_msdu_size);
	printf("min_srv_interval %d\n", ts->min_srv_interval);
	printf("max_srv_interval %d\n", ts->max_srv_interval);
	printf("inactivity_interval %d\n", ts->inactivity_interval);
	printf("suspension_interval %d\n", ts->suspension_interval);
	printf("srv_start_time %d\n", ts->srv_start_time);
	printf("min_data_rate %d\n", ts->min_data_rate);
	printf("mean_data_rate %d\n", ts->mean_data_rate);
	printf("peak_data_rate %d\n", ts->peak_data_rate);
	printf("max_burst_size %d\n", ts->max_burst_size);
	printf("delay_bound %d\n", ts->delay_bound);
	printf("min_phy_rate %d\n", ts->min_phy_rate);
	printf("surplus_bw %d\n", ts->surplus_bw);
	printf("medium_time %d\n", ts->medium_time);
}


/* send delts for a specific ea */
/* TODO : Club this with wl_tspec_ea */
static int
wl_cac_delts_ea(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr;
	int err = -1;
	char *endptr = NULL;
	tspec_per_sta_arg_t tsea;
	tspec_arg_t *ts;
	uint argc = 0;

	/* eat command name */
	argv++;

	while (argv[argc])
		argc++;

	/* required argments */
	if (argc < (NUM_TSLIST_PER_EA_ARG + 1)) {
		fprintf(stderr, "Too few arguments\n");
		fprintf(stderr, "wl cac_delts_ea ver 0xaa 0xbb 0xcc xx:xx:xx:xx:xx:xx\n");
		fprintf(stderr, "\twhere ver is the tspec version\n");
		fprintf(stderr, "\twhere 0xaa is byte[0] of tsinfo (bits 0-7)\n");
		fprintf(stderr, "\twhere 0xbb is byte[1] of tsinfo (bits 8-15)\n");
		fprintf(stderr, "\twhere 0xcc is byte[2] of tsinfo (bits 16-23)\n");
		fprintf(stderr, "\twhere xx:xx:xx:xx:xx:xx is mac address )\n");
		return BCME_BADARG;
	}

	memset((uint8 *)&tsea, 0, sizeof(tspec_per_sta_arg_t));

	ts = &tsea.ts;

	ts->length = sizeof(tspec_arg_t) - (2 * sizeof(uint16));
	ts->version = (uint16)strtol(*argv++, &endptr, 0);

	if (*endptr != '\0')
		goto error;

	ts->tsinfo.octets[0] = (uint8)strtol(*argv++, &endptr, 0);
	if (*endptr != '\0')
		goto error;

	ts->tsinfo.octets[1] = (uint8)strtol(*argv++, &endptr, 0);
	if (*endptr != '\0')
		goto error;

	ts->tsinfo.octets[2] = (uint8)strtol(*argv++, &endptr, 0);
	if (*endptr != '\0')
		goto error;


	/* add the ether address after tsinfo */
	if (!*argv) {
		printf("MAC address must be specified\n");
		return -1;
	} else if (!wl_ether_atoe(*argv, &tsea.ea)) {
		printf("Malformed MAC address parameter\n");
		return -1;
	}

	wl_cac_format_tspec_htod(ts);
	if ((err = wlu_var_getbuf(wl, cmd->name, &tsea, sizeof(tspec_per_sta_arg_t), &ptr)) < 0)
		return err;

	return 0;

error:
	return err;

}

static int
wl_antsel(void *wl, cmd_t *cmd, char **argv)
{
	const char *ant_sel = "fixed";
	char *val_name;
	wlc_antselcfg_t val = {{0}, 0};
	int err, argc, i;
	char *endptr = NULL;
	uint32 txchain_bitmap = 0;
	uint16 antsel_mask = 0;

	/* toss the command name */
	val_name = *argv++;

	if (!*argv) {
		if (cmd->get < 0)
			return -1;
	    if ((err = wlu_iovar_get(wl, "txchain", &txchain_bitmap, sizeof(txchain_bitmap))) < 0)
				return err;

		/* iterate over max 4 chains */
		for (i = 0; i < 4; i ++) {
			if (!(txchain_bitmap & (1<<i)))
				antsel_mask |=  (0xF << i * 4);
		}

		if ((err = wlu_iovar_get(wl, val_name, &val, sizeof(wlc_antselcfg_t))) < 0)
			return err;

		printf("C3C2C1C0: ");
		for (i = ANT_SELCFG_TX_UNICAST; i < ANT_SELCFG_MAX; i++) {
			if (val.ant_config[i] & ANT_SELCFG_AUTO)
				ant_sel = "auto";
			printf("0x%04X %s ",
			antsel_mask | (val.ant_config[i] & ANT_SELCFG_MASK), ant_sel);
		}
		printf("\n");
	} else {
		/* arg count */
		for (argc = 0; argv[argc]; argc++);

		if ((argc >= 2 && argc <= 3) || argc > ANT_SELCFG_MAX) {
			printf("invalid %d args\n", argc);
			return -1;
		}

		val.ant_config[ANT_SELCFG_TX_UNICAST] = (uint8)strtol(*argv++, &endptr, 0);
		printf("UTX 0x%02x\n", val.ant_config[ANT_SELCFG_TX_UNICAST]);
		if (*endptr != '\0') {
			printf("Invaild UTX parameter: %s\n", *argv);
			return -1;
		}
		if (argc == 1) {
			val.ant_config[ANT_SELCFG_RX_UNICAST] =
				val.ant_config[ANT_SELCFG_TX_UNICAST];
			val.ant_config[ANT_SELCFG_TX_DEF] = val.ant_config[ANT_SELCFG_TX_UNICAST];
			val.ant_config[ANT_SELCFG_RX_DEF] = val.ant_config[ANT_SELCFG_TX_UNICAST];
		} else {
			val.ant_config[ANT_SELCFG_RX_UNICAST] = (uint8)strtol(*argv++, &endptr, 0);
			printf("URX 0x%02x\n", val.ant_config[ANT_SELCFG_RX_UNICAST]);
			if (*endptr != '\0') {
				printf("Invaild URX parameter: %s\n", *argv);
				return -1;
			}
			val.ant_config[ANT_SELCFG_TX_DEF] = (uint8)strtol(*argv++, &endptr, 0);
			printf("DTX 0x%02x\n", val.ant_config[ANT_SELCFG_TX_DEF]);
			if (*endptr != '\0') {
				printf("Invaild DTX parameter: %s\n", *argv);
				return -1;
			}
			val.ant_config[ANT_SELCFG_RX_DEF] = (uint8)strtol(*argv++, &endptr, 0);
			printf("DRX 0x%02x\n", val.ant_config[ANT_SELCFG_RX_DEF]);
			if (*endptr != '\0') {
				printf("Invaild DRX parameter: %s\n", *argv);
				return -1;
			}
		}
		err = wlu_iovar_set(wl, val_name, &val, sizeof(wlc_antselcfg_t));
	}
	return err;
}

static int
wl_txcore_pwr_offset(void *wl, cmd_t *cmd, char **argv)
{
	wl_txchain_pwr_offsets_t offsets;
	char *endptr;
	int i;
	long val;
	int err;

	/* toss the command name */
	argv++;

	if (!*argv) {
		err = wlu_iovar_get(wl, cmd->name, &offsets, sizeof(wl_txchain_pwr_offsets_t));

		if (err < 0)
			return err;

		printf("txcore offsets qdBm: %d %d %d %d\n",
		       offsets.offset[0], offsets.offset[1],
		       offsets.offset[2], offsets.offset[3]);

		return 0;
	}

	memset(&offsets, 0, sizeof(wl_txchain_pwr_offsets_t));

	for (i = 0; i < WL_NUM_TXCHAIN_MAX; i++, argv++) {
		if (!*argv)
			break;

		val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0')
			return -1;

		if (val > 0)
			return -1;

		offsets.offset[i] = (int8)val;
	}

	err = wlu_iovar_set(wl, cmd->name, &offsets, sizeof(wl_txchain_pwr_offsets_t));

	return err;
}

static int
wl_txcore(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t to;
	const char* fn_name = "wl_txcore";
	int err = 0, opt_err, val;
	uint8 streams = 0;
	bool streams_set = FALSE;
	uint8 core = 0;
	bool core_set = FALSE;
	uint8 cck_mask = 0;
	bool cck_set = FALSE;
	uint8 ofdm_mask = 0;
	bool ofdm_set = FALSE;
	uint8 mcs_mask[4] = {0, 0, 0, 0}; /* pre-initialize # of streams {core:4 | stream:4} */
	bool mcs_set = FALSE;
	uint8 idx;
	uint32 coremask[2] = {0, 0};

	/* toss the command name */
	argv++;

	if (!*argv) {
		if (cmd->get < 0)
			return -1;
		if ((err = wlu_iovar_get(wl, cmd->name, &coremask, sizeof(uint32)*2)) < 0)
			return err;

		printf("txcore enabled bitmap (Nsts {4..1}) 0x%02x 0x%02x 0x%02x 0x%02x\n",
			(coremask[0] >> 24) & 0xff, (coremask[0] >> 16) & 0xff,
			(coremask[0] >> 8) & 0xff, coremask[0] & 0xff);
		printf("txcore mask OFDM 0x%02x  CCK 0x%02x\n",
			(coremask[1] >> 8) & 0xff, coremask[1] & 0xff);
		return 0;
	}

	val = atoi(*argv);
	if (val == -1)
		goto next;

	miniopt_init(&to, fn_name, "w", FALSE);
	while ((opt_err = miniopt(&to, argv)) != -1) {
		if (opt_err == 1) {
			err = -1;
			goto exit;
		}
		argv += to.consumed;

		if (to.opt == 's') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for streams\n",
					fn_name, to.valstr);
				err = -1;
				goto exit;
			}
			streams_set = TRUE;
			streams = (to.val & 0x0f);
			if (streams > 4)
				fprintf(stderr, "%s: Nsts > %d\n", fn_name, to.val);
		}
		if (to.opt == 'c') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for stf core\n",
					fn_name, to.valstr);
				err = -1;
				goto exit;
			}
			core_set = TRUE;
			core = (to.val & 0x0f) << 4;
			if (core == 0) {
				fprintf(stderr, "%s: %1d-stream core cannot be zero\n",
					fn_name, streams);
				err = -1;
				goto exit;
			}
		}
		if (to.opt == 'o') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for streams\n",
					fn_name, to.valstr);
				err = -1;
				goto exit;
			}
			ofdm_set = TRUE;
			ofdm_mask = (to.val & 0x0f);
			if (ofdm_mask == 0) {
				fprintf(stderr, "%s: OFDM core cannot be zero\n", fn_name);
				err = -1;
				goto exit;
			}
		}
		if (to.opt == 'k') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for streams\n",
					fn_name, to.valstr);
				err = -1;
				goto exit;
			}
			cck_set = TRUE;
			cck_mask = (to.val & 0x0f);
			if (cck_mask == 0) {
				fprintf(stderr, "%s: CCK core cannot be zero\n", fn_name);
				err = -1;
				goto exit;
			}
		}

		if (streams_set && core_set) {
			streams_set = core_set = FALSE;
			mcs_set = TRUE;
			idx = streams - 1;
			mcs_mask[idx] = (uint8)(core|streams);
		}
	}

	if (streams_set != core_set) {
		fprintf(stderr, "%s: require to set both -s x -c y\n", fn_name);
		err = -1;
		goto exit;
	}

	if (mcs_set) {
		coremask[0] |= mcs_mask[0] << 0;
		coremask[0] |= mcs_mask[1] << 8;
		coremask[0] |= mcs_mask[2] << 16;
		coremask[0] |= mcs_mask[3] << 24;
	}
	if (cck_set)
		coremask[1] |= cck_mask;
	if (ofdm_set)
		coremask[1] |= ofdm_mask << 8;
next:
	err = wlu_var_setbuf(wl, cmd->name, coremask, sizeof(uint32)*2);
exit:
	return err;
}

#ifdef PLC
static int
wl_plc(void *wl, cmd_t *cmd, char **argv)
{
	char *param;
	char *endptr;
	const char *cmdname = "plc";
	wl_plc_params_t plc, *reply;
	int err;
	void *ptr;

	UNUSED_PARAMETER(cmd);

	bzero(&plc, sizeof(plc));

	if ((param = *++argv) == NULL) {
		plc.cmd = PLC_CMD_FAILOVER;
		/* Get plc failover status */
		err = wlu_var_getbuf_sm(wl, cmdname, &plc, sizeof(plc), &ptr);
		reply = (wl_plc_params_t *)ptr;
		if (err >= 0)
			printf("%d\n", reply->plc_failover);
		return err;
	}

	if ((strcmp(param, "1") == 0) || (strcmp(param, "0") == 0)) {
		plc.cmd = PLC_CMD_FAILOVER;
		plc.plc_failover = strtoul(param, &endptr, 0);
		/* Set plc failover status */
		err = wlu_var_setbuf(wl, cmdname, &plc, sizeof(plc));
		return err;
	} else if (strcmp(param, "node_list") == 0) {
		wl_plc_nodelist_t *list = (wl_plc_nodelist_t *)buf;
		uint32 i, max = (WLC_IOCTL_MAXLEN - sizeof(int)) / sizeof(wl_plc_nodelist_t);

		/* Get the list of known nodes */
		plc.cmd = PLC_CMD_NODE_LIST;
		memset(buf, 0, WLC_IOCTL_MAXLEN);
		err = wlu_iovar_getbuf(wl, cmdname, &plc, sizeof(plc), buf, WLC_IOCTL_MAXLEN);
		if (err < 0)
			return err;

		for (i = 0; i < dtoh32(list->count) && i < max; i++) {
			printf("%s    %-9s   %-d\n", wl_ether_etoa(&list->node[i].ea),
			       list->node[i].node_type == NODE_TYPE_WIFI_ONLY ? "wifi-only" :
			       list->node[i].node_type == NODE_TYPE_PLC_ONLY ? "plc-only" :
			       list->node[i].node_type == NODE_TYPE_WIFI_PLC ? "wifi-plc" :
			       "unknown", list->node[i].cost);
		}
		return err;
	} else if (strcmp(param, "mac_affinity") == 0)
		plc.cmd = PLC_CMD_MAC_COST;
	else if (strcmp(param, "link_affinity") == 0)
		plc.cmd = PLC_CMD_LINK_COST;
	else
		return USAGE_ERROR;

	if ((param = *++argv) == NULL)
		return USAGE_ERROR;

	if (!wl_ether_atoe(param, &plc.node_ea))
		return USAGE_ERROR;

	if ((param = *++argv) == NULL) {
		/* Get plc link/mac path affinity */
		err = wlu_var_getbuf_sm(wl, cmdname, &plc, sizeof(plc), &ptr);
		reply = (wl_plc_params_t *)ptr;
		if (err >= 0)
			printf("%d\n", reply->cost);
		return err;
	}

	/* Set plc link/mac path affinity */
	plc.cost = strtoul(param, &endptr, 0);
	err = wlu_var_setbuf(wl, cmdname, &plc, sizeof(plc));

	return err;
}
#endif /* PLC */

static int
wl_txfifo_sz(void *wl, cmd_t *cmd, char **argv)
{
	char *param;
	const char *cmdname = "txfifo_sz";
	wl_txfifo_sz_t ts, *reply;
	uint fifo;
	int err;
	void *ptr = NULL;

	UNUSED_PARAMETER(cmd);

	if ((param = *++argv) == NULL)
		return USAGE_ERROR;

	fifo = atoi(param);
	if (fifo > NFIFO)
		return USAGE_ERROR;
	ts.fifo = fifo;
	ts.magic = WL_TXFIFO_SZ_MAGIC;

	if ((param = *++argv)) {
		ts.size = atoi(param);
		err = wlu_var_setbuf(wl, cmdname, &ts, sizeof(ts));
	} else {
		if ((err = wlu_var_getbuf_sm(wl, cmdname, &ts, sizeof(ts), &ptr) < 0))
			return err;
		reply = (wl_txfifo_sz_t *)ptr;
		printf("fifo %d size %d\n", fifo, reply->size);
	}
	return err;
}
#ifdef WLPFN
static int
wl_pfn_set(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	wl_pfn_param_t pfn_param;

	UNUSED_PARAMETER(cmd);

	/* Setup default values */
	pfn_param.version = PFN_VERSION;
	/* Sorting based on list order, no back ground scan, no autoswitch,
	  * no immediate event report, no adaptvie scan, but immediate scan
	  */
	pfn_param.flags = (PFN_LIST_ORDER << SORT_CRITERIA_BIT | ENABLE << IMMEDIATE_SCAN_BIT);
	/* Scan frequency of 30 sec */
	pfn_param.scan_freq = 30;
	/* slow adapt scan is off by default */
	pfn_param.slow_freq = 0;
	/* RSSI margin of 30 dBm */
	pfn_param.rssi_margin = 30;
	/* Network timeout 60 sec */
	pfn_param.lost_network_timeout = 60;
	/* best n = 2 by default */
	pfn_param.bestn = DEFAULT_BESTN;
	/* mscan m=0 by default, so not record best networks by default */
	pfn_param.mscan = DEFAULT_MSCAN;
	/*  default repeat = 10 */
	pfn_param.repeat = DEFAULT_REPEAT;
	/* by default, maximum scan interval = 2^2*scan_freq when adaptive scan is turned on */
	pfn_param.exp = DEFAULT_EXP;

	while (*++argv) {
		if (!stricmp(*argv, "scanfrq")) {
			if (*++argv)
				pfn_param.scan_freq = atoi(*argv);
			else {
				fprintf(stderr, "Missing scanfrq option\n");
				return -1;
			}
		} else if (!stricmp(*argv, "netimeout")) {
			if (*++argv)
				pfn_param.lost_network_timeout = atoi(*argv);
			else {
				fprintf(stderr, "Missing netimeout option\n");
				return -1;
			}
		} else if (!stricmp(*argv, "rssi_delta")) {
			if (*++argv)
				pfn_param.rssi_margin = atoi(*argv);
			else {
				fprintf(stderr, "Missing rssi_delta option\n");
				return -1;
			}
		} else if (!stricmp(*argv, "sort")) {
			if (*++argv) {
				pfn_param.flags &= ~SORT_CRITERIA_MASK;
				if (!stricmp(*argv, "listorder"))
					pfn_param.flags |= (PFN_LIST_ORDER << SORT_CRITERIA_BIT);
				else if (!stricmp(*argv, "rssi"))
					pfn_param.flags |= (PFN_RSSI << SORT_CRITERIA_BIT);
				else {
					fprintf(stderr, "Invalid sort option %s\n", *argv);
					return -1;
				}
			} else {
				fprintf(stderr, "Missing sort option\n");
				return -1;
			}
		} else if (!stricmp(*argv, "immediateevent")) {
			if (*++argv) {
				if (!stricmp(*argv, "1")) {
					pfn_param.flags |= IMMEDIATE_EVENT_MASK;
				} else if (!stricmp(*argv, "0")) {
					pfn_param.flags &= ~IMMEDIATE_EVENT_MASK;
				} else {
					fprintf(stderr, "Invalid immediateevent option\n");
					return -1;
				}
			} else {
				fprintf(stderr, "Missing immediateevent option\n");
				return -1;
			}
		} else if (!stricmp(*argv, "bkgscan")) {
			if (*++argv) {
				pfn_param.flags &= ~ENABLE_BKGRD_SCAN_MASK;
				if (atoi(*argv))
					pfn_param.flags |= (ENABLE << ENABLE_BKGRD_SCAN_BIT);
				else
					pfn_param.flags |= (DISABLE << ENABLE_BKGRD_SCAN_BIT);
			} else {
				fprintf(stderr, "Missing bkgscan option\n");
				return -1;
			}
		} else if (!stricmp(*argv, "immediate")) {
			pfn_param.flags &= ~IMMEDIATE_SCAN_MASK;
			if (*++argv) {
				if (atoi(*argv))
					pfn_param.flags |= (ENABLE << IMMEDIATE_SCAN_BIT);
				else
					pfn_param.flags |= (DISABLE << IMMEDIATE_SCAN_BIT);
			} else {
				fprintf(stderr, "Missing immediate option\n");
				return -1;
			}
		} else if (!stricmp(*argv, "bdscan")) {
			if (*++argv) {
				pfn_param.flags &= ~ENABLE_BD_SCAN_MASK;
				if (atoi(*argv))
					pfn_param.flags |= (ENABLE << ENABLE_BD_SCAN_BIT);
				else
					pfn_param.flags |= (DISABLE << ENABLE_BD_SCAN_BIT);
			} else {
				fprintf(stderr, "Missing bdscan option\n");
				return -1;
			}
		} else if (!stricmp(*argv, "adapt")) {
			if (*++argv) {
				pfn_param.flags &= ~ENABLE_ADAPTSCAN_MASK;
				if (!stricmp(*argv, "off")) {
					pfn_param.flags |= (OFF_ADAPT << ENABLE_ADAPTSCAN_BIT);
				} else if (!stricmp(*argv, "smart")) {
					pfn_param.flags |= (SMART_ADAPT << ENABLE_ADAPTSCAN_BIT);
				} else if (!stricmp(*argv, "strict")) {
					pfn_param.flags |= (STRICT_ADAPT << ENABLE_ADAPTSCAN_BIT);
				} else if (!stricmp(*argv, "slow")) {
					pfn_param.flags |= (SLOW_ADAPT << ENABLE_ADAPTSCAN_BIT);
				} else {
					fprintf(stderr, "Invalid adaptive scan option %s\n", *argv);
					return -1;
				}
			} else {
				fprintf(stderr, "Missing adaptive scan option\n");
				return -1;
			}
		} else if (!stricmp(*argv, "bestn")) {
			pfn_param.bestn = atoi(*++argv);
		} else if (!stricmp(*argv, "mscan")) {
			pfn_param.mscan = atoi(*++argv);
		} else if (!stricmp(*argv, "repeat")) {
			pfn_param.repeat = atoi(*++argv);
			if (pfn_param.repeat < 1 || pfn_param.repeat > 20) {
				fprintf(stderr, "repeat %d out of range (1-20)\n",
					pfn_param.repeat);
				return -1;
			}
		} else if (!stricmp(*argv, "exp")) {
			pfn_param.exp = atoi(*++argv);
			if (pfn_param.exp < 1 || pfn_param.exp > 5) {
				fprintf(stderr, "exp %d out of range (1-5)\n",
					pfn_param.exp);
				return -1;
			}
		} else if (!stricmp(*argv, "slowfrq")) {
			if (*++argv)
				pfn_param.slow_freq = atoi(*argv);
			else {
				fprintf(stderr, "Missing slowfrq option\n");
				return -1;
			}
		}  else {
			fprintf(stderr, "Invalid parameter %s\n", *argv);
			return -1;
		}
	}

	if ((((pfn_param.flags & ENABLE_ADAPTSCAN_MASK) ==
	    (SLOW_ADAPT << ENABLE_ADAPTSCAN_BIT)) &&
	    !pfn_param.slow_freq) ||
	    (((pfn_param.flags & ENABLE_ADAPTSCAN_MASK) !=
	    (SLOW_ADAPT << ENABLE_ADAPTSCAN_BIT)) &&
	    pfn_param.slow_freq)) {
		fprintf(stderr, "SLOW_ADAPT flag and slowfrq value not match\n");
		return -1;
	}
	pfn_param.version = htod32(pfn_param.version);
	pfn_param.scan_freq = htod32(pfn_param.scan_freq);
	pfn_param.lost_network_timeout = htod32(pfn_param.lost_network_timeout);
	pfn_param.flags = htod16(pfn_param.flags);
	pfn_param.rssi_margin = htod16(pfn_param.rssi_margin);
	pfn_param.slow_freq = htod32(pfn_param.slow_freq);

	if ((err = wlu_iovar_set(wl, "pfn_set", &pfn_param, sizeof(wl_pfn_param_t))))
		return (err);

	return (0);
}

static bool
validate_hex(char hexchar)
{
	if ((hexchar >= '0' && hexchar <= '9') ||
		(hexchar >= 'a' || hexchar <= 'z') ||
		(hexchar >= 'A' || hexchar <= 'Z'))
		return TRUE;
	else
		return FALSE;
}

static uint8
char2hex(char hexchar)
{
	if (hexchar >= '0' && hexchar <= '9')
		return (hexchar - '0');
	else if (hexchar >= 'a' && hexchar <= 'z')
		return (hexchar - 'a' + 10);
	else if (hexchar >= 'A' && hexchar <= 'Z')
		return (hexchar - 'A' + 10);
	else
	{
		fprintf(stderr, "non-hex\n");
		return 0xff;
	}
}

#define MAXNUM_SSID_PER_ADD	16

static int
wl_pfn_add(void *wl, cmd_t *cmd, char **argv)
{
	int         err;
	wl_pfn_t    *p_pfn_element = NULL;
	int         i, pfn_element_len, cnt;
	wl_pfn_t    *pssidnet = NULL;
	int32       hidden;

	UNUSED_PARAMETER(cmd);

	pfn_element_len = MAXNUM_SSID_PER_ADD * sizeof(wl_pfn_t);
	p_pfn_element = (wl_pfn_t *)malloc(pfn_element_len);
	if (p_pfn_element == NULL) {
		fprintf(stderr, "Failed to allocate buffer for %d bytes\n", pfn_element_len);
		return -1;
	}
	memset(p_pfn_element, '\0', pfn_element_len);

	pssidnet = p_pfn_element;
	for (i = 0; i < MAXNUM_SSID_PER_ADD; i++) {
		/* Default setting, open, no WPA, no WEP and bss */
		pssidnet->auth = DOT11_OPEN_SYSTEM;
		pssidnet->wpa_auth = WPA_AUTH_DISABLED;
		pssidnet->wsec = 0;
		pssidnet->infra = 1;
		pssidnet->flags = 0;
		pssidnet++;
	}
	cnt = -1;
	pssidnet = p_pfn_element;
	while (*++argv) {
		if (!stricmp(*argv, "ssid")) {
			if (*++argv) {
				if (++cnt >= MAXNUM_SSID_PER_ADD) {
					fprintf(stderr, "exceed max 16 SSID per pfn_add\n");
					goto error;
				}
				if (cnt > 0) {
					pssidnet->flags = htod32(pssidnet->flags);
					pssidnet++;
				}
				strncpy((char *)pssidnet->ssid.SSID, *argv,
				        sizeof(pssidnet->ssid.SSID));
				pssidnet->ssid.SSID_len =
				   strlen((char *)pssidnet->ssid.SSID);
				if (pssidnet->ssid.SSID_len > 32) {
					fprintf(stderr, "SSID too long: %s\n", *argv);
					goto error;
				}
				pssidnet->ssid.SSID_len = htod32(pssidnet->ssid.SSID_len);
			} else {
				fprintf(stderr, "no value for ssid\n");
				goto error;
			}
		}
		else if (!stricmp(*argv, "hidden")) {
			if (pssidnet->ssid.SSID_len == 0) {
				fprintf(stderr, "Wrong! Start with SSID\n");
				goto error;
			}
			if (*++argv) {
				hidden = **argv - '0';
				if (hidden != ENABLE && hidden != DISABLE) {
					fprintf(stderr, "invalid hidden setting, use 0/1\n");
					goto error;
				}
				pssidnet->flags |= hidden << WL_PFN_HIDDEN_BIT;
			} else {
				fprintf(stderr, "no value for hidden\n");
				goto error;
			}
		}  else if (!stricmp(*argv, "imode")) {
			if (*++argv) {
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					goto error;
				}
				if (!stricmp(*argv, "bss")) {
					pssidnet->infra = 1;
				} else if (!stricmp(*argv, "ibss")) {
					pssidnet->infra = 0;
				} else {
					fprintf(stderr, "Invalid imode arg %s\n", *argv);
					goto error;
				}
				pssidnet->infra = htod32(pssidnet->infra);
			} else {
				fprintf(stderr, "Missing option for imode\n");
				goto error;
			}
		} else if (!stricmp(*argv, "amode")) {
			if (*++argv) {
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					goto error;
				}
				if (!stricmp(*argv, "open"))
					pssidnet->auth = DOT11_OPEN_SYSTEM;
				else if (!stricmp(*argv, "shared"))
					pssidnet->auth = DOT11_SHARED_KEY;
				else {
					fprintf(stderr, "Invalid imode arg %s\n", *argv);
					goto error;
				}
				pssidnet->auth = htod32(pssidnet->auth);
			} else {
				fprintf(stderr, "Missing option for amode\n");
				goto error;
			}
		} else if (!stricmp(*argv, "wpa_auth")) {
			if (*++argv) {
				uint32 wpa_auth;
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					goto error;
				}

				/* figure requested auth, allow "any" */
				if (!stricmp(*argv, "wpapsk"))
					pssidnet->wpa_auth = WPA_AUTH_PSK;
				else if (!stricmp(*argv, "wpa2psk"))
					pssidnet->wpa_auth = WPA2_AUTH_PSK;
				else if (!stricmp(*argv, "wpadisabled"))
					pssidnet->wpa_auth = WPA_AUTH_DISABLED;
				else if (!stricmp(*argv, "any"))
					pssidnet->wpa_auth = WPA_AUTH_PFN_ANY;
				else if ((wpa_auth = strtoul(*argv, 0, 0)))
					pssidnet->wpa_auth = wpa_auth;
				else {
					fprintf(stderr, "Invalid wpa_auth option %s\n", *argv);
					goto error;
				}
				pssidnet->wpa_auth = htod32(pssidnet->wpa_auth);
			} else {
				fprintf(stderr, "Missing option for wpa_auth\n");
				goto error;
			}
		} else if (!stricmp(*argv, "wsec")) {
			if (*++argv) {
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					goto error;
				}
				if (!stricmp(*argv, "WEP")) {
					pssidnet->wsec = WEP_ENABLED;
				} else if (!stricmp(*argv, "TKIP"))
					pssidnet->wsec = TKIP_ENABLED;
				else if (!stricmp(*argv, "AES"))
					pssidnet->wsec = AES_ENABLED;
				else if (!stricmp(*argv, "TKIPAES"))
					pssidnet->wsec = TKIP_ENABLED | AES_ENABLED;
				else {
					fprintf(stderr, "Invalid wsec option %s\n", *argv);
					goto error;
				}
				pssidnet->wsec = htod32(pssidnet->wsec);
			} else {
				fprintf(stderr, "Missing option for wsec\n");
				goto error;
			}
		} else if (!stricmp(*argv, "suppress")) {
			if (*++argv) {
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					goto error;
				}
				if (!stricmp(*argv, "found")) {
					pssidnet->flags |= WL_PFN_SUPPRESSFOUND_MASK;
				} else if (!stricmp(*argv, "lost")) {
					pssidnet->flags |= WL_PFN_SUPPRESSLOST_MASK;
				} else if (!stricmp(*argv, "neither")) {
					pssidnet->flags &=
					 ~(WL_PFN_SUPPRESSLOST_MASK | WL_PFN_SUPPRESSFOUND_MASK);
				} else {
					fprintf(stderr, "Invalid suppress option %s\n", *argv);
					goto error;
				}
			} else {
				fprintf(stderr, "Missing option for suppress\n");
				goto error;
			}
		} else if (!stricmp(*argv, "rssi")) {
			if (*++argv) {
				int rssi = atoi(*argv);
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					goto error;
				}
				if (rssi >= -128 && rssi <= 0) {
					pssidnet->flags |= (rssi << WL_PFN_RSSI_SHIFT)
						& WL_PFN_RSSI_MASK;
				} else {
					fprintf(stderr, "Invalid rssi option %s\n", *argv);
					goto error;
				}
			} else {
				fprintf(stderr, "Missing option for rssi\n");
				goto error;
			}
		}	else {
			fprintf(stderr, "Invalid parameter %s\n", *argv);
			goto error;
		}
	}
	pssidnet->flags = htod32(pssidnet->flags);

	pfn_element_len = (cnt + 1) * sizeof(wl_pfn_t);
	if ((err = wlu_iovar_set(wl, "pfn_add", p_pfn_element,
	     pfn_element_len))) {
		fprintf(stderr, "pfn_add fail\n");
		goto error;
	}
	free(p_pfn_element);
	return (0);

error:
	free(p_pfn_element);
	return -1;
}

#define MAXNUM_BSSID_PER_ADD	150

static int
wl_pfn_add_bssid(void *wl, cmd_t *cmd, char **argv)
{
	int                 err;
	uint8               *ptr;
	int                 i, bssidlistlen, cnt;
	wl_pfn_bssid_t      *bssidlist;
	wl_pfn_bssid_t      *pbssid = NULL;

	UNUSED_PARAMETER(cmd);

	if (!*(argv + 1)) {
		fprintf(stderr, "Invalid command\n");
		return -1;
	}

	bssidlistlen = MAXNUM_BSSID_PER_ADD * sizeof(wl_pfn_bssid_t);
	bssidlist = (wl_pfn_bssid_t *)malloc(bssidlistlen);
	if (bssidlist == NULL) {
		fprintf(stderr, "Failed to allocate buffer for %d bytes\n", bssidlistlen);
		return -1;
	}
	memset(bssidlist, '\0', bssidlistlen);

	cnt = 0;
	while (*++argv) {
		if (!stricmp(*argv, "bssid")) {
			if (*++argv) {
				if (cnt >= MAXNUM_BSSID_PER_ADD) {
					fprintf(stderr, "exceed max 150 BSSID per pfn_add_bssid\n");
					goto error;
				}
				if (!cnt)
					pbssid = bssidlist;
				else {
					pbssid->flags = htod16(pbssid->flags);
					pbssid++;
				}

				ptr = (uint8 *)*argv;
				for (i = 0; i < ETHER_ADDR_LEN; i++)
				{
					if (!validate_hex(*ptr) || !validate_hex(*(ptr + 1)))
					{
						fprintf(stderr, "non-hex in BSSID\n");
						goto error;
					}
					pbssid->macaddr.octet[i] =
					      char2hex(*ptr) << 4 | char2hex(*(ptr+1));
					ptr += 3;
				}
				cnt++;
			} else {
				fprintf(stderr, "Missing option for bssid\n");
				goto error;
			}
		} else if (!stricmp(*argv, "suppress")) {
			if (!pbssid || ETHER_ISNULLADDR(pbssid->macaddr.octet)) {
				fprintf(stderr, "Wrong! Start with BSSID\n");
				goto error;
			}
			if (*++argv) {
				if (!stricmp(*argv, "found")) {
					pbssid->flags |= WL_PFN_SUPPRESSFOUND_MASK;
				} else if (!stricmp(*argv, "lost")) {
					pbssid->flags |= WL_PFN_SUPPRESSLOST_MASK;
				} else if (!stricmp(*argv, "neither")) {
					pbssid->flags &=
					 ~(WL_PFN_SUPPRESSFOUND_MASK | WL_PFN_SUPPRESSLOST_MASK);
				} else {
					fprintf(stderr, "Invalid suppress option %s\n", *argv);
					goto error;
				}
			} else {
				fprintf(stderr, "Missing option for suppress\n");
				goto error;
			}
		} else if (!stricmp(*argv, "rssi")) {
			if (*++argv) {
				int rssi = atoi(*argv);
				if (!pbssid || ETHER_ISNULLADDR(pbssid->macaddr.octet)) {
					fprintf(stderr, "Wrong! Start with BSSID\n");
					goto error;
				}
				if (rssi >= -128 && rssi <= 0) {
					pbssid->flags |= (rssi << WL_PFN_RSSI_SHIFT)
						& WL_PFN_RSSI_MASK;
				} else {
					fprintf(stderr, "Invalid rssi option %s\n", *argv);
					goto error;
				}
			} else {
				fprintf(stderr, "Missing option for rssi\n");
				goto error;
			}
		} else {
			fprintf(stderr, "Invalid parameter %s\n", *argv);
			goto error;
		}
	}
	pbssid->flags = htod16(pbssid->flags);

	bssidlistlen = cnt * sizeof(wl_pfn_bssid_t);
	if ((err = wlu_iovar_set(wl, "pfn_add_bssid", bssidlist,
	     bssidlistlen))) {
		fprintf(stderr, "pfn_add_bssid fail\n");
		goto error;
	}
	free(bssidlist);
	return 0;

error:
	free(bssidlist);
	return -1;
}

static int
wl_pfn_cfg(void *wl, cmd_t *cmd, char **argv)
{
	wl_pfn_cfg_t pfncfg_param;
	int          nchan = 0;
	int          err;

	UNUSED_PARAMETER(cmd);

	memset(&pfncfg_param, '\0', sizeof(wl_pfn_cfg_t));

	/* Setup default values */
	pfncfg_param.reporttype = WL_PFN_REPORT_ALLNET;
	pfncfg_param.channel_num = 0;

	while (*++argv) {
		if (!stricmp(*argv, "report")) {
			if (*++argv) {
				if (!stricmp(*argv, "all")) {
					pfncfg_param.reporttype = WL_PFN_REPORT_ALLNET;
				} else if (!stricmp(*argv, "ssidonly")) {
					pfncfg_param.reporttype = WL_PFN_REPORT_SSIDNET;
				} else if (!stricmp(*argv, "bssidonly")) {
					pfncfg_param.reporttype = WL_PFN_REPORT_BSSIDNET;
				} else {
					fprintf(stderr, "Invalid report option %s\n", *argv);
					return -1;
				}
			} else {
				fprintf(stderr, "no value for report\n");
				return -1;
			}
		} else if (!stricmp(*argv, "channel")) {
			if (*++argv) {
				nchan = wl_parse_channel_list(*argv, pfncfg_param.channel_list,
				                              WL_NUMCHANNELS);
				if (nchan < 0) {
					fprintf(stderr, "error parsing channel\n");
					return -1;
				}
			} else {
				fprintf(stderr, "Missing option for channel\n");
				return -1;
			}
		} else {
			fprintf(stderr, "Invalid parameter %s\n", *argv);
			return -1;
		}
	}

	pfncfg_param.reporttype = htod32(pfncfg_param.reporttype);
	pfncfg_param.channel_num = htod32(nchan);

	if ((err = wlu_iovar_set(wl, "pfn_cfg", &pfncfg_param,
	     sizeof(wl_pfn_cfg_t)))) {
		fprintf(stderr, "pfn_cfg fail\n");
		return err;
	}

	return 0;
}

static int
wl_pfn(void *wl, cmd_t *cmd, char **argv)
{
	int err, val;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {
		val = atoi(*argv);
		err = wlu_iovar_setint(wl, "pfn", (val ? 1 : 0));
	} else {
		err = wlu_iovar_getint(wl, "pfn", &val);
		if (!err)
			wl_printint(val);
	}

	return err;
}
#define WL_PFN_BESTNET_LEN	2048

static int
wl_pfnbest(void *wl, cmd_t *cmd, char **argv)
{
	int	err;
	wl_pfn_scanresults_t *bestnet;
	wl_pfn_net_info_t *netinfo;
	uint32 i, j;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {
		fprintf(stderr, "Invalid parameter %s\n", *argv);
		return -1;
	}
	bestnet = (wl_pfn_scanresults_t *)malloc(WL_PFN_BESTNET_LEN);
	if (bestnet == NULL) {
		fprintf(stderr, "Failed to allocate buffer of %d bytes\n", WL_PFN_BESTNET_LEN);
		return -1;
	}
	bzero(bestnet, WL_PFN_BESTNET_LEN);
	while (bestnet->status != PFN_COMPLETE) {
		if ((err = wlu_iovar_get(wl, "pfnbest", (void *)bestnet, WL_PFN_BESTNET_LEN))) {
			fprintf(stderr, "pfnbest fail\n");
			return err;
		}
		printf("ver %d, status %d, count %d\n",
		 bestnet->version, bestnet->status, bestnet->count);
		netinfo = bestnet->netinfo;
		for (i = 0; i < bestnet->count; i++) {
			for (j = 0; j < netinfo->pfnsubnet.SSID_len; j++)
				printf("%c", netinfo->pfnsubnet.SSID[j]);
			printf("\n");
			printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
			        netinfo->pfnsubnet.BSSID.octet[0],
			        netinfo->pfnsubnet.BSSID.octet[1],
			        netinfo->pfnsubnet.BSSID.octet[2],
			        netinfo->pfnsubnet.BSSID.octet[3],
			        netinfo->pfnsubnet.BSSID.octet[4],
			        netinfo->pfnsubnet.BSSID.octet[5]);
			printf("channel: %d, RSSI: %d, timestamp: %d\n",
			 netinfo->pfnsubnet.channel, netinfo->RSSI, netinfo->timestamp);
			netinfo++;
		}
	}

	return 0;
}

static int
wl_pfn_suspend(void *wl, cmd_t *cmd, char **argv)
{
	int	err, val;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {
		val = atoi(*argv);
		err = wlu_iovar_setint(wl, "pfn_suspend", (val ? 1 : 0));
	} else {
		err = wlu_iovar_getint(wl, "pfn_suspend", &val);
		if (!err)
			wl_printint(val);
	}

	return err;
}

#if defined(linux)
static void
wl_pfn_printnet(wl_pfn_scanresults_t *ptr, int event_type)
{
	wl_pfn_net_info_t *netinfo = ptr->netinfo;
	uint32 i, j;

	if (WLC_E_PFN_NET_FOUND == event_type) {
		printf("WLC_E_PFN_NET_FOUND:\n");
	} else if (WLC_E_PFN_NET_LOST == event_type) {
		printf("WLC_E_PFN_NET_LOST:\n");
	} else {
		return;
	}
	printf("ver %d, status %d, count %d\n",
	        ptr->version, ptr->status, ptr->count);
	for (i = 0; i < ptr->count; i++) {
		printf("%d. ", i + 1);
		for (j = 0; j < netinfo->pfnsubnet.SSID_len; j++)
			printf("%c", netinfo->pfnsubnet.SSID[j]);
		printf("\n");
		printf("BSSID %02x:%02x:%02x:%02x:%02x:%02x\n",
		        netinfo->pfnsubnet.BSSID.octet[0],
		        netinfo->pfnsubnet.BSSID.octet[1],
		        netinfo->pfnsubnet.BSSID.octet[2],
		        netinfo->pfnsubnet.BSSID.octet[3],
		        netinfo->pfnsubnet.BSSID.octet[4],
		        netinfo->pfnsubnet.BSSID.octet[5]);
		printf("channel %d, RSSI %d, timestamp %d\n",
		      netinfo->pfnsubnet.channel, netinfo->RSSI, netinfo->timestamp);

		netinfo++;
	}
}

static int
wl_pfn_event_check(void *wl, cmd_t *cmd, char **argv)
{
	int                 fd, err;
	struct sockaddr_ll	sll;
	struct ifreq        ifr;
	char                ifnames[IFNAMSIZ] = {"eth1"};
	bcm_event_t         * event;
	char                data[512];
	int                 event_type;
	struct ether_addr   *addr;
	char                eabuf[ETHER_ADDR_STR_LEN];
	wl_pfn_scanresults_t *ptr;
	wl_pfn_net_info_t   *info;
	uint32              i, j;
	uint32              foundcnt, lostcnt;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	if (*++argv)
		strncpy(ifnames, *argv, IFNAMSIZ);

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifnames, IFNAMSIZ);

	fd = socket(PF_PACKET, SOCK_RAW, hton16(ETHER_TYPE_BRCM));
	if (fd < 0) {
		printf("Cannot create socket %d\n", fd);
		return -1;
	}

	err = ioctl(fd, SIOCGIFINDEX, &ifr);
	if (err < 0) {
		printf("Cannot get index %d\n", err);
		return -1;
	}

	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = hton16(ETHER_TYPE_BRCM);
	sll.sll_ifindex = ifr.ifr_ifindex;
	err = bind(fd, (struct sockaddr *)&sll, sizeof(sll));
	if (err < 0) {
		printf("Cannot get index %d\n", err);
		return -1;
	}

	while (1) {
		recv(fd, data, sizeof(data), 0);
		event = (bcm_event_t *)data;
		addr = (struct ether_addr *)&(event->event.addr);

		event_type = ntoh32(event->event.event_type);

		if (addr != NULL) {
			sprintf(eabuf, "%02x:%02x:%02x:%02x:%02x:%02x",
				(uchar)addr->octet[0]&0xff,
				(uchar)addr->octet[1]&0xff,
				(uchar)addr->octet[2]&0xff,
				(uchar)addr->octet[3]&0xff,
				(uchar)addr->octet[4]&0xff,
				(uchar)addr->octet[5]&0xff);
		}

		if (ntoh32(event->event.datalen)) {
			if (WLC_E_PFN_SCAN_COMPLETE == event_type) {
				ptr = (wl_pfn_scanresults_t *)(data + sizeof(bcm_event_t));
				info = ptr->netinfo;
				foundcnt = ptr->count & 0xffff;
				lostcnt = ptr->count >> 16;
				printf("ver %d, status %d, found %d, lost %d\n",
				        ptr->version, ptr->status, foundcnt, lostcnt);
				if (foundcnt)
					printf("Network found:\n");
				for (i = 0; i < foundcnt; i++) {
					printf("%d. ", i + 1);
					for (j = 0; j < info->pfnsubnet.SSID_len; j++)
						printf("%c", info->pfnsubnet.SSID[j]);
					printf("\n");
					printf("BSSID %02x:%02x:%02x:%02x:%02x:%02x\n",
					        info->pfnsubnet.BSSID.octet[0],
					        info->pfnsubnet.BSSID.octet[1],
					        info->pfnsubnet.BSSID.octet[2],
					        info->pfnsubnet.BSSID.octet[3],
					        info->pfnsubnet.BSSID.octet[4],
					        info->pfnsubnet.BSSID.octet[5]);
					printf("channel %d, RSSI %d, timestamp %d\n",
					 info->pfnsubnet.channel, info->RSSI, info->timestamp);
					info++;
				}
				if (lostcnt)
					printf("Network lost:\n");
				for (i = 0; i < lostcnt; i++) {
					printf("%d. ", i + 1);
					for (j = 0; j < info->pfnsubnet.SSID_len; j++)
						printf("%c", info->pfnsubnet.SSID[j]);
					printf("\n");
					printf("BSSID %02x:%02x:%02x:%02x:%02x:%02x\n",
					        info->pfnsubnet.BSSID.octet[0],
					        info->pfnsubnet.BSSID.octet[1],
					        info->pfnsubnet.BSSID.octet[2],
					        info->pfnsubnet.BSSID.octet[3],
					        info->pfnsubnet.BSSID.octet[4],
					        info->pfnsubnet.BSSID.octet[5]);
					printf("channel %d, RSSI %d, timestamp %d\n",
					 info->pfnsubnet.channel, info->RSSI, info->timestamp);
					info++;
				}
			} else if ((WLC_E_PFN_NET_FOUND == event_type) ||
			           (WLC_E_PFN_NET_LOST == event_type)) {
				wl_pfn_printnet(
				   (wl_pfn_scanresults_t *)(data + sizeof(bcm_event_t)),
				                            event_type);
			}

			if (WLC_E_LINK == event_type || WLC_E_NDIS_LINK == event_type) {
				if (ntoh16(event->event.flags) & WLC_EVENT_MSG_LINK)
					printf("MACEVENT Link up :%s\n", eabuf);
				else
					printf("MACEVENT Link down :%s\n", eabuf);
			}
		} else {
			if (WLC_E_PFN_SCAN_NONE == event_type) {
				printf("Got WLC_E_PFN_SCAN_NONE\n");
			}
			if (WLC_E_PFN_SCAN_ALLGONE == event_type) {
				printf("Got WLC_E_PFN_SCAN_ALLGONE\n");
			}
		}
	}
	return (0);
}

#define ESCAN_EVENTS_BUFFER_SIZE 2048

static int
wl_escan_event_check(void *wl, cmd_t *cmd, char **argv)
{
	int					fd, err, i, octets;
	struct sockaddr_ll	sll;
	struct ifreq		ifr;
	char				ifnames[IFNAMSIZ] = {"eth1"};
	uint8               print_flag = 4;
	bcm_event_t			* event;
	uint32              status;
	char*				data;
	int					event_type;
	uint8	event_inds_mask[WL_EVENTING_MASK_LEN];	/* 128-bit mask */

	wl_escan_result_t* escan_data;


	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	if (*++argv) {
		strncpy(ifnames, *argv, (IFNAMSIZ - 1));
		if (*++argv)
			print_flag = atoi(*argv);
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifnames, (IFNAMSIZ - 1));

	memset(event_inds_mask, '\0', WL_EVENTING_MASK_LEN);
	event_inds_mask[WLC_E_ESCAN_RESULT / 8] |= 1 << (WLC_E_ESCAN_RESULT % 8);
	if ((err = wlu_iovar_set(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN)))
		return (err);

	fd = socket(PF_PACKET, SOCK_RAW, hton16(ETHER_TYPE_BRCM));
	if (fd < 0) {
		printf("Cannot create socket %d\n", fd);
		return -1;
	}

	err = ioctl(fd, SIOCGIFINDEX, &ifr);
	if (err < 0) {
		printf("Cannot get index %d\n", err);
		return -1;
	}

	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = hton16(ETHER_TYPE_BRCM);
	sll.sll_ifindex = ifr.ifr_ifindex;
	err = bind(fd, (struct sockaddr *)&sll, sizeof(sll));
	if (err < 0) {
		printf("Cannot bind %d\n", err);
		return -1;
	}

	data = (char*)malloc(ESCAN_EVENTS_BUFFER_SIZE);

	if (data == NULL) {
		printf("Cannot not allocate %d bytes for events receive buffer\n",
			ESCAN_EVENTS_BUFFER_SIZE);
		return -1;
	}

	while (1) {
		octets = recv(fd, data, ESCAN_EVENTS_BUFFER_SIZE, 0);
		event = (bcm_event_t *)data;

		event_type = ntoh32(event->event.event_type);

		if ((event_type == WLC_E_ESCAN_RESULT) && (octets > 0)) {
			escan_data = (wl_escan_result_t*)&data[sizeof(bcm_event_t)];
			status = ntoh32(event->event.status);

			if (print_flag & 1)
				printf("WLC_E_ESCAN_RESULT, (sync_id,status) = (%d,%d)\n",
				escan_data->sync_id, status);

			if (print_flag & 2)
				for (i = 0; i < escan_data->bss_count; i++)
					dump_bss_info(&escan_data->bss_info[i]);

			if (print_flag & 4) {
				if (status == WLC_E_STATUS_PARTIAL) {
					printf("sync_id: %d, WLC_E_STATUS_PARTIAL\n",
					escan_data->sync_id);
					for (i = 0; i < escan_data->bss_count; i++)
						dump_bss_info(&escan_data->bss_info[i]);
				}
				if (status == WLC_E_STATUS_SUCCESS)
					printf("sync_id: %d, WLC_E_STATUS_SUCCESS => SCAN_DONE\n",
					escan_data->sync_id);
				if ((status != WLC_E_STATUS_SUCCESS) &&
					(status != WLC_E_STATUS_PARTIAL))
					printf("sync_id: %d, status:%d, misc. error/abort\n",
					escan_data->sync_id, status);
			}

			if (print_flag & 8) {
				int    remainder = escan_data->bss_info[0].ie_length;
				int    processed = sizeof(wl_escan_result_t);
				uint8* iebuf = &((uint8*)escan_data)[sizeof(wl_escan_result_t)];

				if (status != WLC_E_STATUS_PARTIAL)
					continue;

				printf("MOREINFO: (sync_id,buflen,ielen) = (%d,%d,%d)\n",
					escan_data->sync_id,
					escan_data->buflen,
					escan_data->bss_info[0].ie_length);

				/* do a tlv sanity check */
				while (remainder > 0) {
					processed += 1 + 1 + iebuf[1];
					remainder -= 1 + 1 + iebuf[1];
					iebuf     += 1 + 1 + iebuf[1];
				}
				if (processed >= ESCAN_EVENTS_BUFFER_SIZE)
					break;

				if (remainder != 0) {
					printf("ERROR: IE tlv sanity check failed for "
						"(ssid,sync_id,buflen,ielen,remainder) = "
						"(%s,%d,%d,%d,%d)\n",
						escan_data->bss_info[0].SSID,
						escan_data->sync_id, escan_data->buflen,
						escan_data->bss_info[0].ie_length,
						remainder);
					iebuf = &((uint8*)escan_data)[sizeof(wl_escan_result_t)];
					if ((escan_data->buflen - sizeof(wl_escan_result_t)) > 0) {
						for (i = 0;
						i < (int)(escan_data->buflen -
							sizeof(wl_escan_result_t));
						i++) {
							printf("%02x ", iebuf[i]);
						}
						printf("\n");
					}
				}
			}
		}
	}

	/* if we ever reach here */
	free(data);

	return (0);
}

struct escan_bss {
	struct escan_bss *next;
	wl_bss_info_t bss[1];
};
#define ESCAN_BSS_FIXED_SIZE 4

static int
wl_escanresults(void *wl, cmd_t *cmd, char **argv)
{
	int params_size = (WL_SCAN_PARAMS_FIXED_SIZE + OFFSETOF(wl_escan_params_t, params)) +
	    (WL_NUMCHANNELS * sizeof(uint16));
	wl_escan_params_t *params;
	int fd, err, octets;
	struct sockaddr_ll sll;
	struct ifreq ifr;
	char ifnames[IFNAMSIZ] = {"eth1"};
	bcm_event_t *event;
	uint32 status;
	char *data;
	int event_type;
	uint8 event_inds_mask[WL_EVENTING_MASK_LEN];	/* 128-bit mask */
	wl_escan_result_t *escan_data;
	struct escan_bss *escan_bss_head = NULL;
	struct escan_bss *escan_bss_tail = NULL;
	struct escan_bss *result;

	params_size += WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t);
	params = (wl_escan_params_t*)malloc(params_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan params\n", params_size);
		return -1;
	}
	memset(params, 0, params_size);

	err = wl_scan_prep(wl, cmd, argv, &params->params, &params_size);
	if (err)
		goto exit2;

	memset(&ifr, 0, sizeof(ifr));
	if (wl)
		strncpy(ifr.ifr_name, ((struct ifreq *)wl)->ifr_name, (IFNAMSIZ - 1));
	else
		strncpy(ifr.ifr_name, ifnames, (IFNAMSIZ - 1));

	memset(event_inds_mask, '\0', WL_EVENTING_MASK_LEN);

	/* Read the event mask from driver and unmask the event WLC_E_ESCAN_RESULT */
	if ((err = wlu_iovar_get(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN)))
		goto exit2;
	event_inds_mask[WLC_E_ESCAN_RESULT / 8] |= 1 << (WLC_E_ESCAN_RESULT % 8);
	if ((err = wlu_iovar_set(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN)))
		goto exit2;

	fd = socket(PF_PACKET, SOCK_RAW, hton16(ETHER_TYPE_BRCM));
	if (fd < 0) {
		printf("Cannot create socket %d\n", fd);
		err = -1;
		goto exit2;
	}

	err = ioctl(fd, SIOCGIFINDEX, &ifr);
	if (err < 0) {
		printf("Cannot get index %d\n", err);
		goto exit2;
	}

	/* bind the socket first before starting escan so we won't miss any event */
	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = hton16(ETHER_TYPE_BRCM);
	sll.sll_ifindex = ifr.ifr_ifindex;
	err = bind(fd, (struct sockaddr *)&sll, sizeof(sll));
	if (err < 0) {
		printf("Cannot bind %d\n", err);
		goto exit2;
	}

	params->version = htod32(ESCAN_REQ_VERSION);
	params->action = htod16(WL_SCAN_ACTION_START);

#if defined(linux)
	srand((unsigned)time(NULL));
	params->sync_id = htod16(rand() & 0xffff);
#else
	params->sync_id = htod16(4321);
#endif /* #if defined(linux) */

	params_size += OFFSETOF(wl_escan_params_t, params);
	err = wlu_iovar_setbuf(wl, "escan", params, params_size, buf, WLC_IOCTL_MAXLEN);

	data = (char*)malloc(ESCAN_EVENTS_BUFFER_SIZE);

	if (data == NULL) {
		printf("Cannot not allocate %d bytes for events receive buffer\n",
			ESCAN_EVENTS_BUFFER_SIZE);
		err = -1;
		goto exit2;
	}

	/* receive scan result */
	while (1) {
		octets = recv(fd, data, ESCAN_EVENTS_BUFFER_SIZE, 0);
		event = (bcm_event_t *)data;
		event_type = ntoh32(event->event.event_type);

		if ((event_type == WLC_E_ESCAN_RESULT) && (octets > 0)) {
			escan_data = (wl_escan_result_t*)&data[sizeof(bcm_event_t)];
			status = ntoh32(event->event.status);

			if (status == WLC_E_STATUS_PARTIAL) {
				wl_bss_info_t *bi = &escan_data->bss_info[0];
				wl_bss_info_t *bss;

				/* check if we've received info of same BSSID */
				for (result = escan_bss_head; result; result = result->next) {
					bss = result->bss;

#define WLC_BSS_RSSI_ON_CHANNEL 0x0002 /* Copied from wlc.h. Is there a better way to do this? */

					if (!wlu_bcmp(bi->SSID, bss->SSID, ETHER_ADDR_LEN) &&
						CHSPEC_BAND(bi->chanspec) ==
						CHSPEC_BAND(bss->chanspec) &&
						bi->SSID_len == bss->SSID_len &&
						!wlu_bcmp(bi->SSID, bss->SSID, bi->SSID_len))
						break;
				}

				if (!result) {
					/* New BSS. Allocate memory and save it */
					struct escan_bss *ebss = malloc(ESCAN_BSS_FIXED_SIZE
						+ bi->length);

					if (!ebss) {
						perror("can't allocate memory for bss");
						goto exit1;
					}

					ebss->next = NULL;
					memcpy(&ebss->bss, bi, bi->length);
					if (escan_bss_tail) {
						escan_bss_tail->next = ebss;
					}
					else {
						escan_bss_head = ebss;
					}
					escan_bss_tail = ebss;
				}
				else {
					/* We've got this BSS. Update rssi if necessary */
					if ((bss->flags & WLC_BSS_RSSI_ON_CHANNEL) ==
						(bi->flags & WLC_BSS_RSSI_ON_CHANNEL)) {
						/* preserve max RSSI if the measurements are
						 * both on-channel or both off-channel
						 */
						bss->RSSI = MAX(bss->RSSI, bi->RSSI);
					} else if ((bss->flags & WLC_BSS_RSSI_ON_CHANNEL) &&
						(bi->flags & WLC_BSS_RSSI_ON_CHANNEL) == 0) {
						/* preserve the on-channel rssi measurement
						 * if the new measurement is off channel
						*/
						bss->RSSI = bi->RSSI;
						bss->flags |= WLC_BSS_RSSI_ON_CHANNEL;
					}
				}
			}
			else if (status == WLC_E_STATUS_SUCCESS) {
				/* Escan finished. Let's go dump the results. */
				break;
			}
			else {
				printf("sync_id: %d, status:%d, misc. error/abort\n",
					escan_data->sync_id, status);
				goto exit1;
			}
		}
	}

	/* print scan results */
	for (result = escan_bss_head; result; result = result->next) {
		dump_bss_info(result->bss);
	}

exit1:
	/* free scan results */
	result = escan_bss_head;
	while (result) {
		struct escan_bss *tmp = result->next;
		free(result);
		result = tmp;
	}

	free(data);
	close(fd);
exit2:
	free(params);

	/* Read the event mask from driver and mask the event WLC_E_ESCAN_RESULT */
	if (!(err = wlu_iovar_get(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN))) {
		event_inds_mask[WLC_E_ESCAN_RESULT / 8] &= (~(1 << (WLC_E_ESCAN_RESULT % 8)));
		err = wlu_iovar_set(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN);
	}

	return err;
}
#endif   /* linux */

static int
wl_event_filter(void *wl, cmd_t *cmd, char **argv)
{
	int     err;
	uint8   event_inds_mask[WL_EVENTING_MASK_LEN];  /* 128-bit mask */

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	memset(event_inds_mask, '\0', WL_EVENTING_MASK_LEN);

	/* Register for following event for pfn */
	event_inds_mask[WLC_E_LINK / 8] |= 1 << (WLC_E_LINK % 8);
	event_inds_mask[WLC_E_PFN_NET_FOUND / 8] |= 1 << (WLC_E_PFN_NET_FOUND % 8);
	event_inds_mask[WLC_E_PFN_NET_LOST / 8] |= 1 << (WLC_E_PFN_NET_LOST % 8);
	event_inds_mask[WLC_E_PFN_SCAN_NONE/ 8] |= 1 << (WLC_E_PFN_SCAN_NONE % 8);
	event_inds_mask[WLC_E_PFN_SCAN_ALLGONE/ 8] |= 1 << (WLC_E_PFN_SCAN_ALLGONE % 8);

	if ((err = wlu_iovar_set(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN)))
		return (err);

	return (0);
}
#endif /* WLPFN */

static dbg_msg_t toe_cmpnt[] = {
	{TOE_TX_CSUM_OL,	"tx_csum_ol"},
	{TOE_RX_CSUM_OL,	"rx_csum_ol"},
	{0,			NULL}
};

static dbg_msg_t arpoe_cmpnt[] = {
	{ARP_OL_AGENT,		"agent"},
	{ARP_OL_SNOOP,		"snoop"},
	{ARP_OL_HOST_AUTO_REPLY, "host_auto_reply"},
	{ARP_OL_PEER_AUTO_REPLY, "peer_auto_reply"},
	{0,			NULL}
};

/*
 *  Tcpip Offload Component-wise get/set control.
 */
static int
wl_offload_cmpnt(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i;
	uint val, last_val = 0, cmpnt_add = 0, cmpnt_del = 0;
	char *endptr;
	dbg_msg_t *dbg_msg = NULL;
	void *ptr = NULL;
	int cmpnt;

	if (strcmp(cmd->name, "toe_ol") == 0)
		dbg_msg = toe_cmpnt;
	else if (strcmp(cmd->name, "arp_ol") == 0)
		dbg_msg = arpoe_cmpnt;

	if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return (ret);
	cmpnt = dtoh32(*(int *)ptr);

	if (!*++argv) {
		printf("0x%x ", cmpnt);
		for (i = 0; (val = dbg_msg[i].value); i++) {
			if ((cmpnt & val) && (val != last_val))
				printf(" %s", dbg_msg[i].string);
			last_val = val;
		}
		printf("\n");
		return (0);
	}

	while (*argv) {
		char *s = *argv;
		if (*s == '+' || *s == '-')
			s++;
		else
			cmpnt_del = ~0;	/* make the whole list absolute */
		val = strtoul(s, &endptr, 0);
		/* not a plain integer if not all the string was parsed by strtoul */
		if (*endptr != '\0') {
			for (i = 0; (val = dbg_msg[i].value); i++)
				if (stricmp(dbg_msg[i].string, s) == 0)
					break;
			if (!val)
				goto usage;
		}
		if (**argv == '-')
			cmpnt_del |= val;
		else
			cmpnt_add |= val;
		++argv;
	}

	cmpnt &= ~cmpnt_del;
	cmpnt |= cmpnt_add;

	cmpnt = htod32(cmpnt);
	return (wlu_var_setbuf(wl, cmd->name, &cmpnt, sizeof(int)));

usage:
	fprintf(stderr, "msg values may be a list of numbers or names from the following set.\n");
	fprintf(stderr, "Use a + or - prefix to make an incremental change.");

	for (i = 0; (val = dbg_msg[i].value); i++) {
		if (val != last_val)
			fprintf(stderr, "\n0x%04x %s", val, dbg_msg[i].string);
		else
			fprintf(stderr, ", %s", dbg_msg[i].string);
		last_val = val;
	}
	fprintf(stderr, "\n");

	return 0;
}

#if defined(WLNDOE) || defined(WLOFFLD)
/*
 * If a host IP address is given, add it to the host-cache,
 * e.g. "wl nd_hostip fe00:0:0:0:0:290:1fc0:18c0 ".
 * If no address is given, dump all the addresses.
 */
static int
wl_hostipv6(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i;
	struct ipv6_addr ipa_set, *ipa_get, null_ipa;
	uint16 *ip_addr;
	if (!*++argv) {
		/* Get */
		void *ptr = NULL;

		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;

		ip_addr = (uint16*)ptr;
		memset(null_ipa.addr, 0, IPV6_ADDR_LEN);
		for (ipa_get = (struct ipv6_addr *)ptr;
			 memcmp(null_ipa.addr, ipa_get->addr, IPV6_ADDR_LEN) != 0;
			 ipa_get++) {
			/* Print ipv6 Addr */
			for (i = 0; i < 8; i++) {
				printf("%x", ntoh16(ip_addr[i]));
				if (i < 7)
					printf(":");
			}
			printf("\r\n");

			ip_addr += 8;
		}
	} else {
		/* Add */
		if (!wl_atoipv6(*argv, &ipa_set))
			return -1;

		/* we add one ip-addr at a time */
		return wlu_var_setbuf(wl, cmd->name, &ipa_set, IPV6_ADDR_LEN);
	}
	return ret;
}
#endif /* defined(WLOFFLD) || defined(WLNDOE) */

#ifdef WLNDOE
static int
wl_ndstatus(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	struct nd_ol_stats_t *nd_stats;

	if (!*++argv) {
	/* Get */
	void *ptr = NULL;

		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;

		nd_stats = (struct nd_ol_stats_t *)ptr;

		printf("host_ip_entries %d\r\n", nd_stats->host_ip_entries);
		printf("host_ip_overflow %d\r\n", nd_stats->host_ip_overflow);
		printf("peer_request %d\r\n", nd_stats->peer_request);
		printf("peer_request_drop %d\r\n", nd_stats->peer_request_drop);
		printf("peer_reply_drop %d\r\n", nd_stats->peer_reply_drop);
		printf("peer_service %d\r\n", nd_stats->peer_service);
	} else {
		printf("Cannot set nd stats\n");
	}

	return 0;
}

/*
 * If a solicit IP address is given, add it
 * e.g. "wl nd_solicitip fe00:0:0:0:0:290:1fc0:18c0 ".
 * If no address is given, dump all the addresses.
 */
static int
wl_solicitipv6(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i;
	struct ipv6_addr ipa_set, *ipa_get, null_ipa;
	uint16 *ip_addr;
	if (!*++argv) {
		/* Get */
		void *ptr = NULL;

		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;

		ip_addr = (uint16*)ptr;
		memset(null_ipa.addr, 0, IPV6_ADDR_LEN);
		for (ipa_get = (struct ipv6_addr *)ptr;
			 memcmp(null_ipa.addr, ipa_get->addr, IPV6_ADDR_LEN) != 0;
			 ipa_get++) {
			/* Print ipv6 Addr */
			for (i = 0; i < 8; i++) {
				printf("%x", ntoh16(ip_addr[i]));
				if (i < 7)
					printf(":");
			}
			printf("\r\n");

			ip_addr += 8;
		}
	} else {
		/* Add */
		if (!wl_atoipv6(*argv, &ipa_set))
			return -1;

		/* we add one ip-addr at a time */
		return wlu_var_setbuf(wl, cmd->name, &ipa_set, IPV6_ADDR_LEN);
	}
	return ret;
}

/*
 * If a remote IP address is given, add it
 * e.g. "wl nd_remoteip fe00:0:0:0:0:290:1fc0:18c0 ".
 * If no address is given, dump the addresses.
 */
static int
wl_remoteipv6(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i;
	struct ipv6_addr ipa_set, *ipa_get, null_ipa;
	uint16 *ip_addr;
	if (!*++argv) {
		/* Get */
		void *ptr = NULL;

		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;

		ip_addr = (uint16*)ptr;
		memset(null_ipa.addr, 0, IPV6_ADDR_LEN);
		for (ipa_get = (struct ipv6_addr *)ptr;
			 memcmp(null_ipa.addr, ipa_get->addr, IPV6_ADDR_LEN) != 0;
			 ipa_get++) {
			/* Print ipv6 Addr */
			for (i = 0; i < 8; i++) {
				printf("%x", ntoh16(ip_addr[i]));
				if (i < 7)
					printf(":");
			}
			printf("\r\n");

			ip_addr += 8;
		}
	} else {
		/* Add */
		if (!wl_atoipv6(*argv, &ipa_set))
			return -1;

		/* we add one ip-addr at a time */
		return wlu_var_setbuf(wl, cmd->name, &ipa_set, IPV6_ADDR_LEN);
	}
	return ret;
}
#endif /* WLNDOE */
/*
 * If a host IP address is given, add it to the host-cache, e.g. "wl arp_hostip 192.168.1.1".
 * If no address is given, dump all the addresses.
 */
static int
wl_hostip(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	struct ipv4_addr ipa_set, *ipa_get, null_ipa;

	if (!*++argv) {
		/* Get */
		void *ptr = NULL;
		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;

		memset(null_ipa.addr, 0, IPV4_ADDR_LEN);

		for (ipa_get = (struct ipv4_addr *)ptr;
		     memcmp(null_ipa.addr, ipa_get->addr, IPV4_ADDR_LEN) != 0;
		     ipa_get++)
			printf("%s\n", wl_iptoa(ipa_get));

		printf("Total %d host addresses\n", (int)(ipa_get - (struct ipv4_addr *)ptr));
	} else {
		/* Add */
		if (!wl_atoip(*argv, &ipa_set))
			return -1;
		/* we add one ip-addr at a time */
		return wlu_var_setbuf(wl, cmd->name, &ipa_set, sizeof(IPV4_ADDR_LEN));
	}

	return ret;
}

static int
wl_ol_notify_bcn_ie(void *wl, cmd_t *cmd, char **argv)
{
	int ret = -1;
	void *ptr = NULL;

#define	VNDR_IE_ID		(221)
#define OLMSG_BCN_MAX_IE	(222)

	struct beacon_ie_notify_cmd {
		uint32		id;
		uint32		enable;
		struct ipv4_addr vndriemask;
	} beacon_notification_cmd;

	memset(&beacon_notification_cmd, 0, sizeof(beacon_notification_cmd));

	++argv; /* skip the command iteself */

	if (!*argv) {
		/* Get Everything */
		beacon_notification_cmd.id = -1; /* -1 indicates NONE */
		ret = wlu_var_getbuf(wl, cmd->name, &beacon_notification_cmd,
			sizeof(beacon_notification_cmd), &ptr);
	} else {
		if (stricmp(*argv, "enable") == 0) { /* Enable Global flag */
			beacon_notification_cmd.id = -1; /* -1 indicates NONE */
			beacon_notification_cmd.enable = 1;
			return wlu_var_setbuf(wl, cmd->name, &beacon_notification_cmd,
				sizeof(beacon_notification_cmd));
		} else if (stricmp(*argv, "disable") == 0) { /* Disable Global flag */
			beacon_notification_cmd.id = -1; /* -1 indicates NONE */
			beacon_notification_cmd.enable = 0;
			return wlu_var_setbuf(wl, cmd->name, &beacon_notification_cmd,
				sizeof(beacon_notification_cmd));
		} else { /* Get/Set Enable/Disable some ID */

		    beacon_notification_cmd.id = (uint8) atoi(*argv);

		    if (beacon_notification_cmd.id == VNDR_IE_ID) {
			/* Get VNDR OUI */
			++argv;
			if (*argv) { /* Get a specific Vendor IE */
				if (!wl_atoip(*argv, &beacon_notification_cmd.vndriemask)) {
					return -1;
				}
				++argv;
				/* Get enable/disable flag */
				if (*argv) { /* Set a specific Vendor ie */
					if (stricmp(*argv, "enable") == 0)
						beacon_notification_cmd.enable = 1;
					else if (stricmp(*argv, "disable") == 0)
						beacon_notification_cmd.enable = 0;
					else
						return -1;

					return wlu_var_setbuf(wl, cmd->name,
						&beacon_notification_cmd,
						sizeof(beacon_notification_cmd));
				} else {
					ret = wlu_var_getbuf(wl, cmd->name,
						&beacon_notification_cmd,
						sizeof(beacon_notification_cmd),
						&ptr);
				}
			} else { /* Get ALL Vendor IE */
				ret = wlu_var_getbuf(wl, cmd->name,
					&beacon_notification_cmd,
					sizeof(beacon_notification_cmd),
					&ptr);
			}
		    } else {
			if (beacon_notification_cmd.id > OLMSG_BCN_MAX_IE) {
				return -1;
			}
			/* Get enable/disable flag */
			++argv;
			if(*argv) { /* Set IE ID */
				if (stricmp(*argv, "enable") == 0)
					beacon_notification_cmd.enable = 1;
				else if (stricmp(*argv, "disable") == 0)
					beacon_notification_cmd.enable = 0;
				else
					return -1;
				return wlu_var_setbuf(wl, cmd->name,
					&beacon_notification_cmd,
					sizeof(beacon_notification_cmd));
			} else { /* Get IE ID */
				ret = wlu_var_getbuf(wl, cmd->name,
					&beacon_notification_cmd,
					sizeof(beacon_notification_cmd),
					&ptr);
			}
		    }
		}
	}
	if ((ret >= 0) && (ptr != NULL)) {
		printf("%s\n", (char *)ptr); /* Print Everything */
	}
	return ret;
}

static int
wl_mcast_ackmac(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	struct ether_addr ea;

	if (!*++argv) {
		if ((ret = wlu_iovar_get(wl, cmd->name, &ea, ETHER_ADDR_LEN)) < 0)
			return ret;
		printf("%s\n", wl_ether_etoa(&ea));
	} else {
		if (!wl_ether_atoe(*argv, &ea))
			return -1;
		if ((ea.octet[0] == 1 && ea.octet[1] == 0 && ea.octet[2] == 0x5e &&
			!(ea.octet[3] & 0x80)) || (ea.octet[0] == 0x33 && ea.octet[1] == 0x33))
		{
			ret = wlu_iovar_set(wl, cmd->name, &ea, ETHER_ADDR_LEN);
		}
		else {
			fprintf(stderr, "multicast mac started with 01:00:5e:0... or 33:33:...\n");
			ret = -1;
		}
	}
	return ret;
}

static int
wl_mcast_status(void *wl, cmd_t *cmd, char **argv)
{
	int err, i;
	wl_relmcast_status_t status;

	if (!*++argv) {
		/* Get */
		if ((err = wlu_iovar_get(wl, cmd->name, (void *) &status,
			(sizeof(wl_relmcast_status_t)))) < 0)
			return (err);

		if (status.ver != WL_RELMCAST_VER)
			printf("Wrong Version %d %d\n", WL_RELMCAST_VER, status.ver);
		else if (status.num == 0)
			printf("No clients associated\n");
		else {
			for (i = 0; i < status.num; i++)
			{
				printf("%s\t%d\t%c%c%c\n", wl_ether_etoa(&status.clients[i].addr),
					status.clients[i].rssi,
					((status.clients[i].flag & WL_RELMCAST_FLAG_ACTIVEACKER) ?
					'A' : ' '),
					((status.clients[i].flag & WL_RELMCAST_FLAG_INBLACKLIST) ?
					'B' : ' '),
					((status.clients[i].flag & WL_RELMCAST_FLAG_RELMCAST) ?
					'R' : ' '));
			}
		}
	} else {
		printf("Cannot set reliable multicast status\n");
	}

	return 0;
}

static int
wl_arp_stats(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	struct arp_ol_stats_t *arpstats;

	if (!*++argv) {
		/* Get */
		void *ptr = NULL;

		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;
		arpstats = (struct arp_ol_stats_t *)ptr;
		printf("host_ip_entries = %d\n", dtoh32(arpstats->host_ip_entries));
		printf("host_ip_overflow = %d\n", dtoh32(arpstats->host_ip_overflow));
		printf("arp_table_entries = %d\n", dtoh32(arpstats->arp_table_entries));
		printf("arp_table_overflow = %d\n", dtoh32(arpstats->arp_table_overflow));
		printf("host_request = %d\n", dtoh32(arpstats->host_request));
		printf("host_reply = %d\n", dtoh32(arpstats->host_reply));
		printf("host_service = %d\n", dtoh32(arpstats->host_service));
		printf("peer_request = %d\n", dtoh32(arpstats->peer_request));
		printf("peer_request_drop = %d\n", dtoh32(arpstats->peer_request_drop));
		printf("peer_reply = %d\n", dtoh32(arpstats->peer_reply));
		printf("peer_reply_drop = %d\n", dtoh32(arpstats->peer_reply_drop));
		printf("peer_service = %d\n", dtoh32(arpstats->peer_service));
		printf("host_ip_entries = %d\n", dtoh32(arpstats->host_ip_entries));
	} else
		printf("Cannot set arp stats, use 'wl arp_stats_clear' to clear the counters\n");

	return 0;
}

static int
wl_toe_stats(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	struct toe_ol_stats_t *toestats;

	if (!*++argv) {
		/* Get */
		void *ptr = NULL;

		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;
		toestats = (struct toe_ol_stats_t *)ptr;
		printf("tx_summed = %d\n", dtoh32(toestats->tx_summed));
		printf("tx_iph_fill = %d\n", dtoh32(toestats->tx_iph_fill));
		printf("tx_tcp_fill = %d\n", dtoh32(toestats->tx_tcp_fill));
		printf("tx_udp_fill = %d\n", dtoh32(toestats->tx_udp_fill));
		printf("tx_icmp_fill = %d\n", dtoh32(toestats->tx_icmp_fill));
		printf("rx_iph_good = %d\n", dtoh32(toestats->rx_iph_good));
		printf("rx_iph_bad = %d\n", dtoh32(toestats->rx_iph_bad));
		printf("rx_tcp_good = %d\n", dtoh32(toestats->rx_tcp_good));
		printf("rx_tcp_bad = %d\n", dtoh32(toestats->rx_tcp_bad));
		printf("rx_udp_good = %d\n", dtoh32(toestats->rx_udp_good));
		printf("rx_udp_bad = %d\n", dtoh32(toestats->rx_udp_bad));
		printf("rx_icmp_good = %d\n", dtoh32(toestats->rx_icmp_good));
		printf("rx_icmp_bad = %d\n", dtoh32(toestats->rx_icmp_bad));
		printf("tx_tcp_errinj = %d\n", dtoh32(toestats->tx_tcp_errinj));
		printf("tx_udp_errinj = %d\n", dtoh32(toestats->tx_udp_errinj));
		printf("tx_icmp_errinj = %d\n", dtoh32(toestats->tx_icmp_errinj));
		printf("rx_tcp_errinj = %d\n", dtoh32(toestats->rx_tcp_errinj));
		printf("rx_udp_errinj = %d\n", dtoh32(toestats->rx_udp_errinj));
		printf("rx_icmp_errinj = %d\n", dtoh32(toestats->rx_icmp_errinj));
	} else
		printf("Cannot set toe stats, use 'wl toe_stats_clear' to clear the counters\n");

	return 0;
}

static void
wl_rate_histo_print(wl_mac_ratehisto_res_t *rate_histo_res)
{
	uint i, nss;

	printf("Rates\n");
	for (i = 0; i < (WLC_MAXRATE + 1); i++) {
		if (rate_histo_res->rate[i]) {
			if (DIV_REM(i, 2))
				printf("%.2d\t%d.%d Mbit/s\n",
					rate_histo_res->rate[i], DIV_QUO(i, 2), DIV_REM(i, 2)/10);
			else
				printf("%.2d\t%d Mbit/s\n",
					rate_histo_res->rate[i], DIV_QUO(i, 2));
		}
	}

	printf("MCS indexes:\n");
	for (i = 0; i < (WL_RATESET_SZ_HT_MCS * WL_TX_CHAINS_MAX); i++) {
		if (rate_histo_res->mcs[i]) {
			printf("%d\tMCS %d\n", rate_histo_res->mcs[i], i);
		}
	}

	printf("VHT indexes:\n");
	for (nss = 0; nss < WL_TX_CHAINS_MAX; nss++) {
		for (i = 0; i < WL_RATESET_SZ_VHT_MCS; i++) {
			if (rate_histo_res->vht[i][nss]) {
				printf("%d\tVHT %d Nss %d\n", rate_histo_res->vht[i][nss], i,
					nss + 1);
			}
		}
	}

	return;
}

static int
wl_rate_histo(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr = NULL;
	int err;
	wl_mac_ratehisto_res_t *rate_histo_res;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return err;

	rate_histo_res = (wl_mac_ratehisto_res_t *)ptr;

	wl_rate_histo_print(rate_histo_res);

	return 0;
}

static int
wl_mac_rate_histo(void *wl, cmd_t *cmd, char **argv)
{
	struct ether_addr ea;
	int buflen, err;
	wl_mac_ratehisto_cmd_t *rate_histo_cmd;
	wl_mac_ratehisto_res_t *rate_histo_res;

	if (!*++argv || !wl_ether_atoe(*argv, &ea))
		return -1;

	strcpy(buf, "mac_rate_histo");
	buflen = strlen(buf) + 1;
	rate_histo_cmd = (wl_mac_ratehisto_cmd_t *)(buf + buflen);
	memcpy((char*)&rate_histo_cmd->ea, (char*)&ea, ETHER_ADDR_LEN);

	if (*++argv)
	{
		/* The access category is obtained and checked for validity */
		rate_histo_cmd->ac_cat = (uint8)strtol(*argv, NULL, 0);
		if (!(rate_histo_cmd->ac_cat == 0x10 || rate_histo_cmd->ac_cat == 0x4)) {
			printf("Only Access Category 0x10 and 0x4 is supported\n");
			return -1;
		}

		if (*++argv) {
			/* The number of pkts to avg is obtained and checked for valid range */
			rate_histo_cmd->num_pkts = (uint8)strtol(*argv, NULL, 10);
		} else {
			/* Set default value as maximum of all access categories
			 * so that it is set to the max value below
			 */
			rate_histo_cmd->num_pkts = 64;
		}

		if (rate_histo_cmd->ac_cat == 0x10 && rate_histo_cmd->num_pkts > 64) {
			rate_histo_cmd->num_pkts = 64;
		} else if (rate_histo_cmd->ac_cat == 0x4 && rate_histo_cmd->num_pkts > 32) {
			rate_histo_cmd->num_pkts = 32;
		}
	} else {
		return -1;
	}

	if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return err;

	rate_histo_res = (wl_mac_ratehisto_res_t *)buf;

	wl_rate_histo_print(rate_histo_res);

	printf("First TSF Timestamp: %08x%08x\n", rate_histo_res->tsf_timer[0][1],
		rate_histo_res->tsf_timer[0][0]);
	printf("Last TSF Timestamp : %08x%08x\n", rate_histo_res->tsf_timer[1][1],
		rate_histo_res->tsf_timer[1][0]);

	return 0;
}
#endif /* !ATE_BUILD */

static int
wl_pkteng_stats(void *wl, cmd_t *cmd, char **argv)
{
	wl_pkteng_stats_t *stats;
	void *ptr = NULL;
	int err;
	uint16 *pktstats;
	int i, j;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return err;

	stats = ptr;
	printf("Lost frame count %d\n", dtoh32(stats->lostfrmcnt));
	printf("RSSI %d\n", dtoh32(stats->rssi));
	printf("Signal to noise ratio %d\n", dtoh32(stats->snr));
	printf("rx1mbps %d rx2mbps %d rx5mbps5 %d\n"
		"rx6mbps %d rx9mbps %d, rx11mbps %d\n"
		"rx12mbps %d rx18mbps %d rx24mbps %d\n"
		"rx36mbps %d rx48mbps %d rx54mbps %d\n",
		stats->rxpktcnt[3], stats->rxpktcnt[1], stats->rxpktcnt[2],
		stats->rxpktcnt[7], stats->rxpktcnt[11], stats->rxpktcnt[0],
		stats->rxpktcnt[6], stats->rxpktcnt[10], stats->rxpktcnt[5],
		stats->rxpktcnt[9], stats->rxpktcnt[4], stats->rxpktcnt[8]);
	pktstats = &stats->rxpktcnt[NUM_80211b_RATES+NUM_80211ag_RATES];
	for (i = 0; i < NUM_80211n_RATES/4; i++) {
		for (j = 0; j < 4; j++) {
			printf("rxmcs%d %d ", j+4*i, pktstats[j+4*i]);
		}
		printf("\n");
	}
	printf("rxmcsother %d\n", stats->rxpktcnt[NUM_80211_RATES]);
	return 0;
}

#ifndef ATE_BUILD
#if defined(WLTEST)
static int
wl_phy_sslpnphy_cga_2g(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int8 offset[14];
	char *endptr;
	int8 cga[14];
	uint8 i;

	if (!*++argv) {
		if ((err = wlu_iovar_get(wl, cmd->name, buf, 14 * sizeof(int8))) < 0)
			return err;
		printf("cga_2g : ");
		for (i = 0; i <= 13; i++) {
			cga[i] = ((int8*)buf)[i];
			printf("%d  ", cga[i]);
		}
		printf("\n");

	}
	else
	{
		for (i = 0; (i < 14) && (*argv != NULL); i++, argv++) {
			offset[i] = (int8)strtol(*argv, &endptr, 0);
			if ((endptr == *argv) || (*endptr != '\0'))
				break;
		}

		if (((i < 14) && (*argv == NULL)) || ((i == 14) && (*argv != NULL))) {
			fprintf(stderr, "Wrong number of args\n");
			return -1;
		}


		if ((err = wlu_var_setbuf(wl, cmd->name, offset, 14*sizeof(int8))) < 0)
			return err;
	}

	return 0;
}
static int
wl_phy_sslpnphy_cga_5g(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int8 offset[24];
	char *endptr;
	int8 cga[24];
	uint8 i;

	if (!*++argv) {
		if ((err = wlu_iovar_get(wl, cmd->name, buf, 24 * sizeof(int8))) < 0)
			return err;
		printf("cga_5g : ");
		for (i = 0; i <= 23; i++) {
			cga[i] = ((int8*)buf)[i];
			printf("%d  ", cga[i]);
		}
		printf("\n");

	}
	else
	{
		for (i = 0; (i < 24) && (*argv != NULL); i++, argv++) {
			offset[i] = (int8)strtol(*argv, &endptr, 0);
			if ((endptr == *argv) || (*endptr != '\0'))
				break;
		}

		if (((i < 24) && (*argv == NULL)) || ((i == 24) && (*argv != NULL))) {
			fprintf(stderr, "Wrong number of args\n");
			return -1;
		}


		if ((err = wlu_var_setbuf(wl, cmd->name, offset, 24*sizeof(int8))) < 0)
			return err;
	}

	return 0;
}
static int
wl_sslpnphy_tx_locc(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int8 offset[6];
	char *endptr;
	int8  di0, dq0, ei0, eq0, fi0, fq0;

	if (!*++argv) {
		if ((err = wlu_iovar_get(wl, cmd->name, buf, 6 * sizeof(int8))) < 0)
			return err;

		di0 = ((int8*)buf)[0];
		dq0 = ((int8*)buf)[1];
		ei0 = ((int8*)buf)[2];
		eq0 = ((int8*)buf)[3];
		fi0 = ((int8*)buf)[4];
		fq0 = ((int8*)buf)[5];

		printf("lo cc :  %d %d %d %d %d %d \n ", di0, dq0, ei0, eq0, fi0, fq0);
	}
	else
	{
		int i;
		for (i = 0; (i < 6) && (*argv != NULL); i++, argv++) {
			offset[i] = (int8)strtol(*argv, &endptr, 0);
			if ((endptr == *argv) || (*endptr != '\0'))
				break;
		}

		if (((i < 6) && (*argv == NULL)) || ((i == 6) && (*argv != NULL))) {
			fprintf(stderr, "Wrong number of args\n");
			return -1;
		}

		if (i < 6) {
			fprintf(stderr, "Bad value for %s: <%s>\n",
			        ((i == 0) ? "start" : "end"), *argv);
			return -1;
		}

		if ((err = wlu_var_setbuf(wl, cmd->name, offset, 6*sizeof(int8))) < 0)
			return err;
	}

	return 0;
}
static int
wl_sslpnphy_tx_iqcc(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int8 offset[2];
	char *endptr;
	int8  a, b;

	if (!*++argv) {
		if ((err = wlu_iovar_get(wl, cmd->name, buf, 2 * sizeof(int8))) < 0)
			return err;

		a = ((int8*)buf)[0];
		b = ((int8*)buf)[1];

		printf("iq cc : %d  %d\n ", a, b);
	}
	else
	{
		int i;
		for (i = 0; (i < 2) && (*argv != NULL); i++, argv++) {
			offset[i] = (int8)strtol(*argv, &endptr, 0);
			if ((endptr == *argv) || (*endptr != '\0'))
				break;
		}

		if (((i < 2) && (*argv == NULL)) || ((i == 2) && (*argv != NULL))) {
			fprintf(stderr, "Wrong number of args\n");
			return -1;
		}

		if (i < 2) {
			fprintf(stderr, "Bad value for %s: <%s>\n",
			        ((i == 0) ? "start" : "end"), *argv);
			return -1;
		}

		if ((err = wlu_var_setbuf(wl, cmd->name, offset, 2*sizeof(int8))) < 0)
			return err;
	}

	return 0;
}
static int
wl_phy_spbrange(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	uint16 offset[2];
	char *endptr;
	uint16  start, end;

	if (!*++argv) {
		if ((err = wlu_iovar_get(wl, cmd->name, buf, 2 * sizeof(uint16))) < 0)
			return err;

		start = ((uint16*)buf)[0];
		end = ((uint16*)buf)[1];

		printf("start=%d, end=%d \n", start, end);
	}
	else
	{
		int i;
		for (i = 0; (i < 2) && (*argv != NULL); i++, argv++) {
			offset[i] = (uint16)strtol(*argv, &endptr, 0);
			if ((endptr == *argv) || (*endptr != '\0'))
				break;
		}

		if (((i < 2) && (*argv == NULL)) || ((i == 2) && (*argv != NULL))) {
			fprintf(stderr, "Wrong number of args\n");
			return -1;
		}

		if (i < 2) {
			fprintf(stderr, "Bad value for %s: <%s>\n",
			        ((i == 0) ? "start" : "end"), *argv);
			return -1;
		}

		if ((err = wlu_var_setbuf(wl, cmd->name, offset, 2*sizeof(uint16))) < 0)
			return err;
	}

	return 0;
}
static int
wl_sslpnphy_spbdump(void *wl, cmd_t *cmd, char **argv)
{
	wl_sslpnphy_spbdump_data_t *debug;
	void *ptr = NULL;
	int err;
	uint i;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0) {
		printf("err\n");
		return err;
	}
	debug = ptr;
	for (i = 0; i < debug->tbl_length; i++)
		printf("%4d %4d \n", debug->spbreal[i], debug->spbimg[i]);

	return 0;
}
static int
wl_sslpnphy_debug(void *wl, cmd_t *cmd, char **argv)
{
	wl_sslpnphy_debug_data_t  *debug;
	void *ptr = NULL;
	int err;
	uint i;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0) {
		printf("err\n");
		return err;
	}
	debug = ptr;
	for (i = 0; i < 64; i++)
		printf("%d \t %d \n", debug->papdcompRe[i], debug->papdcompIm[i]);

	return 0;
}
static int
wl_sslpnphy_papd_debug(void *wl, cmd_t *cmd, char **argv)
{
	wl_sslpnphy_papd_debug_data_t  *papd_debug;
	void *ptr = NULL;
	int err;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0) {
		printf("err\n");
		return err;
	}
	papd_debug = ptr;
	printf("psat_pwr =%d \n", papd_debug->psat_pwr);
	printf("psat_indx =%d \n", papd_debug->psat_indx);
	printf("min_phase =%d \n", papd_debug->min_phase);
	printf("final_idx =%d \n", papd_debug->final_idx);
	printf("start_idx =%d \n", papd_debug->start_idx);
	printf("voltage =%d \n", papd_debug->voltage);
	printf("temperature =%d \n", papd_debug->temperature);
	return 0;
}

static int
wl_sslpnphy_percal_debug(void *wl, cmd_t *cmd, char **argv)
{
	wl_sslpnphy_percal_debug_data_t  *percal_debug;
	void *ptr = NULL;
	int err;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0) {
		printf("err\n");
		return err;
	}
	percal_debug = ptr;
	printf("cur_idx =%d \n", percal_debug->cur_idx);
	printf("tx_drift =%d \n", percal_debug->tx_drift);
	printf("prev_cal_idx =%d \n", percal_debug->prev_cal_idx);
	printf("percal_ctr =%d \n", percal_debug->percal_ctr);
	printf("nxt_cal_idx =%d \n", percal_debug->nxt_cal_idx);
	printf("force_1idxcal =%d \n", percal_debug->force_1idxcal);
	printf("1dxcal_req  =%d \n", percal_debug->onedxacl_req);
	printf("lastcal_volt  =%d \n", percal_debug->last_cal_volt);
	printf("last_cal_temp  =%d \n", percal_debug->last_cal_temp);
	printf("volt winner  =%d \n", percal_debug->volt_winner);
	printf("vbat ripple  =%d \n", percal_debug->vbat_ripple);
	printf("exit route  =%d \n", percal_debug->exit_route);
	return 0;
}
#endif 

#define LPPHY_PAPD_EPS_TBL_SIZE 64
static int
wl_phy_papdepstbl(void *wl, cmd_t *cmd, char **argv)
{
	int32 eps_real, eps_imag;
	int i;
	uint32 eps_tbl[LPPHY_PAPD_EPS_TBL_SIZE];
	int err;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_iovar_get(wl, cmd->name, &eps_tbl, sizeof(eps_tbl))) < 0)
		return err;

	for (i = 0; i < LPPHY_PAPD_EPS_TBL_SIZE; i++) {
		if ((eps_real = (int32)(eps_tbl[i] >> 12)) > 0x7ff)
				eps_real -= 0x1000; /* Sign extend */
		if ((eps_imag = (int32)(eps_tbl[i] & 0xfff)) > 0x7ff)
				eps_imag -= 0x1000; /* Sign extend */
		printf("%d %d\n", eps_real, eps_imag);
	}

	return 0;
}

static int
wl_phy_txiqcc(void *wl, cmd_t *cmd, char **argv)
{
	int i;
	int err;
	int32 iqccValues[4];
	int32 value;
	char *endptr;
	int32 a, b, a1, b1;
	wlc_rev_info_t revinfo;
	uint32 phytype;

	memset(&revinfo, 0, sizeof(revinfo));
	if ((err = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo))) < 0)
		return err;

	phytype = dtoh32(revinfo.phytype);

	if (phytype != WLC_PHY_TYPE_N) {
		if (!*++argv) {
			if ((err = wlu_iovar_get(wl, cmd->name, iqccValues, 2*sizeof(int32))) < 0)
				return err;
			a = (int16)iqccValues[0];
			b = (int16)iqccValues[1];
			/* sign extend a, b from 10 bit signed value to 32 bit signed value */
			a = ((a << 22) >> 22);
			b = ((b << 22) >> 22);
			printf("%d  %d\n", a, b);
		}
		else
		{
			for (i = 0; i < 2; i++) {
				value = strtol(*argv++, &endptr, 0);
				if (value > 511 || value < -512) {
					return BCME_BADARG;
				}
				iqccValues[i] = value;
			}

		if ((err = wlu_var_setbuf(wl, cmd->name, iqccValues, 2*sizeof(int32))) < 0)
			return err;
		}
	} else {
		if (!*++argv) {
			if ((err = wlu_iovar_get(wl, cmd->name, iqccValues, 4*sizeof(int32))) < 0)
				return err;
			a = (int16)iqccValues[0];
			b = (int16)iqccValues[1];
			a1 = (int16)iqccValues[2];
			b1 = (int16)iqccValues[3];
			/* sign extend a, b from 10 bit signed value to 32 bit signed value */
			a = ((a << 22) >> 22);
			b = ((b << 22) >> 22);
			a1 = ((a1 << 22) >> 22);
			b1 = ((b1 << 22) >> 22);
			printf("%d  %d  %d  %d\n", a, b, a1, b1);
		}
		else
		{
			for (i = 0; i < 4; i++) {
				value = strtol(*argv++, &endptr, 0);
				if (value > 511 || value < -512) {
					return BCME_BADARG;
				}
				iqccValues[i] = value;
			}

			if ((err = wlu_var_setbuf(wl, cmd->name, iqccValues, 4*sizeof(int32))) < 0)
				return err;
		}
	}

	return 0;
}

static int
wl_phy_txlocc(void *wl, cmd_t *cmd, char **argv)
{
	int i;
	int err;
	int8 loccValues[12];
	int32 value;
	char *endptr;
	wlc_rev_info_t revinfo;
	uint32 phytype;

	memset(&revinfo, 0, sizeof(revinfo));
	if ((err = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo))) < 0)
		return err;

	phytype = dtoh32(revinfo.phytype);

	if (phytype != WLC_PHY_TYPE_N) {
		if (!*++argv) {
			if ((err = wlu_iovar_get(wl, cmd->name, loccValues,
				sizeof(loccValues))) < 0)
				return err;

			/* sign extend the loccValues */
			loccValues[2] = (loccValues[2] << 3) >> 3;
			loccValues[3] = (loccValues[3] << 3) >> 3;
			loccValues[4] = (loccValues[4] << 3) >> 3;
			loccValues[5] = (loccValues[5] << 3) >> 3;

			printf("%d  %d  %d  %d  %d  %d\n", loccValues[0],
				loccValues[1], loccValues[2], loccValues[3],
				loccValues[4], loccValues[5]);
		}
		else
		{
			for (i = 0; i < 6; i++) {
				value = strtol(*argv++, &endptr, 0);
				if (((i < 2) && (value > 63 || value < -64)) ||
					((i >= 2) && (value > 15 || value < -15))) {
					return BCME_BADARG;
				}
				loccValues[i] = (int8)value;
			}

			if ((err = wlu_var_setbuf(wl, cmd->name, loccValues, 6*sizeof(int8))) < 0)
				return err;
		}
	} else {
		if (!*++argv) {
			if ((err = wlu_iovar_get(wl, cmd->name, loccValues,
				sizeof(loccValues))) < 0)
				return err;

			/* sign extend the loccValues */
			loccValues[2] = (loccValues[2] << 3) >> 3;
			loccValues[3] = (loccValues[3] << 3) >> 3;
			loccValues[4] = (loccValues[4] << 3) >> 3;
			loccValues[5] = (loccValues[5] << 3) >> 3;
			loccValues[8] = (loccValues[8] << 3) >> 3;
			loccValues[9] = (loccValues[9] << 3) >> 3;
			loccValues[10] = (loccValues[10] << 3) >> 3;
			loccValues[11] = (loccValues[11] << 3) >> 3;

			printf("%d  %d  %d  %d  %d  %d  %d  %d  %d  %d  %d  %d\n", loccValues[0],
				loccValues[1], loccValues[2], loccValues[3], loccValues[4],
				loccValues[5], loccValues[6], loccValues[7], loccValues[8],
				loccValues[9], loccValues[10], loccValues[11]);
		}
		else
		{
			for (i = 0; i < 12; i++) {
				value = strtol(*argv++, &endptr, 0);
				if (((i < 2) && (value > 63 || value < -64)) ||
					((i >= 2) && (value > 15 || value < -15))) {
					return BCME_BADARG;
				}
				loccValues[i] = (int8)value;
			}

			if ((err = wlu_var_setbuf(wl, cmd->name, loccValues, 12*sizeof(int8))) < 0)
				return err;
		}
	}

	return 0;
}

static int
wl_phytable(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int32 tableInfo[4];
	char *endptr;
	void *ptr = NULL;
	int32 tableId, tableOffset, tableWidth, tableElement;
	FILE *fp = NULL;
	char line[32];

	if (*++argv != NULL)
		tableId = strtol(*argv, &endptr, 0);
	else
		return USAGE_ERROR;

	if (*++argv != NULL)
		tableOffset = strtol(*argv, &endptr, 0);
	else
		return USAGE_ERROR;

	if (*++argv != NULL)
		tableWidth = strtol(*argv, &endptr, 0);
	else
		return USAGE_ERROR;

	if ((tableId < 0) || (tableOffset < 0))
		return BCME_BADARG;

	if ((tableWidth != 8) && (tableWidth != 16) && (tableWidth != 32))
		return BCME_BADARG;

	if (!*++argv) {
		tableInfo[0] = tableId;
		tableInfo[1] = tableOffset;
		tableInfo[2] = tableWidth;

		if ((err = wlu_var_getbuf(wl, cmd->name, tableInfo, 4*sizeof(int32), &ptr)) < 0)
			return err;

		tableElement = ((int32*)ptr)[0];

		printf("0x%x(%d)\n", tableElement, tableElement);
	}
	else
	{
		fp = fopen(*argv, "r");

		while (((fp == NULL) && (*argv)) ||
			((fp != NULL) && (fgets(line, 32*sizeof(char), fp) != NULL))) {

			tableElement = (int32) strtoul((fp == NULL) ? *argv++ : line, &endptr, 0);

			tableInfo[0] = tableId;
			tableInfo[1] = tableOffset++;
			tableInfo[2] = tableWidth;
			tableInfo[3] = tableElement;

			if ((err = wlu_var_setbuf(wl, cmd->name, tableInfo, 4*sizeof(int32))) < 0) {
				if (fp != NULL)
					fclose(fp);
				return err;
			}
		}

		if (fp != NULL)
			fclose(fp);
	}

	return 0;
}
#endif /* !ATE_BUILD */

static int
wl_phy_txpwrindex(void *wl, cmd_t *cmd, char **argv)
{
	uint i;
	int ret;
	uint32 txpwridx[4] = { 0 };
	int8 idx[4] = { 0 };
	uint argc;
	char *endptr;

	/* arg count */
	for (argc = 0; argv[argc]; argc++);
	argc--;

	for (i = 0; i < 4; i++) {
		if (argc > i) {
			txpwridx[i] = strtol(argv[1 + i], &endptr, 0);
			if (*endptr != '\0') {
				printf("error\n");
				return -1;
			}
		}
	}

	if (argc == 0) {
		if ((ret = wlu_iovar_getint(wl, cmd->name, (int*)&txpwridx[0])) < 0) {
			return (ret);
		}
		txpwridx[0] = dtoh32(txpwridx[0]);
		idx[0] = (int8)(txpwridx[0] & 0xff);
		idx[1] = (int8)((txpwridx[0] >> 8) & 0xff);
		idx[2] = (int8)((txpwridx[0] >> 16) & 0xff);
		idx[3] = (int8)((txpwridx[0] >> 24) & 0xff);
		printf("txpwrindex for core{0...3}: %d %d %d %d\n", idx[0], idx[1],
		       idx[2], idx[3]);
	} else {

		wlc_rev_info_t revinfo;
		uint32 phytype;

		memset(&revinfo, 0, sizeof(revinfo));
		if ((ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo))) < 0)
			return ret;

		phytype = dtoh32(revinfo.phytype);

		if (phytype == WLC_PHY_TYPE_HT) {
			if (argc != 3) {
				printf("HTPHY must specify 3 core txpwrindex\n");
				return BCME_BADARG;
			}
		} else if (phytype == WLC_PHY_TYPE_N) {
			if (argc != 2) {
				printf("NPHY must specify 2 core txpwrindex\n");
				return BCME_BADARG;
			}
		}

		ret = wlu_iovar_setbuf(wl, cmd->name, txpwridx, 4*sizeof(uint32),
			buf, WLC_IOCTL_MAXLEN);
	}

	return ret;
}

static int
wl_sarlimit(void *wl, cmd_t *cmd, char **argv)
{
	uint i;
	int ret;
	sar_limit_t sar;
	uint argc;
	char *endptr;

	/* arg count */
	for (argc = 0; argv[argc]; argc++);
	argc--;

	if (argc != 0 && argc != sizeof(sar_limit_t)) {
		printf("Error: Input %d SAR values, need total %d SAR values\n",
		       argc, (int)sizeof(sar_limit_t));
		return -1;
	}

	if (argc == 0) {
		if ((ret = wlu_iovar_get(wl, cmd->name, &sar, sizeof(sar_limit_t))) < 0) {
			return (ret);
		}
		printf("\t2G:    %4d %4d %4d %4d\n",
		       sar.band2g[0], sar.band2g[1], sar.band2g[2], sar.band2g[3]);
		for (i = 0; i < WLC_SUBBAND_MAX; i++) {
			printf("\t5G[%1d]  %4d %4d %4d %4d\n", i,
			       sar.band5g[i][0], sar.band5g[i][1],
			       sar.band5g[i][2], sar.band5g[i][3]);
		}
	} else {
		uint8 *ptr = (uint8 *)&sar;
		memset(ptr, WLC_TXPWR_MAX, sizeof(sar_limit_t));
		for (i = 0; i < argc; i++) {
			ptr[i] = (uint8)(strtol(argv[1 + i], &endptr, 0));
			if (*endptr != '\0') {
				printf("error\n");
				return -1;
			}
		}
		printf("\t2G:    %4d %4d %4d %4d\n",
		       sar.band2g[0], sar.band2g[1], sar.band2g[2], sar.band2g[3]);
		for (i = 0; i < WLC_SUBBAND_MAX; i++) {
			printf("\t5G[%1d]  %4d %4d %4d %4d\n", i,
			       sar.band5g[i][0], sar.band5g[i][1],
			       sar.band5g[i][2], sar.band5g[i][3]);
		}
		ret = wlu_iovar_set(wl, cmd->name, &sar, sizeof(sar_limit_t));
	}

	return ret;
}

#ifndef ATE_BUILD
static int
wl_phy_pavars(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#else
	const pavars_t *pav = pavars;
	uint16	inpa[WL_PHY_PAVARS_LEN];
	char	*cpar = NULL, *p = NULL;
	char	*par;
	char	delimit[2] = " \0";
	int	err = 0;
	unsigned int val, val2[16];
	void	*ptr = NULL;

	if (*++argv) {	/* set */
		while (pav->phy_type != PHY_TYPE_NULL) {
			bool found = FALSE;
			int i = 0;

			inpa[i++] = pav->phy_type;
			inpa[i++] = pav->bandrange;
			inpa[i++] = pav->chain;

			par = malloc(strlen(pav->vars)+1);
			if (!par)
				return BCME_NOMEM;

			strcpy(par, pav->vars);

			cpar = strtok (par, delimit);	/* current param */

			if (pav->phy_type == PHY_TYPE_AC) {
				int pnum = 0, n;

				if (pav->bandrange == WL_CHAN_FREQ_RANGE_2G)
					pnum = 3;
				else if (pav->bandrange == WL_CHAN_FREQ_RANGE_5G_4BAND)
					pnum = 12;

				/* Find the parameter in the input argument list */
				if ((p = find_pattern2(argv, cpar, val2, pnum))) {
					found = TRUE;
					for (n = 0; n < pnum; n ++)
						inpa[i + n] = (uint16)val2[n];
				}
			} else {
				do {
					val = 0;

					/* Find the parameter in the input argument list */
					if ((p = find_pattern(argv, cpar, &val))) {
						found = TRUE;
						inpa[i] = (uint16)val;
					} else
						inpa[i] = 0;
					i++;
				} while ((cpar = strtok (NULL, delimit)));
			}
			free(par);

			if (found) {
				if ((err = wlu_var_setbuf(wl, cmd->name, inpa,
					WL_PHY_PAVARS_LEN * sizeof(uint16))) < 0) {
					printf("wl_phy_pavars: fail to set\n");
					return err;
				}
			}
			pav++;
		}
	} else {	/* get */
		while (pav->phy_type != PHY_TYPE_NULL) {
			int i = 0;
			uint16	*outpa;

			inpa[i++] = pav->phy_type;
			inpa[i++] = pav->bandrange;
			inpa[i++] = pav->chain;

			par = malloc(strlen(pav->vars)+1);
			if (!par)
				return BCME_NOMEM;

			strcpy(par, pav->vars);

			if ((err = wlu_var_getbuf_sm(wl, cmd->name, inpa,
				WL_PHY_PAVARS_LEN * sizeof(uint16), &ptr)) < 0) {
				printf("phy %x band %x chain %d err %d\n", pav->phy_type,
					pav->chain, pav->bandrange, err);
				free(par);
				break;
			}

			outpa = (uint16*)ptr;
			if (outpa[0] == PHY_TYPE_NULL) {
				pav++;
				free(par);
				continue;
			}

			cpar = strtok(par, delimit);	/* current param */

			if (pav->phy_type == PHY_TYPE_AC) {
				int pnum = 0, n;

				if (pav->bandrange == WL_CHAN_FREQ_RANGE_2G)
					pnum = 3;
				else if (pav->bandrange == WL_CHAN_FREQ_RANGE_5G_4BAND)
					pnum = 12;

				printf("%s=", cpar);
				for (n = 0; n < pnum; n ++) {
					if (n != 0)
						printf(",");
					printf("0x%x", outpa[i + n]);
				}
				printf("\n");
			} else {
				do {
					printf("%s=0x%x\n", cpar, outpa[i++]);
				} while ((cpar = strtok (NULL, delimit)));
			}
			pav++;
			free(par);
		}
	}

	return err;
#endif /* _CFE_ */
}

static int
wl_phy_povars(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#else
	const povars_t *pov = povars;
	wl_po_t	inpo;
	char	*cpar = NULL, *p = NULL;
	char	*par;	/* holds longest povars->vars */
	char	delimit[2] = " \0";
	int	err = 0;
	uint val;
	void	*ptr = NULL;

	if (*++argv) {	/* set */
		while (pov->phy_type != PHY_TYPE_NULL) {
			bool found = FALSE;
			int i = 0;

			inpo.phy_type = pov->phy_type;
			inpo.band = pov->bandrange;

			par = malloc(strlen(pov->vars)+1);
			if (!par)
				return BCME_NOMEM;

			strcpy(par, pov->vars);

			/* Take care of cck and ofdm before walking through povars->vars */
			if (pov->bandrange == WL_CHAN_FREQ_RANGE_2G) {
				p = find_pattern(argv, "cck2gpo", &val);
				if (p)	found = TRUE;
				inpo.cckpo = p ? (uint16)val : 0;

				p = find_pattern(argv, "ofdm2gpo", &val);
			} else if (pov->bandrange == WL_CHAN_FREQ_RANGE_5GL) {
				p = find_pattern(argv, "ofdm5glpo", &val);
			} else if (pov->bandrange == WL_CHAN_FREQ_RANGE_5GM) {
				p = find_pattern(argv, "ofdm5gpo", &val);
			} else if (pov->bandrange == WL_CHAN_FREQ_RANGE_5GH) {
				p = find_pattern(argv, "ofdm5ghpo", &val);
			}
			inpo.ofdmpo = p ? (uint32)val : 0;
			if (p)	found = TRUE;

			cpar = strtok (par, delimit);	/* current param */
			do {
				val = 0;

				/* Find the parameter in the input argument list */
				p = find_pattern(argv, cpar, &val);
				if (p)	found = TRUE;
				inpo.mcspo[i] = p ? (uint16)val : 0;
				i++;
			} while ((cpar = strtok (NULL, delimit)));

			if (found) {
				if ((err = wlu_var_setbuf(wl, cmd->name, &inpo,
					sizeof(wl_po_t))) < 0) {
					printf("wl_phy_povars: fail to set\n");
					free(par);
					return err;
				}
			}
			pov++;
			free(par);
		}
	} else {	/* get */
		while (pov->phy_type != PHY_TYPE_NULL) {
			int i = 0;
			wl_po_t	*outpo;

			inpo.phy_type = pov->phy_type;
			inpo.band = pov->bandrange;

			par = malloc(strlen(pov->vars)+1);
			if (!par)
				return BCME_NOMEM;

			strcpy(par, pov->vars);

			if ((err = wlu_var_getbuf(wl, cmd->name, &inpo, sizeof(povars_t),
				&ptr)) < 0) {
				printf("phy %x band %x err %d\n", pov->phy_type,
					pov->bandrange, err);
				free(par);
				break;
			}

			outpo = (wl_po_t*)ptr;
			if (outpo->phy_type == PHY_TYPE_NULL) {
				pov++;
				free(par);
				continue;
			}

			/* Take care of cck and ofdm before walking through povars->vars */
			if (outpo->band == WL_CHAN_FREQ_RANGE_2G) {
				printf("cck2gpo=0x%x\n", outpo->cckpo);
				printf("ofdm2gpo=0x%x\n", outpo->ofdmpo);
			} else if (pov->bandrange == WL_CHAN_FREQ_RANGE_5GL) {
				printf("ofdm5glpo=0x%x\n", outpo->ofdmpo);
			} else if (pov->bandrange == WL_CHAN_FREQ_RANGE_5GM) {
				printf("ofdm5gpo=0x%x\n", outpo->ofdmpo);
			} else if (pov->bandrange == WL_CHAN_FREQ_RANGE_5GH) {
				printf("ofdm5ghpo=0x%x\n", outpo->ofdmpo);
			}

			cpar = strtok(par, delimit);	/* current param */
			do {
				printf("%s=0x%x\n", cpar, outpo->mcspo[i++]);
			} while ((cpar = strtok (NULL, delimit)));

			pov++;
			free(par);
		}
	}

	return err;
#endif /* _CFE_ */
}

static int
wl_phy_fem(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#else
	srom_fem_t	fem;
	srom_fem_t	*rfem;
	void		*ptr;
	bool	found = FALSE;
	int	err = 0;
	uint	val;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {	/* write fem */

		/* fem2g */
		memset(&fem, 0, sizeof(srom_fem_t));

		if (find_pattern(argv, "tssipos2g", &val)) {
			found = TRUE;
			fem.tssipos = val;
		}

		if (find_pattern(argv, "extpagain2g", &val)) {
			found = TRUE;
			fem.extpagain = val;
		}

		if (find_pattern(argv, "pdetrange2g", &val)) {
			found = TRUE;
			fem.pdetrange = val;
		}

		if (find_pattern(argv, "triso2g", &val)) {
			found = TRUE;
			fem.triso = val;
		}

		if (find_pattern(argv, "antswctl2g", &val)) {
			found = TRUE;
			fem.antswctrllut = val;
		}

		if (found) {
			if ((err = wlu_var_setbuf(wl, "fem2g", &fem, sizeof(srom_fem_t)) < 0))
				printf("wl_phy_fem: fail to set fem2g\n");
			else
				printf("fem2g set\n");
		}

		found = FALSE;
		/* fem5g */
		memset(&fem, 0, sizeof(srom_fem_t));

		if (find_pattern(argv, "tssipos5g", &val)) {
			found = TRUE;
			fem.tssipos = val;
		}

		if (find_pattern(argv, "extpagain5g", &val)) {
			found = TRUE;
			fem.extpagain = val;
		}

		if (find_pattern(argv, "pdetrange5g", &val)) {
			found = TRUE;
			fem.pdetrange = val;
		}

		if (find_pattern(argv, "triso5g", &val)) {
			found = TRUE;
			fem.triso = val;
		}

		if (find_pattern(argv, "antswctl5g", &val)) {
			found = TRUE;
			fem.antswctrllut = val;
		}

		if (found) {
			if ((err = wlu_var_setbuf(wl, "fem5g", &fem, sizeof(srom_fem_t)) < 0))
				printf("wl_phy_fem: fail to set fem5g\n");
			else
				printf("fem5g set\n");
		}
	} else {
		if ((err = wlu_var_getbuf(wl, "fem2g", NULL, 0, (void**)&ptr) < 0)) {
			printf("wl_phy_fem: fail to get fem2g\n");
		} else {
			rfem = (srom_fem_t*)ptr; /* skip the "fem2g" */
			printf("tssipos2g=0x%x extpagain2g=0x%x pdetrange2g=0x%x"
			       " triso2g=0x%x antswctl2g=0x%x\n",
			       rfem->tssipos, rfem->extpagain, rfem->pdetrange,
			       rfem->triso, rfem->antswctrllut);
	       }

		if ((err = wlu_var_getbuf(wl, "fem5g", NULL, 0, (void**)&ptr) < 0)) {
			printf("wl_phy_fem: fail to get fem5g\n");
		} else {
			rfem = (srom_fem_t*)ptr; /* skip the "fem2g" */
			printf("tssipos5g=0x%x extpagain5g=0x%x pdetrange5g=0x%x"
			       " triso5g=0x%x antswctl5g=0x%x\n",
			       rfem->tssipos, rfem->extpagain, rfem->pdetrange,
			       rfem->triso, rfem->antswctrllut);
		}
	}

	return err;
#endif /* _CFE_ */
}

static int
wl_phy_maxpower(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#else
	int	err = 0;
	uint	val;
	uint8	maxp[8];
	void	*ptr;
	uint8	*rmaxp;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {	/* write maxpower */

		if (find_pattern(argv, "maxp2ga0", &val))
			maxp[0] = val;
		else
			printf("Missing maxp2ga0\n");

		if (find_pattern(argv, "maxp2ga1", &val))
			maxp[1] = val;
		else
			printf("Missing maxp2ga1\n");

		if (find_pattern(argv, "maxp5ga0", &val))
			maxp[2] = val;
		else
			printf("Missing maxp5ga0\n");

		if (find_pattern(argv, "maxp5ga1", &val))
			maxp[3] = val;
		else
			printf("Missing maxp5ga1\n");

		if (find_pattern(argv, "maxp5gla0", &val))
			maxp[4] = val;
		else
			printf("Missing maxp5gla0\n");

		if (find_pattern(argv, "maxp5gla1", &val))
			maxp[5] = val;
		else
			printf("Missing maxp5gla1\n");

		if (find_pattern(argv, "maxp5gha0", &val))
			maxp[6] = val;
		else
			printf("Missing maxp5gha0\n");

		if (find_pattern(argv, "maxp5gha1", &val))
			maxp[7] = val;
		else
			printf("Missing maxp5gha1\n");

		if ((err = wlu_var_setbuf(wl, "maxpower", &maxp, 8 * sizeof(uint8)) < 0)) {
			printf("wl_phy_maxpower: fail to set\n");
			err = -1;
		}
	} else {
		if ((err = wlu_var_getbuf(wl, "maxpower", NULL, 0, &ptr) < 0)) {
			printf("wl_phy_maxpower: fail to get maxpower\n");
			err = -1;
		}
		rmaxp = (uint8*)ptr;
		printf("maxp2ga0=%x\n", rmaxp[0]);
		printf("maxp2ga1=%x\n", rmaxp[1]);
		printf("maxp5ga0=%x\n", rmaxp[2]);
		printf("maxp5ga1=%x\n", rmaxp[3]);
		printf("maxp5gla0=%x\n", rmaxp[4]);
		printf("maxp5gla1=%x\n", rmaxp[5]);
		printf("maxp5gha0=%x\n", rmaxp[6]);
		printf("maxp5gha1=%x\n", rmaxp[7]);
	}

	return err;
#endif /* _CFE_ */
}

static int
wl_antgain(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#else
	int	err = 0;
	uint	val;
	uint8	ag[2];
	uint8	*rag;
	void	*ptr;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {	/* write maxpower */
		if (find_pattern(argv, "ag0", &val))
			ag[0] = val & 0xff;
		else {
			printf("Missing ag0\n");
			return 0;
		}

		if (find_pattern(argv, "ag1", &val))
			ag[1] = val & 0xff;
		else {
			printf("Missing ag1\n");
			return 0;
		}

		if ((err = wlu_var_setbuf(wl, "antgain", &ag, 2 * sizeof(uint8)) < 0)) {
			printf("wl_antgain: fail to set\n");
			err = -1;
		}
	} else {
		if ((err = wlu_var_getbuf(wl, "antgain", NULL, 0, &ptr) < 0)) {
			printf("wl_antgain: fail to get antgain\n");
			err = -1;
		}
		rag = (uint8*)ptr;
		printf("ag0=%x\n", rag[0]);
		printf("ag1=%x\n", rag[1]);
	}

	return err;
#endif /* _CFE_ */
}
#endif /* !ATE_BUILD */

static int
wl_pkteng(void *wl, cmd_t *cmd, char **argv)
{
	wl_pkteng_t pkteng;

	memset(&pkteng, 0, sizeof(pkteng));
	if (strcmp(cmd->name, "pkteng_stop") == 0) {
		if (!*++argv)
			return -1;
		if (strcmp(*argv, "tx") == 0)
			pkteng.flags = WL_PKTENG_PER_TX_STOP;
		else if (strcmp(*argv, "rx") == 0)
			pkteng.flags = WL_PKTENG_PER_RX_STOP;
		else
			return -1;
	}
	else if (strcmp(cmd->name, "pkteng_start") == 0) {
		if (!*++argv)
			return -1;
		if (!wl_ether_atoe(*argv, (struct ether_addr *)&pkteng.dest))
			return -1;
		if (!*++argv)
			return -1;
		if ((strcmp(*argv, "tx") == 0) || (strcmp(*argv, "txwithack") == 0))  {
			if (strcmp(*argv, "tx") == 0)
				pkteng.flags = WL_PKTENG_PER_TX_START;
			else
				pkteng.flags = WL_PKTENG_PER_TX_WITH_ACK_START;
			if (!*++argv)
				return -1;
			if (strcmp(*argv, "async") == 0)
				pkteng.flags &= ~WL_PKTENG_SYNCHRONOUS;
			else if (strcmp(*argv, "sync") == 0)
				pkteng.flags |= WL_PKTENG_SYNCHRONOUS;
			else
				/* neither optional parameter [async|sync] */
				--argv;
			if (!*++argv)
				return -1;
			pkteng.delay = strtoul(*argv, NULL, 0);
			if (!*++argv)
				return -1;
			pkteng.length = strtoul(*argv, NULL, 0);
			if (!*++argv)
				return -1;
			pkteng.nframes = strtoul(*argv, NULL, 0);
			if (*++argv)
				if (!wl_ether_atoe(*argv, (struct ether_addr *)&pkteng.src))
					return -1;
		}
		else if ((strcmp(*argv, "rx") == 0) || (strcmp(*argv, "rxwithack") == 0)) {
			if ((strcmp(*argv, "rx") == 0))
				pkteng.flags = WL_PKTENG_PER_RX_START;
			else
				pkteng.flags = WL_PKTENG_PER_RX_WITH_ACK_START;

			if (*++argv) {
				if (strcmp(*argv, "async") == 0)
					pkteng.flags &= ~WL_PKTENG_SYNCHRONOUS;
				else if (strcmp(*argv, "sync") == 0) {
					pkteng.flags |= WL_PKTENG_SYNCHRONOUS;
					/* sync mode requires number of frames and timeout */
					if (!*++argv)
						return -1;
					pkteng.nframes = strtoul(*argv, NULL, 0);
					if (!*++argv)
						return -1;
					pkteng.delay = strtoul(*argv, NULL, 0);
				}
			}
		}
		else
			return -1;
	}
	else {
		printf("Invalid command name %s\n", cmd->name);
		return 0;
	}

	pkteng.flags = htod32(pkteng.flags);
	pkteng.delay = htod32(pkteng.delay);
	pkteng.nframes = htod32(pkteng.nframes);
	pkteng.length = htod32(pkteng.length);

	return (wlu_var_setbuf(wl, "pkteng", &pkteng, sizeof(pkteng)));
}

#ifndef ATE_BUILD
static int
wl_rxiq(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t to;
	const char* fn_name = "wl_rxiqest";
	int err, argc, opt_err;
	uint32 rxiq;
	uint8 resolution = 0;
	uint8 lpf_hpc = 1;
	uint8 dig_lpf = 1;
	uint8 gain_correct = 0;
	uint8 extra_gain_3dBsteps = 0;

	/* arg count */
	for (argc = 0; argv[argc]; argc++);

	/* DEFAULT:
	 * gain_correct = 0 (disable gain correction),
	 * lpf_hpc = 1 (sets lpf hpc to lowest value),
	 * dig_lpf = 1; (sets to ltrn_lpf mode)
	 * resolution = 0 (coarse),
	 * samples = 1024 (2^10) and antenna = 3
	 */
	rxiq = (extra_gain_3dBsteps << 28) | (gain_correct << 24) | (dig_lpf << 22)
	  | (lpf_hpc << 20) | (resolution << 16) | (10 << 8) | 3;

	if (argc != 0) {
		miniopt_init(&to, fn_name, NULL, FALSE);
		while ((opt_err = miniopt(&to, argv)) != -1) {
			if (opt_err == 1) {
				err = -1;
				goto exit;
			}
			argv += to.consumed;

			if (to.opt == 'g') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int"
						" for gain-correction (0, 1)\n",
						fn_name, to.valstr);

					err = -1;
					goto exit;
				}
				if ((to.val < 0) || (to.val > 1)) {
					fprintf(stderr, "%s: invalid gain-correction select %d"
						" (0,1)\n", fn_name, to.val);
					err = -1;
					goto exit;
				}
				gain_correct = to.val & 0xf;
				rxiq = ((gain_correct << 24) | (rxiq & 0xf0ffffff));
			}
			if (to.opt == 'f') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int"
						" for lpf-hpc override select (0, 1)\n",
						fn_name, to.valstr);
					err = -1;
					goto exit;
				}
				if ((to.val < 0) || (to.val > 1)) {
					fprintf(stderr, "%s: invalid lpf-hpc override select %d"
						" (0,1)\n", fn_name, to.val);
					err = -1;
					goto exit;
				}
				lpf_hpc = to.val & 0xf;
				rxiq = ((lpf_hpc << 20) | (rxiq & 0xff0fffff));
			}
			if (to.opt == 'w') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int"
						" for dig-lpf override select (0, 1 or 2)\n",
						fn_name, to.valstr);
					err = -1;
					goto exit;
				}
				if ((to.val < 0) || (to.val > 2)) {
					fprintf(stderr, "%s: invalid dig-lpf override select %d"
						" (0,1,2)\n", fn_name, to.val);
					err = -1;
					goto exit;
				}
				dig_lpf = to.val & 0x3;
				rxiq = ((dig_lpf << 22) | (rxiq & 0xff3fffff));
			}
			if (to.opt == 'r') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int"
						" for resolution (0, 1)\n", fn_name, to.valstr);
					err = -1;
					goto exit;
				}
				if ((to.val < 0) || (to.val > 1)) {
					fprintf(stderr, "%s: invalid resolution select %d"
						" (0,1)\n", fn_name, to.val);
					err = -1;
					goto exit;
				}
				resolution = to.val & 0xf;
				rxiq = ((resolution << 16) | (rxiq & 0xfff0ffff));
			}
			if (to.opt == 's') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for"
						" the sample count\n", fn_name, to.valstr);
					err = -1;
					goto exit;
				}
				if (to.val < 0 || to.val > 16) {
					fprintf(stderr, "%s: sample count too large %d"
						"(10 <= x <= 16)\n", fn_name, to.val);
					err = -1;
					goto exit;
				}
				rxiq = (((to.val & 0xff) << 8) | (rxiq & 0xffff00ff));
			}
			if (to.opt == 'a') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int"
						" for antenna (0, 1, 3)\n", fn_name, to.valstr);
					err = -1;
					goto exit;
				}
				if ((to.val < 0) || (to.val > 3)) {
					fprintf(stderr, "%s: invalid antenna select %d\n",
						fn_name, to.val);
					err = -1;
					goto exit;
				}
				rxiq = ((rxiq & 0xffffff00) | (to.val & 0xff));
			}
			if (to.opt == 'e') {
				if (!to.good_int) {
					fprintf(stderr,
					        "%s: could not parse \"%s\" as an int"
					        " for extra INITgain\n", fn_name, to.valstr);
					err = -1;
					goto exit;
				}
				if ((to.val < 0) || (to.val > 24) || (to.val % 3 != 0)) {
					fprintf(stderr,
					        "%s: Valid extra INITgain = {0, 3, .., 21, 24}\n",
					        fn_name);
					err = -1;
					goto exit;
				}
				rxiq = ((((to.val/3) & 0xf) << 28) | (rxiq & 0x0fffffff));
			}
		}
	}

	if ((err = wlu_iovar_setint(wl, cmd->name, (int)rxiq)) < 0)
		return err;
	if ((err = wlu_iovar_getint(wl, cmd->name, (int*)&rxiq)) < 0)
		return err;

	if (resolution == 1) {
		/* fine resolution power reporting (0.25dB resolution) */
		uint8 core;
		int16 tmp;
		if (rxiq >> 20) {
			/* Three chains: */
			for (core = 0; core < 3; core ++) {
				tmp = (rxiq >> (10*core)) & 0x3ff;
				tmp = ((int16)(tmp << 6)) >> 6; /* sign extension */
				if (tmp < 0) {
					tmp = -1*tmp;
					printf("-%d.%ddBm ", (tmp >> 2), (tmp & 0x3)*25);
				} else {
					printf("%d.%ddBm ", (tmp >> 2), (tmp & 0x3)*25);
				}
			}
			printf("\n");
		} else if (rxiq >> 10) {
			/* 2 chains */
			for (core = 0; core < 2; core ++) {
				tmp = (rxiq >> (10*core)) & 0x3ff;
				tmp = ((int16)(tmp << 6)) >> 6; /* sign extension */
				if (tmp < 0) {
					tmp = -1*tmp;
					printf("-%d.%ddBm ", (tmp >> 2), (tmp & 0x3)*25);
				} else {
					printf("%d.%ddBm ", (tmp >> 2), (tmp & 0x3)*25);
				}
			}
			printf("\n");
		} else {
			/* 1 chain */
			tmp = rxiq & 0x3ff;
			tmp = ((int16)(tmp << 6)) >> 6; /* sign extension */
			if (tmp < 0) {
				tmp = -1*tmp;
				printf("-%d.%ddBm ", (tmp >> 2), (tmp & 0x3)*25);
			} else {
				printf("%d.%ddBm ", (tmp >> 2), (tmp & 0x3)*25);
			}
			printf("\n");
		}
	} else {
		if (rxiq >> 16)
			printf("%ddBm %ddBm %ddBm\n", (int8)(rxiq & 0xff),
			       (int8)((rxiq >> 8) & 0xff), (int8)((rxiq >> 16) & 0xff));
		else if (rxiq >> 8)
			printf("%ddBm %ddBm\n", (int8)(rxiq & 0xff), (int8)((rxiq >> 8) & 0xff));
		else
			printf("%ddBm\n", (int8)(rxiq & 0xff));
	}
exit:
	return err;
}

/* Convert user's input in hex pattern to byte-size mask */
static int
wl_pattern_atoh(char *src, char *dst)
{
	int i;
	if (strncmp(src, "0x", 2) != 0 &&
	    strncmp(src, "0X", 2) != 0) {
		printf("Data invalid format. Needs to start with 0x\n");
		return -1;
	}
	src = src + 2; /* Skip past 0x */
	if (strlen(src) % 2 != 0) {
		printf("Data invalid format. Needs to be of even length\n");
		return -1;
	}
	for (i = 0; *src != '\0'; i++) {
		char num[3];
		strncpy(num, src, 2);
		num[2] = '\0';
		dst[i] = (uint8)strtoul(num, NULL, 16);
		src += 2;
	}
	return i;
}

static int
wl_wowl_status(void *wl, cmd_t *cmd, char **argv)
{
	int flags_prev = 0;
	int err;

	UNUSED_PARAMETER(cmd);

	argv++;

	if ((err = wlu_iovar_getint(wl, "wowl_status", &flags_prev)))
		return err;

	printf("Status of last wakeup:\n");
	printf("\tflags:0x%x\n", flags_prev);

	if (flags_prev & WL_WOWL_BCN)
		printf("\t\tWake-on-Loss-of-Beacons enabled\n");

	if (flags_prev & WL_WOWL_MAGIC)
		printf("\t\tWake-on-Magic frame enabled\n");
	if (flags_prev & WL_WOWL_NET)
		printf("\t\tWake-on-Net pattern enabled\n");
	if (flags_prev & WL_WOWL_DIS)
		printf("\t\tWake-on-Deauth enabled\n");

	if (flags_prev & WL_WOWL_RETR)
		printf("\t\tRetrograde TSF enabled\n");
	if (flags_prev & WL_WOWL_TST)
		printf("\t\tTest-mode enabled\n");

	printf("\n");

	return 0;
}

static int
wl_wowl_wakeind(void *wl, cmd_t *cmd, char **argv)
{
	wl_wowl_wakeind_t wake = {0, 0};
	int err;

	UNUSED_PARAMETER(cmd);

	argv++;

	if (*argv) {
		if (strcmp(*argv, "clear"))
			return -1;
		err = wlu_iovar_set(wl, "wowl_wakeind", *argv, strlen(*argv));
		return err;
	}

	if ((err = wlu_iovar_get(wl, "wowl_wakeind", &wake, sizeof(wl_wowl_wakeind_t))) < 0)
		return err;

	if (wake.pci_wakeind)
		printf("PCI Indication set\n");
	if (wake.ucode_wakeind != 0) {
		printf("MAC Indication set\n");

		if ((wake.ucode_wakeind & WL_WOWL_MAGIC) == WL_WOWL_MAGIC)
			printf("\tMAGIC packet received\n");
		if ((wake.ucode_wakeind & WL_WOWL_NET) == WL_WOWL_NET)
			printf("\tPacket received with Netpattern\n");
		if ((wake.ucode_wakeind & WL_WOWL_DIS) == WL_WOWL_DIS)
			printf("\tDisassociation/Deauth received\n");
		if ((wake.ucode_wakeind & WL_WOWL_RETR) == WL_WOWL_RETR)
			printf("\tRetrograde TSF detected\n");
		if ((wake.ucode_wakeind & WL_WOWL_BCN) == WL_WOWL_BCN)
			printf("\tBeacons Lost\n");
		if ((wake.ucode_wakeind & WL_WOWL_TST) == WL_WOWL_TST)
			printf("\tTest Mode\n");
		if ((wake.ucode_wakeind & WL_WOWL_M1) == WL_WOWL_M1)
			printf("\tPTK Refresh received.\n");
		if ((wake.ucode_wakeind & WL_WOWL_EAPID) == WL_WOWL_EAPID)
			printf("\tEAP-Identity request received\n");
		if ((wake.ucode_wakeind & WL_WOWL_GTK_FAILURE) == WL_WOWL_GTK_FAILURE)
			printf("\tWake on GTK failure.\n");
		if ((wake.ucode_wakeind & WL_WOWL_EXTMAGPAT) == WL_WOWL_EXTMAGPAT)
			printf("\tExtended Magic Packet received.\n");

		if ((wake.ucode_wakeind & (WL_WOWL_NET | WL_WOWL_MAGIC | WL_WOWL_EXTMAGPAT))) {
			if ((wake.ucode_wakeind & WL_WOWL_BCAST) == WL_WOWL_BCAST)
				printf("\t\tBroadcast/Mcast frame received\n");
			else
				printf("\t\tUnicast frame received\n");
		}
	}

	if (!wake.pci_wakeind && wake.ucode_wakeind == 0)
		printf("No wakeup indication set\n");

	return 0;
}
/* Used by NINTENDO2 */
static int
wl_wowl_wake_reason(void *wl, cmd_t *cmd, char **argv)
{
	int err = -1;
	wl_wr_t wr;

	UNUSED_PARAMETER(cmd);

	if (!*++argv) {
		err = wlu_iovar_get(wl, "wakeup_reason", &wr, sizeof(wl_wr_t));
		if (err)
			return err;

		if (wr.reason && wr.reason < REASON_LAST) {
			printf("ID: %d\t", wr.id);

			if (wr.reason == LCD_ON)
			printf("Reason: LCD_ON\n");
			else if (wr.reason == LCD_OFF)
			printf("Reason: LCD_OFF\n");
			else if (wr.reason == DRC1_WAKE)
			printf("Reason: DRC1_WAKE\n");
			else if (wr.reason == DRC2_WAKE)
			printf("Reason: DRC2_WAKE\n");
		}
		else
			printf("Unknown wakeup Reason\n");
	}
		return err;
}

/* Send a wakeup frame to sta in WAKE mode */
static int
wl_wowl_pkt(void *wl, cmd_t *cmd, char **argv)
{
	char *arg = buf;
	const char *str;
	char *dst;
	uint tot = 0;
	uint16 type, pkt_len;
	int dst_ea = 0; /* 0 == manual, 1 == bcast, 2 == ucast */
	char *ea[ETHER_ADDR_LEN];
	if (!*++argv)
		return -1;

	UNUSED_PARAMETER(cmd);

	str = "wowl_pkt";
	strncpy(arg, str, strlen(str));
	arg[strlen(str)] = '\0';
	dst = arg + strlen(str) + 1;
	tot += strlen(str) + 1;

	pkt_len = (uint16)htod32(strtoul(*argv, NULL, 0));

	*((uint16*)dst) = pkt_len;

	dst += sizeof(pkt_len);
	tot += sizeof(pkt_len);

	if (!*++argv) {
		printf("Dest of the packet needs to be provided\n");
		return -1;
	}

	/* Dest of the frame */
	if (!strcmp(*argv, "bcast")) {
		dst_ea = 1;
		if (!wl_ether_atoe("ff:ff:ff:ff:ff:ff", (struct ether_addr *)dst))
			return -1;
	} else if (!strcmp(*argv, "ucast")) {
		dst_ea = 2;
		if (!*++argv) {
			printf("EA of ucast dest of the packet needs to be provided\n");
			return -1;
		}
		if (!wl_ether_atoe(*argv, (struct ether_addr *)dst))
			return -1;
		/* Store it */
		memcpy(ea, dst, ETHER_ADDR_LEN);
	} else if (!wl_ether_atoe(*argv, (struct ether_addr *)dst))
		return -1;

	dst += ETHER_ADDR_LEN;
	tot += ETHER_ADDR_LEN;

	if (!*++argv) {
		printf("type - magic/net needs to be provided\n");
		return -1;
	}

	if (strncmp(*argv, "magic", strlen("magic")) == 0)
		type = WL_WOWL_MAGIC;
	else if (strncmp(*argv, "net", strlen("net")) == 0)
		type = WL_WOWL_NET;
	else if (strncmp(*argv, "eapid", strlen("eapid")) == 0)
		type = WL_WOWL_EAPID;
	else
		return -1;

	*((uint16*)dst) = type;
	dst += sizeof(type);
	tot += sizeof(type);

	if (type == WL_WOWL_MAGIC) {
		if (pkt_len < MAGIC_PKT_MINLEN)
			return -1;

		if (dst_ea == 2)
			memcpy(dst, ea, ETHER_ADDR_LEN);
		else {
			if (!*++argv)
				return -1;

			if (!wl_ether_atoe(*argv, (struct ether_addr *)dst))
				return -1;
		}
		tot += ETHER_ADDR_LEN;
	} else if (type == WL_WOWL_NET) {
		wl_wowl_pattern_t *wl_pattern;
		wl_pattern = (wl_wowl_pattern_t *)dst;

		if (!*++argv) {
			printf("Starting offset not provided\n");
			return -1;
		}

		wl_pattern->offset = (uint)htod32(strtoul(*argv, NULL, 0));

		wl_pattern->masksize = 0;

		wl_pattern->patternoffset = (uint)htod32(sizeof(wl_wowl_pattern_t));

		dst += sizeof(wl_wowl_pattern_t);

		if (!*++argv) {
			printf("pattern not provided\n");
			return -1;
		}

		wl_pattern->patternsize =
		        (uint)htod32(wl_pattern_atoh((char *)(uintptr)*argv, dst));
		dst += wl_pattern->patternsize;
		tot += sizeof(wl_wowl_pattern_t) + wl_pattern->patternsize;

		wl_pattern->reasonsize = 0;
		if (*++argv) {
			wl_pattern->reasonsize =
				(uint)htod32(wl_pattern_atoh((char *)(uintptr)*argv, dst));
			tot += wl_pattern->reasonsize;
		}
	} else {	/* eapid */
		if (!*++argv) {
			printf("EAPOL identity string not provided\n");
			return -1;
		}

		*dst++ = strlen(*argv);
		strncpy(dst, *argv, strlen(*argv));
		tot += 1 + strlen(*argv);
	}
	return (wlu_set(wl, WLC_SET_VAR, arg, tot));
}

static int
wl_wowl_pattern(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	uint i, j;
	uint8 *ptr;
	wl_wowl_pattern_t *wl_pattern;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {
		char *arg = buf;
		const char *str;
		char *dst;
		uint tot = 0;

		if (strcmp(*argv, "add") != 0 && strcmp(*argv, "del") != 0 &&
		    strcmp(*argv, "clr") != 0) {
			return -1;
		}

		str = "wowl_pattern";
		strncpy(arg, str, strlen(str));
		arg[strlen(str)] = '\0';
		dst = arg + strlen(str) + 1;
		tot += strlen(str) + 1;

		str = *argv;
		strncpy(dst, str, strlen(str));
		tot += strlen(str) + 1;

		if (strcmp(str, "clr") != 0) {
			wl_pattern = (wl_wowl_pattern_t *)(dst + strlen(str) + 1);
			dst = (char*)wl_pattern + sizeof(wl_wowl_pattern_t);
			if (!*++argv) {
				printf("Starting offset not provided\n");
				return -1;
			}
			wl_pattern->offset = htod32(strtoul(*argv, NULL, 0));
			if (!*++argv) {
				printf("Mask not provided\n");
				return -1;
			}

			/* Parse the mask */
			str = *argv;
			wl_pattern->masksize = htod32(wl_pattern_atoh((char *)(uintptr)str, dst));
			if (wl_pattern->masksize == (uint)-1)
				return -1;

			dst += wl_pattern->masksize;
			wl_pattern->patternoffset = htod32((sizeof(wl_wowl_pattern_t) +
			                                    wl_pattern->masksize));

			if (!*++argv) {
				printf("Pattern value not provided\n");
				return -1;
			}

			/* Parse the value */
			str = *argv;
			wl_pattern->patternsize =
			        htod32(wl_pattern_atoh((char *)(uintptr)str, dst));
			if (wl_pattern->patternsize == (uint)-1)
				return -1;
			tot += sizeof(wl_wowl_pattern_t) + wl_pattern->patternsize +
			        wl_pattern->masksize;
		}

		return (wlu_set(wl, WLC_SET_VAR, arg, tot));
	} else {
		wl_wowl_pattern_list_t *list;
		if ((err = wlu_iovar_get(wl, "wowl_pattern", buf, WLC_IOCTL_MAXLEN)) < 0)
			return err;
		list = (wl_wowl_pattern_list_t *)buf;
		printf("#of patterns :%d\n", list->count);
		ptr = (uint8 *)list->pattern;
		for (i = 0; i < list->count; i++) {
			uint8 *pattern;

			wl_pattern = (wl_wowl_pattern_t *)ptr;
			printf("Pattern %d:\n", i+1);
			printf("ID         :0x%x\n"
				"Offset     :%d\n"
				"Masksize   :%d\n"
				"Mask       :0x",
				(uint32)wl_pattern->id, wl_pattern->offset, wl_pattern->masksize);
			pattern = ((uint8 *)wl_pattern + sizeof(wl_wowl_pattern_t));
			for (j = 0; j < wl_pattern->masksize; j++)
				printf("%02x", pattern[j]);
			printf("\n"
			       "PatternSize:%d\n"
			       "Pattern    :0x", wl_pattern->patternsize);
			/* Go to end to find pattern */
			pattern = ((uint8*)wl_pattern + wl_pattern->patternoffset);
			for (j = 0; j < wl_pattern->patternsize; j++)
				printf("%02x", pattern[j]);
			printf("\n\n");
			ptr += (wl_pattern->masksize + wl_pattern->patternsize +
			        sizeof(wl_wowl_pattern_t));
		}
	}

	return err;
}

#if defined(BCMDBG)
static int
wl_phy_debug_cmd(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int val;
	char *val_name;

	UNUSED_PARAMETER(cmd);

	/* command name */
	val_name = *argv++;
	val = (*argv == NULL) ? 0 : atoi(*argv);


	if ((err = wlu_iovar_setint(wl, val_name, (int)val)) < 0)
		printf("PHY DEBUG COMMAND error %d\n", err);

	return err;

}
#endif 

static int
wl_wowl_extended_magic(void *wl, cmd_t *cmd, char **argv)
{
	char *arg = buf;
	const char *str;
	char *dst;
	uint tot;
	int ret;

	str = "wowl_ext_magic";
	strncpy(arg, str, strlen(str));
	arg[strlen(str)] = '\0';

	if (*++argv) {
		dst = arg + strlen(str) + 1;
		tot = strlen(str) + 1;
		ret = wl_pattern_atoh(*argv, dst);
		if (ret == -1)
			return -1;
		if (ret != 6) {
			printf("Extended magic pattern must be 6-byte length\n");
			return -1;
		}
		tot += 6;

		ret = wlu_set(wl, cmd->set, arg, tot);
		return ret;
	}


	if ((ret = wlu_get(wl, cmd->get, arg, WLC_IOCTL_MAXLEN)) < 0)
		return ret;

	printf("0x");
	for (ret = 0; ret < 6; ret++)
		printf("%02x", (uint8)arg[ret]);
	printf("\n");

	return 0;

}


static int
wl_rifs(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int val, rifs;
	char *val_name;

	UNUSED_PARAMETER(cmd);

	/* command name */
	val_name = *argv++;

	if (!*argv) {
		if ((err = wlu_iovar_getint(wl, val_name, (int*)&rifs)) < 0)
			return err;

		printf("%s\n", ((rifs & 0xff) ? "On" : "Off"));
		return 0;
	}

	val = rifs = (atoi(*argv) ? 1 : 0);
	if (rifs != 0 && rifs != 1)
		return USAGE_ERROR;

	if ((err = wlu_set(wl, WLC_SET_FAKEFRAG, &val, sizeof(int))) < 0) {
		printf("Set frameburst error %d\n", err);
		return err;
	}
	if ((err = wlu_iovar_setint(wl, val_name, (int)rifs)) < 0)
		printf("Set rifs error %d\n", err);

	return err;
}

static int
wl_rifs_advert(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int rifs_advert;
	char *val_name;

	BCM_REFERENCE(cmd);

	/* command name */
	val_name = *argv++;

	if (!*argv) {
		if ((err = wlu_iovar_getint(wl, val_name, (int*)&rifs_advert)) < 0)
			return err;

		printf("%s\n", ((rifs_advert & 0xff) ? "On" : "Off"));
		return 0;
	}

	if (strcmp(*argv, "-1") && strcmp(*argv, "0"))
		return USAGE_ERROR;

	rifs_advert = atoi(*argv);

	if ((err = wlu_iovar_setint(wl, val_name, (int)rifs_advert)) < 0)
		printf("Set rifs mode advertisement error %d\n", err);

	return err;
}
#endif /* !ATE_BUILD */

/* this is the batched command packet size. now for remoteWL, we set it to 512 bytes */
#define MEMBLOCK (512 - 32) /* allow 32 bytes for overhead (header, alignment, etc) */

int wl_seq_batch_in_client(bool enable)
{
	batch_in_client = enable;
	return 0;
}

#ifndef ATE_BUILD
int
wl_seq_start(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	if (!batch_in_client) {
		return wlu_iovar_setbuf(wl, "seq_start", NULL, 0, buf, WLC_IOCTL_MAXLEN);
	}
	else {
		if (cmd_batching_mode) {
			printf("calling seq_start() when it's already in batching mode\n");
			clean_up_cmd_list();
			cmd_batching_mode = FALSE;
			return USAGE_ERROR;
		}
		else {
			cmd_batching_mode = TRUE;
			cmd_pkt_list_num = 0;

			cmd_list.head = NULL;
			cmd_list.tail = NULL;
		}
	}

	return 0;
}

int
wl_seq_stop(void *wl, cmd_t *cmd, char **argv)
{
	char *bufp;
	int ret = 0;
	int seq_list_len;
	int len;
	wl_seq_cmd_pkt_t *next_cmd;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	if (!batch_in_client) {
		return wlu_iovar_setbuf(wl, "seq_stop", NULL, 0, buf, WLC_IOCTL_MAXLEN);
	}
	else {
		if (!cmd_batching_mode) {
			printf("calling seq_stop when it's already out of batching mode\n");
			return IOCTL_ERROR;
		}
		cmd_batching_mode = FALSE;

		next_cmd = cmd_list.head;

		/* dump batched commands to the DUT */

		if (next_cmd == NULL) {
			printf("no command batched\n");
			return 0;
		}

		ret = wlu_iovar_setbuf(wl, "seq_start", NULL, 0, buf, WLC_IOCTL_MAXLEN);
		if (ret) {
			printf("failed to send seq_start\n");
			goto fail;
		}

		while (next_cmd != NULL) {
			bufp = buf;
			memset(bufp, 0, WLC_IOCTL_MAXLEN);

			strcpy(bufp, "seq_list");
			bufp += (strlen("seq_list") + 1);
			bufp = ALIGN_ADDR(bufp, WL_SEQ_CMD_ALIGN_BYTES);
			seq_list_len = bufp - buf;

			while ((seq_list_len < MEMBLOCK) && (next_cmd != NULL)) {
				len = ROUNDUP(next_cmd->cmd_header.len, WL_SEQ_CMD_ALIGN_BYTES);
				len += (seq_list_len + sizeof(wl_seq_cmd_ioctl_t));

				if (len < MEMBLOCK) {
					memcpy(bufp, &(next_cmd->cmd_header),
						sizeof(wl_seq_cmd_ioctl_t));
					bufp += sizeof(wl_seq_cmd_ioctl_t);
					memcpy(bufp, next_cmd->data, next_cmd->cmd_header.len);
					bufp += next_cmd->cmd_header.len;
					bufp = ALIGN_ADDR(bufp, WL_SEQ_CMD_ALIGN_BYTES);
					seq_list_len = len;

					next_cmd = next_cmd->next;
				}
				else
					break;
			}

			ret = wl_set(wl, WLC_SET_VAR, &buf[0], seq_list_len);

			if (ret) {
				printf("failed to send seq_list\n");
				goto fail;
			}
		}

		ret = wlu_iovar_setbuf(wl, "seq_stop", NULL, 0, buf, WLC_IOCTL_MAXLEN);
		if (ret) {
			printf("failed to send seq_stop\n");
		}

	fail:
		clean_up_cmd_list();
		return ret;
	}
}

static int
wl_obss_scan_params_range_chk(wl_obss_scan_arg_t *obss_scan_arg)
{
	if (obss_scan_arg->passive_dwell < 0)
		obss_scan_arg->passive_dwell = WLC_OBSS_SCAN_PASSIVE_DWELL_DEFAULT;
	else if (obss_scan_arg->passive_dwell < WLC_OBSS_SCAN_PASSIVE_DWELL_MIN ||
		obss_scan_arg->passive_dwell > WLC_OBSS_SCAN_PASSIVE_DWELL_MAX) {
		printf("passive dwell not in range %d\n", obss_scan_arg->passive_dwell);
		return -1;
	}

	if (obss_scan_arg->active_dwell < 0)
		obss_scan_arg->active_dwell = WLC_OBSS_SCAN_ACTIVE_DWELL_DEFAULT;
	else if (obss_scan_arg->active_dwell < WLC_OBSS_SCAN_ACTIVE_DWELL_MIN ||
		obss_scan_arg->active_dwell > WLC_OBSS_SCAN_ACTIVE_DWELL_MAX) {
		printf("active dwell not in range %d\n", obss_scan_arg->active_dwell);
		return -1;
	}

	if (obss_scan_arg->bss_widthscan_interval < 0)
		obss_scan_arg->bss_widthscan_interval =
			WLC_OBSS_SCAN_WIDTHSCAN_INTERVAL_DEFAULT;
	else if (obss_scan_arg->bss_widthscan_interval < WLC_OBSS_SCAN_WIDTHSCAN_INTERVAL_MIN ||
		obss_scan_arg->bss_widthscan_interval > WLC_OBSS_SCAN_WIDTHSCAN_INTERVAL_MAX) {
		printf("Width Trigger Scan Interval not in range %d\n",
		       obss_scan_arg->bss_widthscan_interval);
		return -1;
	}

	if (obss_scan_arg->chanwidth_transition_delay < 0)
		obss_scan_arg->chanwidth_transition_delay =
			WLC_OBSS_SCAN_CHANWIDTH_TRANSITION_DLY_DEFAULT;
	else if ((obss_scan_arg->chanwidth_transition_delay <
		WLC_OBSS_SCAN_CHANWIDTH_TRANSITION_DLY_MIN) ||
		(obss_scan_arg->chanwidth_transition_delay >
		WLC_OBSS_SCAN_CHANWIDTH_TRANSITION_DLY_MAX)) {
		printf("Width Channel Transition Delay Factor not in range %d\n",
		       obss_scan_arg->chanwidth_transition_delay);
		return -1;
	}

	if (obss_scan_arg->passive_total < 0)
		obss_scan_arg->passive_total = WLC_OBSS_SCAN_PASSIVE_TOTAL_PER_CHANNEL_DEFAULT;
	else if (obss_scan_arg->passive_total < WLC_OBSS_SCAN_PASSIVE_TOTAL_PER_CHANNEL_MIN ||
		obss_scan_arg->passive_total > WLC_OBSS_SCAN_PASSIVE_TOTAL_PER_CHANNEL_MAX) {
		printf("Passive Total per Channel not in range %d\n", obss_scan_arg->passive_total);
		return -1;
	}

	if (obss_scan_arg->active_total < 0)
		obss_scan_arg->active_total = WLC_OBSS_SCAN_ACTIVE_TOTAL_PER_CHANNEL_DEFAULT;
	if (obss_scan_arg->active_total < WLC_OBSS_SCAN_ACTIVE_TOTAL_PER_CHANNEL_MIN ||
		obss_scan_arg->active_total > WLC_OBSS_SCAN_ACTIVE_TOTAL_PER_CHANNEL_MAX) {
		printf("Active Total per Channel not in range %d\n", obss_scan_arg->active_total);
		return -1;
	}

	if (obss_scan_arg->activity_threshold < 0)
		obss_scan_arg->activity_threshold = WLC_OBSS_SCAN_ACTIVITY_THRESHOLD_DEFAULT;
	else if (obss_scan_arg->activity_threshold < WLC_OBSS_SCAN_ACTIVITY_THRESHOLD_MIN ||
		obss_scan_arg->activity_threshold > WLC_OBSS_SCAN_ACTIVITY_THRESHOLD_MAX) {
		printf("Activity Threshold not in range %d\n", obss_scan_arg->activity_threshold);
		return -1;
	}
	return 0;
}

/* Send a periodic keep-alive packet at the specificed interval. */
static int
wl_keep_alive(void *wl, cmd_t *cmd, char **argv)
{
	const char 				*str;
	wl_keep_alive_pkt_t	keep_alive_pkt;
	wl_keep_alive_pkt_t	*keep_alive_pktp;
	int						buf_len;
	int						str_len;
	int						i;
	int 						rc;
	void						*ptr = NULL;


	if (*++argv == NULL) {
	   /*
	   ** Get current keep-alive status.
	   */
		if ((rc = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return rc;

		keep_alive_pktp = (wl_keep_alive_pkt_t *) ptr;

		printf("Period (msec) :%d\n"
		       "Length        :%d\n"
		       "Packet        :0x",
		       dtoh32(keep_alive_pktp->period_msec),
		       dtoh16(keep_alive_pktp->len_bytes));

		for (i = 0; i < keep_alive_pktp->len_bytes; i++)
			printf("%02x", keep_alive_pktp->data[i]);

		printf("\n");
	}
	else {
		/*
		** Set keep-alive attributes.
		*/

		str = "keep_alive";
		str_len = strlen(str);
		strncpy(buf, str, str_len);
		buf[ str_len ] = '\0';

		keep_alive_pktp = (wl_keep_alive_pkt_t *) (buf + str_len + 1);
		keep_alive_pkt.period_msec = htod32(strtoul(*argv, NULL, 0));
		buf_len = str_len + 1;


		if (keep_alive_pkt.period_msec == 0) {
			keep_alive_pkt.len_bytes = 0;

			buf_len += sizeof(wl_keep_alive_pkt_t);
		}
		else {
			if (NULL != *++argv) {
				keep_alive_pkt.len_bytes =
				htod16(wl_pattern_atoh(*argv, (char *) keep_alive_pktp->data));
				buf_len += (WL_KEEP_ALIVE_FIXED_LEN + keep_alive_pkt.len_bytes);
			}
			else {
				keep_alive_pkt.len_bytes = 0;
				buf_len += WL_KEEP_ALIVE_FIXED_LEN;
			}
		}

		/* Keep-alive attributes are set in local	variable (keep_alive_pkt), and
		 * then memcpy'ed into buffer (keep_alive_pktp) since there is no
		 * guarantee that the buffer is properly aligned.
		 */
		memcpy((char *)keep_alive_pktp, &keep_alive_pkt, WL_KEEP_ALIVE_FIXED_LEN);


		rc = wlu_set(wl,
		            WLC_SET_VAR,
		            buf,
		            buf_len);

	}

	return (rc);
}

/* mkeep-alive : Send a periodic keep-alive packet or null-data at the specificed interval. */
/* wowl_keepalive : Send a periodic keep alive packet the specificed interval in wowl mode. */
static int
wl_mkeep_alive(void *wl, cmd_t *cmd, char **argv)
{
	const char 				*str;
	wl_mkeep_alive_pkt_t	mkeep_alive_pkt;
	wl_mkeep_alive_pkt_t	*mkeep_alive_pktp;
	int						buf_len;
	int						str_len;
	int						len_bytes;
	int						i;
	int 					rc;
	void					*ptr = NULL;

	memset(&mkeep_alive_pkt, 0, sizeof(wl_mkeep_alive_pkt_t));

	str = *argv;  /* mkeep_alive or wowl_keepalive */
	if (*++argv == NULL) {
		return -1;
	}
	else {
		/* read the packet index */
		int mkeep_alive_id = htod32(strtoul(*argv, NULL, 0));

		if (*++argv == NULL) {
			/*
			** Get current keep-alive status.
			*/
			if ((rc = wlu_var_getbuf(wl, cmd->name, &mkeep_alive_id,
				sizeof(int), &ptr)) < 0)
				return rc;

			mkeep_alive_pktp = (wl_mkeep_alive_pkt_t *) ptr;

			printf("Id            :%d\n"
				   "Period (msec) :%d\n"
				   "Length        :%d\n"
				   "Packet        :0x",
				   mkeep_alive_pktp->keep_alive_id,
				   dtoh32(mkeep_alive_pktp->period_msec),
				   dtoh16(mkeep_alive_pktp->len_bytes));

			for (i = 0; i < mkeep_alive_pktp->len_bytes; i++)
				printf("%02x", mkeep_alive_pktp->data[i]);

			printf("\n");
			return rc;
		}

		str_len = strlen(str);
		strncpy(buf, str, str_len);
		buf[ str_len ] = '\0';
		mkeep_alive_pktp = (wl_mkeep_alive_pkt_t *) (buf + str_len + 1);
		mkeep_alive_pkt.period_msec = htod32(strtoul(*argv, NULL, 0));
		buf_len = str_len + 1;
		mkeep_alive_pkt.version = htod16(WL_MKEEP_ALIVE_VERSION);
		mkeep_alive_pkt.length = htod16(WL_MKEEP_ALIVE_FIXED_LEN);
		mkeep_alive_pkt.keep_alive_id = mkeep_alive_id;

		len_bytes = 0;

		buf_len += WL_MKEEP_ALIVE_FIXED_LEN;
		if (mkeep_alive_pkt.period_msec != 0) {
			if (NULL != *++argv) {
				len_bytes = wl_pattern_atoh(*argv, (char *) mkeep_alive_pktp->data);
				buf_len += len_bytes;
			}
		}
		mkeep_alive_pkt.len_bytes = htod16(len_bytes);

		/* Keep-alive attributes are set in local	variable (mkeep_alive_pkt), and
		 * then memcpy'ed into buffer (mkeep_alive_pktp) since there is no
		 * guarantee that the buffer is properly aligned.
		 */
		memcpy((char *)mkeep_alive_pktp, &mkeep_alive_pkt, WL_MKEEP_ALIVE_FIXED_LEN);

		rc = wlu_set(wl,
		            WLC_SET_VAR,
		            buf,
		            buf_len);

	}

	return (rc);
}


/* Enable/disable installed packet filter. */
static int
wl_pkt_filter_enable(void *wl, cmd_t *cmd, char **argv)
{
	wl_pkt_filter_enable_t	enable_parm;
	int							rc;

	if (*++argv == NULL) {
		printf("No args provided\n");
		return (-1);
	}

	/* Parse packet filter id. */
	enable_parm.id = htod32(strtoul(*argv, NULL, 0));

	if (*++argv == NULL) {
		printf("Enable/disable value not provided\n");
		return (-1);
	}

	/* Parse enable/disable value. */
	enable_parm.enable = htod32(strtoul(*argv, NULL, 0));


	/* Enable/disable the specified filter. */
	rc = wlu_var_setbuf(wl,
	                   cmd->name,
	                   &enable_parm,
	                   sizeof(wl_pkt_filter_enable_t));

	return (rc);
}


/* Install a new packet filter. */
static int
wl_pkt_filter_add(void *wl, cmd_t *cmd, char **argv)
{
	const char 		*str;
	wl_pkt_filter_t		pkt_filter;
	wl_pkt_filter_t		*pkt_filterp;
	int			buf_len;
	int			str_len;
	int 			rc;
	uint32			mask_size;
	uint32			pattern_size;

	UNUSED_PARAMETER(cmd);

	if (*++argv == NULL) {
		printf("No args provided\n");
		return (-1);
	}

	str = "pkt_filter_add";
	str_len = strlen(str);
	strncpy(buf, str, str_len);
	buf[ str_len ] = '\0';
	buf_len = str_len + 1;

	pkt_filterp = (wl_pkt_filter_t *) (buf + str_len + 1);

	/* Parse packet filter id. */
	pkt_filter.id = htod32(strtoul(*argv, NULL, 0));

	if (*++argv == NULL) {
		printf("Polarity not provided\n");
		return -1;
	}

	/* Parse filter polarity. */
	pkt_filter.negate_match = htod32(strtoul(*argv, NULL, 0));

	if (*++argv == NULL) {
		printf("Filter type not provided\n");
		return -1;
	}

	/* Parse filter type. */
	pkt_filter.type = htod32(strtoul(*argv, NULL, 0));

	if (*++argv == NULL) {
		printf("Offset not provided\n");
		return -1;
	}

	/* Parse pattern filter offset. */
	pkt_filter.u.pattern.offset = htod32(strtoul(*argv, NULL, 0));

	if (*++argv == NULL) {
		printf("Bitmask not provided\n");
		return -1;
	}

	/* Parse pattern filter mask. */
	rc = wl_pattern_atoh(*argv, (char *) pkt_filterp->u.pattern.mask_and_pattern);
	if (rc <= 0) {
		printf("Invalid mask provided\n");
		return -1;
	}
	mask_size = htod32(rc);

	if (*++argv == NULL) {
		printf("Pattern not provided\n");
		return -1;
	}

	/* Parse pattern filter pattern. */
	rc = wl_pattern_atoh(*argv, (char *) &pkt_filterp->u.pattern.mask_and_pattern[mask_size]);
	if (rc <= 0) {
		printf("Invalid pattern provided\n");
		return -1;
	}
	pattern_size = htod32(rc);

	if (mask_size != pattern_size) {
		printf("Mask and pattern not the same size\n");
		return -1;
	}


	pkt_filter.u.pattern.size_bytes = mask_size;
	buf_len += WL_PKT_FILTER_FIXED_LEN;
	buf_len += (WL_PKT_FILTER_PATTERN_FIXED_LEN + 2 * mask_size);

	/* Keep-alive attributes are set in local	variable (keep_alive_pkt), and
	** then memcpy'ed into buffer (keep_alive_pktp) since there is no
	** guarantee that the buffer is properly aligned.
	*/
	memcpy((char *)pkt_filterp,
	       &pkt_filter,
	       WL_PKT_FILTER_FIXED_LEN + WL_PKT_FILTER_PATTERN_FIXED_LEN);


	rc = wlu_set(wl,
	            WLC_SET_VAR,
	            buf,
	            buf_len);


	return (rc);
}


/* List installed packet filters. */
static int
wl_pkt_filter_list(void *wl, cmd_t *cmd, char **argv)
{
	wl_pkt_filter_list_t	*list;
	wl_pkt_filter_t		*filterp;
	void						*ptr = NULL;
	unsigned int			i;
	unsigned int			j;
	int 						rc;
	unsigned int			filter_len;
	uint32					enable;


	if (*++argv == NULL) {
		printf("No args provided\n");
		return (-1);
	}

	/* Parse filter list to retrieve (enabled/disabled). */
	enable = htod32(strtoul(*argv, NULL, 0));
	/*
	** Get list of installed packet filters.
	*/
	if ((rc = wlu_var_getbuf(wl, cmd->name, &enable, sizeof(enable), &ptr)) < 0)
		return rc;

	list = (wl_pkt_filter_list_t *) ptr;

	printf("Num filters: %d\n\n", list->num);

	filterp = list->filter;
	for (i = 0; i < list->num; i++)
	{
		printf("Id          :%d\n"
				  "Negate      :%d\n"
				  "Type        :%d\n"
				  "Offset      :%d\n"
				  "Pattern len :%d\n"
				  "Mask        :0x",
				  dtoh32(filterp->id),
				  dtoh32(filterp->negate_match),
				  dtoh32(filterp->type),
				  dtoh32(filterp->u.pattern.offset),
				  dtoh32(filterp->u.pattern.size_bytes));


		for (j = 0; j < filterp->u.pattern.size_bytes; j++)
			printf("%02x", filterp->u.pattern.mask_and_pattern[j]);

		printf("\nPattern     :0x");

		for (; j < 2 * filterp->u.pattern.size_bytes; j++)
			printf("%02x", filterp->u.pattern.mask_and_pattern[j]);

		printf("\n\n");


		filter_len = WL_PKT_FILTER_FIXED_LEN;
		filter_len += WL_PKT_FILTER_PATTERN_FIXED_LEN + 2 * filterp->u.pattern.size_bytes;
		filterp = (wl_pkt_filter_t *) ((uint8 *)filterp + filter_len);
		filterp = ALIGN_ADDR(filterp, sizeof(uint32));

	}

	return (rc);
}


/* Get packet filter debug statistics. */
static int
wl_pkt_filter_stats(void *wl, cmd_t *cmd, char **argv)
{
	wl_pkt_filter_stats_t	*stats;
	uint32						id;
	int 							rc;
	void							*ptr = NULL;

	if (*++argv == NULL) {
		printf("No args provided\n");
		return (-1);
	}

	/* Parse filter id to retrieve. */
	id = htod32(strtoul(*argv, NULL, 0));


	/* Get debug stats. */
	if ((rc = wlu_var_getbuf(wl, cmd->name, &id, sizeof(id), &ptr)) < 0)
		return rc;

	stats = (wl_pkt_filter_stats_t *) ptr;

	printf("Packets matched for filter '%d': %d\n"
			 "Total packets discarded        : %d\n"
			 "Total packet forwarded         : %d\n",
			 id,
			 dtoh32(stats->num_pkts_matched),
			 dtoh32(stats->num_pkts_discarded),
			 dtoh32(stats->num_pkts_forwarded));

	return (rc);
}

#ifdef TRAFFIC_MGMT
/* Get/set traffic management configuration. */
static int
wl_trf_mgmt_config(void *wl, cmd_t *cmd, char **argv)
{
	uint		    argc = 0;
	uint32		    i;
	trf_mgmt_config_t   *ptrf_mgmt_config;
	uint8		    buf[sizeof(trf_mgmt_config_t)];
	int		    buf_len;
	char		    *endptr = NULL;
	int		    rc = -1;
	void		    *ptr = NULL;

	if (!*++argv) {
	   /*
	    * Get current traffic management configuration.
	    */
	    if ((rc = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return rc;

	    ptrf_mgmt_config = (trf_mgmt_config_t *)ptr;

	    printf("Enabled                   : %d\n",
	        dtoh32(ptrf_mgmt_config->trf_mgmt_enabled));
	    printf("Host IP Address           : %s\n",
	        wl_iptoa((void *)&ptrf_mgmt_config->host_ip_addr));
	    printf("Host IP Subnet Mask       : %s\n",
	        wl_iptoa((void *)&ptrf_mgmt_config->host_subnet_mask));
	    printf("Downlink Bandwidth        : %d\n",
	        dtoh32(ptrf_mgmt_config->downlink_bandwidth));
	    printf("Uplink Bandwidth          : %d\n",
	        dtoh32(ptrf_mgmt_config->uplink_bandwidth));
	    printf("\n");

	    printf("Minimum Tx Bandwidth[BK]  : %d\n",
	        dtoh32(ptrf_mgmt_config->min_tx_bandwidth[0]));
	    printf("Minimum Tx Bandwidth[BE]  : %d\n",
	        dtoh32(ptrf_mgmt_config->min_tx_bandwidth[1]));
	    printf("Minimum Tx Bandwidth[VI]  : %d\n",
	        dtoh32(ptrf_mgmt_config->min_tx_bandwidth[2]));
	    printf("\n");

	    printf("Minimum Rx Bandwidth[BK]  : %d\n",
	        dtoh32(ptrf_mgmt_config->min_rx_bandwidth[0]));
	    printf("Minimum Rx Bandwidth[BE]  : %d\n",
	        dtoh32(ptrf_mgmt_config->min_rx_bandwidth[1]));
	    printf("Minimum Rx Bandwidth[VI]  : %d\n",
	        dtoh32(ptrf_mgmt_config->min_rx_bandwidth[2]));
	    printf("\n");

	    printf("Flags                     : 0x%04X\n",
	        dtoh32(ptrf_mgmt_config->flags));
	}
	else {
	    /* arg count */
	    while (argv[argc])
		argc++;

	    /* required	arguments */
	    if ((argc != 1) && (argc != 5) && (argc != 6)) {
		fprintf(stderr,	"Too few/many arguments	(require 1 or 5 or 6 , got %d)\n", argc);
		return rc;
	    }

	    ptrf_mgmt_config = (trf_mgmt_config_t *)buf;
	    buf_len	     = sizeof(trf_mgmt_config_t);
	    memset((uint8 *)buf, 0, buf_len);

	    ptrf_mgmt_config->trf_mgmt_enabled = htod32((int32)strtol(*argv++, &endptr,	0));
	    if (*endptr	!= '\0')
		goto error;

	    if (argc > 1) {
		if (ptrf_mgmt_config->trf_mgmt_enabled)	{
		    if (!wl_atoip(*argv++, (void *)&ptrf_mgmt_config->host_ip_addr))
			goto error;

		    if (!wl_atoip(*argv++, (void *)&ptrf_mgmt_config->host_subnet_mask))
			goto error;

		    ptrf_mgmt_config->downlink_bandwidth =
		        htod32((int32)strtol(*argv++, &endptr, 0));
		    ptrf_mgmt_config->uplink_bandwidth =
		        htod32((int32)strtol(*argv++, &endptr, 0));

		    /*
		     * Zero-fill min bandwidth based. This will	cause the driver to use
		     * defult settings
		     */
		    for	(i = 0;	i < TRF_MGMT_MAX_PRIORITIES; i++) {
			ptrf_mgmt_config->min_tx_bandwidth[i] =	0;
			ptrf_mgmt_config->min_rx_bandwidth[i] =	0;
		    }

		    if (argc ==	6) {
			ptrf_mgmt_config->flags	= htod32((int32)strtol(*argv++,	&endptr, 0));
		    }
		} else
		    goto error;
	    }

	    rc = wlu_var_setbuf(wl, cmd->name, ptrf_mgmt_config, buf_len);
	}

error:
	return rc;
}
#endif  /* TRAFFIC_MGMT */

#ifdef TRAFFIC_MGMT
/* Sets a traffic management filter. */
static int
wl_trf_mgmt_filters_add(void *wl, cmd_t *cmd, char **argv)
{
	uint                    argc = 0;
	trf_mgmt_filter_list_t  *ptrf_mgmt_filter_list;
	trf_mgmt_filter_t       *ptrf_mgmt_filter;
	uint8                   buf[sizeof(trf_mgmt_filter_list_t)];
	int                     buf_len;
	char                    *param;
	char                    *endptr = NULL;
	int                     rc = -1;

	(void)param;
	/* arg count */
	param = *++argv;
	while (argv[argc])
	    argc++;

	/* required arguments */
	if (argc != 4) {
	    fprintf(stderr, "Too few/many arguments (require %d, got %d)\n", 4, argc);
	    return rc;
	}

	ptrf_mgmt_filter_list = (trf_mgmt_filter_list_t *)buf;
	buf_len               = sizeof(trf_mgmt_filter_list_t);
	memset((uint8 *)buf, 0, buf_len);

	ptrf_mgmt_filter_list->num_filters = 1;
	ptrf_mgmt_filter                   = &ptrf_mgmt_filter_list->filter[0];

	ptrf_mgmt_filter->dst_port = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
	    goto error;

	ptrf_mgmt_filter->src_port = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
	    goto error;

	ptrf_mgmt_filter->prot = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
	    goto error;

	ptrf_mgmt_filter->priority = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
	    goto error;

	rc = wlu_var_setbuf(wl, cmd->name, ptrf_mgmt_filter_list, buf_len);

error:
	return rc;
}
/* Sets a traffic management filter L2/L3/L4 */
static int
wl_trf_mgmt_filters_addex(void *wl, cmd_t *cmd, char **argv)
{
	uint                    argc = 0;
	trf_mgmt_filter_list_t  *ptrf_mgmt_filter_list;
	trf_mgmt_filter_t       *ptrf_mgmt_filter;
	uint8                   buf[sizeof(trf_mgmt_filter_list_t)];
	int                     buf_len;
	char                    *param;
	char                    *endptr = NULL;
	int                     rc = -1;

	(void)param;
	(void)cmd;
	/* arg count */
	param = *++argv;
	while (argv[argc])
	    argc++;

	/* required arguments */
	if (argc < 3) {
		fprintf(stderr, "Too few arguments (require > 3  got %d)\n", argc);
		return rc;
	}

	ptrf_mgmt_filter_list = (trf_mgmt_filter_list_t *)buf;
	buf_len               = sizeof(trf_mgmt_filter_list_t);
	memset((uint8 *)buf, 0, buf_len);

	ptrf_mgmt_filter_list->num_filters = 1;
	ptrf_mgmt_filter                   = &ptrf_mgmt_filter_list->filter[0];

	ptrf_mgmt_filter->flags = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
		goto error;

	if (ptrf_mgmt_filter->flags & TRF_FILTER_MAC_ADDR) {

		if (argc != 3) {
			fprintf(stderr, "Too many arguments (require 3 got %d)\n", argc);
			return rc;
		}

		if (!wl_ether_atoe(*argv++, &ptrf_mgmt_filter->dst_ether_addr))
			goto error;

		ptrf_mgmt_filter->priority = htod16((int16)strtol(*argv++, &endptr, 0));
			if (*endptr != '\0')
				goto error;
	} else {
		/* required arguments */
		if (argc != 5) {
			fprintf(stderr, "Too few/many arguments (require 5 got %d)\n", argc);
			return rc;
		}

		ptrf_mgmt_filter->dst_port = htod16((int16)strtol(*argv++, &endptr, 0));
			if (*endptr != '\0')
				goto error;

		ptrf_mgmt_filter->src_port = htod16((int16)strtol(*argv++, &endptr, 0));
			if (*endptr != '\0')
			    goto error;

		ptrf_mgmt_filter->prot = htod16((int16)strtol(*argv++, &endptr, 0));
			if (*endptr != '\0')
			    goto error;

		ptrf_mgmt_filter->priority = htod16((int16)strtol(*argv++, &endptr, 0));
			if (*endptr != '\0')
			    goto error;
	}

	rc = wlu_var_setbuf(wl, "trf_mgmt_filters_add", ptrf_mgmt_filter_list, buf_len);

error:
	return rc;
}
#endif  /* TRAFFIC_MGMT */

#ifdef TRAFFIC_MGMT
/* Removes a traffic management filter. */
static int
wl_trf_mgmt_filters_remove(void *wl, cmd_t *cmd, char **argv)
{
	uint                    argc = 0;
	trf_mgmt_filter_list_t  *ptrf_mgmt_filter_list;
	trf_mgmt_filter_t       *ptrf_mgmt_filter;
	uint8                   buf[sizeof(trf_mgmt_filter_list_t)];
	int                     buf_len;
	char                    *endptr = NULL;
	char                    *param;
	int                     rc = -1;

	(void)param;
	/* arg count */
	param = *++argv;
	while (argv[argc])
	    argc++;

	/* required arguments */
	if (argc != 3) {
	    fprintf(stderr, "Too few/many arguments (require %d, got %d)\n", 3, argc);
	    return rc;
	}

	ptrf_mgmt_filter_list = (trf_mgmt_filter_list_t *)buf;
	buf_len               = sizeof(trf_mgmt_filter_list_t);

	memset((uint8 *)buf, 0, buf_len);

	ptrf_mgmt_filter_list->num_filters = 1;
	ptrf_mgmt_filter                   = &ptrf_mgmt_filter_list->filter[0];

	ptrf_mgmt_filter->dst_port = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
	    goto error;

	ptrf_mgmt_filter->src_port = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
	    goto error;

	ptrf_mgmt_filter->prot = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
	    goto error;

	rc = wlu_var_setbuf(wl, cmd->name, ptrf_mgmt_filter_list, buf_len);

error:
	return rc;
}

/* Removes a traffic management filter for L2/L3/L4 */
static int
wl_trf_mgmt_filters_removeex(void *wl, cmd_t *cmd, char **argv)
{
	uint                    argc = 0;
	trf_mgmt_filter_list_t  *ptrf_mgmt_filter_list;
	trf_mgmt_filter_t       *ptrf_mgmt_filter;
	uint8                   buf[sizeof(trf_mgmt_filter_list_t)];
	int                     buf_len;
	char                    *endptr = NULL;
	char                    *param;
	int                     rc = -1;

	(void)param;
	(void)cmd;
	/* arg count */
	param = *++argv;
	while (argv[argc])
	    argc++;

	/* required arguments */
	if (argc < 2) {
	    fprintf(stderr, "Too few/many arguments (require %d, got %d)\n", 2, argc);
	    return rc;
	}

	ptrf_mgmt_filter_list = (trf_mgmt_filter_list_t *)buf;
	buf_len               = sizeof(trf_mgmt_filter_list_t);

	memset((uint8 *)buf, 0, buf_len);

	ptrf_mgmt_filter_list->num_filters = 1;
	ptrf_mgmt_filter                   = &ptrf_mgmt_filter_list->filter[0];

	ptrf_mgmt_filter->flags = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
		goto error;

	if (ptrf_mgmt_filter->flags & TRF_FILTER_MAC_ADDR) {

		if (argc != 2) {
			fprintf(stderr, "Too many arguments (require 2 got %d)\n", argc);
			return rc;
		}

		if (!wl_ether_atoe(*argv++, &ptrf_mgmt_filter->dst_ether_addr))
			goto error;

	} else {
		if (argc < 4) {
			fprintf(stderr, "Too few/many arguments (require %d, got %d)\n", 4, argc);
			return rc;
		}
		ptrf_mgmt_filter->dst_port = htod16((int16)strtol(*argv++, &endptr, 0));
		if (*endptr != '\0')
		    goto error;

		ptrf_mgmt_filter->src_port = htod16((int16)strtol(*argv++, &endptr, 0));
		if (*endptr != '\0')
		    goto error;

		ptrf_mgmt_filter->prot = htod16((int16)strtol(*argv++, &endptr, 0));
		if (*endptr != '\0')
		    goto error;
	}

	rc = wlu_var_setbuf(wl, "trf_mgmt_filters_remove", ptrf_mgmt_filter_list, buf_len);

error:
	return rc;
}
#endif  /* TRAFFIC_MGMT */

#ifdef TRAFFIC_MGMT
/* lists the current traffic management filters. */
static int
wl_trf_mgmt_filters_list(void *wl, cmd_t *cmd, char **argv)
{
	trf_mgmt_filter_list_t  *ptrf_mgmt_filter_list;
	trf_mgmt_filter_t       *ptrf_mgmt_filter;
	uint                    i;
	int                     rc = -1;
	void                    *ptr = NULL;

	UNUSED_PARAMETER(argv);

	/*
	 * Get current traffic management filters.
	 */
	if ((rc = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
	    return rc;

	ptrf_mgmt_filter_list = (trf_mgmt_filter_list_t *)ptr;

	printf("Number of filters : %d\n", dtoh32(ptrf_mgmt_filter_list->num_filters));

	for (i = 0; i < ptrf_mgmt_filter_list->num_filters; i++) {
	    ptrf_mgmt_filter = &ptrf_mgmt_filter_list->filter[i];

		if (ptrf_mgmt_filter->flags & TRF_FILTER_MAC_ADDR) {
			printf("\n");
			printf("Filter #%d\n", i);
			printf("Flags    : 0x%02x\n", dtoh32(ptrf_mgmt_filter->flags));
			printf("Dst EtherAddr    : %s\n",
				wl_ether_etoa(&ptrf_mgmt_filter->dst_ether_addr));
			printf("Priority : %d\n", dtoh32(ptrf_mgmt_filter->priority));
		} else {
			printf("\n");
			printf("Filter #%d\n", i);
			printf("Dst Port : %d\n", dtoh32(ptrf_mgmt_filter->dst_port));
			printf("Src Port : %d\n", dtoh32(ptrf_mgmt_filter->src_port));
			printf("Protocol : %d\n", dtoh32(ptrf_mgmt_filter->prot));
			printf("Flags    : 0x%02x\n", dtoh32(ptrf_mgmt_filter->flags));
			printf("Priority : %d\n", dtoh32(ptrf_mgmt_filter->priority));
		}
	}

	return rc;
}
#endif  /* TRAFFIC_MGMT */

#ifdef TRAFFIC_MGMT
/* Clears the traffic management filters. */
static int
wl_trf_mgmt_filters_clear(void *wl, cmd_t *cmd, char **argv)
{
	int rc = -1;

	UNUSED_PARAMETER(argv);

	rc = wlu_var_setbuf(wl, cmd->name, NULL, 0);

	return rc;
}
#endif  /* TRAFFIC_MGMT */

#ifdef TRAFFIC_MGMT
/*
 * Get/set traffic management bandwidth configuration. We support the ability to get/set just the
 * bandwidth parameters in the global trf_mgmt_config_t structure.
 */
static int
wl_trf_mgmt_bandwidth(void *wl, cmd_t *cmd, char **argv)
{
	uint                argc = 0;
	trf_mgmt_config_t   *ptrf_mgmt_config;
	uint8               buf[sizeof(trf_mgmt_config_t)];
	int                 buf_len;
	char                *endptr = NULL;
	int                 i, total_bandwidth;
	int                 rc = -1;
	void                *ptr = NULL;

	if (!*++argv) {
	   /*
	    * Get current traffic management bandwidth settings.
	    */
	    if ((rc = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return rc;

	    ptrf_mgmt_config = (trf_mgmt_config_t *)ptr;

	    printf("Downlink Bandwidth        : %d\n",
	        dtoh32(ptrf_mgmt_config->downlink_bandwidth));
	    printf("Uplink Bandwidth          : %d\n",
	        dtoh32(ptrf_mgmt_config->uplink_bandwidth));
	    printf("\n");

	    printf("Minimum Tx Bandwidth[BK]  : %d\n",
	        dtoh32(ptrf_mgmt_config->min_tx_bandwidth[0]));
	    printf("Minimum Tx Bandwidth[BE]  : %d\n",
	        dtoh32(ptrf_mgmt_config->min_tx_bandwidth[1]));
	    printf("Minimum Tx Bandwidth[VI]  : %d\n",
	        dtoh32(ptrf_mgmt_config->min_tx_bandwidth[2]));
	    printf("\n");

	    printf("Minimum Rx Bandwidth[BK]  : %d\n",
	        dtoh32(ptrf_mgmt_config->min_tx_bandwidth[0]));
	    printf("Minimum Rx Bandwidth[BE]  : %d\n",
	        dtoh32(ptrf_mgmt_config->min_tx_bandwidth[1]));
	    printf("Minimum Rx Bandwidth[VI]  : %d\n",
	        dtoh32(ptrf_mgmt_config->min_tx_bandwidth[2]));
	}
	else {
	    /* arg count */
	    while (argv[argc])
		argc++;

	    /* required arguments */
	    if (argc < 5) {
		fprintf(stderr, "Too few/many arguments (require %d, got %d)\n", 5, argc);
		return rc;
	    }

	    if ((rc = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return rc;

	    ptrf_mgmt_config = (trf_mgmt_config_t *)buf;
	    buf_len          = sizeof(trf_mgmt_config_t);

	    memcpy(buf, ptr, buf_len);

	    ptrf_mgmt_config->downlink_bandwidth = htod32((int32)strtol(*argv++, &endptr, 0));
	    if (*endptr != '\0')
		goto error;

	    ptrf_mgmt_config->uplink_bandwidth = htod32((int32)strtol(*argv++, &endptr, 0));
	    if (*endptr != '\0')
		goto error;

	    for (i = 0, total_bandwidth = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
		ptrf_mgmt_config->min_tx_bandwidth[i] =
		    htod32((int32)strtol(*argv++, &endptr, 0));

		if (*endptr != '\0')
		    goto error;

		total_bandwidth += ptrf_mgmt_config->min_tx_bandwidth[i];
	    }

	    if (total_bandwidth != 100) {
		fprintf(stderr,
		    "Sum of gauranteed bandwidth levels must equal 100 (got %d)\n",
		    total_bandwidth);

		return rc;
	    }

	    if (argc > 5) {
		for (i = 0, total_bandwidth = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
		    ptrf_mgmt_config->min_rx_bandwidth[i] =
		        htod32((int32)strtol(*argv++, &endptr, 0));

		    if (*endptr != '\0')
			goto error;

		    total_bandwidth += ptrf_mgmt_config->min_rx_bandwidth[i];
		}

		if (total_bandwidth != 100) {
		    fprintf(stderr,
		        "Sum of gauranteed rx bandwidth levels must equal 100 (got %d)\n",
		        total_bandwidth);

		    return rc;
		}
	    } else {
		for (i = 0, total_bandwidth = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
		    ptrf_mgmt_config->min_rx_bandwidth[i] = ptrf_mgmt_config->min_tx_bandwidth[i];
		}
	    }

	    rc = wlu_var_setbuf(wl, cmd->name, ptrf_mgmt_config, buf_len);
	}

error:
	return rc;
}
#endif /* TRAFFIC_MGMT */

#ifdef TRAFFIC_MGMT
/*
 * Get/set traffic management operational flags. We use this to change flags that
 * can't be set by GUI. This allows us to configure certain options that we may want to
 * enable/disable in the shipping product.
 */
static int
wl_trf_mgmt_flags(void *wl, cmd_t *cmd, char **argv)
{
	uint                argc = 0;
	uint32              flags;
	char                *endptr = NULL;
	int                 rc = -1;
	void                *ptr = NULL;

	if (!*++argv) {
	   /*
	    * Get current traffic management bandwidth settings.
	    */
	    if ((rc = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return rc;

	    flags = *(uint32 *)ptr;

	    printf("Flags : 0x%04X\n", flags);
	}
	else {
	    /* arg count */
	    while (argv[argc])
		argc++;

	    /* required arguments */
	    if (argc != 1) {
		fprintf(stderr, "Too few/many arguments (require %d, got %d)\n", 1, argc);
		return rc;
	    }

	    flags = htod32((int32)strtol(*argv++, &endptr, 0));
	    if (*endptr != '\0')
		goto error;

	    rc = wlu_var_setbuf(wl, cmd->name, &flags, sizeof(uint32));
	}

error:
	return rc;
}
#endif /* TRAFFIC_MGMT */

#ifdef TRAFFIC_MGMT
/* Print traffic management statistics. */
static void
wl_trf_mgmt_print_stats(void *ptr, uint index)
{
	trf_mgmt_stats_array_t  *ptrf_mgmt_statistics_array;
	trf_mgmt_stats_t        *ptrf_mgmt_statistics;

	ptrf_mgmt_statistics_array = (trf_mgmt_stats_array_t *)ptr;

	ptrf_mgmt_statistics = &ptrf_mgmt_statistics_array->tx_queue_stats[index];

	printf("Statistics for Tx Queue[%d]\n", index);
	printf("\n");
	printf("Num. packets processed : %d\n",
	       dtoh32(ptrf_mgmt_statistics->num_processed_packets));
	printf("Num. bytes processed   : %d\n",
	       dtoh32(ptrf_mgmt_statistics->num_processed_bytes));
	printf("Num. packets discarded : %d\n",
	       dtoh32(ptrf_mgmt_statistics->num_discarded_packets));

	ptrf_mgmt_statistics = &ptrf_mgmt_statistics_array->rx_queue_stats[index];

	printf("\n");
	printf("Statistics for Rx Queue[%d]\n", index);
	printf("\n");
	printf("Num. packets processed : %d\n",
	       dtoh32(ptrf_mgmt_statistics->num_processed_packets));
	printf("Num. bytes processed   : %d\n",
	       dtoh32(ptrf_mgmt_statistics->num_processed_bytes));
	printf("Num. packets discarded : %d\n",
	       dtoh32(ptrf_mgmt_statistics->num_discarded_packets));

	printf("\n");
}
#endif /* TRAFFIC_MGMT */

#ifdef TRAFFIC_MGMT
/* Get traffic management statistics. */
static int
wl_trf_mgmt_stats(void *wl, cmd_t *cmd, char **argv)
{
	uint    argc = 0;
	uint    i;
	int     rc = -1;
	char    *endptr = NULL;
	void    *ptr = NULL;

	/*
	 * Get current traffic management statistics.
	 */
	if ((rc = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
	    return rc;

	if (!*++argv) {
	    /*
	     * Print all of the current traffic management statistics.
	     */
	    for (i = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
		wl_trf_mgmt_print_stats(ptr, i);
	    }
	}
	else {
	    /* arg count */
	    while (argv[argc])
		argc++;

	    /* required arguments */
	    if (argc != 1) {
		fprintf(stderr, "Too few/many arguments (require %d, got %d)\n", 1, argc);
		return rc;
	    }

	    i = htod16((int16)strtol(*argv, &endptr, 0));
	    if (i >= TRF_MGMT_MAX_PRIORITIES) {
		fprintf(stderr, "Index must be < %d)\n", TRF_MGMT_MAX_PRIORITIES);
		return rc;
	    }

	    /* Print the current traffic management statistics for the specified queue index. */
	    wl_trf_mgmt_print_stats(ptr, i);
	}


	return rc;
}
#endif /* TRAFFIC_MGMT */

#ifdef TRAFFIC_MGMT
/* Clears the traffic management statistics. */
static int
wl_trf_mgmt_stats_clear(void *wl, cmd_t *cmd, char **argv)
{
	int rc = -1;

	UNUSED_PARAMETER(argv);

	rc = wlu_var_setbuf(wl, cmd->name, NULL, 0);

	return rc;
}
#endif /* TRAFFIC_MGMT */

#ifdef TRAFFIC_MGMT
/* Print traffic management shaping info. */
static void
wl_trf_mgmt_print_global_shaping_info(void *ptr)
{
	trf_mgmt_shaping_info_array_t   *ptrf_mgmt_shaping_info_array;
	trf_mgmt_global_info_t          *ptrf_mgmt_global_info;

	ptrf_mgmt_shaping_info_array = (trf_mgmt_shaping_info_array_t *)ptr;

	ptrf_mgmt_global_info = &ptrf_mgmt_shaping_info_array->tx_global_shaping_info;

	printf("Global shaping info. for Tx Queues\n");
	printf("\n");
	printf("Maximum bytes/second                      : %d\n",
	    ptrf_mgmt_global_info->maximum_bytes_per_second);
	printf("Maximum bytes/sampling period             : %d\n",
	    ptrf_mgmt_global_info->maximum_bytes_per_sampling_period);
	printf("Total bytes consumed per second           : %d\n",
	    ptrf_mgmt_global_info->total_bytes_consumed_per_second);
	printf("Total bytes consumed per sampling period  : %d\n",
	    ptrf_mgmt_global_info->total_bytes_consumed_per_sampling_period);
	printf("Unused bytes for current sampling period  : %d\n",
	    ptrf_mgmt_global_info->total_unused_bytes_per_sampling_period);

	printf("\n");

	ptrf_mgmt_global_info = &ptrf_mgmt_shaping_info_array->rx_global_shaping_info;

	printf("Global shaping info. for Rx Queues\n");
	printf("\n");
	printf("Maximum bytes/second                      : %d\n",
	    ptrf_mgmt_global_info->maximum_bytes_per_second);
	printf("Maximum bytes/sampling period             : %d\n",
	    ptrf_mgmt_global_info->maximum_bytes_per_sampling_period);
	printf("Total bytes consumed per second           : %d\n",
	    ptrf_mgmt_global_info->total_bytes_consumed_per_second);
	printf("Total bytes consumed per sampling period  : %d\n",
	    ptrf_mgmt_global_info->total_bytes_consumed_per_sampling_period);
	printf("Unused bytes for current sampling period  : %d\n",
	    ptrf_mgmt_global_info->total_unused_bytes_per_sampling_period);

	printf("\n");
}
#endif /* TRAFFIC_MGMT */

#ifdef TRAFFIC_MGMT
static void
wl_trf_mgmt_print_shaping_info(void *ptr, uint index)
{
	trf_mgmt_shaping_info_array_t   *ptrf_mgmt_shaping_info_array;
	trf_mgmt_shaping_info_t         *ptrf_mgmt_shaping_info;

	ptrf_mgmt_shaping_info_array = (trf_mgmt_shaping_info_array_t *)ptr;

	ptrf_mgmt_shaping_info = &ptrf_mgmt_shaping_info_array->tx_queue_shaping_info[index];

	printf("Shaping info. for Tx Queue[%d]\n", index);
	printf("\n");
	printf("Gauranteed bandwidth percentage  : %d%%\n",
	    dtoh32(ptrf_mgmt_shaping_info->gauranteed_bandwidth_percentage));
	printf("Guaranteed bytes/second          : %d\n",
	    dtoh32(ptrf_mgmt_shaping_info->guaranteed_bytes_per_second));
	printf("Guaranteed bytes/sampling period : %d\n",
	    dtoh32(ptrf_mgmt_shaping_info->guaranteed_bytes_per_sampling_period));
	printf("Num. bytes produced per second   : %d\n",
	    dtoh32(ptrf_mgmt_shaping_info->num_bytes_produced_per_second));
	printf("Num. bytes consumed per second   : %d\n",
	    dtoh32(ptrf_mgmt_shaping_info->num_bytes_consumed_per_second));
	printf("Num. packets pending             : %d\n",
	    dtoh32(ptrf_mgmt_shaping_info->num_queued_packets));
	printf("Num. bytes pending               : %d\n",
	    dtoh32(ptrf_mgmt_shaping_info->num_queued_bytes));

	ptrf_mgmt_shaping_info = &ptrf_mgmt_shaping_info_array->rx_queue_shaping_info[index];

	printf("\n");
	printf("Shaping info. for Rx Queue[%d]\n", index);
	printf("\n");
	printf("Gauranteed bandwidth percentage  : %d%%\n",
	    dtoh32(ptrf_mgmt_shaping_info->gauranteed_bandwidth_percentage));
	printf("Guaranteed bytes/second          : %d\n",
	    dtoh32(ptrf_mgmt_shaping_info->guaranteed_bytes_per_second));
	printf("Guaranteed bytes/sampling period : %d\n",
	    dtoh32(ptrf_mgmt_shaping_info->guaranteed_bytes_per_sampling_period));
	printf("Num. bytes produced per second   : %d\n",
	    dtoh32(ptrf_mgmt_shaping_info->num_bytes_produced_per_second));
	printf("Num. bytes consumed per second   : %d\n",
	    dtoh32(ptrf_mgmt_shaping_info->num_bytes_consumed_per_second));
	printf("Num. packets pending             : %d\n",
	    dtoh32(ptrf_mgmt_shaping_info->num_queued_packets));
	printf("Num. bytes pending               : %d\n",
	    dtoh32(ptrf_mgmt_shaping_info->num_queued_bytes));

	printf("\n");
}
#endif /* TRAFFIC_MGMT */

#ifdef TRAFFIC_MGMT
/* Get traffic management shaping info. */
static int
wl_trf_mgmt_shaping_info(void *wl, cmd_t *cmd, char **argv)
{
	uint    argc = 0;
	uint    i;
	int     rc = -1;
	char    *endptr = NULL;
	void    *ptr = NULL;

	/*
	 * Get current traffic management shaping info.
	 */
	if ((rc = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
	    return rc;

	if (!*++argv) {
	    /*
	     * Print all of the current traffic management shaping info.
	     */
	    wl_trf_mgmt_print_global_shaping_info(ptr);

	    for (i = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
		wl_trf_mgmt_print_shaping_info(ptr, i);
	    }
	}
	else {
	    /* arg count */
	    while (argv[argc])
		argc++;

	    /* required arguments */
	    if (argc != 1) {
		fprintf(stderr, "Too few/many arguments (require %d, got %d)\n", 1, argc);
		return rc;
	    }

	    i = htod16((int16)strtol(*argv, &endptr, 0));
	    if (i >= TRF_MGMT_MAX_PRIORITIES) {
		fprintf(stderr, "Index must be < %d)\n", TRF_MGMT_MAX_PRIORITIES);
		return rc;
	    }

	    /* Print the current traffic management shaping info for the specified queue index. */
	    wl_trf_mgmt_print_global_shaping_info(ptr);
	    wl_trf_mgmt_print_shaping_info(ptr, i);
	}


	return rc;
}
#endif  /* TRAFFIC_MGMT */

static int
wl_obss_scan(void *wl, cmd_t *cmd, char **argv)
{
	int err = -1;
	wl_obss_scan_arg_t obss_scan_arg;
	char *endptr = NULL;
	uint buflen;
	uint argc = 0;

	if (!*++argv) {
		void *ptr = NULL;
		wl_obss_scan_arg_t *obss_scan_param;

		err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr);
		if (err < 0)
		        return err;

		obss_scan_param = (wl_obss_scan_arg_t *)ptr;
		printf("%d %d %d %d %d %d %d\n",
		       dtoh16(obss_scan_param->passive_dwell),
		       dtoh16(obss_scan_param->active_dwell),
		       dtoh16(obss_scan_param->bss_widthscan_interval),
		       dtoh16(obss_scan_param->passive_total),
		       dtoh16(obss_scan_param->active_total),
		       dtoh16(obss_scan_param->chanwidth_transition_delay),
		       dtoh16(obss_scan_param->activity_threshold));
		return 0;
	}

	/* arg count */
	while (argv[argc])
		argc++;

	buflen = WL_OBSS_SCAN_PARAM_LEN;
	memset((uint8 *)&obss_scan_arg, 0, buflen);

	/* required argments */
	if (argc < WL_MIN_NUM_OBSS_SCAN_ARG) {
		fprintf(stderr, "Too few/many arguments (require %d, got %d)\n",
			WL_MIN_NUM_OBSS_SCAN_ARG, argc);
		return err;
	}

	obss_scan_arg.passive_dwell = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
		goto error;

	obss_scan_arg.active_dwell = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
		goto error;

	obss_scan_arg.bss_widthscan_interval = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
		goto error;

	obss_scan_arg.passive_total = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
		goto error;

	obss_scan_arg.active_total = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
		goto error;

	obss_scan_arg.chanwidth_transition_delay = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
		goto error;

	obss_scan_arg.activity_threshold = htod16((int16)strtol(*argv++, &endptr, 0));
	if (*endptr != '\0')
		goto error;

	if (wl_obss_scan_params_range_chk(&obss_scan_arg))
		return BCME_RANGE;

	err = wlu_var_setbuf(wl, cmd->name, &obss_scan_arg, buflen);

error:
	return err;
}

#ifdef RWL_WIFI
/* Function added to support RWL_WIFI Transport
* Used to find the remote server with proper mac address given by
* the user,this cmd is specific to RWL_WIFIi protocol
*/
static int wl_wifiserver(void *wl, cmd_t *cmd, char **argv)
{
	int ret;

	if ((ret = wlu_iovar_set(wl, cmd->name, *argv, strlen(*argv))) < 0) {
		printf("Error finding the remote server  %s\n", argv[0]);
		return ret;
	}
	return ret;
}
#endif
static int
wl_obss_coex_action(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	char var[256];
	wl_action_obss_coex_req_t *req = (wl_action_obss_coex_req_t *)var;
	int val;
	int num = 0;
	uint8 options = 0;

	argv++;
	memset(&var, 0, sizeof(wl_action_obss_coex_req_t));
	while (*argv) {
		if (!strncmp(*argv, "-i", 2) && ((options & 0x1) != 0x1)) {
			argv++;
			if (!*argv)
				return -1;
			val = atoi(*argv);
			if ((val != 0) && (val != 1))
				return -1;
			req->info |= val ? WL_COEX_40MHZ_INTOLERANT : 0;
			options |= 0x1;
		}
		else if (!strncmp(*argv, "-w", 2) && ((options & 0x2) != 0x2)) {
			argv++;
			if (!*argv)
				return -1;
			val = atoi(*argv);
			if ((val != 0) && (val != 1))
				return -1;
			req->info |= val ? WL_COEX_WIDTH20 : 0;
			options |= 0x2;
		}
		else if (!strncmp(*argv, "-c", 2) && ((options & 0x4) != 0x4)) {
			argv++;
			while (*argv) {
				if (isdigit((unsigned char)(**argv))) {
					val = htod32(strtoul(*argv, NULL, 0));
					if ((val == 0) || (val > 14)) {
						printf("Invalid channel %d\n", val);
						return -1;
					}
					req->ch_list[num] = (uint8)val;
					num++;
					argv++;
					if (num > 14) {
						printf("Too many channels (max 14)\n");
						return -1;
					}
				} else
					break;
			}
			if (!num) {
				printf("With option '-c' specified, a channel list is required\n");
				return -1;
			}
			req->num = num;
			options |= 0x4;
			continue;
		}
		else
			return -1;
		argv++;
	}
	if (!options)
		return -1;
	err = wlu_var_setbuf(wl, cmd->name, &var, (sizeof(wl_action_obss_coex_req_t) +
		(req->num ? (req->num - 1) * sizeof(uint8) : 0)));
	return err;
}

static int
wl_srchmem(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	struct args {
		int reg;
		uint32 ssidlen;
		uint8 ssid[DOT11_MAX_SSID_LEN];
	} x;
	char *endptr;
	uint argc;
	char *iovar;

	UNUSED_PARAMETER(cmd);

	memset(&x, 0, sizeof(x));

	/* save command name */
	iovar = argv[0];
	argv++;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	/* required arg: reg offset */
	if (argc < 1)
		return -1;

	x.reg = strtol(argv[0], &endptr, 0);
	if (*endptr != '\0' || x.reg > 15)
		return -1;

	if (argc > 2)
		return (-1);

	if (argc == 2) {
		uint32 len;

		len = strlen(argv[1]);
		if (len > sizeof(x.ssid)) {
			printf("ssid too long\n");
			return (-1);
		}
		memcpy(x.ssid, argv[1], len);
		x.ssidlen = len;
	}

	/* issue the get or set ioctl */
	if ((argc == 1)) {
		x.reg = htod32(x.reg);

		ret = wlu_iovar_getbuf(wl, iovar, &x, sizeof(x), buf, WLC_IOCTL_SMLEN);
		if (ret < 0) {
			printf("get returned error 0x%x\n", ret);
			return (ret);
		}

		wl_hexdump((uchar *)buf, sizeof(x.ssidlen) + sizeof(x.ssid));
	} else {
		x.reg = htod32(x.reg);
		x.ssidlen = htod32(x.ssidlen);

		ret = wlu_iovar_set(wl, iovar, &x, sizeof(x));
		if (ret < 0) {
			printf("set returned error 0x%x\n", ret);
			return (ret);
		}
	}

	return (ret);
}
#ifdef DSLCPE
static int
wl_assoclist_info(void *wl, cmd_t *cmd, char **argv)
{
	int err, i;
	void *p = NULL;
	const char *iovar = "assoclist_info";
	struct ASSOC_LIST *plistbuf;
	
	if ((err = wlu_var_getbuf(wl, iovar, NULL, 0, &p)) < 0) {
		return err;
	}
	
	plistbuf=p;
	printf("Associated stations:%d\n", plistbuf->count);
	printf("Num     Mac Address        Time\n");
	for (i=0; i<plistbuf->count; i++) {
		if(*(plistbuf->client[i].mac))
			printf("%d    %s    %s\n", i+1, plistbuf->client[i].mac, plistbuf->client[i].time);
	}
	return err;	
	
}

#ifdef DSLCPE_SCBLIST
static int
wl_readscb(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	void *p = NULL;
	char cmd_arg[32] ="scblist_info";

	if ( argv[1] != NULL )
		sprintf(cmd_arg, "%s:%s", argv[1], "scblist_info");
	
	if ((err = wlu_var_getbuf(wl, cmd_arg, NULL, 0, &p)) < 0) {
		return err;
	}
	
	printf("scblist info\n%s\n", buf );
	return 0;
}
#endif /* DSLCPE_SCBLIST */
#endif /* DSLCPE */

cntry_name_t cntry_names[] = {

{"AFGHANISTAN",		"AF"},
{"ALBANIA",		"AL"},
{"ALGERIA",		"DZ"},
{"AMERICAN SAMOA",	"AS"},
{"ANDORRA",		"AD"},
{"ANGOLA",		"AO"},
{"ANGUILLA",		"AI"},
{"ANTARCTICA",		"AQ"},
{"ANTIGUA AND BARBUDA",	"AG"},
{"ARGENTINA",		"AR"},
{"ARMENIA",		"AM"},
{"ARUBA",		"AW"},
{"ASCENSION ISLAND",	"AC"},
{"AUSTRALIA",		"AU"},
{"AUSTRIA",		"AT"},
{"AZERBAIJAN",		"AZ"},
{"BAHAMAS",		"BS"},
{"BAHRAIN",		"BH"},
{"BANGLADESH",		"BD"},
{"BARBADOS",		"BB"},
{"BELARUS",		"BY"},
{"BELGIUM",		"BE"},
{"BELIZE",		"BZ"},
{"BENIN",		"BJ"},
{"BERMUDA",		"BM"},
{"BHUTAN",		"BT"},
{"BOLIVIA",		"BO"},
{"BOSNIA AND HERZEGOVINA",		"BA"},
{"BOTSWANA",		"BW"},
{"BOUVET ISLAND",	"BV"},
{"BRAZIL",		"BR"},
{"BRITISH INDIAN OCEAN TERRITORY",		"IO"},
{"BRUNEI DARUSSALAM",	"BN"},
{"BULGARIA",		"BG"},
{"BURKINA FASO",	"BF"},
{"BURUNDI",		"BI"},
{"CAMBODIA",		"KH"},
{"CAMEROON",		"CM"},
{"CANADA",		"CA"},
{"CAPE VERDE",		"CV"},
{"CAYMAN ISLANDS",	"KY"},
{"CENTRAL AFRICAN REPUBLIC",		"CF"},
{"CHAD",		"TD"},
{"CHILE",		"CL"},
{"CHINA",		"CN"},
{"CHRISTMAS ISLAND",	"CX"},
{"CLIPPERTON ISLAND",	"CP"},
{"COCOS (KEELING) ISLANDS",		"CC"},
{"COLOMBIA",		"CO"},
{"COMOROS",		"KM"},
{"CONGO",		"CG"},
{"CONGO, THE DEMOCRATIC REPUBLIC OF THE",		"CD"},
{"COOK ISLANDS",	"CK"},
{"COSTA RICA",		"CR"},
{"COTE D'IVOIRE",	"CI"},
{"CROATIA",		"HR"},
{"CUBA",		"CU"},
{"CYPRUS",		"CY"},
{"CZECH REPUBLIC",	"CZ"},
{"DENMARK",		"DK"},
{"DJIBOUTI",		"DJ"},
{"DOMINICA",		"DM"},
{"DOMINICAN REPUBLIC",	"DO"},
{"ECUADOR",		"EC"},
{"EGYPT",		"EG"},
{"EL SALVADOR",		"SV"},
{"EQUATORIAL GUINEA",	"GQ"},
{"ERITREA",		"ER"},
{"ESTONIA",		"EE"},
{"ETHIOPIA",		"ET"},
{"FALKLAND ISLANDS (MALVINAS)",		"FK"},
{"FAROE ISLANDS",	"FO"},
{"FIJI",		"FJ"},
{"FINLAND",		"FI"},
{"FRANCE",		"FR"},
{"FRENCH GUIANA",	"GF"},
{"FRENCH POLYNESIA",	"PF"},
{"FRENCH SOUTHERN TERRITORIES",		"TF"},
{"GABON",		"GA"},
{"GAMBIA",		"GM"},
{"GEORGIA",		"GE"},
{"GERMANY",		"DE"},
{"GHANA",		"GH"},
{"GIBRALTAR",		"GI"},
{"GREECE",		"GR"},
{"GREENLAND",		"GL"},
{"GRENADA",		"GD"},
{"GUADELOUPE",		"GP"},
{"GUAM",		"GU"},
{"GUATEMALA",		"GT"},
{"GUERNSEY",		"GG"},
{"GUINEA",		"GN"},
{"GUINEA-BISSAU",	"GW"},
{"GUYANA",		"GY"},
{"HAITI",		"HT"},
{"HEARD ISLAND AND MCDONALD ISLANDS",		"HM"},
{"HOLY SEE (VATICAN CITY STATE)",		"VA"},
{"HONDURAS",		"HN"},
{"HONG KONG",		"HK"},
{"HUNGARY",		"HU"},
{"ICELAND",		"IS"},
{"INDIA",		"IN"},
{"INDONESIA",		"ID"},
{"IRAN, ISLAMIC REPUBLIC OF",		"IR"},
{"IRAQ",		"IQ"},
{"IRELAND",		"IE"},
{"ISRAEL",		"IL"},
{"ITALY",		"IT"},
{"JAMAICA",		"JM"},
{"JAPAN",		"JP"},
{"JERSEY",		"JE"},
{"JORDAN",		"JO"},
{"KAZAKHSTAN",		"KZ"},
{"KENYA",		"KE"},
{"KIRIBATI",		"KI"},
{"KOREA, DEMOCRATIC PEOPLE'S REPUBLIC OF",		"KP"},
{"KOREA, REPUBLIC OF",	"KR"},
{"KUWAIT",		"KW"},
{"KYRGYZSTAN",		"KG"},
{"LAO PEOPLE'S DEMOCRATIC REPUBLIC",		"LA"},
{"LATVIA",		"LV"},
{"LEBANON",		"LB"},
{"LESOTHO",		"LS"},
{"LIBERIA",		"LR"},
{"LIBYAN ARAB JAMAHIRIYA",		"LY"},
{"LIECHTENSTEIN",	"LI"},
{"LITHUANIA",		"LT"},
{"LUXEMBOURG",		"LU"},
{"MACAO",		"MO"},
{"MACEDONIA, THE FORMER YUGOSLAV REPUBLIC OF",		"MK"},
{"MADAGASCAR",		"MG"},
{"MALAWI",		"MW"},
{"MALAYSIA",		"MY"},
{"MALDIVES",		"MV"},
{"MALI",		"ML"},
{"MALTA",		"MT"},
{"MAN, ISLE OF",	"IM"},
{"MARSHALL ISLANDS",	"MH"},
{"MARTINIQUE",		"MQ"},
{"MAURITANIA",		"MR"},
{"MAURITIUS",		"MU"},
{"MAYOTTE",		"YT"},
{"MEXICO",		"MX"},
{"MICRONESIA, FEDERATED STATES OF",		"FM"},
{"MOLDOVA, REPUBLIC OF",		"MD"},
{"MONACO",		"MC"},
{"MONGOLIA",		"MN"},
{"MONTENEGRO",		"ME"},
{"MONTSERRAT",		"MS"},
{"MOROCCO",		"MA"},
{"MOZAMBIQUE",		"MZ"},
{"MYANMAR",		"MM"},
{"NAMIBIA",		"NA"},
{"NAURU",		"NR"},
{"NEPAL",		"NP"},
{"NETHERLANDS",		"NL"},
{"NETHERLANDS ANTILLES",		"AN"},
{"NEW CALEDONIA",	"NC"},
{"NEW ZEALAND",		"NZ"},
{"NICARAGUA",		"NI"},
{"NIGER",		"NE"},
{"NIGERIA",		"NG"},
{"NIUE",		"NU"},
{"NORFOLK ISLAND",		"NF"},
{"NORTHERN MARIANA ISLANDS",		"MP"},
{"NORWAY",		"NO"},
{"OMAN",		"OM"},
{"PAKISTAN",		"PK"},
{"PALAU",		"PW"},
{"PALESTINIAN TERRITORY, OCCUPIED",		"PS"},
{"PANAMA",		"PA"},
{"PAPUA NEW GUINEA",	"PG"},
{"PARAGUAY",		"PY"},
{"PERU",		"PE"},
{"PHILIPPINES",		"PH"},
{"PITCAIRN",		"PN"},
{"POLAND",		"PL"},
{"PORTUGAL",		"PT"},
{"PUERTO RICO",		"PR"},
{"QATAR",		"QA"},
{"Q1",		"Q1"},
{"REUNION",		"RE"},
{"ROMANIA",		"RO"},
{"RUSSIAN FEDERATION",	"RU"},
{"RWANDA",		"RW"},
{"SAINT HELENA",	"SH"},
{"SAINT KITTS AND NEVIS",		"KN"},
{"SAINT LUCIA",		"LC"},
{"SAINT PIERRE AND MIQUELON",		"PM"},
{"SAINT VINCENT AND THE GRENADINES",		"VC"},
{"SAMOA",		"WS"},
{"SAN MARINO",		"SM"},
{"SAO TOME AND PRINCIPE",		"ST"},
{"SAUDI ARABIA",	"SA"},
{"SENEGAL",		"SN"},
{"SERBIA",		"RS"},
{"SEYCHELLES",		"SC"},
{"SIERRA LEONE",	"SL"},
{"SINGAPORE",		"SG"},
{"SLOVAKIA",		"SK"},
{"SLOVENIA",		"SI"},
{"SOLOMON ISLANDS",	"SB"},
{"SOMALIA",		"SO"},
{"SOUTH AFRICA",	"ZA"},
{"SOUTH GEORGIA AND THE SOUTH SANDWICH ISLANDS",		"GS"},
{"SPAIN",		"ES"},
{"SRI LANKA",		"LK"},
{"SUDAN",		"SD"},
{"SURINAME",		"SR"},
{"SVALBARD AND JAN MAYEN",		"SJ"},
{"SWAZILAND",		"SZ"},
{"SWEDEN",		"SE"},
{"SWITZERLAND",		"CH"},
{"SYRIAN ARAB REPUBLIC",		"SY"},
{"TAIWAN, PROVINCE OF CHINA",		"TW"},
{"TAJIKISTAN",		"TJ"},
{"TANZANIA, UNITED REPUBLIC OF",		"TZ"},
{"THAILAND",		"TH"},
{"TIMOR-LESTE (EAST TIMOR)",		"TL"},
{"TOGO",		"TG"},
{"TOKELAU",		"TK"},
{"TONGA",		"TO"},
{"TRINIDAD AND TOBAGO",	"TT"},
{"TRISTAN DA CUNHA",	"TA"},
{"TUNISIA",		"TN"},
{"TURKEY",		"TR"},
{"TURKMENISTAN",	"TM"},
{"TURKS AND CAICOS ISLANDS",		"TC"},
{"TUVALU",		"TV"},
{"UGANDA",		"UG"},
{"UKRAINE",		"UA"},
{"UNITED ARAB EMIRATES",		"AE"},
{"UNITED KINGDOM",	"GB"},
{"UNITED STATES",	"US"},
{"UNITED STATES MINOR OUTLYING ISLANDS",		"UM"},
{"URUGUAY",		"UY"},
{"UZBEKISTAN",		"UZ"},
{"VANUATU",		"VU"},
{"VENEZUELA",		"VE"},
{"VIET NAM",		"VN"},
{"VIRGIN ISLANDS, BRITISH",		"VG"},
{"VIRGIN ISLANDS, U.S.",		"VI"},
{"WALLIS AND FUTUNA",	"WF"},
{"WESTERN SAHARA",	"EH"},
{"YEMEN",		"YE"},
{"YUGOSLAVIA",		"YU"},
{"ZAMBIA",		"ZM"},
{"ZIMBABWE",		"ZW"},
{"RADAR CHANNELS",	"RDR"},
{"ALL CHANNELS",	"ALL"},
{NULL,			NULL}
};

static void
wl_print_mcsset(char *mcsset)
{
	int i;

	printf("MCS SET : [ ");
	for (i = 0; i < (MCSSET_LEN * 8); i++)
		if (isset(mcsset, i))
			printf("%d ", i);
	printf("]\n");
}

static void
wl_print_vhtmcsset(uint16 *mcsset)
{
	int i, j;

	for (i = 0; i < VHT_CAP_MCS_MAP_NSS_MAX; i++) {
		if (mcsset[i]) {
			if (i == 0)
				printf("VHT SET : ");
			else
				printf("        : ");
			for (j = 0; j <= 9; j++)
				if (isbitset(mcsset[i], j))
					printf("%dx%d ", j, i + 1);
			printf("\n");
		} else {
			break;
		}
	}
}

static void
wl_print_txbf_mcsset(char *mcsset, char *prefix)
{
	int i;

	printf("%s MCS : [ ", prefix);
	for (i = 0; i < (TXBF_RATE_MCS_ALL * 8); i++)
		if (isset(mcsset, i))
			printf("%d ", i);
	printf("]\n");
}

static void
wl_print_txbf_vhtmcsset(uint16 *mcsset, char *prefix)
{
	int i, j;

	for (i = 0; i < TXBF_RATE_VHT_ALL; i++) {
		if (mcsset[i]) {
			if (i == 0)
				printf("%s VHT : ", prefix);
			else
				printf("        : ");
			for (j = 0; j <= 9; j++)
				if (isbitset(mcsset[i], j))
					printf("%dx%d ", j, i + 1);
			printf("\n");
		} else {
			break;
		}
	}
}

static int
wl_txmcsset(void *wl, cmd_t *cmd, char **argv)
{
	int err;

	if ((err = wl_var_get(wl, cmd, argv)) < 0)
		return err;
	wl_print_mcsset(buf);

	return err;
}

static int
wl_rxmcsset(void *wl, cmd_t *cmd, char **argv)
{
	int err;

	if ((err = wl_var_get(wl, cmd, argv)) < 0)
		return err;

	wl_print_mcsset(buf);

	return err;
}

static int
wl_mimo_stf(void *wl, cmd_t *cmd, char **argv)
{
	char var[256];
	int32 int_val;
	bool get = TRUE;
	uint32 len = 0;
	void *ptr = NULL;
	char *endptr;
	int i = 0, j = 0;

	while (argv[i])
		i++;

	if (i > 4)
		return -1;

	/* toss the command name */
	argv++;
	j = 1;

	if (i == 1) {
		int_val = -1;
		memcpy(&var[len], (char *)&int_val, sizeof(int_val));
		len += sizeof(int_val);
	}
	else {
		if (isdigit((unsigned char)(**argv))) {
			get = FALSE;
			int_val = htod32(strtoul(*argv, &endptr, 0));
			if ((int_val != 0) && (int_val != 1)) {
				fprintf(stderr, "wl mimo_ss_stf: bad stf mode.\n");
				return -1;
			}
			memcpy(var, (char *)&int_val, sizeof(int_val));
			len += sizeof(int_val);
			argv++;
			j++;
		}

		 if (j == i) {
			int_val = -1;
			memcpy(&var[len], (char *)&int_val, sizeof(int_val));
			len += sizeof(int_val);
		}
		else if (!strncmp(*argv, "-b", 2)) {
			argv++;
			j++;
			if (j == i)
				return -1;

			if (!strcmp(*argv, "a"))
				int_val = 1;
			else if (!strcmp(*argv, "b"))
				int_val = 0;
			else {
				fprintf(stderr,
					"wl mimo_ss_stf: wrong -b option, \"-b a\" or \"-b b\"\n");
				return -1;
			}
			j++;
			if (j < i)
				return -1;
			memcpy(&var[len], (char *)&int_val, sizeof(int_val));
			len += sizeof(int_val);
		}
	}

	if (get) {
		if (wlu_var_getbuf(wl, cmd->name, var, sizeof(var), &ptr) < 0)
			return -1;

		printf("0x%x\n", dtoh32(*(int *)ptr));
	}
	else
		wlu_var_setbuf(wl, cmd->name, &var, sizeof(var));
	return 0;
}

#ifdef WLEXTLOG
static int
wl_extlog(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	char *endptr;
	int err;
	int from_last;
	int i, j;
	char *log_p = NULL;
	wlc_extlog_req_t r_args;
	wlc_extlog_results_t *results;
	void *ptr = NULL;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if (argc > 3)
		return -1;

	if (argc == 1)
		from_last = 0;
	else {
		from_last = htod32(strtoul(argv[1], &endptr, 0));
		if ((from_last != 0) && (from_last != 1))
			return -1;
	}

	r_args.from_last = from_last;
	if (argc == 3)
		r_args.num = htod32(strtoul(argv[2], &endptr, 0));
	else
		r_args.num = 32;

	if ((err = wlu_var_getbuf(wl, cmd->name, &r_args, sizeof(wlc_extlog_req_t), &ptr)) < 0)
			return err;

	results = (wlc_extlog_results_t *)buf;

	printf("get external log records: %d\n", results->num);
	if (!results->num)
		return 0;

	if (results->version != EXTLOG_CUR_VER) {
		printf("version mismatch: version = 0x%x, expected 0x%0x\n",
			results->version, EXTLOG_CUR_VER);
		return 0;
	}

	log_p = (char *)&results->logs[0];

	printf("Seq:\tTime(ms) Log\n");
	for (i = 0; i < (int)results->num; i++) {
		printf("%d:\t%d\t ", ((log_record_t*)log_p)->seq_num,
			((log_record_t*)log_p)->time);
		for (j = 0; j < FMTSTR_MAX_ID; j++)
			if (((log_record_t*)log_p)->id == extlog_fmt_str[j].id)
				break;
		if (j == FMTSTR_MAX_ID) {
			printf("fmt string not found for id %d\n", ((log_record_t*)log_p)->id);
			log_p += results->record_len;
			continue;
		}

		switch (extlog_fmt_str[j].arg_type) {
			case LOG_ARGTYPE_NULL:
				printf("%s", extlog_fmt_str[j].fmt_str);
				break;

			case LOG_ARGTYPE_STR:
				printf(extlog_fmt_str[j].fmt_str, ((log_record_t*)log_p)->str);
				break;

			case LOG_ARGTYPE_INT:
				printf(extlog_fmt_str[j].fmt_str, ((log_record_t*)log_p)->arg);
				break;

			case LOG_ARGTYPE_INT_STR:
				printf(extlog_fmt_str[j].fmt_str, ((log_record_t*)log_p)->arg,
					((log_record_t*)log_p)->str);
				break;

			case LOG_ARGTYPE_STR_INT:
				printf(extlog_fmt_str[j].fmt_str, ((log_record_t*)log_p)->str,
					((log_record_t*)log_p)->arg);
				break;
			}
		log_p += results->record_len;
	}

	return 0;

}

static int
wl_extlog_cfg(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err = 0;
	char *endptr;
	wlc_extlog_cfg_t *r_cfg;
	wlc_extlog_cfg_t w_cfg;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if (argc == 1) {
		err = wl_var_get(wl, cmd, argv);
		if (err < 0)
			return err;
		r_cfg = (wlc_extlog_cfg_t *)buf;
		printf("max_number=%d, module=%x, level=%d, flag=%d, version=0x%04x\n",
			r_cfg->max_number, r_cfg->module, r_cfg->level,
			r_cfg->flag, r_cfg->version);
	}
	else if (argc == 4) {
		w_cfg.module = htod16((uint16)(strtoul(argv[1], &endptr, 0)));
		w_cfg.level = (uint8)strtoul(argv[2], &endptr, 0);
		w_cfg.flag = (uint8)strtoul(argv[3], &endptr, 0);
		wlu_var_setbuf(wl, cmd->name, &w_cfg, sizeof(wlc_extlog_cfg_t));
	}
	else {
		fprintf(stderr, "illegal command!\n");
		return -1;
	}

	return err;
}
#endif /* WLEXTLOG */

static int
wl_assertlog(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err;
	int i;
	char *log_p = NULL;
	assertlog_results_t *results;
	void *ptr = NULL;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if (argc > 1)
		return -1;

	if ((err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return err;

	results = (assertlog_results_t *)buf;

	printf("get external assert logs: %d\n", results->num);
	if (!results->num)
		return 0;

	if (results->version != ASSERTLOG_CUR_VER) {
		printf("Version mismatch: version = 0x%x, expected 0x%x\n",
			results->version, ASSERTLOG_CUR_VER);
		return 0;
	}

	log_p = (char *)&results->logs[0];

	printf("id: \ttime(ms) \tstring\n");
	for (i = 0; i < (int)results->num; i++) {
		printf("%d: \t%d \t%s", i, ((assert_record_t *)log_p)->time,
			((assert_record_t *)log_p)->str);
		log_p += results->record_len;
	}

	return 0;
}

extern cca_congest_channel_req_t *
cca_per_chan_summary(cca_congest_channel_req_t *input,
	cca_congest_channel_req_t *avg, bool percent);

extern int
cca_analyze(cca_congest_channel_req_t *input[], int num_chans, uint flags, chanspec_t *answer);

static const char *
cca_level(int score, int med, int hi)
{
	if (score < med)
		return ("Low");
	if (score >= med && score < hi)
		return ("Medium");
	if (score >= hi)
		return ("High");
	return NULL;
}

static const char *cca_errors[] = {
	"No error",
	"Preferred band",
	"Dwell Duration too low",
	"Channel prefs",
	"Interference too high",
	"Only 1 channel inoput"
};

static int
wl_cca_get_stats(void *wl, cmd_t *cmd, char **argv)
{
	cca_congest_channel_req_t *results;
	cca_congest_channel_req_t req;
	cca_congest_t *chptr;
	cca_congest_channel_req_t *avg[MAX_CCA_CHANNELS]; /* Max num of channels */
	void *ptr = NULL;
	char *param, *val_p;
	int base, limit, i, channel, err = 0;
	int ibss_per, obss_per, inter_per, val;
	const char *ibss_lvl = NULL;
	const char *obss_lvl = NULL;
	const char *inter_lvl = NULL;
	int tmp_channel;
	chanspec_t new_chanspec, cur_chanspec;
	bool do_average = TRUE;
	bool do_individ = FALSE;
	bool do_analyze = TRUE;
	bool curband = FALSE;
	int avg_chan_idx = 0;
	uint32 flags;


	req.num_secs = 10;
	tmp_channel = 0xff;

	argv++;

	/* Parse args */
	while ((param = *argv++) != NULL) {
		if (stricmp(param, "-n") == 0) {
			do_analyze = FALSE;
			continue;
		}
		if (stricmp(param, "-i") == 0) {
			do_individ = TRUE;
			continue;
		}
		if (stricmp(param, "-curband") == 0) {
			curband = TRUE;
			continue;
		}

		if ((val_p = *argv++) == NULL) {
			printf("Need value following %s\n", param);
			return USAGE_ERROR;
		}
		if (stricmp(param, "-c") == 0) {
			tmp_channel = (int)strtoul(val_p, NULL, 0);
		}

		if (stricmp(param, "-cs") == 0) {
			if ((new_chanspec = wf_chspec_aton(val_p)))
				tmp_channel = wf_chspec_ctlchan(new_chanspec);
		}

		if (stricmp(param, "-s") == 0) {
			req.num_secs = (int)strtoul(val_p, NULL, 0);
			if (req.num_secs == 0 || req.num_secs > MAX_CCA_SECS) {
				printf("%d: Num of seconds must be <= %d\n",
					req.num_secs, MAX_CCA_SECS);
				return USAGE_ERROR;
			}
		}
	}
	if (!do_average && !do_individ) {
		printf("Must pick at least one of averages or individual secs\n");
		return USAGE_ERROR;
	}

	if (tmp_channel == 0) {
		/* do all channels */
		base = 1; limit = 255;
	} else {
		/* Use current channel as default if none specified */
		if (tmp_channel == 0xff) {
			if ((err = wlu_iovar_getint(wl, "chanspec", (int*)&val)) < 0) {
				printf("CCA: Can't get currrent chanspec\n");
				return err;
			}
			cur_chanspec = wl_chspec32_from_driver(val);
			tmp_channel = wf_chspec_ctlchan(cur_chanspec);
			printf("Using channel %d\n", tmp_channel);
		}
		base = limit = tmp_channel;
	}


	for (channel = base; channel <= limit; channel++) {

		/* Get stats for each channel */
		req.chanspec = CH20MHZ_CHSPEC(channel);
		req.chanspec = wl_chspec_to_driver(req.chanspec);
		if ((err = wlu_var_getbuf(wl, cmd->name, &req, sizeof(req), &ptr)) < 0)
			return err;

		results = (cca_congest_channel_req_t *)ptr;
		results->chanspec = wl_chspec_from_driver(results->chanspec);
		if (results->chanspec == 0 || results->num_secs == 0)
			continue;

		if (results->num_secs > MAX_CCA_SECS) {
			printf("Bogus num of seconds returned %d\n", results->num_secs);
			return -1;
		}

		/* Summarize and save summary for this channel */
		if (do_average) {
			avg[avg_chan_idx] = (cca_congest_channel_req_t *)
				malloc(sizeof(cca_congest_channel_req_t));
			cca_per_chan_summary(results, avg[avg_chan_idx], 1);
			if (avg[avg_chan_idx]->num_secs)
				avg_chan_idx++;
		}

		/* printf stats for each second of each channel */
		if (do_individ) {
			if (channel == base)
				printf("chan dur      ibss           obss"
					"           interf       time\n");
			for (i = 0; i < results->num_secs; i++) {
				chptr = &results->secs[i];
				if (chptr->duration) {
					/* Percentages */
					ibss_per = chptr->congest_ibss * 100 /chptr->duration;
					obss_per = chptr->congest_obss * 100 /chptr->duration;
					inter_per = chptr->interference * 100 /chptr->duration;
					/* Levels */
					ibss_lvl = cca_level(ibss_per, IBSS_MED, IBSS_HI);
					obss_lvl = cca_level(obss_per, OBSS_MED, OBSS_HI);
					inter_lvl = cca_level(inter_per, INTERFER_MED, INTERFER_HI);

				printf("%-3u %4d %4u %2d%% %-6s %4u %2d%% %-6s %4u %2d%% %-6s %d\n",
					CHSPEC_CHANNEL(results->chanspec),
					chptr->duration,
					chptr->congest_ibss, ibss_per, ibss_lvl,
					chptr->congest_obss, obss_per, obss_lvl,
					chptr->interference, inter_per, inter_lvl,
					chptr->timestamp);
				}
			}
		}
	}

	/* Print summary stats of each channel */
	if (do_average) {
		int j;
		printf("Summaries:\n");
		printf("chan dur      ibss           obss             interf     num seconds\n");
		for (j = 0; j < avg_chan_idx; j++) {
			/* Percentages */
			ibss_per = avg[j]->secs[0].congest_ibss;
			obss_per = avg[j]->secs[0].congest_obss;
			inter_per = avg[j]->secs[0].interference;
			/* Levels */
			ibss_lvl = cca_level(ibss_per, IBSS_MED, IBSS_HI);
			obss_lvl = cca_level(obss_per, OBSS_MED, OBSS_HI);
			inter_lvl = cca_level(inter_per, INTERFER_MED, INTERFER_HI);

			if (avg[j]->num_secs) {
				printf("%-3u %4d %4s %2d%% %-6s %4s %2d%% %-6s %4s %2d%% %-6s %d\n",
					CHSPEC_CHANNEL(avg[j]->chanspec),
					avg[j]->secs[0].duration,
					"", avg[j]->secs[0].congest_ibss, ibss_lvl,
					"", avg[j]->secs[0].congest_obss, obss_lvl,
					"", avg[j]->secs[0].interference, inter_lvl,
					avg[j]->num_secs);
			}
		}
	}

	if (!do_analyze)
		return err;

	if ((err = wlu_iovar_getint(wl, "chanspec", (int *)&val)) < 0) {
		printf("CCA: Can't get currrent chanspec\n");
		return err;
	}
	cur_chanspec = wl_chspec32_from_driver(val);
	flags = 0;
	if (curband) {
		if (CHSPEC_IS5G(cur_chanspec))
			flags |= CCA_FLAG_5G_ONLY;
		if (CHSPEC_IS2G(cur_chanspec))
			flags |= CCA_FLAG_2G_ONLY;
	}

	if ((err = cca_analyze(avg,  avg_chan_idx, flags, &new_chanspec)) != 0) {
		printf("Cannot find a good channel due to: %s\n", cca_errors[err]);
		return 0;
	}
	printf("Recommended channel: %d\n", wf_chspec_ctlchan(new_chanspec));

	return 0;
}

static int
wl_itfr_get_stats(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	interference_source_rep_t *iftr_stats = NULL;
	const char *iftr_source[] = {"none", "wireless phone", "wireless video camera",
		"microwave oven", "wireless baby monitor", "bluetooth device",
		"wireless video camera or baby monitor", "bluetooth or baby monitor",
		"video camera or phone", "unidentified"}; /* sync with interference_source_t */

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf(wl, cmd->name, NULL, 0, (void*)&iftr_stats)) < 0)
		return err;

	if (iftr_stats->flags & ITFR_NOISY_ENVIRONMENT)
		printf("Feature is stopped due to noisy environment\n");
	else
		printf("Interference %s detected. last interference at timestamp %d: "
			"source is %s on %s channel\n",
			(iftr_stats->flags & ITFR_INTERFERENCED) ? "is" : "is not",
			iftr_stats->timestamp, iftr_source[iftr_stats->source],
			(iftr_stats->flags & ITFR_HOME_CHANNEL) ? "home" : "non-home");

	return err;
}

static int
wl_chanim_acs_record(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr = NULL;
	int err = 0, i;
	wl_acs_record_t *result;

	/* need to add to this str if new acs trigger type is added */
	const char *trig_str[] = {"None", "IOCTL", "CHANIM", "TIMER", "BTA"};

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return err;

	result = (wl_acs_record_t *) ptr;

	if (!result->count) {
		printf("There is no ACS recorded\n");
		return err;
	}

	printf("current timestamp: %u (ms)\n", result->timestamp);

	printf("Timestamp(ms)  ACS Trigger  Selected Channel  Glitch Count  CCA Count\n");
	for (i = 0; i < result->count; i++) {
		uint8 idx = CHANIM_ACS_RECORD - result->count + i;
		chanim_acs_record_t * record = &result->acs_record[idx];

		record->selected_chspc = wl_chspec_from_driver(record->selected_chspc);

		printf("%10u \t%s \t%10d \t%12d \t%8d\n", record->timestamp,
		   trig_str[record->trigger], wf_chspec_ctlchan(record->selected_chspc),
		   record->glitch_cnt, record->ccastats);
	}
	return err;
}

static int
wl_chanim_stats(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	wl_chanim_stats_t *list;
	wl_chanim_stats_t param;
	chanim_stats_t *stats;
	void *ptr;
	int j;

	UNUSED_PARAMETER(argv);

	param.buflen = htod32(sizeof(wl_chanim_stats_t));
	param.count = htod32(WL_CHANIM_COUNT_ONE);

	if ((err = wlu_var_getbuf(wl, cmd->name, &param, sizeof(wl_chanim_stats_t), &ptr)) < 0) {
		printf("failed to get chanim results");
		return err;
	}

	list = (wl_chanim_stats_t*)ptr;

	list->buflen = dtoh32(list->buflen);
	list->version = dtoh32(list->version);
	list->count = dtoh32(list->count);

	printf("version: %d \n", list->version);

	if (list->buflen == 0) {
		list->version = 0;
		list->count = 0;
	} else if (list->version != WL_CHANIM_STATS_VERSION) {
		printf("Sorry, your driver has wl_chanim_stats version %d "
			"but this program supports only version %d.\n",
				list->version, WL_CHANIM_STATS_VERSION);
		list->buflen = 0;
		list->count = 0;
	}

	stats = list->stats;
	stats->glitchcnt = htod32(stats->glitchcnt);
	stats->badplcp = htod32(stats->badplcp);
	stats->chanspec = htod16(stats->chanspec);
	stats->timestamp = htod32(stats->timestamp);

	printf("chanspec tx   inbss   obss   nocat   nopkt   doze     txop     "
		   "goodtx  badtx   glitch   badplcp  knoise  timestamp\n");
	printf("0x%4x\t", stats->chanspec);
	for (j = 0; j < CCASTATS_MAX; j++)
		printf("%d\t", stats->ccastats[j]);
	printf("%d\t%d\t%d\t%d", dtoh32(stats->glitchcnt), dtoh32(stats->badplcp),
		stats->bgnoise, dtoh32(stats->timestamp));
	printf("\n");

	return (err);
}
static int
wl_txdelay_params(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	txdelay_params_t param;
	int argc;

	argv++;

	if (*argv == NULL) {
		/* get current txdelay params */
		if ((err = wlu_iovar_get(wl, cmd->name, (void *) &param,
			(sizeof(txdelay_params_t)))) < 0)
			return (err);

		printf("Txdelay params: ratio[%d] cnt[%d] period[%d] tune[%d]\n",
			param.ratio, param.cnt,	param.period, param.tune);
	}
	else {
		char *endptr;
		/* Validate num of entries */
		for (argc = 0; argv[argc]; argc++);
		if (argc != 4)
			return USAGE_ERROR;

		argc = 0;
		param.ratio = strtol(argv[argc], &endptr, 0);
		argc++;
		param.cnt = strtol(argv[argc], &endptr, 0);
		argc++;
		param.period = strtol(argv[argc], &endptr, 0);
		argc++;
		param.tune = strtol(argv[argc], &endptr, 0);

		/* Set txdelay params */
		err = wlu_iovar_set(wl, cmd->name, (void *) &param,
			(sizeof(txdelay_params_t)));
	}
	return (err);
}


static int
ARGCNT(char **argv)
{
	int i;

	for (i = 0; argv[i] != NULL; i ++)
		;
	return i;
}

#ifdef WLP2P
static int
wl_p2p_state(void *wl, cmd_t *cmd, char **argv)
{
	wl_p2p_disc_st_t st;
	int count;
	char *endptr;

	argv++;

	count = ARGCNT(argv);
	if (count < 1)
		return -1;

	st.state = (uint8) strtol(argv[0], &endptr, 0);
	if (st.state == WL_P2P_DISC_ST_LISTEN) {
		if (count != 3)
			return -1;
		if ((st.chspec = wf_chspec_aton(argv[1])) == 0) {
			fprintf(stderr, "error parsing chanspec arg \"%s\"\n", argv[1]);
			return BCME_BADARG;
		}
		st.chspec = wl_chspec_to_driver(st.chspec);
		if (st.chspec == INVCHANSPEC) {
			return USAGE_ERROR;
		}
		st.dwell = (uint16) strtol(argv[2], &endptr, 0);
	}

	return wlu_var_setbuf(wl, cmd->name, &st, sizeof(st));
}

static int
wl_p2p_scan(void *wl, cmd_t *cmd, char **argv)
{
	wl_p2p_scan_t *params = NULL;
	int params_size = 0;
	int malloc_size = 0;
	int sparams_size = 0;
	int err = 0;

	if (*(argv + 1) != NULL) {
		malloc_size = sizeof(wl_p2p_scan_t);
		switch (toupper(**(argv + 1))) {
		case 'S':
			malloc_size += WL_SCAN_PARAMS_FIXED_SIZE + WL_NUMCHANNELS * sizeof(uint16);
			break;
		case 'E':
			malloc_size += OFFSETOF(wl_escan_params_t, params) +
			        WL_SCAN_PARAMS_FIXED_SIZE + WL_NUMCHANNELS * sizeof(uint16);
			break;
		}
	}
	if (malloc_size == 0) {
		fprintf(stderr, "wrong syntax, need 'S' or 'E'\n");
		return -1;
	}

	malloc_size += WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t);
	params = (wl_p2p_scan_t *)malloc(malloc_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan params\n", malloc_size);
		return -1;
	}
	memset(params, 0, malloc_size);

	switch (toupper(**(argv + 1))) {
	case 'S': {
		wl_scan_params_t *sparams = (wl_scan_params_t *)(params+1);
		sparams_size = malloc_size - sizeof(wl_p2p_scan_t);

		params->type = 'S';

		err = wl_scan_prep(wl, cmd, argv + 1, sparams, &sparams_size);
		params_size = sizeof(wl_p2p_scan_t) + sparams_size;
		break;
	}

	case 'E': {
		wl_escan_params_t *eparams = (wl_escan_params_t *)(params+1);
		sparams_size = malloc_size - sizeof(wl_p2p_scan_t) - sizeof(wl_escan_params_t);

		params->type = 'E';

		eparams->version = htod32(ESCAN_REQ_VERSION);
		eparams->action = htod16(WL_SCAN_ACTION_START);

#if defined(linux)
		srand((unsigned)time(NULL));
		eparams->sync_id = htod16(rand() & 0xffff);
#else
		eparams->sync_id = htod16(4321);
#endif /* #if defined(linux) */

		err = wl_scan_prep(wl, cmd, argv + 1, &eparams->params, &sparams_size);
		params_size = sizeof(wl_p2p_scan_t) + sizeof(wl_escan_params_t) + sparams_size;
		break;
	}
	}

	if (!err)
		err = wlu_iovar_setbuf(wl, cmd->name, params, params_size, buf, WLC_IOCTL_MAXLEN);

	free(params);
	return err;
}

static int
wl_p2p_ifadd(void *wl, cmd_t *cmd, char **argv)
{
	wl_p2p_if_t ifreq;
	int count;

	argv++;

	count = ARGCNT(argv);
	if (count < 2)
		return -1;

	if (!wl_ether_atoe(argv[0], &ifreq.addr))
		return -1;

	if (stricmp(argv[1], "go") == 0)
		ifreq.type = WL_P2P_IF_GO;
	else if (stricmp(argv[1], "client") == 0)
		ifreq.type = WL_P2P_IF_CLIENT;
	else if (stricmp(argv[1], "dyngo") == 0)
		ifreq.type = WL_P2P_IF_DYNBCN_GO;
	else
		return -1;

	if (ifreq.type == WL_P2P_IF_GO || ifreq.type == WL_P2P_IF_DYNBCN_GO) {
		if (count > 2) {
			if ((ifreq.chspec = wf_chspec_aton(argv[2])) == 0) {
				fprintf(stderr, "error parsing chanspec arg \"%s\"\n", argv[2]);
				return BCME_BADARG;
			}
			ifreq.chspec = wl_chspec_to_driver(ifreq.chspec);
			if (ifreq.chspec == INVCHANSPEC) {
				return BCME_BADARG;
			}
		}
		else
			ifreq.chspec = 0;
	}

	return wlu_var_setbuf(wl, cmd->name, &ifreq, sizeof(ifreq));
}

static int
wl_p2p_ifdel(void *wl, cmd_t *cmd, char **argv)
{
	struct ether_addr addr;
	int count;

	argv++;

	count = ARGCNT(argv);
	if (count != 1)
		return -1;

	if (!wl_ether_atoe(argv[0], &addr))
		return -1;

	return wlu_var_setbuf(wl, cmd->name, &addr, sizeof(addr));
}

static int
wl_p2p_ifupd(void *wl, cmd_t *cmd, char **argv)
{
	wl_p2p_if_t ifreq;
	int count;
	int ret;
	int bsscfg_idx = 0;
	int consumed = 0;

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((ret = wl_cfg_option(argv, cmd->name, &bsscfg_idx, &consumed)) != 0)
		return ret;
	argv += consumed;
	if (consumed == 0)
		bsscfg_idx = -1;

	count = ARGCNT(argv);
	if (count < 2)
		return -1;

	if (!wl_ether_atoe(argv[0], &ifreq.addr))
		return -1;

	if (stricmp(argv[1], "go") == 0)
		ifreq.type = WL_P2P_IF_GO;
	else if (stricmp(argv[1], "client") == 0)
		ifreq.type = WL_P2P_IF_CLIENT;
	else
		return -1;

	ifreq.chspec = 0;

	if (bsscfg_idx == -1)
		return wlu_var_setbuf(wl, cmd->name, &ifreq, sizeof(ifreq));
	return wlu_bssiovar_setbuf(wl, cmd->name, bsscfg_idx,
	                          &ifreq, sizeof(ifreq),
	                          buf, WLC_IOCTL_MAXLEN);
}

static int
wl_p2p_if(void *wl, cmd_t *cmd, char **argv)
{
	struct ether_addr addr;
	int count;
	wl_p2p_ifq_t *ptr;
	int err;

	argv++;

	count = ARGCNT(argv);
	if (count != 1)
		return -1;

	if (!wl_ether_atoe(argv[0], &addr))
		return -1;

	err = wlu_var_getbuf(wl, cmd->name, &addr, sizeof(addr), (void*)&ptr);
	if (err >= 0)
		printf("%u %s\n", dtoh32(ptr->bsscfgidx), (ptr->ifname));

	return err;
}

static int
wl_p2p_ops(void *wl, cmd_t *cmd, char **argv)
{
	wl_p2p_ops_t ops;
	int count;
	char *endptr;

	argv++;

	count = ARGCNT(argv);
	if (count < 1) {
		wl_p2p_ops_t *ops;
		int err;

		err = wlu_var_getbuf(wl, cmd->name, NULL, 0, (void *)&ops);
		if (err != BCME_OK) {
			fprintf(stderr, "%s: error %d\n", cmd->name, err);
			return BCME_ERROR;
		}

		printf("ops: %u ctw: %u\n", ops->ops, ops->ctw);

		return BCME_OK;
	}

	ops.ops = (uint8) strtol(argv[0], &endptr, 0);
	if (ops.ops != 0) {
		if (count != 2)
			return -1;
		ops.ctw = (uint8) strtol(argv[1], &endptr, 0);
	}
	else
		ops.ctw = 0;

	return wlu_var_setbuf(wl, cmd->name, &ops, sizeof(ops));
}

static int
wl_p2p_noa(void *wl, cmd_t *cmd, char **argv)
{
	int count;
	wl_p2p_sched_t *noa;
	int len;
	int i;
	char *endptr;

	argv ++;

	strcpy(buf, cmd->name);

	count = ARGCNT(argv);
	if (count < 2) {
		int err = wlu_get(wl, WLC_GET_VAR, buf, WLC_IOCTL_MAXLEN);
		wl_p2p_sched_t *sched;
		int i;

		if (err != BCME_OK) {
			fprintf(stderr, "%s: error %d\n", cmd->name, err);
			return BCME_ERROR;
		}

		sched = (wl_p2p_sched_t *)buf;
		for (i = 0; i < 16; i ++) {
			if (sched->desc[i].count == 0)
				break;
			printf("start: %u interval: %u duration: %u count: %u\n",
			       sched->desc[i].start, sched->desc[i].interval,
			       sched->desc[i].duration, sched->desc[i].count);
		}

		return BCME_OK;
	}

	len = strlen(buf);

	noa = (wl_p2p_sched_t *)&buf[len + 1];
	len += 1;

	noa->type = (uint8)strtol(argv[0], &endptr, 0);
	len += sizeof(noa->type);
	noa->action = (uint8)strtol(argv[1], &endptr, 0);
	len += sizeof(noa->action);

	argv += 2;
	count -= 2;

	/* action == -1 is to cancel the current schedule */
	if (noa->action == WL_P2P_SCHED_ACTION_RESET) {
		/* the fixed portion of wl_p2p_sched_t with action == WL_P2P_SCHED_ACTION_RESET
		 * is required to cancel the curret schedule.
		 */
		len += (char *)&noa->desc[0] - ((char *)buf + len);
	}
	/* Take care of any special cases only and let all other cases fall through
	 * as normal 'start/interval/duration/count' descriptions.
	 * All cases start with 'type' 'action' 'option'.
	 * Any count value greater than 255 is to repeat unlimited.
	 */
	else {
		switch (noa->type) {
		case WL_P2P_SCHED_TYPE_ABS:
		case WL_P2P_SCHED_TYPE_REQ_ABS:
			if (count < 1)
				return -2;
			noa->option = (uint8)strtol(argv[0], &endptr, 0);
			len += sizeof(noa->option);
			argv += 1;
			count -= 1;
			break;
		}

		/* add any paddings before desc field */
		len += (char *)&noa->desc[0] - ((char *)buf + len);

		switch (noa->type) {
		case WL_P2P_SCHED_TYPE_ABS:
			switch (noa->option) {
			case WL_P2P_SCHED_OPTION_BCNPCT:
				if (count == 1) {
					noa->desc[0].duration = htod32(strtol(argv[0], &endptr, 0));
					noa->desc[0].start = 100 - noa->desc[0].duration;
				}
				else if (count == 2) {
					noa->desc[0].start = htod32(strtol(argv[0], &endptr, 0));
					noa->desc[0].duration = htod32(strtol(argv[1], &endptr, 0));
				}
				else {
					fprintf(stderr, "Usage: wl p2p_noa 0 %d 1 "
					        "<start-pct> <duration-pct>\n",
					        noa->action);
					return BCME_ERROR;
				}
				len += sizeof(wl_p2p_sched_desc_t);
				break;

			default:
				if (count < 4 || (count % 4) != 0) {
					fprintf(stderr, "Usage: wl p2p_noa 0 %d 0 "
					        "<start> <interval> <duration> <count> ...\n",
					        noa->action);
					return BCME_ERROR;
				}
				goto normal;
			}
			break;

		default:
			if (count != 4) {
				fprintf(stderr, "Usage: wl p2p_noa 1 %d "
				        "<start> <interval> <duration> <count> ...\n",
				        noa->action);
				return BCME_ERROR;
			}
			/* fall through... */
		normal:
			for (i = 0; i < count; i += 4) {
				noa->desc[i / 4].start = htod32(strtoul(argv[i], &endptr, 0));
				noa->desc[i / 4].interval = htod32(strtol(argv[i + 1], &endptr, 0));
				noa->desc[i / 4].duration = htod32(strtol(argv[i + 2], &endptr, 0));
				noa->desc[i / 4].count = htod32(strtol(argv[i + 3], &endptr, 0));
				len += sizeof(wl_p2p_sched_desc_t);
			}
			break;
		}
	}

	return wlu_set(wl, WLC_SET_VAR, buf, len);
}
#endif /* WLP2P */

static int
wl_rpmt(void *wl, cmd_t *cmd, char **argv)
{
	int count;
	int len;
	char *endptr;
	uint32 val;

	argv ++;

	count = ARGCNT(argv);
	if (count != 2) {
		return BCME_ERROR;
	}

	strcpy(buf, cmd->name);
	len = strlen(buf) + 1;

	val = htod32(strtoul(argv[0], &endptr, 0));
	memcpy(&buf[len], &val, sizeof(uint32));
	len += sizeof(uint32);
	val = htod32(strtoul(argv[1], &endptr, 0));
	memcpy(&buf[len], &val, sizeof(uint32));
	len += sizeof(uint32);

	return wlu_set(wl, WLC_SET_VAR, buf, len);
}

static int
wl_ledbh(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	wl_led_info_t led;
	void	*ptr = NULL;

	memset(&led, 0, sizeof(wl_led_info_t));
	if (*++argv == NULL) {
		printf("Usage: ledbh [led#] [behavior#]\n");
		return -1;
	}
	led.index = (int)strtoul(*argv, NULL, 10);

#ifdef DSLCPE
	if (led.index >= WL_LED_NUMGPIO) {
		printf("exceed the max led support\n");
		return -1;
	}
#else
	if (led.index > 3) {
		printf("only 4 led supported\n");
		return -1;
	}
#endif

	if (*++argv) {	/* set */
		/* Read the original back so we don't toggle the activehi */
		if ((err = wlu_var_getbuf(wl, cmd->name, (void*)&led,
			sizeof(wl_led_info_t), &ptr)) < 0) {
			printf("wl_ledbh: fail to get. code %x\n", err);
		}
		led.behavior = (int)strtoul(*argv, NULL, 10);
		led.activehi = ((wl_led_info_t*)ptr)->activehi;

		if ((err = wlu_var_setbuf(wl, cmd->name, (void*)&led,
			sizeof(wl_led_info_t))) < 0) {
			printf("wl_ledbh: fail to set\n");
		}
	} else {	/* get */
		wl_led_info_t *ledo;

		if ((err = wlu_var_getbuf(wl, cmd->name, (void*)&led,
			sizeof(wl_led_info_t), &ptr)) < 0) {
			printf("wl_ledbh: fail to get\n");
		}
		ledo = (wl_led_info_t*)ptr;

		printf("led %d behavior %d\n", ledo->index, ledo->behavior);
	}

	return 0;
}

static int
wl_led_blink_sync(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err = 0;
	int in_arg[2];
	void *ptr = NULL;
	char *endptr;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if (argc > 3 || argc < 2)
		return -1;

	in_arg[0] = htod32((uint32)(strtoul(argv[1], &endptr, 0)));

	if (in_arg[0] > 3) {
		printf("only 4 led supported\n");
		return -1;
	}

	if (argc == 2) {
		err = wlu_var_getbuf(wl, cmd->name, (void*)in_arg, sizeof(int), &ptr);
		if (err < 0)
			return err;
		printf("led%d, blink_sync is %s\n", in_arg[0],
			(dtoh32(*(int*)ptr) != 0) ? "TRUE" : "FALSE");
	}
	else if (argc == 3) {
		in_arg[1] = htod32((uint32)(strtoul(argv[2], &endptr, 0)));
		wlu_var_setbuf(wl, cmd->name, in_arg, sizeof(in_arg));
	}
	else {
		fprintf(stderr, "illegal command!\n");
		return -1;
	}

	return err;
}

static int
wl_rrm_stat_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	const char *cmdname = "rrm_stat_req";
	statreq_t sreq_buf;

	UNUSED_PARAMETER(cmd);
	memset(&sreq_buf, 0, sizeof(statreq_t));

	if (argv[1]) {
		/* da */
		if (!wl_ether_atoe(argv[1], &sreq_buf.da)) {
			printf("wl_rrm_stat_req parsing da failed\n");
			return -1;
		}
	}
	/* random interval */
	if (argv[2]) {
		sreq_buf.random_int = htod32(strtoul(argv[2], NULL, 0));
	}
	/* duration */
	if (argv[3]) {
		sreq_buf.dur = htod32(strtoul(argv[3], NULL, 0));
	}
	/* peer address */
	if (argv[4]) {
		if (!wl_ether_atoe(argv[4], &sreq_buf.peer)) {
			printf("wl_rrm_stat_req parsing peer failed\n");
			return -1;
		}
	}
	/* group id */
	if (argv[5]) {
		sreq_buf.group_id =
			htod32(strtoul(argv[5], NULL, 0));
	}
	/* repetitions */
	if (argv[6]) {
		sreq_buf.reps = htod32(strtoul(argv[6], NULL, 0));
	}
	err = wlu_iovar_set(wl, cmdname, &sreq_buf, sizeof(sreq_buf));
	return err;
}

static int
wl_rrm_frame_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	const char *cmdname = "rrm_frame_req";
	framereq_t freq_buf;

	UNUSED_PARAMETER(cmd);

	memset(&freq_buf, 0, sizeof(framereq_t));
	if (argv[1]) {
		/* Regulatory class */
		freq_buf.reg = htod32(strtoul(argv[1], NULL, 0));
	}

	/* da */
	if (argv[2]) {
		if (!wl_ether_atoe(argv[2], &freq_buf.da)) {
			printf("wl_rrm_frame_req parsing da failed\n");
			return -1;
		}
	}
	/* duration */
	if (argv[3]) {
		freq_buf.dur = htod32(strtoul(argv[3], NULL, 0));
	}
	/* random interval */
	if (argv[4]) {
		freq_buf.random_int = htod32(strtoul(argv[4], NULL, 0));
	}
	/* channel */
	if (argv[5]) {
		freq_buf.chan = htod32(strtoul(argv[5], NULL, 0));
	}
	/* transmit address */
	if (argv[6]) {
		if (!wl_ether_atoe(argv[6], &freq_buf.ta)) {
			printf("wl_rrm_frame_req parsing ta failed\n");
			return -1;
		}
	}
	/* repetitions */
	if (argv[7]) {
		freq_buf.reps = htod32(strtoul(argv[7], NULL, 0));
	}
	err = wlu_iovar_set(wl, cmdname, &freq_buf, sizeof(freq_buf));
	return err;
}

static int
wl_rrm_chload_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	const char *cmdname = "rrm_chload_req";
	rrmreq_t chreq_buf;

	UNUSED_PARAMETER(cmd);
	memset(&chreq_buf, 0, sizeof(rrmreq_t));

	if (argv[1]) {
		/* Regulatory class */
		chreq_buf.reg = htod32(strtoul(argv[1], NULL, 0));
	}
	/* da */
	if (argv[2]) {
		if (!wl_ether_atoe(argv[2], &chreq_buf.da)) {
			printf("wl_rrm_chload_req parsing da failed\n");
			return -1;
		}
	}
	/* duration */
	if (argv[3]) {
		chreq_buf.dur = htod32(strtoul(argv[3], NULL, 0));
	}
	/* random interval */
	if (argv[4]) {
		chreq_buf.random_int = htod32(strtoul(argv[4], NULL, 0));
	}
	/* channel */
	if (argv[5]) {
		chreq_buf.chan = htod32(strtoul(argv[5], NULL, 0));
	}
	/* repetitions */
	if (argv[6]) {
		chreq_buf.reps = htod32(strtoul(argv[6], NULL, 0));
	}
	err = wlu_iovar_set(wl, cmdname, &chreq_buf, sizeof(chreq_buf));
	return err;
}

static int
wl_rrm_noise_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	const char *cmdname = "rrm_noise_req";
	rrmreq_t nreq_buf;

	UNUSED_PARAMETER(cmd);
	printf("wl_rrm_noise_req\n");

	memset(&nreq_buf, 0, sizeof(rrmreq_t));
	if (argv[1]) {
		/* Regulatory class */
		nreq_buf.reg = htod32(strtoul(argv[1], NULL, 0));
	}
	/* da */
	if (argv[2]) {
		if (!wl_ether_atoe(argv[2], &nreq_buf.da)) {
			printf("wl_rrm_noise_req parsing da failed\n");
			return -1;
		}
	}
	/* duration */
	if (argv[3]) {
		nreq_buf.dur = htod32(strtoul(argv[3], NULL, 0));

	}
	/* random interval */
	if (argv[4]) {
		nreq_buf.random_int = htod32(strtoul(argv[4], NULL, 0));
	}
	/* channel */
	if (argv[5]) {
		nreq_buf.chan = htod32(strtoul(argv[5], NULL, 0));
	}
	/* repetitions */
	if (argv[6]) {
		nreq_buf.reps = htod32(strtoul(argv[6], NULL, 0));
	}
	err = wlu_iovar_set(wl, cmdname, &nreq_buf, sizeof(nreq_buf));
	return err;
}

static int
wl_rrm_bcn_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
#ifdef DSLCPE
	uint8 mode = 0;
#else
	uint8 mode;
#endif
	const char *cmdname = "rrm_bcn_req";
	bcnreq_t bcnreq_buf;
	wlc_ssid_t ssid;

	UNUSED_PARAMETER(cmd);
	memset(&bcnreq_buf, 0, sizeof(bcnreq_t));

	if (argv[1]) {
		/* bcn mode: ACTIVE/PASSIVE/SCAN_CACHE */
		mode = htod32(strtoul(argv[1], NULL, 0));
		if (mode > 2) {
			printf("wl_rrm_bcn_req parsing bcn mode failed\n");
			return -1;
		}
		bcnreq_buf.bcn_mode = mode;
	}
	/* da */
	if (argv[2]) {
		if (!wl_ether_atoe(argv[2], &bcnreq_buf.da)) {
			printf("wl_rrm_bcn_req parsing da failed\n");
			return -1;
		}
	}
	/* duration */
	if (argv[3]) {
		bcnreq_buf.dur = htod32(strtoul(argv[3], NULL, 0));
	}

	/* random interval */
	if (argv[4]) {
		bcnreq_buf.random_int = htod32(strtoul(argv[4], NULL, 0));
	}

	/* channel */
	if (argv[5]) {
		bcnreq_buf.channel = htod32(strtoul(argv[5], NULL, 0));
	}
	printf("wl_rrm_bcn_req:bcn mode: %d, duration: %d, "
			"chan: %d\n", mode,
			bcnreq_buf.dur, bcnreq_buf.channel);

	/* SSID */
	if (argv[6]) {
		uint32 len;

		len = strlen(argv[6]);
		if (len > DOT11_MAX_SSID_LEN) {
			printf("ssid too long\n");
			return (-1);
		}
		memset(&ssid, 0, sizeof(wlc_ssid_t));
		memcpy(ssid.SSID, argv[6], len);
		ssid.SSID_len = len;
		memcpy(&bcnreq_buf.ssid, &ssid, sizeof(wlc_ssid_t));
	}

	/* repetitions */
	if (argv[7]) {
		bcnreq_buf.reps = htod32(strtoul(argv[7], NULL, 0));
	}

	err = wlu_iovar_set(wl, cmdname, &bcnreq_buf, sizeof(bcnreq_buf));
	return err;
}

static int
wl_rrm_lm_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	const char *cmdname = "rrm_lm_req";

	struct ether_addr da;
	UNUSED_PARAMETER(cmd);

	if (argv[1]) {
		if (!wl_ether_atoe(argv[1], &da)) {
			printf("wl_rrm_lm_req parsing arg1 failed\n");
			return -1;
		}
	}
	err = wlu_iovar_set(wl, cmdname, &da, sizeof(da));
	return err;
}

static int
wl_rrm_nbr_req(void *wl, cmd_t *cmd, char **argv)
{
	int err, buflen;
	wlc_ssid_t ssid;

	UNUSED_PARAMETER(cmd);

	strcpy(buf, "rrm_nbr_req");
	buflen = strlen("rrm_nbr_req") + 1;

	if (*++argv) {
		uint32 len;

		len = strlen(*argv);
		if (len > DOT11_MAX_SSID_LEN) {
			printf("ssid too long\n");
			return (-1);
		}
		memset(&ssid, 0, sizeof(wlc_ssid_t));
		memcpy(ssid.SSID, *argv, len);
		ssid.SSID_len = len;
		memcpy(&buf[buflen], &ssid, sizeof(wlc_ssid_t));
		buflen += sizeof(wlc_ssid_t);
	}

	err = wlu_set(wl, WLC_SET_VAR, buf, buflen);

	return err;
}

static dbg_msg_t wl_wnm_msgs[] = {
	{WL_WNM_BSSTRANS,		"BSS-Transition"},
	{WL_WNM_PROXYARP,		"Proxy-ARP"},
	{WL_WNM_BSS_MAX_IDLE_PERIOD,	"BSS-Max-Idle-Period"},
	{WL_WNM_TIM_BCAST,		"TIM-Broadcast"},
	{WL_WNM_TFS,			"Traffic-Filter-Service"},
	{WL_WNM_SLEEP,			"WNM-Sleep-Mode"},
	{WL_WNM_DMS,			"Direct-Multi-Cast"},
	{WL_WNM_FMS,			"Flexible-Multi-Cast"},
	{WL_WNM_NOTIF,			"Notification"},
	{0, ""}
};

static int
wl_wnm(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i;
	uint val, last_val = 0;
	int wnmmask;
	char *s;
	dbg_msg_t *dbg_msg = wl_wnm_msgs;

	if ((ret = wlu_iovar_getint(wl, cmd->name, &wnmmask)) < 0)
		return (ret);

	if (!*++argv) {
		printf("0x%x: ", wnmmask);
		for (i = 0; (val = dbg_msg[i].value); i++) {
			if ((wnmmask & val) && (val != last_val)) {
				printf("%s ", dbg_msg[i].string);
			}
			last_val = val;
		}
		printf("\n");
		return (0);
	}

	s = *argv;
	if (*s == '?')
		goto usage;

	wnmmask = strtoul(s, NULL, 16);

	return (wlu_iovar_setint(wl, cmd->name, wnmmask));

usage:
	fprintf(stderr, "wnm mask values need to be the combination of numbers "
		"from the following set.\n");

	for (i = 0; (val = wl_wnm_msgs[i].value); i++) {
		if (val != last_val)
			fprintf(stderr, "\n0x%04x %s", val, wl_wnm_msgs[i].string);
		else
			fprintf(stderr, ", %s", wl_wnm_msgs[i].string);
		last_val = val;
	}
	fprintf(stderr, "\n");

	return 0;
}

static int
wl_wnm_bsstq(void *wl, cmd_t *cmd, char **argv)
{
	int err, buflen;
	wlc_ssid_t ssid;

	UNUSED_PARAMETER(cmd);

	strcpy(buf, "wnm_bsstq");
	buflen = strlen("wnm_bsstq") + 1;

	if (*++argv) {
		uint32 len;

		len = strlen(*argv);
		if (len > DOT11_MAX_SSID_LEN) {
			printf("ssid too long\n");
			return (-1);
		}
		memset(&ssid, 0, sizeof(wlc_ssid_t));
		memcpy(ssid.SSID, *argv, len);
		ssid.SSID_len = len;
		memcpy(&buf[buflen], &ssid, sizeof(wlc_ssid_t));
		buflen += sizeof(wlc_ssid_t);
	}

	err = wlu_set(wl, WLC_SET_VAR, buf, buflen);

	return err;
}

#ifdef WLWNM
#define WNM_TCLAS_ARG_CHECK(argv, str) \
	do { \
		if (*++(argv) == NULL) { \
			printf("TCLAS Frmae Classifier: %s not provided\n", (str)); \
			return BCME_ERROR; \
		} \
	} while (0)

static int
wl_wnm_tclas_add(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err = -1, buflen;
	dot11_tclas_fc_t *fc_param;

	UNUSED_PARAMETER(cmd);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	strcpy(buf, "wnm_tclas_add");
	buflen = strlen("wnm_tclas_add") + 1;

	if (argc <= 2 || *++(argv) == NULL) {
		printf("No args or no service (dms, tfs, fms) provided\n");
		return BCME_ERROR;
	}

	/* Check the service type */
	if (!strcmp(*argv, "dms"))
		buf[buflen++] = TCLAS_SERVICE_DMS;
	else if (!strcmp(*argv, "tfs"))
		buf[buflen++] = TCLAS_SERVICE_TFS;
	else if (!strcmp(*argv, "fms"))
		buf[buflen++] = TCLAS_SERVICE_FMS;
	else {
		printf("Service (dms, tfs, fms) not found\n");
		return BCME_ERROR;
	}

	/* Check the user priority value */
	WNM_TCLAS_ARG_CHECK(argv, "user priority");
	buf[buflen++] = (uint8)strtoul(*argv, NULL, 0);

	fc_param = (dot11_tclas_fc_t *) (buf + buflen);
	memset((void *)fc_param, 0, sizeof(dot11_tclas_fc_t));

	/* Parse frame classifier type and mask */
	WNM_TCLAS_ARG_CHECK(argv, "type");
	fc_param->u.data[0] = (uint8)strtoul(*argv, NULL, 0);

	WNM_TCLAS_ARG_CHECK(argv, "mask");
	fc_param->u.data[1] = (uint8)strtoul(*argv, NULL, 0);

	if (fc_param->u.data[0] == DOT11_TCLAS_FC_TYPE_1_IP ||
		fc_param->u.data[0] == DOT11_TCLAS_FC_TYPE_4_IP_HIGHER) {
		uint8 version;

		WNM_TCLAS_ARG_CHECK(argv, "ip version");
		version = (uint8)strtoul(*argv, NULL, 0);

		if (version == IP_VER_4) {
			dot11_tclas_fc_4_ipv4_t *fc_ipv4;
			fc_ipv4 = (dot11_tclas_fc_4_ipv4_t *)fc_param;

			fc_ipv4->version = version;

			WNM_TCLAS_ARG_CHECK(argv, "ipv4 source ip");
			wl_atoip(*argv, (struct ipv4_addr *)&fc_ipv4->src_ip);

			WNM_TCLAS_ARG_CHECK(argv, "ipv4 dest ip");
			wl_atoip(*argv, (struct ipv4_addr *)&fc_ipv4->dst_ip);

			WNM_TCLAS_ARG_CHECK(argv, "ipv4 source port");
			fc_ipv4->src_port = (uint16)strtoul(*argv, NULL, 0);
			fc_ipv4->src_port = hton16(fc_ipv4->src_port);

			WNM_TCLAS_ARG_CHECK(argv, "ipv4 dest port");
			fc_ipv4->dst_port = (uint16)strtoul(*argv, NULL, 0);
			fc_ipv4->dst_port = hton16(fc_ipv4->dst_port);

			WNM_TCLAS_ARG_CHECK(argv, "ipv4 dscp");
			fc_ipv4->dscp = (uint8)strtoul(*argv, NULL, 0);

			WNM_TCLAS_ARG_CHECK(argv, "ipv4 protocol");
			fc_ipv4->protocol = (uint8)strtoul(*argv, NULL, 0);

			buflen += DOT11_TCLAS_FC_TYPE_4_IPV4_LEN;

			err = wlu_set(wl, WLC_SET_VAR, buf, buflen);
		} else
			return BCME_ERROR;
	} else {
		printf("Unsupported frame classifier type 0x%2x\n", fc_param->u.data[0]);
		return BCME_ERROR;
	}

	return err;
}

static int
wl_wnm_tclas_del(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	uint8 len = 0, start_idx, end_idx;
	int err = -1, buflen;

	UNUSED_PARAMETER(cmd);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	strcpy(buf, "wnm_tclas_del");
	buflen = strlen("wnm_tclas_del") + 1;

	if ((argc < 3 || argc > 4) || *++(argv) == NULL) {
		printf("Incorrect args or no service (dms, tfs, fms) provided\n");
		return BCME_ERROR;
	}

	/* Check the service type */
	if (!strcmp(*argv, "dms"))
		buf[buflen] = TCLAS_SERVICE_DMS;
	else if (!strcmp(*argv, "tfs"))
		buf[buflen] = TCLAS_SERVICE_TFS;
	else if (!strcmp(*argv, "fms"))
		buf[buflen] = TCLAS_SERVICE_FMS;
	else {
		printf("Service (dms, tfs, fms) not found\n");
		return BCME_ERROR;
	}

	buflen += 1;

	if (argc >= 3) {
		if (*++argv == NULL)
			return BCME_ERROR;
		start_idx = (uint8)strtoul(*argv, NULL, 0);
		len++;
	}

	if (argc == 4) {
		if (*++argv == NULL)
			return BCME_ERROR;
		end_idx = (uint8)strtoul(*argv, NULL, 0);
		if (end_idx < start_idx) {
			printf("Incorrect end_idx value (%d) < start_idx (%d)\n",
				end_idx, start_idx);
			return BCME_ERROR;
		}
		len++;
	}

	buf[buflen++] = len;
	buf[buflen++] = start_idx;
	if (argc == 4)
		buf[buflen++] = end_idx;

	err = wlu_set(wl, WLC_SET_VAR, buf, buflen);

	return err;
}

static void
wl_wnm_tclas_param_dump(wl_tclas_fc_param_t *fc_param)
{
	dot11_tclas_fc_t *param = &fc_param->param;

	printf("up %d ", fc_param->user_priority);

	/* Check frame classifier parameter type */
	if (param->u.data[0] == DOT11_TCLAS_FC_TYPE_1_IP ||
		param->u.data[0] == DOT11_TCLAS_FC_TYPE_4_IP_HIGHER) {
		/* Check parameter type 4 IP version */
		if (param->u.data[2] == IP_VER_4) {
			dot11_tclas_fc_4_ipv4_t *ipv4;

			ipv4 = (dot11_tclas_fc_4_ipv4_t *)param;

			printf("type %d ", ipv4->type);
			printf("mask %d ", ipv4->mask);
			printf("ver %d ", ipv4->version);
			printf("sip %s ", wl_iptoa((struct ipv4_addr *)&ipv4->src_ip));
			printf("dip %s ", wl_iptoa((struct ipv4_addr *)&ipv4->dst_ip));
			printf("sp %d ", ntoh16(ipv4->src_port));
			printf("dp %d ", ntoh16(ipv4->dst_port));
			printf("dscp %d ", ipv4->dscp);
			printf("prot %d\n", ipv4->protocol);
		}
	} else {
		printf("Unsupported frame classifier type 0x%2x\n", param->u.data[0]);
	}
}

static int
wl_wnm_tclas_list_parse(wl_tclas_fc_param_list_t *list)
{
	uint32 i;
	uint8 *ptr = (uint8 *)&list->param[0];
	int retlen = 0;

	for (i = 0; i < list->num; i++) {
		wl_tclas_fc_param_t *fc_param = (wl_tclas_fc_param_t *)ptr;

		printf("tclas idx %d: ", i);

		wl_wnm_tclas_param_dump(fc_param);

		retlen += sizeof(fc_param->user_priority) + sizeof(fc_param->len) + fc_param->len;

		/* Move to the next tclas frame classifier parameter */
		ptr += sizeof(fc_param->user_priority) + sizeof(fc_param->len) + fc_param->len;
	}

	return retlen;
}

static int
wl_wnm_tclas_list(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err = -1;
	void *ptr = NULL;
	uint8 service_type;
	wl_tclas_fc_param_list_t *fc_param_list;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if (argc != 2 || *++(argv) == NULL) {
		printf("Incorrect args or no service (dms, tfs, fms) provided\n");
		return BCME_ERROR;
	}

	/* Check the service type */
	if (!strcmp(*argv, "dms"))
		service_type = TCLAS_SERVICE_DMS;
	else if (!strcmp(*argv, "tfs"))
		service_type = TCLAS_SERVICE_TFS;
	else if (!strcmp(*argv, "fms"))
		service_type = TCLAS_SERVICE_FMS;
	else {
		printf("Service (dms, tfs, fms) not found\n");
		return BCME_ERROR;
	}

	if ((err = wlu_var_getbuf(wl, cmd->name, &service_type, sizeof(service_type), &ptr)) < 0)
		return err;

	fc_param_list = (wl_tclas_fc_param_list_t *)ptr;

	if (fc_param_list->num == 0)
		printf("No tclas frame classifier parameter entry\n");
	else
		wl_wnm_tclas_list_parse(fc_param_list);

	return err;
}

static int
wl_wnm_tfsreq_add(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err = -1, buflen;
	wl_tfs_req_t tfs_req;

	UNUSED_PARAMETER(cmd);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	strcpy(buf, "wnm_tfsreq_add");
	buflen = strlen("wnm_tfsreq_add") + 1;

	if (argc != 5 || *++(argv) == NULL) {
		printf("Incorrect args provided\n");
		return BCME_ERROR;
	}

	tfs_req.tfs_id = (uint8)strtoul(*argv++, NULL, 0);

	if (*argv != NULL)
		tfs_req.tfs_actcode = (uint8)strtoul(*argv++, NULL, 0);
	else {
		printf("Incorrect args provided\n");
		return BCME_ERROR;
	}

	if (*argv != NULL)
		tfs_req.tfs_subelem_id = (uint8)strtoul(*argv++, NULL, 0);
	else {
		printf("Incorrect args provided\n");
		return BCME_ERROR;
	}

	if (*argv != NULL)
		tfs_req.send = strtoul(*argv, NULL, 0) == 0 ? FALSE : TRUE;
	else {
		printf("Incorrect args provided\n");
		return BCME_ERROR;
	}

	if (tfs_req.tfs_id == 0 ||
		tfs_req.tfs_actcode > 3 ||
		tfs_req.tfs_subelem_id > 1) {
		printf("Input args not in range\n");
		return BCME_ERROR;
	}

	buf[buflen++] = tfs_req.tfs_id;
	buf[buflen++] = tfs_req.tfs_actcode;
	buf[buflen++] = tfs_req.tfs_subelem_id;
	buf[buflen++] = tfs_req.send;

	err = wlu_set(wl, WLC_SET_VAR, buf, buflen);

	return err;
}

static int
wl_wnm_dmsdesc_add(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err = -1, buflen;

	UNUSED_PARAMETER(cmd);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	strcpy(buf, "wnm_dmsdesc_add");
	buflen = strlen("wnm_dmsdesc_add") + 1;

	if (argc != 2 || *++(argv) == NULL) {
		printf("Incorrect args provided\n");
		return BCME_ERROR;
	}

	buf[buflen++] = strtoul(*argv, NULL, 0) == 0 ? FALSE : TRUE;

	err = wlu_set(wl, WLC_SET_VAR, buf, buflen);

	return err;
}
#endif /* WLWNM */

static int
wl_tsf(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "tsf";
	struct tsf {
		uint32 low;
		uint32 high;
	} tsf_buf;
	char *endptr;
	int err;

	UNUSED_PARAMETER(cmd);

	/* toss the command name */
	argv++;

	if (*argv == NULL) {
		/* get */
		err = wlu_iovar_get(wl, cmdname, &tsf_buf, sizeof(tsf_buf));
		if (err)
			return err;
		printf("0x%08X 0x%08X\n", htod32(tsf_buf.high), htod32(tsf_buf.low));
	} else {
		/* set */
		if (argv[1] == NULL)
			return USAGE_ERROR;

		tsf_buf.high = (uint32)strtoul(*argv, &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "%s: %s: error parsing \"%s\" as an integer\n",
			        wlu_av0, cmdname, *argv);
			return USAGE_ERROR;
		}

		argv++;
		tsf_buf.low = (uint32)strtoul(*argv, &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "%s: %s: error parsing \"%s\" as an integer\n",
			        wlu_av0, cmdname, *argv);
			return USAGE_ERROR;
		}

		tsf_buf.low = dtoh32(tsf_buf.low);
		tsf_buf.high = dtoh32(tsf_buf.high);

		err = wlu_iovar_set(wl, cmdname, &tsf_buf, sizeof(tsf_buf));
		if (err)
			return err;
	}

	return err;
}

static int
wl_mfp_config(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err = 0;
	int flag = 0;
	const char *cmdname = "mfp";

	UNUSED_PARAMETER(cmd);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if (argc > 1 && argv[1]) {
		flag = htod32(atoi(argv[1]));
		*(int *)buf = flag;
	}

	err = wlu_iovar_set(wl, cmdname, buf, 256);

	return (err);

}

static int
wl_mfp_sha256(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err = 0;
	int flag = 0;
	const char *cmdname = "mfp_sha256";

	UNUSED_PARAMETER(cmd);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if (argc > 1 && argv[1]) {
		flag = htod32(atoi(argv[1]));
		*(int *)buf = flag;
		err = wlu_iovar_set(wl, cmdname, buf, 256);
	} else {
		/* get */
		err = wlu_iovar_getint(wl, cmdname, &flag);
		if (err == BCME_OK)
			printf("%d\n", flag);
	}

	return (err);

}

static int
wl_mfp_sa_query(void *wl, cmd_t *cmd, char **argv)
{
	wl_sa_query_t * query;
	int argc;
	int err = 0;

	UNUSED_PARAMETER(cmd);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if ((query = (wl_sa_query_t *) malloc(sizeof(wl_sa_query_t))) == NULL) {
		printf("unable to allocate frame \n");
		return -1;
	}
	memset(query, 0, sizeof(wl_sa_query_t));

	/* add the flag */
	if (argc > 1 && argv[1]) {
		query->flag = htod32(atoi(argv[1]));
	}

	/* add the action */
	if (argc > 2 && argv[2]) {
		query->action = htod32(atoi(argv[2]));
	}

	/* add the id */
	if (argc > 3 && argv[3]) {
		query->id = htod32(atoi(argv[3]));
	}

	err = wlu_var_setbuf(wl, "mfp_sa_query", query, sizeof(wl_sa_query_t));

	free(query);

	return (err);

}

static int
wl_mfp_disassoc(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "mfp_disassoc";
	int argc;
	int	flag;
	char varbuf[256];
	int err;

	UNUSED_PARAMETER(cmd);
	memset(varbuf, 0, 256);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	/* add the action */
	if (argc > 1 && argv[1]) {
		flag = htod32(atoi(argv[1]));
		*(int *)varbuf = flag;
	}
	if (argc > 2 && argv[2]) {
		flag = htod32(atoi(argv[2]));
		*(int *)(varbuf + sizeof(flag)) = flag;
	}

	err = wlu_iovar_set(wl, cmdname, varbuf, 256);
	if (err)
		return err;

	return err;
}

static int
wl_mfp_deauth(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "mfp_deauth";
	int argc;
	int	flag;
	char varbuf[256];
	int err;

	UNUSED_PARAMETER(cmd);

	memset(varbuf, 0, 256);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	/* add the action */
	if (argc > 1 && argv[1]) {
		flag = htod32(atoi(argv[1]));
		*(int *)varbuf = flag;
	}
	if (argc > 2 && argv[2]) {
		flag = htod32(atoi(argv[2]));
		*(int *)(varbuf + sizeof(flag)) = flag;
	}

	err = wlu_iovar_set(wl, cmdname, varbuf, 256);
	if (err)
		return err;

	return err;
}

static int
wl_mfp_assoc(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "mfp_assoc";
	int argc;
	int	flag;
	char varbuf[256];
	int err;

	UNUSED_PARAMETER(cmd);
	memset(varbuf, 0, 256);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	/* add the action */
	if (argc > 1 && argv[1]) {
		flag = htod32(atoi(argv[1]));
		*(int *)varbuf = flag;
	}
	if (argc > 2 && argv[2]) {
		flag = htod32(atoi(argv[2]));
		*(int *)(varbuf + sizeof(int)) = flag;
	}

	err = wlu_iovar_set(wl, cmdname, varbuf, 256);
	if (err)
		return err;

	return err;
}

static int
wl_mfp_auth(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "mfp_auth";
	int argc;
	int	flag;
	char varbuf[256];
	int err;

	UNUSED_PARAMETER(cmd);
	memset(varbuf, 0, 256);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	/* add the action */
	if (argc > 1 && argv[1]) {
		flag = htod32(atoi(argv[1]));
		*(int *)varbuf = flag;
	}
	if (argc > 2 && argv[2]) {
		flag = htod32(atoi(argv[2]));
		*(int *)(varbuf + sizeof(int)) = flag;
	}

	err = wlu_iovar_set(wl, cmdname, varbuf, 256);
	if (err)
		return err;

	return err;
}


static int
wl_mfp_reassoc(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "mfp_reassoc";
	int argc;
	int	flag;
	char varbuf[256];
	int err;

	UNUSED_PARAMETER(cmd);
	memset(varbuf, 0, 256);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	/* add the action */
	if (argc > 1 && argv[1]) {
		flag = htod32(atoi(argv[1]));
		*(int *)varbuf = flag;
	}
	if (argc > 2 && argv[2]) {
		flag = htod32(atoi(argv[2]));
		*(int *)(varbuf + sizeof(int)) = flag;
	}

	err = wlu_iovar_set(wl, cmdname, varbuf, 256);
	if (err)
		return err;

	return err;
}

#ifdef SERDOWNLOAD
/* Check that strlen("membytes")+1 + 2*sizeof(int32) + MEMBLOCK <= WLC_IOCTL_MAXLEN */
#if (MEMBLOCK + 17 > WLC_IOCTL_MAXLEN)
#error MEMBLOCK/WLC_IOCTL_MAXLEN sizing
#endif

static int dhd_hsic_download(void *dhd, char *fwname, char *nvname);
static int ReadFiles(char *fwfile, char *nvfile, unsigned char ** buffer);
static int check_file(unsigned char *headers);

static char* chip_select = "none";

int
dhd_init(void *dhd, cmd_t *cmd, char **argv)
{
	int ret = -1;
	UNUSED_PARAMETER(cmd);

	if (!*++argv) {
		fprintf(stderr, "Error: Missing require chip ID"
			"<4325,  4329, 43291, 4330a1, 4330 or hsic>\n");
	}
	else if (strcmp(*argv, "4325") && strcmp(*argv, "4329") &&
		strcmp(*argv, "43291") && strcmp(*argv, "4330") &&
		strcmp(*argv, "4330a1") && strcmp(*argv, "hsic")) {
		fprintf(stderr, "Error: Unsupported chip ID %s\n", *argv);
	}
	else if ((ret = wlu_iovar_setbuf(dhd, "init", *argv, strlen(*argv) + 1,
		buf, WLC_IOCTL_MAXLEN))) {
		fprintf(stderr, "Error: %s: failed to initialize the dongle \n",
		        "dhd_init()");
	}
	else
		ret = 0;

	if (ret == 0) {
		if (!strcmp(*argv, "4325"))  {
			fprintf(stdout, "4325 is the selected chip id\n");
			chip_select = "4325";
		} else if (!strcmp(*argv, "4329"))  {
			fprintf(stdout, "4329 is the selected chip id\n");
			chip_select = "4329";
	        } else if (!strcmp(*argv, "43291"))  {
			fprintf(stdout, "43291 is the selected chip id\n");
		        chip_select = "43291";
		} else if (!strcmp(*argv, "4330"))  {
			fprintf(stdout, "4330b0 is the selected chip id\n");
		        chip_select = "4330b0";
		} else if (!strcmp(*argv, "4330a1"))  {
			fprintf(stdout, "4330a1 is the selected chip id\n");
		        chip_select = "4330a1";
		} else if (!strcmp(*argv, "hsic"))  {
			fprintf(stdout, "hsic interface is selected\n");
		        chip_select = "hsic";
		} else
			chip_select = "none";
	}

	return ret;
}

int
dhd_download(void *dhd, cmd_t *cmd, char **argv)
{
	char *fname = NULL;
	char *vname = NULL;
	uint32 start = 0;
	uint32 last4bytes;
	int ret = 0;
	uint file_size;
	int ram_size, var_size, var_words, nvram_len, remained_bytes;
	FILE *fp = NULL;
	struct stat finfo;
	char *bufp;
	int len;
	uint8 memblock[MEMBLOCK];
	uint8 varbuf[WLC_IOCTL_MAXLEN];

	UNUSED_PARAMETER(cmd);

	if (!strcmp(chip_select, "none")) {
		fprintf(stderr, "chip init must be called before firmware download. \n");
		goto exit;
	}

	if (!strcmp(chip_select, "4325")) {
		fprintf(stdout, "using 4325 ram_info\n");
		ram_size = RAM_SIZE_4325;
	} else if (!strcmp(chip_select, "4329")) {
		fprintf(stdout, "using 4329 ram_info\n");
		ram_size = RAM_SIZE_4329;
	} else if (!strcmp(chip_select, "43291")) {
		fprintf(stdout, "using 43291 ram_info\n");
		ram_size = RAM_SIZE_43291;
	} else if (!strcmp(chip_select, "4330b0")) {
		fprintf(stdout, "using 4330 b0 ram_info\n");
		ram_size = RAM_SIZE_4330_b0;
	} else if (!strcmp(chip_select, "4330a1")) {
		fprintf(stdout, "using 4330a1 ram_info\n");
		ram_size = RAM_SIZE_4330_a1;
	} else if (!strcmp(chip_select, "hsic")) {
		fprintf(stdout, "using hsic interface\n");
	} else {
		fprintf(stderr, "Error: unknown chip\n");
		goto exit;
	}

	if (!*++argv) {
		fprintf(stderr, "Require dongle image filename \n");
		ret = -1;
		goto exit;
	}
	else {
		fname = *argv;
		if (debug)
			printf("dongle image file is %s\n", fname);
	}

	if (!*++argv) {
		fprintf(stderr, "vars filename missing, assuming no var file\n");
		ret = -1;
		goto exit;
	}
	else {
		vname = *argv;
		if (debug)
			printf("dongle var file is %s\n", vname);
	}


	/* do the download on hsic */
	/* merge the firmware and the nvram */
	if (!strcmp(chip_select, "hsic")) {
		/* the hsic firwmare and download code */
		ret = dhd_hsic_download(dhd, fname, vname);
		return ret;
	}

	if (!(fp = fopen(fname, "rb"))) {
		perror(fname);
		ret = -1;
		goto exit;
	}

	if (stat(fname, &finfo)) {
		perror(fname);
		ret = -1;
		goto exit;
	}
	file_size = finfo.st_size;
	if (debug) {
		printf("%s file_size=%d\n", fname, file_size);
	}

	/* do the download reset if not suppressed */
	if ((ret = wlu_iovar_setint(dhd, "download", TRUE))) {
		fprintf(stderr, "%s: failed to put dongle in download mode\n",
				"dhd_iovar_setint()");
		goto exit;
	}

	memset(memblock, 0, MEMBLOCK);

	printf("downloading %s, file_size=%d\n", fname, file_size);

	/* read the file and push blocks down to memory */
	while ((len = fread(memblock, sizeof(uint8), MEMBLOCK, fp))) {
		if (len < MEMBLOCK && !feof(fp)) {
			fprintf(stderr, "%s: error reading file %s\n", "fread()", fname);
			ret = -1;
			goto exit;
		}

		if (debug) {
			printf("memblock=\n%s\n", memblock);
		}

		bufp = buf;
		memset(bufp, 0, WLC_IOCTL_MAXLEN);
		strcpy(bufp, "membytes");
		bufp += strlen("membytes") + 1;
		memcpy(bufp, &start, sizeof(int));
		bufp += sizeof(int);
		memcpy(bufp, &len, sizeof(int));
		bufp += sizeof(int);
		memcpy(bufp, memblock, len);

		ret = wl_set(dhd, WLC_SET_VAR, &buf[0], (bufp - buf + len));

		if (ret) {
			fprintf(stderr, "%s: error %d on writing %d membytes at 0x%08x\n",
			        "wl_set()", ret, len, start);
			goto exit;
		}

		start += len;
		memset(memblock, 0, MEMBLOCK);
	}

	if (!feof(fp)) {
		fprintf(stderr, "%s: error reading file %s\n", "feof()", fname);
		ret = -1;
		goto exit;
	}
	fclose(fp);
	fp = NULL;

	if (vname) {
	/* download the vars file if specified */
	/* read in the file */
		if (!(fp = fopen(vname, "rb"))) {
			perror(vname);
			ret = -1;
			goto exit;
		}

		if (stat(vname, &finfo)) {
			perror(vname);
			ret = -1;
			goto exit;
		}
		file_size = finfo.st_size;

		printf("downloading %s, file_size=%d\n", vname, file_size);

		memset(varbuf, 0, WLC_IOCTL_MAXLEN);

		/* read the file and push blocks down to memory */
		if (fread(varbuf, 1, file_size, fp) != file_size) {
			perror(fname);
			ret = -1;
			goto exit;
		}

		fclose(fp);
		fp = NULL;

		if (debug) {
			printf("the original varbuf=%s\n", varbuf);
		}

		/* convert linefeeds to nuls */
		nvram_len = process_nvram_vars((char*)&varbuf, file_size);
		if (debug) {
			printf("after process_nvram_vars(), %s nvram_len=%d\n%s\n",
			vname, nvram_len, varbuf);
		}
		bufp = (char*)&varbuf + nvram_len;
		*bufp++ = 0;

		var_size = ROUNDUP(nvram_len + 1, 4);
		/* calculate start address */
		start = ram_size - var_size - 4;

		if (debug) {
			printf("var_size=%d, start=0x%0X\n", var_size, start);
		}

		/* need to send the last 4 bytes. */
		var_words = var_size / 4;
		last4bytes = (~var_words << 16) | (var_words & 0x0000FFFF);
		last4bytes = htol32(last4bytes);

		if (debug) {
			printf("last4bytes=0x%0X\n", last4bytes);
		}

		bufp = (char*)&varbuf + var_size;
		memcpy(bufp, &last4bytes, 4);

		/* send down var_size+4 bytes with each time "membytes" MEMBLOCK bytes */
		bufp = (char*)&varbuf;
		remained_bytes = var_size + 4;

		while (remained_bytes > 0) {
			char *p;

			p = buf;
			memset(p, 0, WLC_IOCTL_MAXLEN);

			strcpy(p, "membytes");
			p += strlen("membytes") + 1;

			memcpy(p, &start, sizeof(int));
			p += sizeof(int);

			if (remained_bytes >= MEMBLOCK) {
				len = MEMBLOCK;
			}
			else
				len = remained_bytes;

			memcpy(p, &len, sizeof(int));
			p += sizeof(int);

			memcpy(p, bufp, len);
			p += len;

			if (debug) {
				printf("sending %d bytes block:\n", (int)(p - buf));
				printf("%s\n", buf);
			}

			ret = wl_set(dhd, WLC_SET_VAR, &buf[0], (p - buf));

			if (ret) {
				fprintf(stderr, "%s: error %d on writing %d membytes at 0x%08x\n",
						"wl_set()", ret, len, start);
				goto exit;
			}

			start += len;
			bufp += len;
			remained_bytes -= len;
		}
	}

	/* start running the downloaded code if not suppressed */
	if ((ret = wlu_iovar_setint(dhd, "download", FALSE))) {
		fprintf(stderr, "%s: failed to take dongle out of download mode\n",
				"dhd_iovar_setint()");
		goto exit;
	}

exit:
	if (fp)
		fclose(fp);

	return ret;
}

int
hsic_download(void *dhd, cmd_t *cmd, char **argv)
{
	char *fname = NULL;
	char *vname = NULL;
	int ret = 0;
	UNUSED_PARAMETER(cmd);
	if (!*++argv) {
		fprintf(stderr, "Require dongle image filename \n");
		ret = -1;
		goto exit;
	}
	else {
		fname = *argv;
		if (debug)
			printf("dongle image file is %s\n", fname);
	}
	if (!*++argv) {
		fprintf(stderr, "vars filename missing, assuming no var file\n");
		ret = -1;
		goto exit;
	}
	else {
		vname = *argv;
		if (debug)
			printf("dongle var file is %s\n", vname);
	}
	ret = dhd_hsic_download(dhd, fname, vname);
exit:
	return ret;
}

int
dhd_upload(void *dhd, cmd_t *cmd, char **argv)
{
	char *fname = NULL;
	uint32 start = 0;
	uint32 size;
	int ram_size;
	FILE *fp = NULL;
	uint len;
	int ret = 0;

	UNUSED_PARAMETER(cmd);

	if (!strcmp(chip_select, "none")) {
		fprintf(stderr, "chip init must be called before firmware download. \n");
		goto exit;
	}

	if (!strcmp(chip_select, "4325")) {
		fprintf(stdout, "using 4325 ram_info\n");
		ram_size = RAM_SIZE_4325;
	} else if (!strcmp(chip_select, "4329")) {
		fprintf(stdout, "using 4329 ram_info\n");
		ram_size = RAM_SIZE_4329;
	} else if (!strcmp(chip_select, "43291")) {
		fprintf(stdout, "using 43291 ram_info\n");
		ram_size = RAM_SIZE_43291;
	} else if (!strcmp(chip_select, "4330b0")) {
		fprintf(stdout, "using 4330 b0 ram_info\n");
		ram_size = RAM_SIZE_4330_b0;
	} else if (!strcmp(chip_select, "4330a1")) {
		fprintf(stdout, "using 4330 a1 ram_info\n");
		ram_size = RAM_SIZE_4330_a1;
	} else {
		fprintf(stderr, "Error: unknown chip\n");
		goto exit;
	}

	argv++;

	if (debug) {
		printf("argv=%s\n", *argv);
	}

	fname = *argv;

	/* validate arguments */
	if (!fname) {
		fprintf(stderr, "filename required\n");
		ret = -1;
		goto exit;
	}

	if (!(fp = fopen(fname, "wb"))) {
		perror(fname);
		ret = -1;
		goto exit;
	}

	/* default size to full RAM */
	size = ram_size - start;

	/* read memory and write to file */
	while (size) {
		char *ptr;
		int params[2];

		len = MIN(MEMBLOCK, size);

		params[0] = start;
		params[1] = len;
		ret = wlu_iovar_getbuf(dhd, "membytes", params, 2 * sizeof(int),
		(void**)&ptr, MEMBLOCK);
		if (ret) {
			fprintf(stderr, "dhd_upload(): failed reading %d membytes from 0x%08x\n",
			        len, start);
			break;
		}

		if (fwrite(ptr, sizeof(*ptr), len, fp) != len) {
			fprintf(stderr, "dhd_upload(): error writing to file %s\n", fname);
			ret = -1;
			break;
		}

		start += len;
		size -= len;
	}

	fclose(fp);
exit:
	return ret;
}

static int
dhd_hsic_download(void *dhd, char *fwfile, char *nvfile)
{
	unsigned char *bufp  = NULL;
	int len = 0, length = 0;
	int ret = 0;
	char *p;
	unsigned char *buff = NULL;
	int remained_bytes;
	uint32 start = 0;

	/* read and merge fw and nvram files */
	length = ReadFiles(fwfile, nvfile, &bufp);
	if (length <= 0) {
		ret = -1;
		goto exit;
	}

	printf("Starting download, total file length is %d\n", length);

	/* do the download reset */
	if ((ret = wlu_iovar_setint(dhd, "download", TRUE))) {
		fprintf(stderr, "dhd_hsic_download: failed to put dongle to download mode\n");
		ret = -1;
		goto exit;
	}

	buff = bufp;
	remained_bytes = len = length;

	while (remained_bytes > 0) {
		printf(".");
		p = buf;
		memset(p, 0, WLC_IOCTL_MAXLEN);
		strcpy(p, "membytes");
		p += strlen("membytes") + 1;

		memcpy(p, &start, sizeof(int));
		p += sizeof(int);

		if (remained_bytes >= MEMBLOCK)
			len = MEMBLOCK;
		else
			len = remained_bytes;

		memcpy(p, &len, sizeof(int));
		p += sizeof(int);

		memcpy(p, buff, len);
		p += len;

		if (debug) {
			printf("sending %d bytes block: \n", (int)(p - buf));
		}

		ret = wlu_set(dhd, WLC_SET_VAR, &buf[0], (p - buf));

		if (ret) {
			fprintf(stderr, "%s: error %d on writing %d membytes at 0x%08x\n",
				"wl_set()", ret, len, start);
			goto exit;
		}

		start += len;
		buff += len;
		remained_bytes -= len;
	}
	printf("\n");

	/* start running the downloaded code, download complete */
	if ((ret = wlu_iovar_setint(dhd, "download", FALSE))) {
		fprintf(stderr, "dhd_hsic_download: failed to take dongle out of download mode\n");
		goto exit;
	}

exit:
	if (bufp)
		free(bufp);

	return ret;
}

static int ReadFiles(char *fname, char *vname, unsigned char ** buffer)
{
	FILE *fp = NULL;
	FILE *fp1 = NULL;
	struct stat finfo;
	uint8 *buf = NULL;
	int len, fwlen, actual_len, nvlen = 0;
	struct trx_header *hdr;
	unsigned long status;
	unsigned int pad;
	unsigned int i;
	int ret = -1;

	/* Open the firmware file */
	if (!(fp = fopen(fname, "rb"))) {
		perror(fname);
		ret = -1;
		goto exit;
	}

	if (stat(fname, &finfo)) {
		printf("ReadFiles: %s: %s\n", fname, strerror(errno));
		ret = -1;
		goto exit;
	}
	len = fwlen = finfo.st_size;

	/* Open nvram file */
	if (!(fp1 = fopen(vname, "rb"))) {
		perror(fname);
		ret = -1;
		goto exit;
	}

	if (stat(vname, &finfo)) {
		printf("ReadFiles: %s: %s\n", vname, strerror(errno));
		ret = -1;
		goto exit;
	}
	nvlen = finfo.st_size;
	len += nvlen;

	if ((buf = malloc(len +4)) == NULL) {
		printf("ReadFiles: Unable to allowcate %d bytes!\n", len);
		ret = -1;
		goto exit;
	}

	/* Read the firmware file into the buffer */
	status  = fread(buf, sizeof(uint8), fwlen, fp);

	/* close the firmware file */
	fclose(fp);

	if ((int)status < fwlen) {
		printf("ReadFiles: Short read in %s!\n", fname);
		ret = -1;
		goto exit;
	}

	/* Validating the format /length etc of the file */
	if ((actual_len = check_file(buf)) <= 0) {
		printf("ReadFiles: Failed input file check %s!\n", fname);
		ret = -1;
		goto exit;
	}

	/* Read the nvram file into the buffer */
	status  = fread(buf + actual_len, sizeof(uint8), nvlen, fp1);

	/* close the nvram file */
	fclose(fp1);

	if ((int)status < nvlen) {
		printf("ReadFiles: Short read in %s!\n", vname);
		ret = -1;
		goto exit;
	}

	/* porcess nvram vars if user specifics a text file instead of binary */
	nvlen = process_nvram_vars((char*) &buf[actual_len], (unsigned int) nvlen);

	if (nvlen % 4) {
		pad = 4 - (nvlen % 4);
		for (i = 0; i < pad; i ++)
			buf[actual_len + nvlen + i] = 0;
		nvlen += pad;
	}

	/* fix up len to be actual len + nvram len */
	len = actual_len + nvlen;
	/* update trx header with added nvram bytes */
	hdr = (struct trx_header *) buf;
	hdr->len = htol32(len);
	/* pass the actual fw len */
	hdr->offsets[TRX_OFFSETS_NVM_LEN_IDX] = htol32(nvlen);
	/* caculate CRC over header */
	hdr->crc32 = hndcrc32((uint8 *) &hdr->flag_version,
		SIZEOF_TRX(hdr) - OFFSETOF(struct trx_header, flag_version),
		CRC32_INIT_VALUE);

	/* Calculate CRC over data */
	for (i = SIZEOF_TRX(hdr); i < (unsigned int)len; ++i)
		hdr->crc32 = hndcrc32((uint8 *)&buf[i], 1, hdr->crc32);
	hdr->crc32 = htol32(hdr->crc32);

	*buffer  = buf;
	return len;

exit:
	if (buf)
		free(buf);

	return ret;
}

static int
check_file(unsigned char *headers)
{
	struct trx_header *trx;
	int actual_len = -1;

	/* Extract trx header */
	trx = (struct trx_header *)headers;
	if ((ltoh32(trx->magic)) != TRX_MAGIC) {
		printf("check_file: Error: trx bad hdr %x!\n", ltoh32(trx->magic));
		return -1;
	}

	headers += SIZEOF_TRX(trx);
	if (ltoh32(trx->flag_version) & TRX_UNCOMP_IMAGE) {
		actual_len = ltoh32(trx->offsets[TRX_OFFSETS_DLFWLEN_IDX]) +
			SIZEOF_TRX(trx);
		return actual_len;
	}
	return actual_len;
}
#endif /* SERDOWNLOAD */

static int
wl_spatial_policy(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr = NULL;
	int err, i, *reply;
	int mode[SPATIAL_MODE_MAX_IDX] = {-1, -1, -1, -1, -1};

	/* Order is 2G, 5G-LOW, 5G-MID, 5G-HIGH, 5G-UPPER
	 * if only one argument given, than all band or sub-band take the
	 * same value
	 */
	if (!*++argv) {
		bool all_same = TRUE;
		if ((err = wlu_var_getbuf(wl, cmd->name, &mode, sizeof(mode), &ptr)) < 0)
			return err;
		reply = (int *)ptr;
		for (i = 1; i < SPATIAL_MODE_MAX_IDX; i++) {
			 /* check if return values for each band/sub-band is same or not */
			 if (reply[i-1] != reply[i])
				 all_same = FALSE;
		}
		if (all_same)
			printf("%2d\n", reply[0]);
		else {
			printf("2.4GHz       : %2d\n", reply[SPATIAL_MODE_2G_IDX]);
			printf("5GHz (lower) : %2d\n", reply[SPATIAL_MODE_5G_LOW_IDX]);
			printf("5GHz (middle): %2d\n", reply[SPATIAL_MODE_5G_MID_IDX]);
			printf("5GHz (high)  : %2d\n", reply[SPATIAL_MODE_5G_HIGH_IDX]);
			printf("5GHz (upper) : %2d\n", reply[SPATIAL_MODE_5G_UPPER_IDX]);
		}
		return 0;
	}
	mode[0] = atoi(*argv);
	if (!*++argv) {
		for (i = 1; i < SPATIAL_MODE_MAX_IDX; i++)
			mode[i] = mode[0];
	} else {
		for (i = 1; i < SPATIAL_MODE_MAX_IDX; i++) {
			mode[i] = atoi(*argv);
			if (!*++argv && i < (SPATIAL_MODE_MAX_IDX - 1)) {
				printf("error: missing arguments\n");
				return -1;
			}
		}
	}
	err = wlu_var_setbuf(wl, cmd->name, &mode, sizeof(mode));
	return err;
}

static int
wl_ratetbl_ppr(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr = NULL;
	int err, i, *reply;
	int val[12];

	/* Order is 2G, 5G-LOW, 5G-MID, 5G-HIGH, 5G-UPPER
	 * if only one argument given, than all band or sub-band take the
	 * same value
	 */
	memset(&val, 0, sizeof(val));
	if (!*++argv) {
		if ((err = wlu_var_getbuf(wl, cmd->name, &val, sizeof(val), &ptr)) < 0)
			return err;
		reply = (int *)ptr;
		for (i = 0; i < 12; i++)
			printf("%s: %2d\n", (reply[i] & 0x80) ? "OFDM" : "CCK ", (reply[i] & 0x7f));
		return 0;
	}
	val[0] = atoi(*argv++);
	val[1] = atoi(*argv++);
	err = wlu_var_setbuf(wl, cmd->name, &val, sizeof(val));
	return err;
}

static int
wlu_mempool(void *wl, cmd_t *cmd, char **argv)
{
	void               *ptr;
	int                ret;
	int                i;
	wl_mempool_stats_t *stats;
	bcm_mp_stats_t     *mp_stats;


	UNUSED_PARAMETER(argv);

	if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return ret;

	stats = (wl_mempool_stats_t *) ptr;
	mp_stats = stats->s;

	printf("%-8s %8s %8s %8s %8s %8s\n", "Name", "SZ", "Max", "Curr", "HiWater", "Failed");
	for (i = 0; i < stats->num; i++) {
		printf("%-8s %8d %8d %8d %8d %8d\n", mp_stats->name, (int) mp_stats->objsz,
		       mp_stats->nobj, mp_stats->num_alloc, mp_stats->high_water,
		       mp_stats->failed_alloc);

		mp_stats++;
	}

	return (0);
}

static int
wl_ie(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr;
	int err;
	uchar *data;
	int bsscfg_idx = 0;
	int consumed = 0;
	int iecount;
	ie_setbuf_t *ie_setbuf;
	ie_getbuf_t param;
	uchar datalen, type, count, col;

	/* parse a bsscfg_idx option if present */
	if ((err = wl_cfg_option(argv + 1, argv[0], &bsscfg_idx, &consumed)) != 0)
		return err;
	if (consumed)
		argv = argv + consumed;
	else
		bsscfg_idx = -1;

	if (!*++argv) {
		fprintf(stderr, "missing parameter type\n");
		return -1;
	}
	/* get IE type */
	type = (uchar)atoi(argv[0]);

	if (!*++argv) {
		param.id = type;
		ptr = buf;
		if (bsscfg_idx == -1)
			err = wlu_var_getbuf(wl, cmd->name, &param, sizeof(param), &ptr);
		else
			err = wl_bssiovar_getbuf(wl, cmd->name, bsscfg_idx, &param, sizeof(param),
			buf, WLC_IOCTL_MAXLEN);
		if (err == 0) {
			data = (uchar *)ptr;
			datalen = data[1]+2;
			printf("%s len %d\n", cmd->name, datalen);
			printf("%s Data:\n", cmd->name);
			for (count = 0; (count < datalen);) {
				for (col = 0; (col < MAX_DATA_COLS) &&
					(count < datalen); col++, count++) {
					printf("%02x", *data++);
				}
				printf("\n");
			}
		}
		else {
			fprintf(stderr, "Error %d getting IOVar\n", err);
		}
		return err;
	}

	/* get IE length */
	datalen = (uchar)atoi(argv[0]);

	if (datalen > 0) {
		if (!argv[1]) {
			fprintf(stderr, "Data bytes should be specified for IE of length %d\n",
				datalen);
			return -1;
		}
		else {
			/* Ensure each data byte is 2 characters long */
			if ((int)strlen (argv[1]) < (datalen * 2)) {
				fprintf(stderr, "Please specify all the data bytes for this IE\n");
				return -1;
			}
		}
	}

	if ((datalen == 0) && (argv[1] != NULL))
		fprintf(stderr, "Ignoring data bytes for IE of length %d\n", datalen);

	count = sizeof(ie_setbuf_t) + datalen - 1;
	data = malloc(count);
	if (data == NULL) {
		fprintf(stderr, "memory alloc failure\n");
		return -1;
	}

	ie_setbuf = (ie_setbuf_t *) data;
	/* Copy the ie SET command ("add") to the buffer */
	strncpy(ie_setbuf->cmd, "add", VNDR_IE_CMD_LEN - 1);
	ie_setbuf->cmd[VNDR_IE_CMD_LEN - 1] = '\0';

	/* Buffer contains only 1 IE */
	iecount = htod32(1);
	memcpy((void *)&ie_setbuf->ie_buffer.iecount, &iecount, sizeof(int));

	/* Now, add the IE to the buffer */
	ie_setbuf->ie_buffer.ie_list[0].ie_data.id = type;
	ie_setbuf->ie_buffer.ie_list[0].ie_data.len = datalen;

	if (datalen > 0) {
		if ((err = get_ie_data ((uchar *)argv[1],
		          (uchar *)&ie_setbuf->ie_buffer.ie_list[0].ie_data.data[0],
		          datalen))) {
			free(data);
			fprintf(stderr, "Error parsing data arg\n");
			return err;
		}
	}

	if (bsscfg_idx == -1)
		err = wlu_var_setbuf(wl, cmd->name, data, count);
	else
		err = wlu_bssiovar_setbuf(wl, cmd->name, bsscfg_idx,
			data, count, buf, WLC_IOCTL_MAXLEN);

	free(data);
	return (err);
}

static int
wl_wnm_url(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr;
	int err;
	uchar *data;
	uchar datalen, count;
	wnm_url_t *url;

	if (!*++argv) {
		err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr);
		if (err == 0) {
			data = (uchar *)ptr;
			datalen = data[0];
			data ++;
			*(data + datalen) = '\0';
			printf("%s URL len %d\n", cmd->name, datalen);
			printf("%s URL: %s\n", cmd->name, data);
		}
		else {
			fprintf(stderr, "Error %d getting IOVar\n", err);
		}
		return err;
	}

	/* get URL length */
	datalen = (uchar)atoi(argv[0]);

	if (datalen > 0) {
		if (!argv[1]) {
			fprintf(stderr, "URL string should be specified for URL length %d\n",
				datalen);
			return -1;
		}
		else {
			if ((int)strlen (argv[1]) != datalen) {
				fprintf(stderr, "length is matching to string\n");
				return -1;
			}
		}
	}

	if ((datalen == 0) && (argv[1] != NULL))
		fprintf(stderr, "Ignoring data bytes for length %d\n", datalen);

	count = sizeof(wnm_url_t) + datalen - 1;
	data = malloc(count);
	if (data == NULL) {
		fprintf(stderr, "memory alloc failure\n");
		return -1;
	}

	url = (wnm_url_t *) data;
	url->len = datalen;
	if (datalen > 0) {
		memcpy(url->data, argv[1], datalen);
	}

	err = wlu_var_setbuf(wl, cmd->name, data, count);

	free(data);
	return (err);
}

/* Restore the ignored warnings status */
#endif /* !ATE_BUILD */

#ifdef WIN32
#pragma warning(pop)
#endif
