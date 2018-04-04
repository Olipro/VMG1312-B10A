/* PPPoE support library "libpppoe"
 *
 * Copyright 2000 Michal Ostrowski <mostrows@styx.uwaterloo.ca>,
 *		  Jamal Hadi Salim <hadi@cyberus.ca>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#include "pppoe.h"
#ifndef BRCM_CMS_BUILD
#include <syscall.h>
#endif

// patch2
int ses_retries=8;

#if 1 //__MSTC__, Jeff, support vlanautohunt
extern int vlanautohunt;
#endif

static int std_rcv_pado(struct session* ses,
			struct pppoe_packet *p_in,
			struct pppoe_packet **p_out){
    
    if( verify_packet(ses, p_in) < 0)
	return -2;
    
    if(ses->state != PADO_CODE ){
	poe_error(ses,"Unexpected packet: %P",p_in);
	return 0;
    }
    
#if 1 //__MSTC__, Jeff
	if(vlanautohunt == 1)
	{
		printf("VLANAUTOHUNT: pppoe detected\n");
		sendMsgToVAH(CMS_MSG_VLANAUTOHUNT_HUNTED);
		kill(getpid(), SIGTERM);
		sleep(1000);
	}
#endif
    
    if (DEB_DISC2) {
	poe_dbglog (ses,"PADO received: %P", p_in);
    }
#ifdef MSTC_LOG //__MSTC__, TengChang, Log
	zyLog_app(LOG_LEVEL_NOTICE, LOG_FAC_PPPOE, "PADO received");
#endif //__MSTC__, TengChang, Log

    // brcm: add code to get service name and put it in the /var/fyi/wan/servicename file
    if (p_in->tags[0]->tag_type == PTT_SRV_NAME)
    {
    char sName[255]="";
//	char path[64]="";
//	char cmd[320]="";
    memset(sName, 0, p_in->tags[0]->tag_len+1);
    strncpy(sName, p_in->tags[0]->tag_data, p_in->tags[0]->tag_len);
#ifdef BRCM_CMS_BUILD
extern char servicename[BUFLEN_264]; /* service name from the connection,  defined in options.c */
    cmsLog_debug("servicename=%s", sName);
    strncpy(servicename,  sName, sizeof(servicename));
#else
    //printf("PPPoE Service Name: %s\n", sName);
	sprintf(path, "%s/%s/%s", "/proc/var/fyi/wan", session_path, "servicename");
	sprintf(cmd, "echo %s > %s", sName, path); 
	system(cmd);
#endif	
    }
    memcpy(&ses->remote, &p_in->addr, sizeof(struct sockaddr_ll));
    memcpy( &ses->curr_pkt.addr, &ses->remote , sizeof(struct sockaddr_ll));
           
    ses->curr_pkt.hdr->code = PADR_CODE;
    
    /* The HOST_UNIQ has been verified already... there's no "if" about this */
    /* if(ses->filt->htag) */
    copy_tag(&ses->curr_pkt,get_tag(p_in->hdr,PTT_HOST_UNIQ));	
    
    if (ses->filt->ntag) {
    	ses->curr_pkt.tags[TAG_AC_NAME]=NULL;
    }
//    copy_tag(&ses->curr_pkt,get_tag(p_in->hdr,PTT_AC_NAME));
    
#if 1 //brcm
    /* Our service name has been verified against the service name tags offered by
     * the server in the call to verify_packet().  We can just use it.
     */
    copy_tag(&ses->curr_pkt, ses->filt->stag);
#else
    if(ses->filt->stag) {
    	ses->curr_pkt.tags[TAG_SRV_NAME]=NULL;
    }
    copy_tag(&ses->curr_pkt,get_tag(p_in->hdr,PTT_SRV_NAME));
#endif    
    
    copy_tag(&ses->curr_pkt,get_tag(p_in->hdr,PTT_AC_COOKIE));
    copy_tag(&ses->curr_pkt,get_tag(p_in->hdr,PTT_RELAY_SID));
    
    ses->state = PADS_CODE;
    
    create_msg(BCM_PPPOE_CLIENT_STATE_PADS, MDMVS_ERROR_NONE);    
    syslog(LOG_CRIT,"PPP server detected.\n");
    
    ses->retransmits = 0;
    
    send_disc(ses, &ses->curr_pkt);
#ifdef MSTC_LOG //__MSTC__, TengChang, Log
    zyLog_app(LOG_LEVEL_NOTICE, LOG_FAC_PPPOE, "Send PADR");
#endif //__MSTC__, TengChang, Log
    (*p_out) = &ses->curr_pkt;

    if (ses->np)
	return 1;
    
    return 0;
}

static int std_init_disc(struct session* ses,
			 struct pppoe_packet *p_in,
			 struct pppoe_packet **p_out){
    
    memset(&ses->curr_pkt,0, sizeof(struct pppoe_packet));

    
    /* Check if already connected */
    if( ses->state != PADO_CODE ){
	return 1;
    }
    
    ses->curr_pkt.hdr = (struct pppoe_hdr*) ses->curr_pkt.buf;
    ses->curr_pkt.hdr->ver  = 1;
    ses->curr_pkt.hdr->type = 1;
    ses->curr_pkt.hdr->code = PADI_CODE;
    
    
    memcpy( &ses->curr_pkt.addr, &ses->remote , sizeof(struct sockaddr_ll));
    
    poe_info (ses,"Sending PADI");
#ifdef MSTC_LOG //__MSTC__, TengChang, Log
    zyLog_app(LOG_LEVEL_NOTICE, LOG_FAC_PPPOE, "Send PADI");
#endif //__MSTC__, TengChang, Log
    if (DEB_DISC)
	poe_dbglog (ses,"Sending PADI");
    
    ses->retransmits = 0 ;
    
    if(ses->filt->ntag) {
	ses->curr_pkt.tags[TAG_AC_NAME]=ses->filt->ntag;
	poe_info(ses,"overriding AC name\n");
    }
    
    if(ses->filt->stag)
	ses->curr_pkt.tags[TAG_SRV_NAME]=ses->filt->stag;
    
    if(ses->filt->htag)
	ses->curr_pkt.tags[TAG_HOST_UNIQ]=ses->filt->htag;
    
    send_disc(ses, &ses->curr_pkt);
    (*p_out)= &ses->curr_pkt;
    return 0;
}


static int std_rcv_pads(struct session* ses,
			struct pppoe_packet *p_in,
			struct pppoe_packet **p_out){
    if( verify_packet(ses, p_in) < 0)
	return -2;
    
    if (DEB_DISC)
	poe_dbglog (ses,"Got connection: %x",
		    ntohs(p_in->hdr->sid));
    poe_info (ses,"Got connection: %x", ntohs(p_in->hdr->sid));
#ifdef MSTC_LOG //__MSTC__, TengChang, Log
    zyLog_app(LOG_LEVEL_NOTICE, LOG_FAC_PPPOE, "Receive PADS");
#endif //__MSTC__, TengChang, Log

    ses->sp.sa_family = AF_PPPOX;
    ses->sp.sa_protocol = PX_PROTO_OE;
    ses->sp.sa_addr.pppoe.sid = p_in->hdr->sid;
    memcpy(ses->sp.sa_addr.pppoe.dev,ses->name, IFNAMSIZ);
    memcpy(ses->sp.sa_addr.pppoe.remote, p_in->addr.sll_addr, ETH_ALEN);

    create_msg(BCM_PPPOE_CLIENT_STATE_CONFIRMED, MDMVS_ERROR_NONE);
    syslog(LOG_CRIT,"PPP session established.\n");
    // patch2
    save_session_info(ses->sp.sa_addr.pppoe.remote, ses->sp.sa_addr.pppoe.sid);
    
    return 1;
}

static int std_rcv_padt(struct session* ses,
			struct pppoe_packet *p_in,
			struct pppoe_packet **p_out){
    ses->state = PADO_CODE;
    
    create_msg(BCM_PPPOE_CLIENT_STATE_PADO, MDMVS_ERROR_NONE);
    syslog(LOG_CRIT,"PPP session terminated.\n");
#ifdef MSTC_LOG //__MSTC__, TengChang, Log
    zyLog_app(LOG_LEVEL_NOTICE, LOG_FAC_PPPOE, "Receive PADT");
#endif //__MSTC__, TengChang, Log
    
    return 0;
}


extern int disc_sock;
int client_init_ses (struct session *ses, char* devnam)
{
    int i=0;
    int retval;
    char dev[IFNAMSIZ+1];
    int addr[ETH_ALEN];
    int sid;
    
    /* do error checks here; session name etc are valid */
//    poe_info (ses,"init_ses: creating socket");
    
    /* Make socket if necessary */
    if( disc_sock < 0 ){
	
	disc_sock = socket(PF_PACKET, SOCK_DGRAM, 0);
	if( disc_sock < 0 ){
	    poe_fatal(ses,
		      "Cannot create PF_PACKET socket for PPPoE discovery\n");
	}
	
    }
    
    /* Check for long format */
    retval =sscanf(devnam, FMTSTRING(IFNAMSIZ),addr, addr+1, addr+2,
		   addr+3, addr+4, addr+5,&sid,dev);
    if( retval != 8 ){
	/* Verify the device name , construct ses->local */
	retval = get_sockaddr_ll(devnam,&ses->local);
	if (retval < 0)
	    poe_fatal(ses, "client_init_ses: "
		      "Cannot create PF_PACKET socket for PPPoE discovery\n");
	
	
	ses->state = PADO_CODE;

        create_msg(BCM_PPPOE_CLIENT_STATE_PADO, MDMVS_ERROR_NONE);

	memcpy(&ses->remote, &ses->local, sizeof(struct sockaddr_ll) );
	
	memset( ses->remote.sll_addr, 0xff, ETH_ALEN);
    }else{
	/* long form parsed */

	/* Verify the device name , construct ses->local */
	retval = get_sockaddr_ll(dev,&ses->local);
	if (retval < 0)
	    poe_fatal(ses,"client_init_ses(2): "
		      "Cannot create PF_PACKET socket for PPPoE discovery\n");
	ses->state = PADS_CODE;
	ses->sp.sa_family = AF_PPPOX;
	ses->sp.sa_protocol = PX_PROTO_OE;
	ses->sp.sa_addr.pppoe.sid = sid;

        create_msg(BCM_PPPOE_CLIENT_STATE_PADS, MDMVS_ERROR_NONE);

	memcpy(&ses->remote, &ses->local, sizeof(struct sockaddr_ll) );
	
	for(; i < ETH_ALEN ; ++i ){
	    ses->sp.sa_addr.pppoe.remote[i] = addr[i];
	    ses->remote.sll_addr[i]=addr[i];
	}
	memcpy(ses->sp.sa_addr.pppoe.dev, dev, IFNAMSIZ);
	
	
	
    }
    if( retval < 0 )
	error("bad device name: %s",devnam);
    
    
    retval = bind( disc_sock ,
		   (struct sockaddr*)&ses->local,
		   sizeof(struct sockaddr_ll));
    
    
    if( retval < 0 ){
	error("bind to PF_PACKET socket failed: %m");
    }
    
	// brcm
	if (ses->fd > 0)
		close(ses->fd);

    ses->fd = socket(AF_PPPOX,SOCK_STREAM,PX_PROTO_OE);
    if(ses->fd < 0)
    {
	poe_fatal(ses,"Failed to create PPPoE socket: %m");
    }
    
    
    ses->init_disc = std_init_disc;
    ses->rcv_pado  = std_rcv_pado;
    ses->rcv_pads  = std_rcv_pads;
    ses->rcv_padt  = std_rcv_padt;
    
    /* this should be filter overridable */
    ses->retries = ses_retries;

    return ses->fd;
}

