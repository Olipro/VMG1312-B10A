/***********************************************************************
 *
 *  Copyright (c) 2006-2007  Broadcom Corporation
 *  All Rights Reserved
 *
 * <:label-BRCM:2011:DUAL/GPL:standard
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation (the "GPL").
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * 
 * A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
 * writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 *:>
 *
 ************************************************************************/


#ifndef __SKB_DEFINES_H__
#define __SKB_DEFINES_H__


/*!\file skb_defines.h
 * \brief Header file contains constants and macros used to set or
 * get various bit fields defined for skb->mark.
 *
 */

/* queue = mark[4:0] */
#define SKBMARK_Q_S             0
#define SKBMARK_Q_M             (0x1F << SKBMARK_Q_S)
#define SKBMARK_GET_Q(MARK)     ((MARK & SKBMARK_Q_M) >> SKBMARK_Q_S)
#define SKBMARK_SET_Q(MARK, Q)  ((MARK & ~SKBMARK_Q_M) | (Q << SKBMARK_Q_S))
//*****************************************************************************
#if 1 //__ZYXEL__, Mark
/* dhcp policy route [6:5] */
#define SKBMARK_DHCP_POLICYRT_ID_S         5
#define SKBMARK_DHCP_POLICYRT_ID_M         (0x03 << SKBMARK_DHCP_POLICYRT_ID_S)
#define SKBMARK_GET_DHCP_POLICYRT_ID(MARK) ((MARK & SKBMARK_DHCP_POLICYRT_ID_M) >> SKBMARK_DHCP_POLICYRT_ID_S)
#define SKBMARK_SET_DHCP_POLICYRT_ID(MARK, DP) \
    ((MARK & ~SKBMARK_DHCP_POLICYRT_ID_M) | (DP << SKBMARK_DHCP_POLICYRT_ID_S))	
/* qos policy route [10:7] */
#define SKBMARK_QOS_POLICYRT_ID_S         7
#define SKBMARK_QOS_POLICYRT_ID_M         (0x0F << SKBMARK_QOS_POLICYRT_ID_S)
#define SKBMARK_GET_QOS_POLICYRT_ID(MARK) ((MARK & SKBMARK_QOS_POLICYRT_ID_M) >> SKBMARK_QOS_POLICYRT_ID_S)
#define SKBMARK_SET_QOS_POLICYRT_ID(MARK, QOP) \
    ((MARK & ~SKBMARK_QOS_POLICYRT_ID_M) | (QOP << SKBMARK_QOS_POLICYRT_ID_S))
/* traffic_class_id = mark[10:5] */
#define SKBMARK_TC_ID_S         5
#define SKBMARK_TC_ID_M         (0x3F << SKBMARK_TC_ID_S)
#define SKBMARK_GET_TC_ID(MARK) ((MARK & SKBMARK_TC_ID_M) >> SKBMARK_TC_ID_S)
#define SKBMARK_SET_TC_ID(MARK, TC) \
    ((MARK & ~SKBMARK_TC_ID_M) | (TC << SKBMARK_TC_ID_S))
/* flow_id = mark[15:11] */
#define SKBMARK_FLOW_ID_S       11
#define SKBMARK_FLOW_ID_M       (0x1F << SKBMARK_FLOW_ID_S)
#define SKBMARK_GET_FLOW_ID(MARK) \
    ((MARK & SKBMARK_FLOW_ID_M) >> SKBMARK_FLOW_ID_S)
#define SKBMARK_SET_FLOW_ID(MARK, FLOW) \
    ((MARK & ~SKBMARK_FLOW_ID_M) | (FLOW << SKBMARK_FLOW_ID_S))	
/* Policy Forwarding = mark[18:16] */
#define SKBMARK_POLICYFD_ID_S       16
#define SKBMARK_POLICYFD_ID_M       (0x07 << SKBMARK_POLICYFD_ID_S)
#define SKBMARK_GET_POLICYFD_ID(MARK) \
    ((MARK & SKBMARK_POLICYFD_ID_M) >> SKBMARK_POLICYFD_ID_S)
#define SKBMARK_SET_POLICYFD_ID(MARK, PFD) \
    ((MARK & ~SKBMARK_POLICYFD_ID_M) | (PFD << SKBMARK_POLICYFD_ID_S))	
#else
/* traffic_class_id = mark[10:5] */
#define SKBMARK_TC_ID_S         5
#define SKBMARK_TC_ID_M         (0x3F << SKBMARK_TC_ID_S)
#define SKBMARK_GET_TC_ID(MARK) ((MARK & SKBMARK_TC_ID_M) >> SKBMARK_TC_ID_S)
#define SKBMARK_SET_TC_ID(MARK, TC) \
    ((MARK & ~SKBMARK_TC_ID_M) | (TC << SKBMARK_TC_ID_S))
/* flow_id = mark[18:11] */
#define SKBMARK_FLOW_ID_S       11
#define SKBMARK_FLOW_ID_M       (0xFF << SKBMARK_FLOW_ID_S)
#define SKBMARK_GET_FLOW_ID(MARK) \
    ((MARK & SKBMARK_FLOW_ID_M) >> SKBMARK_FLOW_ID_S)
#define SKBMARK_SET_FLOW_ID(MARK, FLOW) \
    ((MARK & ~SKBMARK_FLOW_ID_M) | (FLOW << SKBMARK_FLOW_ID_S))
#endif
//*****************************************************************************
/* iq_prio = mark[19]; for Ingress QoS used when TX is WLAN */
#define SKBMARK_IQPRIO_MARK_S    19
#define SKBMARK_IQPRIO_MARK_M    (0x01 << SKBMARK_IQPRIO_MARK_S)
#define SKBMARK_GET_IQPRIO_MARK(MARK) \
    ((MARK & SKBMARK_IQPRIO_MARK_M) >> SKBMARK_IQPRIO_MARK_S)
#define SKBMARK_SET_IQPRIO_MARK(MARK, IQPRIO_MARK) \
    ((MARK & ~SKBMARK_IQPRIO_MARK_M) | (IQPRIO_MARK << SKBMARK_IQPRIO_MARK_S))
#if 1 //__MSTC__, Eason, use gpon mark values for other feature
#if 1 //__MSTC__, KuanJung, Change the bits used for Qos vlan operation. Original bits and WMM overlap
/* VID_ACTION = mark[21:20] */
#define SKBMARK_VID_ACTION_S       20
#define SKBMARK_VID_ACTION_M       (0x03 << SKBMARK_VID_ACTION_S)
#define SKBMARK_GET_VID_ACTION(MARK) \
    ((MARK & SKBMARK_VID_ACTION_M) >> SKBMARK_VID_ACTION_S)
#define SKBMARK_SET_VID_ACTION(MARK, FLOW) \
    ((MARK & ~SKBMARK_VID_ACTION_M) | (FLOW << SKBMARK_VID_ACTION_S))
#endif
#if 1 //__MSTC__, Eric, Change the bits used for Qos policer.
/* POLICER = mark[23:22] */
#define SKBMARK_POLICER_S       22
#define SKBMARK_POLICER_M       (0x03 << SKBMARK_POLICER_S)
#define SKBMARK_GET_POLICER(MARK) \
    ((MARK & SKBMARK_POLICER_M) >> SKBMARK_POLICER_S)
#define SKBMARK_SET_POLICER(MARK, FLOW) \
    ((MARK & ~SKBMARK_POLICER_M) | (FLOW << SKBMARK_POLICER_S))
#endif
/* service route = mark[25:24]; for service route*/
#define SKBMARK_SERVICEROUTE_S          24
#define SKBMARK_SERVICEROUTE_M          (0x03 << SKBMARK_SERVICEROUTE_S)
#define SKBMARK_GET_SERVICEROUTE(MARK) \
    ((MARK & SKBMARK_SERVICEROUTE_M) >> SKBMARK_SERVICEROUTE_S)
#define SKBMARK_SET_SERVICEROUTE(MARK, SERVICEROUTE) \
    ((MARK & ~SKBMARK_SERVICEROUTE_M) | (PORT << SKBMARK_SERVICEROUTE_S))
/*custom feature = mark[26:26]; for custom feature*/
#define SKBMARK_CUSTOM_S          26
#define SKBMARK_CUSTOM_M          (0x01 << SKBMARK_CUSTOM_S)
#define SKBMARK_GET_CUSTOM(MARK) \
    ((MARK & SKBMARK_CUSTOM_S) >> SKBMARK_CUSTOM_S)
#define SKBMARK_SET_CUSTOM(MARK, CUSTOM) \
    ((MARK & ~SKBMARK_CUSTOM_S) | (CUSTOM << SKBMARK_CUSTOM_S))

#define SKBMARK_CUSTOM_WANDEFTAG  SKBMARK_SET_CUSTOM(0, 1) //QoS default WAN tagging


//keep the original setting for gpon preventing undefine reference
/* port = mark[26:20]; for enet driver of gpon port, this is gem_id */
#define SKBMARK_PORT_S          20
#define SKBMARK_PORT_M          (0x7F << SKBMARK_PORT_S)
#define SKBMARK_GET_PORT(MARK) \
    ((MARK & SKBMARK_PORT_M) >> SKBMARK_PORT_S)
#define SKBMARK_SET_PORT(MARK, PORT) \
    ((MARK & ~SKBMARK_PORT_M) | (PORT << SKBMARK_PORT_S))

#else
/* port = mark[26:20]; for enet driver of gpon port, this is gem_id */
#define SKBMARK_PORT_S          20
#define SKBMARK_PORT_M          (0x7F << SKBMARK_PORT_S)
#define SKBMARK_GET_PORT(MARK) \
    ((MARK & SKBMARK_PORT_M) >> SKBMARK_PORT_S)
#define SKBMARK_SET_PORT(MARK, PORT) \
    ((MARK & ~SKBMARK_PORT_M) | (PORT << SKBMARK_PORT_S))
#endif    
/* iffwan_mark = mark[27] --  BRCM defined-- */
#define SKBMARK_IFFWAN_MARK_S    27
#define SKBMARK_IFFWAN_MARK_M    (0x01 << SKBMARK_IFFWAN_MARK_S)
#define SKBMARK_GET_IFFWAN_MARK(MARK) \
    ((MARK & SKBMARK_IFFWAN_MARK_M) >> SKBMARK_IFFWAN_MARK_S)
#define SKBMARK_SET_IFFWAN_MARK(MARK, IFFWAN_MARK) \
    ((MARK & ~SKBMARK_IFFWAN_MARK_M) | (IFFWAN_MARK << SKBMARK_IFFWAN_MARK_S))
/* ipsec_mark = mark[28] */
#define SKBMARK_IPSEC_MARK_S    28
#define SKBMARK_IPSEC_MARK_M    (0x01 << SKBMARK_IPSEC_MARK_S)
#define SKBMARK_GET_IPSEC_MARK(MARK) \
    ((MARK & SKBMARK_IPSEC_MARK_M) >> SKBMARK_IPSEC_MARK_S)
#define SKBMARK_SET_IPSEC_MARK(MARK, IPSEC_MARK) \
    ((MARK & ~SKBMARK_IPSEC_MARK_M) | (IPSEC_MARK << SKBMARK_IPSEC_MARK_S))
/* policy_routing = mark[31:29] */
#define SKBMARK_POLICY_RTNG_S   29
#define SKBMARK_POLICY_RTNG_M   (0x07 << SKBMARK_POLICY_RTNG_S)
#define SKBMARK_GET_POLICY_RTNG(MARK)  \
    ((MARK & SKBMARK_POLICY_RTNG_M) >> SKBMARK_POLICY_RTNG_S)
#define SKBMARK_SET_POLICY_RTNG(MARK, POLICY) \
    ((MARK & ~SKBMARK_POLICY_RTNG_M) | (POLICY << SKBMARK_POLICY_RTNG_S))

#endif /* __SKB_DEFINES_H__ */
