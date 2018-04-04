/*
**  main for mailsend - a simple mail sender via SMTP protocol
**
**  Limitations and Comments:
**      I needed to send a alert mail from a bare-bone networked NT machine,
**      but could not find a simple mail sender to do this (not surprised!).
**      so I wrote this one!
**
**
**  Development History:
**      who                  when           why
**      muquit@muquit.com    Mar-23-2001    first cut
*/

#define __MAIN__    1

#include "mailsend.h"


/* exits after writing the usage */
static void usage(void)
{
    char
        **p;

    static char
        *options[]=
        {
" -smtp hostname/IP*    - of the SMTP server",
" -port SMTP port       - SMTP port",
" -d    domain          - domain name for SMTP HELO/EHLO",
" -t    to,to..*        - email address/es of the reciepient/s",
" -cc   cc,cc..         - Carbon copy address/es",
" +cc                   - don't ask for Carbon Copy",
" -bc   bcc,bcc..       - Blind carbon copy address/es",
" +bc                   - don't ask for Blind carbon copy",
" +D                    - don't add Date header",
" -f    address*        - email address of the sender",
" -sub  subject         - subject",
" -l    file            - a file containing the email addresses",
" -attach file,mime_type,[i/a] (i=inline,a=attachment)",
"                       - attach this file as attachment or inline",
" -cs   character set   - for text/plain attachments (default is us-ascii)",
" -M    \"one line msg\"  - attach this one line text message",
" -name \"Full Name\"     - add name in the From header",
" -v                    - verbose mode",
" -V                    - show version info",
" -w                    - wait for a CR after sending the mail",
" -rt  email_address    - add Reply-To header",
" -rrr email_address    - request read receipts to this address",
" -starttls             - Check for STARTTLS and if server supports, do it",
" -auth                 - Try CRAM-MD5,LOGIN,PLAIN in that order",
" -auth-cram-md5        - use AUTH CRAM-MD5 authentication",
" -auth-plain           - use AUTH PLAIN authentication",
" -auth-login           - use AUTH LOGIN authentication",
" -user username        - username for ESMTP authentication",
" -pass password        - password for ESMTP authentication",
" -example              - show examples",
" -ehlo                 - force EHLO",
" -info                 - show SMTP server information",
" -help                 - shows this help",
" -q                    - quiet",
            (char *) NULL
        };
    (void) printf("\n");
    (void) printf("Version: %.1024s\n\n",MAILSEND_VERSION);
    (void) printf("Copyright: %.1024s\n\n",NO_SPAM_STATEMENT);
#ifdef HAVE_OPENSSL
    (void) fprintf(stdout,"(Compiled with %s)\n",
                   SSLeay_version(SSLEAY_VERSION));
#else
    (void) fprintf(stdout,"(Not compiled with OpenSSL)\n");
#endif /* HAVE_OPENSSL */

    (void) printf("usage: mailsend [options]\n");
    (void) printf("Where the options are:\n");

    for (p=options; *p != NULL; p++)
        (void) printf("%s\n",*p);

    (void) fprintf(stdout,"\nThe options with * must the specified\n");

    exit(0);
}

static void show_examples(void)
{
    (void) fprintf(stdout,"Example (Note: type without newline):\n");
(void) fprintf(stderr,
"Show server info:\n"
" mailsend -info -smtp smtp.gmail.com\n\n");

    (void) fprintf(stdout,
" mailsend -f muquit@example.com -d example.com -smtp 10.100.30.1\n"
"  -t muquit@muquit.com -sub test -a \"file.txt,text/plain\"\n"
"  -a \"/usr/file.gif,image/gif\" -a \"file.jpeg,image/jpg\"\n\n");

(void) fprintf(stdout,
" mailsend -f muquit@example.com -d example.com -smtp 192.168.0.2\n"
"  -t muquit@muquit.com -sub test +cc +bc\n"
"  -a \"c:\\file.gif,image/gif\" -M \"Sending a GIF file\"\n\n");

(void) fprintf(stdout,
" mailsend -f muquit@example.com -d example.com -smtp 192.168.0.2\n"
"  -t muquit@muquit.com -sub test +cc +bc -cs \"ISO-8859-1\"\n"
"  -a \"file2.txt,text/plain\"\n\n");

    (void) fprintf(stdout,"Change content disposition to inline:\n");
    (void) fprintf(stdout,
" mailsend -f muquit@example.com -d example.com -smtp 10.100.30.1\n"
"  -t muquit@muquit.com -sub test -a \"nf.jpg,image/jpeg,i\"\n"
"  -M \"content disposition is inline\"\n\n");

    (void) fprintf(stdout,"STARTTLS+AUTH PLAIN:\n");
(void) fprintf(stdout,
" mailsend -f muquit@example.com -d example.com -smtp smtp.gmail.com\n"
" -sub test -from muquit@muquit.com +cc +bc -v -starttls -auth-plain\n"
" -user you -pass 'secert'\n\n");

(void) fprintf(stdout,"STARTTLS+AUTH CRAM-MD5:\n");
(void) fprintf(stdout,
" mailsend -f muquit@example.com -d example.com -smtp 1.2.3.4\n"
" -sub test -from muquit@muquit.com +cc +bc -v -starttls -auth-cram-md5\n"
" -user you -pass 'secert'\n\n");

(void) fprintf(stdout,"STARTTLS+AUTH LOGIN:\n");
(void) fprintf(stdout,
" mailsend -f muquit@example.com -d example.com -smtp 1.2.3.4\n"
" -sub test -from muquit@muquit.com +cc +bc -v -starttls -auth-login\n"
" -user you -pass 'secert'\n");
(void) fprintf(stdout,
"(Password can be set by env var SMTP_USER_PASS instead of -pass)\n\n");
(void) fprintf(stdout,
"Note: I suggest you always use STARTTLS if your server supports it\n");

}

int main(int argc,char **argv)
{
    char
        *x,
        buf[BUFSIZ],
        *cipher=NULL,
        *option;

    int
        smtp_info=0,
        is_mime=0,
        add_dateh=1,
        port=(-1),
        rc,
        no_cc=0,
        no_bcc=0,
        i;

    char
        *address_file=NULL,
        *helo_domain=NULL,
        *smtp_server=NULL,
        *attach_file=NULL,
        *msg_body_file=NULL,
        *the_msg=NULL,
        *to=NULL,
        *save_to=NULL,
        *save_cc=NULL,
        *save_bcc=NULL,
        *from=NULL,
        *sub=NULL,
#if 1 //__MSTC__, Jeff
		*resultPath=NULL,
#endif
        *cc=NULL,
        *bcc=NULL,
        *rt=NULL,
        *rrr=NULL;

    g_verbose=0;
    g_quiet=0;
    g_wait_for_cr=0;
    g_do_auth=0;
    g_esmtp=0;
    g_auth_plain=0;
    g_auth_cram_md5=0;
    g_auth_login=0;
    g_do_starttls=0;
    memset(g_username,0,sizeof(g_username));
    memset(g_userpass,0,sizeof(g_userpass));
    memset(g_from_name,0,sizeof(g_from_name));

    (void) strcpy(g_charset,"us-ascii");

    for  (i=1; i < argc; i++)
    {
        option=argv[i];
        switch (*(option+1))
        {

            case 'a':
            {
                if (strncmp("attach",option+1,2) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing file to attach");
                            return (1);
                        }
                        attach_file=argv[i];
                        add_attachment_to_list(attach_file);
                    }
                }
                else if (strncmp("auth-plain",option+1,
                            strlen("auth-plain"))==0)
                {
                    if (*option == '-')
                    {
                        g_auth_plain=1;
                    }
                }
                else if (strncmp("auth-cram-md5",option+1,
                            strlen("auth-cram-md5"))==0)
                {
                    if (*option == '-')
                    {
                        g_auth_cram_md5=1;
                    }
                }
                else if (strncmp("auth-login",option+1,
                            strlen("auth-login"))==0)
                {
                    if (*option == '-')
                    {
                        g_auth_login=1;
                    }
                }
                else if (strncmp("auth",option+1,
                            strlen("auth"))==0)
                {
                    if (*option == '-')
                    {
                        g_auth_login=1;
                        g_auth_cram_md5=1;
                        g_auth_login=1;
                        g_do_auth=1;
                    }
                }

                else
                {
                    errorMsg("Unknown flag: %s\n",option);
                    return(1);
                }
                break;
            }

            case 'b':
            {
                if (strncmp("bcc",option+1,2) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing BCc address/es");
                            return (1);
                        }
                        bcc=argv[i];
                        save_bcc=mutilsStrdup(bcc);

                        /* collapse all spaces to a comma */
                        mutilsSpacesToChar(bcc,',');
                        addAddressToList(bcc,"Bcc");
                    }
                    else if (*option == '+')
                    {
                        no_bcc=1;
                    }
                }
                else
                {
                    errorMsg("Unknown flag: %s\n",option);
                    return(1);
                }
                break;
            }


            case 'c':
            {
                if (strncmp("cc",option+1,2) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing Cc address/es");
                            return (1);
                        }
                        cc=argv[i];
                        save_cc=mutilsStrdup(cc);

                        /* collapse all spaces to a comma */
                        mutilsSpacesToChar(cc,',');
                        addAddressToList(cc,"Cc");
                    }
                    else if (*option == '+')
                    {
                        no_cc=1;
                    }
                }
                else if (strncmp("cs",option+1,2) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing character set");
                            return (1);
                        }
                        mutilsSafeStrcpy(g_charset,argv[i],sizeof(g_charset)-1);
                    }

                }
                else
                {
                    errorMsg("Unknown flag: %s\n",option);
                    return(1);
                }
                break;
            }

            case 'D':
            {
                if (strncmp("D",option+1,1) == 0)
                {
                    if (*option == '+')
                    {
                        add_dateh=0;
                    }
                }
                break;
            }


            case 'd':
            {
                if (strncmp("domain",option+1,1) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing domain name");
                            return (1);
                        }
                        helo_domain=argv[i];
                    }
                }
                break;
            }

            case 'e':
            {
                if (*option == '-')
                {
                    if (strncmp("example",option+1,3) == 0)
                    {
                        show_examples();
                        return(1);
                    }
                    if (strncmp("ehlo",option+1,4) == 0)
                    {
                        g_esmtp=1;
                    }
                }
                break;
            }

            case 'f':
            {
                if (strncmp("from",option+1,1) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing From address/es");
                            return (1);
                        }
                        from=argv[i];
                    }
                }
                break;
            }

            case 'h':
            {
                if (strncmp("help",option+1,1) == 0)
                {
                    usage();
                }
                /* won't be here */
                break;
            }

            case 'i':
            {
                if (strncmp("info",option+1,2) == 0)
                {
                    smtp_info=1;
                }
                break;
            }

            case 'l':
            {
                if (strncmp("list_address",option+1,1) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing address list file"); 
                            return (1);
                        }
                        address_file=argv[i];
                    }
                }
                break;
            }


            case 'p':
            {
                if (strncmp("port",option+1,2) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing SMTP Port with -port");
                            return (1);
                        }
                        port=atoi(argv[i]);
                    }
                }
                else if (strncmp("pass",option+1,2) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing password with -pass");
                            return (1);
                        }
                        (void) snprintf(g_userpass,sizeof(g_userpass)-1,
                                        "%s",argv[i]);
                    }

                }
                else
                {
                    errorMsg("Unknown flag: %s\n",option);
                    return(1);
                }
                break;
            }

            /* -gone-
            case 'm':
            {
                if (strncmp("msgbody",option+1,1) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing message body file");
                            return (1);
                        }
                        msg_body_file=argv[i];
                    }
                }
                else
                {
                    errorMsg("Unknown flag: %s\n",option);
                    return(1);
                }

                break;
            }
            */

            case 'M':
            {
                if (strncmp("Message",option+1,1) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing text message");
                            return (1);
                        }
                        the_msg=argv[i];
                    }
                }
                else
                {
                    errorMsg("Unknown flag: %s\n",option);
                    return(1);
                }

                break;
            }

            case 'n':
            {
                if (strncmp("name",option+1,3) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing Name with -n");
                            return (1);
                        }
                        (void) snprintf(g_from_name,sizeof(g_from_name)-1,
                                        "%s",argv[i]);
                    }
                }
                else
                {
                    errorMsg("Unknown flag: %s\n",option);
                    return(1);
                }

                break;
            }

            case 's':
            {
                if (strncmp("smtp",option+1,3) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing smtp server");
                            return (1);
                        }
                        smtp_server=argv[i];
                    }
                }
                else if (strncmp("subject",option+1,3) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing subject with -sub");
                            return (1);
                        }
                        sub=argv[i];
                    }
                }
                else if (strncmp("starttls",option+1,3) == 0)
                {
#ifdef HAVE_OPENSSL
                    g_do_starttls=1;
#else
                    (void) fprintf(stderr,"Warning: '-starttls' not available, only avaible if compiled with OpenSSL\n");
                    
#endif /* HAVE_OPENSSL */
                }
#if 1 //__MSTC__, Jeff
				else if((strncmp("saveresult",option+1,3) == 0))
				{
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing saveresult path");
                            return (1);
                        }
                        resultPath=argv[i];
                    }
				}
#endif

                else
                {
                    errorMsg("Unknown flag: %s\n",option);
                    return(1);
                }

                break;
            }

            case 'u':
            {
                if (strncmp("user",option+1,2) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            errorMsg("Missing smtp server");
                            return (1);
                        }
                        (void) snprintf(g_username,sizeof(g_username)-1,
                                        "%s",argv[i]);
                    }
                }
                else
                {
                    errorMsg("Unknown flag: %s\n",option);
                    return(1);
                }

                break;
            }



            case 'v':
            {
                if (strncmp("verbose",option+1,1) == 0)
                {
                    if (*option == '-')
                    {
                        g_verbose=1;
                    }
                }
                break;
            }

            case 'q':
            {
                if (strncmp("quiet",option+1,1) == 0)
                {
                    if (*option == '-')
                    {
                        g_quiet=1;
                    }
                }
                break;
            }
            

            case 'V':
            {
                (void) fprintf(stderr,"mailsend Version: %.1024s\n",MAILSEND_VERSION);
#ifdef HAVE_OPENSSL
                (void) fprintf(stderr,"Compiled with %s\n",
                               SSLeay_version(SSLEAY_VERSION));
#else
                (void) fprintf(stderr,"Not Compiled OpenSSL, some auth methods will be unavailable\n");
#endif /* ! HAVE_OPENSSL */
                exit(0);
                break;
            }

           case 't':
           {
                if (strncmp("to",option+1,1) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            (void) fprintf(stderr,"Error: missing to addresses\n");
                            return (1);
                        }
                        to=argv[i];
                        save_to=mutilsStrdup(to);
                        if (save_to == NULL)
                        {
                            errorMsg("memory allocation problem for -to");
                            return(-1);
                        }
                        save_to=fix_to(save_to);
                        to=fix_to(to);
                        /* collapse all spaces to a comma */
                        mutilsSpacesToChar(to,',');

                        /* add addresses to a singly linked list */
                        addAddressToList(to,"To");
                        /* Note: to is modifed now! */
                    }
                }
                break;
            }

            case 'r':
            {
                if (strncmp("rrr",option+1,3) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            (void) fprintf(stderr,"Error: missing to addresses for -rrr\n");
                            return (1);
                        }
                        rrr=mutilsStrdup(argv[i]);
                        if (rrr == NULL)
                        {
                            errorMsg("memory allocation problem for -rrr");
                            return(1);
                        }
                    }
                }
                else if (strncmp("rt",option+1,2) == 0)
                {
                    if (*option == '-')
                    {
                        i++;
                        if (i == argc)
                        {
                            (void) fprintf(stderr,"Error: missing to addresses for -rt\n");
                            return (1);
                        }
                        rt=mutilsStrdup(argv[i]);
                        if (rt == NULL)
                        {
                            errorMsg("memory allocation problem for -rt");
                            return(1);
                        }
                    }
                }
                else
                {
                    errorMsg("Unknown flag: %s\n",option);
                    return(1);
                }

                break;
            }

            case 'w':
            {
                if (strncmp("wait",option+1,1) == 0)
                {
                    if (*option == '-')
                    {
                        g_wait_for_cr=1;
                    }
                }

                break;
            }

            default:
            {
                (void) fprintf(stderr,"Error: Unrecognized option: %s\n",
                               option);
                return (1);
            }


        }
    }
    
    initialize_openssl(cipher);

    if (port == -1)
        port=MAILSEND_SMTP_PORT;
    if (smtp_info)
    {
        if (smtp_server == NULL)
            smtp_server="localhost";
    }
    if (smtp_info && smtp_server)
    {
        if (helo_domain == NULL)
            helo_domain="localhost";
        show_smtp_info(smtp_server,port,helo_domain);
        return(1);
    }


    print_attachemtn_list();

    /*
    ** attaching a file or a one line message will make the mail a 
    ** MIME mail
    */
    if (attach_file || the_msg || msg_body_file)
    {
        is_mime=1;
    }

    if (smtp_server == NULL)
    {
        memset(buf,0,sizeof(buf));
        x=askFor(buf,sizeof(buf)-1,"SMTP server address/IP: ",EMPTY_NOT_OK);
        if (x)
            smtp_server=xStrdup(x);
    }

    if (helo_domain == NULL)
    {
        /*
        memset(buf,0,sizeof(buf));
        x=askFor(buf,sizeof(buf)-1,"Domain: ",EMPTY_NOT_OK);
        if (x)
            helo_domain=xStrdup(x);
        */
        /* use localhost */
        helo_domain=xStrdup("localhost");
    }

    if (from == NULL)
    {
        memset(buf,0,sizeof(buf));
        x=askFor(buf,sizeof(buf)-1,"From: ",EMPTY_NOT_OK);
        if (x)
            from=xStrdup(x);
    }

    x=getenv("SMTP_USER_PASS");
    if (x)
    {
        if (*g_userpass == '\0')
        {
            (void) snprintf(g_userpass,sizeof(g_userpass)-1,"%s",x);
        }
    }

    /* if address file specified, add the addresses to the list as well */
    if (address_file != NULL)
    {
        addAddressesFromFileToList(address_file);
        printAddressList();
    }

   /*
    ** The To address must be speicifed, even if the file with the list of
    ** addresses is specified. The specified To will be shown in the 
    ** envelope. None of the To, Cc and Bcc from the address list file will
    ** be shown anywhre.. that's how I like it. I hate long To, Cc or Bcc.
    ** muquit@muquit.com, Thu Mar 29 11:56:45 EST 2001 
    */

    if (save_to == NULL)
    {
        /* don't ask for To add addresses are specified with -l file */
        if (getAddressList() == NULL)
        {
            memset(buf,0,sizeof(buf));
            x=askFor(buf,sizeof(buf)-1,"To: ",EMPTY_NOT_OK);
            if (x)
            {
                save_to=xStrdup(x);
                addAddressToList(x,"To");
            }
        }
    }

    /*
    ** if msg file specified, dont ask for unneeded things, as it could
    ** be used from other programs, and it will wait for input
    ** muquit@muquit.com Tue Apr 10 18:02:12 EST 2001 
    */

#ifdef WINNT
    if (attach_file == NULL && isInConsole(_fileno(stdin)))
#else
    if (attach_file == NULL && isatty(fileno(stdin)))
#endif /* WINNT */
    {
        if (save_cc == NULL && !no_cc)
        {
            memset(buf,0,sizeof(buf));
            x=askFor(buf,sizeof(buf)-1,"Carbon copy: ",EMPTY_OK);
            if (x)
            {
                save_cc=xStrdup(x);
                addAddressToList(x,"Cc");
            }
        }

        if (save_bcc == NULL && ! no_bcc)
        {
            memset(buf,0,sizeof(buf));
            x=askFor(buf,sizeof(buf)-1,"Blind Carbon copy: ",EMPTY_OK);
            if (x)
            {
                save_bcc=xStrdup(x);
                addAddressToList(x,"BCc");
            }
        }

        if (sub == NULL)
        {
            memset(buf,0,sizeof(buf));
            x=askFor(buf,sizeof(buf)-1,"Subject: ",EMPTY_OK);
            if (x)
                sub=xStrdup(x);
        }
    }



    /* TODO: read from default file or registry */
    rc=validateMusts(from,save_to,smtp_server,helo_domain);
    if (rc == -1)
        return (1); /* exit */

#ifdef UNIX
    signal(SIGPIPE,SIG_IGN);
#endif /* UNIX */

    rc=send_the_mail(from,save_to,save_cc,save_bcc,sub,smtp_server,port,
                helo_domain,attach_file,msg_body_file,the_msg,is_mime,rrr,rt,add_dateh);

#if 1 //__MSTC__, Jeff
	if(resultPath){
		FILE *fp;
		fp = fopen(resultPath,"w");
		if(rc == 0)
			fputc('1',fp);
		else
			fputc('0',fp);
		fclose(fp);
	}
#endif

    if (rc == 0)
    {
        if (isInteractive())
        {
            (void) printf("Mail sent successfully\n");
            (void) fflush(stdout);
        }
    }
    else
    {
        if (isInteractive())
        {
            (void) printf("Could not send mail\n");
        }
    }

    if (isInteractive())
    {
        if (g_wait_for_cr)
        {
            printf("\nPress Enter to Exit: ");
            fgets(buf,sizeof(buf)-1,stdin);
        }
    }


    return (rc);
}
