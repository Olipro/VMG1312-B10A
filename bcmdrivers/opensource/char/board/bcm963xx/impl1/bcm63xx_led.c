/*
    Copyright 2000-2011 Broadcom Corporation
 
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

/***************************************************************************
 * File Name  : bcm63xx_led.c
 *
 * Description: 
 *    This file contains bcm963xx board led control API functions. 
 *
 ***************************************************************************/

/* Includes. */
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/bcm_assert_locks.h>
#include <asm/uaccess.h>

#include <bcm_map_part.h>
#include <board.h>
#include <boardparms.h>

extern spinlock_t bcm_gpio_spinlock;

#define k125ms              (HZ / 8)   // 125 ms
#define kFastBlinkCount     0          // 125ms
#define kSlowBlinkCount     1          // 250ms

#define kLedOff             0
#define kLedOn              1

#define kLedGreen           0
#define kLedRed             1

// uncomment // for debug led
// #define DEBUG_LED

typedef int (*BP_LED_FUNC) (unsigned short *);

typedef struct {
    BOARD_LED_NAME ledName;
    BP_LED_FUNC bpFunc;
    BP_LED_FUNC bpFuncFail;
} BP_LED_INFO, *PBP_LED_INFO;

typedef struct {
    unsigned short ledGreenGpio;             // GPIO # for LED
    unsigned short ledRedGpio;               // GPIO # for Fail LED
    BOARD_LED_STATE ledState;       // current led state
    short blinkCountDown;           // Count for blink states
} LED_CTRL, *PLED_CTRL;

static BP_LED_INFO bpLedInfo[] =
{
    {kLedAdsl, BpGetAdslLedGpio, BpGetAdslFailLedGpio},
    {kLedSecAdsl, BpGetSecAdslLedGpio, BpGetSecAdslFailLedGpio},
#if 1 //__MSTC__, RaynorChung: Ssupport VDSL LED   
    {kLedVdsl, BpGetVdslLedGpio, BpGetVdslFailLedGpio},
    {kLedSecVdsl, BpGetSecVdslLedGpio, BpGetSecVdslFailLedGpio},
#endif
    {kLedWanData, BpGetWanDataLedGpio, BpGetWanErrorLedGpio},
#if 1 // __MSTC__, Richard Huang
    {kLedInternetData, BpGetInternetDataLedGpio, NULL},
#endif
    {kLedSes, BpGetWirelessSesLedGpio, NULL},
    {kLedVoip, BpGetVoipLedGpio, NULL},
    {kLedVoip1, BpGetVoip1LedGpio, BpGetVoip1FailLedGpio},
    {kLedVoip2, BpGetVoip2LedGpio, BpGetVoip2FailLedGpio},
    {kLedPots, BpGetPotsLedGpio, NULL},
    {kLedDect, BpGetDectLedGpio, NULL},
    {kLedGpon, BpGetGponLedGpio, BpGetGponFailLedGpio},
#if 1 //__MSTC__, Dennis
    {kLedMoCAL, NULL, NULL},
    {kLedPowerG, BpGetBootloaderPowerOnLedGpio, BpGetBootloaderStopLedGpio},
    {kLedMoCAW, NULL, NULL},
#else
    {kLedMoCA, BpGetMoCALedGpio, BpGetMoCAFailLedGpio},
#endif
#if 1 //__MSTC__, RaynorChung: Ssupport USB LED   
	{kLedUSB0, BpGetUSB0LedGpio, BpGetUSB0FailLedGpio},
	{kLedUSB1, BpGetUSB1LedGpio, BpGetUSB1FailLedGpio},
#endif
#if 1 //__MSTC__, RaynorChung: Ssupport USB LED   
	{kLedGWAN100M, BpGetGWAN100MLedGpio, NULL},
	{kLedGWAN1000M, BpGetGWAN1000MLedGpio, NULL},
#endif
    {kLedPhone1, BpGetPhone1MWILedGpio, NULL},
    {kLedPhone2, BpGetPhone2MWILedGpio, NULL},
    {kLed5GWps, BpGetWireless5GWpsLedGpio, NULL},
#if 1 /* chchien */
	{kLedWanEth3, BpGetWanEth3LedGpio, NULL},
#endif	    
    {kLedEnd, NULL, NULL}
};

// global variables:
static struct timer_list gLedTimer;
static PLED_CTRL gLedCtrl = NULL;
static int gTimerOn = FALSE;
static int gTimerOnRequests = 0;
static unsigned int gLedRunningCounter = 0;  // only used by WLAN
#if 1 //__MSTC__, Leo, when LED is turned off by auto-scheduling, some LEDs will turn on after reboot. 
static int allLedStatus = 1;
#endif
void (*ethsw_led_control)(unsigned long ledMask, int value) = NULL;
EXPORT_SYMBOL(ethsw_led_control);

/** This spinlock protects all access to gLedCtrl, gTimerOn
 *  gTimerOnRequests, gLedRunningCounter, and ledTimerStart function.
 *  Use spin_lock_irqsave to lock the spinlock because ledTimerStart
 *  may be called from interrupt handler (WLAN?)
 */
static spinlock_t brcm_ledlock;
static void ledTimerExpire(void);

//**************************************************************************************
// LED operations
//**************************************************************************************

// turn led on and set the ledState
static void setLed (PLED_CTRL pLed, unsigned short led_state, unsigned short led_type)
{
    short led_gpio;
    unsigned short gpio_state;
    unsigned long flags;

    if (led_type == kLedRed)
        led_gpio = pLed->ledRedGpio;
    else
        led_gpio = pLed->ledGreenGpio;

    if (led_gpio == -1)
        return;

    if (((led_gpio & BP_ACTIVE_LOW) && (led_state == kLedOn)) || 
        (!(led_gpio & BP_ACTIVE_LOW) && (led_state == kLedOff)))
        gpio_state = 0;
    else
        gpio_state = 1;

    /* spinlock to protect access to GPIODir, GPIOio */
    spin_lock_irqsave(&bcm_gpio_spinlock, flags);

#if defined(CONFIG_BCM96328) || defined(_BCM96318_)
    /* Enable LED controller to drive this GPIO */
    if (!(led_gpio & BP_GPIO_SERIAL))
        GPIO->GPIOMode |= GPIO_NUM_TO_MASK(led_gpio);
#endif

#if defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268)
    /* Enable LED controller to drive this GPIO */
    if (!(led_gpio & BP_GPIO_SERIAL))
        GPIO->LEDCtrl |= GPIO_NUM_TO_MASK(led_gpio);
#endif

#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318)
#if !defined(CONFIG_ZYXEL_VMG1312) //__MSTC__, Eason

	if (led_gpio & BP_GPIO_SERIAL) {
		GPIO->GPIOMode |= (GPIO_MODE_SERIAL_LED_CLK | GPIO_MODE_SERIAL_LED_DATA);
	}


    if ((led_gpio & BP_GPIO_NUM_MASK) < 24) {		
    LED->ledMode &= ~(LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
    if( gpio_state )
        LED->ledMode |= (LED_MODE_OFF << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
    else
        LED->ledMode |= (LED_MODE_ON << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));

    }
    else {
#if defined(CONFIG_ZYXEL_VMG5313)
        short WPS_LED;
        BpGetWirelessSesLedGpio(&WPS_LED);	//WPS LED using GPIO 44, cannot control by LED
        if(led_gpio == WPS_LED)
        {
            GPIO->GPIOCtrl |= GPIO_NUM_TO_MASK(led_gpio - 32);	// this region control by wlan
        }
#endif
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
        if( gpio_state )
            GPIO->GPIOio |= GPIO_NUM_TO_MASK(led_gpio);
        else
            GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(led_gpio);
    }
#else
    LED->ledMode &= ~(LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
    if( gpio_state )
        LED->ledMode |= (LED_MODE_OFF << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
    else
        LED->ledMode |= (LED_MODE_ON << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
#endif
#else
    if (led_gpio & BP_GPIO_SERIAL) {
        while (GPIO->SerialLedCtrl & SER_LED_BUSY);
        if( gpio_state )
            GPIO->SerialLed |= GPIO_NUM_TO_MASK(led_gpio);
        else
            GPIO->SerialLed &= ~GPIO_NUM_TO_MASK(led_gpio);
    }
    else {
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
        if( gpio_state )
            GPIO->GPIOio |= GPIO_NUM_TO_MASK(led_gpio);
        else
            GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(led_gpio);
    }
#endif

    spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
}

// toggle the LED
static void ledToggle(PLED_CTRL pLed)
{
    short led_gpio;
#if defined(CONFIG_ZYXEL_VMG5313)
	short WPS_LED;
#endif
    led_gpio = pLed->ledGreenGpio;

    if (led_gpio == -1)
        return;

#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318)
#if defined(CONFIG_ZYXEL_VMG5313)
	BpGetWirelessSesLedGpio(&WPS_LED);	//WPS LED using GPIO 44, cannot control by LED
	if(led_gpio == WPS_LED)
	{
		GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
		GPIO->GPIOio ^= GPIO_NUM_TO_MASK(led_gpio);
	}else
#endif
    LED->ledMode ^= (LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
#else

    if (led_gpio & BP_GPIO_SERIAL) {
        while (GPIO->SerialLedCtrl & SER_LED_BUSY);
        GPIO->SerialLed ^= GPIO_NUM_TO_MASK(led_gpio);
    }
    else {
        unsigned long flags;

        spin_lock_irqsave(&bcm_gpio_spinlock, flags);
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
        GPIO->GPIOio ^= GPIO_NUM_TO_MASK(led_gpio);
        spin_unlock_irqrestore(&bcm_gpio_spinlock, flags);
    }

#endif
}   

/** Start the LED timer
 *
 * Must be called with brcm_ledlock held
 */
static void ledTimerStart(void)
{
#if defined(DEBUG_LED)
    printk("led: add_timer\n");
#endif

    BCM_ASSERT_HAS_SPINLOCK_C(&brcm_ledlock);

    init_timer(&gLedTimer);
    gLedTimer.function = (void*)ledTimerExpire;
    gLedTimer.expires = jiffies + k125ms;        // timer expires in ~100ms
    add_timer (&gLedTimer);
} 


// led timer expire kicks in about ~100ms and perform the led operation according to the ledState and
// restart the timer according to ledState
static void ledTimerExpire(void)
{
    int i;
    PLED_CTRL pCurLed;
    unsigned long flags;

    BCM_ASSERT_NOT_HAS_SPINLOCK_V(&brcm_ledlock);

    for (i = 0, pCurLed = gLedCtrl; i < kLedEnd; i++, pCurLed++)
    {
#if defined(DEBUG_LED)
        printk("led[%d]: Mask=0x%04x, State = %d, blcd=%d\n", i, pCurLed->ledGreenGpio, pCurLed->ledState, pCurLed->blinkCountDown);
#endif
        spin_lock_irqsave(&brcm_ledlock, flags);        // LEDs can be changed from ISR
        switch (pCurLed->ledState)
        {
        case kLedStateOn:
        case kLedStateOff:
        case kLedStateFail:
            pCurLed->blinkCountDown = 0;            // reset the blink count down
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;
#if 1 /* LED , __TELEFONICA__, Mitrastar Curtis, 20120106 */
        case ALLLedStateOn:
        case ALLLedStateOff:
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;
#endif
        case kLedStateSlowBlinkContinues:
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = kSlowBlinkCount;
                ledToggle(pCurLed);
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;

        case kLedStateFastBlinkContinues:
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = kFastBlinkCount;
                ledToggle(pCurLed);
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;

        case kLedStateUserWpsInProgress:          /* 200ms on, 100ms off */
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = (((gLedRunningCounter++)&1)? kFastBlinkCount: kSlowBlinkCount);
                ledToggle(pCurLed);
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;             

        case kLedStateUserWpsError:               /* 100ms on, 100ms off */
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = kFastBlinkCount;
                ledToggle(pCurLed);
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;        

        case kLedStateUserWpsSessionOverLap:      /* 100ms on, 100ms off, 5 times, off for 500ms */        
            if (pCurLed->blinkCountDown-- == 0)
            {
                if(((++gLedRunningCounter)%10) == 0) {
                    pCurLed->blinkCountDown = 4;
                    setLed(pCurLed, kLedOff, kLedGreen);
                    pCurLed->ledState = kLedStateUserWpsSessionOverLap;
                }
                else
                {
                    pCurLed->blinkCountDown = kFastBlinkCount;
                    ledToggle(pCurLed);
                }
            }
            gTimerOnRequests++;
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            break;        

        default:
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            printk("Invalid state = %d\n", pCurLed->ledState);
        }
    }

    // Restart the timer if any of our previous LED operations required
    // us to restart the timer, or if any other threads requested a timer
    // restart.  Note that throughout this function, gTimerOn == TRUE, so
    // any other thread which want to restart the timer would only
    // increment the gTimerOnRequests and not call ledTimerStart.
    spin_lock_irqsave(&brcm_ledlock, flags);
    if (gTimerOnRequests > 0)
    {
        ledTimerStart();
        gTimerOnRequests = 0;
    }
    else
    {
        gTimerOn = FALSE;
    }
    spin_unlock_irqrestore(&brcm_ledlock, flags);
}

void __init boardLedInit(void)
{
    PBP_LED_INFO pInfo;
    unsigned short i;
    unsigned short gpio;
#if defined(CONFIG_BCM96816)
    ETHERNET_MAC_INFO EnetInfo;
#endif

    spin_lock_init(&brcm_ledlock);

#if !(defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318))
    /* Set blink rate for hardware LEDs. */
    GPIO->LEDCtrl &= ~LED_INTERVAL_SET_MASK;
    GPIO->LEDCtrl |= LED_INTERVAL_SET_80MS;
#endif

    gLedCtrl = (PLED_CTRL) kmalloc((kLedEnd * sizeof(LED_CTRL)), GFP_KERNEL);

    if( gLedCtrl == NULL ) {
        printk( "LED memory allocation error.\n" );
        return;
    }

    /* Initialize LED control */
    for (i = 0; i < kLedEnd; i++) {
        gLedCtrl[i].ledGreenGpio = -1;
        gLedCtrl[i].ledRedGpio = -1;
        gLedCtrl[i].ledState = kLedStateOff;
        gLedCtrl[i].blinkCountDown = 0;            // reset the blink count down
    }

    for( pInfo = bpLedInfo; pInfo->ledName != kLedEnd; pInfo++ ) {
        if( pInfo->bpFunc && (*pInfo->bpFunc) (&gpio) == BP_SUCCESS )
        {
            gLedCtrl[pInfo->ledName].ledGreenGpio = gpio;
        }
        if( pInfo->bpFuncFail && (*pInfo->bpFuncFail)(&gpio)==BP_SUCCESS )
        {
            gLedCtrl[pInfo->ledName].ledRedGpio = gpio;
        }
        setLed(&gLedCtrl[pInfo->ledName], kLedOff, kLedGreen);
        setLed(&gLedCtrl[pInfo->ledName], kLedOff, kLedRed);
    }

#if defined(CONFIG_BCM96816)
    if( BpGetEthernetMacInfo( &EnetInfo, 1 ) == BP_SUCCESS )
    {
        if ( EnetInfo.sw.ledInfo[0].duplexLed != BP_NOT_DEFINED )
        {
            gLedCtrl[kLedEth0Duplex].ledGreenGpio = EnetInfo.sw.ledInfo[0].duplexLed;
            setLed(&gLedCtrl[kLedEth0Duplex], kLedOff, kLedGreen);
        }
        if ( EnetInfo.sw.ledInfo[0].speedLed100 != BP_NOT_DEFINED )
        {
            gLedCtrl[kLedEth0Spd100].ledGreenGpio = EnetInfo.sw.ledInfo[0].speedLed100;
            setLed(&gLedCtrl[kLedEth0Spd100], kLedOff, kLedGreen);
        }
        if ( EnetInfo.sw.ledInfo[0].speedLed1000 != BP_NOT_DEFINED )
        {
            gLedCtrl[kLedEth0Spd1000].ledGreenGpio = EnetInfo.sw.ledInfo[0].speedLed1000;
            setLed(&gLedCtrl[kLedEth0Spd1000], kLedOff, kLedGreen);
        }
     
        if ( EnetInfo.sw.ledInfo[1].duplexLed != BP_NOT_DEFINED )
        {
            gLedCtrl[kLedEth1Duplex].ledGreenGpio = EnetInfo.sw.ledInfo[1].duplexLed;
            setLed(&gLedCtrl[kLedEth1Duplex], kLedOff, kLedGreen);
        }
        if ( EnetInfo.sw.ledInfo[1].speedLed100 != BP_NOT_DEFINED )
        {
            gLedCtrl[kLedEth1Spd100].ledGreenGpio = EnetInfo.sw.ledInfo[1].speedLed100;
            setLed(&gLedCtrl[kLedEth1Spd100], kLedOff, kLedGreen);
        }
        if ( EnetInfo.sw.ledInfo[1].speedLed1000 != BP_NOT_DEFINED )
        {
            gLedCtrl[kLedEth1Spd1000].ledGreenGpio = EnetInfo.sw.ledInfo[1].speedLed1000;
            setLed(&gLedCtrl[kLedEth1Spd1000], kLedOff, kLedGreen);
        }
    }
#endif

#if defined(DEBUG_LED)
    for (i=0; i < kLedEnd; i++)
        printk("initLed: led[%d]: Gpio=0x%04x, FailGpio=0x%04x\n", i, gLedCtrl[i].ledGreenGpio, gLedCtrl[i].ledRedGpio);
#endif
#if !defined(CONFIG_ZYXEL_VMG1312)
    printk("\n\r LED->ledHWDis:%x\n\r", (unsigned int)LED->ledHWDis);
    printk("\n\r LED->ledInit:%x\n\r", (unsigned int)LED->ledInit);
    LED->ledInit &= ~(LED->ledInit & LED_SERIAL_LED_SHIFT_DIR );

    printk("\n\r LED->ledInit:%x\n\r", (unsigned int)LED->ledInit);
    printk("\n\r GPIO->LEDCtrl:%x\n\r", (unsigned int)GPIO->LEDCtrl);
    printk("\n\r LED->ledLinkActSelLow:%x\n\r", (unsigned int)LED->ledLinkActSelLow);
    printk("\n\r LED->ledLinkActSelHigh:%x\n\r", (unsigned int)LED->ledLinkActSelHigh);
    printk("\n\r LED->ledMode:%016llx\n\r", LED->ledMode);
#endif
}

// led ctrl.  Maps the ledName to the corresponding ledInfoPtr and perform the led operation
void boardLedCtrl(BOARD_LED_NAME ledName, BOARD_LED_STATE ledState)
{
    unsigned long flags;
    PLED_CTRL pLed;

    BCM_ASSERT_NOT_HAS_SPINLOCK_V(&brcm_ledlock);

    spin_lock_irqsave(&brcm_ledlock, flags);     // LED can be changed from ISR
#if 1 //__MSTC__, Leo, when LED is turned off by auto-scheduling, some LEDs will turn on after reboot.
	if((ledName==kLedPowerG) && (ledState==kLedStateOff))
		allLedStatus = 0;
	if((ledName==kLedPowerG) && (ledState==kLedStateOn))
		allLedStatus = 1;
#endif

    if( (int) ledName < kLedEnd ) {

        pLed = &gLedCtrl[ledName];

        // If the state is kLedStateFail and there is not a failure LED defined
        // in the board parameters, change the state to kLedStateSlowBlinkContinues.
        if( ledState == kLedStateFail && pLed->ledRedGpio == -1 )
            ledState = kLedStateSlowBlinkContinues;

        // Save current LED state
        pLed->ledState = ledState;

        // Start from LED Off and turn it on later as needed
        setLed (pLed, kLedOff, kLedGreen);
        setLed (pLed, kLedOff, kLedRed);

        // Disable HW control for WAN Data LED. 
        // It will be enabled only if LED state is On
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318)
        if (ledName == kLedWanData)
            LED->ledHWDis |= GPIO_NUM_TO_MASK(pLed->ledGreenGpio);
#elif defined(CONFIG_BCM96368)
        if (ledName == kLedWanData) {
            GPIO->AuxLedCtrl |= AUX_HW_DISAB_2;
            if (pLed->ledGreenGpio & BP_ACTIVE_LOW)
                GPIO->AuxLedCtrl |= (LED_STEADY_ON << AUX_MODE_SHFT_2);
            else
                GPIO->AuxLedCtrl &= ~(LED_STEADY_ON << AUX_MODE_SHFT_2);
        }
#endif
#if 1 //__MSTC__, Leo, when LED is turned off by auto-scheduling, some LEDs will turn on after reboot.
       if(allLedStatus)
	   {
#endif
        switch (ledState) {
        case kLedStateOn:
            // Enable SAR to control INET LED
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318)
            if (ledName == kLedWanData)
                LED->ledHWDis &= ~GPIO_NUM_TO_MASK(pLed->ledGreenGpio);
#elif defined(CONFIG_BCM96368)
            if (ledName == kLedWanData) {
                GPIO->AuxLedCtrl &= ~AUX_HW_DISAB_2;
                if (pLed->ledGreenGpio & BP_ACTIVE_LOW)
                    GPIO->AuxLedCtrl &= ~(LED_STEADY_ON << AUX_MODE_SHFT_2);
                else
                    GPIO->AuxLedCtrl |= (LED_STEADY_ON << AUX_MODE_SHFT_2);
            }
#endif
            setLed (pLed, kLedOn, kLedGreen);
            break;

        case kLedStateOff:
            break;
#if 1 /* LED , __TELEFONICA__, Mitrastar Curtis, 20120106 */
        case ALLLedStateOn:                
            setLed(pLed, kLedOn, kLedGreen);
            setLed(pLed, kLedOn, kLedRed);
            break;
        case ALLLedStateOff:
            setLed(pLed, kLedOff, kLedGreen);
            setLed(pLed, kLedOff, kLedRed);
            break;
#endif
        case kLedStateFail:
            setLed (pLed, kLedOn, kLedRed);
            break;

        case kLedStateFailOff:
            setLed (pLed, kLedOff, kLedRed);
            break;

        case kLedStateSlowBlinkContinues:
            pLed->blinkCountDown = kSlowBlinkCount;
            gTimerOnRequests++;
            break;

        case kLedStateFastBlinkContinues:
            pLed->blinkCountDown = kFastBlinkCount;
            gTimerOnRequests++;
            break;

        case kLedStateUserWpsInProgress:          /* 200ms on, 100ms off */
            pLed->blinkCountDown = kFastBlinkCount;
            gLedRunningCounter = 0;
            gTimerOnRequests++;
            break;             

        case kLedStateUserWpsError:               /* 100ms on, 100ms off */
            pLed->blinkCountDown = kFastBlinkCount;
            gLedRunningCounter = 0;
            gTimerOnRequests++;
            break;        

        case kLedStateUserWpsSessionOverLap:      /* 100ms on, 100ms off, 5 times, off for 500ms */
            pLed->blinkCountDown = kFastBlinkCount;
            gTimerOnRequests++;
            break;        

        default:
            printk("Invalid led state\n");
        }
#if 1 //__MSTC__, Leo, when LED is turned off by auto-scheduling, some LEDs will turn on after reboot.
       }
#endif
    }

    // If gTimerOn is false, that means ledTimerExpire has already finished
    // running and has not restarted the timer.  Then we can start it here.
    // Otherwise, we only leave the gTimerOnRequests > 0 which will be
    // noticed at the end of the ledTimerExpire function.
    if (!gTimerOn && gTimerOnRequests > 0)
    {
        ledTimerStart();
        gTimerOn = TRUE;
        gTimerOnRequests = 0;
    }
    spin_unlock_irqrestore(&brcm_ledlock, flags);
}
