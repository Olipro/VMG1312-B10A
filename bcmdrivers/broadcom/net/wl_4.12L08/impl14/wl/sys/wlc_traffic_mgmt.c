/*
 * Common (OS-independent) portion of
 * Broadcom traffic management support. A lot of the design was based
 * on information found at http://phix.me/dm.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_traffic_mgmt.c 355890 2012-09-10 08:57:40Z $
 */

#include <epivers.h>
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <proto/ethernet.h>
#include <proto/802.1d.h>
#include <proto/802.3.h>
#include <proto/802.11.h>
#include <proto/bcmip.h>
#include <proto/bcmudp.h>
#include <proto/bcmtcp.h>
#include <proto/vlan.h>
#include <bcmendian.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wl_export.h>

#include <wlc_traffic_mgmt.h>

/* Traffic management defines */
#define	TRF_MGMT_SAMPLING_PERIOD	100				/* In units of ms */
#define TRF_MGMT_MAX_SAMPLING_PERIODS	(1000 / TRF_MGMT_SAMPLING_PERIOD)

#define MAX_RX_PACKET_LEN		(DOT11_MAC_HDR_LEN +		\
					 DOT11_LLC_SNAP_HDR_LEN +	\
					 ETHER_MAX_DATA)

#define MAX_TX_PACKET_LEN		(ETHER_HDR_LEN +		\
					 DOT11_LLC_SNAP_HDR_LEN +	\
					 ETHER_MAX_DATA)

#define MAX_TX_QUEUE_LEN		128	/* Max queue depth of tx queues */
#define MAX_RX_QUEUE_LEN		128  	/* Max queue depth of rx queues */

#define MAX_TX_BANDWIDTH_ADJUST         15      /* Percent adjustment for bandwidth utilization. */
#define MAX_RX_BANDWIDTH_ADJUST         15

#define MIN_BANDWIDTH_ADJUST            10     /* Min. adjusted BW percentage */

#define MIN_SUPPORTED_BANDWIDTH         16384   /* Min bandwidth in bytes/sec (128 kbps) */

#ifdef OMIT
#define CHECK_USAGE(global_info)        if (global_info->total_bytes_consumed_per_second > \
					    global_info->maximum_bytes_per_second)       \
						_asm int 3
#else
#define CHECK_USAGE(global_info)
#endif

/* Packet descriptor that is initialized by the parsing engine */
typedef struct wlc_pkt_desc {
    trf_mgmt_filter_t   filter;
    struct vlan_header	*vlan_hdr;
    struct ipv4_hdr     *ip_hdr;
    struct bcmudp_hdr   *udp_hdr;
    struct bcmtcp_hdr   *tcp_hdr;
} wlc_pkt_desc_t;


/*
 * Supported IP DSCP values:
 * These are mapped to trf_mgmt_priority_class values.
 */
enum trf_mgmt_dscp_values {
    trf_mgmt_dscp_bk    = 0x0A,  /* IEEE 802.1p background */
    trf_mgmt_dscp_be    = 0x00,  /* IEEE 802.1p best effort */
    trf_mgmt_dscp_vi    = 0x1C   /* IEEE 802.1p video */
};

const uint32 trf_mgmt_dscp_tbl[] = {
    trf_mgmt_dscp_bk,           /* trf_mgmt_priority_low */
    trf_mgmt_dscp_be,           /* trf_mgmt_priority_medium */
    trf_mgmt_dscp_vi            /* trf_mgmt_priority_high */
 };


/*
 * Supported IEEE 802.1p PCP values:
 * These are mapped to trf_mgmt_priority_class enum values.
 * The traffic management engine classifies packets based on 802.1p values.
 */
const uint32 trf_mgmt_pcp_tbl[] = {
    PRIO_8021D_BK,  /* trf_mgmt_priority_low */
    PRIO_8021D_BE,  /* trf_mgmt_priority_medium */
    PRIO_8021D_VI   /* trf_mgmt_priority_high */
};


const uint32 trf_mgmt_default_guaranteed_bandwidth[TRF_MGMT_MAX_PRIORITIES] = {
    10,	/* minimum bandwidth percentage for trf_mgmt_priority_low */
    20,	/* minimum bandwidth percentage for trf_mgmt_priority_medium */
    70	/* minimum bandwidth percentage for trf_mgmt_priority_high */
};

/*
 * Packet queue structures and defines:
 * These are used to queue packets based on priority class. This is also used to
 * shape the traffic for a  priority class. Traffic shaping is based loosely on a token bucket
 * algorithm, where each token is a byte.
 */
typedef struct trf_mgmt_queue_sample {
    uint32	bytes_produced;
    uint32	bytes_consumed;
} trf_mgmt_queue_sample_t;

typedef struct trf_mgmt_queue {
    uint32                  priority;           /* Priority assigned to this queue. */
    bool                    is_rx_queue;        /* TRUE iff rx queue. */
    trf_mgmt_shaping_info_t info;               /* Shaping parameters. */
    trf_mgmt_stats_t        stats;              /* Per-queue packet/byte counters */
    trf_mgmt_queue_sample_t sampled_data[TRF_MGMT_MAX_SAMPLING_PERIODS];
    struct spktq            pkt_queue;
} trf_mgmt_queue_t;


/*
 * Filter hash table (FHT) structures and defines.
 */
#define FHT_MAX_HASH_BUCKETS            32   /* Maximum number of buckets in the hash table. */
#define FHT_MAX_HASH_HEAP_ELEMENTS      64   /* Maximum number of elements per heap row. */

/*
 * We use a hash key that combines the filter's protocol and its SPORT and DPORT
 * or mac address
 * TODO: Use L3 data in hash key.
 */
#define HASH_MAC_KEY(id) ((id[3] ^ id[4] ^ id[5]) % (FHT_MAX_HASH_BUCKETS))

#define HASH_KEY(filter)	((filter->flags & TRF_FILTER_MAC_ADDR) \
			? HASH_MAC_KEY(filter->dst_ether_addr.octet) \
			: (filter->prot +     \
				  filter->dst_port + \
				  filter->src_port) % FHT_MAX_HASH_BUCKETS)

/* Forward declarations */
typedef struct hash_element_desc    hash_element_desc_t;
typedef struct hash_heap_desc       hash_heap_desc_t;
typedef struct hash_bucket_desc     hash_bucket_desc_t;

/* Filter hash element descriptor */
struct hash_element_desc {
    hash_element_desc_t      *next_hash_element;     /* Pointer to the next hash element */
    hash_element_desc_t      *prev_hash_element;     /* Pointer to the previous hash element */
    hash_bucket_desc_t       *hash_bucket;           /* Pointer to the hash bucket */
    hash_heap_desc_t         *heap_desc;             /* Heap row element was allocated from */
    trf_mgmt_filter_t        filter;                 /* Filter data. */
};

/*
 * Filter hash heap descriptor:
 * Elements are allocated from the heap in order to be added to a bucket.
 */
struct hash_heap_desc {
    hash_heap_desc_t        *next_heap_desc;        /* Next heap descriptor in chain */
    hash_element_desc_t     *heap_mem;
    uint32                  num_free_elements;      /* Number of free elements in this heap row */
    hash_element_desc_t     *first_free_element;    /* Pointer to first free element */
};

/* Filter hash bucket descriptor */
struct hash_bucket_desc {
    uint32                  num_bucket_elements;    /* Number of hash elements in this bucket */
    hash_element_desc_t     *first_bucket_element;  /* Pointer to first element in this bucket */
};

#ifdef TRAFFIC_MGMT_RSSI_POLICY
/* RSSI based trafic management information */
typedef struct {
	struct scb		*rssi_scb;	/* The SCB to up- or downgrade. */
	int			rssi_policy;	/* Selected RSSI policy */
	bool			rssi_on;	/* Is RSSI update turned on ? */
} trf_mgmt_rssi_info_t;

# if defined(BCMDBG)
/* Internal debugging */
#define WL_RSSI(s) WL_TRF_MGMT(s)
#else
#define WL_RSSI(s)
#endif /* BCMDBG */

#endif /* TRAFFIC_MGMT_RSSI_POLICY */
/*
 * Traffic management private info structure
 */
struct wlc_trf_mgmt_ctxt {
    wlc_info_t              *wlc;
#ifdef TRAFFIC_MGMT_RSSI_POLICY
	trf_mgmt_rssi_info_t    rssi_info;
#endif /* TRAFFIC_MGMT_RSSI_POLICY */
    bool                    up_dn_registered;
};

struct wlc_trf_mgmt_info {
    wlc_info_t              *wlc;
    wlc_trf_mgmt_ctxt_t     *trf_mgmt_ctxt;
    wlc_bsscfg_t            *bsscfg;
    trf_mgmt_config_t       config;				/* Config settings */
    hash_bucket_desc_t      filter_bucket[FHT_MAX_HASH_BUCKETS];
    hash_bucket_desc_t      wildcard_bucket;
    uint32                  sizeof_hash_heap_desc;
    uint32                  num_hash_heap_desc;
    hash_heap_desc_t        *hash_heap_desc_list;
    trf_mgmt_queue_t        tx_queue[TRF_MGMT_MAX_PRIORITIES];	/* Queue of outbound packets */
    trf_mgmt_queue_t        rx_queue[TRF_MGMT_MAX_PRIORITIES];	/* Queue of inbound packets */
#ifdef TRAFFIC_SHAPING
    uint32		    uplink_bps;				/* Max uplink bytes/second */
    uint32		    downlink_bps;			/* Max downlink bytes/second */
    bool                    bandwidth_settings_adjusted;        /* TRUE if settings were changed */
    trf_mgmt_global_info_t  tx_global_shaping_info;             /* Global info for tx queues */
    trf_mgmt_global_info_t  rx_global_shaping_info;             /* Global info for tx queues */
    struct wl_timer	    *sampling_timer;			/* Periodic timer */
    uint32                  sample_index;                       /* Index into sample arrays */
#endif  /* TRAFFIC_SHAPING */
};

/* Net management flags macros */
#define TRF_MGMT_FLAGS(trf_mgmt_info, _flags)	((trf_mgmt_info)->config.flags & (_flags))

/* wlc access macros */
#define WLCUNIT(trf_mgmt_info)	    ((trf_mgmt_info)->wlc->pub->unit)
#define WLCPUB(trf_mgmt_info)	    ((trf_mgmt_info)->wlc->pub)
#define WLCOSH(trf_mgmt_info)	    ((trf_mgmt_info)->wlc->osh)
#define WLCCFG(trf_mgmt_info)	    ((trf_mgmt_info)->wlc->cfg)
#define WLCWLCIF(trf_mgmt_info)	    ((trf_mgmt_info)->wlc->cfg->wlcif)
#define WLCWL(trf_mgmt_info)	    ((trf_mgmt_info)->wlc->wl)


/* RFC1024 llc/snap header without type field */
static const uint8 rfc1042_snap_hdr[] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};


/* IOVar table */
enum {
    IOV_TRF_MGMT_CONFIG,            /* Get/set traffic management parameters */
    IOV_TRF_MGMT_FILTERS_ADD,       /* Adds one or more traffic management filters. */
    IOV_TRF_MGMT_FILTERS_REMOVE,    /* Removes one or more traffic management filters. */
    IOV_TRF_MGMT_FILTERS_LIST,      /* Lists all current traffic management filters. */
    IOV_TRF_MGMT_FILTERS_CLEAR,     /* Clear traffic management filters. */
    IOV_TRF_MGMT_FLAGS,		    /* Get/set traffic management operational flags */
    IOV_TRF_MGMT_STATS,             /* Get traffic management statistics. */
    IOV_TRF_MGMT_STATS_CLEAR,       /* Clear traffic management statistics. */
#ifdef TRAFFIC_MGMT
    IOV_TRF_MGMT_BANDWIDTH,         /* Get/set traffic management bandwidth parameters */
    IOV_TRF_MGMT_SHAPING_INFO,      /* Get the shaping parameters. */
#endif
#ifdef TRAFFIC_MGMT_RSSI_POLICY
    IOV_TRF_MGMT_RSSI_POLICY,	    /* Get/set traffic management RSSI policy */
#endif /* TRAFFIC_MGMT_RSSI_POLICY */
};

#ifdef TRAFFIC_MGMT_RSSI_POLICY
/*
 * Various traffic management policies. Limited to DOWNGRADE_LOWEST on request.
 */
enum {
	TRF_MGMT_RSSI_NONE = 0,			/* default value is zero */
	TRF_MGMT_RSSI_DOWNGRADE_LOWEST,		/* Most desired behaviour */
	/* range checking enum, sets last valid value */
	TRF_MGMT_RSSI_MAX = TRF_MGMT_RSSI_DOWNGRADE_LOWEST
};
#endif /* TRAFFIC_MGMT_RSSI_POLICY */

static const bcm_iovar_t trf_mgmt_iovars[] = {
	{"trf_mgmt_config",
	IOV_TRF_MGMT_CONFIG,
	(0),
	IOVT_BUFFER,
	sizeof(trf_mgmt_config_t)
	},

	{"trf_mgmt_filters_add",
	IOV_TRF_MGMT_FILTERS_ADD,
	(0),
	IOVT_BUFFER,
	sizeof(trf_mgmt_filter_list_t)
	},

	{"trf_mgmt_filters_remove",
	IOV_TRF_MGMT_FILTERS_REMOVE,
	(0),
	IOVT_BUFFER,
	sizeof(trf_mgmt_filter_list_t)
	},

	{"trf_mgmt_filters_list",
	IOV_TRF_MGMT_FILTERS_LIST,
	(0),
	IOVT_BUFFER,
	sizeof(trf_mgmt_filter_list_t)
	},

	{"trf_mgmt_filters_clear",
	IOV_TRF_MGMT_FILTERS_CLEAR,
	(0),
	IOVT_VOID,
	0
	},

	{"trf_mgmt_flags",
	IOV_TRF_MGMT_FLAGS,
	(0),
	IOVT_BUFFER,
	sizeof(uint32)
	},

	{"trf_mgmt_stats",
	IOV_TRF_MGMT_STATS,
	(0),
	IOVT_BUFFER,
	sizeof(trf_mgmt_stats_array_t)
	},

	{"trf_mgmt_stats_clear",
	IOV_TRF_MGMT_STATS_CLEAR,
	(0),
	IOVT_VOID,
	0
	},

#ifdef TRAFFIC_SHAPING
	{"trf_mgmt_bandwidth",
	IOV_TRF_MGMT_BANDWIDTH,
	(0),
	IOVT_BUFFER,
	sizeof(trf_mgmt_config_t)
	},

	{"trf_mgmt_shaping_info",
	IOV_TRF_MGMT_SHAPING_INFO,
	(0),
	IOVT_BUFFER,
	sizeof(trf_mgmt_shaping_info_array_t)
	},
#endif
#ifdef TRAFFIC_MGMT_RSSI_POLICY
	{"trf_mgmt_rssi_policy",
	IOV_TRF_MGMT_RSSI_POLICY,
	(0),
	IOVT_UINT32,
	0
	},
#endif /* TRAFFIC_MGMT_RSSI_POLICY */

	{NULL, 0, 0, 0, 0 }
};


/* Forward declarations for functions registered for this module */
static int wlc_trf_mgmt_doiovar(
		    void                *hdl,
		    const bcm_iovar_t   *vi,
		    uint32              actionid,
		    const char          *name,
		    void                *p,
		    uint                plen,
		    void                *a,
		    int                 alen,
		    int                 vsize,
		    struct wlc_if       *wlcif);

static int wlc_trf_mgmt_watchdog(void *cntxt);

static int wlc_trf_mgmt_interface_up(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt);
static int wlc_trf_mgmt_interface_down(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt);

/* Forward declarations for wlc_trf_mgmt_ctxt_t-based functions */
static int wlc_trf_mgmt_dump_config(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt, struct bcmstrbuf *b);
static int wlc_trf_mgmt_dump_stats(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt, struct bcmstrbuf *b);

static wlc_trf_mgmt_info_t * wlc_trf_mgmt_allocate_info_block(
				wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt,
				wlc_bsscfg_t *bsscfg);

static int wlc_trf_mgmt_configure(wlc_trf_mgmt_info_t *trf_mgmt_info, void *a);
static int wlc_trf_mgmt_flags(wlc_trf_mgmt_info_t *trf_mgmt_info, void *a);

static void wlc_trf_mgmt_bsscfg_updn_handler(void *ctx, bsscfg_up_down_event_data_t *evt);
static void wlc_trf_mgmt_bsscfg_up(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt, wlc_bsscfg_t *bsscfg);
static void wlc_trf_mgmt_bsscfg_down(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt, wlc_bsscfg_t *bsscfg);

static void BCMFASTPATH wlc_trf_mgmt_enq(void *context, struct scb *scb, void *pkt, uint prec);
static uint wlc_trf_mgmt_txpktcnt(void *context);
static void wlc_trf_mgmt_activate(void *context, struct scb *scb);
static void wlc_trf_mgmt_deactivate(void *context, struct scb *scb);

static txmod_fns_t trf_mgmt_txmod_fns = {
	wlc_trf_mgmt_enq,
	wlc_trf_mgmt_txpktcnt,
	wlc_trf_mgmt_deactivate,
	wlc_trf_mgmt_activate
};


/* Forward declarations for wlc_trf_mgmt_info_t-based functions */
static void wlc_trf_mgmt_free_info_block(wlc_trf_mgmt_info_t *trf_mgmt_info);
static void wlc_trf_mgmt_clear_data(wlc_trf_mgmt_info_t *trf_mgmt_info);

static int wlc_trf_mgmt_parse_pkt(
		    wlc_trf_mgmt_info_t *trf_mgmt_info,
		    void                *pkt,
		    bool                in_tx_path,
		    wlc_pkt_desc_t      *pkt_desc);

static int wlc_trf_mgmt_filter_pkt(
		    wlc_trf_mgmt_info_t	*trf_mgmt_info,
		    void		*pkt,
		    bool		in_tx_path,
		    wlc_pkt_desc_t	*pkt_desc);

static uint16 wlc_trf_mgmt_ip_checksum(wlc_trf_mgmt_info_t *trf_mgmt_info, struct ipv4_hdr *ip_hdr);

#ifdef TRAFFIC_SHAPING
static int32 wlc_trf_mgmt_process_pkt(wlc_trf_mgmt_info_t *trf_mgmt_info, trf_mgmt_queue_t *queue);
#endif

static int wlc_trf_mgmt_filter_add(
		    wlc_trf_mgmt_info_t     *trf_mgmt_info,
		    trf_mgmt_filter_list_t  *trf_mgmt_filter_list,
		    uint32                  cmd_length);

static int wlc_trf_mgmt_filter_remove(
		    wlc_trf_mgmt_info_t     *trf_mgmt_info,
		    trf_mgmt_filter_list_t  *trf_mgmt_filter_list,
		    uint32                  cmd_length);

static int wlc_trf_mgmt_filter_list(
		    wlc_trf_mgmt_info_t     *trf_mgmt_info,
		    trf_mgmt_filter_list_t  *trf_mgmt_filter_list,
		    uint32                  cmd_length);

static int wlc_trf_mgmt_hash_allocate(wlc_trf_mgmt_info_t *trf_mgmt_info);

static void wlc_trf_mgmt_hash_free(wlc_trf_mgmt_info_t *trf_mgmt_info);

static hash_element_desc_t *wlc_trf_mgmt_hash_add(
				wlc_trf_mgmt_info_t *trf_mgmt_info,
				trf_mgmt_filter_t   *filter);

static hash_element_desc_t *wlc_trf_mgmt_hash_find(
				wlc_trf_mgmt_info_t *trf_mgmt_info,
				trf_mgmt_filter_t   *filter);

static hash_element_desc_t *wlc_trf_mgmt_hash_wldcard_find(
				wlc_trf_mgmt_info_t *trf_mgmt_info,
				trf_mgmt_filter_t   *filter);

static void wlc_trf_mgmt_hash_remove(
				wlc_trf_mgmt_info_t *trf_mgmt_info,
				hash_element_desc_t *phash_element);

static bool wlc_trf_mgmt_is_wildcard_filter(
				wlc_trf_mgmt_info_t *trf_mgmt_info,
				trf_mgmt_filter_t   *filter);


#ifdef TRAFFIC_SHAPING
/* Forward declarations for traffic shaping code */
static int wlc_trf_mgmt_dump_shaping_info(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt, struct bcmstrbuf *b);

static void wlc_trf_mgmt_sampling_timer(wlc_trf_mgmt_info_t *trf_mgmtif);
static void wlc_trf_mgmt_set_bandwidth_usage_parameters(
		    wlc_trf_mgmt_info_t *trf_mgmt_info,
		    trf_mgmt_queue_t *queue);

static void wlc_trf_mgmt_set_shaping_parameters(wlc_trf_mgmt_info_t *trf_mgmt_info);

static void wlc_trf_mgmt_adjust_bandwidth_usage_parameters(
	            wlc_trf_mgmt_info_t *trf_mgmt_info,
	            bool                restore_configured_parameters);

static void wlc_trf_mgmt_update_shaping_parameters(
		    wlc_trf_mgmt_info_t *trf_mgmt_info,
		    trf_mgmt_queue_t *queue);

static void wlc_trf_mgmt_handle_full_queue(
		    wlc_trf_mgmt_info_t *trf_mgmt_info,
		    trf_mgmt_queue_t    *queue);

static void wlc_trf_mgmt_flush_tx_queue(wlc_trf_mgmt_info_t *trf_mgmt_info, bool free_pkts);

static void wlc_trf_mgmt_flush_rx_queue(wlc_trf_mgmt_info_t *trf_mgmt_info, bool free_pkts);

static void wlc_trf_mgmt_process_queue(
				wlc_trf_mgmt_info_t *trf_mgmt_info,
				trf_mgmt_queue_t    *queue);

static void wlc_trf_mgmt_process_all_queues(
				wlc_trf_mgmt_info_t *trf_mgmt_info,
				trf_mgmt_queue_t    *queue_array);

static void wlc_trf_mgmt_process_queue_unused_bytes(
				wlc_trf_mgmt_info_t *trf_mgmt_info,
				trf_mgmt_queue_t    *queue_array);
#endif  /* TRAFFIC_SHAPING */

#ifdef TRAFFIC_MGMT_RSSI_POLICY
/* Return whether RSSI needs to be collected or not. */
static inline bool
trf_mgmt_rssi_needed(wlc_trf_mgmt_ctxt_t *tm)
{
	return (tm) ? (AP_ENAB(tm->wlc->pub) && (tm->rssi_info.rssi_policy != TRF_MGMT_RSSI_NONE))
		: FALSE;
}

/* Return true if traffic management is enabled on this scb. */
static bool
trf_mgmt_is_enabled(struct scb *scb)
{
	wlc_bsscfg_t *bss;

	bss = SCB_BSSCFG(scb);

	return ((bss) && (bss->trf_mgmt_info) && (bss->trf_mgmt_info->config.trf_mgmt_enabled));
}

/* When an SCB is created, turn on RSSI collection if needed. */
static int
trf_mgmt_scbcubby_init(void *handle, struct scb *scb)
{
	wlc_trf_mgmt_ctxt_t *tm = handle;

	if (trf_mgmt_rssi_needed(tm)) {

		WL_RSSI(("%s: Enabling RSSI collection for scb %p\n", __FUNCTION__, scb));

		wlc_scb_rssi_update_enable(scb, TRUE, RSSI_UPDATE_FOR_TM);
		/* Hook in the txmod if not yet done by tm. */
		if (!trf_mgmt_is_enabled(scb)) {
			wlc_txmod_config(tm->wlc, scb, TXMOD_TRF_MGMT);
		}
	}
	return BCME_OK;
}

static void
trf_mgmt_scbcubby_exit(void *handle, struct scb *scb)
{
	BCM_REFERENCE(handle);
	BCM_REFERENCE(scb);
}

/* Set the RSSI policy value, and take action (enable rssi, configure txmod) if needed. */
static int
trf_mgmt_set_rssi_policy(wlc_trf_mgmt_ctxt_t *tm, uint32 policy)
{
	struct scb *scb;
	struct scb_iter scbiter;

	/* RSSI policy works only on the AP side */
	if (!AP_ENAB(tm->wlc->pub))
		return BCME_NOTAP;

	if (policy > TRF_MGMT_RSSI_MAX)
		return BCME_BADARG;

	/* If policy has not changed, no need to take any action. */
	if (tm->rssi_info.rssi_policy == policy)
		return BCME_OK;

	/* Turn on or off RSSI collection for all SCBs as needed */

	if (policy == TRF_MGMT_RSSI_NONE) {

		/* RSSI based policy is being disabled. */

		if (tm->rssi_info.rssi_on) {
			/* Currently turned on, turn it off. */
			FOREACHSCB(tm->wlc->scbstate, &scbiter, scb) {

				WL_RSSI(("%s: Disabling RSSI collection for scb %p\n",
					__FUNCTION__, scb));

				wlc_scb_rssi_update_enable(scb, FALSE, RSSI_UPDATE_FOR_TM);

				/* If trf_mgmt_config is off, remove the txmod, else leave it. */
				if (!trf_mgmt_is_enabled(scb)) {
					WL_RSSI(("%s: TM is disabled, unconfig TXMOD for scb %p\n",
					__FUNCTION__, scb));
					wlc_txmod_unconfig(tm->wlc, scb, TXMOD_TRF_MGMT);
				}
			}
			tm->rssi_info.rssi_on = FALSE;
		}
	} else {

		/* RSSI based policy is being enabled. */

		if (!tm->rssi_info.rssi_on) {
			/* Currently turned off, turn it on. */
			FOREACHSCB(tm->wlc->scbstate, &scbiter, scb) {

				WL_RSSI(("%s: Enabling RSSI collection for scb %p\n",
					__FUNCTION__, scb));

				wlc_scb_rssi_update_enable(scb, TRUE, RSSI_UPDATE_FOR_TM);

				/* Hook in the txmod if not yet done by tm. */
				if (!trf_mgmt_is_enabled(scb)) {
					wlc_txmod_config(tm->wlc, scb, TXMOD_TRF_MGMT);
				}
			}
			tm->rssi_info.rssi_on = TRUE;
		}
	}

	tm->rssi_info.rssi_policy = policy;

	WL_RSSI(("%s: trf_mgmt_rssi_policy set to %d, RSSI collection %s\n",
		__FUNCTION__, policy, (tm->rssi_info.rssi_on) ? "ON":"OFF"));

	return BCME_OK;
}

/* Watchdog function for RSSI-policy based processing. Called from the TM watchdog function. */
static void
trf_mgmt_rssi_watchdog(wlc_trf_mgmt_ctxt_t *tm)
{
	if (trf_mgmt_rssi_needed(tm)) {
		struct scb *selected_scb = NULL;
		struct scb *scb;
		struct scb_iter scbiter;
		int rssi_worst = -1;
		int rssi;
		int scb_count = 0;

		/* Figure out the station to up- or downgrade. */
		FOREACHSCB(tm->wlc->scbstate, &scbiter, scb) {

			rssi = wlc_scb_rssi(scb);

			if ((rssi_worst > rssi) && (rssi > -100)) {
			    rssi_worst = rssi;
			    /*
			     * The station we selected could be behind a proxy sta, if so,
			     * select the owning psta. This will cause the rssi policy to
			     * apply to all stations behind that psta.
			     */
			    selected_scb = (scb->psta_prim) ? scb->psta_prim : scb;
			}

			scb_count++;
		}

		/* If only one SCB is associated, we do not take any action. */
		if (scb_count <= 1)
			selected_scb = NULL;

		tm->rssi_info.rssi_scb = selected_scb;

	}
}

/* Do the actual RSSI-policy based processing of a packet. */
static void
trf_mgmt_do_rssi_processing(wlc_trf_mgmt_ctxt_t *tm, struct scb *scb, void *pkt)
{
	/*
	 * Do RSSI based priority adjustment, taking into account that the scb selected
	 * for processing may be sitting behind a proxy sta.
	 */
	if (trf_mgmt_rssi_needed(tm) &&
	    (tm->rssi_info.rssi_scb == ((scb->psta_prim) ? scb->psta_prim : scb))) {

		switch (tm->rssi_info.rssi_policy) {

		case TRF_MGMT_RSSI_DOWNGRADE_LOWEST:
			/* downgrade to BE */
			PKTSETPRIO(pkt, PRIO_8021D_BE);
			WL_RSSI(("%s: Packet for scb %p/%p (RSSI %d) downgraded to BE\n",
				__FUNCTION__, scb, scb->psta_prim, wlc_scb_rssi(scb)));
			break;
		}
	}
}

# define TRF_MGMT_DO_RSSI_PROCESSING(c, s, p) trf_mgmt_do_rssi_processing((c), (s), (p))

#else /* TRAFFIC_MGMT_RSSI_POLICY */

#define TRF_MGMT_DO_RSSI_PROCESSING(c, s, p) /* nop */

#endif /* TRAFFIC_MGMT_RSSI_POLICY */

/*
 * Registered Functions:
 * These functions are exported through registration calls for this module.
 */
/*
 * Initialize the traffic management private context and resources.
 * Returns a pointer to the traffic management private context, NULL on failure.
 */
wlc_trf_mgmt_ctxt_t *
BCMATTACHFN(wlc_trf_mgmt_attach)(wlc_info_t *wlc)
{
	wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt;

	/* allocate traffic management private info struct */
	trf_mgmt_ctxt = MALLOC(wlc->osh, sizeof(wlc_trf_mgmt_ctxt_t));
	if (!trf_mgmt_ctxt) {
	    WL_ERROR(("wl%d: %s: MALLOC failed; total mallocs %d bytes\n",
	        WLCUNIT(trf_mgmt_ctxt),
	        __FUNCTION__,
	        MALLOCED(wlc->osh)));

	    return NULL;
	}

	/* init traffic management context struct */
	bzero(trf_mgmt_ctxt, sizeof(wlc_trf_mgmt_ctxt_t));

	trf_mgmt_ctxt->wlc = wlc;

	if (wlc_bsscfg_updown_register(
		wlc,
		wlc_trf_mgmt_bsscfg_updn_handler,
		trf_mgmt_ctxt) != BCME_OK) {
	    WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_register() failed\n",
	        wlc->pub->unit,
	        __FUNCTION__));

	    goto fail;
	}

	trf_mgmt_ctxt->up_dn_registered = TRUE;

	/* register module */
	if (wlc_module_register(
			    wlc->pub,
			    trf_mgmt_iovars,
			    "trf_mgmt",
			    trf_mgmt_ctxt,
			    wlc_trf_mgmt_doiovar,
			    wlc_trf_mgmt_watchdog,
			    (up_fn_t) wlc_trf_mgmt_interface_up,
			    (down_fn_t)wlc_trf_mgmt_interface_down)) {
				WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
				    WLCUNIT(trf_mgmt_ctxt),
				    __FUNCTION__));

				goto fail;
			    }
#ifdef TRAFFIC_MGMT_RSSI_POLICY
	if (wlc_scb_cubby_reserve(wlc,
				  0,       /* no additional data needed for now */
				  trf_mgmt_scbcubby_init,
				  trf_mgmt_scbcubby_exit,
				  NULL,    /* no data, so nothing to dump */
				  trf_mgmt_ctxt) < 0) {

				WL_ERROR(("wl%d: %s wlc_scb_cubby_reserve() failed\n",
					WLCUNIT(trf_mgmt_ctxt),
					__FUNCTION__));

		wlc_module_unregister(wlc->pub, "trf_mgmt", trf_mgmt_ctxt);

		goto fail;
	}
#endif /* TRAFFIC_MGMT_RSSI_POLICY */

	/* register the txmod functions. Do this after we register the module. */
	wlc_txmod_fn_register(wlc, TXMOD_TRF_MGMT, trf_mgmt_ctxt, trf_mgmt_txmod_fns);

	/* register info dump functions for the traffic management module. */
	wlc_dump_register(
	    wlc->pub,
	    "trf_mgmt_config",
	    (dump_fn_t)wlc_trf_mgmt_dump_config,
	    (void *)trf_mgmt_ctxt);

	wlc_dump_register(
	    wlc->pub,
	    "trf_mgmt_stats",
	    (dump_fn_t)wlc_trf_mgmt_dump_stats,
	    (void *)trf_mgmt_ctxt);

#ifdef TRAFFIC_SHAPING
	wlc_dump_register(
	    wlc->pub,
	    "trf_mgmt_shaping_info",
	    (dump_fn_t)wlc_trf_mgmt_dump_shaping_info,
	    (void *)trf_mgmt_ctxt);
#endif

	/* Enable the module. */
	TRAFFIC_MGMT_ENAB(WLCPUB(trf_mgmt_ctxt)) = TRUE;

	return trf_mgmt_ctxt;

fail:
	/* Unregister the up-dn callback if it was registered. */
	if (trf_mgmt_ctxt->up_dn_registered) {
		wlc_bsscfg_updown_unregister(trf_mgmt_ctxt->wlc,
			wlc_trf_mgmt_bsscfg_updn_handler,
			trf_mgmt_ctxt);
	}

	MFREE(WLCOSH(trf_mgmt_ctxt), trf_mgmt_ctxt, sizeof(wlc_trf_mgmt_ctxt_t));

	return NULL;
}

/*
 * Release traffic management private context and resources.
 */
void
BCMATTACHFN(wlc_trf_mgmt_detach)(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt)
{
	wlc_bsscfg_t    *bsscfg;
	uint32          i;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_detach()\n", WLCUNIT(trf_mgmt_ctxt)));

	if (!trf_mgmt_ctxt)
	    return;

	/* Release any trf. mgmt resources for each bsscfg. */
	FOREACH_BSS(trf_mgmt_ctxt->wlc, i, bsscfg) {
	    if (bsscfg->trf_mgmt_info != NULL) {
		wlc_trf_mgmt_free_info_block(bsscfg->trf_mgmt_info);

		bsscfg->trf_mgmt_info = NULL;
	    }
	}

	/* Disable the module. */
	TRAFFIC_MGMT_ENAB(WLCPUB(trf_mgmt_ctxt)) = FALSE;

	/* Unregister the up-dn callback. */
	if (trf_mgmt_ctxt->up_dn_registered) {
	    wlc_bsscfg_updown_unregister(
		trf_mgmt_ctxt->wlc,
		wlc_trf_mgmt_bsscfg_updn_handler,
		trf_mgmt_ctxt);
	}

	/* Unregister the module */
	wlc_module_unregister(WLCPUB(trf_mgmt_ctxt), "trf_mgmt", trf_mgmt_ctxt);
	MFREE(WLCOSH(trf_mgmt_ctxt), trf_mgmt_ctxt, sizeof(wlc_trf_mgmt_ctxt_t));
}

/*
 * Handle traffic management-related iovars
 */
static int
BCMATTACHFN(wlc_trf_mgmt_doiovar)(
	void                *hdl,
	const bcm_iovar_t   *vi,
	uint32              actionid,
	const char          *name,
	void                *p,
	uint                plen,
	void                *a,
	int                 alen,
	int                 vsize,
	struct wlc_if       *wlcif)
{
	wlc_trf_mgmt_ctxt_t             *trf_mgmt_ctxt = hdl;
	wlc_trf_mgmt_info_t             *trf_mgmt_info;
	trf_mgmt_filter_list_t          *filter_list;
	trf_mgmt_stats_array_t          *stats_array;
	wlc_bsscfg_t                    *bsscfg;
	int                             err = 0;
	int                             i;
#ifdef TRAFFIC_SHAPING
	trf_mgmt_config_t               *config;
	trf_mgmt_shaping_info_array_t   *info_array;
#endif

	WL_TRF_MGMT(("wl%d: trf_mgmt_doiovar()\n", WLCUNIT(trf_mgmt_ctxt)));

	BCM_REFERENCE(vi);
	BCM_REFERENCE(name);
	BCM_REFERENCE(p);
	BCM_REFERENCE(plen);
	BCM_REFERENCE(vsize);

	/* Resolve the bsscfg for this request. */
	bsscfg = wlc_bsscfg_find_by_wlcif(trf_mgmt_ctxt->wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/*
	 * Resolve the pointer to the traffic management info. block.
	 */
	if (bsscfg->trf_mgmt_info == NULL) {
	    /* Allocate and initialize the info block for this bsscfg. */
	    trf_mgmt_info = wlc_trf_mgmt_allocate_info_block(trf_mgmt_ctxt, bsscfg);

	    if (trf_mgmt_info == NULL) {
		return BCME_ERROR;
	    }
	} else {
	    trf_mgmt_info = bsscfg->trf_mgmt_info;
	}

	switch (actionid) {
	    case IOV_SVAL(IOV_TRF_MGMT_CONFIG):
		if (BSSCFG_STA(bsscfg) && !bsscfg->associated) {
		    /*
		     * For the STA, don't allow configuration if we're not associated. This
		     * is because we can only get some settings (such as local IP subnet data)
		     * after an association is made.
		     */
		    return BCME_NOTUP;
		}

		return wlc_trf_mgmt_configure(trf_mgmt_info, a);

	    case IOV_GVAL(IOV_TRF_MGMT_CONFIG):
		if (alen < sizeof(trf_mgmt_config_t))
		    return BCME_BUFTOOSHORT;

		bcopy(&trf_mgmt_info->config, a, sizeof(trf_mgmt_config_t));

		break;

	    case IOV_SVAL(IOV_TRF_MGMT_FILTERS_ADD):
		if (alen < sizeof(trf_mgmt_filter_list_t))
		    return BCME_BUFTOOSHORT;

		filter_list = (trf_mgmt_filter_list_t *)a;

		return wlc_trf_mgmt_filter_add(trf_mgmt_info, filter_list, alen);

	    case IOV_SVAL(IOV_TRF_MGMT_FILTERS_REMOVE):
		if (alen < sizeof(trf_mgmt_filter_list_t))
		    return BCME_BUFTOOSHORT;

		filter_list = (trf_mgmt_filter_list_t *)a;

		return wlc_trf_mgmt_filter_remove(trf_mgmt_info, filter_list, alen);

	    case IOV_GVAL(IOV_TRF_MGMT_FILTERS_LIST):
		if (alen < sizeof(trf_mgmt_filter_list_t))
		    return BCME_BUFTOOSHORT;

		return wlc_trf_mgmt_filter_list(trf_mgmt_info, (trf_mgmt_filter_list_t *)a, alen);

	    case IOV_SVAL(IOV_TRF_MGMT_FILTERS_CLEAR):
		WL_TRF_MGMT(("wl%d: %s: Clearing filters\n",
		    WLCUNIT(trf_mgmt_info),
		    __FUNCTION__));

		wlc_trf_mgmt_hash_free(trf_mgmt_info);

		break;

	   case IOV_SVAL(IOV_TRF_MGMT_FLAGS):
		return wlc_trf_mgmt_flags(trf_mgmt_info, a);

	    case IOV_GVAL(IOV_TRF_MGMT_FLAGS):
		if (alen < sizeof(uint32))
		    return BCME_BUFTOOSHORT;

		*(uint32 *)a = trf_mgmt_info->config.flags;

		break;

	    case IOV_GVAL(IOV_TRF_MGMT_STATS):
		if (alen < sizeof(trf_mgmt_stats_array_t))
		    return BCME_BUFTOOSHORT;

		stats_array = (trf_mgmt_stats_array_t *)a;

		for (i = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
		    bcopy(
			&trf_mgmt_info->tx_queue[i].stats,
			&stats_array->tx_queue_stats[i],
			sizeof(trf_mgmt_stats_t));

		    bcopy(
			&trf_mgmt_info->rx_queue[i].stats,
			&stats_array->rx_queue_stats[i],
			sizeof(trf_mgmt_stats_t));
		}

		break;

	    case IOV_SVAL(IOV_TRF_MGMT_STATS_CLEAR):
		for (i = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
		    bzero(&trf_mgmt_info->tx_queue[i].stats, sizeof(trf_mgmt_stats_t));
		    bzero(&trf_mgmt_info->rx_queue[i].stats, sizeof(trf_mgmt_stats_t));
		}

		break;

#ifdef TRAFFIC_SHAPING
	    case IOV_SVAL(IOV_TRF_MGMT_BANDWIDTH):
		config = (trf_mgmt_config_t *)a;

		if (trf_mgmt_info->config.trf_mgmt_enabled == 0)
		    return BCME_ERROR;

		trf_mgmt_info->config.downlink_bandwidth = config->downlink_bandwidth;
		trf_mgmt_info->config.uplink_bandwidth   = config->uplink_bandwidth;

		for (i = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
		    trf_mgmt_info->config.min_tx_bandwidth[i] = config->min_tx_bandwidth[i];
		    trf_mgmt_info->config.min_rx_bandwidth[i] = config->min_rx_bandwidth[i];
		}

		if (!TRF_MGMT_FLAGS(trf_mgmt_info, TRF_MGMT_FLAG_DISABLE_SHAPING)) {
		    wlc_trf_mgmt_set_shaping_parameters(trf_mgmt_info);
		}

		break;

	    case IOV_GVAL(IOV_TRF_MGMT_BANDWIDTH):
		/* We support queries of just the bandwidth parameters. */
		if (alen < sizeof(trf_mgmt_config_t))
		    return BCME_BUFTOOSHORT;

		bcopy(&trf_mgmt_info->config, a, sizeof(trf_mgmt_config_t));

		break;

	    case IOV_GVAL(IOV_TRF_MGMT_SHAPING_INFO):
		if (alen < sizeof(trf_mgmt_shaping_info_array_t))
		    return BCME_BUFTOOSHORT;

		info_array = (trf_mgmt_shaping_info_array_t *)a;

		bcopy(
		    &trf_mgmt_info->tx_global_shaping_info,
		    &info_array->tx_global_shaping_info,
		    sizeof(trf_mgmt_global_info_t));

		bcopy(
		    &trf_mgmt_info->rx_global_shaping_info,
		    &info_array->rx_global_shaping_info,
		    sizeof(trf_mgmt_global_info_t));

		for (i = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
		    bcopy(
			&trf_mgmt_info->tx_queue[i].info,
			&info_array->tx_queue_shaping_info[i],
			sizeof(trf_mgmt_shaping_info_t));

		    bcopy(&trf_mgmt_info->rx_queue[i].info,
		        &info_array->rx_queue_shaping_info[i],
		        sizeof(trf_mgmt_shaping_info_t));
		}

		break;
#endif  /* TRAFFIC_SHAPING */

#ifdef TRAFFIC_MGMT_RSSI_POLICY

	case IOV_GVAL(IOV_TRF_MGMT_RSSI_POLICY):
		if (alen < sizeof(uint32)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		*(uint32 *)a = trf_mgmt_ctxt->rssi_info.rssi_policy;
		break;

	case IOV_SVAL(IOV_TRF_MGMT_RSSI_POLICY):
		if (alen < sizeof(uint32)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		err = trf_mgmt_set_rssi_policy(trf_mgmt_ctxt, *(uint32 *)a);
		break;

#endif /* TRAFFIC_MGMT_RSSI_POLICY */

	    default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/*
 * Watchdog timer - do we need this?
 */
static int
BCMATTACHFN(wlc_trf_mgmt_watchdog)(void *cntxt)
{
#ifdef TRAFFIC_MGMT_RSSI_POLICY

	trf_mgmt_rssi_watchdog(cntxt);

#else

	BCM_REFERENCE(cntxt);

#endif /* TRAFFIC_MGMT_RSSI_POLICY */

	return BCME_OK;
}

/*
 * Process an interface up condition - do we need this?
 */
static int
BCMATTACHFN(wlc_trf_mgmt_interface_up)(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt)
{
	ASSERT(trf_mgmt_ctxt);
	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_interface_up()\n", WLCUNIT(trf_mgmt_ctxt)));

	return BCME_OK;
}

/*
 * Process an interface down condition - do we need this?
 */
int
BCMATTACHFN(wlc_trf_mgmt_interface_down)(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt)
{
	ASSERT(trf_mgmt_ctxt);
	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_interface_down()\n", WLCUNIT(trf_mgmt_ctxt)));

	return BCME_OK;
}

/*
 * BSS up/down handler that we register for the traffic management module.
 */
static void
wlc_trf_mgmt_bsscfg_updn_handler(void *ctx, bsscfg_up_down_event_data_t *evt)
{
	wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt = (wlc_trf_mgmt_ctxt_t *)ctx;
	wlc_bsscfg_t        *bsscfg;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_bsscfg_updn_handler()\n", WLCUNIT(trf_mgmt_ctxt)));
	ASSERT(ctx != NULL);
	ASSERT(evt != NULL);

	bsscfg = evt->bsscfg;

	if (evt->up) {
	    wlc_trf_mgmt_bsscfg_up(trf_mgmt_ctxt, bsscfg);
	} else {
	    wlc_trf_mgmt_bsscfg_down(trf_mgmt_ctxt, bsscfg);
	}
}

/*
 * Process a bsscfg up condition.
 * NOTE: If traffic mgmt. was previously configured, we'll carry over the
 * settings to the scb associated with the bsscfg that's now up. However, the
 * config settings include the IP identity for the bss which could be stale if
 * we've roamed. We rely on the tray app to plumb the new identity to us.
 */
static void
wlc_trf_mgmt_bsscfg_up(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt, wlc_bsscfg_t *bsscfg)
{
	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_bsscfg_up()\n", WLCUNIT(trf_mgmt_ctxt)));
	ASSERT(trf_mgmt_ctxt);

	if (bsscfg->trf_mgmt_info == NULL) {
	    /* Prepare an info block for this BSS. */
	    wlc_trf_mgmt_allocate_info_block(trf_mgmt_ctxt, bsscfg);
	}

	if (BSSCFG_STA(bsscfg)) {
	    struct scb  *scb;

	    if (bsscfg->trf_mgmt_info->config.trf_mgmt_enabled != TRUE)
		return;

	    /*
	     * Resolve the SCB for this associated bsscfg. We'll need this when
	     * registering the tx module.
	     * NOTE: We'll need to do something different for other interfaces.
	     */
	    scb = wlc_scbfind(
		    trf_mgmt_ctxt->wlc,
		    bsscfg,
		    &bsscfg->BSSID);

	    ASSERT(scb);

	    wlc_txmod_config(trf_mgmt_ctxt->wlc, scb, TXMOD_TRF_MGMT);
	}
}

/*
 * Process a bsscfg down condition.
 */
static void
wlc_trf_mgmt_clear_data(wlc_trf_mgmt_info_t *trf_mgmt_info)
{
	uint32  i;

	/* Unconditionally clear the tx/rx statistics and sampling data */
	for (i = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
#ifdef TRAFFIC_SHAPING
	    bzero(
		&trf_mgmt_info->tx_queue[i].sampled_data,
		sizeof(trf_mgmt_queue_sample_t) * TRF_MGMT_MAX_SAMPLING_PERIODS);

	    bzero(
		&trf_mgmt_info->rx_queue[i].sampled_data,
		sizeof(trf_mgmt_queue_sample_t) * TRF_MGMT_MAX_SAMPLING_PERIODS);

	    bzero(&trf_mgmt_info->tx_queue[i].info, sizeof(trf_mgmt_shaping_info_t));
	    bzero(&trf_mgmt_info->rx_queue[i].info, sizeof(trf_mgmt_shaping_info_t));
#endif  /* TRAFFIC_SHAPING */

	    bzero(&trf_mgmt_info->tx_queue[i].stats, sizeof(trf_mgmt_stats_t));
	    bzero(&trf_mgmt_info->rx_queue[i].stats, sizeof(trf_mgmt_stats_t));
	}

#ifdef TRAFFIC_SHAPING
	bzero(&trf_mgmt_info->tx_global_shaping_info, sizeof(trf_mgmt_global_info_t));
	bzero(&trf_mgmt_info->rx_global_shaping_info, sizeof(trf_mgmt_global_info_t));

	trf_mgmt_info->sample_index = 0;
#endif  /* TRAFFIC_SHAPING */
}


static void
wlc_trf_mgmt_bsscfg_down(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt, wlc_bsscfg_t *bsscfg)
{
	wlc_trf_mgmt_info_t *trf_mgmt_info;
	struct scb          *scb;
	struct scb_iter     scbiter;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_bsscfg_down()\n", WLCUNIT(trf_mgmt_ctxt)));
	ASSERT(trf_mgmt_ctxt);

	if (bsscfg->trf_mgmt_info == NULL) {
	    return;
	} else {
	    trf_mgmt_info = bsscfg->trf_mgmt_info;
	}

	if (trf_mgmt_info->config.trf_mgmt_enabled != TRUE)
	    return;

	/* Clear configuration parameters */
	bzero(&trf_mgmt_info->config, sizeof(trf_mgmt_config_t));

#ifdef TRAFFIC_SHAPING
	/* Flush any queued tx/rx packets */
	wlc_trf_mgmt_flush_tx_queue(trf_mgmt_info, FALSE);
	wlc_trf_mgmt_flush_rx_queue(trf_mgmt_info, FALSE);
#endif

	/* Unconditionally clear the tx/rx statistics and sampling data */
	wlc_trf_mgmt_clear_data(trf_mgmt_info);

#ifdef TRAFFIC_SHAPING
	/*
	 * Cancel the periodic sampling timer. We'll start it later if
	 * we're enabled.
	 */
	wl_del_timer(WLCWL(trf_mgmt_info), trf_mgmt_info->sampling_timer);
#endif  /* TRAFFIC_SHAPING */

	/* Clear the packet filter hash */
	wlc_trf_mgmt_hash_free(trf_mgmt_info);

	/*
	 * Disable the traffic management tx module for all scbs
	 * associated with this bsscfg.
	 */
#ifdef TRAFFIC_MGMT_RSSI_POLICY
	/* Only unconfigure the txmod if no RSSI policy is active */
	if (trf_mgmt_ctxt->rssi_info.rssi_policy == TRF_MGMT_RSSI_NONE) {

	    WL_RSSI(("%s: No RSSI policy, unconfiguring txmod for SCBs in BSS %p\n",
	        __FUNCTION__, trf_mgmt_info->bsscfg));

#endif /* TRAFFIC_MGMT_RSSI_POLICY */
	    FOREACH_BSS_SCB(trf_mgmt_info->wlc->scbstate, &scbiter, trf_mgmt_info->bsscfg, scb) {
	        if (!SCB_IS_IBSS_PEER(scb)) {
		    wlc_txmod_unconfig(trf_mgmt_ctxt->wlc, scb, TXMOD_TRF_MGMT);
	        }
	    }
#ifdef TRAFFIC_MGMT_RSSI_POLICY
	} /* RSSI policy == NONE */
#endif /* TRAFFIC_MGMT_RSSI_POLICY */
}

/*
 * Process a bsscfg allocate event. This is called when the BSSCFG struct is allocated.
 */
int
wlc_trf_mgmt_bsscfg_allocate(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt, wlc_bsscfg_t *bsscfg)
{
	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_bsscfg_allocate()\n", WLCUNIT(trf_mgmt_ctxt)));
	ASSERT(trf_mgmt_ctxt);

	if (bsscfg->trf_mgmt_info == NULL) {
	    /* Prepare an info block for this BSS. */
	    wlc_trf_mgmt_allocate_info_block(trf_mgmt_ctxt, bsscfg);
	}

	return BCME_OK;
}

/*
 * Process a bsscfg free condition. This is called when the BSSCFG structure is freed.
 */
int
wlc_trf_mgmt_bsscfg_free(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt, wlc_bsscfg_t *bsscfg)
{
	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_bsscfg_free()\n", WLCUNIT(trf_mgmt_ctxt)));
	ASSERT(trf_mgmt_ctxt);

	if (bsscfg->trf_mgmt_info != NULL) {
	    /* Release the info block for this BSS. */
	    wlc_trf_mgmt_free_info_block(bsscfg->trf_mgmt_info);

	    bsscfg->trf_mgmt_info = NULL;
	}

	return BCME_OK;
}

/*
 * Handle traffic management dump request for config settings.
 */
static int
wlc_trf_mgmt_dump_config(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt, struct bcmstrbuf *b)
{
	wlc_trf_mgmt_info_t *trf_mgmt_info;
	wlc_bsscfg_t        *bsscfg;
	int                 err = 0;
	int                 i;
	char                buf[32];

	FOREACH_BSS(trf_mgmt_ctxt->wlc, i, bsscfg) {
	    if (bsscfg->trf_mgmt_info == NULL) {
		continue;
	    }

	    trf_mgmt_info = bsscfg->trf_mgmt_info;

	    bcm_bprintf(b, "\n");
	    bcm_bprintf(b, "Traffic Management Configuration Settings for BSSCFG Index: %d\n", i);
	    bcm_bprintf(b, "Enabled                   : %d\n",
	        trf_mgmt_info->config.trf_mgmt_enabled);
	    bcm_bprintf(b, "Host IP Address           : %s\n",
	        bcm_ip_ntoa((struct ipv4_addr *)&trf_mgmt_info->config.host_ip_addr, buf));
	    bcm_bprintf(b, "Host IP Subnet Mask       : %s\n",
	        bcm_ip_ntoa((struct ipv4_addr *)&trf_mgmt_info->config.host_subnet_mask, buf));
#ifdef TRAFFIC_SHAPING
	    bcm_bprintf(b, "Downlink Bandwidth        : %d\n",
	        trf_mgmt_info->config.downlink_bandwidth);
	    bcm_bprintf(b, "Uplink Bandwidth          : %d\n",
	        trf_mgmt_info->config.uplink_bandwidth);
	    bcm_bprintf(b, "\n");

	    bcm_bprintf(b, "Minimum Tx Bandwidth[BK]  : %d\n",
	        trf_mgmt_info->config.min_tx_bandwidth[0]);
	    bcm_bprintf(b, "Minimum Tx Bandwidth[BE]  : %d\n",
	        trf_mgmt_info->config.min_tx_bandwidth[1]);
	    bcm_bprintf(b, "Minimum Tx Bandwidth[VI]  : %d\n",
	        trf_mgmt_info->config.min_tx_bandwidth[2]);
	    bcm_bprintf(b, "\n");

	    bcm_bprintf(b, "Minimum Rx Bandwidth[BK]  : %d\n",
	        trf_mgmt_info->config.min_rx_bandwidth[0]);
	    bcm_bprintf(b, "Minimum Rx Bandwidth[BE]  : %d\n",
	        trf_mgmt_info->config.min_rx_bandwidth[1]);
	    bcm_bprintf(b, "Minimum Rx Bandwidth[VI]  : %d\n",
	        trf_mgmt_info->config.min_rx_bandwidth[2]);
	    bcm_bprintf(b, "\n");
#endif  /* TRAFFIC_SHAPING */

	    bcm_bprintf(b, "Flags                     : 0x%04X\n",
	        trf_mgmt_info->config.flags);
	}

	return err;
}

/*
 * Handle traffic management dump request for statistics.
 */
static void
wlc_trf_mgmt_print_stats(
	trf_mgmt_queue_t    *queue,
	struct bcmstrbuf    *b)
{
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "Packets processed   : %d\n", queue->stats.num_processed_packets);
	bcm_bprintf(b, "Bytes processed     : %d\n", queue->stats.num_processed_bytes);
	bcm_bprintf(b, "Packets discarded   : %d\n", queue->stats.num_discarded_packets);
	bcm_bprintf(b, "\n");
}

static int
wlc_trf_mgmt_dump_stats(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt, struct bcmstrbuf *b)
{
	wlc_trf_mgmt_info_t *trf_mgmt_info;
	wlc_bsscfg_t        *bsscfg;
	int                 err = 0;
	int                 i, j;

	FOREACH_BSS(trf_mgmt_ctxt->wlc, i, bsscfg) {
	    if (bsscfg->trf_mgmt_info == NULL) {
		continue;
	    }

	    trf_mgmt_info = bsscfg->trf_mgmt_info;

	    bcm_bprintf(b, "\n");
	    bcm_bprintf(b, "Traffic Management Statictics for BSSCFG Index: %d\n", i);

	    if (trf_mgmt_info->config.trf_mgmt_enabled == 0) {
		bcm_bprintf(b, "Traffic management not enabled on this BSSCFG\n");

		continue;
	    }

	    bcm_bprintf(b, "\n");

	    for (j = 0; j < TRF_MGMT_MAX_PRIORITIES; j++) {
		bcm_bprintf(b, "Statistics for Tx Queue[%d]\n", j);
		wlc_trf_mgmt_print_stats(&trf_mgmt_info->tx_queue[j], b);

		bcm_bprintf(b, "Statistics for Rx Queue[%d]\n", j);
		wlc_trf_mgmt_print_stats(&trf_mgmt_info->rx_queue[j], b);
	    }
	}

	return err;
}


/*
 * Tx Module Functions:
 * These are called through the tx_mod chain which is started in wlc_sendpkt.
 */
/*
 * Process a tx packet and determine whether to manage it or pass it through.
 */
static void BCMFASTPATH
wlc_trf_mgmt_enq(void *context, struct scb *scb, void *pkt, uint prec)
{
	wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt = (wlc_trf_mgmt_ctxt_t *)context;

	if (wlc_trf_mgmt_handle_pkt(
		trf_mgmt_ctxt,
		wlc_bsscfg_primary(trf_mgmt_ctxt->wlc),
		pkt,
		TRUE) == BCME_OK) {
		    /*
		     * Traffic management accepted the packet for shaping.
		     */
		    return;
	}

	/* Take care of RSSI based processing as the last step */
	TRF_MGMT_DO_RSSI_PROCESSING(trf_mgmt_ctxt, scb, pkt);

	/* Pass the packet on to the next tx module. */
	SCB_TX_NEXT(TXMOD_TRF_MGMT, scb, pkt, prec);
}

/*
 * Return the transmit packets held by the traffic management
 * tx module.
 */
static uint
wlc_trf_mgmt_txpktcnt(void *context)
{
	uint                pktcnt = 0;
#ifdef TRAFFIC_SHAPING
	wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt = (wlc_trf_mgmt_ctxt_t *)context;
	wlc_bsscfg_t        *bsscfg;
	uint                i, j;
	trf_mgmt_queue_t    *tx_queue;

	/*
	 * We only queue packets for packet shaping. This function should return 0 if shaping
	 * is not compiled in.
	 */
	FOREACH_BSS(trf_mgmt_ctxt->wlc, i, bsscfg) {
	    if (bsscfg->trf_mgmt_info != NULL) {
		for (j = 0; j < TRF_MGMT_MAX_PRIORITIES; j++) {
		    tx_queue = &bsscfg->trf_mgmt_info->tx_queue[j];
		    pktcnt  += pktq_len(&tx_queue->pkt_queue);
		}
	    }
	}
#endif  /* TRAFFIC_SHAPING */

	return pktcnt;
}

/*
 * Handle an activation notification - do we need this?
 */
static void wlc_trf_mgmt_activate(void *context, struct scb *scb)
{
	BCM_REFERENCE(context);
	BCM_REFERENCE(scb);
}

/*
 * Handle a deactivation notification - do we need this?
 */
static void
wlc_trf_mgmt_deactivate(void *context, struct scb *scb)
{
	BCM_REFERENCE(context);
	BCM_REFERENCE(scb);
}


/*
 * Packet Filtering and Management Functions:
 * These functions include filtering and queue management function.
 */
/*
 * Traffic management mainline packet handler.
 *
 *  Return value:
 *   BCME_OK      Packet parsed and filtered successfully for trafic shaping
 *   BCME_ERROR   Packet rejected for traffic shaping
 */
int BCMFASTPATH
wlc_trf_mgmt_handle_pkt(
	wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt,
	wlc_bsscfg_t        *bsscfg,
	void                *pkt,
	bool                in_tx_path)
{
	wlc_trf_mgmt_info_t *trf_mgmt_info;
	wlc_pkt_desc_t      pkt_desc;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_handle_pkt()\n", WLCUNIT(trf_mgmt_ctxt)));

	trf_mgmt_info = bsscfg->trf_mgmt_info;

	/* Don't shape traffic if safe mode is enabled on this BSSCFG */
	if (BSSCFG_SAFEMODE(bsscfg)) {
	    return BCME_ERROR;
	}

	/* Don't shape traffic if traffic management is not enabled. */
	if ((trf_mgmt_info == NULL) || (trf_mgmt_info->config.trf_mgmt_enabled != TRUE)) {
	    return BCME_ERROR;
	}

	if (((!in_tx_path) && TRF_MGMT_FLAGS(trf_mgmt_info, TRF_MGMT_FLAG_NO_RX))) {
		/* return if traffic mgmt not needed for rx direction */
		return BCME_ERROR;
	}

	if (BSSCFG_STA(bsscfg)) {
	    if (!TRF_MGMT_FLAGS(trf_mgmt_info, TRF_MGMT_FLAG_DISABLE_SHAPING)) {
		/*
		 * Only perform traffic shaping on packets for the primary interface.
		 */
		if (bsscfg != wlc_bsscfg_primary(trf_mgmt_ctxt->wlc)) {
		    WL_ERROR((
			"wl%d: wlc_trf_mgmt_handle_pkt: Invalid bsscfg\n",
			WLCUNIT(trf_mgmt_info)));

		    return BCME_ERROR;
		}

		/*
		 * We'll only shape traffic if we're operating in STA mode and associated
		 * with an AP.If not, let it pass through.
		 */
		if (BSSCFG_IBSS(bsscfg) || !bsscfg->associated) {
		    WL_ERROR((
			"wl%d: wlc_trf_mgmt_handle_pkt: Invalid configuration\n",
			WLCUNIT(trf_mgmt_info)));

			return BCME_ERROR;
		}
	    }
	}

	/* Parse packet */
	if (wlc_trf_mgmt_parse_pkt(trf_mgmt_info, pkt, in_tx_path, &pkt_desc) != BCME_OK) {
	    WL_ERROR(("wl%d: %s: Invalid packet for traffic mgmt\n",
	        WLCUNIT(trf_mgmt_info),
	        __FUNCTION__));

	    return BCME_ERROR;
	}

	/* Filter packet    */
	if (wlc_trf_mgmt_filter_pkt(trf_mgmt_info, pkt, in_tx_path, &pkt_desc) != BCME_OK) {
	    WL_TRF_MGMT(("wl%d: %s: Allow packet to pass through (dport %d, sport %d, prot %d)\n",
	        WLCUNIT(trf_mgmt_info),
	        __FUNCTION__,
	        pkt_desc.filter.dst_port,
	        pkt_desc.filter.src_port,
	        pkt_desc.filter.prot));

	    return BCME_ERROR;
	}

#ifdef TRAFFIC_SHAPING
	/*
	 * Process queued packets now and send them out if bandwidth is available.
	 * Packets that can't be sent out will be handled by the sampling timer.
	 */
	if (in_tx_path == TRUE) {
	    wlc_trf_mgmt_process_all_queues(trf_mgmt_info, trf_mgmt_info->tx_queue);
	} else {
	    wlc_trf_mgmt_process_all_queues(trf_mgmt_info, trf_mgmt_info->rx_queue);
	}
#endif

	return BCME_OK;
}

/*
 * Send a packet out through the WLC's tx module chain.
 */
static void
wlc_trf_mgmt_send_pkt(wlc_trf_mgmt_info_t *trf_mgmt_info, void *pkt)
{
	BCM_REFERENCE(trf_mgmt_info);

	/* Take care of RSSI based processing as the last step */
	TRF_MGMT_DO_RSSI_PROCESSING(trf_mgmt_info->trf_mgmt_ctxt, WLPKTTAGSCBGET(pkt), pkt);

	/* Send this packet out through the chain of tx modules. */
	SCB_TX_NEXT(TXMOD_TRF_MGMT, WLPKTTAGSCBGET(pkt), pkt, WLC_PRIO_TO_PREC(PKTPRIO(pkt)));
}


/*
 * Private Functions:
 * These functions are not exposed outside of the scope of this module. Most of these
 * functions operate off of the traffic management info block (wlc_trf_mgmt_info_t *)
 * within a wlc_bsscfg_t structure,
 */
/*
 * IOVAR Support Functions:
 * These functions perform actions based on IOVAR requests.
 */
/*
 * Handle IOV_TRF_MGMT_CONFIG IOVAR.
 */
static int
wlc_trf_mgmt_configure(wlc_trf_mgmt_info_t *trf_mgmt_info, void *a)
{
	wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt = trf_mgmt_info->trf_mgmt_ctxt;
	struct scb          *scb;
	struct scb_iter     scbiter;
	trf_mgmt_config_t   *config = (trf_mgmt_config_t *)a;
	bool                trf_mgmt_was_enabled;
#ifdef TRAFFIC_SHAPING
	int                 i;
#endif

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_configure()\n", WLCUNIT(trf_mgmt_info)));

	trf_mgmt_was_enabled = (trf_mgmt_info->config.trf_mgmt_enabled > 0);

	if (config->trf_mgmt_enabled == 0) {
	    /*
	     * If we're disabled for traffic management, then clear the settings
	     * and release any packets
	    */
	    wlc_trf_mgmt_bsscfg_down(trf_mgmt_ctxt, trf_mgmt_info->bsscfg);
	} else {
	    /*
	     * Do a quick sanity check on the config settings.
	     */
	    if ((config->host_ip_addr == 0) || (config->host_subnet_mask == 0)) {
		WL_ERROR(("wl%d: %s: Configured with 0 values for local IP subnet\n",
		    WLCUNIT(trf_mgmt_info),
		    __FUNCTION__));

		return BCME_ERROR;
	    }

	    if ((config->downlink_bandwidth == 0) || (config->uplink_bandwidth == 0)) {
		WL_ERROR(("wl%d: %s: Configured with 0 values for bandwidth\n",
		    WLCUNIT(trf_mgmt_info),
		    __FUNCTION__));

		return BCME_ERROR;
	    }


	    /*
	     * If we're enabled for traffic management, then save the settings
	     * and configure the mgmt engine.
	    */
	    bcopy(a, &trf_mgmt_info->config, sizeof(trf_mgmt_config_t));

#ifdef TRAFFIC_SHAPING
	    /*
	     * Cancel the periodic sampling timer. We'll start it later if
	     * we're enabled.
	     */
	    wl_del_timer(WLCWL(trf_mgmt_info), trf_mgmt_info->sampling_timer);
#endif /* TRAFFIC_SHAPING */

	    /*
	     * NOTE: If we're currently enabled., we don't want to clear the current
	     * sampling data. We just want to apply any changes to the bandwidth reservations.
	     */
	    if (trf_mgmt_was_enabled != TRUE) {
		/* Clear the packet filter hash */
		wlc_trf_mgmt_hash_free(trf_mgmt_info);

		/* Clear the tx/rx shaping parameters, statistics, and sampling data */
		wlc_trf_mgmt_clear_data(trf_mgmt_info);

		/*
		 * Enable the traffic management tx module for all scbs
		 * currently associated with this bsscfg.
		 */
		FOREACH_BSS_SCB(
		    trf_mgmt_info->wlc->scbstate,
		    &scbiter,
		    trf_mgmt_info->bsscfg, scb) {
			if (!SCB_IS_IBSS_PEER(scb)) {
			    wlc_txmod_config(trf_mgmt_ctxt->wlc, scb, TXMOD_TRF_MGMT);
			}
		}
	    }

#ifdef TRAFFIC_SHAPING
	    /*
	     * If the minimum bandwidth parameters are all 0, then use
	     * the default settings
	     */
	    for (i = 0;  i < TRF_MGMT_MAX_PRIORITIES; i++) {
		if (trf_mgmt_info->config.min_tx_bandwidth[i] != 0)
		    break;
		}

	    if (i >= TRF_MGMT_MAX_PRIORITIES) {
		for (i = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
		    trf_mgmt_info->config.min_tx_bandwidth[i] =
		        trf_mgmt_default_guaranteed_bandwidth[i];
		}
	    }

	    for (i = 0;  i < TRF_MGMT_MAX_PRIORITIES; i++) {
		if (trf_mgmt_info->config.min_rx_bandwidth[1] != 0)
		    break;
		}

	    if (i >= TRF_MGMT_MAX_PRIORITIES) {
		for (i = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
		    trf_mgmt_info->config.min_rx_bandwidth[i] =
		        trf_mgmt_default_guaranteed_bandwidth[i];
		}
	    }

	    if (!TRF_MGMT_FLAGS(trf_mgmt_info, TRF_MGMT_FLAG_DISABLE_SHAPING)) {
		/* Set up the bandwidth parameters */
		wlc_trf_mgmt_set_shaping_parameters(trf_mgmt_info);

		/*
		 * Start the BSS's periodic sampling timer.
		 */
		wl_add_timer(
		    WLCWL(trf_mgmt_info),
		    trf_mgmt_info->sampling_timer,
		    TRF_MGMT_SAMPLING_PERIOD,
		    TRUE);
	    }
#endif  /* TRAFFIC_SHAPING */
	}

	return BCME_OK;
}

/*
 * Handle IOV_TRF_MGMT_CONFIG IOvar.
 */
static int
wlc_trf_mgmt_flags(wlc_trf_mgmt_info_t *trf_mgmt_info, void *a)
{
	uint32  *flags;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_flags()\n", WLCUNIT(trf_mgmt_info)));

	flags = (uint32 *)a;

#ifdef TRAFFIC_SHAPING
	/*
	 * Enable or disable traffic shaping. We do this only if we've been
	 * enabled to start traffic management.
	 */
	if (trf_mgmt_info->config.trf_mgmt_enabled == 1) {
	    if ((*flags & TRF_MGMT_FLAG_DISABLE_SHAPING) &&
	        (!TRF_MGMT_FLAGS(trf_mgmt_info, TRF_MGMT_FLAG_DISABLE_SHAPING))) {
		    /*
		     * We've been shaping traffic but have been requested to stop.
		     */
		    wl_del_timer(WLCWL(trf_mgmt_info), trf_mgmt_info->sampling_timer);

		    /* Process any queued tx/rx packets */
		    wlc_trf_mgmt_flush_tx_queue(trf_mgmt_info, FALSE);
		    wlc_trf_mgmt_flush_rx_queue(trf_mgmt_info, FALSE);

		    /* Unconditionally clear the tx/rx statistics and sampling data */
		    wlc_trf_mgmt_clear_data(trf_mgmt_info);
		} else
		if (!(*flags & TRF_MGMT_FLAG_DISABLE_SHAPING) &&
		    (TRF_MGMT_FLAGS(trf_mgmt_info, TRF_MGMT_FLAG_DISABLE_SHAPING))) {
		    /*
		     * We've not been shaping traffic but have been requested to start.
		     * First, set up the bandwidth parameters.
		     */
		    wlc_trf_mgmt_set_shaping_parameters(trf_mgmt_info);

		    /*
		     * Start the BSS's periodic sampling timer.
		     */
		    wl_add_timer(
			WLCWL(trf_mgmt_info),
			trf_mgmt_info->sampling_timer,
			TRF_MGMT_SAMPLING_PERIOD,
			TRUE);

		    /* Flush any queued tx/rx packets */
		    wlc_trf_mgmt_flush_tx_queue(trf_mgmt_info, FALSE);
		    wlc_trf_mgmt_flush_rx_queue(trf_mgmt_info, FALSE);
		    }
		}
#endif  /* TRAFFIC_SHAPING */

	trf_mgmt_info->config.flags = *flags;

	return BCME_OK;
}

/*
 * Allocate resources for a BSS info block
 */
static wlc_trf_mgmt_info_t *
wlc_trf_mgmt_allocate_info_block(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt, wlc_bsscfg_t *bsscfg)
{
	wlc_trf_mgmt_info_t *trf_mgmt_info;
	wlc_info_t          *wlc = trf_mgmt_ctxt->wlc;
#ifdef TRAFFIC_SHAPING
	int                 i;
#endif  /* TRAFFIC_SHAPING */

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_allocate_info_block()\n", WLCUNIT(trf_mgmt_ctxt)));

	trf_mgmt_info = MALLOC(wlc->osh, sizeof(wlc_trf_mgmt_info_t));
	if (!trf_mgmt_info) {
	    WL_ERROR(("wl%d: %s: MALLOC failed; total mallocs %d bytes\n",
	        WLCUNIT(trf_mgmt_info),
	        __FUNCTION__,
	        MALLOCED(wlc->osh)));

	    return NULL;
	}

	/* init traffic management context struct */
	bzero(trf_mgmt_info, sizeof(wlc_trf_mgmt_info_t));

	trf_mgmt_info->wlc             = trf_mgmt_ctxt->wlc;
	trf_mgmt_info->trf_mgmt_ctxt   = trf_mgmt_ctxt;
	trf_mgmt_info->bsscfg          = bsscfg;

	trf_mgmt_info->sizeof_hash_heap_desc =
	    sizeof(hash_heap_desc_t) + (sizeof(hash_element_desc_t) * FHT_MAX_HASH_HEAP_ELEMENTS);

#ifdef TRAFFIC_SHAPING
	/* Initialize the priority queues for packets */
	for (i = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
	    pktqinit(&trf_mgmt_info->tx_queue[i].pkt_queue, MAX_TX_QUEUE_LEN);
	    pktqinit(&trf_mgmt_info->rx_queue[i].pkt_queue, MAX_RX_QUEUE_LEN);
	}

	/*
	 * Initialize the sampling timer for this info block. This samples the packet flow
	 * for traffic shaping.
	 */
	trf_mgmt_info->sampling_timer =	wl_init_timer(
	                                    wlc->wl,
	                                    wlc_trf_mgmt_sampling_timer,
	                                    trf_mgmt_info,
	                                    "trf_mgmt_sampling_timer");

	if (trf_mgmt_info->sampling_timer == NULL) {
	    WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
	        WLCUNIT(trf_mgmt_info),
	        __FUNCTION__));

		/* Free the trf_mgmt_info structure on error */
		MFREE(wlc->osh, trf_mgmt_info, sizeof(wlc_trf_mgmt_info_t));

	    return NULL;
	}
#endif  /* TRAFFIC_SHAPING */

	bsscfg->trf_mgmt_info = trf_mgmt_info;

	return trf_mgmt_info;
}

/*
 * Frees allocated resources for a BSS info block
 */
static void
wlc_trf_mgmt_free_info_block(wlc_trf_mgmt_info_t *trf_mgmt_info)
{
	/* Free the resources for the hash heap */
	wlc_trf_mgmt_hash_free(trf_mgmt_info);

#ifdef TRAFFIC_SHAPING
	/* Free the packets in the tx queues */
	wlc_trf_mgmt_flush_tx_queue(trf_mgmt_info, TRUE);

	/* Free the packets in the rx queues */
	wlc_trf_mgmt_flush_rx_queue(trf_mgmt_info, TRUE);

	/* Free the sampling timer */
	if (trf_mgmt_info->sampling_timer) {
	    wl_del_timer(WLCWL(trf_mgmt_info), trf_mgmt_info->sampling_timer);
	    wl_free_timer(WLCWL(trf_mgmt_info), trf_mgmt_info->sampling_timer);
	}
#endif  /* TRAFFIC_SHAPING */

	MFREE(WLCOSH(trf_mgmt_info->trf_mgmt_ctxt), trf_mgmt_info, sizeof(wlc_trf_mgmt_info_t));
}


/*
 * Add one or more packet filters.
 *
 * Returns BCME_ERROR if filters could not be added. Otherwise, returns BCME_OK.
 */
static int
wlc_trf_mgmt_filter_add(
	wlc_trf_mgmt_info_t     *trf_mgmt_info,
	trf_mgmt_filter_list_t  *trf_mgmt_filter_list,
	uint32                  cmd_length)
{
	uint32              i;
	trf_mgmt_filter_t   *trf_mgmt_filter;
	hash_element_desc_t *phash_element;

	/* Check parameter length is adequate */
	if (cmd_length <
	    (OFFSETOF(trf_mgmt_filter_list_t, filter) +
	    trf_mgmt_filter_list->num_filters * sizeof(trf_mgmt_filter_t)))
		return BCME_ERROR;

	for (i = 0; i < trf_mgmt_filter_list->num_filters; i++) {
	    trf_mgmt_filter = &trf_mgmt_filter_list->filter[i];

	    /* Validate the priority and protocol value. */
	    if ((uint32)trf_mgmt_filter->priority >= trf_mgmt_priority_invalid)
		return BCME_ERROR;

	    if (!(trf_mgmt_filter->flags & TRF_FILTER_MAC_ADDR)) {
		if ((trf_mgmt_filter->prot != IP_PROT_TCP) &&
		    (trf_mgmt_filter->prot != IP_PROT_UDP))
			return BCME_ERROR;
	    }

	    /* Looks good, so add or update this entry */
	    phash_element = wlc_trf_mgmt_hash_find(trf_mgmt_info, trf_mgmt_filter);
	    if (phash_element) {
		/* Overwrite hash element with specified data. */
		bcopy(trf_mgmt_filter, (char *)&phash_element->filter, sizeof(trf_mgmt_filter_t));
	    } else {
		phash_element = wlc_trf_mgmt_hash_add(trf_mgmt_info, trf_mgmt_filter);
		ASSERT(phash_element != NULL);
	    }

	    WL_TRF_MGMT(("wl%d: %s: Added filter (dport %d, sport %d, prot %d)\n",
	        WLCUNIT(trf_mgmt_info),
	        __FUNCTION__,
	        trf_mgmt_filter->dst_port,
	        trf_mgmt_filter->src_port,
	        trf_mgmt_filter->prot));
	}

	return BCME_OK;
}

/*
 * Removes one or more packet filters.
 *
 * Returns BCME_ERROR if filters could not be removed. Otherwise, Returns BCME_OK.
 */
static int
wlc_trf_mgmt_filter_remove(
	wlc_trf_mgmt_info_t     *trf_mgmt_info,
	trf_mgmt_filter_list_t  *trf_mgmt_filter_list,
	uint32                  cmd_length)
{
	uint32              i;
	trf_mgmt_filter_t   *trf_mgmt_filter;
	hash_element_desc_t *phash_element;

	/* Check parameter length is adequate */
	if (cmd_length <
	    (OFFSETOF(trf_mgmt_filter_list_t, filter) +
	    trf_mgmt_filter_list->num_filters * sizeof(trf_mgmt_filter_t)))
		return BCME_ERROR;

	for (i = 0; i < trf_mgmt_filter_list->num_filters; i++) {
	    trf_mgmt_filter = &trf_mgmt_filter_list->filter[i];
	    phash_element   = wlc_trf_mgmt_hash_find(trf_mgmt_info, trf_mgmt_filter);

	    if (phash_element) {
		wlc_trf_mgmt_hash_remove(trf_mgmt_info, phash_element);

		WL_TRF_MGMT(("wl%d: %s: Removed filter (dport %d, sport %d, prot %d)\n",
		    WLCUNIT(trf_mgmt_info),
		    __FUNCTION__,
		    trf_mgmt_filter->dst_port,
		    trf_mgmt_filter->src_port,
		    trf_mgmt_filter->prot));
	    } else {
		WL_ERROR(("wl%d: %s: Could not find hash element (dport %d, sport %d, prot %d)\n",
		    WLCUNIT(trf_mgmt_info),
		    __FUNCTION__,
		    trf_mgmt_filter->dst_port,
		    trf_mgmt_filter->src_port,
		    trf_mgmt_filter->prot));

		return BCME_ERROR;
	    }
	}

	return BCME_OK;
}

/*
 * Lists the packet filters.
 *
 * Returns BCME_ERROR if filters could not be listed. Otherwise, Returns BCME_OK.
 */
static int
wlc_trf_mgmt_filter_list(
	wlc_trf_mgmt_info_t     *trf_mgmt_info,
	trf_mgmt_filter_list_t  *trf_mgmt_filter_list,
	uint32                  cmd_length)
{
	uint32               i, j;
	hash_bucket_desc_t   *phash_bucket;
	hash_element_desc_t  *phash_element;

	/*
	 * Clear the buffer and decrement the cmd length for the members at the start
	 * of the trf_mgmt_filter_list_t struct
	 */
	bzero(trf_mgmt_filter_list, cmd_length);
	cmd_length -= OFFSETOF(trf_mgmt_filter_list_t, filter);

	/* Go through each packet filter hash bucket and return the current filters */
	for (i = 0; i < FHT_MAX_HASH_BUCKETS; i++) {
	    phash_bucket = &trf_mgmt_info->filter_bucket[i];

	    if (phash_bucket->num_bucket_elements != 0) {
		phash_element = phash_bucket->first_bucket_element;

		for (j = 0; j < phash_bucket->num_bucket_elements; j++) {
		    if (cmd_length < sizeof(trf_mgmt_filter_t))
			return BCME_BUFTOOSHORT;

		    bcopy(
			&phash_element->filter,
			(char *)&trf_mgmt_filter_list->filter[trf_mgmt_filter_list->num_filters],
			sizeof(trf_mgmt_filter_t));

		    trf_mgmt_filter_list->num_filters++;

		    phash_element = phash_element->next_hash_element;
		    cmd_length   -= sizeof(trf_mgmt_filter_t);
		}
	    }
	}

	/* Go through the wildcard filter hash bucket and return the current filters */
	phash_bucket = &trf_mgmt_info->wildcard_bucket;

	if (phash_bucket->num_bucket_elements != 0) {
	    phash_element = phash_bucket->first_bucket_element;

	    for (j = 0; j < phash_bucket->num_bucket_elements; j++) {
		if (cmd_length < sizeof(trf_mgmt_filter_t))
		    return BCME_BUFTOOSHORT;

		bcopy(
		    &phash_element->filter,
		    (char *)&trf_mgmt_filter_list->filter[trf_mgmt_filter_list->num_filters],
		    sizeof(trf_mgmt_filter_t));

		trf_mgmt_filter_list->num_filters++;

		phash_element = phash_element->next_hash_element;
		cmd_length   -= sizeof(trf_mgmt_filter_t);
	    }
	}

	return BCME_OK;
}


/*
 * Packet Handling Functions:
 * These functions provide the traffic management support for packet queueing and
 * filtering.
 */
/*
 * Parse out the IP packet and prepare a packet descriptor for the frame. This also performs
 * L2 filtering, such as screening out mcast/bcast packets and ucast packets that do not match
 * the NIC's current MAC address.
 *
 * Returns BCME_ERROR if frame is not IP; otherwise, returns BCME_OK if frame should be filtered
 * for the L3/L4 data.
 */
static int
wlc_trf_mgmt_parse_pkt(
	wlc_trf_mgmt_info_t *trf_mgmt_info,
	void                *pkt,
	bool                in_tx_path,
	wlc_pkt_desc_t      *pkt_desc)
{
	uint8                           *frame = PKTDATA(WLCOSH(trf_mgmt_info), pkt);
	int                             length = PKTLEN(WLCOSH(trf_mgmt_info), pkt);
	uint8                           *ptr;
	uint16                          ethertype;
	int                             l4_pkt_len;
	struct dot11_llc_snap_header    *llc_hdr;
	struct vlan_header              *vlan_hdr = NULL;
	struct ipv4_hdr                 *ip_hdr = NULL;
	struct bcmudp_hdr               *udp_hdr = NULL;
	struct bcmtcp_hdr               *tcp_hdr = NULL;
	void				*pktp = pkt;

	BCM_REFERENCE(pktp);
	bzero(pkt_desc, sizeof(wlc_pkt_desc_t));

	if (in_tx_path) {
	    struct ether_header *ether_hdr;

	    ether_hdr = (struct ether_header *)frame;
		llc_hdr   = (struct dot11_llc_snap_header *)(frame + ETHER_HDR_LEN);

	    /*
	     * Check if the L2 DADDR is mcast. If so, let the packet pass through.
	     * TODO: See if this is the right thing to do. We assume that mcast is intended for
	     * local subnet. May want to inspect TTL to glean hop count.
	     */
	    if (ETHER_ISMULTI((uint8 *)&ether_hdr->ether_dhost)) {
		WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_parse_pkt: MCast packet\n",
		    WLCUNIT(trf_mgmt_info)));

		return BCME_ERROR;
	    }

	    /*
	     * Packets submitted from tx path are in 802.3/SNAP format.
	     * We'll check the first 6 bytes of the LLC header to see if they
	     * match the RFC1042 format. We'll inspect the ethertype later.
	     */
	    if (length < ETHER_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN) {
			WL_ERROR(("wl%d: %s: short eth frame (%d)\n",
			    WLCUNIT(trf_mgmt_info),
			    __FUNCTION__,
			    length));

			return BCME_ERROR;
	    } else
		    if (bcmp(rfc1042_snap_hdr, llc_hdr, sizeof(rfc1042_snap_hdr))) {
				WL_ERROR(("wl%d: %s: Invalid RFC1042 SNAP header\n",
				WLCUNIT(trf_mgmt_info),
				__FUNCTION__));

			return BCME_ERROR;
	    }

	    /* Save off the ethertype and see if we need to grab the next fragment. */
	    ethertype = ntoh16(llc_hdr->type);

	    if (length == ETHER_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN) {
		/*
		 * We have a fragmented packet, so grab the next fragment.
		 */
		 pkt = PKTNEXT(WLCOSH(trf_mgmt_info), pkt);

		 if (pkt == NULL) {
		    WL_ERROR(("wl%d: %s: runt 802.3 frame\n",
		        WLCUNIT(trf_mgmt_info),
		        __FUNCTION__));

		    return BCME_ERROR;
		}

		frame =   PKTDATA(WLCOSH(trf_mgmt_info), pkt);
		length += PKTLEN(WLCOSH(trf_mgmt_info), pkt);

		ptr = frame;
	    } else {
		ptr = frame + ETHER_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN;
	    }
	} else {
	    struct dot11_header *dot11_hdr;
	    uint16              fc;
	    uint16              hdr_len;

	    /* Packets submitted from rx path are in 802.11 format. */
	    if (length <= DOT11_A3_HDR_LEN) {
		WL_TRF_MGMT(("wl%d: %s: short 802.11 data frame (%d)\n",
		    WLCUNIT(trf_mgmt_info),
		    __FUNCTION__,
		    length));

		return BCME_ERROR;
	    }

	    dot11_hdr = (struct dot11_header *)frame;
	    fc        = ltoh16(dot11_hdr->fc);

	    if (FC_TYPE(fc) != FC_TYPE_DATA) {
		WL_TRF_MGMT(("wl%d: %s: Not an 802.11 data frame\n",
		    WLCUNIT(trf_mgmt_info),
		    __FUNCTION__));

		return BCME_ERROR;
	    }

	    /*
	     * Check if the L2 DADDR is mcast or the SADDR is not ours (for promiscuous receives).
	     * If so, let the packet pass through.
	     * TODO: See if this is the right thing to do. We assume that mcast is intended for
	     * local subnet. May want to inspect TTL to glean hop count.
	     */
	    if (ETHER_ISMULTI((uint8 *)&dot11_hdr->a1) ||
	        (bcmp(
		    (uint8 *)&dot11_hdr->a1,
		    (uint8 *)&WLCPUB(trf_mgmt_info)->cur_etheraddr,
		    ETHER_ADDR_LEN) != 0)) {
		        WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_parse_pkt: MCast packet\n",
		            WLCUNIT(trf_mgmt_info)));

		         return BCME_ERROR;
	    }

	    /* Determine the 802.11 headr length and account for QoS headers */
	    if ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS)) {
		hdr_len = DOT11_A4_HDR_LEN;
	    } else {
		hdr_len = DOT11_A3_HDR_LEN;
	    }

	    if ((fc & FC_KIND_MASK) == FC_QOS_DATA) {
		hdr_len += DOT11_QOS_LEN;
	    }

	    /*
	     * We'll check the first 6 bytes of the LLC header to see if they
	     * match the RFC1042 format. We'll inspect the ethertype later.
	     */
	    llc_hdr = (struct dot11_llc_snap_header *)(frame + hdr_len);
	    length -= hdr_len;

	    if (length <= sizeof(struct dot11_llc_snap_header)) {
		WL_ERROR(("wl%d: %s: Null 802.11 data frame\n",
		    WLCUNIT(trf_mgmt_info),
		    __FUNCTION__));

		return BCME_ERROR;
	    } else
	    if (bcmp(rfc1042_snap_hdr, llc_hdr, sizeof(rfc1042_snap_hdr))) {
		WL_ERROR(("wl%d: %s: Invalid RFC1042 SNAP header\n",
		    WLCUNIT(trf_mgmt_info),
		    __FUNCTION__));

		return BCME_ERROR;
	    }

	    length -= sizeof(struct dot11_llc_snap_header);

	    if (length == 0) {
		WL_ERROR(("wl%d: %s: Zero-length data frame\n",
		    WLCUNIT(trf_mgmt_info),
		    __FUNCTION__));

		return BCME_ERROR;
	    }

	    /*
	     * Align the pointer to start of the Ethertype field so the rest of the code
	     * follows the 802.3 processing path.
	     */
	    ptr       = (uint8 *)llc_hdr + DOT11_LLC_SNAP_HDR_LEN;
	    ethertype = ntoh16(llc_hdr->type);
	}

	/* Skip VLAN tag, if any */
	if (ethertype == ETHER_TYPE_8021Q) {
	    /* Save off the location of the 802.1Q VLAN tag */
	    pkt_desc->vlan_hdr = (struct vlan_header *)ptr;
	    ptr               += VLAN_TAG_LEN;

	    if (ptr + ETHER_TYPE_LEN > frame + length) {
		WL_ERROR(("wl%d: %s: short VLAN frame (%d)\n",
		    WLCUNIT(trf_mgmt_info),
		    __FUNCTION__,
		    length));

		return BCME_ERROR;
	    }

	    ethertype = ntoh16_ua((const void *)ptr);
	}

	/*
	 *Skip packets that aren't IPv4
	 * TODO: IPv6 header parsing
	 */
	if (ethertype != ETHER_TYPE_IP) {
	    WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_parse_pkt: non-IP frame (ethertype 0x%x, length %d)\n",
	        WLCUNIT(trf_mgmt_info),
	        ethertype,
	        length));

	    return BCME_ERROR;
	}

	ip_hdr     = (struct ipv4_hdr *)(ptr);
	l4_pkt_len = ntoh16(ip_hdr->tot_len) - IPV4_HLEN(ip_hdr);
	ASSERT(l4_pkt_len >= 0);

	if (((ip_hdr->prot == IP_PROT_UDP) && (l4_pkt_len < sizeof(struct bcmudp_hdr))) ||
		((ip_hdr->prot == IP_PROT_TCP) && (l4_pkt_len < sizeof(struct bcmtcp_hdr)))) {
	    WL_ERROR(("wl%d: %s: truncated IP packet, protocol (%d), length (%d)\n",
	        WLCUNIT(trf_mgmt_info),
	        __FUNCTION__,
	        ip_hdr->prot,
	        l4_pkt_len));

	    return BCME_ERROR;
	}

	/*
	 * Parse TCP or UDP packets. Other packet types (e.g., ICMP) will be
	 * assigned to the BE queue. Also, return port info based on packet I/O path.
	 */
	ptr += IPV4_HLEN(ip_hdr);

	pkt_desc->filter.prot = ip_hdr->prot;

	if (ip_hdr->prot == IP_PROT_TCP) {
	    tcp_hdr = (struct bcmtcp_hdr *)(ptr);

	    if (in_tx_path) {
		pkt_desc->filter.src_port = ntoh16(tcp_hdr->src_port);
		pkt_desc->filter.dst_port = ntoh16(tcp_hdr->dst_port);
	    } else {
		pkt_desc->filter.src_port = ntoh16(tcp_hdr->dst_port);
		pkt_desc->filter.dst_port = ntoh16(tcp_hdr->src_port);
	    }
	} else if (ip_hdr->prot == IP_PROT_UDP) {
	    udp_hdr = (struct bcmudp_hdr *)(ptr);

	    if (in_tx_path) {
		pkt_desc->filter.src_port = ntoh16(udp_hdr->src_port);
		pkt_desc->filter.dst_port = ntoh16(udp_hdr->dst_port);
	    } else {
		pkt_desc->filter.src_port = ntoh16(udp_hdr->dst_port);
		pkt_desc->filter.dst_port = ntoh16(udp_hdr->src_port);
	    }
	}

	/* Save off links to the L3 and L4 headers for packet inspection by the caller */
	if (AP_ENAB(trf_mgmt_info->wlc->pub)) {
			eacopy((void *)PKTDATA(WLCOSH(trf_mgmt_info), pktp),
				pkt_desc->filter.dst_ether_addr.octet);
	}

	pkt_desc->vlan_hdr = vlan_hdr;
	pkt_desc->ip_hdr   = ip_hdr;
	pkt_desc->udp_hdr  = udp_hdr;
	pkt_desc->tcp_hdr  = tcp_hdr;

	return BCME_OK;
}


/*
 * Filter L3/L4 packet payload to determine the priority class to assign.
 *
 * Returns BCME_ERROR if frame should pass through. Otherwise, returns BCME_OK.
 */
static int
wlc_trf_mgmt_filter_pkt(
	wlc_trf_mgmt_info_t *trf_mgmt_info,
	void                *pkt,
	bool                in_tx_path,
	wlc_pkt_desc_t      *pkt_desc)
{
	trf_mgmt_config_t       *config        =  &trf_mgmt_info->config;
	uint32                  priority_class =  trf_mgmt_priority_invalid;
	wlc_bsscfg_t            *bsscfg;
	hash_element_desc_t     *hash_element_desc = NULL;
#ifdef TRAFFIC_SHAPING
	uint8                   *tcp_hdr_flags;
	uint16                  tcp_hdr_len;
	uint8                   tcp_flags;
	uint32                  pkt_len;
	trf_mgmt_queue_t        *queue;
#endif
	BCM_REFERENCE(config);

	if (BSSCFG_STA(trf_mgmt_info->bsscfg)) {
	    if (!TRF_MGMT_FLAGS(trf_mgmt_info, TRF_MGMT_FLAG_MANAGE_LOCAL_TRAFFIC)) {
		if (!in_tx_path) {
		    /* Don't manage packets received from the local subnet. */
		    if (((*(uint32 *)&pkt_desc->ip_hdr->src_ip) & config->host_subnet_mask) ==
		        (config->host_ip_addr  & config->host_subnet_mask)) {
			    return BCME_ERROR;
		    }
		} else {
		    /* Don't manage packets sent to the local subnet. */
		    if (((*(uint32 *)&pkt_desc->ip_hdr->dst_ip) & config->host_subnet_mask) ==
		        (config->host_ip_addr  & config->host_subnet_mask)) {
			return BCME_ERROR;
		    }
		}
	    }

	    /* Put non-UDP or non-TCP packets in the BE queue (medium priority class) */
	    if ((pkt_desc->ip_hdr->prot != IP_PROT_TCP) &&
	        (pkt_desc->ip_hdr->prot != IP_PROT_UDP)) {
		priority_class = trf_mgmt_priority_medium;

		if (priority_class < trf_mgmt_priority_nochange) {
	            /*
	             * Set the priority value in the packet's OOB datat.
	             */
	            PKTSETPRIO(pkt, trf_mgmt_pcp_tbl[priority_class]);
		}

		goto queue_it;
	    }
	}

	/*
	 * See if we have a filter for this. If not, put the packet on the BE queue.
	 * First, see if there is a filter that matches the specified sport.
	 */
	if (TRF_MGMT_FLAGS(trf_mgmt_info, TRF_MGMT_FLAG_FILTER_ON_MACADDR)) {
	    pkt_desc->filter.flags |= TRF_FILTER_MAC_ADDR;
	}

	hash_element_desc = wlc_trf_mgmt_hash_find(trf_mgmt_info, &pkt_desc->filter);

	if (hash_element_desc == NULL && trf_mgmt_info->wildcard_bucket.num_bucket_elements > 0) {
	    /*
	     * Next, see if there is a matching wildcard filter.
	     */
	    hash_element_desc =
	        wlc_trf_mgmt_hash_wldcard_find(trf_mgmt_info, &pkt_desc->filter);
	}

	if (hash_element_desc != NULL) {
	    priority_class = hash_element_desc->filter.priority;

	    if (hash_element_desc->filter.flags & TRF_FILTER_FAVORED) {
		WLPKTTAG(pkt)->flags2 |= WLF2_FAVORED;
	    }
	} else {
	    /* If we don't have a filter, mark the packet's priority as BE by default. */
	    priority_class = trf_mgmt_priority_medium;
	}

	if (priority_class < trf_mgmt_priority_nochange) {
	    /*
	     * Set the priority value in the packet's OOB datat.
	     */
	    PKTSETPRIO(pkt, trf_mgmt_pcp_tbl[priority_class]);
	}

#ifdef TRAFFIC_SHAPING
	if (pkt_desc->ip_hdr->prot == IP_PROT_TCP) {
	    tcp_hdr_flags = (uint8 *)&pkt_desc->tcp_hdr->hdrlen_rsvd_flags;
	    tcp_hdr_len   = TCP_HDRLEN(tcp_hdr_flags[0]) * 4;
	    tcp_flags     = TCP_FLAGS(tcp_hdr_flags[1]);

	    /*
	     * Inspect TCP packets with no payload to see if we need to handle flags-only frames.
	     */
	    if (IPV4_HLEN(pkt_desc->ip_hdr) + tcp_hdr_len == ntoh16(pkt_desc->ip_hdr->tot_len)) {
		if (in_tx_path) {
		    /*
		     * Send out TCP frames with only SYN or ACK flags.
		     */
		     if (TCP_FLAGS(tcp_flags) & (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
			wlc_trf_mgmt_send_pkt(trf_mgmt_info, pkt);

			return BCME_OK;
		     }
		} else {
		    /*
		     * Send up TCP frames with only SYN flag.
		     */
		     if (TCP_FLAGS(tcp_flags) & TCP_FLAG_SYN) {
			bsscfg = trf_mgmt_info->wlc->bsscfg[WLPKTTAGBSSCFGGET(pkt)];

			wl_sendup(WLCWL(trf_mgmt_info), bsscfg->wlcif->wlif, pkt, 1);

			return BCME_OK;
		     }
		}
	    }
	}
#endif  /* TRAFFIC_SHAPING */

queue_it:
	ASSERT(priority_class < trf_mgmt_priority_invalid);

	/*
	 * Tag the packet's L2/L3 priority value if the filter doesn't disable it or
	 * we don't have a hit in our cache. We've already set the OOB priority value for
	 */
	if ((priority_class < trf_mgmt_priority_nochange) ||
	    (hash_element_desc == NULL)) {
		/*
		* Add the DSCP value if we've been configured to do so. We'll need to recompute the
		* IP header checksum if we do this.
		*/
		if (TRF_MGMT_FLAGS(trf_mgmt_info, TRF_MGMT_FLAG_ADD_DSCP) &&
		    (trf_mgmt_dscp_tbl[priority_class] != 0)) {
		    pkt_desc->ip_hdr->tos = (uint8)trf_mgmt_dscp_tbl[priority_class];

		    pkt_desc->ip_hdr->hdr_chksum = 0;
		    pkt_desc->ip_hdr->hdr_chksum =
		        wlc_trf_mgmt_ip_checksum(trf_mgmt_info, pkt_desc->ip_hdr);
	        }

	        /*
	         * Set the priority value in the packet's lb struct.
	         */
	        if (pkt_desc->vlan_hdr != NULL) {
		    /*
		     * Save the priority PCP value in the 802.1Q tag. We mask out the VID
		     * and CFI and retain those values.
		     */
		    pkt_desc->vlan_hdr->vlan_tag =
		        (uint16)((pkt_desc->vlan_hdr->vlan_tag & 0x1fff) +
		        (trf_mgmt_pcp_tbl[priority_class] << VLAN_PRI_SHIFT));
	        }
	}

	/*
	 * If we're only doing priority classification, then just send the packet out and
	 * avoid the traffic shaping code.
	 */
	if (TRF_MGMT_FLAGS(trf_mgmt_info, TRF_MGMT_FLAG_DISABLE_SHAPING)) {
	    if (in_tx_path) {
		wlc_trf_mgmt_send_pkt(trf_mgmt_info, pkt);
	    } else {
		bsscfg = trf_mgmt_info->wlc->bsscfg[WLPKTTAGBSSCFGGET(pkt)];

		wl_sendup(WLCWL(trf_mgmt_info), bsscfg->wlcif->wlif, pkt, 1);
	    }

	    return BCME_OK;
	}

#ifdef TRAFFIC_SHAPING
	/*
	 * We determined the priority class for this packet, so queue it. So, do specific
	 * tasks related to whether we're in the tx or rx path.
	 */
	if (in_tx_path) {
	    queue = &trf_mgmt_info->tx_queue[priority_class];

	    /*
	     * Check to see if this class has any bandwidth reserved for it. If not, we're
	     * blocking traffic for this class, so free the packet and tell the caller we
	     * consumed the packet.
	     */
	    if (trf_mgmt_info->config.min_tx_bandwidth[priority_class] == 0) {
		PKTFREE(WLCOSH(trf_mgmt_info), pkt, TRUE);

		return BCME_OK;
		}
	} else {
	    queue = &trf_mgmt_info->rx_queue[priority_class];

	    /*
	     * Check to see if this class has any bandwidth reserved for it. If not, we're
	     * blocking traffic for this class, so free the packet and tell the caller we
	     * consumed the packet.
	     */
	    if (trf_mgmt_info->config.min_rx_bandwidth[priority_class] == 0) {
		PKTFREE(WLCOSH(trf_mgmt_info), pkt, FALSE);

		return BCME_OK;
	    }
	}

	/*
	 * If we've adjusted the bandwidth reservations, set it back to the default settings
	 * if this is a high priority packet.
	 */
	if ((trf_mgmt_info->bandwidth_settings_adjusted == TRUE) &&
	    (priority_class == trf_mgmt_priority_high)) {
		wlc_trf_mgmt_adjust_bandwidth_usage_parameters(trf_mgmt_info, TRUE);
	}

	/*
	 * Do we have space in the queue?
	 * If not, deq the head (oldest packet in queue) and decide what to do with it.
	 */
	 if (pktq_full(&queue->pkt_queue)) {
	     wlc_trf_mgmt_handle_full_queue(trf_mgmt_info, queue);
	}

	pktenq(&queue->pkt_queue, pkt);

	/* Increment the queue statistics for this queue */
	pkt_len = pkttotlen(WLCOSH(trf_mgmt_info), pkt);

	queue->sampled_data[trf_mgmt_info->sample_index].bytes_produced += pkt_len;

	queue->info.num_bytes_produced_per_second += pkt_len;
	queue->info.num_queued_bytes              += pkt_len;
	queue->info.num_queued_packets++;
#endif  /* TRAFFIC_SHAPING */

	return BCME_OK;
}

/*
 * Caluclates an IP header checksum. The caller must have 0'd the checksum member
 * in the header before calling this.
 *
 * Returns the IP header checksum.
 */
static uint16
wlc_trf_mgmt_ip_checksum(wlc_trf_mgmt_info_t *trf_mgmt_info, struct ipv4_hdr *ip_hdr)
{
	uint32  cksum = 0;
	uint16  val;
	uint8   *buffer = (uint8 *)ip_hdr;
	int     size = IPV4_HLEN(ip_hdr);

	BCM_REFERENCE(trf_mgmt_info);

	while (size > 1) {
	    val  = *buffer++ << 8;
	    val |= *buffer++;

	    cksum += val;
	    size  -= 2;
	}

	if (size > 0) {
	    cksum += (*buffer) << 8;
	}

	cksum = (cksum >> 16) + (cksum & 0XFFFF);
	cksum += (cksum >>16);

	return ntoh16((uint16)~cksum);
}


/*
 * Traffic Management Hash Functions:
 * These functions manage the hash table and heap of free elements for packet filtering.
 */
/*
 * Allocate a row of the hash heap.
 *
 * Returns BCME_ERROR if initialization failed; otherwise, returns BCME_OK.
 */
static int
wlc_trf_mgmt_hash_allocate(wlc_trf_mgmt_info_t *trf_mgmt_info)
{
	uint32                  i;
	hash_element_desc_t     *phash_element;
	hash_heap_desc_t        *phash_heap_desc;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_hash_allocate()\n", WLCUNIT(trf_mgmt_info)));

	/* allocate the memory for the heap descriptor */
	phash_heap_desc = MALLOC(WLCOSH(trf_mgmt_info), trf_mgmt_info->sizeof_hash_heap_desc);

	if (!phash_heap_desc) {
	    WL_ERROR(("wl%d: %s: MALLOC failed; total mallocs %d bytes\n",
	        WLCUNIT(trf_mgmt_info),
	        __FUNCTION__,
	        MALLOCED(WLCOSH(trf_mgmt_info))));

	    return BCME_ERROR;
	}

	/* Initialize the hash heap for this heap descriptor. */
	bzero((void *)phash_heap_desc, trf_mgmt_info->sizeof_hash_heap_desc);

	phash_heap_desc->heap_mem =
	    (hash_element_desc_t *)((uint8 *)phash_heap_desc + sizeof(hash_heap_desc_t));

	phash_heap_desc->first_free_element = &phash_heap_desc->heap_mem[0];
	phash_heap_desc->num_free_elements  = FHT_MAX_HASH_HEAP_ELEMENTS;

	for (i = 0; i < FHT_MAX_HASH_HEAP_ELEMENTS; i++) {
	    phash_element = &phash_heap_desc->heap_mem[i];

	    phash_element->heap_desc = phash_heap_desc;

	    if (i < (FHT_MAX_HASH_HEAP_ELEMENTS - 1))
		phash_element->next_hash_element = &phash_heap_desc->heap_mem[i + 1];
	    else
		phash_element->next_hash_element = NULL;
	}

	/* Add the cache descriptor to the chain */
	if (trf_mgmt_info->num_hash_heap_desc != 0) {
	    phash_heap_desc->next_heap_desc = trf_mgmt_info->hash_heap_desc_list;
	} else {
	    phash_heap_desc->next_heap_desc = NULL;
	}

	trf_mgmt_info->hash_heap_desc_list = phash_heap_desc;
	trf_mgmt_info->num_hash_heap_desc++;

	return TRUE;
}

/*
 * Free all rows of the hash heap and table.
 */
static void
wlc_trf_mgmt_hash_free(wlc_trf_mgmt_info_t *trf_mgmt_info)
{
	uint32               i;
	hash_bucket_desc_t   *phash_bucket;
	hash_heap_desc_t     *phash_heap_desc, *ptemp_desc;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_hash_free()\n", WLCUNIT(trf_mgmt_info)));

	/* Free any resources for packet filter hash bucket elements. */
	for (i = 0; i < FHT_MAX_HASH_BUCKETS; i++) {
	    phash_bucket = &trf_mgmt_info->filter_bucket[i];

	    while (phash_bucket->first_bucket_element != NULL) {
		wlc_trf_mgmt_hash_remove(trf_mgmt_info, phash_bucket->first_bucket_element);
	    }

	    ASSERT(phash_bucket->num_bucket_elements == 0);
	}

	/* Free any resources for wildcard filter hash bucket elements. */
	    phash_bucket = &trf_mgmt_info->wildcard_bucket;

	    while (phash_bucket->first_bucket_element != NULL) {
		wlc_trf_mgmt_hash_remove(trf_mgmt_info, phash_bucket->first_bucket_element);
	    }

	ASSERT(phash_bucket->num_bucket_elements == 0);

	/* Free the allocated memory for each hash heap descriptor. */
	phash_heap_desc = trf_mgmt_info->hash_heap_desc_list;

	for (i = 0; i < trf_mgmt_info->num_hash_heap_desc; i++) {
	    ptemp_desc      = phash_heap_desc;
	    phash_heap_desc = phash_heap_desc->next_heap_desc;

	    MFREE(WLCOSH(trf_mgmt_info), ptemp_desc, trf_mgmt_info->sizeof_hash_heap_desc);
	}

	trf_mgmt_info->num_hash_heap_desc = 0;
	trf_mgmt_info->hash_heap_desc_list = NULL;
}

/*
 * Is this a wildcard filter?
 */
static bool
wlc_trf_mgmt_is_wildcard_filter(wlc_trf_mgmt_info_t *trf_mgmt_info, trf_mgmt_filter_t *filter)
{
	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_is_wildcard_filter()\n", WLCUNIT(trf_mgmt_info)));

	/*
	 * Wilcard filters have one or more members that are 0. L4 filters may have only port
	 * data, so we don't consider L2 or L3 members that are 0 wildcards unless
	 * the filter flags specify to use those mebers.
	 * TODO: Add flags for L3 filters.
	 */
	if (((filter->dst_port == 0) || (filter->src_port == 0)) &&
	    !(filter->flags & TRF_FILTER_MAC_ADDR)) {
	    return TRUE;
	}

	return FALSE;
}

/*
 * Adds a packet filter entry to a hash bucket.
 */
static hash_element_desc_t *
wlc_trf_mgmt_hash_add(wlc_trf_mgmt_info_t *trf_mgmt_info, trf_mgmt_filter_t *filter)
{
	uint32               i;
	hash_element_desc_t  *phash_element = NULL;
	hash_heap_desc_t     *phash_heap_desc;
	hash_bucket_desc_t   *phash_bucket;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_hash_add()\n", WLCUNIT(trf_mgmt_info)));

	/* Determine the hash bucket for the new entry. */
	if (wlc_trf_mgmt_is_wildcard_filter(trf_mgmt_info, filter) == TRUE) {
	    phash_bucket = &trf_mgmt_info->wildcard_bucket;
	} else {
	    phash_bucket = &trf_mgmt_info->filter_bucket[HASH_KEY(filter)];
	}

	/* Get a hash element from the heap. */
	phash_heap_desc = trf_mgmt_info->hash_heap_desc_list;

	for (i = 0; i < trf_mgmt_info->num_hash_heap_desc; i++) {
	    if (phash_heap_desc->num_free_elements != 0) {
		phash_element                       = phash_heap_desc->first_free_element;
		phash_heap_desc->first_free_element = phash_element->next_hash_element;
		phash_heap_desc->num_free_elements--;

		break;
	    }

	    /* Go to next hash heap descriptor */
	    phash_heap_desc = phash_heap_desc->next_heap_desc;
	}

	if (i >= trf_mgmt_info->num_hash_heap_desc) {
	    /*
	     * We don't have any free heap elements, so add another descriptor array.
	     * This will be added to the start of the hash heap descriptor list.
	     */
	    if (!wlc_trf_mgmt_hash_allocate(trf_mgmt_info)) {
		WL_ERROR(("wl%d: %s: Unable to add heap row. Current # of rows: %d\n",
		    WLCUNIT(trf_mgmt_info),
		    __FUNCTION__,
		    trf_mgmt_info->num_hash_heap_desc));

		return NULL;
	    }

	    phash_heap_desc = trf_mgmt_info->hash_heap_desc_list;
	    ASSERT(phash_heap_desc != NULL);

	    phash_element                       = phash_heap_desc->first_free_element;
	    phash_heap_desc->first_free_element = phash_element->next_hash_element;
	    phash_heap_desc->num_free_elements--;
	}

	/*
	 * Save off the filter info.
	 * NOTE: We oly use the scalar data from the packet descriptors for hash table lookups.
	 * The L3/L4 header pointers may be unreliable since these are per-packet offsets.
	 */
	ASSERT(phash_element);
	bcopy(filter, (char *)&phash_element->filter, sizeof(trf_mgmt_filter_t));

	/* Add the element to the start of the element list for the bucket. */
	phash_element->next_hash_element = phash_bucket->first_bucket_element;
	phash_element->prev_hash_element = NULL;
	phash_element->hash_bucket       = phash_bucket;

	if (phash_bucket->first_bucket_element != NULL) {
	    phash_bucket->first_bucket_element->prev_hash_element = phash_element;
	}

	phash_bucket->first_bucket_element = phash_element;

	phash_bucket->num_bucket_elements++;

	return phash_element;
}

/*
 * Removes a packet filter entry from a hash bucket.
 */
static void
wlc_trf_mgmt_hash_remove(wlc_trf_mgmt_info_t *trf_mgmt_info, hash_element_desc_t *phash_element)
{
	hash_heap_desc_t     *phash_heap_desc = phash_element->heap_desc;
	hash_bucket_desc_t   *phash_bucket = phash_element->hash_bucket;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_hash_remove()\n", WLCUNIT(trf_mgmt_info)));

	/* Remove the element from the bucket */
	if (phash_element->prev_hash_element != NULL) {
	    phash_element->prev_hash_element->next_hash_element = phash_element->next_hash_element;

	    if (phash_element->next_hash_element != NULL) {
		phash_element->next_hash_element->prev_hash_element =
		    phash_element->prev_hash_element;
	    }
	}
	else {
	    phash_bucket->first_bucket_element = phash_element->next_hash_element;

	    if (phash_element->next_hash_element != NULL) {
		phash_element->next_hash_element->prev_hash_element = NULL;
	    }
	}

	phash_element->hash_bucket = NULL;
	phash_bucket->num_bucket_elements--;

	/* Add the element to the start of its allocated heap descriptor array. */
	phash_element->prev_hash_element    = NULL;
	phash_element->next_hash_element    = phash_heap_desc->first_free_element;
	phash_heap_desc->first_free_element = phash_element;

	phash_heap_desc->num_free_elements++;
}

/*
 * Searches the hash heap for an exact filter match.
 * This function is also used to find filter entries for adding or removal.
 */
static hash_element_desc_t *
wlc_trf_mgmt_hash_find(wlc_trf_mgmt_info_t *trf_mgmt_info, trf_mgmt_filter_t *filter)
{
	uint32               i;
	hash_element_desc_t  *phash_element;
	hash_bucket_desc_t   *phash_bucket;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_hash_find()\n", WLCUNIT(trf_mgmt_info)));

	if (wlc_trf_mgmt_is_wildcard_filter(trf_mgmt_info, filter) == TRUE) {
	    phash_bucket = &trf_mgmt_info->wildcard_bucket;
	} else {
	    phash_bucket = &trf_mgmt_info->filter_bucket[HASH_KEY(filter)];
	}

	/* Find a match in this bucket. */
	phash_element = phash_bucket->first_bucket_element;
	for (i = 0; i < phash_bucket->num_bucket_elements; i++) {
	    ASSERT(phash_element != NULL);

	    /* TODO: Add dst IP addr in search. */
	    if (TRF_MGMT_FLAGS(trf_mgmt_info, TRF_MGMT_FLAG_FILTER_ON_MACADDR) &&
	        ether_cmp(phash_element->filter.dst_ether_addr.octet,
	            filter->dst_ether_addr.octet) == 0) {
			return phash_element;
	    } else
	    if ((phash_element->filter.src_port == filter->src_port) &&
	        (phash_element->filter.dst_port == filter->dst_port) &&
	        (phash_element->filter.prot     == filter->prot)) {
		/* Found it! */
		return phash_element;
	    }

	    phash_element = phash_element->next_hash_element;
	}

	return NULL;
}

/*
 * Searches the hash heap for an wildcard filter match.
 * This function is only used for packet inspection.
 */
static hash_element_desc_t *
wlc_trf_mgmt_hash_wldcard_find(wlc_trf_mgmt_info_t *trf_mgmt_info, trf_mgmt_filter_t *filter)
{
	uint32               i;
	hash_element_desc_t  *phash_element;
	hash_bucket_desc_t   *phash_bucket;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_hash_wldcard_find()\n", WLCUNIT(trf_mgmt_info)));

	phash_bucket = &trf_mgmt_info->wildcard_bucket;

	/* Find a match in this bucket. */
	phash_element = phash_bucket->first_bucket_element;
	for (i = 0; i < phash_bucket->num_bucket_elements; i++) {
	    ASSERT(phash_element != NULL);

	    /*
	     * If we have a matching protocol, see if we have a wildcard port value.
	     * TODO: Revise wildcard matching based on filter flags to allow for
	     * L2/L3 wildcards.
	     */
	    if (phash_element->filter.prot  == filter->prot) {
		if ((phash_element->filter.src_port == 0) &&
		    (phash_element->filter.dst_port == 0)) {
		    /* Found it! */
		    return phash_element;		}

		if ((phash_element->filter.dst_port == filter->dst_port) &&
		    (phash_element->filter.src_port == 0)) {
		    /* Found it! */
		    return phash_element;
		}

		if ((phash_element->filter.src_port == filter->src_port) &&
		    (phash_element->filter.dst_port == 0)) {
		    /* Found it! */
		    return phash_element;
		}
	    }

	    phash_element = phash_element->next_hash_element;
	}

	return NULL;
}

/* Add trf mgmt to the newly associating scb */
void
wlc_scb_trf_mgmt(wlc_info_t *wlc,  wlc_bsscfg_t *bsscfg, struct scb *scb)
{
	if ((bsscfg->trf_mgmt_info) && (bsscfg->trf_mgmt_info->config.trf_mgmt_enabled)) {
		 wlc_txmod_config(wlc, scb, TXMOD_TRF_MGMT);
	}
}

#ifdef TRAFFIC_SHAPING
/*
 * Traffic Shaping Internal Functions:
 * These functions provide the shaping support used by this module
 */
/*
 * Handle traffic management dump request for shaping info.
 */
static void
wlc_trf_mgmt_print_global_info(
	trf_mgmt_global_info_t  *global_info,
	struct bcmstrbuf        *b)
{
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "Maximum bytes/second                      : %d\n",
	    global_info->maximum_bytes_per_second);
	bcm_bprintf(b, "Maximum bytes/sampling period             : %d\n",
	    global_info->maximum_bytes_per_sampling_period);
	bcm_bprintf(b, "Total bytes consumed per second           : %d\n",
	    global_info->total_bytes_consumed_per_second);
	bcm_bprintf(b, "Total bytes consumed per sampling period  : %d\n",
	    global_info->total_bytes_consumed_per_sampling_period);
	bcm_bprintf(b, "Unused bytes for current sampling period  : %d\n",
	    global_info->total_unused_bytes_per_sampling_period);
	bcm_bprintf(b, "\n");
}

static void
wlc_trf_mgmt_print_shaping_info(
	trf_mgmt_queue_t    *queue,
	struct bcmstrbuf    *b)
{
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "Gauranteed bandwidth percentage  : %d%\n",
	    queue->info.gauranteed_bandwidth_percentage);
	bcm_bprintf(b, "Garaunteed bytes/second          : %d\n",
	    queue->info.guaranteed_bytes_per_second);
	bcm_bprintf(b, "Garaunteed bytes/sampling period : %d\n",
	    queue->info.guaranteed_bytes_per_sampling_period);
	bcm_bprintf(b, "Bytes produced per second        : %d\n",
	    queue->info.num_bytes_produced_per_second);
	bcm_bprintf(b, "Bytes consumed per second        : %d\n",
	    queue->info.num_bytes_consumed_per_second);
	bcm_bprintf(b, "Packets pending                  : %d\n",
	    queue->info.num_queued_packets);
	bcm_bprintf(b, "Bytes pending                    : %d\n",
	    queue->info.num_queued_bytes);
	bcm_bprintf(b, "\n");
}
/*
 * Dump the shaping info.
 */
static int
wlc_trf_mgmt_dump_shaping_info(wlc_trf_mgmt_ctxt_t *trf_mgmt_ctxt, struct bcmstrbuf *b)
{
	wlc_trf_mgmt_info_t *trf_mgmt_info;
	wlc_bsscfg_t        *bsscfg;
	int                 err = 0;
	int                 i, j;

	FOREACH_BSS(trf_mgmt_ctxt->wlc, i, bsscfg) {
	    if (bsscfg->trf_mgmt_info == NULL) {
		continue;
	    }

	    trf_mgmt_info = bsscfg->trf_mgmt_info;

	    bcm_bprintf(b, "\n");
	    bcm_bprintf(b, "Traffic Management Shaping Info for BSSCFG Index: %d\n", i);

	    if (trf_mgmt_info->config.trf_mgmt_enabled == 0) {
		bcm_bprintf(b, "Traffic management not enabled on this BSSCFG\n");

		continue;
	    }

	    bcm_bprintf(b, "\n");

	    bcm_bprintf(b, "Global Tx Shaping info\n");
	    wlc_trf_mgmt_print_global_info(&trf_mgmt_info->tx_global_shaping_info, b);

	    for (j = 0; j < TRF_MGMT_MAX_PRIORITIES; j++) {
		bcm_bprintf(b, "Shaping info. for Tx Queue[%d]\n", j);
		wlc_trf_mgmt_print_shaping_info(&trf_mgmt_info->tx_queue[j], b);
	    }

	    bcm_bprintf(b, "Global Rx Shaping info\n");
	    wlc_trf_mgmt_print_global_info(&trf_mgmt_info->rx_global_shaping_info, b);

	    for (j = 0; j < TRF_MGMT_MAX_PRIORITIES; j++) {
		bcm_bprintf(b, "Shaping info. for Rx Queue[%d]\n", j);
		wlc_trf_mgmt_print_shaping_info(&trf_mgmt_info->rx_queue[j], b);
	    }
	}

	return err;
}
#endif  /* TRAFFIC_SHAPING */


#ifdef TRAFFIC_SHAPING
/*
 * Traffic Shaping Queue and Packet Functions:
 * These functions manage the queueing and processing of packets for priority shaping.
 */
/*
 * Process a packet from the queue and send it out or up.
 */
static int32
wlc_trf_mgmt_process_pkt(wlc_trf_mgmt_info_t *trf_mgmt_info, trf_mgmt_queue_t *queue)
{
	void                    *pkt;
	uint32                  pkt_len;
	wlc_bsscfg_t            *bsscfg;
	trf_mgmt_global_info_t  *global_info;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_process_pkt()\n", WLCUNIT(trf_mgmt_info)));

	if (queue->is_rx_queue == TRUE) {
	    global_info = &trf_mgmt_info->rx_global_shaping_info;
	} else {
	    global_info = &trf_mgmt_info->tx_global_shaping_info;
	}

	pkt = pktdeq(&queue->pkt_queue);
	ASSERT(pkt != NULL);

	/*
	 * Save the packet length to be returned later.
	 */
	pkt_len = pkttotlen(WLCOSH(trf_mgmt_info), pkt);

	/*
	 * Decrement pending byte count and increment the bytes consumed since we're consuming it
	 */
	queue->info.num_queued_bytes -= pkt_len;
	queue->info.num_queued_packets--;

	queue->stats.num_processed_bytes += pkt_len;
	queue->stats.num_processed_packets++;

	/*
	 * Increment the shaping stats.
	 */
	queue->sampled_data[trf_mgmt_info->sample_index].bytes_consumed += pkt_len;
	queue->info.num_bytes_consumed_per_second                       += pkt_len;

	global_info->total_bytes_consumed_per_second          += pkt_len;
	global_info->total_bytes_consumed_per_sampling_period += pkt_len;

	/* Send this packet up or out. */
	if (queue->is_rx_queue) {
	    bsscfg = trf_mgmt_info->wlc->bsscfg[WLPKTTAGBSSCFGGET(pkt)];

	    wl_sendup(WLCWL(trf_mgmt_info), bsscfg->wlcif->wlif, pkt, 1);
	} else {
	    /* Send this packet to the next tx module */
	    wlc_trf_mgmt_send_pkt(trf_mgmt_info, pkt);
	}

	return pkt_len;
}

/*
 * Flush all queued tx packets.
 */
static void
wlc_trf_mgmt_flush_tx_queue(wlc_trf_mgmt_info_t *trf_mgmt_info, bool free_pkts)
{
	int32               i;
	void                *pkt;
	trf_mgmt_queue_t    *tx_queue;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_flush_tx_queue()\n", WLCUNIT(trf_mgmt_info)));

	/* Free the packets in the tx priority queues. These are flushed in priority order. */
	for (i = trf_mgmt_priority_high; i >= trf_mgmt_priority_low; i--) {
	    tx_queue = &trf_mgmt_info->tx_queue[i];

	    while (pktq_len(&tx_queue->pkt_queue) > 0) {
		pkt = pktdeq(&tx_queue->pkt_queue);
		ASSERT(pkt != NULL);

		if (free_pkts) {
		    PKTFREE(WLCOSH(trf_mgmt_info), pkt, TRUE);
		} else {
		    wlc_trf_mgmt_send_pkt(trf_mgmt_info, pkt);
		}
	    }

	bzero(&tx_queue->stats, sizeof(trf_mgmt_stats_t));
	}
}

/*
 * Flush all queued rx packets.
 */
static void
wlc_trf_mgmt_flush_rx_queue(wlc_trf_mgmt_info_t *trf_mgmt_info, bool free_pkts)
{
	int32               i;
	void                *pkt;
	trf_mgmt_queue_t    *rx_queue;
	wlc_bsscfg_t        *bsscfg;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_flush_rx_queue()\n", WLCUNIT(trf_mgmt_info)));

	/* Free the packets in the rx priority queues. These are flushed in priority order. */
	for (i = trf_mgmt_priority_high; i >= trf_mgmt_priority_low; i--) {
	    rx_queue = &trf_mgmt_info->rx_queue[i];

	    while (pktq_len(&rx_queue->pkt_queue) > 0) {
		pkt = pktdeq(&rx_queue->pkt_queue);
		ASSERT(pkt != NULL);

		if (free_pkts) {
		    PKTFREE(WLCOSH(trf_mgmt_info), pkt, FALSE);
		} else {
		    /* Send this packet up from the bsscfg interface */
		    bsscfg = trf_mgmt_info->wlc->bsscfg[WLPKTTAGBSSCFGGET(pkt)];

		    wl_sendup(WLCWL(trf_mgmt_info), bsscfg->wlcif->wlif, pkt, 1);
		}
	    }

	   bzero(&rx_queue->stats, sizeof(trf_mgmt_stats_t));
	}
}

/*
 * Handle a full queue and decide what to do with the packet at the head.
 */
static void
wlc_trf_mgmt_handle_full_queue(wlc_trf_mgmt_info_t *trf_mgmt_info, trf_mgmt_queue_t *queue)
{
	void                    *pkt;
	uint32                  pkt_len;
	wlc_pkt_desc_t          pkt_desc;
	trf_mgmt_global_info_t  *global_info;
	uint8                   *tcp_hdr_flags;
	uint16                  tcp_hdr_len;
	uint8                   tcp_flags;

	pkt     = pktq_peek((struct pktq *)&queue->pkt_queue, NULL);
	pkt_len = pkttotlen(WLCOSH(trf_mgmt_info), pkt);

	if (queue->is_rx_queue) {
	    global_info = &trf_mgmt_info->rx_global_shaping_info;

	    /*
	     * If we haven't been able to send this up, go ahead and do it now if we haven't
	     * exceeded the threshold for the queue during the current sampling period.
	     */
	    if ((pkt_len + global_info->total_bytes_consumed_per_sampling_period) <
		global_info->maximum_bytes_per_sampling_period) {
		    wlc_trf_mgmt_process_pkt(trf_mgmt_info, queue);

		    return;
	    }
	} else {
	    global_info = &trf_mgmt_info->tx_global_shaping_info;

	    wlc_trf_mgmt_parse_pkt(trf_mgmt_info, pkt, TRUE, &pkt_desc);

	    if (pkt_desc.ip_hdr->prot == IP_PROT_UDP) {
		/*
		 * We want to optimize for UDP as much as possible. If we haven't been able
		 * to send this out, go ahead and do it now if we haven't exceeded the threshold
		 * for the queue during the current sampling period.
		 */
		if ((pkt_len + global_info->total_bytes_consumed_per_sampling_period) <
		    global_info->maximum_bytes_per_sampling_period) {
			wlc_trf_mgmt_process_pkt(trf_mgmt_info, queue);

			return;
		}
	    } else
	    if (pkt_desc.ip_hdr->prot == IP_PROT_TCP) {
		tcp_hdr_flags = (uint8 *)&pkt_desc.tcp_hdr->hdrlen_rsvd_flags;
		tcp_hdr_len   = TCP_HDRLEN(tcp_hdr_flags[0]) * 4;
		tcp_flags     = TCP_FLAGS(tcp_hdr_flags[1]);

		/*
		 * For tx packets, just send out TCP packets that have only SYN or ACK
		 * flags set. Hopefully, this will reduce retransmits.
		 */
		if ((TCP_FLAGS(tcp_flags) & (TCP_FLAG_SYN | TCP_FLAG_ACK)) &&
		    (IPV4_HLEN(pkt_desc.ip_hdr) +
		    tcp_hdr_len == ntoh16(pkt_desc.ip_hdr->tot_len))) {
			wlc_trf_mgmt_process_pkt(trf_mgmt_info, queue);

			return;
		    }
		}
	    }

	/*
	 * Decrement the byte count for the packet we're deleting.
	 */
	queue->info.num_queued_bytes -= pkt_len;
	queue->info.num_queued_packets--;
	queue->stats.num_discarded_packets++;

	pkt = pktdeq(&queue->pkt_queue);

	PKTFREE(WLCOSH(trf_mgmt_info), pkt, TRUE);
}

/*
 * Process the packets in a single queue.
 */
static void
wlc_trf_mgmt_process_queue(wlc_trf_mgmt_info_t *trf_mgmt_info, trf_mgmt_queue_t *queue)
{
	void                    *pkt;
	uint32                  index = trf_mgmt_info->sample_index;
	uint32                  *current_unused_bytes;
	uint32                  pkt_len;
	trf_mgmt_global_info_t  *global_info;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_process_queue()\n", WLCUNIT(trf_mgmt_info)));

	if (queue->is_rx_queue == TRUE) {
	    global_info = &trf_mgmt_info->rx_global_shaping_info;
	} else {
	    global_info = &trf_mgmt_info->tx_global_shaping_info;
	}

	/*
	 * See how many of the unused bytes we can use.
	 */
	current_unused_bytes = &global_info->total_unused_bytes_per_sampling_period;

	CHECK_USAGE(global_info);

	/* Process the queued packets and send them out if bandwidth is available */
	while (pktq_len(&queue->pkt_queue) > 0) {
	    pkt = pktq_peek((struct pktq *)&queue->pkt_queue, NULL);
	    ASSERT(pkt != NULL);

	    /*
	     * See if we have enough bytes to send this packet out from this queue.
	     */
	    pkt_len = pkttotlen(WLCOSH(trf_mgmt_info), pkt);

	    if ((global_info->total_bytes_consumed_per_second + pkt_len) >
		global_info->maximum_bytes_per_second) {
		/*
		 * We've exceeded the threshold, so don't send anything. Wait until
		 * the sampling timer adjusts the per second counters to see if we can
		 * send this out.
		 */
		break;
	    }

	    if ((pkt_len + queue->sampled_data[index].bytes_consumed) <=
	        queue->info.guaranteed_bytes_per_sampling_period) {
		/* We do, so send out the packet. */
		wlc_trf_mgmt_process_pkt(trf_mgmt_info, queue);
	    } else
	    if ((queue->priority == trf_mgmt_priority_high) &&
	        (pkt_len <= *current_unused_bytes)) {
		    *current_unused_bytes -= wlc_trf_mgmt_process_pkt(trf_mgmt_info, queue);
		}
	    else
	    if ((queue->priority == trf_mgmt_priority_medium) &&
	        (trf_mgmt_info->bandwidth_settings_adjusted == TRUE)) {
		/*
		 * See if we can consume unused bytes from the previous sampling period.
		 * We do this to optimize the medium priority queue when we've adjusted
		 * the bandwidth reservations.
		 */
		if ((pkt_len <= *current_unused_bytes) &&
		    ((global_info->total_bytes_consumed_per_sampling_period + pkt_len) <=
		      global_info->maximum_bytes_per_sampling_period)) {
			*current_unused_bytes -= wlc_trf_mgmt_process_pkt(trf_mgmt_info, queue);
		} else {
		    break;
		}
	    } else {
		break;
	    }
	}

	CHECK_USAGE(global_info);
}

/*
 * Process the queued packets.
 */
static void
wlc_trf_mgmt_process_all_queues(wlc_trf_mgmt_info_t *trf_mgmt_info, trf_mgmt_queue_t *queue_array)
{
	int32   i;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_process_all_queues()\n", WLCUNIT(trf_mgmt_info)));

	/*
	 * Process the queued packets in priority order and send them out if
	 * bandwidth is available.
	 */
	for (i = trf_mgmt_priority_high; i >= trf_mgmt_priority_low; i--) {
	    wlc_trf_mgmt_process_queue(trf_mgmt_info, &queue_array[i]);
	}
}

/*
 * Process the unused bytes for the queues. This is done at the end of each sampling period.
 * We'll only send out as many bytes that we can, assuming that some queues may have
 * exceeded their max. number of bytes during this sampling period.
 */
static void
wlc_trf_mgmt_process_queue_unused_bytes(
	wlc_trf_mgmt_info_t *trf_mgmt_info,
	trf_mgmt_queue_t    *queue_array)
{
	int32                   i;
	void                    *pkt;
	uint32                  index = trf_mgmt_info->sample_index;
	uint32                  *current_unused_bytes;
	uint32                  num_consumed_bytes;
	int32                   num_unused_bytes;
	uint32                  pkt_len;
	trf_mgmt_queue_t        *queue;
	trf_mgmt_global_info_t  *global_info;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_process_queue_unused_bytes()\n", WLCUNIT(trf_mgmt_info)));

	if (queue_array[0].is_rx_queue == TRUE) {
	    global_info = &trf_mgmt_info->rx_global_shaping_info;
	} else {
	    global_info = &trf_mgmt_info->tx_global_shaping_info;
	}

	for (i = 0, num_consumed_bytes = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
	    queue = &queue_array[i];

	    /*
	     * Keep a counter of the bytes consumed during this sampling period.
	     */
	    num_consumed_bytes += queue->sampled_data[index].bytes_consumed;
	}

	/*
	 * We accumulate the total unused bytes over this sampling period as well
	 * as what is left from previous sampling periods.
	 */
	current_unused_bytes = &global_info->total_unused_bytes_per_sampling_period;

	if (num_consumed_bytes <= global_info->maximum_bytes_per_sampling_period) {
	    num_unused_bytes =
	        min(global_info->maximum_bytes_per_sampling_period,
	            (global_info->maximum_bytes_per_sampling_period - num_consumed_bytes) +
	                *current_unused_bytes);
	} else {
	    num_unused_bytes = 0;
	}

	/*
	 * See if we can send any packets out based on the number of unused bytes for this
	 * sampling period. We do this in priority order to use up the unused bytes from this
	 * sampling period.
	 */
	if (num_unused_bytes > 0) {
	    for (i = trf_mgmt_priority_high; i >= trf_mgmt_priority_low; i--) {
		queue = &queue_array[i];

		CHECK_USAGE(global_info);

		while (pktq_len(&queue->pkt_queue) > 0) {
		    pkt = pktq_peek((struct pktq *)&queue->pkt_queue, NULL);
		    ASSERT(pkt != NULL);

		    pkt_len = pkttotlen(WLCOSH(trf_mgmt_info), pkt);

		    /*
		     * To optimize transfers, flush out as many packets while we have unused bytes.
		     * This may cause exceeding the per-sampling threshold, but it spreads out the
		     * loads over time.
		     */
		    if ((pkt_len <= (uint32)num_unused_bytes) &&
		        ((global_info->total_bytes_consumed_per_sampling_period + pkt_len) <=
		          global_info->maximum_bytes_per_sampling_period)) {
			    num_unused_bytes -= wlc_trf_mgmt_process_pkt(trf_mgmt_info, queue);

			    CHECK_USAGE(global_info);
		    } else {
			break;
		    }
		}

		CHECK_USAGE(global_info);
	    }
	}

	/*
	 * Save the count of unused bytes from this sampling period and use
	 * them in the next sampling period.
	 */
	if (num_unused_bytes <= 0) {
	    /*
	     * The number of unused bytes may be < 0 if we needed to use some of these
	     * bytes in order to get packets out once the queue's threshold was exceeded.
	     */
	    *current_unused_bytes = 0;
	} else {
	    *current_unused_bytes = num_unused_bytes;
	}
}
#endif  /* TRAFFIC_SHAPING */


#ifdef TRAFFIC_SHAPING
/*
 * Traffic Shaping Configuration Functions:
 * These functions set and revise the parameters that are used for traffic shaping.
 */
/*
 * Sets bandwidth usage settings. These settings are for the gaurunteed bytes/sec that are
 * reserved for a priority class.
 */
static void
wlc_trf_mgmt_set_bandwidth_usage_parameters(
	wlc_trf_mgmt_info_t *trf_mgmt_info,
	trf_mgmt_queue_t    *queue)
{
	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_set_bandwidth_usage_parameters()\n",
	    WLCUNIT(trf_mgmt_info)));

	/*
	 * Set the maximum number of bytes that are guaranteed to be reserved
	 * for use by each priority class. If the gauranteed percentage is 0, then
	 * the class is blocked. Otherwise, we must allocate at least an MTU
	 * number of bytes to allow the class to transmit 1 packet.
	 */
	if (queue->is_rx_queue == TRUE) {
	    queue->info.guaranteed_bytes_per_second =
	        (trf_mgmt_info->rx_global_shaping_info.maximum_bytes_per_second *
	            queue->info.gauranteed_bandwidth_percentage) / 100;

	    if (queue->info.gauranteed_bandwidth_percentage == 0) {
		queue->info.guaranteed_bytes_per_sampling_period = 0;
	    } else {
		queue->info.guaranteed_bytes_per_sampling_period =
		    MAX(MAX_RX_PACKET_LEN,
		        (queue->info.guaranteed_bytes_per_second /
		            TRF_MGMT_MAX_SAMPLING_PERIODS));
	    }
	} else {
	    queue->info.guaranteed_bytes_per_second =
	        (trf_mgmt_info->tx_global_shaping_info.maximum_bytes_per_second *
	            queue->info.gauranteed_bandwidth_percentage) / 100;

	    if (queue->info.gauranteed_bandwidth_percentage == 0) {
		queue->info.guaranteed_bytes_per_sampling_period = 0;
	    } else {
		queue->info.guaranteed_bytes_per_sampling_period =
		    MAX(MAX_TX_PACKET_LEN,
		        (queue->info.guaranteed_bytes_per_second /
		            TRF_MGMT_MAX_SAMPLING_PERIODS));
	    }
	}
}

/*
 * Adjusts bandwidth usage settings. These settings are either restored to their
 * configured values or adjusted to allow more bandwidth to be allocated for priorities
 * with active usage.
 */
static void
wlc_trf_mgmt_adjust_bandwidth_usage_parameters(
	wlc_trf_mgmt_info_t *trf_mgmt_info,
	bool                restore_configured_parameters)
{
	uint32                  i, index;
	trf_mgmt_queue_t        *rx_queue_high, *rx_queue_medium;
	trf_mgmt_queue_t        *tx_queue_high, *tx_queue_medium;

	WL_TRF_MGMT(
	    ("wl%d: wlc_trf_mgmt_adjust_bandwidth_usage_parameters()\n",
	    WLCUNIT(trf_mgmt_info)));

	if (restore_configured_parameters == TRUE) {
	    /*
	     * Restore reserved bandwidth settings to configured values.
	     * Set the maximum number of bytes that are guaranteed to be reserved
	     * for use by each priority class. If the max number is 0, then
	     * the class is blocked. Otherwise, we must allocate at least an MTU
	     * number of bytes to allow the class to transmit 1 packet.
	     */
	    for (i = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
		/* Initialize the tx queue */
		trf_mgmt_info->tx_queue[i].priority    = i;
		trf_mgmt_info->tx_queue[i].is_rx_queue = FALSE;

		trf_mgmt_info->tx_queue[i].info.gauranteed_bandwidth_percentage =
		    trf_mgmt_info->config.min_tx_bandwidth[i];

		wlc_trf_mgmt_set_bandwidth_usage_parameters(
		    trf_mgmt_info,
		    &trf_mgmt_info->tx_queue[i]);

		/* Initialize the rx queue */
		trf_mgmt_info->rx_queue[i].priority    = i;
		trf_mgmt_info->rx_queue[i].is_rx_queue = TRUE;

		trf_mgmt_info->rx_queue[i].info.gauranteed_bandwidth_percentage =
		    trf_mgmt_info->config.min_rx_bandwidth[i];

		wlc_trf_mgmt_set_bandwidth_usage_parameters(
		    trf_mgmt_info,
		    &trf_mgmt_info->rx_queue[i]);
	    }

	    trf_mgmt_info->bandwidth_settings_adjusted = FALSE;
	} else {
	    if (trf_mgmt_info->bandwidth_settings_adjusted == TRUE) {
		return;
	    }

	    /*
	     * Check the usage of the high priority queues. If no activity has been seen
	     * in the last 3 sample periods, adjust the min. bandwidth reservation and add
	     * increase the best effort/medium queues.
	     */
	    rx_queue_high        = &trf_mgmt_info->rx_queue[trf_mgmt_priority_high];
	    rx_queue_medium      = &trf_mgmt_info->rx_queue[trf_mgmt_priority_medium];
	    tx_queue_high        = &trf_mgmt_info->tx_queue[trf_mgmt_priority_high];
	    tx_queue_medium      = &trf_mgmt_info->tx_queue[trf_mgmt_priority_medium];

	    if ((rx_queue_high->info.gauranteed_bandwidth_percentage == 0) &&
	        (tx_queue_high->info.gauranteed_bandwidth_percentage == 0)) {
		    /*
		     * If traffic is blocked for this priority class, just return since
		     * there's nothing to adjust.
		     */
		    return;
	    }

	    for (i = 0; i < TRF_MGMT_MAX_SAMPLING_PERIODS; i++) {
		index = (trf_mgmt_info->sample_index + (TRF_MGMT_MAX_SAMPLING_PERIODS - i)) %
			    TRF_MGMT_MAX_SAMPLING_PERIODS;

		/*
		 * Check the sampling periods for any high-priority
		 * tx/rx activity. We need to see some activity on either the
		 * rx or tx queues before we can adjust the bandwidth settings.
		 */
		if ((rx_queue_high->sampled_data[index].bytes_produced != 0) ||
		    (rx_queue_high->sampled_data[index].bytes_consumed != 0)) {
			break;
		} else
		if ((tx_queue_high->sampled_data[index].bytes_produced != 0) ||
		    (tx_queue_high->sampled_data[index].bytes_consumed != 0)) {
			break;
		}
	    }

	    if (i < TRF_MGMT_MAX_SAMPLING_PERIODS) {
		return;
	    }

	    /*
	     * Adjust the medium priority (BE) queue's bandwidth reservation if there wasn't any
	     * I/O activity on the high priority queues. We'll leave the low priority queues as
	     * they were configured.
	     */
	    if (tx_queue_high->info.gauranteed_bandwidth_percentage > MIN_BANDWIDTH_ADJUST) {
		tx_queue_medium->info.gauranteed_bandwidth_percentage +=
		    tx_queue_high->info.gauranteed_bandwidth_percentage - MIN_BANDWIDTH_ADJUST;

		tx_queue_high->info.gauranteed_bandwidth_percentage = MIN_BANDWIDTH_ADJUST;

		wlc_trf_mgmt_set_bandwidth_usage_parameters(trf_mgmt_info, tx_queue_high);
		wlc_trf_mgmt_set_bandwidth_usage_parameters(trf_mgmt_info, tx_queue_medium);

		trf_mgmt_info->bandwidth_settings_adjusted = TRUE;
	    }

	    if (rx_queue_high->info.gauranteed_bandwidth_percentage > MIN_BANDWIDTH_ADJUST) {
		rx_queue_medium->info.gauranteed_bandwidth_percentage +=
		    rx_queue_high->info.gauranteed_bandwidth_percentage - MIN_BANDWIDTH_ADJUST;

		rx_queue_high->info.gauranteed_bandwidth_percentage = MIN_BANDWIDTH_ADJUST;

		wlc_trf_mgmt_set_bandwidth_usage_parameters(trf_mgmt_info, rx_queue_high);
		wlc_trf_mgmt_set_bandwidth_usage_parameters(trf_mgmt_info, rx_queue_medium);

		trf_mgmt_info->bandwidth_settings_adjusted = TRUE;
	    }
	}
}

/*
 * Sets shaping parameters based on IOVAR parameters.
 * NOTE: This function is called from various places, so don't clear out the
 * shaping info or sampling data. Let the caller do that!
 */
static void
wlc_trf_mgmt_set_shaping_parameters(wlc_trf_mgmt_info_t *trf_mgmt_info)
{
	uint64  uplink_bps, downlink_bps;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_set_shaping_parameters()\n", WLCUNIT(trf_mgmt_info)));

	/*
	 * Adjust bandwidth utilization settings based on config parameters. First, convert
	 * from kilobits per second to bytes per second.
	 */
	uplink_bps   = ((trf_mgmt_info->config.uplink_bandwidth * 1024) / 8);
	downlink_bps = ((trf_mgmt_info->config.downlink_bandwidth * 1024) / 8);

	/*
	 * We only support a min of 128 kbps for bandwidth rates.
	 */
	if (uplink_bps < MIN_SUPPORTED_BANDWIDTH) {
	    uplink_bps = MIN_SUPPORTED_BANDWIDTH;
	}

	if (downlink_bps < MIN_SUPPORTED_BANDWIDTH) {
	    downlink_bps = MIN_SUPPORTED_BANDWIDTH;
	}

	trf_mgmt_info->uplink_bps   = (uint32)uplink_bps;
	trf_mgmt_info->downlink_bps = (uint32)downlink_bps;

	/*
	 * Initialize the global settings for the tx and rx queues.
	 */
	trf_mgmt_info->tx_global_shaping_info.maximum_bytes_per_second =
	    trf_mgmt_info->uplink_bps;

	trf_mgmt_info->rx_global_shaping_info.maximum_bytes_per_second =
	    trf_mgmt_info->downlink_bps;

	/* Adjust the bandwidth to account for fluctuations over DSL */
	trf_mgmt_info->tx_global_shaping_info.maximum_bytes_per_second +=
	    (trf_mgmt_info->uplink_bps * MAX_TX_BANDWIDTH_ADJUST) / 100;

	trf_mgmt_info->rx_global_shaping_info.maximum_bytes_per_second +=
	    (trf_mgmt_info->downlink_bps * MAX_RX_BANDWIDTH_ADJUST) / 100;

	/* Set the max bytes per sampling period */
	trf_mgmt_info->tx_global_shaping_info.maximum_bytes_per_sampling_period =
	    MAX(MAX_TX_PACKET_LEN,
	    (trf_mgmt_info->tx_global_shaping_info.maximum_bytes_per_second /
	        TRF_MGMT_MAX_SAMPLING_PERIODS));

	trf_mgmt_info->rx_global_shaping_info.maximum_bytes_per_sampling_period =
	    MAX(MAX_RX_PACKET_LEN,
	        (trf_mgmt_info->rx_global_shaping_info.maximum_bytes_per_second /
	        TRF_MGMT_MAX_SAMPLING_PERIODS));

	/*
	 * Set the bandwidth usage parameters to their configured settings.
	 */
	wlc_trf_mgmt_adjust_bandwidth_usage_parameters(trf_mgmt_info, TRUE);
}

/*
 * Sets the shaping parameters for a queue.
 */
static void
wlc_trf_mgmt_update_shaping_parameters(wlc_trf_mgmt_info_t *trf_mgmt_info, trf_mgmt_queue_t *queue)
{
	trf_mgmt_queue_sample_t *samples = queue->sampled_data;
	trf_mgmt_global_info_t  *global_info;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_update_shaping_parameters()\n", WLCUNIT(trf_mgmt_info)));

	if (queue->is_rx_queue == TRUE) {
	    global_info = &trf_mgmt_info->rx_global_shaping_info;
	} else {
	    global_info = &trf_mgmt_info->tx_global_shaping_info;
	}

	/* Decrement the running per second counters based on what was in the new entry */
	queue->info.num_bytes_produced_per_second -=
	    samples[trf_mgmt_info->sample_index].bytes_produced;

	queue->info.num_bytes_consumed_per_second -=
	    samples[trf_mgmt_info->sample_index].bytes_consumed;

	global_info->total_bytes_consumed_per_second -=
	    samples[trf_mgmt_info->sample_index].bytes_consumed;

	global_info->total_bytes_consumed_per_sampling_period = 0;
	samples[trf_mgmt_info->sample_index].bytes_produced   = 0;
	samples[trf_mgmt_info->sample_index].bytes_consumed   = 0;
}
#endif  /* TRAFFIC_SHAPING */

#ifdef TRAFFIC_SHAPING
/*
 * Traffic Shaping Timer Functions:
 * These timer functions sample or delay the packet traffic for each priority class.
 */
/*
 * Periodic sampling timer. This monitors the traffic flow for each priority class and adjusts the
 * delays used to shape traffic.
 *
 */
static void
wlc_trf_mgmt_sampling_timer(wlc_trf_mgmt_info_t *trf_mgmt_info)
{
	uint32  i;

	WL_TRF_MGMT(("wl%d: wlc_trf_mgmt_sampling_timer()\n", WLCUNIT(trf_mgmt_info)));

	/*
	 * Process any unused bytes for the current sampling period.
	 */
	wlc_trf_mgmt_process_queue_unused_bytes(
			trf_mgmt_info,
			trf_mgmt_info->rx_queue);

	wlc_trf_mgmt_process_queue_unused_bytes(
			trf_mgmt_info,
			trf_mgmt_info->tx_queue);

	/* Update the index into the sample table */
	trf_mgmt_info->sample_index =
	    (trf_mgmt_info->sample_index + 1) % TRF_MGMT_MAX_SAMPLING_PERIODS;

	/*
	 * Go through each queue and update the bandwidth settings based on traffic usage.
	 */
	for (i = 0; i < TRF_MGMT_MAX_PRIORITIES; i++) {
	    wlc_trf_mgmt_update_shaping_parameters(trf_mgmt_info, &trf_mgmt_info->rx_queue[i]);

	    wlc_trf_mgmt_update_shaping_parameters(trf_mgmt_info, &trf_mgmt_info->tx_queue[i]);
	}

	/*
	 * See if we can adjust the bandwidth reservations in case we have no high
	 * priority activity.
	 */
	wlc_trf_mgmt_adjust_bandwidth_usage_parameters(trf_mgmt_info, FALSE);

	/*
	 * Process the queues for the new sampling period.
	 */
	wlc_trf_mgmt_process_all_queues(trf_mgmt_info, trf_mgmt_info->rx_queue);
	wlc_trf_mgmt_process_all_queues(trf_mgmt_info, trf_mgmt_info->tx_queue);
}
#endif  /* TRAFFIC_SHAPING */
