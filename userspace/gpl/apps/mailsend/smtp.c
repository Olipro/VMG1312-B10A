/*
**  SMTP routines for mailsend - a simple mail sender via SMTP
**
**  Limitations and Comments:
**    SMTP RF[C] RFC-821
**    Also look at: TCP/IP Illustraged Vol 1 by Richard Stevens
**
**    Written mainly for NT, Unix has this kind of tools in hundreds.
**
**  Development History:
**      who                  when           why
**      muquit@muquit.com    Mar-26-2001    first cut
*/

#include "mailsend.h"

static char buf[BUFSIZ];
static int  break_out=0;

static char smtp_line[BUFSIZ];
static char smtp_errbuf[BUFSIZ];
static int  smtp_code;
static int  smtp_sep;
static int  s_esmtp=0;

#ifdef WINNT
static BOOL 
    WINAPI CntrlHandler(DWORD CtrlEvent);
#else
#define closesocket close
#endif /* WINNT */

void smtpDisconnect(void)
{
    msock_close();
}
/* connect to SMTP server and returns the socket fd */
static SOCKET smtpConnect(char *smtp_server,int port)
{
    SOCKET
        sfd;

    sfd=clientSocket(smtp_server,port);
    if (sfd == INVALID_SOCKET)
    {
        errorMsg("Could not connect to SMTP server \"%s\" at port %d",
                smtp_server,port);
        return (INVALID_SOCKET);
    }

    /* set the socket to msock lib's static place, not thread safe*/
    msock_set_socket(sfd);

    return (sfd);
}

/*
 * sfd   socket 
 *
 * return 0 on success, -1 on failure
 *
 * populates globals: smtp_code,smtp_line and smtp_errbuf on error
 *
 */
int read_smtp_line(void)
{
    int
        rc=(-1),
        n;

    char
        tbuf[BUFSIZ],
        lbuf[BUFSIZ];

    memset(smtp_line,0,sizeof(smtp_line));
    memset(smtp_errbuf,0,sizeof(smtp_errbuf));
    smtp_code=(-1);

    memset(lbuf,0,sizeof(lbuf));
    /* read a line */
    n=msock_gets(lbuf,sizeof(lbuf)-1);
    if (n < 3 )
    {
        //errorMsg("Error reading SMTP line, read %d bytes",n);
        return(-1);
    }
    showVerbose("[S] %s\n",lbuf);
    if (n >= 5)
    {
        memset(tbuf,0,sizeof(tbuf));
        memcpy(tbuf,lbuf,3);
        smtp_code=atoi(tbuf);

        smtp_sep=lbuf[3];

        (void) snprintf(smtp_line,sizeof(smtp_line)-1,"%s",lbuf + 4);
        rc=0;
/*
        (void) fprintf(stderr," Line: \"%s\"\n",lbuf);
        (void) fprintf(stderr," Code: \"%d\"\n",smtp_code);
        (void) fprintf(stderr," Sep: '%c'\n",smtp_sep);
        (void) fprintf(stderr," line: \"%s\"\n",smtp_line);
*/
    }
    else
    {
        (void) snprintf(smtp_errbuf,sizeof(smtp_errbuf)-1,"%s",lbuf);
    }
    

    return(rc);
}

int smtp_start_tls(int sfd)
{
    int
        rc=(-1);
#ifdef HAVE_OPENSSL
    SSL
        *ssl=NULL;
#endif /* HAVE_OPENSSL */
    memset(buf,0,sizeof(buf));
    (void) snprintf(buf,sizeof(buf)-1,"%s\r\n","STARTTLS");
    showVerbose("[C] %s",buf);
    msock_puts(buf);
    rc=read_smtp_line();
    if (smtp_code != 220)
    {
        errorMsg("Unknown STARTTLS response code %d\n",smtp_code);
        return(-1);
    }
#ifdef HAVE_OPENSSL
    ssl=msock_get_ssl();
    if (ssl)
    {
        if (!SSL_set_fd(ssl,sfd))
        {
            errorMsg("failed to set socket to SSL\n");
            return(-1);
        }
        /* must set back to msock's static */
        msock_set_ssl(ssl);
        rc=SSL_connect(ssl);
        if (rc < 1)
        {
            errorMsg("SSL connection failed\n");
            ERR_print_errors_fp(stderr);
            return(-1);
        }
        print_cert_info(ssl);
        /* tell msock everything is ssl after that */
        msock_turn_ssl_on();
        rc=0;
    }
    else
    {
        errorMsg("Could not start STARTTLS, SSL not initialized properly");
        rc=(-1);
    }
#else
    errorMsg("Not Compiled with OpenSSL, could not do STARTTLS");
#endif /*HAVE_OPENSSL */
    return(rc);
}

/* SMTP: HELO */
static int say_helo(char *helo_domain)
{
    int
        cnt=0,
        rc;

    (void) snprintf(buf,sizeof(buf)-1,"%s %s\r\n",
                    s_esmtp ? "EHLO" : "HELO",helo_domain);
    showVerbose("[C] %s",buf);

    /* send */
    msock_puts(buf);
    rc=read_smtp_line();
    if (smtp_code != 250)
    {
        errorMsg("%s failed", s_esmtp ? "EHLO" : "HELO");
        return(-1);
    }
    /* read all the capabilities if separator is - */
    if (smtp_sep == A_DASH)
    {
        for (;;)
        {
            rc=read_smtp_line();
            if (rc == 0)
                add_server_cap_to_list(smtp_line);
            cnt++;
            if (cnt >= 1000)
                break;
            if (rc < 0 || smtp_sep != A_DASH)
                break;
        }
    }

    return(rc);
}


/* SMTP: MAIL FROM */
static int smtp_MAIL_FROM(char *from)
{
    memset(buf,0,sizeof(buf));
    (void) snprintf(buf,sizeof(buf)-1,"MAIL FROM: <%s>\r\n",from);
    showVerbose("[C] %s",buf);

    msock_puts(buf);
    read_smtp_line();
    if (smtp_code != 250)
    {
        errorMsg("MAIL FROM failed: '%d %s'",smtp_code,smtp_line);
        return(-1);
    }
    return(0);
}

/* SMTP: quit */
static int smtp_QUIT(void)
{
    int
        rc;
    showVerbose("[C] QUIT\r\n");
    msock_puts("QUIT\r\n");
    rc=read_smtp_line();
    /*
    ** google does not seem to write anything back in response to QUIT
    ** command. I'll ignore it anyway
    */
    return(0);
}

/* SMTP: RSET */
/* aborts current mail transaction and cause both ends to reset */
static int smtp_RSET()
{
    msock_puts("RSET\r\n");
    return(read_smtp_line());
}



/* SMTP: RCPT TO */
static int smtp_RCPT_TO(void)
{
    Sll
        *l,
        *al;

    Address
        *a;

    char
        *x;

    int
        rc;

    al=getAddressList();
    
    for (l=al; l; l=l->next)
    {
        a=(Address *) l->data;
        if (! a)
            return(-1);
        if (! a->address)
            return(-1);

        memset(buf,0,sizeof(buf));
        x=getenv("NOTIFY_RCPT");
        if (x != NULL)
        {
            /* MS Exchange has it */
            showVerbose("NOTIFY_RCPT=%s\n",x);
            (void) snprintf(buf,sizeof(buf)-1,"RCPT TO: %s %s\r\n",
                            a->address,x);
        }
        else
        {
            (void) snprintf(buf,sizeof(buf)-1,"RCPT TO: <%s>\r\n",a->address);
        }

        showVerbose("[C] %s",buf);
        
        msock_puts(buf);
        rc=read_smtp_line();
        if (rc == 0)
        {
            if (smtp_code != 250)
            {
                errorMsg("RCPT TO: <%s> failed '%d:%s'\n",
                        a->address,smtp_code,smtp_line);
                smtp_RSET();
                return(-1);
            }
        }
    }
    return (0);

}

/* SMTP: DATA */
static int smtp_DATA(void)
{
    int
        rc;

    msock_puts("DATA\r\n");
    showVerbose("[C] DATA\r\n");

    rc=read_smtp_line();
    if (rc == 0)
    {
        if (smtp_code != 354)
        {
            errorMsg("DATA failed: '%d %s'\n",smtp_code,smtp_line);
            return(-1);
        }
    }
    return(0);
}

/* SMTP: EOM */
int smtpEom(int sfd)
{
    msock_puts("\r\n.\r\n");

    showVerbose("\r\n[C] .\r\n");

    return (read_smtp_line());
}

void doCleanup(void)
{
    smtpDisconnect();
}

#ifdef WINNT
/*
** Handle Ctrl+C 
*/
static BOOL WINAPI CntrlHandler(DWORD CtrlEvent)
{
    break_out=0;

    switch (CtrlEvent)
    {
        case CTRL_C_EVENT:
        {
            break_out=1;
            (void) fprintf(stderr,"\nNot sending mail. Exiting.......\n");
            exit(1); /* XXXXXXXXXXXXXXXXXXXXXX */
            break;
        }
    }

    return (TRUE);
}
#endif /* WINNT */

/* SMTP: mail */
static int smtpMail(int sfd,char *to,char *cc,char *bcc,char *from,char *rrr,char *rt,
                    char *subject,char *attach_file,char *msg_body_file,
                    char *the_msg,int is_mime,int add_dateh)
{
    char
        *os="Unix",
        boundary[17],
        mbuf[1000];

    int
        newline_before;

    Sll
        *attachment_list;

#ifdef WINNT
    os="Windows";
#else
    os="Unix";
#endif /* WINNT */

    attachment_list=get_attachment_list();
    if (attachment_list)
    {
        is_mime=1;
    }

    if (subject)
    {
        memset(buf,0,sizeof(buf));
        (void) snprintf(buf,sizeof(buf)-1,"Subject: %s\r\n",subject);

        msock_puts(buf);

        showVerbose(buf);
    }

    /* headers */
    if (from)
    {
        memset(buf,0,sizeof(buf));
        if (*g_from_name != '\0')
        {
            /* Name in From: */

            memset(buf,0,sizeof(buf));
            (void) snprintf(buf,sizeof(buf)-1,"From: %s <%s>\r\n",
                            g_from_name,from);
        }
        else
        {
            (void) snprintf(buf,sizeof(buf)-1,"From: %s\r\n",from);
        }
        msock_puts(buf);

        showVerbose(buf);
    }

    if (add_dateh)
    {
        /* add Date: header */
        char
            datebuf[65];

        memset(datebuf,0,sizeof(datebuf));
        if (rfc822_date(time(NULL),datebuf,sizeof(datebuf)-1) == 0)
        {
            memset(buf,0,sizeof(buf));
            (void) snprintf(buf,sizeof(buf)-1,"Date: %s\r\n",datebuf);
            msock_puts(buf);

            showVerbose(buf);
        }
    }
    
    if (to)
    {
        memset(buf,0,sizeof(buf));
        (void) snprintf(buf,sizeof(buf)-1,"To: %s\r\n",to);
        msock_puts(buf);

        showVerbose(buf);

    }

    if (cc)
    {
        memset(buf,0,sizeof(buf));
        (void) snprintf(buf,sizeof(buf)-1,"Cc: %s\r\n",cc);
        msock_puts(buf);
        showVerbose(buf);
    }

    /*
    if (bcc)
    {
        memset(buf,0,sizeof(buf));
        (void) snprintf(buf,sizeof(buf)-1,"Bcc: %s\r\n",bcc);
        msock_puts(buf);

        showVerbose(buf);
    }
    */

    if (rt != NULL)
    {
        memset(buf,0,sizeof(buf));
        (void) snprintf(buf,sizeof(buf)-1,"Reply-To: %s\r\n",rt);
        msock_puts(buf);
        showVerbose(buf);

    }
    if (rrr != NULL)
    {
        memset(buf,0,sizeof(buf));
        (void) snprintf(buf,sizeof(buf)-1,"Disposition-Notification-To: %s\r\n",rrr);
        msock_puts(buf);
        showVerbose(buf);
    }

    memset(buf,0,sizeof(buf));
    (void) snprintf(buf,sizeof(buf)-1,"X-Mailer: %s (%s)\r\n",MAILSEND_VERSION,os);
    msock_puts(buf);
    showVerbose(buf);

    memset(buf,0,sizeof(buf));
    (void) snprintf(buf,sizeof(buf)-1,"X-Copyright: %s\r\n",NO_SPAM_STATEMENT);
    msock_puts(buf);
    showVerbose(buf);

    /*
    if (is_mime && msg_file)
    */
    if (is_mime)
    {
        srand(time(NULL));
        memset(boundary,0,sizeof(boundary));
        mutilsGenerateMIMEBoundary(boundary,sizeof(boundary));
        (void) snprintf(buf,sizeof(buf)-1,"Content-type: multipart/mixed; boundary=\"%s\"\r\n",boundary);
        msock_puts(buf);
        showVerbose(buf);

        (void) snprintf(buf,sizeof(buf)-1,"Mime-version: 1.0\r\n");
        msock_puts(buf);
        showVerbose(buf);
    }

    msock_puts("\r\n");
    showVerbose("\r\n");

    /*
    if (is_mime && msg_file)
    */
    if (is_mime)
    {
        char
            mime_tmpfile[BUFSIZ];

        FILE
            *tfp=NULL,
            *fp=NULL;
        int
            tfwd;

        /*
        ** If there a txt file or a one line messgae, attach them first
        */
        /* Part added by Smeeta Jalan -- starts */
        if (the_msg)
        {
            (void) snprintf(buf,sizeof(buf)-1,"\r\n--%s\r\n",boundary);
            msock_puts(buf);
            showVerbose(buf);

            (void) snprintf(buf,sizeof(buf)-1,"Content-Type: text/plain; charset=%s\r\n",g_charset);
            msock_puts(buf);
            showVerbose(buf);

            (void) strcpy(buf,"Content-Disposition: inline\r\n");
            msock_puts(buf);
            showVerbose(buf);

            msock_puts("\r\n");
            showVerbose("\r\n");

            /* (void) strcpy(buf,the_msg); */
            mutilsSafeStrcpy(buf,the_msg,sizeof(buf)-1);
            msock_puts(buf);
            showVerbose("[C] %s",the_msg); 

            (void) snprintf(buf,sizeof(buf)-1,"\r\n\r\n");
            msock_puts(buf);
            showVerbose(buf);
        }

        if (msg_body_file)
        {  
            int
                rc;
            char
                mime_type[32],
                filename[1024];

            FILE
                *fp;

            rc=get_filepath_mimetype(msg_body_file,filename,
                    sizeof(filename)-1,mime_type,sizeof(mime_type)-1);
           /*
            * The file should not have binary characters it it.
            * It's user's responsibilty to make sure file is not binary.
            * If file is binary mail will have garbage as I'll not convert
            * the content to base64
            */
            fp=fopen(filename,"r");
            if (fp == (FILE *) NULL)
            {
                errorMsg("Could not open message body file: %s",filename);
                return (-1);
            } 

            (void) snprintf(buf,sizeof(buf)-1,"\r\n--%s\r\n",boundary);
            msock_puts(buf);
            showVerbose(buf);

            (void) snprintf(buf,sizeof(buf)-1,"Content-Type: %s; charset=%s\r\n",mime_type,g_charset);
            msock_puts(buf);
            showVerbose(buf);

            (void) strcpy(buf,"Content-Disposition: inline\r\n");
            msock_puts(buf);
            showVerbose(buf);

            msock_puts("\r\n");
            showVerbose("\r\n");

            while (fgets(mbuf,sizeof(mbuf)-1,fp))
            {
                msock_puts(mbuf);
                showVerbose("[C] %s",mbuf); 
            }
            (void) fclose(fp);

            (void) snprintf(buf,sizeof(buf)-1,"\r\n\r\n");
            msock_puts(buf);
            showVerbose(buf);
        }
        /* Part added by Smeeta Jalan --ends --*/

        /* open a tmp file writing MIME content */
        (void) strcpy(mime_tmpfile,"./mailsendXXXXXX");

        /* handle mime */
        {
            Attachment
                *a;
            Sll
                *al;

            for (al=attachment_list; al; al=al->next)
            {
                a=(Attachment *) al->data;
                if (a == NULL)
                    continue;

                /* open the mime input file */
                fp=fopen(a->file_path,"rb");
                if (fp == (FILE *) NULL)
                {
                    errorMsg("%s (%d) - Could not open file for reading: %s (%s)",MFL,a->file_path,ERR_STR);
                    return (-1);
                }
#if 1 //__MSTC__, Jeff
				(void) strcpy(mime_tmpfile,"/var/mailsendXXXXXX");
#else
                (void) strcpy(mime_tmpfile,"./mailsendXXXXXX");
#endif

#ifdef HAVE_MKSTEMP
                tfwd=mkstemp(mime_tmpfile);
                if (tfwd == -1)
                {
                    errorMsg("%s (%d) - Could not open tmp file \"%s\" for writing (for %s)\n",MFL,mime_tmpfile,a->file_name);
                    return (-1);
                }
                tfp=fdopen(tfwd,"w");
                if (tfp == NULL)
                {
                    (void) fprintf(stderr,"fdopen() failed on %s\n",mime_tmpfile);
                    close(tfwd);
                    unlink(mime_tmpfile);
                    return(-1);
                }
#else
                mktemp(mime_tmpfile);

                tfp=fopen(mime_tmpfile,"w");
                if (tfp == NULL)
                {
                    (void) fprintf(stderr,"Could not open tmp file \"%s\" for writing (for %s)\n",mime_tmpfile,a->file_name);
                    return (-1);
                }
#endif /* HAVE_MKSTEMP */

                /* base64 encode only if it is not mime type of text/plain */

                if (mutilsStrcasecmp(a->mime_type,"text/plain") != 0)
                {
                    mutilsBase64Encode(fp,tfp);
                    if (tfp)
                    {
                        (void) fclose(tfp);
                    }
                }
                else
                {
                    FILE
                        *ttp;
                    char
                        xbuf[BUFSIZ];

                    /* write the text file to tmp file */
                    ttp=fopen(a->file_path,"r");
                    if (ttp == NULL)

                    {
                        errorMsg("%s (%d) - could not open file %s for reading (%s)\n",MFL,a->file_path,ERR_STR);
                        continue;
                    }
                    while(fgets(xbuf,sizeof(xbuf)-1,ttp))
                    {
                        fputs(xbuf,tfp);
                    }
                    fclose(ttp);
                    (void) fclose(tfp);

                }
                if (fp)
                    (void) fclose(fp);

                (void) snprintf(buf,sizeof(buf)-1,"--%s\r\n",boundary);
                msock_puts(buf);
                showVerbose(buf);
                

                if (mutilsStrcasecmp(a->mime_type,"text/plain") == 0)
                {
                    /*
                    ** Guess the file type first. 
                    ** If binary, change my type and base64 encode it.
                    ** If the file is text and in Unix format, write it
                    ** back as Dos file (with \r\n
                    ** TODO
                    ** muquit@muquit.com Mar-21-2007 
                    */

                    (void) snprintf(buf,sizeof(buf)-1,"Content-Type: text/plain; charset=%s\r\n",g_charset);
                    msock_puts(buf);
                    showVerbose(buf);

                    (void) snprintf(buf,sizeof(buf)-1,"Content-Disposition: %s; filename=\"%s\"\r\n",
                                    a->content_disposition,
                                    a->file_name);
                    msock_puts(buf);
                    showVerbose(buf);

                    msock_puts("\r\n");
                    showVerbose("\r\n");
                }
                else
                {
                    (void) snprintf(buf,sizeof(buf)-1,"Content-Type: %s; name=%s\r\n",a->mime_type,a->file_name);
                    msock_puts(buf);
                    showVerbose(buf);

                    (void) snprintf(buf,sizeof(buf)-1,"Content-Disposition: %s; filename=\"%s\"\r\n",
                                    a->content_disposition,
                                    a->file_name);
                    msock_puts(buf);
                    showVerbose(buf);

                    msock_puts("Content-Transfer-Encoding: base64\r\n\r\n");
                    showVerbose("Content-Transfer-Encoding: base64\r\n\r\n");
                }

                fp=fopen(mime_tmpfile,"r");
                if (fp == (FILE *) NULL)
                {
                    (void) fprintf(stderr,"Could not open tmp file \"%s\" for reading\n",mime_tmpfile);
                    return (-1);
                }
                while (fgets(mbuf,sizeof(mbuf)-1,fp))
                {
                    msock_puts(mbuf);
                    showVerbose("[C] %s",mbuf); /* new line is there */
                }
                (void) fclose(fp);
                unlink(mime_tmpfile);
            }
            
            (void) snprintf(buf,sizeof(buf)-1,"\r\n--%s--\r\n",boundary);
            msock_puts(buf);
            showVerbose(buf);
        }
        goto done;
    }

    /* mail body */
    if (attach_file == NULL && the_msg == NULL) /* read from stdin */
    {

        /* if stdin is a terminal, print the instruction */
        if (isInConsole(_fileno(stdin)))
        {
            (void) printf("=========================================================================\n");
            (void) printf("Type . in a new line and press Enter to end the message, CTRL+C to abort\n");
            (void) printf("=========================================================================\n");
        }

#ifdef WINNT
        SetConsoleCtrlHandler(CntrlHandler,TRUE);
#endif /* WINNT */

        newline_before=1;
        while (fgets(mbuf,sizeof(mbuf)-1,stdin) && (break_out == 0))
        {
            if (newline_before && *mbuf == '.')
            {
                break;
            }
            else
            {
                int
                    len;
                /* vinicio qmail fix */
                len=strlen(mbuf);
                if (mbuf[len-1] != '\n')
                    strcat(mbuf,"\r\n");
                else
                {
                   mbuf[--len]='\0';
                   strcat(mbuf,"\r\n");
                }
                /* vinicio qmail fix */
                msock_puts(mbuf);
                showVerbose("[C] %s",mbuf);
            }
            newline_before=(*mbuf != '\0' && mbuf[strlen(mbuf)-1] == '\n');
            if (break_out == 1)
            {
                (void) fprintf(stderr," Breaking out\n");
                return (0);
            }
        }
    }
done:

    return (0);
}

/* greeting can be multi line */
static int read_greetings(void)
{
    int
        lcnt=0,
        rc=(-1);

    rc=read_smtp_line();
    if (rc < 0)
        goto cleanup;

    s_esmtp=g_esmtp; /* if foreced with -ehlo */

    if (smtp_code != 220)
    {
        errorMsg("Expected smtp code 220, got %d\n",smtp_code);
        rc=(-1);
        goto cleanup;
    }
    if (mutilsStristr(smtp_line,"ESMTP"))
    {
        s_esmtp=1;
    }
    /* greeting can be multi-line */
    if (smtp_sep == A_DASH)
    {
        for (;;)
        {
            rc=read_smtp_line();
            if (rc < 0)
                break;
            lcnt++;
            if (lcnt >= 100)
            {
                errorMsg("Too many greeting lines\n");
                rc=(-1);
                goto cleanup;
            }
            if (mutilsStristr(smtp_line,"ESMTP"))
            {
                s_esmtp=1;
            }
            if (smtp_sep != A_DASH)
                break;
        }
    }
cleanup:
    return(rc);
}

/* It's stupid, I need to change all the args to a struct, one of those 
 * days! I'll do it
 */

/* returns 0 on success, -1 on failure */
int send_the_mail(char *from,char *to,char *cc,char *bcc,char *sub,
             char *smtp_server,int smtp_port,char *helo_domain,
             char *attach_file,char *txt_msg_file,char *the_msg,int is_mime,char *rrr,char *rt,
             int add_dateh)
{
    SOCKET
        sfd;

    TheMail
        *mail;

    Sll
        *al;

    int
        rc=(-1);

    char
        *mech=NULL,
        *b64=NULL,
        *auth=NULL;

    unsigned long
        b64len=0;

    int
        authenticated=0;
 
    /*
    (void) fprintf(stderr,"From: %s\n",from);
    (void) fprintf(stderr,"To: %s\n",to);
    (void) fprintf(stderr,"Cc: %s\n",cc);
    (void) fprintf(stderr,"Cc: %s\n",bcc);
    (void) fprintf(stderr,"Sub: %s\n",sub);
    (void) fprintf(stderr,"smtp: %s\n",smtp_server);
    (void) fprintf(stderr,"smtp port: %d\n",smtp_port);
    (void) fprintf(stderr,"domain: %s\n",helo_domain);
    (void) fprintf(stderr,"attach file: %s\n",attach_file);
    (void) fprintf(stderr,"txt_msg_file: %s\n",txt_msg_file);
    (void) fprintf(stderr,"the_msg: %s\n",the_msg);
    (void) fprintf(stderr,"is_mime: %d\n",is_mime);
    */

    al=getAddressList();

    if (al == (Sll *) NULL)
    {
        errorMsg("No To address/es specified");
        return (-1);
    }

    if (from == (char *) NULL)
    {
        errorMsg("No From address specified");
        return (-1);
    }

    if (smtp_server == (char *) NULL)
        smtp_server="127.0.0.1";

    if (smtp_port == -1)
        smtp_port=MAILSEND_SMTP_PORT;

    if (sub == (char *) NULL)
        sub=MAILSEND_DEF_SUB;

    if (helo_domain == (char *) NULL)
    {
        errorMsg("No domain specified");
        return (-1);
    }

    mail=newTheMail();
    if (mail == (TheMail *) NULL)
    {
        (void) fprintf(stderr,"Error: malloc failed in createTheMail()\n");
        return (-1);
    }

    /* open the network connection */
    sfd=smtpConnect(smtp_server,smtp_port);
    if (sfd == INVALID_SOCKET)
    {
        rc=(-1);
        goto cleanup;
    }

    /* read greeting */
    rc=read_greetings();
    if (rc < 0)
        goto cleanup;

    /* say HELO/EHLO */
    say_helo(helo_domain);

    /* check if the server supports STARTTLS or TLS */
    if (g_do_starttls)
    {
        if (check_server_cap("STARTTLS") ||
            check_server_cap("TLS"))
        {
            rc=smtp_start_tls(sfd);
            if (rc == 0)
            {
               /* send HELO again */
                say_helo(helo_domain);
            }
        }
    }

    auth=check_server_cap("AUTH");
    if (!auth)
        goto MailFrom;
    if (auth && g_do_auth)
    {
        g_auth_cram_md5=1;
        g_auth_login=1;
        g_auth_plain=1;
    }
    /* Try CRAM-MD5 first */
    mech="CRAM-MD5";
    if (g_auth_cram_md5 && check_server_cap(mech))
    {
        char
            *cmd5;

        CHECK_USERNAME(mech);
        CHECK_USERPASS(mech);

#ifndef HAVE_OPENSSL
        errorMsg("Must be compiled with OpenSSL in order to get CRAM-MD5 support\n");
        goto cleanup;
#endif /* !HAVE_OPENSSL */
        showVerbose("Using AUTH %s\n",mech);
        memset(buf,0,sizeof(buf));
        (void) snprintf(buf,sizeof(buf)-1,"AUTH %s\r\n",mech);
        showVerbose("[C] %s",buf);
        msock_puts(buf);

        read_smtp_line();
        if (smtp_code != 334)
        {
            errorMsg("AUTH CRAM-MD5 failed: '%d %s'",
                    smtp_code,
                    smtp_line);
            rc=(-1);
            goto cleanup;
        }
        cmd5=encode_cram_md5(smtp_line,g_username,g_userpass);
        memset(buf,0,sizeof(buf));
        (void) snprintf(buf,sizeof(buf)-1,"%s\r\n",cmd5);
        showVerbose("[C] %s",buf);
        msock_puts(buf);
        read_smtp_line();
        if (smtp_code != 235)
        {
            errorMsg("AUTH CRAM-MD5 failed: '%d %s'",
                    smtp_code,
                    smtp_line);
            rc=(-1);
            goto cleanup;
        }

        showVerbose("%s Authentication succeeded\n",mech);
        authenticated++;

    }
    if (authenticated)
        goto MailFrom;

    mech="LOGIN";
    if (g_auth_login && check_server_cap(mech))
    {
        CHECK_USERNAME(mech);
        CHECK_USERPASS(mech);

        showVerbose("Using AUTH %s\n",mech);
        memset(buf,0,sizeof(buf));
        (void) snprintf(buf,sizeof(buf)-1,"AUTH %s\r\n",mech);
        showVerbose("[C] %s",buf);
        msock_puts(buf);

        read_smtp_line();
        if (smtp_code != 334)
        {
            errorMsg("AUTH LOGIN failed: '%d %s'",
                    smtp_code,
                    smtp_line);
            rc=(-1);
            goto cleanup;
        }

        b64=mutils_encode_base64(g_username,strlen(g_username),&b64len);
        if (b64 == NULL)
        {
            errorMsg("Could not base64 encode user: %s",buf);
            rc=(-1);
            goto cleanup;
        }
        /* mutils_encode_base64 adds CRLF */
        b64[b64len-2]='\0';

        memset(buf,0,sizeof(buf));
        (void) snprintf(buf,sizeof(buf)-1,"%s\r\n",b64);
        msock_puts(buf);
        read_smtp_line();
        if (smtp_code != 334)
        {
            errorMsg("AUTH LOGIN failed: '%d %s'",
                    smtp_code,
                    smtp_line);
            rc=(-1);
            goto cleanup;
        }

        b64=mutils_encode_base64(g_userpass,strlen(g_userpass),&b64len);
        if (b64 == NULL)
        {
            errorMsg("Could not base64 encode user: %s",buf);
            rc=(-1);
            goto cleanup;
        }
        /* mutils_encode_base64 adds CRLF */
        b64[b64len-2]='\0';

        memset(buf,0,sizeof(buf));
        (void) snprintf(buf,sizeof(buf)-1,"%s\r\n",b64);
        msock_puts(buf);
        read_smtp_line();
        if (smtp_code != 235)
        {
            errorMsg("AUTH LOGIN failed: '%d %s'",
                    smtp_code,
                    smtp_line);
            rc=(-1);
            goto cleanup;
        }
        authenticated++;
    }

    if (authenticated)
        goto MailFrom;

    mech="PLAIN";
    if (g_auth_plain && check_server_cap(mech))
    {
        int
            len,
            ulen,
            plen;

        unsigned char
            *b64=NULL;

        CHECK_USERNAME(mech);
        CHECK_USERPASS(mech);

        memset(buf,0,sizeof(buf));
        /*
        ** authzid\0authid\0pass
        ** authzid can be skipped if both are the same
        */

        ulen=strlen(g_username);
        memcpy(buf + 1,g_username,ulen);
        plen=strlen(g_userpass);

        memcpy(buf + ulen + 2,g_userpass,plen);
        len=ulen + plen + 2;
        b64=mutils_encode_base64(buf,len,&b64len);
        /* mutils_encode_base64 adds CRLF */
        b64[b64len-2]='\0';
        (void) snprintf(buf,sizeof(buf)-1,"AUTH PLAIN %s\r\n",(char *) b64);

        showVerbose("[C] %s",buf);
        msock_puts(buf);

        read_smtp_line();
        if (smtp_code != 235)
        {
            errorMsg("AUTH PLAIN failed: '%d %s'",
                    smtp_code,
                    smtp_line);
            rc=(-1);
            goto cleanup;
        }
        showVerbose("PLAIN Authentication succeeded\n");
        authenticated++;
    }

    if (authenticated)
        goto MailFrom;

    if (auth && !g_quiet)
    {
        if (!g_auth_cram_md5)
        {
            if (check_server_cap("CRAM-MD5"))
            {
            (void) fprintf(stderr,
" *Server supports AUTH CRAM-MD5.");
            (void) fprintf(stderr,
" Enable with -auth-cram-md5 or -auth\n");
            }
        }
        if (!g_auth_login)
        {
            if (check_server_cap("LOGIN"))
            {
            (void) fprintf(stderr,
" *Server supports AUTH LOGIN.");
            (void) fprintf(stderr,
" Enable with -auth-login or -auth\n");
            (void) fprintf(stderr,
" AUTH LOGIN should be used if server supports STARTTLS (-starttls)\n");
            }
        }

        if (!g_auth_plain)
        {
            if (check_server_cap("PLAIN"))
            {
            (void) fprintf(stderr,
" *Server supports AUTH PLAIN.");
            (void) fprintf(stderr,
" Enable with -auth-plain or -auth\n");
            (void) fprintf(stderr,
" AUTH PLAIN should be used if server supports STARTTLS (-starttls)\n");
            }
        }
    }

MailFrom:
    rc=smtp_MAIL_FROM(from);
    if (rc != 0)
        goto cleanup;

    rc=smtp_RCPT_TO();
    if (rc != 0)
        goto cleanup;

    rc=smtp_DATA();
    if (rc != 0)
        goto cleanup;

    rc=smtpMail(sfd,to,cc,bcc,from,rrr,rt,sub,attach_file,txt_msg_file,the_msg,is_mime,add_dateh);

    rc=smtpEom(sfd);
    smtp_QUIT();

cleanup:
    return (rc);
}

/*
** print info about SMTP server on stdout
*/
void show_smtp_info(char *smtp_server,int port,char *domain)
{
    int
        fd,
        rc;
    char
        *x;
    g_verbose=1;
    x="localhost";
    if (mutilsStrncasecmp(smtp_server,x,strlen(x)) == 0)
    {
        print_info("SMTP serve: localhost, specify different one with -smtp <smtp_server>\n");
    }
    else
    {
        print_info("SMTP server: %s\n",smtp_server);
    }
    /* connect */
    fd=smtpConnect(smtp_server,port);
    if (fd == INVALID_SOCKET)
    {
        goto ExitProcessing;
    }

    /* read greeting */
    rc=read_greetings();
    if (rc < 0)
        goto ExitProcessing;

    /* say HELO/EHLO */
    say_helo(domain);

    /* check if server supports STARTTLS */
    if (check_server_cap("STARTTLS") ||
        check_server_cap("TLS"))
    {
        rc=smtp_start_tls(fd);
        if (rc == 0)
            say_helo(domain);
        else
            goto ExitProcessing;
    }
    smtp_QUIT();
    smtpDisconnect();

ExitProcessing:
    return;
}

