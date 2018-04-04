/*
<:label-BRCM:2012:DUAL/GPL:standard

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

#ifndef __BCM6318_MAP_PART_H
#define __BCM6318_MAP_PART_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __BCM6318_MAP_H
#define __BCM6318_MAP_H

#include "bcmtypes.h"

#define PERF_BASE                   0xb0000000  /* chip control */
#define TIMR_BASE                   0xb0000040  /* timer registers */
#define GPIO_BASE                   0xb0000080  /* gpio registers */
#define UART_BASE                   0xb0000100  /* uart registers */
#define LED_BASE                    0xb0000200  /* LED control registers */
#define MISC_BASE                   0xb0000280  /* Miscellaneous Registers */
#define PLL_PWR_CTRL_BASE           0xb0000880  /* PLL Power Control Registers */
#define STRAP_BASE                  0xb0000900  /* Strap Override Control Registers */
#define HSSPIM_BASE                 0xb0003000  /* High-Speed SPI registers */
#define SDRAM_CTRL_BASE             0xb0004000  /* SDRAM Control */

#define USB_EHCI_BASE               0x10005000  /* USB host registers */
#define USB_OHCI_BASE               0x10005100  /* USB host registers */
#define USBH_CFG_BASE               0xb0005200
#define USB_CTL_BASE                0xb0006000  /* USB 2.0 device control */
#define SAR_BASE                    0xb0008000
#define SAR_DMA_BASE                0xb000c000  /* XTM SAR DMA control */
#define PCIE_BASE                   0xb0010000
#define SWITCH_BASE                 0xb0080000
#define SWITCH_DMA_BASE             0xb0088000


#endif



/*
** Peripheral Controller
*/

#define IRQ_BITS 64
typedef struct  {
    uint64         ExtIrqMask;
    uint64         IrqMask;
    uint64         ExtIrqStatus;
    uint64         IrqStatus;
} IrqControl_t;

typedef struct  {
    uint32         ExtIrqMaskHi;
    uint32         ExtIrqMaskLo;
    uint32         IrqMaskHi;
    uint32         IrqMaskLo;
    uint32         ExtIrqStatusHi;
    uint32         ExtIrqStatusLo;
    uint32         IrqStatusHi;
    uint32         IrqStatusLo;
} IrqControl_32_t;

typedef struct PerfControl {
     uint32        RevID;             /* (00) word 0 */
#define CHIP_ID_SHIFT   16
#define CHIP_ID_MASK    (0xffff << CHIP_ID_SHIFT)
#define CHIP_VAR_SHIFT   12
#define CHIP_VAR_MASK    (0xf << CHIP_VAR_SHIFT)
#define REV_ID_MASK     0xff

     uint32        blkEnables;        /* (04) word 1 */
#define QPROC_CLK_EN        (1 << 30)
#define AFE_CLK_EN          (1 << 29)
#define PHYMIPS_CLK_EN      (1 << 28)
#define PCIE25_CLK_EN       (1 << 27)
#define SPIM_CLK_EN         (1 << 25)
#define USBD_CLK_EN         (1 << 20)
#define USBH_CLK_EN         (1 << 20) /* There is no USBH clk really. */
#define SDR_CLK_EN          (1 << 19)
#define ROBOSW025_CLK_EN    (1 << 17)
#define ROBOSW250_CLK_EN    (1 << 16)
#define PCIE_CLK_EN         (1 << 14)
#define MIPS_CLK_EN         (1 << 13)
#define SAR125_CLK_EN       (1 << 12)
#define ADSL_CLK_EN         (1 << 11)
#define CPUBUS160_CLK_EN    (1 << 10)
#define PERIPH_ASB_CLK_EN   (1 << 9)
#define SWREG_ASB_CLK_EN    (1 << 8)
#define SDR_ASB_CLK_EN      (1 << 7)
#define SAR_ASB_CLK_EN      (1 << 6)
#define ROBOSW_ASB_CLK_EN   (1 << 5)
#define PHYMIPS_ASB_CLK_EN  (1 << 4)
#define PCIE_ASB_CLK_EN     (1 << 3)
#define MIPS_ASB_CLK_EN     (1 << 2)
#define USB_ASB_CLK_EN      (1 << 1)
#define ADSL_ASB_CLK_EN     (1 << 0)


     uint32        blkEnablesUbus;    /* (08) word 2 */
#define USB_UBUS_CLK_EN     (1 << 9)
#define SDR_UBUS_CLK_EN     (1 << 8)
#define SAR_UBUS_CLK_EN     (1 << 7)
#define ROBOSW_UBUS_CLK_EN  (1 << 6)
#define PHYMIPS_UBUS_CLK_EN (1 << 5)
#define PERIPH_UBUS_CLK_EN  (1 << 4)
#define PCIE_UBUS_CLK_EN    (1 << 3)
#define MIPS_UBUS_CLK_EN    (1 << 2)
#define ARB_UBUS_CLK_EN     (1 << 1)
#define ADSL_UBUS_CLK_EN    (1 << 0)


     uint32        deviceTimeoutEn;   /* (0c) word 3 */
     uint32        softResetB;        /* (10) word 4 */
#define SOFT_RST_HOSTMIPS       (1 << 12)
#define SOFT_RST_PHYMIPS        (1 << 11)
#define SOFT_RST_ADSL           (1 << 10)
#define SOFT_RST_PCIE_HARD      (1 << 9)
#define SOFT_RST_PCIE_EXT       (1 << 8)
#define SOFT_RST_PCIE           (1 << 7)
#define SOFT_RST_PCIE_CORE      (1 << 6)
#define SOFT_RST_USBH           (1 << 5)
#define SOFT_RST_USBD           (1 << 4)
#define SOFT_RST_SWITCH         (1 << 3)
#define SOFT_RST_SAR            (1 << 2)
#define SOFT_RST_EPHY           (1 << 1)
#define SOFT_RST_HS_SPIM        (1 << 0)

    uint32        diagControl;        /* (14) word 5 */
    uint32        ExtIrqCfg;          /* (18) word 6*/
#define EI_SENSE_SHFT   0
#define EI_STATUS_SHFT  4
#define EI_CLEAR_SHFT   8
#define EI_MASK_SHFT    12
#define EI_INSENS_SHFT  16
#define EI_LEVEL_SHFT   20

    uint32        unused1;            /* (1c) word 7 */

    union {
         IrqControl_t     IrqControl[1];    /* (20) */
         IrqControl_32_t  IrqControl32[1];  /* (20) */
    };
} PerfControl;

#define PERF ((volatile PerfControl * const) PERF_BASE)

/*
** Timer
*/
typedef struct Timer {
    uint32        TimerMask;
#define TIMER0EN        0x01
#define TIMER1EN        0x02
#define TIMER2EN        0x04
#define TIMER3EN        0x08
    uint32        TimerInts;
#define TIMER0          0x01
#define TIMER1          0x02
#define TIMER2          0x04
#define TIMER3          0x08
#define WDINT           0x10

    uint32        TimerCtl0;
    uint32        TimerCtl1;
    uint32        TimerCtl2;
    uint32        TimerCtl3;
#define TIMERENABLE     0x80000000
#define RSTCNTCLR       0x40000000
    uint32        TimerCnt0;
    uint32        TimerCnt1;
    uint32        TimerCnt2;
    uint32        TimerCnt3;
    uint32        WatchDogDefCount;

    /* Write 0xff00 0x00ff to Start timer
     * Write 0xee00 0x00ee to Stop and re-load default count
     * Read from this register returns current watch dog count
     */
    uint32        WatchDogCtl;

    /* Number of 50-MHz ticks for WD Reset pulse to last */
    uint32        WDResetCount;

    uint32        SoftRst;
#define SOFT_RESET              0x00000001      // 0    
} Timer;

#define TIMER ((volatile Timer * const) TIMR_BASE)

/*
** UART
*/
typedef struct UartChannel {
    byte          unused0;
    byte          control;
#define BRGEN           0x80    /* Control register bit defs */
#define TXEN            0x40
#define RXEN            0x20
#define LOOPBK          0x10
#define TXPARITYEN      0x08
#define TXPARITYEVEN    0x04
#define RXPARITYEN      0x02
#define RXPARITYEVEN    0x01

    byte          config;
#define XMITBREAK       0x40
#define BITS5SYM        0x00
#define BITS6SYM        0x10
#define BITS7SYM        0x20
#define BITS8SYM        0x30
#define ONESTOP         0x07
#define TWOSTOP         0x0f
    /* 4-LSBS represent STOP bits/char
     * in 1/8 bit-time intervals.  Zero
     * represents 1/8 stop bit interval.
     * Fifteen represents 2 stop bits.
     */
    byte          fifoctl;
#define RSTTXFIFOS      0x80
#define RSTRXFIFOS      0x40
    /* 5-bit TimeoutCnt is in low bits of this register.
     *  This count represents the number of characters
     *  idle times before setting receive Irq when below threshold
     */
    uint32        baudword;
    /* When divide SysClk/2/(1+baudword) we should get 32*bit-rate
     */

    byte          txf_levl;       /* Read-only fifo depth */
    byte          rxf_levl;       /* Read-only fifo depth */
    byte          fifocfg;        /* Upper 4-bits are TxThresh, Lower are
                                   *      RxThreshold.  Irq can be asserted
                                   *      when rx fifo> thresh, txfifo<thresh
                                   */
    byte          prog_out;       /* Set value of DTR (Bit0), RTS (Bit1)
                                   *  if these bits are also enabled to GPIO_o
                                   */
#define DTREN   0x01
#define RTSEN   0x02

    byte          unused1;
    byte          DeltaIPEdgeNoSense;     /* Low 4-bits, set corr bit to 1 to
                                           * detect irq on rising AND falling
                                           * edges for corresponding GPIO_i
                                           * if enabled (edge insensitive)
                                           */
    byte          DeltaIPConfig_Mask;     /* Upper 4 bits: 1 for posedge sense
                                           *      0 for negedge sense if
                                           *      not configured for edge
                                           *      insensitive (see above)
                                           * Lower 4 bits: Mask to enable change
                                           *  detection IRQ for corresponding
                                           *  GPIO_i
                                           */
    byte          DeltaIP_SyncIP;         /* Upper 4 bits show which bits
                                           *  have changed (may set IRQ).
                                           *  read automatically clears bit
                                           * Lower 4 bits are actual status
                                           */

    uint16        intMask;                /* Same Bit defs for Mask and status */
    uint16        intStatus;
#define DELTAIP         0x0001
#define TXUNDERR        0x0002
#define TXOVFERR        0x0004
#define TXFIFOTHOLD     0x0008
#define TXREADLATCH     0x0010
#define TXFIFOEMT       0x0020
#define RXUNDERR        0x0040
#define RXOVFERR        0x0080
#define RXTIMEOUT       0x0100
#define RXFIFOFULL      0x0200
#define RXFIFOTHOLD     0x0400
#define RXFIFONE        0x0800
#define RXFRAMERR       0x1000
#define RXPARERR        0x2000
#define RXBRK           0x4000

    uint16        unused2;
    uint16        Data;                   /* Write to TX, Read from RX */
                                          /* bits 11:8 are BRK,PAR,FRM errors */

    uint32        unused3;
    uint32        unused4;
} Uart;

#define UART ((volatile Uart * const) UART_BASE)

/*
** Gpio Controller
*/

typedef struct GpioControl {
    uint64      GPIODir;                    /* 0 */
    uint64      GPIOio;                     /* 8 */
    uint32      unused;
    uint32      SpiSlaveCfg;                /* 14 */
    uint32      GPIOMode;                   /* 18 */

    uint32      PinMuxSel0;                  /* 1C */
#define PINMUX_SEL_GPIO13_MASK    (0x3)
#define PINMUX_SEL_GPIO13_SHIFT   (26)
#define PINMUX_SEL_USB_PWRON      (1)
#define PINMUX_SEL_USB_DEVICE_LED (2)

#define EPHY0_SPD_LED       0
#define EPHY1_SPD_LED       1
#define EPHY2_SPD_LED       2
#define EPHY3_SPD_LED       3    

#define EPHY0_ACT_LED       4
#define EPHY1_ACT_LED       5
#define EPHY2_ACT_LED       6
#define EPHY3_ACT_LED       7
#define SERIAL_LED_DATA     6
#define SERIAL_LED_CLK      7

#define INET_ACT_LED        8
#define INET_FAIL_LED       9
#define DSL_LED             10
#define POST_FAIL_LED       11
#define WLAN_WPS_LED        12
#define USB_ACT_LED         13
    
#define PINMUX_SERIAL_LED_DATA     (3 << (SERIAL_LED_DATA << 1))
#define PINMUX_SERIAL_LED_CLK      (3 << (SERIAL_LED_CLK << 1))
#define PINMUX_INET_ACT_LED        (1 << (INET_ACT_LED << 1)) 
#define PINMUX_INET_FAIL_LED       (1 << (INET_FAIL_LED << 1)) 
#define PINMUX_EPHY0_SPD_LED       (1 << (EPHY0_SPD_LED << 1))
#define PINMUX_EPHY1_SPD_LED       (1 << (EPHY1_SPD_LED << 1))
#define PINMUX_EPHY2_SPD_LED       (1 << (EPHY2_SPD_LED << 1))
#define PINMUX_EPHY3_SPD_LED       (1 << (EPHY3_SPD_LED << 1))  
#define PINMUX_EPHY0_ACT_LED       (1 << (EPHY0_ACT_LED << 1))
#define PINMUX_EPHY1_ACT_LED       (1 << (EPHY1_ACT_LED << 1))
#define PINMUX_EPHY2_ACT_LED       (1 << (EPHY2_ACT_LED << 1))
#define PINMUX_EPHY3_ACT_LED       (1 << (EPHY3_ACT_LED << 1)) 
#define PINMUX_DSL_LED             (1 << (DSL_LED << 1))
#define PINMUX_POST_FAIL_LED       (1 << (POST_FAIL_LED << 1))
#define PINMUX_WLAN_WPS_LED        (1 << (WLAN_WPS_LED << 1)) 


    uint32      PinMuxSel1;                  /* 20 */   
    uint32      PinMuxSel2;                  /* 24 */          
#define PINMUX_SEL_GPIO40_MASK    (0x3)
#define PINMUX_SEL_GPIO40_SHIFT   (16)
#define PINMUX_SEL_USB_ACTIVE     (2)

    uint32      unused1[2];                 /* 28 */
    uint32      RoboSWLEDControl;           /* 30 */
    uint32      RoboSWLEDLSR;               /* 34 */
    uint32      unused2[1];                 /* 38 */
    uint32      RoboswEphyCtrl;             /* 3c */
#define EPHY_PWR_DOWN_DLL       (1 << 25)
#define EPHY_RST_4              (1<<3)
#define EPHY_RST_3              (1<<2)
#define EPHY_RST_2              (1<<1)
#define EPHY_RST_1              (1<<0)


    uint32      RoboswSwitchCtrl;           /* 40 */
#define RSW_HW_FWDG_EN          (1<<3)
#define RSW_QOS_EN              (1<<2)
#define RSW_WD_CLR_EN           (1<<1)
#define RSW_MII_DUMB_FWDG_EN    (1<<0)

    uint32      unused3[4];                 /* 44 */
    uint32      PadControl[6];              /* 54 */
} GpioControl;

#define GPIO ((volatile GpioControl * const) GPIO_BASE)

/* Number to mask conversion macro used for GPIODir and GPIOio */
#define GPIO_NUM_MAX                    64
#define GPIO_NUM_TO_MASK(X)             ( (((X) & BP_GPIO_NUM_MASK) < GPIO_NUM_MAX) ? ((uint64)1 << ((X) & BP_GPIO_NUM_MASK)) : (0) )


/*
** PLL Power Control Registers
*/

typedef struct PllPwrControl {
     uint32  tbd1[20];                          /* 0x80 - 0xcc */
     uint32  PllPwrControlActiveUbusPorts;      /* 0xd0 */
#define PORT_ID_PCIE                  (1<<6)

     uint32  tbd2[5];                           /* 0xd4 - 0xe4 */
     uint32  PllPwrControlIddqCtrl;             /* 0xe8 */
#define IDDQ_USB                      (1<<1)
#define IDDQ_PCIE                     (1<<0)

} PllPwrControl;

#define PLL_PWR ((volatile PllPwrControl * const) PLL_PWR_CTRL_BASE)


/*
** Strap Override Control Registers
*/

typedef struct StrapControl {
     uint32  strapOverrideBus;                       /* 0x00 */
#define STRAP_BUS_MIPS_FREQ_SHIFT      23
#define STRAP_BUS_MIPS_FREQ_MASK       (0x3<<STRAP_BUS_MIPS_FREQ_SHIFT)
#define STRAP_BUS_SDR_FREQ_SHIFT       26
#define STRAP_BUS_SDR_FREQ_MASK        (0x3<<STRAP_BUS_SDR_FREQ_SHIFT)
#define STRAP_BUS_SDR_SHIFT            17
#define STRAP_BUS_SDR_MASK             (0x1<<STRAP_BUS_SDR_SHIFT)
#define STRAP_BUS_UBUS_FREQ_SHIFT      13
#define STRAP_BUS_UBUS_FREQ_MASK       (0x1<<STRAP_BUS_UBUS_FREQ_SHIFT)

     uint32  strapOverrideControl;                   /* 0x04 */
} StrapControl;

#define STRAP ((volatile StrapControl * const) STRAP_BASE)


/*
** High-Speed SPI Controller
*/

#define __mask(end, start)      (((1 << ((end - start) + 1)) - 1) << start)
typedef struct HsSpiControl {

  uint32    hs_spiGlobalCtrl;   // 0x0000
#define HS_SPI_MOSI_IDLE        (1 << 18)
#define HS_SPI_CLK_POLARITY      (1 << 17)
#define HS_SPI_CLK_GATE_SSOFF       (1 << 16)
#define HS_SPI_PLL_CLK_CTRL     (8)
#define HS_SPI_PLL_CLK_CTRL_MASK    __mask(15, HS_SPI_PLL_CLK_CTRL)
#define HS_SPI_SS_POLARITY      (0)
#define HS_SPI_SS_POLARITY_MASK     __mask(7, HS_SPI_SS_POLARITY)

  uint32    hs_spiExtTrigCtrl;  // 0x0004
#define HS_SPI_TRIG_RAW_STATE   (24)
#define HS_SPI_TRIG_RAW_STATE_MASK  __mask(31, HS_SPI_TRIG_RAW_STATE)
#define HS_SPI_TRIG_LATCHED     (16)
#define HS_SPI_TRIG_LATCHED_MASK    __mask(23, HS_SPI_TRIG_LATCHED)
#define HS_SPI_TRIG_SENSE       (8)
#define HS_SPI_TRIG_SENSE_MASK      __mask(15, HS_SPI_TRIG_SENSE)
#define HS_SPI_TRIG_TYPE        (0)
#define HS_SPI_TRIG_TYPE_MASK       __mask(7, HS_SPI_TRIG_TYPE)
#define HS_SPI_TRIG_TYPE_EDGE       (0)
#define HS_SPI_TRIG_TYPE_LEVEL      (1)

  uint32    hs_spiIntStatus;    // 0x0008
#define HS_SPI_IRQ_PING1_USER       (28)
#define HS_SPI_IRQ_PING1_USER_MASK  __mask(31, HS_SPI_IRQ_PING1_USER)
#define HS_SPI_IRQ_PING0_USER       (24)
#define HS_SPI_IRQ_PING0_USER_MASK  __mask(27, HS_SPI_IRQ_PING0_USER)

#define HS_SPI_IRQ_PING1_CTRL_INV   (1 << 12)
#define HS_SPI_IRQ_PING1_POLL_TOUT  (1 << 11)
#define HS_SPI_IRQ_PING1_TX_UNDER   (1 << 10)
#define HS_SPI_IRQ_PING1_RX_OVER    (1 << 9)
#define HS_SPI_IRQ_PING1_CMD_DONE   (1 << 8)

#define HS_SPI_IRQ_PING0_CTRL_INV   (1 << 4)
#define HS_SPI_IRQ_PING0_POLL_TOUT  (1 << 3)
#define HS_SPI_IRQ_PING0_TX_UNDER   (1 << 2)
#define HS_SPI_IRQ_PING0_RX_OVER    (1 << 1)
#define HS_SPI_IRQ_PING0_CMD_DONE   (1 << 0)

  uint32    hs_spiIntStatusMasked;  // 0x000C
#define HS_SPI_IRQSM__PING1_USER    (28)
#define HS_SPI_IRQSM__PING1_USER_MASK   __mask(31, HS_SPI_IRQSM__PING1_USER)
#define HS_SPI_IRQSM__PING0_USER    (24)
#define HS_SPI_IRQSM__PING0_USER_MASK   __mask(27, HS_SPI_IRQSM__PING0_USER)

#define HS_SPI_IRQSM__PING1_CTRL_INV    (1 << 12)
#define HS_SPI_IRQSM__PING1_POLL_TOUT   (1 << 11)
#define HS_SPI_IRQSM__PING1_TX_UNDER    (1 << 10)
#define HS_SPI_IRQSM__PING1_RX_OVER (1 << 9)
#define HS_SPI_IRQSM__PING1_CMD_DONE    (1 << 8)

#define HS_SPI_IRQSM__PING0_CTRL_INV    (1 << 4)
#define HS_SPI_IRQSM__PING0_POLL_TOUT   (1 << 3)
#define HS_SPI_IRQSM__PING0_TX_UNDER    (1 << 2)
#define HS_SPI_IRQSM__PING0_RX_OVER     (1 << 1)
#define HS_SPI_IRQSM__PING0_CMD_DONE    (1 << 0)

  uint32    hs_spiIntMask;      // 0x0010
#define HS_SPI_IRQM_PING1_USER      (28)
#define HS_SPI_IRQM_PING1_USER_MASK __mask(31, HS_SPI_IRQM_PING1_USER)
#define HS_SPI_IRQM_PING0_USER      (24)
#define HS_SPI_IRQM_PING0_USER_MASK __mask(27, HS_SPI_IRQM_PING0_USER)

#define HS_SPI_IRQM_PING1_CTRL_INV  (1 << 12)
#define HS_SPI_IRQM_PING1_POLL_TOUT (1 << 11)
#define HS_SPI_IRQM_PING1_TX_UNDER  (1 << 10)
#define HS_SPI_IRQM_PING1_RX_OVER   (1 << 9)
#define HS_SPI_IRQM_PING1_CMD_DONE  (1 << 8)

#define HS_SPI_IRQM_PING0_CTRL_INV  (1 << 4)
#define HS_SPI_IRQM_PING0_POLL_TOUT (1 << 3)
#define HS_SPI_IRQM_PING0_TX_UNDER  (1 << 2)
#define HS_SPI_IRQM_PING0_RX_OVER   (1 << 1)
#define HS_SPI_IRQM_PING0_CMD_DONE  (1 << 0)

#define HS_SPI_INTR_CLEAR_ALL       (0xFF001F1F)

  uint32    hs_spiFlashCtrl;    // 0x0014
#define HS_SPI_FCTRL_MB_ENABLE      (23)
#define HS_SPI_FCTRL_SS_NUM         (20)
#define HS_SPI_FCTRL_SS_NUM_MASK    __mask(22, HS_SPI_FCTRL_SS_NUM)
#define HS_SPI_FCTRL_PROFILE_NUM    (16)
#define HS_SPI_FCTRL_PROFILE_NUM_MASK   __mask(18, HS_SPI_FCTRL_PROFILE_NUM)
#define HS_SPI_FCTRL_DUMMY_BYTES    (10)
#define HS_SPI_FCTRL_DUMMY_BYTES_MASK   __mask(11, HS_SPI_FCTRL_DUMMY_BYTES)
#define HS_SPI_FCTRL_ADDR_BYTES     (8)
#define HS_SPI_FCTRL_ADDR_BYTES_MASK    __mask(9, HS_SPI_FCTRL_ADDR_BYTES)
#define HS_SPI_FCTRL_ADDR_BYTES_2   (0)
#define HS_SPI_FCTRL_ADDR_BYTES_3   (1)
#define HS_SPI_FCTRL_ADDR_BYTES_4   (2)
#define HS_SPI_FCTRL_READ_OPCODE    (0)
#define HS_SPI_FCTRL_READ_OPCODE_MASK   __mask(7, HS_SPI_FCTRL_READ_OPCODE)

  uint32    hs_spiFlashAddrBase;    // 0x0018

  char      fill0[0x80 - 0x18];

  uint32    hs_spiPP_0_Cmd;     // 0x0080
#define HS_SPI_PP_SS_NUM        (12)
#define HS_SPI_PP_SS_NUM_MASK       __mask(14, HS_SPI_PP_SS_NUM)
#define HS_SPI_PP_PROFILE_NUM       (8)
#define HS_SPI_PP_PROFILE_NUM_MASK  __mask(10, HS_SPI_PP_PROFILE_NUM)

} HsSpiControl;

typedef struct HsSpiPingPong {

    uint32 command;
#define HS_SPI_SS_NUM (12)
#define HS_SPI_PROFILE_NUM (8)
#define HS_SPI_TRIGGER_NUM (4)
#define HS_SPI_COMMAND_VALUE (0)
    #define HS_SPI_COMMAND_NOOP (0)
    #define HS_SPI_COMMAND_START_NOW (1)
    #define HS_SPI_COMMAND_START_TRIGGER (2)
    #define HS_SPI_COMMAND_HALT (3)
    #define HS_SPI_COMMAND_FLUSH (4)

    uint32 status;
#define HS_SPI_ERROR_BYTE_OFFSET (16)
#define HS_SPI_WAIT_FOR_TRIGGER (2)
#define HS_SPI_SOURCE_BUSY (1)
#define HS_SPI_SOURCE_GNT (0)

    uint32 fifo_status;
    uint32 control;

} HsSpiPingPong;

typedef struct HsSpiProfile {

    uint32 clk_ctrl;
#define HS_SPI_ACCUM_RST_ON_LOOP (15)
#define HS_SPI_SPI_CLK_2X_SEL (14)
#define HS_SPI_FREQ_CTRL_WORD (0)

    uint32 signal_ctrl;
#define	HS_SPI_ASYNC_INPUT_PATH (1 << 16)
#define	HS_SPI_LAUNCH_RISING    (1 << 13)
#define	HS_SPI_LATCH_RISING     (1 << 12)

    uint32 mode_ctrl;
#define HS_SPI_PREPENDBYTE_CNT (24)
#define HS_SPI_MODE_ONE_WIRE (20)
#define HS_SPI_MULTIDATA_WR_SIZE (18)
#define HS_SPI_MULTIDATA_RD_SIZE (16)
#define HS_SPI_MULTIDATA_WR_STRT (12)
#define HS_SPI_MULTIDATA_RD_STRT (8)
#define HS_SPI_FILLBYTE (0)

    uint32 polling_config;
    uint32 polling_and_mask;
    uint32 polling_compare;
    uint32 polling_timeout;
    uint32 reserved;

} HsSpiProfile;

#define HS_SPI_OP_CODE 13
    #define HS_SPI_OP_SLEEP (0)
    #define HS_SPI_OP_READ_WRITE (1)
    #define HS_SPI_OP_WRITE (2)
    #define HS_SPI_OP_READ (3)
    #define HS_SPI_OP_SETIRQ (4)

#define HS_SPI ((volatile HsSpiControl * const) HSSPIM_BASE)
#define HS_SPI_PINGPONG0 ((volatile HsSpiPingPong * const) (HSSPIM_BASE+0x80))
#define HS_SPI_PINGPONG1 ((volatile HsSpiPingPong * const) (HSSPIM_BASE+0xc0))
#define HS_SPI_PROFILES ((volatile HsSpiProfile * const) (HSSPIM_BASE+0x100))
#define HS_SPI_FIFO0 ((volatile uint8 * const) (HSSPIM_BASE+0x200))
#define HS_SPI_FIFO1 ((volatile uint8 * const) (HSSPIM_BASE+0x400))

/*
** Misc Register Set Definitions.
*/

typedef struct Misc {
    uint32  unused0[2];                         /* 0x00 */
    uint32 IrqOutMask;                          /* 0x08 */
#define MISC_PCIE_EP_IRQ_MASK0                  (1<<0)
#define MISC_PCIE_EP_IRQ_MASK1                  (1<<1)

    uint32  unused1[13];                        /* 0x0c */
    uint32  miscSpecialPadControl;              /* 0x40 */
#define RGMII_PAD_SEL_SHIFT  0
#define RGMII_PAD_SEL_MASK   0x3
#define RGMII_PAD_SEL_3_3V   0
#define RGMII_PAD_SEL_2_5V   1

    uint32  miscTestControl;                    /* 0x44 */
    uint32  miscTestPortBlkEn[2];               /* 0x48 */
    uint32  miscTestPortBlkData[2];             /* 0x50 */
    uint32  miscTestPortCommand;                /* 0x58 */   
} Misc;

#define MISC ((volatile Misc * const) MISC_BASE)

/*
** LedControl Register Set Definitions.
*/

#pragma pack(push, 4)
typedef struct LedControl {
    uint32  ledInit;
#define LED_LED_TEST                (1 << 31)
#define LED_SHIFT_TEST              (1 << 30)
#define LED_SERIAL_LED_SHIFT_DIR    (1 << 16)
#define LED_SERIAL_LED_DATA_PPOL    (1 << 15)
#define LEDSERIAL_LED_CLK_NPOL      (1 << 14)
#define LED_SERIAL_LED_MUX_SEL      (1 << 13)
#define LED_SERIAL_LED_EN           (1 << 12)
#define LED_FAST_INTV_SHIFT         6
#define LED_FAST_INTV_MASK          (0x3F<<LED_FAST_INTV_SHIFT)
#define LED_SLOW_INTV_SHIFT         0
#define LED_SLOW_INTV_MASK          (0x3F<<LED_SLOW_INTV_SHIFT)
#define LED_INTERVAL_20MS           1

    uint64  ledMode;
#define LED_MODE_MASK               (uint64)0x3
#define LED_MODE_OFF                (uint64)0x0
#define LED_MODE_BLINK              (uint64)0x1
#define LED_MODE_FLASH              (uint64)0x2
#define LED_MODE_ON                 (uint64)0x3

    uint32  ledHWDis;
    /* TBD. Need mapping of bit to particular LED */

    uint32  ledStrobe;
    uint32  ledLinkActSelHigh;
#define LED_4_ACT_SHIFT             0
#define LED_5_ACT_SHIFT             4
#define LED_6_ACT_SHIFT             8
#define LED_7_ACT_SHIFT             12
#define LED_4_LINK_SHIFT            16
#define LED_5_LINK_SHIFT            20
#define LED_6_LINK_SHIFT            24
#define LED_7_LINK_SHIFT            28
    uint32  ledLinkActSelLow;
#define LED_0_ACT_SHIFT             0
#define LED_1_ACT_SHIFT             4
#define LED_2_ACT_SHIFT             8
#define LED_3_ACT_SHIFT             12
#define LED_0_LINK_SHIFT            16
#define LED_1_LINK_SHIFT            20
#define LED_2_LINK_SHIFT            24
#define LED_3_LINK_SHIFT            28

    uint32  ledReadback;
    uint32  ledSerialMuxSelect;
    uint32  ledXorReg;
} LedControl;
#pragma pack(pop)

#define LED ((volatile LedControl * const) LED_BASE)

#define GPIO_NUM_TO_LED_MODE_SHIFT(X) \
    ((((X) & BP_GPIO_NUM_MASK) < 8) ? (32 + (((X) & BP_GPIO_NUM_MASK) << 1)) : \
    ((((X) & BP_GPIO_NUM_MASK) - 8) << 1))


/*
** SDR/DDR Memory Controller Register Set Definitions.
*/

typedef struct MEMCControl {
    uint32  SDR_CFG;                             /* 0x00 */
#define MEMC_SDRAM_SPACE_SHIFT         4
#define MEMC_SDRAM_SPACE_MASK          (0xF<<MEMC_SDRAM_SPACE_SHIFT)
#define MEMC_SDRAM_SPACE2_SHIFT        8
#define MEMC_SDRAM_SPACE2_MASK         (0xF<<MEMC_SDRAM_SPACE2_SHIFT)
    uint32  SEMAPHORE0;                          /* 0x04 */
    uint32  SEMAPHORE1;                          /* 0x08 */
    uint32  SEMAPHORE2;                          /* 0x0C */
    uint32  SEMAPHORE3;                          /* 0x10 */
    uint32  PRI_CFG;                             /* 0x14 */
    uint32  PID_SELECT0;                         /* 0x18 */
    uint32  PID_SELECT1;                         /* 0x1C */
    uint32  UBUS2_THRESHOLD;                     /* 0x20 */
    uint32  AUTO_REFRESH;                        /* 0x24 */
    uint32  TIMING_PARAM;                        /* 0x28 */
    uint32  RESERVED1;                           /* 0x2C */
    uint32  DDR_TIMING_PARAM;                    /* 0x30 */
    uint32  DDR_DRIVE_PARAM;                     /* 0x34 */
} MEMCControl;

#define MEMC ((volatile MEMCControl * const) SDRAM_CTRL_BASE)


#define IUDMA_MAX_CHANNELS          32

/*
** DMA Channel Configuration (1 .. 32)
*/
typedef struct DmaChannelCfg {
  uint32        cfg;                    /* (00) assorted configuration */
#define         DMA_ENABLE      0x00000001  /* set to enable channel */
#define         DMA_PKT_HALT    0x00000002  /* idle after an EOP flag is detected */
#define         DMA_BURST_HALT  0x00000004  /* idle after finish current memory burst */
  uint32        intStat;                /* (04) interrupts control and status */
  uint32        intMask;                /* (08) interrupts mask */
#define         DMA_BUFF_DONE   0x00000001  /* buffer done */
#define         DMA_DONE        0x00000002  /* packet xfer complete */
#define         DMA_NO_DESC     0x00000004  /* no valid descriptors */
#define         DMA_RX_ERROR    0x00000008  /* rxdma detect client protocol error */
  uint32        maxBurst;               /* (0C) max burst length permitted */
#define         DMA_DESCSIZE_SEL 0x00040000  /* DMA Descriptor Size Selection */
} DmaChannelCfg;

/*
** DMA State RAM (1 .. 16)
*/
typedef struct DmaStateRam {
  uint32        baseDescPtr;            /* (00) descriptor ring start address */
  uint32        state_data;             /* (04) state/bytes done/ring offset */
  uint32        desc_len_status;        /* (08) buffer descriptor status and len */
  uint32        desc_base_bufptr;       /* (0C) buffer descrpitor current processing */
} DmaStateRam;


/*
** DMA Registers
*/
typedef struct DmaRegs {
    uint32 controller_cfg;              /* (00) controller configuration */
#define DMA_MASTER_EN           0x00000001
#define DMA_FLOWC_CH1_EN        0x00000002
#define DMA_FLOWC_CH3_EN        0x00000004

    // Flow control Ch1
    uint32 flowctl_ch1_thresh_lo;           /* 004 */
    uint32 flowctl_ch1_thresh_hi;           /* 008 */
    uint32 flowctl_ch1_alloc;               /* 00c */
#define DMA_BUF_ALLOC_FORCE     0x80000000

    // Flow control Ch3
    uint32 flowctl_ch3_thresh_lo;           /* 010 */
    uint32 flowctl_ch3_thresh_hi;           /* 014 */
    uint32 flowctl_ch3_alloc;               /* 018 */

    // Flow control Ch5
    uint32 flowctl_ch5_thresh_lo;           /* 01C */
    uint32 flowctl_ch5_thresh_hi;           /* 020 */
    uint32 flowctl_ch5_alloc;               /* 024 */

    // Flow control Ch7
    uint32 flowctl_ch7_thresh_lo;           /* 028 */
    uint32 flowctl_ch7_thresh_hi;           /* 02C */
    uint32 flowctl_ch7_alloc;               /* 030 */

    uint32 ctrl_channel_reset;              /* 034 */
    uint32 ctrl_channel_debug;              /* 038 */
    uint32 reserved1;                       /* 03C */
    uint32 ctrl_global_interrupt_status;    /* 040 */
    uint32 ctrl_global_interrupt_mask;      /* 044 */

    // Unused words
    uint8 reserved2[0x200-0x48];

    // Per channel registers/state ram
    DmaChannelCfg chcfg[IUDMA_MAX_CHANNELS];/* (200-3FF) Channel configuration */
    union {
        DmaStateRam     s[IUDMA_MAX_CHANNELS];
        uint32          u32[4 * IUDMA_MAX_CHANNELS];
    } stram;                                /* (400-5FF) state ram */
} DmaRegs;

#define SAR_DMA ((volatile DmaRegs * const) SAR_DMA_BASE)
#define SW_DMA  ((volatile DmaRegs * const) SWITCH_DMA_BASE)

/*
** DMA Buffer
*/
typedef struct DmaDesc {
  union {
    struct {
        uint16        length;                   /* in bytes of data in buffer */
#define          DMA_DESC_USEFPM    0x8000
#define          DMA_DESC_MULTICAST 0x4000
#define          DMA_DESC_BUFLENGTH 0x0fff
        uint16        status;                   /* buffer status */
#define          DMA_OWN                0x8000  /* cleared by DMA, set by SW */
#define          DMA_EOP                0x4000  /* last buffer in packet */
#define          DMA_SOP                0x2000  /* first buffer in packet */
#define          DMA_WRAP               0x1000  /* */
#define          DMA_PRIO               0x0C00  /* Prio for Tx */
#define          DMA_APPEND_BRCM_TAG    0x0200
#define          DMA_APPEND_CRC         0x0100
#define          USB_ZERO_PKT           (1<< 0) // Set to send zero length packet
    };
    uint32      word0;
  };

  uint32        address;                /* address of data */
} DmaDesc;

/*
** 16 Byte DMA Buffer
*/
typedef struct {
  union {
    struct {
        uint16        length;                   /* in bytes of data in buffer */
#define          DMA_DESC_USEFPM        0x8000
#define          DMA_DESC_MULTICAST     0x4000
#define          DMA_DESC_BUFLENGTH     0x0fff
        uint16        status;                   /* buffer status */
#define          DMA_OWN                0x8000  /* cleared by DMA, set by SW */
#define          DMA_EOP                0x4000  /* last buffer in packet */
#define          DMA_SOP                0x2000  /* first buffer in packet */
#define          DMA_WRAP               0x1000  /* */
#define          DMA_PRIO               0x0C00  /* Prio for Tx */
#define          DMA_APPEND_BRCM_TAG    0x0200
#define          DMA_APPEND_CRC         0x0100
#define          USB_ZERO_PKT           (1<< 0) // Set to send zero length packet
    };
    uint32      word0;
  };

  uint32        address;                 /* address of data */
  uint32        control;
#define         GEM_ID_MASK             0x001F
  uint32        reserved;
} DmaDesc16;


typedef struct USBControl {
    uint32 Setup;                         /* 0x5200 */
#define USBH_IPP                (1<<5)
#define USBH_IOC                (1<<4)

    uint32 PllControl1;                   /* 0x5204 */
#define PLLC_PLL_SUSPEND_EN     (1 << 27)
#define PLLC_PHYPLL_BYP         (1 << 29)
#define PLLC_PLL_RESET          (1 << 30)
#define PLLC_PLL_IDDQ_PWRDN     (1 << 31)
    //#define PLLC_PLL_PWRDN_DELAY    0x00000400

    uint32 FrameAdjustValue;              /* 0x5208 */
    uint32 SwapControl;                   /* 0x520C */        
#define USB_DEVICE_SEL          (1<<6)
//#define EHCI_LOGICAL_ADDRESS_EN (1<<5)
#define EHCI_ENDIAN_SWAP        (1<<4)
#define EHCI_DATA_SWAP          (1<<3)
#define OHCI_LOGICAL_ADDRESS_EN (1<<2)
#define OHCI_ENDIAN_SWAP        (1<<1)
#define OHCI_DATA_SWAP          (1<<0)

  uint32 spare;                           /* 0x5210 */   
  uint32 MDIO;                            /* 0x5214 */      
  uint32 MDIO32;                          /* 0x5218 */  
  uint32 TestPortControl;                 /* 0x521C */  
  uint32 USBSimControl;                   /* 0x5220 */  
#define USBH_OHCI_MEM_REQ_DIS   (1<<1)

  uint32 Testctl;                         /* 0x5224 */    
  uint32 TestMon;                         /* 0x5228 */     
  uint32 UTMIControl1;                    /* 0x522C */  
#define UTMI_SOFT_RESETB           (0x2)
#define UTMI_PHY_MODE_MASK         (0x3 << 2)
#define UTMI_PHY_MODE_DEVICE       (0x1 << 2)
#define UTMI_PHY_MODE_P1_MASK      (0x3 << 18)
#define UTMI_PHY_MODE_P1_DEVICE    (0x1 << 18)


} USBControl;

#define USBH ((volatile USBControl * const) USBH_CFG_BASE)

typedef struct EthSwMIBRegs {
    unsigned int TxOctetsLo;
    unsigned int TxOctetsHi;
    unsigned int TxDropPkts;
    unsigned int TxQoSPkts;
    unsigned int TxBroadcastPkts;
    unsigned int TxMulticastPkts;
    unsigned int TxUnicastPkts;
    unsigned int TxCol;
    unsigned int TxSingleCol;
    unsigned int TxMultipleCol;
    unsigned int TxDeferredTx;
    unsigned int TxLateCol;
    unsigned int TxExcessiveCol;
    unsigned int TxFrameInDisc;
    unsigned int TxPausePkts;
    unsigned int TxQoSOctetsLo;
    unsigned int TxQoSOctetsHi;
    unsigned int RxOctetsLo;
    unsigned int RxOctetsHi;
    unsigned int RxUndersizePkts;
    unsigned int RxPausePkts;
    unsigned int Pkts64Octets;
    unsigned int Pkts65to127Octets;
    unsigned int Pkts128to255Octets;
    unsigned int Pkts256to511Octets;
    unsigned int Pkts512to1023Octets;
    unsigned int Pkts1024to1522Octets;
    unsigned int RxOversizePkts;
    unsigned int RxJabbers;
    unsigned int RxAlignErrs;
    unsigned int RxFCSErrs;
    unsigned int RxGoodOctetsLo;
    unsigned int RxGoodOctetsHi;
    unsigned int RxDropPkts;
    unsigned int RxUnicastPkts;
    unsigned int RxMulticastPkts;
    unsigned int RxBroadcastPkts;
    unsigned int RxSAChanges;
    unsigned int RxFragments;
    unsigned int RxExcessSizeDisc;
    unsigned int RxSymbolError;
    unsigned int RxQoSPkts;
    unsigned int RxQoSOctetsLo;
    unsigned int RxQoSOctetsHi;
    unsigned int Pkts1523to2047;
    unsigned int Pkts2048to4095;
    unsigned int Pkts4096to8191;
    unsigned int Pkts8192to9728;
} EthSwMIBRegs;

#define ETHSWMIBREG ((volatile EthSwMIBRegs * const) (SWITCH_BASE + 0x2000))

/* Enet registers controlling rx iuDMA channel */
typedef struct EthSwQosIngressPortPriRegs{
    unsigned short pri_id_map[9];
} EthSwQosIngressPortPriRegs;

#define ETHSWQOSREG ((volatile EthSwQosIngressPortPriRegs * const) (SWITCH_BASE + 0x3050))

/* SAR registers controlling rx iuDMA channel */
typedef struct SarRxMuxRegs{
    unsigned int vcid0_qid;
    unsigned int vcid1_qid;
} SarRxMuxRegs;

#define SARRXMUXREG ((volatile SarRxMuxRegs * const) (SAR_BASE + 0x0400))

/*
** PCI-E
*/
#define UBUS2_PCIE		/* define for UBUS2 architecture */

typedef struct PcieRegs{
  uint32 devVenID;
  uint16 command;
  uint16 status;
  uint32 revIdClassCode;
  uint32 headerTypeLatCacheLineSize;
  uint32 bar1;
  uint32 bar2;
  uint32 priSecBusNo;
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_SUB_BUS_NO_MASK              0x00ff0000
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_SUB_BUS_NO_SHIFT             16
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_SEC_BUS_NO_MASK              0x0000ff00
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_SEC_BUS_NO_SHIFT             8
#define PCIE_CFG_TYPE1_PRI_SEC_BUS_NO_PRI_BUS_NO_MASK              0x000000ff

  uint32 secStatusIoBaseLimit;
  uint32 rcMemBaseLimit;
  uint32 rcPrefBaseLimit;
  uint32 rcPrefBaseHi;
  uint32 rcPrefLimitHi;
  uint32 rcIoBaseLimit;
  uint32 capPointer;
  uint32 expRomBase;
  uint32 bridgeCtrl;
  uint32 unused1[27];

  /* PcieExpressCtrlRegs */
  uint16 pciExpressCap;
  uint16 pcieCapabilitiy;
  uint32 deviceCapability;
  uint16 deviceControl;
  uint16 deviceStatus;
  uint32 linkCapability;
  uint16 linkControl;
  uint16 linkStatus;
  uint32 slotCapability;
  uint16 slotControl;
  uint16 slotStatus;
  uint16 rootControl;
  uint16 rootCap;
  uint32 rootStatus;
  uint32 deviceCapability2;
  uint16 deviceControl2;
  uint16 deviceStatus2;
  uint32 linkCapability2;
  uint16 linkControl2;
  uint16 linkStatus2;
  uint32 slotCapability2;
  uint16 slotControl2;
  uint16 slotStatus2;
  uint32 unused2[6];

  /* PcieErrorRegs */
  uint16 advErrCapId;
  uint16 advErrCapOff;
  uint32 ucErrStatus;
  uint32 ucorrErrMask;
  uint32 ucorrErrSevr;
  uint32 corrErrStatus;
  uint32 corrErrMask;
  uint32 advErrCapControl;
  uint32 headerLog1;
  uint32 headerLog2;
  uint32 headerLog3;
  uint32 headerLog4;
  uint32 rootErrorCommand;
  uint32 rootErrorStatus;
  uint32 rcCorrId;
  uint32 rcFatalNonfatalId;
  uint32 unused3[10];

  /* PcieVcRegs */
  uint16 vcCapId;
  uint16 vcCapOffset;
  uint32 prtVcCapability;
  uint32 portVcCapability2;
  uint16 portVcControl;
  uint16 portVcCtatus;
  uint32 portArbStatus;
  uint32 vcRsrcControl;
  uint32 vcRsrcStatus;
} PcieRegs;

typedef struct PcieRcCfgVendorRegs{
	uint32 vendorCap;
	uint32 specificHeader;
	uint32 specificReg1;
#define PCIE_RC_CFG_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_BAR1_SHIFT			0	
#define PCIE_RC_CFG_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_BAR2_SHIFT			2
#define PCIE_RC_CFG_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_BAR3_SHIFT			4
#define PCIE_RC_CFG_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_WORD_ALIGN			0x0	
#define PCIE_RC_CFG_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_HWORD_ALIGN		0x1	
#define PCIE_RC_CFG_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_BYTE_ALIGN			0x2
}PcieRcCfgVendorRegs;

typedef struct PcieBlk404Regs{
  uint32 unused;          /* 0x404 */
  uint32 config2;         /* 0x408 */
#define PCIE_IP_BLK404_CONFIG_2_BAR1_SIZE_MASK         0x0000000f
#define PCIE_IP_BLK404_CONFIG_2_BAR1_DISABLE           0
  uint32 config3;         /* 0x40c */
  uint32 pmDataA;         /* 0x410 */
  uint32 pmDataB;         /* 0x414 */
} PcieBlk404Regs;

typedef struct PcieBlk428Regs{
  uint32 vpdIntf;        /* 0x428 */
  uint16 unused_g;       /* 0x42c */
  uint16 vpdAddrFlag;    /* 0x42e */
  uint32 vpdData;        /* 0x430 */
  uint32 idVal1;         /* 0x434 */
  uint32 idVal2;         /* 0x438 */
  uint32 idVal3;         /* 0x43c */
#define PCIE_IP_BLK428_ID_VAL3_REVISION_ID_MASK                    0xff000000
#define PCIE_IP_BLK428_ID_VAL3_REVISION_ID_SHIFT                   24
#define PCIE_IP_BLK428_ID_VAL3_CLASS_CODE_MASK                     0x00ffffff
#define PCIE_IP_BLK428_ID_VAL3_CLASS_CODE_SHIFT                    16
#define PCIE_IP_BLK428_ID_VAL3_SUB_CLASS_CODE_SHIFT                 8

  uint32 idVal4;
  uint32 idVal5;
  uint32 unused_h;
  uint32 idVal6;
  uint32 msiData;
  uint32 msiAddr_h;
  uint32 msiAddr_l;
  uint32 msiMask;
  uint32 msiPend;
  uint32 pmData_c;
  uint32 msixControl;
  uint32 msixTblOffBir;
  uint32 msixPbaOffBit;
  uint32 unused_k;
  uint32 pcieCapability;
  uint32 deviceCapability;
  uint32 unused_l;
  uint32 linkCapability;
  uint32 bar2Config;
  uint32 pcieDeviceCapability2;
  uint32 pcieLinkCapability2;
  uint32 pcieLinkControl;
  uint32 pcieLinkCapabilityRc;
  uint32 bar3Config;
  uint32 rootCap;
  uint32 devSerNumCapId;
  uint32 lowerSerNum;
  uint32 upperSerNum;
  uint32 advErrCap;
  uint32 pwrBdgtData0;
  uint32 pwrBdgtData1;
  uint32 pwrBdgtData2;
  uint32 pwdBdgtData3;
  uint32 pwrBdgtData4;
  uint32 pwrBdgtData5;
  uint32 pwrBdgtData6;
  uint32 pwrBdgtData7;
  uint32 pwrBdgtCapability;
  uint32 vsecHdr;
  uint32 rcUserMemLo1;
  uint32 rcUserMemHi1;
  uint32 rcUserMemLo2;
  uint32 rcUserMemHi2;
} PcieBlk428Regs;

typedef struct PcieBlk800Regs{
#define NUM_PCIE_BLK_800_CTRL_REGS  6
  uint32 tlControl[NUM_PCIE_BLK_800_CTRL_REGS];
  uint32 tlCtlStat0;
  uint32 pmStatus0;
  uint32 pmStatus1;

#define NUM_PCIE_BLK_800_TAGS       32
  uint32 tlStatus[NUM_PCIE_BLK_800_TAGS];
  uint32 tlHdrFcStatus;
  uint32 tlDataFcStatus;
  uint32 tlHdrFcconStatus;
  uint32 tlDataFcconStatus;
  uint32 tlTargetCreditStatus;
  uint32 tlCreditAllocStatus;
  uint32 tlSmlogicStatus;
} PcieBlk800Regs;


typedef struct PcieBlk1000Regs{
#define NUM_PCIE_BLK_1000_PDL_CTRL_REGS  16
  uint32 pdlControl[NUM_PCIE_BLK_1000_PDL_CTRL_REGS];
  uint32 dlattnVec;
  uint32 dlAttnMask;
  uint32 dlStatus;        /* 0x1048 */
#define PCIE_IP_BLK1000_DL_STATUS_PHYLINKUP_MASK                   0x00002000
#define PCIE_IP_BLK1000_DL_STATUS_PHYLINKUP_SHIFT                  13
  uint32 dlTxChecksum;
  uint32 dlForcedUpdateGen1;
  uint32 mdioAddr;
  uint32 mdioWrData;
  uint32 mdioRdData;
  uint32 dlRxPFcCl;
  uint32 dlRxCFcCl;
  uint32 dlRxAckNack;
  uint32 dlTxRxSeqnb;
  uint32 dlTxPFcAl;
  uint32 dlTxNpFcAl;
  uint32 regDlSpare;
  uint32 dlRegSpare;
  uint32 dlTxRxSeq;
  uint32 dlRxNpFcCl;
} PcieBlk1000Regs;

typedef struct PcieBlk1800Regs{
#define NUM_PCIE_BLK_1800_PHY_CTRL_REGS         8
  uint32 phyCtrl[NUM_PCIE_BLK_1800_PHY_CTRL_REGS];
#define REG_POWERDOWN_P1PLL_ENA                      (1<<12)
} PcieBlk1800Regs;

typedef struct PcieMiscRegs{
  uint32 reset_ctrl;                    /* 4000 Reset Control Register */
  uint32 eco_ctrl_core;                 /* 4004 ECO Core Reset Control Register */
  uint32 misc_ctrl;                     /* 4008 MISC Control Register */
#define PCIE_MISC_CTRL_CFG_READ_UR_MODE                            (1<<13)
  uint32 cpu_2_pcie_mem_win0_lo;        /* 400c CPU to PCIe Memory Window 0 Low */
#define PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_ADDR_MASK              0xfff00000
#define PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_ADDR_SHIFT             20
#define PCIE_MISC_CPU_2_PCIE_MEM_ENDIAN_MODE_NO_SWAP               0
#define PCIE_MISC_CPU_2_PCIE_MEM_ENDIAN_MODE_HALF_WORD_SWAP        1
#define PCIE_MISC_CPU_2_PCIE_MEM_ENDIAN_MODE_HALF_BYTE_SWAP        2
  uint32 cpu_2_pcie_mem_win0_hi;        /* 4010 CPU to PCIe Memory Window 0 High */
  uint32 cpu_2_pcie_mem_win1_lo;        /* 4014 CPU to PCIe Memory Window 1 Low */
  uint32 cpu_2_pcie_mem_win1_hi;        /* 4018 CPU to PCIe Memory Window 1 High */
  uint32 cpu_2_pcie_mem_win2_lo;        /* 401c CPU to PCIe Memory Window 2 Low */
  uint32 cpu_2_pcie_mem_win2_hi;        /* 4020 CPU to PCIe Memory Window 2 High */
  uint32 cpu_2_pcie_mem_win3_lo;        /* 4024 CPU to PCIe Memory Window 3 Low */
  uint32 cpu_2_pcie_mem_win3_hi;        /* 4028 CPU to PCIe Memory Window 3 High */
  uint32 rc_bar1_config_lo;             /* 402c RC BAR1 Configuration Low Register */
#define  PCIE_MISC_RC_BAR_CONFIG_LO_MATCH_ADDRESS_MASK             0xfff00000
#define  PCIE_MISC_RC_BAR_CONFIG_LO_SIZE_256MB                     0xd
#define  PCIE_MISC_RC_BAR_CONFIG_LO_SIZE_MAX                       0x11     /* max is 4GB */
  uint32 rc_bar1_config_hi;             /* 4030 RC BAR1 Configuration High Register */
  uint32 rc_bar2_config_lo;             /* 4034 RC BAR2 Configuration Low Register */
  uint32 rc_bar2_config_hi;             /* 4038 RC BAR2 Configuration High Register */
  uint32 rc_bar3_config_lo;             /* 403c RC BAR3 Configuration Low Register */
  uint32 rc_bar3_config_hi;             /* 4040 RC BAR3 Configuration High Register */
  uint32 msi_bar_config_lo;             /* 4044 Message Signaled Interrupt Base Address Low Register */
  uint32 msi_bar_config_hi;             /* 4048 Message Signaled Interrupt Base Address High Register */
  uint32 msi_data_config;               /* 404c Message Signaled Interrupt Data Configuration Register */
  uint32 rc_bad_address_lo;             /* 4050 RC Bad Address Register Low */
  uint32 rc_bad_address_hi;             /* 4054 RC Bad Address Register High */
  uint32 rc_bad_data;                   /* 4058 RC Bad Data Register */
  uint32 rc_config_retry_timeout;       /* 405c RC Configuration Retry Timeout Register */
  uint32 eoi_ctrl;                      /* 4060 End of Interrupt Control Register */
  uint32 pcie_ctrl;                     /* 4064 PCIe Control */
  uint32 pcie_status;                   /* 4068 PCIe Status */
  uint32 revision;                      /* 406c PCIe Revision */
  uint32 cpu_2_pcie_mem_win0_base_limit;/* 4070 CPU to PCIe Memory Window 0 base/limit */
#define PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_LIMIT_LIMIT_MASK        0xfff00000
#define PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_LIMIT_LIMIT_SHIFT       20
#define PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_LIMIT_BASE_MASK         0x0000fff0
#define PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_LIMIT_BASE_SHIFT        4
  uint32 cpu_2_pcie_mem_win1_base_limit;/* 4074 CPU to PCIe Memory Window 1 base/limit */
  uint32 cpu_2_pcie_mem_win2_base_limit;/* 4078 CPU to PCIe Memory Window 2 base/limit */
  uint32 cpu_2_pcie_mem_win3_base_limit;/* 407c CPU to PCIe Memory Window 3 base/limit */
  uint32 ubus_ctrl;                     /* 4080 UBUS Control */
  uint32 ubus_timeout;                  /* 4084 UBUS Timeout */
  uint32 ubus_bar1_config_remap;        /* 4088 UBUS BAR1 System Bus Address Remap Register */
#define  PCIE_MISC_UBUS_BAR_CONFIG_OFFSET_MASK                      0xfff00000
#define  PCIE_MISC_UBUS_BAR_CONFIG_ACCESS_EN                        1
  uint32 ubus_bar2_config_remap;        /* 408c UBUS BAR2 System Bus Address Remap Register */
  uint32 ubus_bar3_config_remap;        /* 4090 UBUS BAR3 System Bus Address Remap Register */
  uint32 ubus_status;                   /* 4094 UBUS Status */
} PcieMiscRegs;

typedef struct PcieMiscPerstRegs{
  uint32 perst_eco_ctrl_perst;          /* 4100 ECO PCIE Reset Control Register */
  uint32 perst_eco_cce_status;          /* 4104 Config Copy Engine Status */
} PcieMiscPerstRegs;

typedef struct PcieMiscHardRegs{
  uint32 hard_eco_ctrl_hard;            /* 4200 ECO Hard Reset Control Register */
  uint32 hard_pcie_hard_debug;          /* 4204 PCIE Hard Debug Register */
#define PCIE_MISC_HARD_PCIE_HARD_DEBUG_SERDES_IDDQ                   (1<<23)
} PcieMiscHardRegs;

typedef struct PcieCpuIntr1Regs{
  uint32 status;
#define PCIE_CPU_INTR1_IPI_CPU_INTR                                  (1<<8)
#define PCIE_CPU_INTR1_PCIE_UBUS_CPU_INTR                            (1<<7)
#define PCIE_CPU_INTR1_PCIE_NMI_CPU_INTR                             (1<<6)
#define PCIE_CPU_INTR1_PCIE_INTR_CPU_INTR                            (1<<5)
#define PCIE_CPU_INTR1_PCIE_INTD_CPU_INTR                            (1<<4)
#define PCIE_CPU_INTR1_PCIE_INTC_CPU_INTR                            (1<<3)
#define PCIE_CPU_INTR1_PCIE_INTB_CPU_INTR                            (1<<2)
#define PCIE_CPU_INTR1_PCIE_INTA_CPU_INTR                            (1<<1)
#define PCIE_CPU_INTR1_PCIE_ERR_ATTN_CPU_INTR                        (1<<0)
  uint32 maskStatus;
  uint32 maskSet;
  uint32 maskClear;  
} PcieCpuIntr1Regs;

typedef struct PcieExtCfgRegs{
  uint32 index;
#define PCIE_EXT_CFG_BUS_NUM_MASK                                     0x0ff00000
#define PCIE_EXT_CFG_BUS_NUM_SHIFT                                    20
#define PCIE_EXT_CFG_DEV_NUM_MASK                                     0x000f0000
#define PCIE_EXT_CFG_DEV_NUM_SHIFT                                    15
#define PCIE_EXT_CFG_FUNC_NUM_MASK                                    0x00007000
#define PCIE_EXT_CFG_FUNC_NUM_SHIFT                                   12
  uint32 data;
  uint32 scratch;
} PcieExtCfgRegs;

#define PCIEH                         ((volatile uint32 * const) PCIE_BASE)
#define PCIEH_REGS                    ((volatile PcieRegs * const) PCIE_BASE)
#define PCIEH_RC_CFG_VENDOR_REGS      ((volatile PcieRcCfgVendorRegs * const) \
                                        (PCIE_BASE+0x180)) 
#define PCIEH_BLK_404_REGS            ((volatile PcieBlk404Regs * const) \
                                        (PCIE_BASE+0x404))
#define PCIEH_BLK_428_REGS            ((volatile PcieBlk428Regs * const) \
                                        (PCIE_BASE+0x428))
#define PCIEH_BLK_800_REGS            ((volatile PcieBlk800Regs * const) \
                                        (PCIE_BASE+0x800))                                        
#define PCIEH_BLK_1000_REGS           ((volatile PcieBlk1000Regs * const) \
                                        (PCIE_BASE+0x1000))
#define PCIEH_BLK_1800_REGS           ((volatile PcieBlk1800Regs * const) \
                                        (PCIE_BASE+0x1800))
#define PCIEH_MISC_REGS               ((volatile PcieMiscRegs * const)  \
                                        (PCIE_BASE+0x4000))
#define PCIEH_MISC_PERST_REGS         ((volatile PcieMiscPerstRegs * const)  \
                                        (PCIE_BASE+0x4100))
#define PCIEH_MISC_HARD_REGS         ((volatile PcieMiscHardRegs * const)  \
                                        (PCIE_BASE+0x4200))
#define PCIEH_CPU_INTR1_REGS         ((volatile PcieCpuIntr1Regs * const)  \
                                        (PCIE_BASE+0x8300))
#define PCIEH_PCIE_EXT_CFG_REGS      ((volatile PcieExtCfgRegs * const)  \
                                        (PCIE_BASE+0x8400))
#define PCIEH_DEV_OFFSET              0x9000                                                                           

#define PCIEH_RC_CFG_PRIV0            PCIEH_BLK_404_REGS
#define PCIEH_RC_CFG_PRIV1            PCIEH_BLK_428_REGS
#define PCIEH_RC_TL                   PCIEH_BLK_800_REGS
#define PCIEH_RC_DL                   PCIEH_BLK_1000_REGS
#define PCIEH_EXT_CFG_DATA            PCIEH_DEV_OFFSET

#define PCIEH_MEM1_BASE               0x10200000
#define PCIEH_MEM1_SIZE               0x00100000

#define PCIEH_MEM2_BASE               0xa0000000
#define PCIEH_MEM2_SIZE               0x20000000

#define DDR_UBUS_ADDRESS_BASE         0
#ifdef __cplusplus
}
#endif

#endif

