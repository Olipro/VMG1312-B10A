/* Copyright (C) 2007 Broadcom Corporation */

/*Led Blink type*/
typedef enum ses_blinktype {
        SES_BLINKTYPE_RWO = 0,
        SES_BLINKTYPE_REGOK,
} ses_blinktype_t;


/*Define the WSC SM state const. This definition should be consistence*/
#define WSC_PROC_IDLE             0
#define WSC_PROC_WAITING      1
#define WSC_PROC_SUCC            2
#define WSC_PROC_TIMEOUT      3
#define WSC_PROC_FAIL             4
#define WSC_PROC_M2_SENT      5
#define WSC_PROC_M7_SENT      6
#define WSC_PROC_MSG_DONE   7
#define WSC_PROC_PBC_OVERLAP             8

/*Event const definition: */
#define WSC_EVENTS_UNDEFINE              0
#define WSC_EVENTS_BTN_PRESSED        1

/*WPS SM State*/
#define WSC_EVENTS_PROC_START		   2
#define WSC_EVENTS_PROC_IDLE             		(WSC_PROC_IDLE + WSC_EVENTS_PROC_START)
#define WSC_EVENTS_PROC_WAITING      		(WSC_PROC_WAITING + WSC_EVENTS_PROC_START)
#define WSC_EVENTS_PROC_SUCC            		(WSC_PROC_SUCC  + WSC_EVENTS_PROC_START)
#define WSC_EVENTS_PROC_TIMEOUT      		(WSC_PROC_TIMEOUT + WSC_EVENTS_PROC_START)
#define WSC_EVENTS_PROC_FAIL             		(WSC_PROC_FAIL + WSC_EVENTS_PROC_START)
#define WSC_EVENTS_PROC_M2_SENT      		(WSC_PROC_M2_SENT + WSC_EVENTS_PROC_START)
#define WSC_EVENTS_PROC_M7_SENT      		(WSC_PROC_M7_SENT + WSC_EVENTS_PROC_START)
#define WSC_EVENTS_PROC_MSG_DONE   		(WSC_PROC_MSG_DONE + WSC_EVENTS_PROC_START)
#define WSC_EVENTS_PROC_PBC_OVERLAP         (WSC_PROC_PBC_OVERLAP + WSC_EVENTS_PROC_START)

/*Led Blink*/
#define WSC_LED_OFF             0
#define WSC_LED_ON               1
#define WSC_LED_BLINK          2


#define WSC_LED_OFFSET       0
#define WSC_LED_MASK           0x03L

#define WSC_STATUS_OFFSET   8
#define WSC_STATUS_MASK      0x0000ff00L

#define WSC_EVENT_OFFSET    16
#define WSC_EVENT_MASK       0x00ff0000L

#define WSC_BLINK_OFFSET     24
#define WSC_BLINK_MASK        0xff000000L



/*Global Functions*/

/*initlization function */
int wscLedInit(void);

/* De-Initialization functuion*/
void wscLedDeInit (void);

/* OSL APIs, implement in wps_monitor_xxx */
int wps_osl_led_blink_init(void (*blink_timer)(int));
void wps_osl_led_blink_cleanup();

