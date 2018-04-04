/*
 * Declarations for Broadcom PHY core tables,
 * Networking Adapter Device Driver.
 *
 * THIS IS A GENERATED FILE - DO NOT EDIT
 * Generated on Tue Aug 16 16:22:54 PDT 2011
 *
 * Copyright(c) 2007 Broadcom Corp.
 * All Rights Reserved.
 *
 * $Id: wlc_phytbl_ac.h 365817 2012-10-31 03:42:31Z $
 */
/* FILE-CSTYLED */

#include <wlc_phy_ac.h>

typedef phytbl_info_t acphytbl_info_t;


extern CONST acphytbl_info_t acphytbl_info_rev0[];
extern CONST uint32 acphytbl_info_sz_rev0;
extern CONST acphytbl_info_t acphytbl_info_rev2[];
extern CONST uint32 acphytbl_info_sz_rev2;
extern chan_info_radio2069_t chan_tuning_2069rev3[77];
extern chan_info_radio2069_t chan_tuning_2069rev4[77];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_16_17[77];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_16_17_40[77];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_18[77];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_18_40[77];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_GE16_lp[77];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_GE16_40_lp[77];
#if defined(BCMDBG)
#if defined(DBG_PHY_IOV)
extern radio_20xx_dumpregs_t dumpregs_2069_rev0[];
extern radio_20xx_dumpregs_t dumpregs_2069_rev16[];
extern radio_20xx_dumpregs_t dumpregs_2069_rev17[];
#endif
#endif
extern radio_20xx_prefregs_t prefregs_2069_rev3[];
extern radio_20xx_prefregs_t prefregs_2069_rev4[];
extern radio_20xx_prefregs_t prefregs_2069_rev16[];
extern radio_20xx_prefregs_t prefregs_2069_rev17[];
extern radio_20xx_prefregs_t prefregs_2069_rev18[];
extern radio_20xx_prefregs_t prefregs_2069_rev23[];
extern radio_20xx_prefregs_t prefregs_2069_rev24[];
extern radio_20xx_prefregs_t prefregs_2069_rev25[];
extern radio_20xx_prefregs_t prefregs_2069_rev26[];

extern uint16 ovr_regs_2069_rev2[];
extern uint16 ovr_regs_2069_rev16[];
extern chan_info_rx_farrow rx_farrow_tbl[ACPHY_NUM_BW][ACPHY_NUM_CHANS];
extern chan_info_tx_farrow tx_farrow_dac1_tbl[ACPHY_NUM_BW][ACPHY_NUM_CHANS];
#ifndef ACPHY_1X1_ONLY 
extern chan_info_tx_farrow tx_farrow_dac2_tbl[ACPHY_NUM_BW][ACPHY_NUM_CHANS];
extern chan_info_tx_farrow tx_farrow_dac3_tbl[ACPHY_NUM_BW][ACPHY_NUM_CHANS];
#endif
extern uint16 acphy_txgain_epa_2g_2069rev0[];
extern uint16 acphy_txgain_epa_5g_2069rev0[];
extern uint16 acphy_txgain_ipa_2g_2069rev0[];
extern uint16 acphy_txgain_ipa_5g_2069rev0[];

extern uint16 acphy_txgain_ipa_2g_2069rev16[];

extern uint16 acphy_txgain_epa_2g_2069rev17[];
extern uint16 acphy_txgain_epa_5g_2069rev17[];
extern uint16 acphy_txgain_ipa_2g_2069rev17[];
extern uint16 acphy_txgain_ipa_5g_2069rev17[];
extern uint16 acphy_txgain_epa_2g_2069rev18[];
extern uint16 acphy_txgain_epa_5g_2069rev18[];

#ifndef ACPHY_1X1_ONLY
extern uint16 acphy_txgain_epa_2g_2069rev4[];
extern uint16 acphy_txgain_epa_2g_2069rev4_id1[];
extern uint16 acphy_txgain_epa_5g_2069rev4[];
extern uint16 acphy_txgain_epa_2g_2069rev16[];
extern uint16 acphy_txgain_epa_5g_2069rev16[];
extern uint16 acphy_txgain_ipa_2g_2069rev18[];
extern uint16 acphy_txgain_ipa_5g_2069rev16[];
extern uint16 acphy_txgain_ipa_5g_2069rev18[];
#endif
extern uint32 acphy_txv_for_spexp[];
