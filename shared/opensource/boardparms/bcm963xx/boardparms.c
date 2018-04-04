/*
    Copyright 2000-2010 Broadcom Corporation

    <:label-BRCM:2011:DUAL/GPL:standard
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2, as published by
    the Free Software Foundation (the "GPL").
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    
    A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
    writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
    
    :>
*/

/**************************************************************************
* File Name  : boardparms.c
*
* Description: This file contains the implementation for the BCM63xx board
*              parameter access functions.
*
* Updates    : 07/14/2003  Created.
***************************************************************************/

/* Includes. */
#include "boardparms.h"
#if 0 //__MSTC__, Delon Yu
#include "bcmSpiRes.h"
#endif

/* Typedefs */

/*
You can add a new enum entry anywhere in the enum list below.
Then, you can use the enums in the board parm arrays, with the following restrictions:
-An API must be created for accessing any data from the array except for bp_elemTemplate.
-bp_cpBoardId  must be the first element of the array.
-bp_elemTemplate if used should be the last or second to last (just before bp_last) element
 of the array
-Most enums that are used only once in the array can be placed anywhere after bp_cpBoardId.
-These enums are read with the BpGetUc/BpGetUs/BpGetUl private functions.
-If there is a board parm array that is very similar to one you are adding, then you
 can use the bp_elemTemplate to point to that the board parm array BUT the restriction
 below still applies - that is do not split up the "groups"
 (see g_bcm96362advngr and g_bcm96362advngr2 or g_bcm963168vx and g_bcm963168vx_p300)
-Certain enums can appear multiple times in the board parm arrays
-These enums have special APIs that understand how to loop through each occurence
 They are:
 *packet switch related parameters (from bp_ucPhyAddress to bp_ulPhyId7) which can
  appear once per packet switch (bp_ucPhyType0 and bp_ucPhyType1)
 *led info related parameters (bp_usDuplexLed, bp_usSpeedLed100 and bp_usSpeedLed1000) which can
  appear once per internal led (bp_ulPhyId0 and bp_ulPhyId1)
 *voip dsp related parameters (from bp_ucDspAddress to bp_usGpioDectLed) which can
  appear once per dsp (bp_ucDspType0 and bp_ucDspType1)
*/

#define bp_usGpioOverlay     bp_ulGpioOverlay

enum bp_id {
  bp_cpBoardId,
  bp_cpComment,
  bp_ulGpioOverlay,
  bp_usGpioLedAdsl,
  bp_usGpioLedAdslFail,
  bp_usGpioSecLedAdsl,
  bp_usGpioSecLedAdslFail,
#if 1 //__MSTC__, RaynorChung: Support VDSL LED
  bp_usGpioLedVdsl,
  bp_usGpioLedVdslFail,
  bp_usGpioSecLedVdsl,
  bp_usGpioSecLedVdslFail,
#endif
  bp_usGpioLedSesWireless,
  bp_usGpioLedWanData,
#if 1 // __MSTC__, Richard Huang
  bp_usGpioLedInternetData,
#endif
  bp_usGpioLedWirelessFail,
  bp_usGpioLedTV,
  bp_usGpioLedGPHY,
  bp_usGpioLedWanError,
  bp_usGpioLedBlPowerOn,
  bp_usGpioLedBlStop,
  bp_usGpioFpgaReset,
  bp_usGpioLedGpon,
  bp_usGpioLedGponFail,
  bp_usGpioLedMoCA,
  bp_usGpioLedMoCAFail,
#if 1 //__MSTC__, RaynorChung: Support USB LED
  bp_usGpioLedUSB0,
  bp_usGpioLedUSB0Fail,
  bp_usGpioLedUSB1,
  bp_usGpioLedUSB1Fail,
#endif
#if 1 //__MSTC__, Paul Ho, Wireless On/Off Button
  bp_usGpioWirelessBtn,
#endif
  bp_usGpioLedETH0,
  bp_usGpioLedETH1,
  bp_usGpioLedETH2,
  bp_usGpioLedETH3,
  bp_usExtIntrResetToDefault,
  bp_usExtIntrSesBtnWireless,
#if 1 //__MSTC__, Eason, WLAN_WPS_BTN_SEPARATE
  bp_usExtIntrWlanEnableBtnWireless,
#endif
  bp_usAntInUseWireless,
  bp_usWirelessFlags,
  bp_usGpioWirelessPowerDown,
  bp_ucPhyType0,
  bp_ucPhyType1,
  bp_ucPhyAddress,
  bp_usConfigType,
  bp_ulPortMap,
  bp_ulPhyId0,
  bp_ulPhyId1,
  bp_ulPhyId2,
  bp_ulPhyId3,
  bp_ulPhyId4,
  bp_ulPhyId5,
  bp_ulPhyId6,
  bp_ulPhyId7,
  bp_usGpioLedEphy0Duplex,
  bp_usGpioLedEphy1Duplex,
  bp_usGpioLedEphy2Duplex,
  bp_usGpioLedGphyDuplex,
  bp_usGpioLedEphy0Act,
  bp_usGpioLedEphy1Act,
  bp_usGpioLedEphy2Act,
  bp_usGpioLedGphyAct,
  bp_usGpioLedEphy0Speed,
  bp_usGpioLedEphy1Speed,
  bp_usGpioLedEphy2Speed,
  bp_usGpioLedGphySpeed,
  bp_usGpioLedDslLink,
  bp_usDuplexLed,
  bp_usSpeedLed100,
  bp_usSpeedLed1000,
  bp_usPhyConnType,
  bp_ucPhyDevName,
  bp_ucDspType0,
  bp_ucDspType1,
  bp_ucDspAddress,
  bp_usGpioLedVoip,
  bp_usGpioVoip1Led,
  bp_usGpioVoip1LedFail,
  bp_usGpioVoip2Led,
  bp_usGpioVoip2LedFail,
/*MWI LED*/
  bp_usGpioPhone1MWILed,
  bp_usGpioPhone2MWILed,
  bp_usGpioGWAN100MLed,
  bp_usGpioGWAN1000MLed,
  bp_usGpioPotsLed,
  bp_usGpioDectLed,
  bp_usGpioPassDyingGasp,
  bp_ulAfeId0,
  bp_ulAfeId1,
  bp_usGpioExtAFEReset,
  bp_usGpioExtAFELDPwr,
  bp_usGpioExtAFELDMode,
  bp_usGpioIntAFELDPwr,
  bp_usGpioIntAFELDMode,
  bp_usGpioAFELDRelay,
  bp_usGpioUart2Sdin,
  bp_usGpioUart2Sdout,
  bp_usGpioLaserDis,
  bp_usGpioLaserTxPwrEn,
  bp_usVregSel1P2,
  bp_usGponOpticsType,
  bp_usGpioFemtoReset,
  bp_cpDefaultOpticalParams,
  bp_usEphyBaseAddress,
  bp_usGpioSpiSlaveReset,  
  bp_usSpiSlaveBusNum,  
  bp_usSpiSlaveSelectNum,
  bp_usSpiSlaveMode,
  bp_ulSpiSlaveCtrlState,
  bp_ulSpiSlaveMaxFreq,
  bp_usSpiSlaveProtoRev,
  bp_usGpioExtAFELDData,
  bp_usGpioExtAFELDClk,
  bp_elemTemplate,
  bp_usSerialLEDMuxSel,
  bp_usGpioHpnaReset, 
  bp_ulDeviceOptions,
  bp_pPhyInit,
  bp_usGphyBaseAddress,
  bp_usGpioLedWpsFail,
  bp_usGpioLedSysFail,
  bp_usGpioLed5GWps,
  bp_usGpioLed5GWpsFail,
#if 1 /* chchien */
  bp_usGpioLedWanEth3,
#endif    
  bp_last
};

typedef struct bp_elem {
  enum bp_id id;
  union {
    char * cp;
    unsigned char * ucp;
    unsigned char uc;
    unsigned short us;
    unsigned long ul;
    bp_mdio_init_t * phyinitp;
    struct bp_elem * bp_elemp;
  } u;
} bp_elem_t;

/* Variables */
#if 0
/* Sample structure with all elements present in a valid order */
/* Indentation is used to illustrate the groupings where parameters can be repeated */
/* use bp_elemTemplate to use another structure but do not split the groups */   
static bp_elem_t g_sample[] = {
  {bp_cpBoardId,               .u.cp = "SAMPLE"},
  {bp_ulGpioOverlay,           .u.ul = 0;
  {bp_usGpioLedAdsl,           .u.us = 0},
  {bp_usGpioLedAdslFail,       .u.us = 0},
  {bp_usGpioSecLedAdsl,        .u.us = 0},
  {bp_usGpioSecLedAdslFail,    .u.us = 0},
  {bp_usGpioLedSesWireless,    .u.us = 0},
  {bp_usGpioLedWanData,        .u.us = 0},
  {bp_usGpioLedWanError,       .u.us = 0},
  {bp_usGpioLedBlPowerOn,      .u.us = 0},
  {bp_usGpioLedBlStop,         .u.us = 0},
  {bp_usGpioFpgaReset,         .u.us = 0},
  {bp_usGpioLedGpon,           .u.us = 0},
  {bp_usGpioLedGponFail,       .u.us = 0},
  {bp_usGpioLedMoCA,           .u.us = 0},
  {bp_usGpioLedMoCAFail,       .u.us = 0},
  {bp_usExtIntrResetToDefault, .u.us = 0},
  {bp_usExtIntrSesBtnWireless, .u.us = 0},
  {bp_usAntInUseWireless,      .u.us = 0},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_usGpioWirelessPowerDown, .u.us = 0},
  {bp_ucPhyType0,              .u.uc = 0}, // First switch
    {bp_ucPhyAddress,          .u.uc = 0},
    {bp_usConfigType,          .u.us = 0},
    {bp_ulPortMap,             .u.ul = 0},
    {bp_ulPhyId0,              .u.ul = 0},
      {bp_usDuplexLed,         .u.us = 0},
      {bp_usSpeedLed100,       .u.us = 0},
      {bp_usSpeedLed1000,      .u.us = 0},
    {bp_ulPhyId1,              .u.ul = 0},
      {bp_usDuplexLed,         .u.us = 0},
      {bp_usSpeedLed100,       .u.us = 0},
      {bp_usSpeedLed1000,      .u.us = 0},
    {bp_ulPhyId2,              .u.ul = 0},
    {bp_ulPhyId3,              .u.ul = 0},
    {bp_ulPhyId4,              .u.ul = 0},
    {bp_ulPhyId5,              .u.ul = 0},
    {bp_ulPhyId6,              .u.ul = 0},
    {bp_ulPhyId7,              .u.ul = 0},
  {bp_ucPhyType1,              .u.uc = 0}, // Second switch
    {bp_ucPhyAddress,          .u.uc = 0},
    {bp_usConfigType,          .u.us = 0},
    {bp_ulPortMap,             .u.ul = 0},
    {bp_ulPhyId0,              .u.ul = 0},
      {bp_usDuplexLed,         .u.us = 0},
      {bp_usSpeedLed100,       .u.us = 0},
      {bp_usSpeedLed1000,      .u.us = 0},
    {bp_ulPhyId1,              .u.ul = 0},
      {bp_usDuplexLed,         .u.us = 0},
      {bp_usSpeedLed100,       .u.us = 0},
      {bp_usSpeedLed1000,      .u.us = 0},
    {bp_ulPhyId2,              .u.ul = 0},
    {bp_ulPhyId3,              .u.ul = 0},
    {bp_ulPhyId4,              .u.ul = 0},
    {bp_ulPhyId5,              .u.ul = 0},
    {bp_ulPhyId6,              .u.ul = 0},
    {bp_ulPhyId7,              .u.ul = 0},
  {bp_ucDspType0,              .u.uc = 0}, // First VOIP DSP
    {bp_ucDspAddress,          .u.uc = 0},
    {bp_usGpioLedVoip,         .u.us = 0},
    {bp_usGpioVoip1Led,        .u.us = 0},
    {bp_usGpioVoip1LedFail,    .u.us = 0},
    {bp_usGpioVoip2Led,        .u.us = 0},
    {bp_usGpioVoip2LedFail,    .u.us = 0},
    {bp_usGpioPotsLed,         .u.us = 0},
    {bp_usGpioDectLed,         .u.us = 0},
  {bp_ucDspType1,              .u.uc = 0}, // Second VOIP DSP
    {bp_ucDspAddress,          .u.uc = 0},
    {bp_usGpioLedVoip,         .u.us = 0},
    {bp_usGpioVoip1Led,        .u.us = 0},
    {bp_usGpioVoip1LedFail,    .u.us = 0},
    {bp_usGpioVoip2Led,        .u.us = 0},
    {bp_usGpioVoip2LedFail,    .u.us = 0},
    {bp_usGpioPotsLed,         .u.us = 0},
    {bp_usGpioDectLed,         .u.us = 0},
  {bp_ulAfeId0,                .u.ul = 0},
  {bp_ulAfeId1,                .u.ul = 0},
  {bp_usGpioExtAFEReset,       .u.us = 0},
  {bp_usGpioExtAFELDPwr,       .u.us = 0},
  {bp_usGpioExtAFELDMode,      .u.us = 0},
  {bp_usGpioLaserDis,          .u.us = 0},
  {bp_usGpioLaserTxPwrEn,      .u.us = 0},
  {bp_usVregSel1P2,            .u.us = 0},
  {bp_usGpioFemtoReset,        .u.us = 0},
  {bp_elemTemplate             .u.bp_elemp = g_sample2},
  {bp_usSerialLEDMuxSel        .u.us = 0},
  {bp_last}
};

#endif

#if defined(_BCM96362_) || defined(CONFIG_BCM96362)

static bp_elem_t g_bcm96362advnx[] = {
  {bp_cpBoardId,               .u.cp = "96362ADVNX"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_15_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_10_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_12_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_13_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usWirelessFlags,         .u.us = BP_WLAN_EXCLUDE_ONBOARD},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_9_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_1},
  {bp_last}
};

static bp_elem_t g_bcm96362advngr[] = {
  {bp_cpBoardId,               .u.cp = "96362ADVNgr"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_SPI_SSB3_EXT_CS |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_15_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_10_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_12_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_13_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usWirelessFlags,         .u.us = BP_WLAN_EXCLUDE_ONBOARD},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x3f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18},
  {bp_ulPhyId5,                .u.ul = 0x19},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_9_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_1},
  {bp_last}
};

static bp_elem_t g_bcm96362advngr2[] = {
  {bp_cpBoardId,               .u.cp = "96362ADVNgr2"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_SPI_SSB3_EXT_CS |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_9_AL},
  {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_11_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6302_REV_5_2_2},
  {bp_elemTemplate,            .u.bp_elemp = g_bcm96362advngr},
  {bp_last}
};

static bp_elem_t g_bcm963629epon[] = {
  {bp_cpBoardId,               .u.cp = "963629epon"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_15_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_12_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_13_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usWirelessFlags,         .u.us = BP_WLAN_EXCLUDE_ONBOARD},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18 | BCM_WAN_PORT | CONNECTED_TO_EPON_MAC},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_9_AL},
  {bp_last}
};

static bp_elem_t g_bcm96362advn2xh[] = {
  {bp_cpBoardId,               .u.cp = "96362ADVN2xh"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_SPI_SSB2_EXT_CS |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_15_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_10_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_12_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_13_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usWirelessFlags,         .u.us = BP_WLAN_EXCLUDE_ONBOARD},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x10},
  {bp_ulPhyId4,                .u.ul = RGMII_DIRECT},
  {bp_ucPhyType1,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_SPI_SSB_2},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x00},
  {bp_ulPhyId1,                .u.ul = 0x01},
  {bp_ulPhyId2,                .u.ul = 0x02},
  {bp_ulPhyId3,                .u.ul = 0x03},
  {bp_ulPhyId4,                .u.ul = 0x04},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_9_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_1},
  {bp_last}
};

static bp_elem_t g_bcm96362advn2xm[] = {
  {bp_cpBoardId,               .u.cp = "96362ADVN2XM"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_SPI_SSB2_EXT_CS |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_15_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_10_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_12_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_13_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usWirelessFlags,         .u.us = BP_WLAN_EXCLUDE_ONBOARD},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x10},
  {bp_ulPhyId4,                .u.ul = RGMII_DIRECT},
  {bp_ucPhyType1,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_SPI_SSB_2},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x00},
  {bp_ulPhyId1,                .u.ul = 0x01},
  {bp_ulPhyId2,                .u.ul = 0x02},
  {bp_ulPhyId3,                .u.ul = 0x03},
  {bp_ulPhyId4,                .u.ul = 0x04},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_9_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_1},
  {bp_last}
};


static bp_elem_t g_bcm96361XF[] = {
  {bp_cpBoardId,               .u.cp = "96361XF"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_15_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_10_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_12_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_13_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usWirelessFlags,         .u.us = BP_WLAN_EXCLUDE_ONBOARD},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = MII_DIRECT},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_9_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6302_REV_5_2_2},
  {bp_usVregSel1P2,            .u.us = 0x13},
  {bp_usGpioFemtoReset,        .u.us = BP_GPIO_35_AH},
  {bp_last}
};

static bp_elem_t g_bcm96361I2[] = {
  {bp_cpBoardId ,              .u.cp = "96361I2"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_15_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_10_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_12_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_13_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usWirelessFlags,         .u.us = BP_WLAN_EXCLUDE_ONBOARD},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x3f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = BP_PHY_ID_24 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ulPhyId5,                .u.ul = BP_PHY_ID_25 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_9_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_1},
  {bp_last}
};

static bp_elem_t g_bcm96362rpvt[] = {
  {bp_cpBoardId ,              .u.cp = "96362RPVT"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_15_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_10_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_12_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_13_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usWirelessFlags,         .u.us = BP_WLAN_EXCLUDE_ONBOARD},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x3f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ulPhyId5,                .u.ul = 0x19 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_9_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6302_REV_5_2_2}, 
  {bp_last}
};

static bp_elem_t g_bcm96362rpvt_2u[] = {
  {bp_cpBoardId ,              .u.cp = "96362RPVT_2U"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_15_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_10_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_12_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_13_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usWirelessFlags,         .u.us = BP_WLAN_EXCLUDE_ONBOARD},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x3f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ulPhyId5,                .u.ul = 0x19 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_9_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6302_REV_5_2_2}, 
  {bp_last}
};

static bp_elem_t g_bcm96362ravngr2[] = {
  {bp_cpBoardId,               .u.cp = "96362RAVNGR2"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_15_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_10_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_12_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_13_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usWirelessFlags,         .u.us = BP_WLAN_EXCLUDE_ONBOARD},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_9_AL},
  {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_11_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6302_REV_5_2_2},
  {bp_last}
};

static bp_elem_t g_bcm96362radvnxh5[] = {
  {bp_cpBoardId,               .u.cp = "96362RADVNXH5"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_SPI_SSB2_EXT_CS |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_11_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6302_REV_5_2_2},
  {bp_elemTemplate,            .u.bp_elemp = g_bcm96362advn2xh},
  {bp_last}
};

static bp_elem_t g_bcm96362radvn2xh[] = {
  {bp_cpBoardId,               .u.cp = "96362RADVN2XH"},
  {bp_elemTemplate,            .u.bp_elemp = g_bcm96362radvnxh5},
  {bp_last}
};

static bp_elem_t * g_BoardParms[] = {g_bcm96362advnx, g_bcm96362advngr, g_bcm96362advngr2, g_bcm963629epon, g_bcm96362advn2xh, g_bcm96361XF, g_bcm96361I2, g_bcm96362rpvt, g_bcm96362rpvt_2u, g_bcm96362ravngr2, g_bcm96362advn2xm, g_bcm96362radvnxh5, g_bcm96362radvn2xh, 0};
#endif

#if defined(_BCM96368_) || defined(CONFIG_BCM96368)

static bp_elem_t g_bcm96368vvw[] = {
  {bp_cpBoardId,               .u.cp = "96368VVW"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_USB_LED)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_24_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_33_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_0_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_1_AH},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_ISIL1556 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_ISIL_REV1},
  {bp_last}
};

static bp_elem_t g_bcm96368vvwb[] = {
  {bp_cpBoardId,               .u.cp = "96368VVWB"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_USB_LED)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_24_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_33_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_0_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_1_AH},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_ISIL1556 | BP_AFE_FE_ANNEXB | BP_AFE_FE_REV_ISIL_REV1},
  {bp_last}
};

static bp_elem_t g_bcm96368ntr[] = {
  {bp_cpBoardId,               .u.cp = "96368NTR"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_33_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_0_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_1_AH},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_GPIO_25_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_GPIO_26_AL},
  {bp_usGpioPotsLed,           .u.us = BP_GPIO_27_AL},
  {bp_last}
};

static bp_elem_t g_bcm96368sv2[] = {
  {bp_cpBoardId,               .u.cp = "96368SV2"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_USB_LED)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_33_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_30_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_31_AH},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x3f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x12},
  {bp_ulPhyId5,                .u.ul = 0x11},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_GPIO_25_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_GPIO_26_AL},
  {bp_usGpioPotsLed,           .u.us = BP_GPIO_27_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_ISIL1556 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_ISIL_REV1},
  {bp_last}
};

static bp_elem_t g_bcm96368mvwg[] = {
  {bp_cpBoardId,               .u.cp = "96368MVWG"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_USB_LED)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_27_AL}, /*__MSTC__, Paul Ho, WPS Led */
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_31_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_22_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_24_AH},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_5}, /*__MSTC__, Paul Ho, WPS Button */
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x36},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId4,                .u.ul = 0x12},
  {bp_ulPhyId5,                .u.ul = 0x11},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_last}
};

static bp_elem_t g_bcm96368mvwgb[] = {
  {bp_cpBoardId,               .u.cp = "96368MVWGB"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_USB_LED)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_22_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_31_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_22_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_24_AH},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x36},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId4,                .u.ul = 0x12},
  {bp_ulPhyId5,                .u.ul = 0x11},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXB | BP_AFE_FE_REV_6302_REV1},
  {bp_last}
};

static bp_elem_t g_bcm96368mvwgj[] = {
  {bp_cpBoardId,               .u.cp = "96368MVWGJ"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_USB_LED)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_22_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_31_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_22_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_24_AH},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x36},
  {bp_ulPhyId0,                .u.ul = 0x00},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x00},
  {bp_ulPhyId4,                .u.ul = 0x12},
  {bp_ulPhyId5,                .u.ul = 0x11},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXJ | BP_AFE_FE_REV_6302_REV1},
  {bp_last}
};

static bp_elem_t g_bcm96368mb2g[] = {
  {bp_cpBoardId,               .u.cp = "96368MB2G"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioSecLedAdsl,        .u.us = BP_GPIO_27_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_31_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_22_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_24_AH},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_ulAfeId1,                .u.ul = BP_AFE_CHIP_6306 | BP_AFE_LD_ISIL1556 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_ISIL_REV1},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_35_AL},
  {bp_last}
};

static bp_elem_t g_bcm96368mbg6b[] = {
  {bp_cpBoardId,               .u.cp = "96368MBG6b"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioSecLedAdsl,        .u.us = BP_GPIO_27_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_31_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_22_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_24_AH},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_6306 | BP_AFE_LD_ISIL1556 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_ISIL_REV1},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_35_AL},
  {bp_last}
};

static bp_elem_t g_bcm96368mbg6302[] = {
  {bp_cpBoardId,               .u.cp = "96368MBG6302"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioSecLedAdsl,        .u.us = BP_GPIO_27_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_31_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_22_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_24_AH},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_ulAfeId1,                .u.ul = BP_AFE_CHIP_6306 | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_35_AL},
  {bp_usGpioExtAFELDPwr,       .u.us = BP_GPIO_37_AH},
  {bp_usGpioExtAFELDMode,      .u.us = BP_GPIO_36_AH},
  {bp_last}
};

static bp_elem_t g_bcm96368mng[] = {
  {bp_cpBoardId,               .u.cp = "96368MNG"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_31_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_22_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_24_AH},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x20},
  {bp_ulPhyId5,                .u.ul = 0xff},
  {bp_ucPhyType1,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_SPI_SSB_1},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x00},
  {bp_ulPhyId1,                .u.ul = 0x01},
  {bp_ulPhyId2,                .u.ul = 0x02},
  {bp_ulPhyId3,                .u.ul = 0x03},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_6306 | BP_AFE_LD_ISIL1556 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_ISIL_REV1},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_37_AL},
  {bp_last}
};

static bp_elem_t g_bcm96367avng[] = {
  {bp_cpBoardId,               .u.cp = "96367AVNG"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_USB_LED)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_23_AL},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_31_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_22_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_24_AH},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x2f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId5,                .u.ul = 0x11},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_last}
};

static bp_elem_t g_bcm96368mvngr[] = {
  {bp_cpBoardId,               .u.cp = "96368MVNgr"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_SPI_EXT_CS |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_USB_LED)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_23_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_3_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_22_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_24_AH},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_last}
};

static bp_elem_t g_bcm96368rmvng_nand[] = {
  {bp_cpBoardId,               .u.cp = "96368RMVNg NAND"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_SPI_EXT_CS |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_USB_LED)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_23_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_3_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_22_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_24_AH},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x03},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_last}
};

static bp_elem_t g_bcm96368rmvng_nor[] = {
  {bp_cpBoardId,               .u.cp = "96368RMVNg NOR"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_SPI_EXT_CS |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_USB_LED)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_23_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_3_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_22_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_24_AH},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x13},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId4,                .u.ul = 0x18},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_last}
};

static bp_elem_t g_bcm96368mvngrP2[] = {
  {bp_cpBoardId,               .u.cp = "96368MVNgrP2"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_USB_LED)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_23_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AL},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_3_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_22_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_24_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_last}
};

static bp_elem_t g_bcm96368Ext[] = {
  {bp_cpBoardId,               .u.cp = "96368EXT"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_USB_LED)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_2_AL},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x03},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_last}
};

static bp_elem_t * g_BoardParms[] =
{g_bcm96368vvw, g_bcm96368mvwg, g_bcm96368sv2, g_bcm96368mb2g,
g_bcm96368ntr, g_bcm96368Ext, g_bcm96368vvwb, g_bcm96368mvwgb,
g_bcm96368mng, g_bcm96368mbg6302, g_bcm96368mvwgj, g_bcm96367avng,
g_bcm96368mvngr, g_bcm96368rmvng_nand, g_bcm96368rmvng_nor, g_bcm96368mvngrP2, g_bcm96368mbg6b, 0};
#endif

#if defined(_BCM96816_) || defined(CONFIG_BCM96816)

static bp_elem_t g_bcm96816sv[] = {
  {bp_cpBoardId,               .u.cp = "96816SV"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCI |
                                       BP_OVERLAY_GPHY_LED_0 |
                                       BP_OVERLAY_GPHY_LED_1 |
                                       BP_OVERLAY_MOCA_LED)},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_3_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_4_AH},
  {bp_usGpioLedGpon,           .u.us = BP_GPIO_8_AH},
  {bp_usGpioLedGponFail,       .u.us = BP_GPIO_16_AH},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_15_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_17_AH},
  {bp_usGpioLedMoCA,           .u.us = BP_GPIO_5_AH},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0xbf},
  {bp_ulPhyId0,                .u.ul = 0x00},
  {bp_ulPhyId1,                .u.ul = 0x01},
  {bp_ulPhyId2,                .u.ul = 0x14},
  {bp_ulPhyId3,                .u.ul = 0x12},
  {bp_ulPhyId4,                .u.ul = 0x18},
  {bp_ulPhyId5,                .u.ul = 0xff},
  {bp_ulPhyId7,                .u.ul = 0xff},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGponOpticsType,        .u.ul = BP_GPON_OPTICS_TYPE_LEGACY},
  {bp_last}
};

static bp_elem_t g_bcm96816pvwm[] = {
  {bp_cpBoardId,               .u.cp = "96816PVWM"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_GPHY_LED_0 |
                                       BP_OVERLAY_GPHY_LED_1 |
                                       BP_OVERLAY_MOCA_LED)},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_3_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_4_AH},
  {bp_usGpioLedGpon,           .u.us = BP_GPIO_8_AH},
  {bp_usGpioLedGponFail,       .u.us = BP_GPIO_2_AH},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_19_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_20_AH},
  {bp_usGpioLedMoCA,           .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedMoCAFail,       .u.us = BP_GPIO_37_AH},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0xbf},
  {bp_ulPhyId0,                .u.ul = 0x00},
  {bp_ulPhyId1,                .u.ul = 0x01},
  {bp_ulPhyId2,                .u.ul = 0x11},
  {bp_ulPhyId3,                .u.ul = 0x12},
  {bp_ulPhyId4,                .u.ul = 0xff | PHYCFG_VALID}, /* WAN interface */
  {bp_ulPhyId5,                .u.ul = 0xff},
  {bp_ulPhyId7,                .u.ul = 0xff},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGponOpticsType,        .u.us = BP_GPON_OPTICS_TYPE_LEGACY},
  {bp_last}
};

#if !defined(_CFE_)
unsigned char _6818RgParams[BP_OPTICAL_PARAMS_LEN] = {0x28,0x03,0x02,0x0B,0x00,0xD8,0x00,0x77,
                                                      0x01,0xB8,0x7F,0x02,0x02,0x32,0x00,0x00,
                                                      0x00,0x34,0xFA,0x00,0x00,0x09,0x00,0x00,
                                                      0x08,0x6F,0xFF,0xFF,0xEF,0xFF,0xFF,0xFF,
                                                      0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,
                                                      0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x01};
#endif

static bp_elem_t g_bcm96818rg[] = {
  {bp_cpBoardId,               .u.cp = "96818RG"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_GPHY_LED_0 |
                                       BP_OVERLAY_GPHY_LED_1 |
                                       BP_OVERLAY_SERIAL_LEDS)},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_11_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_19_AH},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_12_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_0_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_1_AH},
  {bp_usGpioLedGpon,           .u.us = BP_GPIO_8_AH},
  {bp_usGpioLedGponFail,       .u.us = BP_SERIAL_GPIO_6_AH},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x8f},
  {bp_ulPhyId0,                .u.ul = BP_PHY_ID_0},
  {bp_usSpeedLed100,           .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usSpeedLed1000,          .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_ulPhyId1,                .u.ul = BP_PHY_ID_1},
  {bp_usSpeedLed100,           .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usSpeedLed1000,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_ulPhyId2,                .u.ul  = PHY_INTEGRATED_VALID | PHY_EXTERNAL | MAC_IF_RGMII | BP_PHY_ID_24},
  {bp_ulPhyId3,                .u.ul  = PHY_INTEGRATED_VALID | PHY_EXTERNAL | MAC_IF_RGMII | BP_PHY_ID_25},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_8_AL},  
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usGpioLaserDis,          .u.us = BP_GPIO_0_AL},
  {bp_usGpioLaserTxPwrEn,      .u.us = BP_GPIO_5_AL},
  {bp_usGponOpticsType,        .u.us = BP_GPON_OPTICS_TYPE_LEGACY},  
  {bp_last}
};

static bp_elem_t g_bcm96818rg_bosa[] = {
  {bp_cpBoardId,               .u.cp = "96818RG_BOSA"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_GPHY_LED_0 |
                                       BP_OVERLAY_GPHY_LED_1 |
                                       BP_OVERLAY_SERIAL_LEDS)},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_11_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_19_AH},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_12_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_0_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_1_AH},
  {bp_usGpioLedGpon,           .u.us = BP_GPIO_8_AH},
  {bp_usGpioLedGponFail,       .u.us = BP_SERIAL_GPIO_6_AH},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x8f},
  {bp_ulPhyId0,                .u.ul = BP_PHY_ID_0},
  {bp_usSpeedLed100,           .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usSpeedLed1000,          .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_ulPhyId1,                .u.ul = BP_PHY_ID_1},
  {bp_usSpeedLed100,           .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usSpeedLed1000,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_ulPhyId2,                .u.ul  = PHY_INTEGRATED_VALID | PHY_EXTERNAL | MAC_IF_RGMII | BP_PHY_ID_24},
  {bp_ulPhyId3,                .u.ul  = PHY_INTEGRATED_VALID | PHY_EXTERNAL | MAC_IF_RGMII | BP_PHY_ID_25},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_8_AL},  
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usGpioLaserDis,          .u.us = BP_GPIO_0_AL},
  {bp_usGpioLaserTxPwrEn,      .u.us = BP_GPIO_5_AL},
  {bp_usGponOpticsType,        .u.us = BP_GPON_OPTICS_TYPE_BOSA},
#if !defined(_CFE_)
  {bp_cpDefaultOpticalParams,  .u.ucp = _6818RgParams},    
#endif
  {bp_last}
};

static bp_elem_t g_bcm96818rg_fe[] = {
  {bp_cpBoardId,               .u.cp = "96818RG_FE"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_GPHY_LED_0 |
                                       BP_OVERLAY_GPHY_LED_1 |
                                       BP_OVERLAY_SERIAL_LEDS)},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_11_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_19_AH},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_12_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_0_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_1_AH},
  {bp_usGpioLedGpon,           .u.us = BP_GPIO_8_AH},
  {bp_usGpioLedGponFail,       .u.us = BP_SERIAL_GPIO_6_AH},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x9f},
  {bp_ulPhyId0,                .u.ul = BP_PHY_ID_0},
  {bp_usSpeedLed100,           .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usSpeedLed1000,          .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_ulPhyId1,                .u.ul = BP_PHY_ID_1},
  {bp_usSpeedLed100,           .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usSpeedLed1000,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_ulPhyId2,                .u.ul  = PHY_INTEGRATED_VALID | PHY_EXTERNAL | MAC_IF_RGMII| BP_PHY_ID_24},
  {bp_ulPhyId3,                .u.ul  = PHY_INTEGRATED_VALID | PHY_EXTERNAL | MAC_IF_RGMII| BP_PHY_ID_25},
  {bp_ulPhyId4,                .u.ul = PHYCFG_VALID | MII_OVER_GPIO_VALID | PHY_INTEGRATED_VALID | PHY_EXTERNAL | BP_PHY_ID_16},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},  
  {bp_usGpioLaserDis,          .u.us = BP_GPIO_0_AL},
  {bp_usGpioLaserTxPwrEn,      .u.us = BP_GPIO_5_AL},
  {bp_usGponOpticsType,        .u.us = BP_GPON_OPTICS_TYPE_BOSA},
#if !defined(_CFE_)
  {bp_cpDefaultOpticalParams,  .u.ucp = _6818RgParams},    
#endif
  {bp_last}
};

#if !defined(_CFE_)
unsigned char _68128RgParams[BP_OPTICAL_PARAMS_LEN] = {0x28,0x03,0x02,0x0B,0x00,0xD8,0x00,0x77,
                                                       0x01,0xC5,0x26,0x02,0x02,0x32,0x00,0x00,
                                                       0x00,0x34,0xFA,0x00,0x00,0x09,0x00,0x00,
                                                       0x08,0x6F,0xFF,0xFF,0xEF,0xFF,0xFF,0xFF,
                                                       0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,
                                                       0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x01};
#endif

static bp_elem_t g_bcm968128rg[] = {
  {bp_cpBoardId,               .u.cp = "968128RG"},
  {bp_ulGpioOverlay,           .u.ul = BP_OVERLAY_GPHY_LED_0},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_18_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_19_AH},
  {bp_usGpioLedGpon,           .u.us = BP_GPIO_8_AL},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x81},
  {bp_ulPhyId0,                .u.ul = BP_PHY_ID_0},
  {bp_usSpeedLed100,           .u.us = BP_GPIO_29_AL},
  {bp_usSpeedLed1000,          .u.us = BP_GPIO_26_AL},
  {bp_ulPhyId7,                .u.ul = BP_PHY_ID_NOT_SPECIFIED},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioLaserDis,          .u.us = BP_GPIO_11_AL},
  {bp_usGpioLaserTxPwrEn,      .u.us = BP_GPIO_39_AH},
  {bp_usGponOpticsType,        .u.us = BP_GPON_OPTICS_TYPE_BOSA},
#if !defined(_CFE_)
  {bp_cpDefaultOpticalParams,  .u.ucp = _68128RgParams},  
#endif
  {bp_last}
};

static bp_elem_t g_bcm96829rg[] = {
  {bp_cpBoardId,               .u.cp = "96829RG"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_GPHY_LED_0 |
                                       BP_OVERLAY_GPHY_LED_1 |
                                       BP_OVERLAY_MOCA_LED)},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_3_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_4_AH},
  {bp_usGpioLedMoCA,           .u.us = BP_GPIO_5_AL},
  {bp_usGpioLedMoCAFail,       .u.us = BP_GPIO_37_AH},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0xa3},
  {bp_ulPhyId0,                .u.ul = 0x00},
  {bp_ulPhyId1,                .u.ul = 0x01},
  {bp_ulPhyId2,                .u.ul = 0xff},
  {bp_ulPhyId3,                .u.ul = 0xff},
  {bp_ulPhyId4,                .u.ul = 0x00},
  {bp_ulPhyId5,                .u.ul = 0xff},
  {bp_ulPhyId6,                .u.ul = 0x00},
  {bp_ulPhyId7,                .u.ul = 0xff},
  {bp_usGponOpticsType,        .u.us = BP_GPON_OPTICS_TYPE_LEGACY},
  {bp_last}
};

static bp_elem_t g_bcm96816p2og[] = {
  {bp_cpBoardId,               .u.cp = "96816P2OG"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_GPHY_LED_0 |
                                       BP_OVERLAY_GPHY_LED_1 |
                                       BP_OVERLAY_PCI |
                                       BP_OVERLAY_MOCA_LED)},
  {bp_usGpioFpgaReset,         .u.us = BP_GPIO_2_AH},
  {bp_usGpioLedGpon,           .u.us = BP_GPIO_8_AH},
  {bp_usGpioLedGponFail,       .u.us = BP_GPIO_16_AH},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0xaf},
  {bp_ulPhyId0,                .u.ul = 0x00},
  {bp_ulPhyId1,                .u.ul = 0x01},
  {bp_ulPhyId2,                .u.ul = 0x11},
  {bp_ulPhyId3,                .u.ul = 0xff},
  {bp_ulPhyId5,                .u.ul = 0xff},
  {bp_ulPhyId7,                .u.ul = 0xff},    
  {bp_usGponOpticsType,        .u.us = BP_GPON_OPTICS_TYPE_LEGACY},
  {bp_last}
};

static bp_elem_t g_bcm96812pg[] = {
  {bp_cpBoardId,               .u.cp = "96812PG"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_GPHY_LED_0 |
                                       BP_OVERLAY_GPHY_LED_1)},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_3_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_4_AH},
  {bp_usGpioLedGpon,           .u.us = BP_GPIO_8_AH},
  {bp_usGpioLedGponFail,       .u.us = BP_GPIO_2_AH},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_5_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_15_AH},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0xaf},
  {bp_ulPhyId0,                .u.ul = 0x00},
  {bp_ulPhyId1,                .u.ul = 0x01},
  {bp_ulPhyId2,                .u.ul = 0x11},
  {bp_ulPhyId3,                .u.ul = 0x12},
  {bp_ulPhyId5,                .u.ul = 0xff},
  {bp_ulPhyId7,                .u.ul = 0xff},
  {bp_usGponOpticsType,        .u.us = BP_GPON_OPTICS_TYPE_LEGACY},
  {bp_last}
};

static bp_elem_t g_bcm96819bhr[] = {
  {bp_cpBoardId,               .u.cp = "96819BHR"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_GPHY_LED_0 |
                                       BP_OVERLAY_GPHY_LED_1 |
                                       BP_OVERLAY_MOCA_LED)},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_3_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_4_AH},
  {bp_usGpioLedMoCA,           .u.us = BP_GPIO_5_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_3},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x3f},
  {bp_ulPhyId0,                .u.ul = 0x00},
  {bp_usSpeedLed100,           .u.us = BP_GPIO_13_AL},
  {bp_usSpeedLed1000,          .u.us = BP_GPIO_38_AL},
  {bp_ulPhyId1,                .u.ul = 0x01},
  {bp_usSpeedLed100,           .u.us = BP_GPIO_30_AL},
  {bp_usSpeedLed1000,          .u.us = BP_GPIO_31_AL},
  {bp_ulPhyId2,                .u.ul = 0x14},
  {bp_ulPhyId3,                .u.ul = 0x12},
  {bp_ulPhyId4,                .u.ul = 0xa1},
  {bp_ulPhyId5,                .u.ul = 0xff},    
  {bp_usGponOpticsType,        .u.us = BP_GPON_OPTICS_TYPE_LEGACY},
  {bp_last}
};

static bp_elem_t g_bcm968121pg[] = {
  {bp_cpBoardId,               .u.cp = "968121PG"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_GPHY_LED_0)},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_3_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_24_AH},
  {bp_usGpioLedGpon,           .u.us = BP_GPIO_8_AL},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x95},
  {bp_ulPhyId0,                .u.ul = 0x00},
  {bp_ulPhyId1,                .u.ul = 0x00},
  {bp_ulPhyId2,                .u.ul = 0x18},
  {bp_ulPhyId3,                .u.ul = 0x00},
  {bp_ulPhyId4,                .u.ul = 0xff | PHYCFG_VALID}, /* WAN Interface */
  {bp_ulPhyId5,                .u.ul = 0x00},
  {bp_ulPhyId6,                .u.ul = 0x00},
  {bp_ulPhyId7,                .u.ul = 0xff},    
  {bp_usGponOpticsType,        .u.us = BP_GPON_OPTICS_TYPE_LEGACY},
  {bp_last}
};

static bp_elem_t g_bcm968121p[] = {
  {bp_cpBoardId,               .u.cp = "968121P"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_GPHY_LED_0)},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_3_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_24_AH},
  {bp_usGpioLedGpon,           .u.us = BP_GPIO_8_AL},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x91},
  {bp_ulPhyId0,                .u.ul = 0x00},
  {bp_ulPhyId4,                .u.ul = 0xff | PHYCFG_VALID}, /* WAN Interface */
  {bp_ulPhyId7,                .u.ul = 0xff},    
  {bp_usGponOpticsType,        .u.us = BP_GPON_OPTICS_TYPE_LEGACY},
  {bp_last}
};

static bp_elem_t g_bcm96819bhrgr[] = {
  {bp_cpBoardId,               .u.cp = "96819BHRGR"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_GPHY_LED_0 |
                                       BP_OVERLAY_GPHY_LED_1 |
                                       BP_OVERLAY_MOCA_LED)},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_3_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_4_AH},
  {bp_usGpioLedMoCA,           .u.us = BP_GPIO_5_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_3},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x3f},
  {bp_ulPhyId0,                .u.ul = 0x00},
  {bp_usSpeedLed100,           .u.us = BP_GPIO_13_AL},
  {bp_usSpeedLed1000,          .u.us = BP_GPIO_38_AL},
  {bp_ulPhyId1,                .u.ul = 0x01},
  {bp_usSpeedLed100,           .u.us = BP_GPIO_2_AL},
  {bp_usSpeedLed1000,          .u.us = BP_GPIO_31_AL},
  {bp_ulPhyId2,                .u.ul = 0x18},
  {bp_ulPhyId3,                .u.ul = 0x19},
  {bp_ulPhyId4,                .u.ul = 0xa1},
  {bp_ulPhyId5,                .u.ul = 0xff},    
  {bp_usGponOpticsType,        .u.us = BP_GPON_OPTICS_TYPE_LEGACY},
  {bp_last}
};

static bp_elem_t * g_BoardParms[] = {g_bcm96816sv, g_bcm96816pvwm,
  g_bcm96829rg, g_bcm96816p2og, g_bcm96812pg, g_bcm96819bhr,
  g_bcm968121pg, g_bcm968121p, g_bcm96819bhrgr, g_bcm96818rg, g_bcm96818rg_bosa, g_bcm96818rg_fe, g_bcm968128rg, 0};
#endif

#if defined(_BCM96328_) || defined(CONFIG_BCM96328)

static bp_elem_t g_bcm96328avng[] = {
  {bp_cpBoardId,               .u.cp = "96328avng"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_3_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_9_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_0_AL},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_4_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_8_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_GPIO_6_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_GPIO_7_AL},
  {bp_usGpioPotsLed,           .u.us = BP_GPIO_5_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_1},
  {bp_last}
};

static bp_elem_t g_bcm96328avngrP1[] = {
  {bp_cpBoardId,               .u.cp = "96328avngrP1"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_SPI_EXT_CS |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_10_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_11_AL},
  {bp_usGpioLedWanData,        .u.us = BP_SERIAL_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_13_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_15_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_12_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_1},
  {bp_last}
};

static bp_elem_t g_bcm96328avngr[] = {
  {bp_cpBoardId,               .u.cp = "96328avngr"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_SPI_EXT_CS |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_10_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_11_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_13_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_15_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_12_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_1},
  {bp_last}
};

static bp_elem_t g_bcm963281TAN[] = {
  {bp_cpBoardId,               .u.cp = "963281TAN"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_11_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_9_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_7_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_4_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_8_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_1},
  {bp_last}
};

static bp_elem_t g_bcm963281TAN2[] = {
  {bp_cpBoardId,               .u.cp = "963281TAN2"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_11_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_9_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_7_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_4_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_8_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_2},
  {bp_last}
};

static bp_elem_t g_bcm963281TAN3[] = {
  {bp_cpBoardId,               .u.cp = "963281TAN3"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_11_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_9_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_7_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_4_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_8_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302| BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6302_REV_5_2_2},
  {bp_last}
};

static bp_elem_t g_bcm963281TAN4[] = {
  {bp_cpBoardId,               .u.cp = "963281TAN4"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_11_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_9_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_1_AL},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_7_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_4_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_8_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_2},
  {bp_last}
};

static bp_elem_t g_bcm963293epon[] = {
  {bp_cpBoardId,               .u.cp = "963293epon"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_SPI_EXT_CS |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_11_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_15_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = MII_DIRECT  | CONNECTED_TO_EPON_MAC},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_12_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_1},
  {bp_last}
};

static bp_elem_t g_bcm963293epon_gphy[] = {
  {bp_cpBoardId,               .u.cp = "963293epon_gphy"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_SPI_EXT_CS |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_11_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_15_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18 | CONNECTED_TO_EPON_MAC},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_12_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_1},
  {bp_last}
};

static bp_elem_t g_bcm963283_24tstbrd[] = {
  {bp_cpBoardId,               .u.cp = "963283_24tstbrd"},
  {bp_ulGpioOverlay,           .u.ul =( BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_29_AL},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18},
  {bp_last}
};

static bp_elem_t g_bcm93715rv[] = {
  {bp_cpBoardId,                 .u.cp = "93715rv"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_SPI_EXT_CS)},

  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x01},
  {bp_ulPhyId0,                .u.ul = 0x01 | CONNECTED_TO_EPON_MAC},

  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_12_AL},
  
  {bp_usGpioPassDyingGasp,     .u.us = BP_SERIAL_GPIO_1_AL},
  
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_1},

  {bp_last}
 };


static bp_elem_t * g_BoardParms[] = {g_bcm96328avng, g_bcm96328avngrP1, g_bcm96328avngr, g_bcm963281TAN, g_bcm963281TAN2, g_bcm963281TAN3, g_bcm963281TAN4, g_bcm963293epon, g_bcm963293epon_gphy, g_bcm963283_24tstbrd, g_bcm93715rv, 0};
#endif

#if defined(_BCM963268_) || defined(CONFIG_BCM963268)

static char g_obsoleteStr[] = "(obsolete)";

static bp_elem_t g_bcm963268sv1[] = {
  {bp_cpBoardId,               .u.cp = "963268SV1"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_17_AL},
  {bp_last}
};

static bp_elem_t g_bcm963168mbv_17a[] = {
  {bp_cpBoardId,               .u.cp = "963168MBV_17A"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_usGpioSecLedAdsl,        .u.us = BP_GPIO_9_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanData,        .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0}, 
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},  
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x5f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ulPhyId6,                .u.ul = 0x19 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_6_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV_7_2_30},
  {bp_ulAfeId1,                .u.ul = BP_AFE_CHIP_6306| BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV_7_2_21},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_17_AL},
  {bp_usGpioAFELDRelay,      .u.us = BP_GPIO_39_AH},
  {bp_last}
};

static bp_elem_t g_bcm963168mbv_30a[] = {
  {bp_cpBoardId ,              .u.cp = "963168MBV_30A"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_usGpioSecLedAdsl,        .u.us = BP_GPIO_9_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanData,        .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0}, 
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},  
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x5f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ulPhyId6,                .u.ul = 0x19 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_6_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_6306 | BP_AFE_LD_ISIL1556 | BP_AFE_FE_AVMODE_VDSL | BP_AFE_FE_REV_12_21 | BP_AFE_FE_ANNEXA },
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_17_AL},
  {bp_usGpioAFELDRelay,      .u.us = BP_GPIO_39_AH},
  {bp_last}
};
#if 0 //__MSTC__ merge from 412L06, remove un-used variable
static bp_elem_t g_bcm963168mbv17a302[] = {
  {bp_cpBoardId,               .u.cp = "963168MBV17A302"},
  {bp_usGpioSecLedAdsl,        .u.us = BP_GPIO_17_AL},
  {bp_usGpioIntAFELDMode,      .u.us = BP_PIN_DSL_CTRL_5},
  {bp_usGpioIntAFELDPwr,       .u.us = BP_PIN_DSL_CTRL_4},
  {bp_usGpioExtAFELDMode,      .u.us = BP_GPIO_13_AH},
  {bp_usGpioExtAFELDPwr,       .u.us = BP_GPIO_12_AH},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_11_AL},
  {bp_usGpioAFELDRelay,        .u.us = BP_GPIO_NONE},
  {bp_elemTemplate,            .u.bp_elemp = g_bcm963168mbv_17a},
  {bp_last}
};

static bp_elem_t g_bcm963168mbv30a302[] = {
  {bp_cpBoardId,               .u.cp = "963168MBV30A302"},
  {bp_usGpioSecLedAdsl,        .u.us = BP_GPIO_17_AL},
  {bp_usGpioIntAFELDMode,      .u.us = BP_PIN_DSL_CTRL_5},
  {bp_usGpioIntAFELDPwr,       .u.us = BP_PIN_DSL_CTRL_4},
  {bp_usGpioExtAFELDMode,      .u.us = BP_GPIO_13_AH},
  {bp_usGpioExtAFELDPwr,       .u.us = BP_GPIO_12_AH},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_11_AL},
  {bp_usGpioAFELDRelay,        .u.us = BP_GPIO_NONE},
  {bp_elemTemplate,            .u.bp_elemp = g_bcm963168mbv_30a},
  {bp_last}
};
#endif
static bp_elem_t g_bcm963268mbv[] = {
  {bp_cpBoardId,                 .u.cp = "963268MBV"},
  {bp_cpComment,               .u.cp = g_obsoleteStr},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_usGpioSecLedAdsl,        .u.us = BP_GPIO_9_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanData,        .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x5f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ulPhyId6,                .u.ul = 0x19 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_6_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV_7_2_30},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_17_AL},
  {bp_usGpioAFELDRelay,      .u.us = BP_GPIO_39_AH},
  {bp_last}
};
#if 0 //__MSTC__ merge from 412L06, remove un-used variable
static bp_elem_t g_bcm963168mbv3[] = {
  {bp_cpBoardId ,              .u.cp = "963168MBV3"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioSecLedAdsl,        .u.us = BP_GPIO_17_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6303 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6303_REV_12_3_30 }, 
  {bp_ulAfeId1,                .u.ul = BP_AFE_CHIP_6306 | BP_AFE_LD_6303 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6303_REV_12_3_20 }, 
  // LDMode is set to NONE in case the board we are inheriting from set them
  {bp_usGpioIntAFELDMode,      .u.us = BP_GPIO_NONE},  
  {bp_usGpioIntAFELDPwr,       .u.us = BP_GPIO_10_AH},
  // IntAFELDClk uses dedicated pin
  // IntAFELDData uses dedicated pin
  {bp_usGpioExtAFELDMode,      .u.us = BP_GPIO_NONE},
  {bp_usGpioExtAFELDPwr,       .u.us = BP_GPIO_9_AH},
  {bp_usGpioExtAFELDClk,       .u.us = BP_GPIO_8_AL},
  {bp_usGpioExtAFELDData,      .u.us = BP_GPIO_11_AH},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_13_AL},
  {bp_usGpioAFELDRelay,        .u.us = BP_GPIO_NONE},
  {bp_elemTemplate,            .u.bp_elemp = g_bcm963168mbv_30a},
  {bp_last}
};
#endif
#if 0
/* Uncomment "#define BP_GET_INT_AFE_DEFINED" in Boardparams.h when these bp_ids are in used:
* bp_usGpioIntAFELDPwr
* bp_usGpioIntAFELDMode
* bp_usGpioAFELDRelay
*/

static bp_elem_t g_bcm963268mbv6b[] = {
  {bp_cpBoardId,               .u.cp = "963168MBV6b"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanData,        .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x5f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18},
  {bp_ulPhyId6,                .u.ul = 0x19},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_6_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_6306 | BP_AFE_LD_ISIL1556 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_12_21},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_17_AL},
  {bp_usGpioExtAFELDPwr,       .u.us = BP_GPIO_13_AH},
  {bp_usGpioExtAFELDMode,      .u.us = BP_GPIO_12_AH},
  {bp_usGpioIntAFELDPwr,       .u.us = BP_GPIO_11_AH},
  {bp_usGpioIntAFELDMode,      .u.us = BP_GPIO_10_AH},
  {bp_usGpioAFELDRelay,      .u.us = BP_GPIO_39_AH},
  {bp_last}
};
#endif
#if 1
//__MSTC__, Eason, DSL-2492GNAU-B1BC C2
static bp_elem_t g_bcm963168vx[] = {
  {bp_cpBoardId,               .u.cp = "963168VX"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_HS_SPI_SSB4_EXT_CS //__MSTC__, Eason, DSL-2492GNAU-B1BC support GPIO16 as SS4 for SLIC2 chip select
                                       )},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_14_AL}, 
  {bp_usGpioLedVdsl,           .u.us = BP_GPIO_15_AL}, 
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AH}, //wireless wps
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},  
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL}, 
  {bp_usGpioPhone1MWILed,          .u.us = BP_GPIO_9_AL},  //MWI 1
  {bp_usGpioPhone2MWILed,          .u.us = BP_GPIO_10_AL}, //MWI 2 
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedInternetData,   .u.us = BP_GPIO_22_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL}, 
#if 1 // Mandy,WPS5GLED
  {bp_usGpioLed5GWps,      .u.us = BP_GPIO_17_AL}, //5g wps
#endif
  {bp_usGpioLedUSB0,            .u.us =  BP_SERIAL_GPIO_3_AL}, 
  {bp_usGpioLedUSB1,            .u.us = BP_SERIAL_GPIO_6_AL},  
  {bp_usGpioGWAN100MLed,		.u.us = BP_SERIAL_GPIO_0_AH},  
  {bp_usGpioGWAN1000MLed,		.u.us = BP_SERIAL_GPIO_1_AH},     
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0}, 
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},  
  {bp_usExtIntrWlanEnableBtnWireless, .u.us = BP_EXT_INTR_2},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x58},
  /*configure internal switch port 3 as wan*/ 
  {bp_ulPhyId3,                .u.ul = BP_PHY_ID_4},
  /*configure internal switch port 4(mii 1) as port(external switch port 6)*/ 
  {bp_ulPhyId4,                .u.ul = TMII_DIRECT|0x14},
  /*configure internal switch port 6(mii 2) as 5G wifi module*/ 
  {bp_ulPhyId6,                .u.ul = RGMII_DIRECT | EXTSW_CONNECTED},
  {bp_ucPhyType1,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_HS_SPI_SSB_0},
  /*bp_ulPortMap:	bit 0:reserved,  bit 1~4: LAN port*/ 
  {bp_ulPortMap,               .u.ul = 0x1e},
  {bp_ulPhyId1,                .u.ul = BP_PHY_ID_1 | PHY_INTEGRATED_VALID | PHY_EXTERNAL},
  {bp_ulPhyId2,                .u.ul = BP_PHY_ID_2 | PHY_INTEGRATED_VALID | PHY_EXTERNAL},
  {bp_ulPhyId3,                .u.ul = BP_PHY_ID_3 | PHY_INTEGRATED_VALID | PHY_EXTERNAL},
  {bp_ulPhyId4,                .u.ul = BP_PHY_ID_4 | PHY_INTEGRATED_VALID | PHY_EXTERNAL},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,				 .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV_7_2_30},		 
  {bp_usGpioIntAFELDMode,      .u.us = BP_PIN_DSL_CTRL_5},
  {bp_usGpioIntAFELDPwr,       .u.us = BP_PIN_DSL_CTRL_4},
#if 1 /* chchien  add for fullrate HW */    
  {bp_usGpioLedWanEth3,        .u.us = BP_GPIO_12_AL},
#endif 
  {bp_usGpioAFELDRelay,      .u.us = BP_NOT_DEFINED},
  {bp_last}
};
#else
static bp_elem_t g_bcm963168vx[] = {
  {bp_cpBoardId,                 .u.cp = "963168VX"},
  {bp_usGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |

                                       BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_7_AH},    
#if 1 //__MSTC__, RaynorChung: Ssupport VDSL LED
  {bp_usGpioLedVdsl,     .u.us = BP_SERIAL_GPIO_3_AH},  
#endif
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_22_AH}, /*__MSTC__, Paul Ho, WPS Led */
  {bp_usGpioLedWanData,        .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedInternetData,   .u.us = BP_SERIAL_GPIO_14_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usGpioWirelessBtn,       .u.us = BP_GPIO_33_AL}, /*__MSTC__, Paul Ho, Wireless Button */   
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
 // {bp_usExtIntrWpsBtnWireless, .u.us = BP_EXT_INTR_3},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usGpioPhone1MWILed,      .u.us = BP_SERIAL_GPIO_13_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_usGpioPhone1MWILed,      .u.us = BP_SERIAL_GPIO_15_AL},

#if 1 //__MSTC__, RaynorChung: Ssupport USB LED
  {bp_usGpioLedUSB0,            .u.us = BP_GPIO_6_AH}, 
  {bp_usGpioLedUSB1,            .u.us = BP_GPIO_6_AH},  
  {bp_usGpioGWAN100MLed,            .u.us = BP_SERIAL_GPIO_0_AL},  
  {bp_usGpioGWAN1000MLed,            .u.us = BP_SERIAL_GPIO_1_AL},  
#endif
  {bp_ulAfeId0,                .u.ul = 0x10608700},
  {bp_last}
};
#endif

#if 0 //__MSTC__ merge from 412L06, remove un-used variable
static bp_elem_t g_bcm963168vx_p300[] = {
  {bp_cpBoardId,               .u.cp = "963168VX_P300"},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV_7_2_30},
  {bp_elemTemplate,            .u.bp_elemp = g_bcm963168vx}, 
  {bp_last}
};
#endif
static bp_elem_t g_bcm963168xf[] = {
  {bp_cpBoardId,               .u.cp = "963168XF"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanData,        .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul =  (PHY_LNK_CFG_VALID | FORCE_LINK_100FD | MAC_CONN_VALID | MAC_MAC_IF | MAC_IFACE_VALID | MAC_IF_RvMII) },  
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_GPIO_14_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_GPIO_15_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_usGpioFemtoReset,        .u.us = BP_GPIO_8_AH},  
  {bp_last}
};

#if 0 //__MSTC__, Delon Yu
static bp_elem_t g_bcm963268sv2_extswitch[] = {
  {bp_cpBoardId,               .u.cp = "963268SV2_EXTSW"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_HS_SPI_SSB7_EXT_CS)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanData,        .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0xbf},
  {bp_ulPhyId0,                .u.ul = BP_PHY_ID_1},
  {bp_ulPhyId1,                .u.ul = BP_PHY_ID_2},
  {bp_ulPhyId2,                .u.ul = BP_PHY_ID_3},
  {bp_ulPhyId3,                .u.ul = BP_PHY_ID_4},
  {bp_ulPhyId4,                .u.ul = RGMII_DIRECT | EXTSW_CONNECTED},
  {bp_ulPhyId5,                .u.ul = 0x18 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ulPhyId7,                .u.ul = 0x19 | MAC_IFACE_VALID | MAC_IF_RGMII},  
  {bp_ucPhyType1,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MDIO},
  {bp_ulPortMap,               .u.ul = 0x03},
  {bp_ulPhyId0,                .u.ul = BP_PHY_ID_0},
  {bp_ulPhyId1,                .u.ul = BP_PHY_ID_1},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_6_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_usGpioSpiSlaveReset,     .u.us = BP_GPIO_22_AH},
  {bp_usSpiSlaveBusNum,        .u.us = HS_SPI_BUS_NUM},
  {bp_usSpiSlaveSelectNum,     .u.us = 7},
  {bp_usSpiSlaveMode,          .u.us = SPI_MODE_3},
  {bp_ulSpiSlaveCtrlState,     .u.ul = SPI_CONTROLLER_STATE_GATE_CLK_SSOFF},
  {bp_ulSpiSlaveMaxFreq,       .u.ul = 781000},
  {bp_usSpiSlaveProtoRev,      .u.us = 0},   
  {bp_last}
};
#endif

static bp_elem_t g_bcm963268bu[] = {
  {bp_cpBoardId,               .u.cp = "963268BU"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_GPHY_LED_0 |
                                       BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  // {bp_usEphyBaseAddress,       .u.us = 0x10},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0xFC},
  {bp_ulPhyId2,                .u.ul = BP_PHY_ID_3 | PHY_INTERNAL | PHY_INTEGRATED_VALID },
  {bp_ulPhyId3,                .u.ul = BP_PHY_ID_4 },
  {bp_ulPhyId4,                .u.ul = BP_PHY_ID_0 | MAC_IFACE_VALID | MAC_IF_RGMII | PHY_INTEGRATED_VALID | PHY_EXTERNAL},
  {bp_ulPhyId5,                .u.ul = BP_PHY_ID_1 | MAC_IFACE_VALID | MAC_IF_RGMII | PHY_INTEGRATED_VALID | PHY_EXTERNAL },
  {bp_ulPhyId6,                .u.ul = BP_PHY_ID_24 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ulPhyId7,                .u.ul = BP_PHY_ID_25 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_6_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_usGpioIntAFELDMode,      .u.us = BP_GPIO_10_AH},
  {bp_usGpioIntAFELDPwr,       .u.us = BP_GPIO_11_AH},
  {bp_last}
};

static bp_elem_t g_bcm963268bu_p300[] = {
  {bp_cpBoardId,               .u.cp = "963268BU_P300"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_INET_LED |
                                       BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_GPHY_LED_0 |
                                       BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_usEphyBaseAddress,       .u.us = 0x10},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0xF9},
  {bp_ulPhyId0,                .u.ul = BP_PHY_ID_17 | PHY_INTERNAL | PHY_INTEGRATED_VALID },
  {bp_ulPhyId3,                .u.ul = BP_PHY_ID_4 | PHY_INTERNAL | PHY_INTEGRATED_VALID },
  {bp_ulPhyId4,                .u.ul = BP_PHY_ID_0 | MAC_IFACE_VALID | MAC_IF_RGMII | PHY_INTEGRATED_VALID | PHY_EXTERNAL},
  {bp_ulPhyId5,                .u.ul = BP_PHY_ID_1 | MAC_IFACE_VALID | MAC_IF_RGMII | PHY_INTEGRATED_VALID | PHY_EXTERNAL },
  {bp_ulPhyId6,                .u.ul = BP_PHY_ID_24 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ulPhyId7,                .u.ul = BP_PHY_ID_25 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_6_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_usGpioIntAFELDMode,      .u.us = BP_GPIO_10_AH},
  {bp_usGpioIntAFELDPwr,       .u.us = BP_GPIO_11_AH},
  {bp_last}
};

static bp_elem_t g_bcm963168xh[] = {
  {bp_cpBoardId,               .u.cp = "963168XH"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE|
                                       BP_OVERLAY_USB_LED|
                                       BP_OVERLAY_PCIE_CLKREQ |
                                       BP_OVERLAY_HS_SPI_SSB5_EXT_CS)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_13_AH},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_10_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},  
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x58},
  {bp_ulPhyId3,                .u.ul = BP_PHY_ID_4},
  {bp_ulPhyId4,                .u.ul = RGMII_DIRECT | EXTSW_CONNECTED},
  {bp_ulPhyId6,                .u.ul = BP_PHY_ID_25 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ucPhyType1,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_HS_SPI_SSB_5},// Remember to make MDIO HW changes(install resistors R540, R541 and R553) BP_ENET_CONFIG_HS_SPI_SSB_5},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = BP_PHY_ID_0},
  {bp_ulPhyId1,                .u.ul = BP_PHY_ID_1},
  {bp_ulPhyId2,                .u.ul = BP_PHY_ID_2},
  {bp_ulPhyId3,                .u.ul = BP_PHY_ID_3},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_6_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_6306 | BP_AFE_LD_ISIL1556 | BP_AFE_FE_AVMODE_VDSL | BP_AFE_FE_REV_12_21 | BP_AFE_FE_ANNEXA },
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_11_AL},  
  {bp_last}
};
#if 0 //__MSTC__ merge from 412L06, remove un-used variable
static bp_elem_t g_bcm963168xh5[] = {
    {bp_cpBoardId,               .u.cp = "963168XH5"},
    {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
    {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_SERIAL_LEDS |
                                      BP_OVERLAY_USB_DEVICE|
                                      BP_OVERLAY_USB_LED|
                                      BP_OVERLAY_PCIE_CLKREQ |
                                      BP_OVERLAY_HS_SPI_SSB5_EXT_CS)},
    {bp_usGpioLedAdsl,           .u.us = BP_GPIO_13_AH},
    {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
    {bp_usGpioLedWanData,        .u.us = BP_GPIO_8_AL},
    {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
    {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_10_AL},
    {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_3_AL},
    {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
    {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},  
    {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
    {bp_usWirelessFlags,         .u.us = 0},
    {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
    {bp_ucPhyAddress,            .u.uc = 0x0},
    {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
    {bp_ulPortMap,               .u.ul = 0x58},
    {bp_ulPhyId3,                .u.ul = BP_PHY_ID_4},
    {bp_ulPhyId4,                .u.ul = RGMII_DIRECT | EXTSW_CONNECTED},
    {bp_ulPhyId6,                .u.ul = BP_PHY_ID_25 | MAC_IFACE_VALID | MAC_IF_RGMII},
    {bp_ucPhyType1,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
    {bp_ucPhyAddress,            .u.uc = 0x0},
    {bp_usConfigType,            .u.us = BP_ENET_CONFIG_HS_SPI_SSB_5},// Remember to make MDIO HW changes(install resistors R540, R541 and R553) BP_ENET_CONFIG_HS_SPI_SSB_5},
    {bp_ulPortMap,               .u.ul = 0x0f},
    {bp_ulPhyId0,                .u.ul = BP_PHY_ID_0},
    {bp_ulPhyId1,                .u.ul = BP_PHY_ID_1},
    {bp_ulPhyId2,                .u.ul = BP_PHY_ID_2},
    {bp_ulPhyId3,                .u.ul = BP_PHY_ID_3},
    {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
    {bp_ucDspAddress,            .u.uc = 0},
    {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
    {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
    {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_6_AL},
    {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_6306 | BP_AFE_LD_ISIL1556 | BP_AFE_FE_AVMODE_VDSL | BP_AFE_FE_REV_12_21 | BP_AFE_FE_ANNEXA },
    {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_11_AL},  
    {bp_last}
};

  static bp_elem_t g_bcm963168xm[] = {
    {bp_cpBoardId,               .u.cp = "963168XM"},
    {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
    {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_SERIAL_LEDS |
                                         BP_OVERLAY_USB_DEVICE|
                                         BP_OVERLAY_PCIE_CLKREQ |
                                         BP_OVERLAY_HS_SPI_SSB5_EXT_CS)},
    {bp_usGpioLedAdsl,           .u.us = BP_GPIO_13_AH},
    {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
    {bp_usGpioLedWanData,        .u.us = BP_GPIO_8_AL},
    {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
    {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_10_AL},
    {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_3_AL},
    {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
    {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},  
    {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
    {bp_usWirelessFlags,         .u.us = 0},
    {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
    {bp_ucPhyAddress,            .u.uc = 0x0},
    {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
    {bp_ulPortMap,               .u.ul = 0x58},
    {bp_ulPhyId3,                .u.ul = BP_PHY_ID_4},
    {bp_ulPhyId4,                .u.ul = RGMII_DIRECT | EXTSW_CONNECTED},
    {bp_ulPhyId6,                .u.ul = BP_PHY_ID_25},
    {bp_ucPhyType1,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
    {bp_ucPhyAddress,            .u.uc = 0x0},
    {bp_usConfigType,            .u.us = BP_ENET_CONFIG_HS_SPI_SSB_5},// Remember to make MDIO HW changes(install resistors R540, R541 and R553) BP_ENET_CONFIG_HS_SPI_SSB_5},
    {bp_ulPortMap,               .u.ul = 0x0f},
    {bp_ulPhyId0,                .u.ul = BP_PHY_ID_0},
    {bp_ulPhyId1,                .u.ul = BP_PHY_ID_1},
    {bp_ulPhyId2,                .u.ul = BP_PHY_ID_2},
    {bp_ulPhyId3,                .u.ul = BP_PHY_ID_3},
    {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
    {bp_ucDspAddress,            .u.uc = 0},
    {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
    {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
    {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_6_AL},
    {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_6306 | BP_AFE_LD_ISIL1556 | BP_AFE_FE_AVMODE_VDSL | BP_AFE_FE_REV_12_21 | BP_AFE_FE_ANNEXA },
    {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_11_AL},  
    {bp_last}
  };
#endif

  static bp_elem_t g_bcm963168mp[] = {
  {bp_cpBoardId,               .u.cp = "963168MP"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanData,        .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},  
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1F},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = MII_DIRECT},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_6_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV_7_2_30},
  {bp_last}
};

  static bp_elem_t g_bcm963268v30a[] = {
  {bp_cpBoardId,               .u.cp = "963268V30A"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCIE_CLKREQ |
                                       BP_OVERLAY_PHY |
                                       BP_OVERLAY_GPHY_LED_0 |
                                       BP_OVERLAY_SERIAL_LEDS )},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN}, // FIXME
  {bp_usWirelessFlags,         .u.us = 0}, // FIXME
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0xD8},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x00 | MAC_IFACE_VALID | MAC_IF_RGMII | PHY_INTEGRATED_VALID | PHY_EXTERNAL},
  {bp_ulPhyId6,                .u.ul = 0x18 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ulPhyId7,                .u.ul = 0x19 | MAC_IFACE_VALID | MAC_IF_RGMII},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_6_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_6306 | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_6302_6306_REV_A_12_40},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_9_AL},  
  {bp_usGpioExtAFELDMode,      .u.us = BP_GPIO_10_AH},
  {bp_usGpioExtAFELDPwr,       .u.us = BP_GPIO_11_AH},
  {bp_last}
};

  static bp_elem_t g_bcm963168media[] = {
  {bp_cpBoardId,               .u.cp = "963168MEDIA"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PCIE_CLKREQ |
                                       BP_OVERLAY_PHY |                                       
                                       BP_OVERLAY_SERIAL_LEDS )},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_SERIAL_GPIO_1_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_6_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN}, // FIXME
  {bp_usWirelessFlags,         .u.us = 0}, // FIXME
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x5F},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18 | MAC_IFACE_VALID | MAC_IF_RGMII | PHY_INTEGRATED_VALID | PHY_EXTERNAL},
  {bp_ulPhyId6,                .u.ul = RGMII_DIRECT},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL}, 
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV_7_2_30},
  {bp_usGpioExtAFELDMode,      .u.us = BP_GPIO_10_AH},
  {bp_usGpioExtAFELDPwr,       .u.us = BP_GPIO_11_AH},
  {bp_usGpioUart2Sdin,         .u.us = BP_GPIO_12_AH},
  {bp_usGpioUart2Sdout,        .u.us = BP_GPIO_13_AH},        
  {bp_last}
};


static bp_elem_t g_bcm963268sv2[] = {
  {bp_cpBoardId,               .u.cp = "963268SV2"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = BP_PHY_ID_1},
  {bp_ulPhyId1,                .u.ul = BP_PHY_ID_2},
  {bp_ulPhyId2,                .u.ul = BP_PHY_ID_3},
  {bp_ulPhyId3,                .u.ul = BP_PHY_ID_4},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV1},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_17_AL},
  {bp_last}
};

static bp_elem_t g_bcm963168xfg3[] = {
  {bp_cpBoardId,               .u.cp = "963168XFG3"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE  |
                                       BP_OVERLAY_PHY         |
                                       BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanData,        .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},  
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1F},
  {bp_ulPhyId0,                .u.ul = BP_PHY_ID_1}, 
  {bp_ulPhyId1,                .u.ul = BP_PHY_ID_2},  
  {bp_ulPhyId2,                .u.ul = BP_PHY_ID_3},         
  {bp_ulPhyId3,                .u.ul = BP_PHY_ID_4},
  {bp_ulPhyId4,                .u.ul = MII_DIRECT},  
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_usGpioExtAFELDMode,      .u.us = BP_GPIO_10_AH},
  {bp_usGpioExtAFELDPwr,       .u.us = BP_GPIO_11_AH},  
  {bp_usGpioFemtoReset,        .u.us = BP_GPIO_8_AH},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6302_REV_7_2_30}, 
  {bp_last}
};

static bp_elem_t g_bcm963168mxh_17a[] = {
  {bp_cpBoardId,               .u.cp = "963168MXH_17A"},
  {bp_usGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_LED |
                                       BP_OVERLAY_PCIE_CLKREQ |
                                       BP_OVERLAY_HS_SPI_SSB5_EXT_CS)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_14_AL},
  {bp_usGpioSecLedAdsl,        .u.us = BP_GPIO_9_AL},
#if 1 /*__MSTC__, FuChia: Support VDSL LED*/
  {bp_usGpioLedVdsl,     .u.us = BP_GPIO_15_AL},
  {bp_usGpioSecLedVdsl,     .u.us = BP_GPIO_16_AL},  
#endif
#if 1 /*__MSTC__, DelonYu: Support DSL-491GNU-B1D C3 board */
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AH},
#else
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
#endif
#if 1 /*__MSTC__, DelonYu: Support DSL-491GNU-B1D C3 board */
  {bp_usGpioLedWanData,        .u.us = BP_SERIAL_GPIO_8_AL},
#else
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_8_AL},
#endif
#if 1 /*__MSTC__, DelonYu: Support DSL-491GNU-B1D C3 board */
  {bp_usGpioLedInternetData,   .u.us = BP_GPIO_22_AH},
#else
  {bp_usGpioLedInternetData,   .u.us = BP_GPIO_22_AL},
#endif
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0}, 
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},  
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
#if 1 /*__MSTC__, DelonYu: Support DSL-491GNU-B1D C3 board */
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x58},
  {bp_ulPhyId3,                .u.ul = BP_PHY_ID_4},
  {bp_ulPhyId4,                .u.ul = TMII_DIRECT|0x14},
  {bp_ulPhyId6,                .u.ul = RGMII_DIRECT | EXTSW_CONNECTED},
  {bp_ucPhyType1,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_HS_SPI_SSB_0},// Remember to make MDIO HW changes(install resistors R540, R541 and R553) BP_ENET_CONFIG_HS_SPI_SSB_5},
  {bp_ulPortMap,               .u.ul = 0x1e},
  {bp_ulPhyId0,                .u.ul = BP_PHY_ID_1},
  {bp_ulPhyId1,                .u.ul = BP_PHY_ID_2},
  {bp_ulPhyId2,                .u.ul = BP_PHY_ID_3},
  {bp_ulPhyId3,                .u.ul = BP_PHY_ID_4},
#else /*__MSTC__, DelonYu: Support DSL-491GNU-B1D C2 board */
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x50},
  {bp_ulPhyId4,                .u.ul = RGMII_DIRECT | EXTSW_CONNECTED},
  {bp_ulPhyId6,                .u.ul = TMII_DIRECT|0x14},
  {bp_ucPhyType1,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_HS_SPI_SSB_0},// Remember to make MDIO HW changes(install resistors R540, R541 and R553) BP_ENET_CONFIG_HS_SPI_SSB_5},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = BP_PHY_ID_0},
  {bp_ulPhyId1,                .u.ul = BP_PHY_ID_1},
  {bp_ulPhyId2,                .u.ul = BP_PHY_ID_2},
  {bp_ulPhyId3,                .u.ul = BP_PHY_ID_3},
  {bp_ulPhyId4,                .u.ul = BP_PHY_ID_4},
#endif
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
#if 1 /*__MSTC__, FuChia: Support USB LED*/
  {bp_usGpioLedUSB0,            .u.us = BP_SERIAL_GPIO_6_AL},  
#endif
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV_7_2_30},
  {bp_ulAfeId1,                .u.ul = BP_AFE_CHIP_6306| BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV_7_2_21},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_17_AL},
  {bp_usGpioAFELDRelay,      .u.us = BP_GPIO_39_AH},
  {bp_last}
};
#if 1
static bp_elem_t g_bcm963168_VMG5313[] = {
  {bp_cpBoardId,               .u.cp = "963168_VMG5313"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_DISABLE_LED_INVERSION},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_EPHY_LED_0  |
                                       BP_OVERLAY_EPHY_LED_1  |
                                       BP_OVERLAY_EPHY_LED_2  |
                                       BP_OVERLAY_GPHY_LED_0)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_16_AL},
  {bp_usGpioLedVdsl,           .u.us = BP_GPIO_17_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_44_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_8_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_14_AL},
  //{bp_usGpioLedInternetData,   .u.us = BP_GPIO_8_AH},//20131017, TimLiu GPIO chande for SR2
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AH},//20130829, TimLiu actually SR1 sample is remark to ACTIVE HIGH
  {bp_usGpioLedUSB0,           .u.us = BP_GPIO_0_AL},
  {bp_usGpioLedUSB1,           .u.us = BP_GPIO_1_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_3},
  {bp_usExtIntrWlanEnableBtnWireless,       .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
//20131128, TimLiu +++ modify because of VMG5313 Lan port 3 cannot link up with some
//                     special product, like ZyXEL GS2200, but others is ok!
  {bp_ulPhyId3,                .u.ul = 0x04 | ADVERTISE_ALL_MII | PHY_ADV_CFG_VALID},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_GPIO_19_AL},
  {bp_usGpioPhone1MWILed,      .u.us = BP_GPIO_23_AL},	//using for voice message
  {bp_usGpioVoip2Led,          .u.us = BP_GPIO_15_AL},
  {bp_usGpioPhone2MWILed,      .u.us = BP_GPIO_18_AL},  //using for voice message
  //{bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV_7_2_30},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXB | BP_AFE_FE_REV_6302_REV_7_2_30},
  {bp_usGpioIntAFELDMode,      .u.us = BP_PIN_DSL_CTRL_5},	//LD1_DSL_MODE
  {bp_usGpioIntAFELDPwr,       .u.us = BP_PIN_DSL_CTRL_4},	//LD1_SUPPLY_EN
  {bp_usGpioAFELDRelay,        .u.us = BP_GPIO_NONE},
  {bp_last}
};
#endif
#if 1 //__MSTC__, Delon Yu
static bp_elem_t * g_BoardParms[] = {g_bcm963268sv1, g_bcm963268mbv, g_bcm963168vx, g_bcm963268bu, g_bcm963268bu_p300, g_bcm963168mbv_17a, g_bcm963168mbv_30a, g_bcm963168xh, g_bcm963168mp, g_bcm963268v30a,   g_bcm963168media, g_bcm963268sv2, g_bcm963168xfg3, g_bcm963168xf, g_bcm963168mxh_17a, g_bcm963168_VMG5313, 0};
#else
static bp_elem_t * g_BoardParms[] = {g_bcm963268sv1, g_bcm963268mbv, g_bcm963168vx, g_bcm963168vx_p300, g_bcm963268bu, g_bcm963268bu_p300, g_bcm963268sv2_extswitch, g_bcm963168mbv_17a, g_bcm963168mbv_30a, g_bcm963168xh, g_bcm963168xh5, g_bcm963168mp, g_bcm963268v30a, g_bcm963168media, g_bcm963268sv2, g_bcm963168xfg3, g_bcm963168xf, g_bcm963168xm, g_bcm963168mbv3, g_bcm963168mbv17a302, g_bcm963168mbv30a302, 0};
#endif

/*__ZyXEL__, Cj_Lai, Support VMG1312-B10A and B10B difference GPIO in board*/
#if defined(CONFIG_ZYXEL_VMG1312)
/*####### DSL-401HNU-B1B_v4 START ########*/
#if 1//__ZyXEL__, Cj_Lai: Support DSL-401HNU-B1B v4 board VMG1312-B10B
static bp_elem_t g_bcm963168vx_v4[] = {
  {bp_cpBoardId,                 .u.cp = "963168VX"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_14_AH},    
#if 1 //__MSTC__, RaynorChung: Ssupport VDSL LED
  {bp_usGpioLedVdsl,     .u.us = BP_GPIO_15_AH},  
#endif
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_16_AH}, /*__MSTC__, Paul Ho, WPS Led */
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_8_AL},
  {bp_usGpioLedInternetData,   .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_18_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usGpioWirelessBtn,       .u.us = BP_GPIO_24_AH}, /*__MSTC__, Paul Ho, Wireless Button */   
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04 | ADVERTISE_ALL_MII | PHY_ADV_CFG_VALID},
#if 1 //__MSTC__, RaynorChung: Ssupport USB LED
  {bp_usGpioLedUSB0,            .u.us = BP_GPIO_17_AH},  
#endif
  {bp_ulAfeId0,                .u.ul = 0x10608700},
  {bp_last}
};
#endif //end from -> Support DSL-401HNU-B1B v4 board VMG1312-B10B

#if 1 //__MSTC__, Delon Yu
static bp_elem_t * g_BoardParms_v4[] = {g_bcm963268sv1, g_bcm963268mbv, g_bcm963168vx_v4, g_bcm963268bu, g_bcm963268bu_p300, g_bcm963168mbv_17a, g_bcm963168mbv_30a, g_bcm963168xh, g_bcm963168mp, g_bcm963268v30a,   g_bcm963168media, g_bcm963268sv2, g_bcm963168xfg3, g_bcm963168xf, g_bcm963168mxh_17a, 0};
#else
static bp_elem_t * g_BoardParms_v4[] = {g_bcm963268sv1, g_bcm963268mbv, g_bcm963168vx_v4, g_bcm963168vx_p300, g_bcm963268bu, g_bcm963268bu_p300, g_bcm963268sv2_extswitch, g_bcm963168mbv_17a, g_bcm963168mbv_30a, g_bcm963168xh, g_bcm963168xh5, g_bcm963168mp, g_bcm963268v30a, g_bcm963168media, g_bcm963268sv2, g_bcm963168xfg3, g_bcm963168xf, g_bcm963168xm, g_bcm963168mbv3, g_bcm963168mbv17a302, g_bcm963168mbv30a302, 0};
#endif
/*####### DSL-401HNU-B1B_v4 END ########*/

/*####### DSL-401HNU-B1B_v2 START ########*/
#if 1//__ZyXEL__, Cj_Lai: Support DSL-401HNU-B1B v4 board VMG1312-B10A
static bp_elem_t g_bcm963168vx_v2[] = {
  {bp_cpBoardId,                 .u.cp = "963168VX"},
  {bp_ulDeviceOptions,         .u.ul = BP_DEVICE_OPTION_ENABLE_GMAC },
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE |
                                       BP_OVERLAY_PCIE_CLKREQ)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_15_AH},    
#if 1 //__MSTC__, RaynorChung: Ssupport VDSL LED
  {bp_usGpioLedVdsl,     .u.us = BP_GPIO_19_AH},  
#endif
  {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_16_AH}, /*__MSTC__, Paul Ho, WPS Led */
  {bp_usGpioLedWanData,        .u.us = BP_SERIAL_GPIO_8_AL},
  //{bp_usGpioLedInternetData,   .u.us = BP_SERIAL_GPIO_14_AH},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_18_AH},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AH},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_0},
  {bp_usGpioWirelessBtn,       .u.us = BP_GPIO_24_AH}, /*__MSTC__, Paul Ho, Wireless Button */   
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04 | ADVERTISE_ALL_MII | PHY_ADV_CFG_VALID},
  {bp_ulPhyId4,                .u.ul = 0x18 | MAC_IFACE_VALID | MAC_IF_RGMII},
#if 1 //__MSTC__, RaynorChung: Ssupport USB LED
  {bp_usGpioLedUSB0,            .u.us = BP_GPIO_17_AH},  
#endif
  {bp_ulAfeId0,                .u.ul = 0x10608700},
  {bp_last}
};
#endif

#if 1 //__MSTC__, Delon Yu
static bp_elem_t * g_BoardParms_v2[] = {g_bcm963268sv1, g_bcm963268mbv, g_bcm963168vx_v2, g_bcm963268bu, g_bcm963268bu_p300, g_bcm963168mbv_17a, g_bcm963168mbv_30a, g_bcm963168xh, g_bcm963168mp, g_bcm963268v30a,   g_bcm963168media, g_bcm963268sv2, g_bcm963168xfg3, g_bcm963168xf, g_bcm963168mxh_17a, 0};
#else
static bp_elem_t * g_BoardParms_v2[] = {g_bcm963268sv1, g_bcm963268mbv, g_bcm963168vx_v2, g_bcm963168vx_p300, g_bcm963268bu, g_bcm963268bu_p300, g_bcm963268sv2_extswitch, g_bcm963168mbv_17a, g_bcm963168mbv_30a, g_bcm963168xh, g_bcm963168xh5, g_bcm963168mp, g_bcm963268v30a, g_bcm963168media, g_bcm963268sv2, g_bcm963168xfg3, g_bcm963168xf, g_bcm963168xm, g_bcm963168mbv3, g_bcm963168mbv17a302, g_bcm963168mbv30a302, 0};
#endif
/*####### DSL-401HNU-B1B_v2 END ########*/
#endif

#endif

#if defined(_BCM96828_) || defined(CONFIG_BCM96828)

static bp_elem_t g_bcm96828epon[] = {
  {bp_cpBoardId,               .u.cp = "96828MBV"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_PHY |
                                       BP_OVERLAY_SERIAL_LEDS |
                                       BP_OVERLAY_USB_DEVICE)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_3_AL},
  {bp_usGpioLedSesWireless,    .u.us = BP_SERIAL_GPIO_7_AL},
  {bp_usGpioLedWanData,        .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_2_AL},
  {bp_usGpioLedBlPowerOn,      .u.us = BP_GPIO_20_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_21_AL},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x5f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18},
  {bp_ulPhyId6,                .u.ul = 0x19},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_usGpioVoip1Led,          .u.us = BP_SERIAL_GPIO_4_AL},
  {bp_usGpioVoip2Led,          .u.us = BP_SERIAL_GPIO_5_AL},
  {bp_usGpioPotsLed,           .u.us = BP_SERIAL_GPIO_6_AL},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6302 | BP_AFE_FE_ANNEXA | BP_AFE_FE_REV_6302_REV_7_2_30},
  {bp_usGpioExtAFEReset,       .u.us = BP_GPIO_17_AL},
  {bp_last}
};

static bp_elem_t * g_BoardParms[] = {g_bcm96828epon, 0};

#endif

#if defined(_BCM96318_) || defined(CONFIG_BCM96318)

static bp_elem_t g_bcm96318sv[] = {
  {bp_cpBoardId,               .u.cp = "96318SV"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 )},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_10_AL},
 // {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_12_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_9_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_11_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18 | MAC_IFACE_VALID | MAC_IF_RGMII | PHY_INTEGRATED_VALID | PHY_EXTERNAL},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_3},
  {bp_last}
};

#ifdef BCM96318SV_SERIAL_LED
static bp_elem_t g_bcm96318sv_sersw[] = {
  {bp_cpBoardId,               .u.cp = "96318SV_SERSW"},
  {bp_ulGpioOverlay,           .u.ul = (BP_OVERLAY_SERIAL_LEDS)},
  {bp_usSerialLEDMuxSel,       .u.us = (BP_SERIAL_MUX_SEL_GROUP0|BP_SERIAL_MUX_SEL_GROUP2)},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_10_AL},
 // {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_12_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_9_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_11_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18 | MAC_IFACE_VALID | MAC_IF_RGMII | PHY_INTEGRATED_VALID | PHY_EXTERNAL},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_3},
  {bp_last}
};

static bp_elem_t g_bcm96318sv_serled[] = {
  {bp_cpBoardId,               .u.cp = "96318SV_SERLED"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 |
                                       BP_OVERLAY_SERIAL_LEDS)},
  {bp_usGpioLedAdsl,           .u.us = BP_SERIAL_GPIO_10_AL},
 // {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_12_AL},
  {bp_usGpioLedWanData,        .u.us = BP_SERIAL_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_SERIAL_GPIO_9_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_SERIAL_GPIO_11_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_1},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x1f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ulPhyId4,                .u.ul = 0x18 | MAC_IFACE_VALID | MAC_IF_RGMII | PHY_INTEGRATED_VALID | PHY_EXTERNAL},  
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_3},
  {bp_last}
};
#endif

static bp_elem_t g_bcm96318ref[] = {
  {bp_cpBoardId,               .u.cp = "96318REF"},
  {bp_ulGpioOverlay,           .u.ul =(BP_OVERLAY_EPHY_LED_0 |
                                       BP_OVERLAY_EPHY_LED_1 |
                                       BP_OVERLAY_EPHY_LED_2 |
                                       BP_OVERLAY_EPHY_LED_3 )},
  {bp_usGpioLedAdsl,           .u.us = BP_GPIO_10_AL},
 // {bp_usGpioLedSesWireless,    .u.us = BP_GPIO_12_AL},
  {bp_usGpioLedWanData,        .u.us = BP_GPIO_8_AL},
  {bp_usGpioLedWanError,       .u.us = BP_GPIO_9_AL},
  {bp_usGpioLedBlStop,         .u.us = BP_GPIO_11_AL},
  {bp_usExtIntrResetToDefault, .u.us = BP_EXT_INTR_1},
  {bp_usExtIntrSesBtnWireless, .u.us = BP_EXT_INTR_0},
  {bp_usAntInUseWireless,      .u.us = BP_WLAN_ANT_MAIN},
  {bp_usWirelessFlags,         .u.us = 0},
  {bp_ucPhyType0,              .u.uc = BP_ENET_EXTERNAL_SWITCH},
  {bp_ucPhyAddress,            .u.uc = 0x0},
  {bp_usConfigType,            .u.us = BP_ENET_CONFIG_MMAP},
  {bp_ulPortMap,               .u.ul = 0x0f},
  {bp_ulPhyId0,                .u.ul = 0x01},
  {bp_ulPhyId1,                .u.ul = 0x02},
  {bp_ulPhyId2,                .u.ul = 0x03},
  {bp_ulPhyId3,                .u.ul = 0x04},
  {bp_ucDspType0,              .u.uc = BP_VOIP_MIPS},
  {bp_ucDspAddress,            .u.uc = 0},
  {bp_ulAfeId0,                .u.ul = BP_AFE_CHIP_INT | BP_AFE_LD_6301 | BP_AFE_FE_ANNEXA | BP_AFE_FE_AVMODE_ADSL | BP_AFE_FE_REV_6301_REV_5_1_3},
  {bp_last}
};
#ifdef BCM96318SV_SERIAL_LED
static bp_elem_t * g_BoardParms[] = {g_bcm96318sv, g_bcm96318ref, g_bcm96318sv_serled, g_bcm96318sv_sersw, 0};
#else
static bp_elem_t * g_BoardParms[] = {g_bcm96318sv, g_bcm96318ref, 0};
#endif

#endif

static bp_elem_t * g_pCurrentBp = 0;

/* Private function prototypes */
bp_elem_t * BpGetElem(enum bp_id id, bp_elem_t **pstartElem, enum bp_id stopAtId);
char *BpGetSubCp(enum bp_id id, bp_elem_t *pstartElem, enum bp_id stopAtId );
unsigned char BpGetSubUc(enum bp_id id, bp_elem_t *pstartElem, enum bp_id stopAtId );
unsigned short BpGetSubUs(enum bp_id id, bp_elem_t *pstartElem, enum bp_id stopAtId );
unsigned long BpGetSubUl(enum bp_id id, bp_elem_t *pstartElem, enum bp_id stopAtId );
int BpGetCp(enum bp_id id, char **pcpValue );
int BpGetUc(enum bp_id id, unsigned char *pucValue );
int BpGetUs(enum bp_id id, unsigned short *pusValue );
int BpGetUl(enum bp_id id, unsigned long *pulValue );
int BpGetGpio(enum bp_id id, unsigned short *pusValue );

/**************************************************************************
* Name       : bpstrcmp
*
* Description: String compare for this file so it does not depend on an OS.
*              (Linux kernel and CFE share this source file.)
*
* Parameters : [IN] dest - destination string
*              [IN] src - source string
*
* Returns    : -1 - dest < src, 1 - dest > src, 0 dest == src
***************************************************************************/
int bpstrcmp(const char *dest,const char *src)
{
    while (*src && *dest) {
        if (*dest < *src) return -1;
        if (*dest > *src) return 1;
        dest++;
        src++;
    }

    if (*dest && !*src) return 1;
    if (!*dest && *src) return -1;
    return 0;
} /* bpstrcmp */


/**************************************************************************
* Name       : BpGetElem
*
* Description: Private function to walk through the profile
*              and find the desired entry
*
* Parameters : [IN] id             - id to search for
*              [IN/OUT] pstartElem - where to start and where it was found
*              [IN] stopAtId       - id to stop at if the searched id is not found
*                                    (allows grouping and repeated ids)
*
* Returns    : ptr to entry found or to last entry otherwise
***************************************************************************/
bp_elem_t * BpGetElem(enum bp_id id, bp_elem_t **pstartElem, enum bp_id stopAtId)
{
    bp_elem_t * pelem;
    
    // when compiling CFE, it does not like 'NULL' hence using 0
    if ( 0 == *pstartElem )
        *pstartElem = g_pCurrentBp;

    for (pelem = *pstartElem; 
         pelem->id != bp_last && pelem->id != id && pelem->id != stopAtId; 
         pelem++, (*pstartElem)++) 
    {
        // found template so jump to it
        // any entries after bp_elemTemplate will be ignored
        if ( bp_elemTemplate == pelem->id ) {
            *pstartElem = pelem->u.bp_elemp;
            pelem = *pstartElem;
            // ignoring the first element of this new array
            // because it is always bp_cpBoardId
        }
    }

    return pelem; 
}

/**************************************************************************
* Name       : BpGetSubCp
*
* Description: Private function to get an char * entry from the profile
*              can be used to search an id within a group by specifying stop id
*
* Parameters : [IN] id         - id to search for
*              [IN] pstartElem - where to start from
*              [IN] stopAtId   - id to stop at if the searched id is not found
*                                (allows grouping and repeated ids)
*
* Returns    : the unsigned char * from the entry
***************************************************************************/
char *BpGetSubCp(enum bp_id id, bp_elem_t *pstartElem, enum bp_id stopAtId )
{
    bp_elem_t *pelem;

    pelem = BpGetElem(id, &pstartElem, stopAtId);
    if (id == pelem->id) { 
        return pelem->u.cp;
    } else { 
        return (char *)BP_NOT_DEFINED;
    }
}

/**************************************************************************
* Name       : BpGetSubUc
*
* Description: Private function to get an unsigned char entry from the profile
*              can be used to search an id within a group by specifying stop id
*
* Parameters : [IN] id         - id to search for
*              [IN] pstartElem - where to start from
*              [IN] stopAtId   - id to stop at if the searched id is not found
*                                (allows grouping and repeated ids)
*
* Returns    : the unsigned char from the entry
***************************************************************************/
unsigned char BpGetSubUc(enum bp_id id, bp_elem_t *pstartElem, enum bp_id stopAtId )
{
    bp_elem_t *pelem;

    pelem = BpGetElem(id, &pstartElem, stopAtId);
    if (id == pelem->id) {
        return pelem->u.uc;
    } else {
        return (unsigned char)BP_NOT_DEFINED;
    }
}

/**************************************************************************
* Name       : BpGetSubUs
*
* Description: Private function to get an unsigned short entry from the profile
*              can be used to search an id within a group by specifying stop id
*
* Parameters : [IN] id         - id to search for
*              [IN] pstartElem - where to start from
*              [IN] stopAtId   - id to stop at if the searched id is not found
*                                (allows grouping and repeated ids)
*
* Returns    : the unsigned short from the entry
***************************************************************************/
unsigned short BpGetSubUs(enum bp_id id, bp_elem_t *pstartElem, enum bp_id stopAtId )
{
    bp_elem_t *pelem;

    pelem = BpGetElem(id, &pstartElem, stopAtId);
    if (id == pelem->id) {
        return pelem->u.us;
    } else {
        return BP_NOT_DEFINED;
    }
}

/**************************************************************************
* Name       : BpGetSubUl
*
* Description: Private function to get an unsigned long entry from the profile
*              can be used to search an id within a group by specifying stop id
*
* Parameters : [IN] id         - id to search for
*              [IN] pstartElem - where to start from
*              [IN] stopAtId   - id to stop at if the searched id is not found
*                                (allows grouping and repeated ids)
*
* Returns    : the unsigned long from the entry
***************************************************************************/
unsigned long BpGetSubUl(enum bp_id id, bp_elem_t *pstartElem, enum bp_id stopAtId )
{
    bp_elem_t *pelem;

    pelem = BpGetElem(id, &pstartElem, stopAtId);
    if (id == pelem->id)
        return pelem->u.ul;
    else
        return BP_NOT_DEFINED;
}

/**************************************************************************
* Name       : BpGetCp
*
* Description: Private function to get an char * entry from the profile
*              can only be used to search an id which appears once in the profile
*
* Parameters : [IN] id       - id to search for
*              [IN] pulValue - char ** found
*
* Returns    : BP_SUCCESS or appropriate error
***************************************************************************/
int BpGetCp(enum bp_id id, char **pcpValue )
{
    int nRet;

    if( g_pCurrentBp ) {
        *pcpValue = BpGetSubCp(id, 0, bp_last);
        nRet = ((char *)BP_NOT_DEFINED != *pcpValue ? BP_SUCCESS : BP_VALUE_NOT_DEFINED);
    } else {     
        *pcpValue = (char *)BP_NOT_DEFINED;
        nRet = BP_BOARD_ID_NOT_SET;
    }

    return( nRet );
}

/**************************************************************************
* Name       : BpGetUc
*
* Description: Private function to get an unsigned char entry from the profile
*              can only be used to search an id which appears once in the profile
*
* Parameters : [IN] id       - id to search for
*              [IN] pucValue - unsigned char found
*
* Returns    : BP_SUCCESS or appropriate error
***************************************************************************/
int BpGetUc(enum bp_id id, unsigned char *pucValue )
{
    int nRet;

    if( g_pCurrentBp ) {
        *pucValue = BpGetSubUc(id, 0, bp_last);
        nRet = ((unsigned char)BP_NOT_DEFINED != *pucValue ? BP_SUCCESS : BP_VALUE_NOT_DEFINED);
    } else {
        *pucValue = (unsigned char)BP_NOT_DEFINED;
        nRet = BP_BOARD_ID_NOT_SET;
    }

    return( nRet );
}

/**************************************************************************
* Name       : BpGetUs
*
* Description: Private function to get an unsigned short entry from the profile
*              can only be used to search an id which appears once in the profile
*
* Parameters : [IN] id       - id to search for
*              [IN] pusValue - unsigned short found
*
* Returns    : BP_SUCCESS or appropriate error
***************************************************************************/
int BpGetUs(enum bp_id id, unsigned short *pusValue )
{
    int nRet;

    if( g_pCurrentBp ) {
        *pusValue = BpGetSubUs(id, 0, bp_last);
        nRet = (BP_NOT_DEFINED != *pusValue ? BP_SUCCESS : BP_VALUE_NOT_DEFINED);
    } else {
        *pusValue = BP_NOT_DEFINED;
        nRet = BP_BOARD_ID_NOT_SET;
    }

    return( nRet );
}

/**************************************************************************
* Name       : BpGetUl
*
* Description: Private function to get an unsigned long entry from the profile
*              can only be used to search an id which appears once in the profile
*
* Parameters : [IN] id       - id to search for
*              [IN] pulValue - unsigned long found
*
* Returns    : BP_SUCCESS or appropriate error
***************************************************************************/
int BpGetUl(enum bp_id id, unsigned long *pulValue )
{
    int nRet;

    if( g_pCurrentBp ) {
        *pulValue = BpGetSubUl(id, 0, bp_last);
        nRet = (BP_NOT_DEFINED != *pulValue ? BP_SUCCESS : BP_VALUE_NOT_DEFINED);
    } else {
        *pulValue = BP_NOT_DEFINED;
        nRet = BP_BOARD_ID_NOT_SET;
    }

    return( nRet );
}

/**************************************************************************
* Name       : BpGetVoipDspConfig
*
* Description: Gets the DSP configuration from the board parameter
*              structure for a given DSP index.
*
* Parameters : [IN] dspNum - DSP index (number)
*
***************************************************************************/
VOIP_DSP_INFO g_VoIPDspInfo[BP_MAX_VOIP_DSP] = {{0}};
VOIP_DSP_INFO *BpGetVoipDspConfig( unsigned char dspNum )
{
    VOIP_DSP_INFO *pDspConfig = 0;
    int i;
    bp_elem_t *pelem;
    bp_elem_t *pDspType;
    enum bp_id bp_aucDspType[BP_MAX_VOIP_DSP+1] = {bp_ucDspType0, bp_ucDspType1, bp_last};
    enum bp_id bp_current, bp_next;

    if( g_pCurrentBp ) {
        /* First initialize the structure to known values */
        for( i = 0 ; i < BP_MAX_VOIP_DSP ; i++ ) {
            g_VoIPDspInfo[i].ucDspType = BP_VOIP_NO_DSP;
        }

        /* Now populate it with what we have in the element array */
        for( i = 0 ; i < BP_MAX_VOIP_DSP ; i++ ) {
            pDspType = 0;
            bp_current = bp_aucDspType[i];
            bp_next    = bp_aucDspType[i+1];
            pelem = BpGetElem(bp_current, &pDspType, bp_next);
            if (bp_current != pelem->id) {
                continue;
            }
            g_VoIPDspInfo[i].ucDspType = pelem->u.uc;

            ++pDspType;
            g_VoIPDspInfo[i].ucDspAddress       = BpGetSubUc(bp_ucDspAddress, pDspType, bp_next);
            g_VoIPDspInfo[i].usGpioLedVoip      = BpGetSubUs(bp_usGpioLedVoip, pDspType, bp_next);
            g_VoIPDspInfo[i].usGpioVoip1Led     = BpGetSubUs(bp_usGpioVoip1Led, pDspType, bp_next);
            g_VoIPDspInfo[i].usGpioVoip1LedFail = BpGetSubUs(bp_usGpioVoip1LedFail, pDspType, bp_next);
            g_VoIPDspInfo[i].usGpioVoip2Led     = BpGetSubUs(bp_usGpioVoip2Led, pDspType, bp_next);
            g_VoIPDspInfo[i].usGpioVoip2LedFail = BpGetSubUs(bp_usGpioVoip2LedFail, pDspType, bp_next);
            g_VoIPDspInfo[i].usGpioPotsLed      = BpGetSubUs(bp_usGpioPotsLed, pDspType, bp_next);
            g_VoIPDspInfo[i].usGpioDectLed      = BpGetSubUs(bp_usGpioDectLed, pDspType, bp_next);
        }

        /* Transfer the requested results */
        for( i = 0 ; i < BP_MAX_VOIP_DSP ; i++ ) {
            if( g_VoIPDspInfo[i].ucDspType != BP_VOIP_NO_DSP &&
                g_VoIPDspInfo[i].ucDspAddress == dspNum ) {
                pDspConfig = &g_VoIPDspInfo[i];
                break;
            }
        }
    }

    return pDspConfig;
} /* BpGetVoipDspConfig */

/**************************************************************************
* Name       : BpSetBoardId
*
* Description: This function find the BOARD_PARAMETERS structure for the
*              specified board id string and assigns it to a global, static
*              variable.
*
* Parameters : [IN] pszBoardId - Board id string that is saved into NVRAM.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_FOUND - Error, board id input string does not
*                  have a board parameters configuration record.
***************************************************************************/
int BpSetBoardId( char *pszBoardId )
{
    int nRet = BP_BOARD_ID_NOT_FOUND;
    bp_elem_t **ppcBp;

    for( ppcBp = g_BoardParms; *ppcBp; ppcBp++ ) {
        if( !bpstrcmp((*ppcBp)[0].u.cp, pszBoardId) ) {
            g_pCurrentBp = *ppcBp;
            nRet = BP_SUCCESS;
            break;
        }
    }

    return( nRet );
} /* BpSetBoardId */

#if defined(CONFIG_ZYXEL_VMG1312)
/*__ZyXEL__, Cj_Lai, Support VMG1312-B10A and B10B difference GPIO in board -> Add NVRAM_DATA */
int BpSetBoardId_DiffGPIO( char *pszBoardId ,NVRAM_DATA *nvRam)
{
	int nRet = BP_BOARD_ID_NOT_FOUND;
	bp_elem_t **ppcBp;

	if( (!bpstrcmp(nvRam->ProductEXTName,MODEL_NAME_10B)) || (!bpstrcmp(nvRam->ProductEXTName,MODEL_NAME_30B)) ){
		for( ppcBp = g_BoardParms_v4; *ppcBp; ppcBp++ ) {
			if( !bpstrcmp((*ppcBp)[0].u.cp, pszBoardId) ) {
			g_pCurrentBp = *ppcBp;
			nRet = BP_SUCCESS;
			break;
			}
		}
	}else if( (!bpstrcmp(nvRam->ProductEXTName,MODEL_NAME_10A)) || (!bpstrcmp(nvRam->ProductEXTName,MODEL_NAME_30A)) ){
		for( ppcBp = g_BoardParms_v2; *ppcBp; ppcBp++ ) {
			if( !bpstrcmp((*ppcBp)[0].u.cp, pszBoardId) ) {
			g_pCurrentBp = *ppcBp;
			nRet = BP_SUCCESS;
			break;
			}
		}
	}else{
		for( ppcBp = g_BoardParms; *ppcBp; ppcBp++ ) {
			if( !bpstrcmp((*ppcBp)[0].u.cp, pszBoardId) ) {
			g_pCurrentBp = *ppcBp;
			nRet = BP_SUCCESS;
			break;
			}
		}
	}

    return( nRet );
} /* BpSetBoardId_DiffGPIO */
#endif

/**************************************************************************
* Name       : BpGetBoardId
*
* Description: This function returns the current board id strings.
*
* Parameters : [OUT] pszBoardIds - Address of a buffer that the board id
*                  string is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
***************************************************************************/
int BpGetBoardId( char *pszBoardId )
{
    int i;

    if (g_pCurrentBp == 0) {
        return -1;
    }

    for (i = 0; i < BP_BOARD_ID_LEN; i++) {
        pszBoardId[i] = g_pCurrentBp[0].u.cp[i];
    }

    return 0;
}

/**************************************************************************
* Name       : BpGetBoardIdNameByIndex
*
* Description: This function returns the pointer to the board id name indexed by i. 
*
* Parameters : [OUT] Address of a buffer that the board id
*                    string is returned in.
*              [IN]  Index of the board in the g_BoardParms array.            
*
* Returns    : BP_SUCCESS - Success, value is returned.
***************************************************************************/
char * BpGetBoardIdNameByIndex( int i )
{
    return g_BoardParms[i][0].u.cp;
}


/**************************************************************************
* Name       : BpGetBoardIds
*
* Description: This function returns all of the supported board id strings.
*
* Parameters : [OUT] pszBoardIds - Address of a buffer that the board id
*                  strings are returned in.  Each id starts at BP_BOARD_ID_LEN
*                  boundary.
*              [IN] nBoardIdsSize - Number of BP_BOARD_ID_LEN elements that
*                  were allocated in pszBoardIds.
*
* Returns    : Number of board id strings returned.
***************************************************************************/
int BpGetBoardIds( char *pszBoardIds, int nBoardIdsSize )
{
    int i;
    char *src;
    char *dest;
    bp_elem_t **ppcBp;

    for( i = 0, ppcBp = g_BoardParms; *ppcBp && nBoardIdsSize;
        i++, ppcBp++, nBoardIdsSize--, pszBoardIds += BP_BOARD_ID_LEN ) {
        dest = pszBoardIds;
        src = (*ppcBp)[0].u.cp;
        while( *src ) {
            *dest++ = *src++;
        }
        *dest = '\0';
    }

    return( i );
} /* BpGetBoardIds */

#if defined(CONFIG_ZYXEL_VMG1312)
/*__ZyXEL__, Cj_Lai, Support VMG1312-B10A and B10B difference GPIO in board -> Add NVRAM_DATA */
int BpGetBoardIds_DiffGPIO( char *pszBoardIds, int nBoardIdsSize ,NVRAM_DATA *nvRam)
{
	int i;
	char *src;
	char *dest;
	bp_elem_t **ppcBp;

	if( (!bpstrcmp(nvRam->ProductEXTName,MODEL_NAME_10B)) || (!bpstrcmp(nvRam->ProductEXTName,MODEL_NAME_30B)) ){
		for( i = 0, ppcBp = g_BoardParms_v4; *ppcBp && nBoardIdsSize;
			i++, ppcBp++, nBoardIdsSize--, pszBoardIds += BP_BOARD_ID_LEN ) {
			dest = pszBoardIds;
			src = (*ppcBp)[0].u.cp;
			while( *src ) {
			*dest++ = *src++;
			}
			*dest = '\0';
		}
	}else if( (!bpstrcmp(nvRam->ProductEXTName,MODEL_NAME_10A)) || (!bpstrcmp(nvRam->ProductEXTName,MODEL_NAME_30A))){
		for( i = 0, ppcBp = g_BoardParms_v2; *ppcBp && nBoardIdsSize;
			i++, ppcBp++, nBoardIdsSize--, pszBoardIds += BP_BOARD_ID_LEN ) {
			dest = pszBoardIds;
			src = (*ppcBp)[0].u.cp;
			while( *src ) {
			*dest++ = *src++;
			}
			*dest = '\0';
		}
	}else{
		for( i = 0, ppcBp = g_BoardParms; *ppcBp && nBoardIdsSize;
			i++, ppcBp++, nBoardIdsSize--, pszBoardIds += BP_BOARD_ID_LEN ) {
			dest = pszBoardIds;
			src = (*ppcBp)[0].u.cp;
			while( *src ) {
			*dest++ = *src++;
			}
			*dest = '\0';
		}
	}

    return( i );
} /* BpGetBoardIds_DiffGPIO */
#endif

int BPGetNumBoardIds(void)
{
    int numElements = sizeof(g_BoardParms) / sizeof(bp_elem_t *);
    return ( numElements - 1);
}


/**************************************************************************
* Name       : BpGetComment
*
* Description: This function returns is used to get a comment for a board.
*
* Parameters : [OUT] pcpValue - comment string.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetComment( char **pcpValue )
{
    return( BpGetCp(bp_cpComment, pcpValue ) );
} /* BpGetComment */

/**************************************************************************
* Name       : BpGetGPIOverlays
*
* Description: This function GPIO overlay configuration
*
* Parameters : [OUT] pusValue - Address of short word that interfaces in use.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetGPIOverlays( unsigned long *pulValue )
{
    return( BpGetUl(bp_ulGpioOverlay, pulValue ) );
} /* BpGetGPIOverlays */


/**************************************************************************
* Name       : BpGetEthernetMacInfo
*
* Description: This function returns all of the supported board id strings.
*
* Parameters : [OUT] pEnetInfos - Address of an array of ETHERNET_MAC_INFO
*                  buffers.
*              [IN] nNumEnetInfos - Number of ETHERNET_MAC_INFO elements that
*                  are pointed to by pEnetInfos.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
***************************************************************************/
int BpGetEthernetMacInfo( PETHERNET_MAC_INFO pEnetInfos, int nNumEnetInfos )
{
    int i, j;
    bp_elem_t *pelem;
    bp_elem_t *pPhyType;
    bp_elem_t *pPhyId;
    int nRet = BP_BOARD_ID_NOT_SET;
    PETHERNET_MAC_INFO pE;
    enum bp_id bp_aucPhyType[BP_MAX_ENET_MACS+1] = {bp_ucPhyType0, bp_ucPhyType1, bp_last};
    enum bp_id bp_current, bp_next;
    enum bp_id bp_aulPhyId[BP_MAX_SWITCH_PORTS+1] = {bp_ulPhyId0, bp_ulPhyId1, bp_ulPhyId2,
                bp_ulPhyId3, bp_ulPhyId4, bp_ulPhyId5, bp_ulPhyId6, bp_ulPhyId7, bp_last};
    enum bp_id bp_current_phyid;

    /* First initialize the structure to known values */
    for( i = 0, pE = pEnetInfos; i < nNumEnetInfos; i++, pE++ ) {
        pE->ucPhyType = BP_ENET_NO_PHY;
        /* The old code only initialized the first set, let's tdo the same so the 2 compare without error */
        if (0 == i) {
            for( j = 0; j < BP_MAX_ENET_INTERNAL; j++ ) {
                pE->sw.ledInfo[j].duplexLed = BP_NOT_DEFINED;
                pE->sw.ledInfo[j].speedLed100 = BP_NOT_DEFINED;
                pE->sw.ledInfo[j].speedLed1000 = BP_NOT_DEFINED;
            }
        }
    }

    if( g_pCurrentBp ) {
        /* Populate it with what we have in the element array */
        for( i = 0, pE = pEnetInfos; i < BP_MAX_ENET_MACS; i++, pE++ ) {
            pPhyType = 0;
            bp_current = bp_aucPhyType[i];
            bp_next    = bp_aucPhyType[i+1];
            pelem = BpGetElem(bp_current, &pPhyType, bp_next);
            if (bp_current != pelem->id)
                continue;
            pE->ucPhyType = pelem->u.uc;

            ++pPhyType;
            pE->ucPhyAddress  = BpGetSubUc(bp_ucPhyAddress, pPhyType, bp_next);
            pE->usConfigType  = BpGetSubUs(bp_usConfigType, pPhyType, bp_next);
            pE->sw.port_map   = BpGetSubUl(bp_ulPortMap, pPhyType, bp_next);

            for( j = 0; j < BP_MAX_SWITCH_PORTS; j++ ) {
                pPhyId = pPhyType;
                bp_current_phyid = bp_aulPhyId[j];
                pelem = BpGetElem(bp_current_phyid, &pPhyId, bp_next);
                pE->sw.phyconn[j] = PHY_CONN_TYPE_NOT_DEFINED;
                pE->sw.phy_devName[j] = PHY_DEVNAME_NOT_DEFINED;
                pE->sw.phyinit[j] = (bp_mdio_init_t* )0;
                if (bp_current_phyid == pelem->id) {
                    pE->sw.phy_id[j] = pelem->u.ul & ~MII_OVER_GPIO_VALID;
                    ++pPhyId;
                        while (pPhyId) {
                            switch (pPhyId->id) {
                            case bp_usDuplexLed:
                                pE->sw.ledInfo[j].duplexLed = pPhyId->u.us;
                                ++pPhyId;
                                break;
                            case bp_usSpeedLed100:
                                pE->sw.ledInfo[j].speedLed100 = pPhyId->u.us;
                                ++pPhyId;
                                break;
                            case bp_usSpeedLed1000:
                                pE->sw.ledInfo[j].speedLed1000 = pPhyId->u.us;
                                ++pPhyId;
                                break;
                        case bp_usPhyConnType:
                            pE->sw.phyconn[j] = pPhyId->u.us;
                            ++pPhyId;
                            break;
                        case bp_ucPhyDevName:
                            pE->sw.phy_devName[j] = pPhyId->u.cp;
                            ++pPhyId;
                            break;                            
                        case bp_pPhyInit:
                             pE->sw.phyinit[j] = pPhyId->u.phyinitp;
                             ++pPhyId;
                            break;
                            default:
                                pPhyId = 0;
                                break;
                            }
                        }
                } else {
                    pE->sw.phy_id[j] = 0;
                }
            }
        }
        nRet = BP_SUCCESS;
    }
    return( nRet );

} /* BpGetEthernetMacInfo */

#if defined(CONFIG_BCM963268) && defined(CONFIG_BCM_EXT_SWITCH)
int BpGetPortConnectedToExtSwitch(void)
{
    unsigned long phy_id, port_map;
    int i, extSwConnPort = -1;

    BpGetUl(bp_ulPortMap, &port_map);

    for (i = 0; i < BP_MAX_SWITCH_PORTS; i++) {
        if ( port_map &  (1 << i)) {
            BpGetUl(bp_ulPhyId0 + i, &phy_id);
            if (phy_id & EXTSW_CONNECTED)
            {
                extSwConnPort = i;
            }
        }
    }
    return extSwConnPort;
}
#endif

/**************************************************************************
* Name       : BpGetMiiOverGpioFlag
*
* Description: This function returns logical disjunction of MII over GPIO
*              flag over all PHY IDs.
*
* Parameters : [OUT] pMiiOverGpioFlag - MII over GPIO flag
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
***************************************************************************/
int BpGetMiiOverGpioFlag( unsigned long* pMiiOverGpioFlag )
{
    int i, j;
    bp_elem_t *pelem;
    bp_elem_t *pPhyType;
    bp_elem_t *pPhyId;
    int nRet;
    enum bp_id bp_aucPhyType[BP_MAX_ENET_MACS+1] = {bp_ucPhyType0, bp_ucPhyType1, bp_last};
    enum bp_id bp_current, bp_next;
    enum bp_id bp_aulPhyId[BP_MAX_SWITCH_PORTS+1] = {bp_ulPhyId0, bp_ulPhyId1, bp_ulPhyId2,
                bp_ulPhyId3, bp_ulPhyId4, bp_ulPhyId5, bp_ulPhyId6, bp_ulPhyId7, bp_last};
    enum bp_id bp_current_phyid;

    *pMiiOverGpioFlag = 0;

    if( g_pCurrentBp ) {
        for( i = 0; i < BP_MAX_ENET_MACS; i++ ) {
            pPhyType = 0;
            bp_current = bp_aucPhyType[i];
            bp_next    = bp_aucPhyType[i+1];
            pelem = BpGetElem(bp_current, &pPhyType, bp_next);
            if (bp_current != pelem->id)
                continue;

            ++pPhyType;
            for( j = 0; j < BP_MAX_SWITCH_PORTS; j++ ) {
                pPhyId = pPhyType;
                bp_current_phyid = bp_aulPhyId[j];
                pelem = BpGetElem(bp_current_phyid, &pPhyId, bp_next);
                if (bp_current_phyid == pelem->id) {
                    *pMiiOverGpioFlag |= pelem->u.ul & MII_OVER_GPIO_VALID;                
                    ++pPhyId;
                }
            }
        }
        // Normalize flag value by positioning in lsb position        
        *pMiiOverGpioFlag >>= MII_OVER_GPIO_S;
        nRet = BP_SUCCESS;
    }
    else
    {
        nRet = BP_BOARD_ID_NOT_SET;
    }

    return( nRet );

} /* BpGetMiiOverGpioFlag */


/**************************************************************************
* Name       : BpGetGpio
*
* Description: Wrapper function to get the Gpio pin number
*
* Parameters : [IN] id       - id to search for
*              [IN] pusValue - unsigned short found
*
* Returns    : BP_SUCCESS or appropriate error
***************************************************************************/
int BpGetGpio(enum bp_id id, unsigned short *pusValue )
{
    int nRet;

    nRet = BpGetUs(id, pusValue );
    if(nRet == BP_SUCCESS && *pusValue == BP_GPIO_NONE)
    {
    	*pusValue = BP_NOT_DEFINED;
    	nRet = BP_VALUE_NOT_DEFINED;
    }

    return( nRet );
}


/**************************************************************************
* Name       : BpGetRj11InnerOuterPairGpios
*
* Description: This function returns the GPIO pin assignments for changing
*              between the RJ11 inner pair and RJ11 outer pair.
*
* Parameters : [OUT] pusInner - Address of short word that the RJ11 inner pair
*                  GPIO pin is returned in.
*              [OUT] pusOuter - Address of short word that the RJ11 outer pair
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, values are returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetRj11InnerOuterPairGpios( unsigned short *pusInner,
                                 unsigned short *pusOuter )
{
    *pusInner = BP_NOT_DEFINED;
    *pusOuter = BP_NOT_DEFINED;

    return( BP_VALUE_NOT_DEFINED );
} /* BpGetRj11InnerOuterPairGpios */

/**************************************************************************
* Name       : BpGetUartRtsCtsGpios
*
* Description: This function returns the GPIO pin assignments for RTS and CTS
*              UART signals.
*
* Parameters : [OUT] pusRts - Address of short word that the UART RTS GPIO
*                  pin is returned in.
*              [OUT] pusCts - Address of short word that the UART CTS GPIO
*                  pin is returned in.
*
* Returns    : BP_SUCCESS - Success, values are returned.
*              BP_BOARD_ID_NOT_SET - Error, board id input string does not
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetRtsCtsUartGpios( unsigned short *pusRts, unsigned short *pusCts )
{
    *pusRts = BP_NOT_DEFINED;
    *pusCts = BP_NOT_DEFINED;

    return( BP_VALUE_NOT_DEFINED );
} /* BpGetUartRtsCtsGpios */

/**************************************************************************
* Name       : BpGetAdslLedGpio
*
* Description: This function returns the GPIO pin assignment for the ADSL
*              LED.
*
* Parameters : [OUT] pusValue - Address of short word that the ADSL LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetAdslLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLedAdsl, pusValue ) );
} /* BpGetAdslLedGpio */

/**************************************************************************
* Name       : BpGetAdslFailLedGpio
*
* Description: This function returns the GPIO pin assignment for the ADSL
*              LED that is used when there is a DSL connection failure.
*
* Parameters : [OUT] pusValue - Address of short word that the ADSL LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetAdslFailLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLedAdslFail, pusValue ) );
} /* BpGetAdslFailLedGpio */

/**************************************************************************
* Name       : BpGetSecAdslLedGpio
*
* Description: This function returns the GPIO pin assignment for the ADSL
*              LED of the Secondary line, applicable more for bonding.
*
* Parameters : [OUT] pusValue - Address of short word that the ADSL LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetSecAdslLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioSecLedAdsl, pusValue ) );
} /* BpGetSecAdslLedGpio */

/**************************************************************************
* Name       : BpGetSecAdslFailLedGpio
*
* Description: This function returns the GPIO pin assignment for the ADSL
*              LED of the Secondary ADSL line, that is used when there is
*              a DSL connection failure.
*
* Parameters : [OUT] pusValue - Address of short word that the ADSL LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetSecAdslFailLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioSecLedAdslFail, pusValue ) );
} /* BpGetSecAdslFailLedGpio */

#if 1 //__MSTC__, RaynorChung: Ssupport VDSL LED
/**************************************************************************
* Name       : BpGetVdslLedGpio
*
* Description: This function returns the GPIO pin assignment for the VDSL
*              LED.
*
* Parameters : [OUT] pusValue - Address of short word that the VDSL LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetVdslLedGpio( unsigned short *pusValue )
{
    return( BpGetUs(bp_usGpioLedVdsl, pusValue ) );
} /* BpGetVdslLedGpio */

/**************************************************************************
* Name       : BpGetVdslFailLedGpio
*
* Description: This function returns the GPIO pin assignment for the VDSL
*              LED that is used when there is a DSL connection failure.
*
* Parameters : [OUT] pusValue - Address of short word that the VDSL LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetVdslFailLedGpio( unsigned short *pusValue )
{
    return( BpGetUs(bp_usGpioLedVdslFail, pusValue ) );
} /* BpGetVdslFailLedGpio */

/**************************************************************************
* Name       : BpGetSecVdslLedGpio
*
* Description: This function returns the GPIO pin assignment for the VDSL
*              LED of the Secondary line, applicable more for bonding.
*
* Parameters : [OUT] pusValue - Address of short word that the VDSL LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetSecVdslLedGpio( unsigned short *pusValue )
{
    return( BpGetUs(bp_usGpioSecLedVdsl, pusValue ) );
} /* BpGetSecVdslLedGpio */

/**************************************************************************
* Name       : BpGetSecVdslFailLedGpio
*
* Description: This function returns the GPIO pin assignment for the VDSL
*              LED of the Secondary VDSL line, that is used when there is
*              a DSL connection failure.
*
* Parameters : [OUT] pusValue - Address of short word that the VDSL LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetSecVdslFailLedGpio( unsigned short *pusValue )
{
    return( BpGetUs(bp_usGpioSecLedVdslFail, pusValue ) );
} /* BpGetSecVdslFailLedGpio */

#endif
#if 1 //__MSTC__, RaynorChung: Ssupport USB LED
/**************************************************************************
* Name       : BpGetUSB0LedGpio
*
* Description: This function returns the GPIO pin assignment for the VDSL
*              LED.
*
* Parameters : [OUT] pusValue - Address of short word that the VDSL LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetUSB0LedGpio( unsigned short *pusValue )
{
    return( BpGetUs(bp_usGpioLedUSB0, pusValue ) );
} /* BpGetVdslLedGpio */
int BpGetGWAN100MLedGpio( unsigned short *pusValue )
{
    return( BpGetUs(bp_usGpioGWAN100MLed, pusValue ) );
} /* BpGetVdslLedGpio */
int BpGetGWAN1000MLedGpio( unsigned short *pusValue )
{
    return( BpGetUs(bp_usGpioGWAN1000MLed, pusValue ) );
} /* BpGetVdslLedGpio */

/**************************************************************************
* Name       : BpGetUSB0lFailLedGpio
*
* Description: This function returns the GPIO pin assignment for the VDSL
*              LED that is used when there is a DSL connection failure.
*
* Parameters : [OUT] pusValue - Address of short word that the VDSL LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetUSB0FailLedGpio( unsigned short *pusValue )
{
    return( BpGetUs(bp_usGpioLedUSB0Fail, pusValue ) );
} /* BpGetVdslFailLedGpio */
/**************************************************************************
* Name       : BpGetUSB1LedGpio
*
* Description: This function returns the GPIO pin assignment for the VDSL
*              LED.
*
* Parameters : [OUT] pusValue - Address of short word that the VDSL LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetUSB1LedGpio( unsigned short *pusValue )
{
    return( BpGetUs(bp_usGpioLedUSB1, pusValue ) );
} /* BpGetVdslLedGpio */

/**************************************************************************
* Name       : BpGetUSB0lFailLedGpio
*
* Description: This function returns the GPIO pin assignment for the VDSL
*              LED that is used when there is a DSL connection failure.
*
* Parameters : [OUT] pusValue - Address of short word that the VDSL LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetUSB1FailLedGpio( unsigned short *pusValue )
{
    return( BpGetUs(bp_usGpioLedUSB1Fail, pusValue ) );
} /* BpGetVdslFailLedGpio */

#endif

/**************************************************************************
* Name       : BpGetWirelessAntInUse
*
* Description: This function returns the antennas in use for wireless
*
* Parameters : [OUT] pusValue - Address of short word that the Wireless Antenna
*                  is in use.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetWirelessAntInUse( unsigned short *pusValue )
{
    return( BpGetUs(bp_usAntInUseWireless, pusValue ) );
} /* BpGetWirelessAntInUse */

/**************************************************************************
* Name       : BpGetWirelessFlags
*
* Description: This function returns optional control flags for wireless
*
* Parameters : [OUT] pusValue - Address of short word control flags
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetWirelessFlags( unsigned short *pusValue )
{
    return( BpGetUs(bp_usWirelessFlags, pusValue ) );
} /* BpGetWirelessAntInUse */

/**************************************************************************
* Name       : BpGetWirelessSesExtIntr
*
* Description: This function returns the external interrupt number for the
*              Wireless Ses Button.
*
* Parameters : [OUT] pusValue - Address of short word that the Wireless Ses
*                  external interrup is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetWirelessSesExtIntr( unsigned short *pusValue )
{
    return( BpGetUs(bp_usExtIntrSesBtnWireless, pusValue ) );
} /* BpGetWirelessSesExtIntr */
#if 1 //__MSTC__, Eason, WLAN_WPS_BTN_SEPARATE
/**************************************************************************
* Name       : BpGetWirelessBtnExtIntr
*
* Description: This function returns the external interrupt number for the
*              Wireless Enable Button.
*
* Parameters : [OUT] pusValue - Address of short word that the Wireless Ses
*                  external interrup is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetWirelessBtnExtIntr( unsigned short *pusValue )
{
    return( BpGetUs(bp_usExtIntrWlanEnableBtnWireless, pusValue ) );
} /* BpGetWirelessSesExtIntr */

#endif

/**************************************************************************
* Name       : BpGetWirelessSesLedGpio
*
* Description: This function returns the GPIO pin assignment for the Wireless
*              Ses Led.
*
* Parameters : [OUT] pusValue - Address of short word that the Wireless Ses
*                  Led GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetWirelessSesLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLedSesWireless, pusValue ) );
} /* BpGetWirelessSesLedGpio */
#if 1 // Mandy WPS5GLED
int BpGetWireless5GWpsLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLed5GWps, pusValue ) );
} /* BpGetWireless5GWpsLedGpio */
#endif

#if defined(_BCM96362_) || defined(CONFIG_BCM96362)
/* The unique part contains the subSytemId (boardId) and the BoardRev number */
static WLAN_SROM_ENTRY wlan_patch_unique_96362ADVNX[] = {
    {2,  0x536},
    {65, 0x1100},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_unique_96362ADVNgr2[] = {
    {2,  0x580},
    {65, 0x1500},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_unique_963629epon[] = {
    {2,  0x5a5},
    {65, 0x1500},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_unique_96362ADVN2xh[] = {
    {2,  0x5a6},
    {65, 0x1200},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_unique_96361XF[] = {
    {2,  0x5b8},
    {65, 0x1100},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_unique_96362RAVNGR2[] = {
    {2,  0x60d},
    {65, 0x1500},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_unique_96362ADVN2XM[] = {
    {2,  0x60d},
    {65, 0x1500},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_unique_96362RADVNXH5[] = {
    {2,  0x63F},
    {65, 0x1100},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_unique_96362RADVN2XH[] = {
    {2,  0x63E},
    {65, 0x1100},
    {0, 0}
};

/* The common part contains the entries that are valid for multiple boards */
static WLAN_SROM_ENTRY wlan_patch_common_96362ADVNX[] = {
    { 87, 0x0319},
    { 96, 0x2058},
    { 97, 0xfe6f},
    { 98, 0x1785},
    { 99, 0xfa21},
    {112, 0x2058},
    {113, 0xfe77},
    {114, 0x17e0},
    {115, 0xfa16},
    {161, 0x5555},
    {162, 0x5555},
    {169, 0x5555},
    {170, 0x5555},
    {171, 0x5555},
    {172, 0x5555},
    {173, 0x3333},
    {174, 0x3333},
    {175, 0x3333},
    {176, 0x3333},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_common_96362ADVNgr2[] = {
    { 96, 0x2040},
    {100, 0x3c3c},
    {101, 0x3c38},    
    {112, 0x2040},
    {116, 0x3c3c},  
    {117, 0x3c38},
    {203, 0x2222},  
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_common_96361XF[] = {
    { 66, 0x0200},
    { 67, 0x0000},
    { 78, 0x0003},
    { 79, 0x0000},
    { 80, 0x0000},
    { 87, 0x0313},
    { 88, 0x0313},
    { 93, 0xffff},
    { 96, 0x2054},
    { 97, 0xfe80},
    { 98, 0x1fa2},
    { 99, 0xf897},
    {112, 0x2054},
    {113, 0xfe8c},
    {114, 0x1f13},
    {115, 0xf8c4},
    {161, 0x4444},
    {162, 0x4444},
    {169, 0x4444},
    {170, 0x4444},
    {171, 0x4444},
    {172, 0x4444},
    {203, 0x2222},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_common_96362ADVN2xh[] = {
    { 66, 0x3200},
    { 67, 0x8000},
    { 78, 0x0003},
    { 79, 0x0000},
    { 80, 0x0000},
    { 87, 0x031f},
    { 88, 0x031f},
    { 93, 0x0202},
    { 96, 0x2068},
    { 97, 0xfe80},
    { 98, 0x1fa2},
    { 99, 0xf897},
    {112, 0x2068},
    {113, 0xfe8c},
    {114, 0x1f13},
    {115, 0xf8c4},
    {203, 0x2222},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_common_96362ADVN2XM[] = {
    /* PA params not updated yet */
    { 65, 0x1200},                
    { 66, 0x3200},
    { 67, 0x8000},
    { 78, 0x0003},
    { 79, 0x0000},
    { 80, 0x0000},
    { 87, 0x031f},
    { 88, 0x031f},
    { 93, 0x0202},
    { 96, 0x205c},
    { 97, 0xfe80},
    { 98, 0x1fa2},
    { 99, 0xf897},
    {112, 0x205c},
    {113, 0xfe8c},
    {114, 0x1f13},
    {115, 0xf8c4},
    {203, 0x2222},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_common_96362RADVNXH5[] = {
    /* PA params not updated yet */
    { 66,  0x3200},
    { 67,  0x8000},
    { 78,  0x0300},
    { 87,  0x031f},
    { 88,  0x031f},
    { 93,  0x0202},
    { 100, 0x4060},
    { 116, 0x4060},
    { 161, 0x0000},
    { 162, 0x0000},
    { 169, 0x0000},
    { 170, 0x0000},
    { 171, 0x0000},
    { 172, 0x0000},
    { 203, 0x2222},
    {0, 0}
};

#endif

#if defined(_BCM963268_) || defined(CONFIG_BCM963268)
/* The unique part contains the subSytemId (boardId) and the BoardRev number */
static WLAN_SROM_ENTRY wlan_patch_unique_963268MBV[] = {
    {2,  0x5BB},
    {65, 0x1204},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_unique_963268V30A[] = {
    {2,  0x5E7},
    {65, 0x1101},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_unique_963268BU[] = {
    {2,  0x5A7},
    {65, 0x1201},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_unique_963168XH[] = {
    {2,  0x5E2},
    {65, 0x1100},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_unique_963168XM[] = {
    {2,  0x61F},
    {65, 0x1100},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_unique_963168XH5[] = {
    {2,  0x640},
    {65, 0x1100},
    {0, 0}
};

/* The common part contains the entries that are valid for multiple boards */
static WLAN_SROM_ENTRY wlan_patch_common_963268MBV[] = {
    { 78,  0x0303}, 
    { 79,  0x0202}, 
    { 80,  0xffff},
    { 87,  0x0315}, 
    { 88,  0x0315},
    { 96,  0x204A}, 
    { 97,  0xFF96}, 
    { 98,  0x17FF},
    { 99,  0xFAB6},
    { 100, 0x3E3C},
    { 101, 0x4038},
    { 102, 0xFF15},
    { 103, 0x1455},
    { 104, 0xFB95},
    { 108, 0xFF28},
    { 109, 0x1315},
    { 110, 0xFBF0},
    { 112, 0x204A},
    { 113, 0xFF84},
    { 114, 0x17F8},
    { 115, 0xFAD4},
    { 116, 0x3E3C},
    { 117, 0x4038},
    { 118, 0xFE87},
    { 119, 0x110F},
    { 120, 0xFB4B},
    { 124, 0xFF8D},
    { 125, 0x1146},
    { 126, 0xFCD6},
    { 203, 0x2220},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_common_963168XH[] = {
    { 66,  0x3200},
    { 67,  0x8000},
    { 87,  0x031f},
    { 88,  0x031f},
    { 93,  0x0202},
    { 96,  0x2068},
    { 97,  0xfe59},
    { 98,  0x1c81},
    { 99,  0xf911},
    { 112, 0x2068},
    { 113, 0xfe55},
    { 114, 0x1c00},
    { 115, 0xf9c2},
    { 161, 0x0000},
    { 162, 0x0000},
    { 169, 0x0000},
    { 170, 0x0000},
    { 171, 0x0000},
    { 172, 0x0000},
    { 203, 0x2222},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_common_963168XM[] = {
    { 66,  0x3200},
    { 67,  0x8000},
    { 87,  0x031f},
    { 88,  0x031f},
    { 93,  0x0202},
    { 96,  0x205c},
    { 97,  0xfe94},
    { 98,  0x1c92},
    { 99,  0xf948},
    { 112, 0x205c},
    { 113, 0xfe81},
    { 114, 0x1be6},
    { 115, 0xf947},
    { 161, 0x0000},
    { 162, 0x0000},
    { 169, 0x0000},
    { 170, 0x0000},
    { 171, 0x0000},
    { 172, 0x0000},
    { 203, 0x2222},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_common_963168XH5[] = {
    { 66,  0x3200},
    { 67,  0x8000},
    { 78,  0x0300},
    { 87,  0x031f},
    { 88,  0x031f},
    { 93,  0x0202},
    { 100, 0x4060},
    { 116, 0x4060},
    { 161, 0x0000},
    { 162, 0x0000},
    { 169, 0x0000},
    { 170, 0x0000},
    { 171, 0x0000},
    { 172, 0x0000},
    { 203, 0x2222},
    {0, 0}
};
#if 1
static WLAN_SROM_ENTRY wlan_patch_common_963168_VMGXXXX[] = {
    {0, 0}
};
static WLAN_SROM_ENTRY wlan_patch_unique_963168_VMG5313[] = {
    {0, 0}
};
#endif
#endif

#if defined(_BCM96828_) || defined(CONFIG_BCM96828)
/* The unique part contains the subSytemId (boardId) and the BoardRev number */
static WLAN_SROM_ENTRY wlan_patch_unique_96828MBV[] = {
    {65, 0x1230},
    {0, 0}
};

static WLAN_SROM_ENTRY wlan_patch_unique_96828BU[] = {
    {65, 0x1230},
    {0, 0}
};

/* The common part contains the entries that are valid for multiple boards */
static WLAN_SROM_ENTRY wlan_patch_common_96828MBV[] = {
    { 78,  0x0303}, 
    { 79,  0x0202}, 
    { 80,  0xff02},
    { 87,  0x0315}, 
    { 88,  0x0315},
    { 96,  0x2040}, 
    { 97,  0xFFB5}, 
    { 98,  0x175F},
    { 99,  0xFB29},
    { 112, 0x2040},
    { 113, 0xFFD7},
    { 114, 0x17D6},
    { 115, 0xFB67},
    {0, 0}
};

#endif

static WLAN_SROM_ENTRY wlan_patch_LAST[] = {
    {0, 0}
};

/* this data structure could be moved to boardparams structure in the future */
/* does not require to rebuild cfe here if more srom entries are needed */
static WLAN_SROM_PATCH_INFO wlanPaInfo[]={
#if defined(_BCM96362_) || defined(CONFIG_BCM96362)
    {"96362ADVNX",    0x6362, 220, wlan_patch_unique_96362ADVNX,    wlan_patch_common_96362ADVNX},
    {"96362ADVNgr2",  0x6362, 220, wlan_patch_unique_96362ADVNgr2,  wlan_patch_common_96362ADVNgr2},
    {"963629epon",    0x6362, 220, wlan_patch_unique_963629epon,    wlan_patch_common_96362ADVNgr2},
    {"96361XF",       0x6362, 220, wlan_patch_unique_96361XF,       wlan_patch_common_96361XF},
    {"96362RAVNGR2",  0x6362, 220, wlan_patch_unique_96362RAVNGR2,  wlan_patch_common_96362ADVNgr2},
    {"96362ADVN2xh",  0x6362, 220, wlan_patch_unique_96362ADVN2xh,  wlan_patch_common_96362ADVN2xh},
    {"96362ADVN2XM",  0x6362, 220, wlan_patch_unique_96362ADVN2XM,  wlan_patch_common_96362ADVN2XM},
    {"96362RADVN2XH", 0x6362, 220, wlan_patch_unique_96362RADVN2XH, wlan_patch_common_96362ADVN2xh},
    {"96362RADVNXH5", 0x6362, 220, wlan_patch_unique_96362RADVNXH5, wlan_patch_common_96362RADVNXH5},
#endif

#if defined(_BCM963268_) || defined(CONFIG_BCM963268)
    {"963268MBV",     0x6362, 220, wlan_patch_unique_963268MBV,     wlan_patch_common_963268MBV},
    {"963168MBV_17A", 0x6362, 220, wlan_patch_unique_963268MBV,     wlan_patch_common_963268MBV},
    {"963168MBV_30A", 0x6362, 220, wlan_patch_unique_963268MBV,     wlan_patch_common_963268MBV},
    {"963168MBV3",    0x6362, 220, wlan_patch_unique_963268MBV,     wlan_patch_common_963268MBV},
    {"963168MBV17A302",0x6362, 220, wlan_patch_unique_963268MBV,     wlan_patch_common_963268MBV},
    {"963168MBV30A302",0x6362, 220, wlan_patch_unique_963268MBV,     wlan_patch_common_963268MBV},
    {"963268V30A",    0x6362, 220, wlan_patch_unique_963268V30A,    wlan_patch_common_963268MBV},
    {"963268BU",      0x6362, 220, wlan_patch_unique_963268BU,      wlan_patch_common_963268MBV},
    {"963268BU_P300", 0x6362, 220, wlan_patch_unique_963268BU,      wlan_patch_common_963268MBV},
    {"963168XH",      0x6362, 220, wlan_patch_unique_963168XH,      wlan_patch_common_963168XH},
    {"963168XM",      0x6362, 220, wlan_patch_unique_963168XM,      wlan_patch_common_963168XM},
    {"963168XH5",     0x6362, 220, wlan_patch_unique_963168XH5,     wlan_patch_common_963168XH5},
    {"963168VX",  0x6362, 220, wlan_patch_unique_963268MBV,     wlan_patch_common_963268MBV},  
#if 1
    {"963168_VMG5313", 0x6362, 220, wlan_patch_unique_963168_VMG5313,	wlan_patch_common_963168_VMGXXXX},
#endif
#endif

#if defined(_BCM96828_) || defined(CONFIG_BCM96828)
    {"96828MBV",     0x6362, 220, wlan_patch_unique_96828MBV,       wlan_patch_common_96828MBV},
    {"96828BU",      0x6362, 220, wlan_patch_unique_96828BU,        wlan_patch_common_96828MBV},
#endif
      
    {"", 0, 0, wlan_patch_LAST, wlan_patch_LAST}, /* last entry*/
};

/**************************************************************************
* Name       : BpUpdateWirelessSromMap
*
* Description: This function patch wireless PA values
*
* Parameters : [IN] unsigned short chipID
*              [IN/OUT] unsigned short* pBase - base of srom map
*              [IN/OUT] int size - size of srom map
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpUpdateWirelessSromMap(unsigned short chipID, unsigned short* pBase, int sizeInWords)
{
    int nRet = BP_BOARD_ID_NOT_FOUND;
    int i = 0;
    int j = 0;

    if(chipID == 0 || pBase == 0 || sizeInWords <= 0 )
        return nRet;

    i = 0;
    while ( wlanPaInfo[i].szboardId[0] != 0 ) {
        /* check boardId */
        if ( !bpstrcmp(g_pCurrentBp[0].u.cp, wlanPaInfo[i].szboardId) ) {
            /* check chipId */
            if ( (wlanPaInfo[i].usWirelessChipId == chipID) && (wlanPaInfo[i].usNeededSize <= sizeInWords) ){

                /* valid , patch common to multiple boards entry */
                while ( wlanPaInfo[i].commonEntries[j].wordOffset != 0) {
                    pBase[wlanPaInfo[i].commonEntries[j].wordOffset] = wlanPaInfo[i].commonEntries[j].value;
                    j++;
                }

                j = 0;
                /* valid , patch board specific entry */
                while ( wlanPaInfo[i].uniqueEntries[j].wordOffset != 0) {
                    pBase[wlanPaInfo[i].uniqueEntries[j].wordOffset] = wlanPaInfo[i].uniqueEntries[j].value;
                    j++;
                }

                nRet = BP_SUCCESS;
                goto srom_update_done;
            }
        }
        i++;
    }

srom_update_done:

    return( nRet );

} /* BpUpdateWirelessSromMap */


static WLAN_PCI_PATCH_INFO wlanPciInfo[]={
#if defined(_BCM96362_) || defined(CONFIG_BCM96362)
    /* this is the patch to boardtype(boardid) for internal PA */
    {"96362ADVNX", 0x435f14e4, 64,
    {{"subpciids", 11, 0x53614e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 96362ADVNgr2 */
    {"96362ADVNgr2", 0x435f14e4, 64,
    {{"subpciids", 11, 0x58014e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 96362ADVN2xh */
    {"96362ADVN2xh", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5a614e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 96361XF */
    {"96361XF", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5b814e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 96361I2 */
    {"96361I2", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5e514e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 96362RAVNGR2 */
    {"96362RAVNGR2", 0x435f14e4, 64,
    {{"subpciids", 11, 0x60d14e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 96362ADVN2XM */
    {"96362ADVN2XM", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5d414e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 96362RADVNXH5 */
    {"96362RADVNXH5", 0x435f14e4, 64,
    {{"subpciids", 11, 0x63F14e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 96362RADVN2XH */
    {"96362RADVN2XH", 0x435f14e4, 64,
    {{"subpciids", 11, 0x63E14e4},
    {"",       0,      0}}},
#endif
#if defined(_BCM963268_) || defined(CONFIG_BCM963268)
    /* this is the patch to boardtype(boardid) for 63268MBV */
    {"963268MBV", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5BB14e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 963168MBV_17A */
    {"963168MBV_17A", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5BB14e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 963168MBV_30A */
    {"963168MBV_30A", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5BB14e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 963168MBV3 */
    {"963168MBV3", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5BB14e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 963168MBV17A302 */
    {"963168MBV17A302", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5BB14e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 963168MBV30A302 */
    {"963168MBV30A302", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5BB14e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 963268V30A */
    {"963268V30A", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5E714e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 63268BU */
    {"63268BU", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5A714e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 963168VX */
    {"963168VX", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5A814e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 963168VX_P300 */
    {"963168VX_P300", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5A814e4},
    {"",       0,      0}}},    
    /* this is the patch to boardtype(boardid) for 63168XH */
    {"963168XH", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5E214e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 63168XM */
    {"963168XM", 0x435f14e4, 64,
    {{"subpciids", 11, 0x61f14e4},
    {"",       0,      0}}},
    /* this is the patch to boardtype(boardid) for 963168XH5 */
    {"963168XH5", 0x435f14e4, 64,
    {{"subpciids", 11, 0x64014e4},
    {"",       0,      0}}},
#if 1
    /* this is the patch to boardtype(boardid) for VMG5313, reference 963168MBV_17A */
    {"963168_VMG5313", 0x435f14e4, 64,
    {{"subpciids", 11, 0x5BB14e4},
    {"",       0,      0}}},
#endif
#endif
    {"",                 0, 0, {{"",       0,      0}}}, /* last entry*/
};

/**************************************************************************
* Name       : BpUpdateWirelessPciConfig
*
* Description: This function patch wireless PCI Config Header
*              This is not functional critial/necessary but for dvt database maintenance
*
* Parameters : [IN] unsigned int pciID
*              [IN/OUT] unsigned int* pBase - base of pci config header
*              [IN/OUT] int sizeInDWords - size of pci config header
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpUpdateWirelessPciConfig (unsigned long pciID, unsigned long* pBase, int sizeInDWords)
{
    int nRet = BP_BOARD_ID_NOT_FOUND;
    int i = 0;
    int j = 0;

    if(pciID == 0 || pBase == 0 || sizeInDWords <= 0 )
        return nRet;

    i = 0;
    while ( wlanPciInfo[i].szboardId[0] != 0 ) {
        /* check boardId */
        if ( !bpstrcmp(g_pCurrentBp[0].u.cp, wlanPciInfo[i].szboardId) ) {
            /* check pciId */
            if ( (wlanPciInfo[i].usWirelessPciId == pciID) && (wlanPciInfo[i].usNeededSize <= sizeInDWords) ){
                /* valid , patch entry */
                while ( wlanPciInfo[i].entries[j].name[0] ) {
                    pBase[wlanPciInfo[i].entries[j].dwordOffset] = wlanPciInfo[i].entries[j].value;
                    j++;
                }
                nRet = BP_SUCCESS;
                goto pciconfig_update_done;
            }
        }
        i++;
    }

pciconfig_update_done:

    return( nRet );

}

/**************************************************************************
* Name       : BpGetWanDataLedGpio
*
* Description: This function returns the GPIO pin assignment for the WAN Data
*              LED.
*
* Parameters : [OUT] pusValue - Address of short word that the WAN Data LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetWanDataLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLedWanData, pusValue ) );
} /* BpGetWanDataLedGpio */


#if 1 /* chchien */
int BpGetWanEth3LedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLedWanEth3, pusValue ) );
} /* BpGetWanEth3LedGpio */
#endif

#if 1 // __MSTC__, Richard Huang

/**************************************************************************
* Name       : BpGetWanDataLedGpio
*
* Description: This function returns the GPIO pin assignment for the Internet Data
*              LED.
*
* Parameters : [OUT] pusValue - Address of short word that the Internet Data LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetInternetDataLedGpio( unsigned short *pusValue )
{
    return( BpGetUs(bp_usGpioLedInternetData, pusValue ) );
} /* BpGetInternetDataLedGpio */
#endif

/**************************************************************************
* Name       : BpGetWanErrorLedGpio
*
* Description: This function returns the GPIO pin assignment for the WAN
*              LED that is used when there is a WAN connection failure.
*
* Parameters : [OUT] pusValue - Address of short word that the WAN LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetWanErrorLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLedWanError, pusValue ) );
} /* BpGetWanErrorLedGpio */

/**************************************************************************
* Name       : BpGetBootloaderPowerOnLedGpio
*
* Description: This function returns the GPIO pin assignment for the power
*              on LED that is set by the bootloader.
*
* Parameters : [OUT] pusValue - Address of short word that the alarm LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetBootloaderPowerOnLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLedBlPowerOn, pusValue ) );
} /* BpGetBootloaderPowerOn */

/**************************************************************************
* Name       : BpGetBootloaderStopLedGpio
*
* Description: This function returns the GPIO pin assignment for the break
*              into bootloader LED that is set by the bootloader.
*
* Parameters : [OUT] pusValue - Address of short word that the break into
*                  bootloader LED GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetBootloaderStopLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLedBlStop, pusValue ) );
} /* BpGetBootloaderStopLedGpio */

/**************************************************************************
* Name       : BpGetVoipLedGpio
*
* Description: This function returns the GPIO pin assignment for the VOIP
*              LED.
*
* Parameters : [OUT] pusValue - Address of short word that the VOIP LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
*
* Note       : The VoIP structure would allow for having one LED per DSP
*              however, the board initialization function assumes only one
*              LED per functionality (ie one LED for VoIP).  Therefore in
*              order to keep this tidy and simple we do not make usage of the
*              one-LED-per-DSP function.  Instead, we assume that the LED for
*              VoIP is unique and associated with DSP 0 (always present on
*              any VoIP platform).  If changing this to a LED-per-DSP function
*              then one need to update the board initialization driver in
*              bcmdrivers\opensource\char\board\bcm963xx\impl1
***************************************************************************/
int BpGetVoipLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLedVoip, pusValue ) );
} /* BpGetVoipLedGpio */

/**************************************************************************
* Name       : BpGetVoip1LedGpio
*
* Description: This function returns the GPIO pin assignment for the VoIP1.
*              LED which is used when FXS0 is active
* Parameters : [OUT] pusValue - Address of short word that the VoIP1
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetVoip1LedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioVoip1Led, pusValue ) );
} /* BpGetVoip1LedGpio */

/**************************************************************************
* Name       : BpGetVoip1FailLedGpio
*
* Description: This function returns the GPIO pin assignment for the VoIP1
*              Fail LED which is used when there's an error with FXS0
* Parameters : [OUT] pusValue - Address of short word that the VoIP1
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetVoip1FailLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioVoip1LedFail, pusValue ) );
} /* BpGetVoip1FailLedGpio */

/**************************************************************************
* Name       : BpGetVoip2LedGpio
*
* Description: This function returns the GPIO pin assignment for the VoIP2.
*              LED which is used when FXS1 is active
* Parameters : [OUT] pusValue - Address of short word that the VoIP2
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetVoip2LedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioVoip2Led, pusValue ) );
} /* BpGetVoip2LedGpio */

/**************************************************************************
* Name       : BpGetVoip2FailLedGpio
*
* Description: This function returns the GPIO pin assignment for the VoIP2
*              Fail LED which is used when there's an error with FXS1
* Parameters : [OUT] pusValue - Address of short word that the VoIP2
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetVoip2FailLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioVoip2LedFail, pusValue ) );
} /* BpGetVoip2FailLedGpio */

/**************************************************************************
* Name       : BpGetPotsLedGpio
*
* Description: This function returns the GPIO pin assignment for the POTS1.
*              LED which is used when DAA is active
* Parameters : [OUT] pusValue - Address of short word that the POTS11
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetPotsLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioPotsLed, pusValue ) );
} /* BpGetPotsLedGpio */

int BpGetPhone1MWILedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioPhone1MWILed, pusValue ) );
} /* BpGetPotsLedGpio */
int BpGetPhone2MWILedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioPhone2MWILed, pusValue ) );
} /* BpGetPotsLedGpio */


/**************************************************************************
* Name       : BpGetDectLedGpio
*
* Description: This function returns the GPIO pin assignment for the DECT.
*              LED which is used when DECT is active
* Parameters : [OUT] pusValue - Address of short word that the DECT
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetDectLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioDectLed, pusValue ) );
} /* BpGetDectLedGpio */


/**************************************************************************
* Name       : BpGetPassDyingGaspGpio
*
* Description: This function returns the GPIO pin assignment used to pass
*                  a dying gasp interrupt to an external processor.
* Parameters : [OUT] pusValue - Address of short word that the DECT
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetPassDyingGaspGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioPassDyingGasp, pusValue ) );
} /* BpGetPassDyingGaspGpio */


/**************************************************************************
* Name       : BpGetFpgaResetGpio
*
* Description: This function returns the GPIO pin assignment for the FPGA
*              Reset signal.
*
* Parameters : [OUT] pusValue - Address of short word that the FPGA Reset
*                  signal GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetFpgaResetGpio( unsigned short *pusValue ) {
    return( BpGetGpio(bp_usGpioFpgaReset, pusValue ) );
} /*BpGetFpgaResetGpio*/

/**************************************************************************
* Name       : BpGetGponLedGpio
*
* Description: This function returns the GPIO pin assignment for the GPON
*              LED.
*
* Parameters : [OUT] pusValue - Address of short word that the GPON LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetGponLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLedGpon, pusValue ) );
} /* BpGetGponLedGpio */

/**************************************************************************
* Name       : BpGetGponFailLedGpio
*
* Description: This function returns the GPIO pin assignment for the GPON
*              LED that is used when there is a GPON connection failure.
*
* Parameters : [OUT] pusValue - Address of short word that the GPON LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetGponFailLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLedGponFail, pusValue ) );
} /* BpGetGponFailLedGpio */

/**************************************************************************
* Name       : BpGetMoCALedGpio
*
* Description: This function returns the GPIO pin assignment for the MoCA
*              LED.
*
* Parameters : [OUT] pusValue - Address of short word that the MoCA LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetMoCALedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLedMoCA, pusValue ) );
} /* BpGetMoCALedGpio */

/**************************************************************************
* Name       : BpGetMoCAFailLedGpio
*
* Description: This function returns the GPIO pin assignment for the MoCA
*              LED that is used when there is a MoCA connection failure.
*
* Parameters : [OUT] pusValue - Address of short word that the MoCA LED
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetMoCAFailLedGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLedMoCAFail, pusValue ) );
} /* BpGetMoCAFailLedGpio */

/**************************************************************************
* Name       : BpGetResetToDefaultExtIntr
*
* Description: This function returns the external interrupt number for the
*              reset to default button.
*
* Parameters : [OUT] pusValue - Address of short word that reset to default
*                  external interrupt is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetResetToDefaultExtIntr( unsigned short *pusValue )
{
    return( BpGetUs(bp_usExtIntrResetToDefault, pusValue ) );
} /* BpGetResetToDefaultExtIntr */

/**************************************************************************
* Name       : BpGetWirelessPowerDownGpio
*
* Description: This function returns the GPIO pin assignment for WLAN_PD
*
*
* Parameters : [OUT] pusValue - Address of short word that the WLAN_PD
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetWirelessPowerDownGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioWirelessPowerDown, pusValue ) );
} /* usGpioWirelessPowerDown */

/**************************************************************************
* Name       : BpGetDslPhyAfeIds
*
* Description: This function returns the DSL PHY AFE ids for primary and
*              secondary PHYs.
*
* Parameters : [OUT] pulValues-Address of an array of two long words where
*              AFE Id for the primary and secondary PHYs are returned.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET-Error, BpSetBoardId hasn't been called.
*              BP_VALUE_NOT_DEFINED - No defined AFE Ids.
**************************************************************************/
int BpGetDslPhyAfeIds( unsigned long *pulValues )
{
    int nRet;

    if( g_pCurrentBp )
    {
        if (BpGetUl(bp_ulAfeId0, &pulValues[0]) != BP_SUCCESS) {
          pulValues[0] = BP_AFE_DEFAULT;
        }
        if (BpGetUl(bp_ulAfeId1, &pulValues[1]) != BP_SUCCESS) {
          pulValues[1] = BP_AFE_DEFAULT;
        }
        nRet = BP_SUCCESS;
    }
    else
    {
        pulValues[0] = BP_AFE_DEFAULT;
        pulValues[1] = BP_AFE_DEFAULT;
        nRet = BP_BOARD_ID_NOT_SET;
    }

    return( nRet );
} /* BpGetDslPhyAfeIds */

/**************************************************************************
* Name       : BpGetUart2SdoutGpio
*
* Description: This function returns the GPIO pin assignment for UART2 SDOUT
*
*
* Parameters : [OUT] pusValue - Address of short word that the bp_usGpioUart2Sdout
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetUart2SdoutGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioUart2Sdout, pusValue ) );
} /* BpGetUart2SdoutGpio */

/**************************************************************************
* Name       : BpGetUart2SdinGpio
*
* Description: This function returns the GPIO pin assignment for UART2 SDIN
*
*
* Parameters : [OUT] pusValue - Address of short word that the bp_usGpioUart2Sdin
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetUart2SdinGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioUart2Sdin, pusValue ) );
} /* BpGetUart2SdinGpio */

/**************************************************************************
* Name       : BpGetExtAFEResetGpio
*
* Description: This function returns the GPIO pin assignment for resetting the external AFE chip
*
*
* Parameters : [OUT] pusValue - Address of short word that the ExtAFEReset
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetExtAFEResetGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioExtAFEReset, pusValue ) );
} /* BpGetExtAFEResetGpio */

/**************************************************************************
* Name       : BpGetAFELDRelayGpio
*
* Description: This function returns the GPIO pin assignment for switching LD relay
*
*
* Parameters : [OUT] pusValue - Address of short word that the bp_usGpioAFELDRelay
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetAFELDRelayGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioAFELDRelay, pusValue ) );
} /* BpGetAFELDRelayGpio */

/**************************************************************************
* Name       : BpGetIntAFELDModeGpio
*
* Description: This function returns the GPIO pin assignment for setting LD Mode to ADSL/VDSL
*                  for the internal path.
*
* Parameters : [OUT] pusValue - Address of short word that the bp_usGpioIntAFELDMode
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetIntAFELDModeGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioIntAFELDMode, pusValue ) );
} /* BpGetIntAFELDModeGpio */

/**************************************************************************
* Name       : BpGetIntAFELDPwrGpio
*
* Description: This function returns the GPIO pin assignment for turning on/off the internal AFE LD
*
*
* Parameters : [OUT] pusValue - Address of short word that the usGpioExtAFELDPwr
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetIntAFELDPwrGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioIntAFELDPwr, pusValue ) );
} /* BpGetIntAFELDPwrGpio */

/**************************************************************************
* Name       : BpGetExtAFELDModeGpio
*
* Description: This function returns the GPIO pin assignment for setting LD Mode to ADSL/VDSL
*                  for the external path.
*
* Parameters : [OUT] pusValue - Address of short word that the usGpioExtAFELDMode
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetExtAFELDModeGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioExtAFELDMode, pusValue ) );
} /* BpGetExtAFELDModeGpio */

/**************************************************************************
* Name       : BpGetExtAFELDPwrGpio
*
* Description: This function returns the GPIO pin assignment for turning on/off the external AFE LD
*
*
* Parameters : [OUT] pusValue - Address of short word that the usGpioExtAFELDPwr
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetExtAFELDPwrGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioExtAFELDPwr, pusValue ) );
} /* BpGetExtAFELDPwrGpio */

/**************************************************************************
* Name       : BpGetExtAFELDDataGpio
*
* Description: This function returns the GPIO pin assignment for sending config data to the external AFE LD
*
*
* Parameters : [OUT] pusValue - Address of short word that the bp_usGpioExtAFELDData
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetExtAFELDDataGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioExtAFELDData, pusValue ) );
} /* BpGetExtAFELDDataGpio */

/**************************************************************************
* Name       : BpGetExtAFELDClkGpio
*
* Description: This function returns the GPIO pin assignment for sending the clk to the external AFE LD
*
*
* Parameters : [OUT] pusValue - Address of short word that the bp_usGpioExtAFELDClk
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/

int BpGetExtAFELDClkGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioExtAFELDClk, pusValue ) );
} /* BpGetExtAFELDClkGpio */

/**************************************************************************
* Name       : BpGet6829PortInfo
*
* Description: This function checks the ENET MAC info to see if a 6829
*              is connected
*
* Parameters : [OUT] portInfo6829 - 0 if 6829 is not present
*                                 - 6829 port information otherwise
*
* Returns    : BP_SUCCESS           - Success, value is returned.
*              BP_BOARD_ID_NOT_SET  - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGet6829PortInfo( unsigned char *portInfo6829 )
{
   ETHERNET_MAC_INFO enetMacInfo;
   ETHERNET_SW_INFO *pSwInfo;
   int               retVal;
   int               i;

   *portInfo6829 = 0;
   retVal = BpGetEthernetMacInfo( &enetMacInfo, 1 );
   if ( BP_SUCCESS == retVal ) {
      pSwInfo = &enetMacInfo.sw;
      for (i = 0; i < BP_MAX_SWITCH_PORTS; i++) {
         if ( ((pSwInfo->phy_id[i] & PHYID_LSBYTE_M) != 0xFF) &&
              ((pSwInfo->phy_id[i] & PHYID_LSBYTE_M) &  0x80) ) {
            *portInfo6829 = pSwInfo->phy_id[i] & PHYID_LSBYTE_M;
            retVal        = BP_SUCCESS;
            break;
         }
      }
   }

   return retVal;

}

/**************************************************************************
* Name       : BpGetEthSpdLedGpio
*
* Description: This function returns the GPIO pin assignment for the
*              specified port and link speed 
*
* Parameters : [IN] port - Internal phy number
*              [IN] enetIdx - index for Ethernet MAC info
*              [IN] ledIdx - 0 -> duplex GPIO
*                          - 1 -> spd 100 GPIO
*                          - 2 -> spd 1000 GPIO
*              [OUT] pusValue - Address of a short word to store the GPIO
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetEthSpdLedGpio( unsigned short port, unsigned short enetIdx,
                        unsigned short ledIdx, unsigned short *pusValue )
{
    ETHERNET_MAC_INFO enetMacInfos[BP_MAX_ENET_MACS];
    unsigned short *pShort;
    int nRet;

    if( g_pCurrentBp ) {
        nRet = BpGetEthernetMacInfo( enetMacInfos, BP_MAX_ENET_MACS );

        if ((enetIdx >= BP_MAX_ENET_MACS) ||
            (port >= BP_MAX_ENET_INTERNAL) ||
            (enetMacInfos[enetIdx].ucPhyType == BP_ENET_NO_PHY)) {
           *pusValue = BP_NOT_DEFINED;
           nRet = BP_VALUE_NOT_DEFINED;
        } else {
           pShort   = &enetMacInfos[enetIdx].sw.ledInfo[port].duplexLed;
           pShort   += ledIdx;
           *pusValue = *pShort;
           if( *pShort == BP_NOT_DEFINED ) {
               nRet = BP_VALUE_NOT_DEFINED;
           } else {
               nRet = BP_SUCCESS;
           }
        }
    } else {
        *pusValue = BP_NOT_DEFINED;
        nRet = BP_BOARD_ID_NOT_SET;
    }

    return( nRet );
} /* BpGetEthSpdLedGpio */


/**************************************************************************
* Name       : BpGetLaserDisGpio
*
* Description: This function returns the GPIO pin assignment for disabling
*              the laser
*
* Parameters : [OUT] pusValue - Address of short word that the usGpioLaserDis
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetLaserDisGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLaserDis, pusValue ) );
} /* BpGetLaserDisGpio */


/**************************************************************************
* Name       : BpGetLaserTxPwrEnGpio
*
* Description: This function returns the GPIO pin assignment for enabling
*              the transmit power of the laser
*
* Parameters : [OUT] pusValue - Address of short word that the usGpioLaserTxPwrEn
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetLaserTxPwrEnGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioLaserTxPwrEn, pusValue ) );
} /* BpGetLaserTxPwrEnGpio */


/**************************************************************************
* Name       : BpGetVregSel1P2
*
* Description: This function returns the desired voltage level for 1V2
*
* Parameters : [OUT] pusValue - Address of short word that the 1V2 level
*                  is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetVregSel1P2( unsigned short *pusValue )
{
    return( BpGetUs(bp_usVregSel1P2, pusValue ) );
} /* BpGetVregSel1P2 */


/**************************************************************************
* Name       : BpGetGponOpticsType
*
* Description: This function returns an indication of whether the current 
*              board type supports GPON legacy or BOSA optics.
*
* Parameters : [Out] pusValue
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetGponOpticsType( unsigned short *pusValue )
{
    return ( BpGetUs (bp_usGponOpticsType, pusValue));
} /* BpGetGponOpticsType */


/**************************************************************************
* Name       : BpGetDefaultOpticalParams
*
* Description: This function returns the optical params for BOSA optics if
*              they exist.  These are only used if they do not exist in NVRAM.
*
* Parameters : [OUT] pOpticalParams - Address of a buffer that the optical
*              params are returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
***************************************************************************/
#if !defined(_CFE_)
int BpGetDefaultOpticalParams( unsigned char *pOpticalParams )
{
    int Index, Ret;    
    unsigned char * pBpOpticalParams = (unsigned char *)0;


    if (BP_SUCCESS == (Ret = BpGetCp (bp_cpDefaultOpticalParams, (char**)&pBpOpticalParams)))
    {
        for (Index = 0; Index < BP_OPTICAL_PARAMS_LEN; Index++)
        {
            pOpticalParams[Index] = pBpOpticalParams[Index];
        }
    }

    return (Ret);

} /* BpGetDefaultOpticalParams */
#endif


/**************************************************************************
* Name       : BpGetFemtoResetGpio
*
* Description: This function returns the GPIO that needs to be toggled high
*              for 2 msec at least to reset the FEMTO chip
*
* Parameters : [OUT] pusValue - Address of short word that the GPIO for
*                  resetting FEMTO chip is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetFemtoResetGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioFemtoReset, pusValue ) );
} /*  BpGetFemtoResetGpio */



/**************************************************************************
* Name       : BpGetEphyBaseAddress
*
* Description: This function returns the base address requested for
*              the internal EPHYs
*
* Parameters : [OUT] pusValue - Address of short word for returned value.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetEphyBaseAddress( unsigned short *pusValue )
{
    return( BpGetUs(bp_usEphyBaseAddress, pusValue ) );
} /*  BpGetEphyBaseAddress */


/**************************************************************************
* Name       : BpGetGphyBaseAddress
*
* Description: This function returns the base address requested for
*              the internal GPHYs
*
* Parameters : [OUT] pusValue - Address of short word for returned value.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - Not defined
***************************************************************************/
int BpGetGphyBaseAddress( unsigned short *pusValue )
{
    return( BpGetUs(bp_usGphyBaseAddress, pusValue ) );
} /*  BpGetGphyBaseAddress */

/**************************************************************************
* Name       : BpGetSpiSlaveResetGpio
*
* Description: This function returns the GPIO pin assignment for the resetting the 
*              the SPI slave.
*
* Parameters : [OUT] pusValue - Address of short word that the spi slave reset
*                  GPIO pin is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetSpiSlaveResetGpio( unsigned short *pusValue )
{
    return( BpGetGpio(bp_usGpioSpiSlaveReset, pusValue ) );
} /* BpGetSpiSlaveResetGpio */

/**************************************************************************
* Name       : BpGetSpiSlaveBusNum
*
* Description: This function returns the bus number of the SPI slave device.
*
* Parameters : [OUT] pusValue - Address of short word that the spi slave select number
*                    is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetSpiSlaveBusNum( unsigned short *pusValue )
{
    return( BpGetUs(bp_usSpiSlaveBusNum, pusValue ) );
} /* BpGetSpiSlaveBusNum */

/**************************************************************************
* Name       : BpGetSpiSlaveSelectNum
*
* Description: This function returns the SPI slave select number connected  
*              to the slave device.
*
* Parameters : [OUT] pusValue - Address of short word that the spi slave select number
*                    is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetSpiSlaveSelectNum( unsigned short *pusValue )
{
    return( BpGetUs(bp_usSpiSlaveSelectNum, pusValue ) );
} /* BpGetSpiSlaveSelectNum */

/**************************************************************************
* Name       : BpGetSpiSlaveMode
*
* Description: This function returns the SPI slave select number connected  
*              to the slave device.
*
* Parameters : [OUT] pusValue - Address of short word that the spi slave mode is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetSpiSlaveMode( unsigned short *pusValue )
{
    return( BpGetUs(bp_usSpiSlaveMode, pusValue ) );
} /* BpGetSpiSlaveMode */

/**************************************************************************
* Name       : BpGetSpiSlaveCtrlState
*
* Description: This function returns the spi controller state that is needed to talk
*              to the spi slave device.
*
* Parameters : [OUT] pusValue - Address of long word that the spi controller state is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetSpiSlaveCtrlState( unsigned long *pulValue )
{
    return( BpGetUl(bp_ulSpiSlaveCtrlState, pulValue ) );
} /* BpGetSpiSlaveCtrlState */

/**************************************************************************
* Name       : BpGetSpiSlaveMaxFreq
*
* Description: This function returns the SPI slaves max frequency for communication.
*
* Parameters : [OUT] pusValue - Address of long word that the max freq is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetSpiSlaveMaxFreq( unsigned long *pulValue )
{
    return( BpGetUl(bp_ulSpiSlaveMaxFreq, pulValue ) );
} /* BpGetSpiSlaveMaxFreq */

/**************************************************************************
* Name       : BpGetSpiSlaveProtoRev
*
* Description: This function returns the protocol revision that the slave device uses.
*
* Parameters : [OUT] pusValue - Address of short word that the spi slave protocol revision
                                is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetSpiSlaveProtoRev( unsigned short *pusValue )
{
    return( BpGetUs(bp_usSpiSlaveProtoRev, pusValue ) );
} /* BpGetSpiSlaveProtoRev */

/**************************************************************************
* Name       : BpGetSerialLEDMuxSel
*
* Description: This function returns the serial LED Mux selection.
*
* Parameters : [OUT] pusValue - Address of short word that the serial LED MUX Selection
                                is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetSerialLEDMuxSel( unsigned short *pusValue )
{
    return( BpGetUs(bp_usSerialLEDMuxSel, pusValue ) );
} /* BpGetSerialLEDMuxSel */

/**************************************************************************
* Name       : BpGetSwitchPortMap
*
* Description: This function returns the switch port map.
*
* Parameters : [OUT] pulValue - Bitmap of switch ports enabled.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_VALUE_NOT_DEFINED - bp_ulPortMap is not defined
*                  for the board.
***************************************************************************/
int BpGetSwitchPortMap (unsigned long *pulValue)
{
    *pulValue = BpGetSubUl(bp_ulPortMap, 0, bp_last);
    if (*pulValue == BP_NOT_DEFINED ){
        *pulValue = 0;
        return BP_VALUE_NOT_DEFINED;
    } else {
        return BP_SUCCESS;
    }
}

/**************************************************************************
* Name       : BpGetDeviceOptions
*
* Description: This function returns the serial LED Mux selection.
*
* Parameters : [OUT] pulValue - Address of word that device options bitmap
                                is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_SET - Error, BpSetBoardId has not been called.
*              BP_VALUE_NOT_DEFINED - At least one return value is not defined
*                  for the board.
***************************************************************************/
int BpGetDeviceOptions( unsigned long *pulValue )
{
    return( BpGetUl(bp_ulDeviceOptions, pulValue ) );
} /* BpGetDeviceOptions */

#if 1 // __MSTC__, Paul Ho, Wireless Button
int BpGetWirelessBtnGpio( unsigned short *pusValue )
{
    return( BpGetUs(bp_usGpioWirelessBtn, pusValue ) );
} /* BpGetVoip1FailLedGpio */
#endif
