/* Used by ebt_policer.c, ZyXEL Stan, 20100107*/
#ifndef __LINUX_BRIDGE_EBT_POLICER_H
#define __LINUX_BRIDGE_EBT_POLICER_H

#define EBT_POLICER_MATCH "policer"  

#define BITS_PER_BYTE 8 
#define KILO_SCALE    1000 

struct ebt_policer_info
{
#if 1//__MSTC__, Jones For compilation
       int policerMode;

	/* For srTCM and trTCM, rate means cRate and burst means cbsBurst.
       For srTCM, pbsBurst means ebsBurst. */
       u_int32_t rate, pRate;    
	u_int32_t burst, pbsBurst;  /* Period multiplier for upper limit. */

       /* Used internally by the kernel */
       unsigned long prev;

	/* For srTCM and trTCM, credit means cbsCredit and creditCap means cbsCreditCap.
	   For srTCM, pbsCreditCap means ebsCreditCap. */
	u_int32_t credit, pbsCredit;
	u_int32_t creditCap, pbsCreditCap;
#else
	u_int32_t avg;    /* Average secs between packets * scale */
	u_int32_t burst;  /* Period multiplier for upper limit. */

	/* Used internally by the kernel */
	unsigned long prev;
	u_int32_t credit;
	u_int32_t credit_cap, cost;
#endif	
};

#endif

