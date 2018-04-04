#ifndef _XT_RATE_H
#define _XT_RATE_H

struct xt_policer_info {
#if 1//__MSTC__, Jones For compilation
    int policerMode;    
    /* For srTCM and trTCM, rate means cRate and burst means cbsBurst.       
    For srTCM, pbsBurst means ebsBurst. */    
    u_int32_t rate, pRate;        
    u_int32_t burst, pbsBurst;  
    /* Period multiplier for upper limit. */  
#ifdef KERNEL_64_USERSPACE_32    
    u_int64_t prev;    
    u_int64_t placeholder;
#else    
    /* Used internally by the kernel */    
    unsigned long prev;    
    struct xt_policer_info *master;
#endif    
    /* For srTCM and trTCM, credit means cbsCredit and creditCap means cbsCreditCap.	   
    For srTCM, pbsCreditCap means ebsCreditCap. */    
    u_int32_t credit, pbsCredit;    
    u_int32_t creditCap, pbsCreditCap;
#else
    u_int32_t avg;    
    u_int32_t burst;  

#ifdef KERNEL_64_USERSPACE_32
    u_int64_t prev;
    u_int64_t placeholder;
#else
	/* Used internally by the kernel */
    unsigned long prev;
    struct xt_policer_info *master;
#endif
    u_int32_t credit;
    u_int32_t credit_cap, cost;
#endif	
};
#endif 

