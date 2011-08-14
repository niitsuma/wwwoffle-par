/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9d.
  Generate information about the contents of the web pages that are cached in the system.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 2002,03,04,05,06,07 Andrew M. Bishop
  Parts of this file Copyright (C) 2007 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <sys/stat.h>
#include <fcntl.h>

#if USE_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#endif

#include "wwwoffle.h"
#include "errors.h"
#include "io.h"
#include "misc.h"
#include "config.h"
#include "certificates.h"


/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif

#if defined(__CYGWIN__)
#define CYGWIN_REPLACE_COLONS(str) {char *_p; for(_p=(str);*_p;++_p) if(*_p==':') *_p='!';}
#define CYGWIN_REVERT_COLONS(str)  {char *_p; for(_p=(str);*_p;++_p) if(*_p=='!') *_p=':';}
#else
#define CYGWIN_REPLACE_COLONS(str)
#define CYGWIN_REVERT_COLONS(str)
#endif

#if USE_GNUTLS

/* Warning: the following construct uses alloca(), so better
   not use inside a loop.
*/
#define RETRY_FUNC_WITH_ALLOCA_BUFFER(func,arg,buf,result)		\
{									\
  size_t _size=sizeof(buf);						\
  int _err=func(arg,buf,&_size);					\
  (result)=buf;								\
  if(_err==GNUTLS_E_SHORT_MEMORY_BUFFER && _size<=MAXDYNBUFSIZE) {	\
    char *_allocbuf=alloca(_size);					\
    if(!(_err=func(arg,_allocbuf,&_size))) {				\
      (result)=_allocbuf;						\
    }									\
  }									\
  if(_err)								\
    snprintf(buf,sizeof(buf),"*** Error: %s ***",gnutls_strerror(_err)); \
}


/*+ The trusted root certificate authority certificates. +*/
extern gnutls_x509_crt_t *trusted_x509_crts;

/*+ The number of trusted root certificate authority certificates. +*/
extern int n_trusted_x509_crts;


static void CertificatesIndexPage(int fd);
static void CertificatesRootPage(int fd,int download);
static void CertificatesServerPage(int fd,URL *Url);
static void CertificatesFakePage(int fd,URL *Url);
static void CertificatesRealPage(int fd,URL *Url);
static void CertificatesTrustedPage(int fd,URL *Url);

static void load_display_certificate(int fd,char *certfile,char *type,char *name);
static void display_certificate(int fd,gnutls_x509_crt_t crt);


/*++++++++++++++++++++++++++++++++++++++
  Display the certificates that are stored.

  int fd The file descriptor to write the output to.

  URL *Url The URL that was requested for the info.

  Header *request_head The header of the original request.
  ++++++++++++++++++++++++++++++++++++++*/

void CertificatesPage(int fd,URL *Url,/*@unused@*/ Header *request_head)
{
 if(!strcmp(Url->path,"/certificates") || !strcmp(Url->path,"/certificates/"))
    CertificatesIndexPage(fd);
 else if(!strcmp(Url->path,"/certificates/root"))
    CertificatesRootPage(fd,0);
 else if(!strcmp(Url->path,"/certificates/root-cert.pem"))
    CertificatesRootPage(fd,1);
 else if(!strcmp(Url->path,"/certificates/server") && Url->args)
    CertificatesServerPage(fd,Url);
 else if(!strcmp(Url->path,"/certificates/fake") && Url->args)
    CertificatesFakePage(fd,Url);
 else if(!strcmp(Url->path,"/certificates/real") && Url->args)
    CertificatesRealPage(fd,Url);
 else if(!strcmp(Url->path,"/certificates/trusted") && Url->args)
    CertificatesTrustedPage(fd,Url);
 else
    HTMLMessage(fd,404,"WWWOFFLE Illegal Certificates Page",NULL,"CertIllegal",
                "url",Url->pathp,
                NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Display the main certificates listing page.

  int fd The file descriptor to write the output to.
  ++++++++++++++++++++++++++++++++++++++*/

static void CertificatesIndexPage(int fd)
{
 DIR *dir;
 struct dirent* ent;
 int i;

 HTMLMessageHead(fd,200,"WWWOFFLE Certificates Index",
                 NULL);

 if(out_err==-1 || head_only) return;

 HTMLMessageBody(fd,"CertIndex-Head",
                 NULL);

 if(out_err==-1) return;

 HTMLMessageBody(fd,"CertIndex-Body",
                 "type","root",
                 "name","WWWOFFLE CA",
                 NULL);

 if(out_err==-1) return;

 /* Read in all of the certificates in the server directory. */

 dir=opendir("certificates/server");
 if(dir)
   {
    ent=readdir(dir);
    if(ent)
      {
       do
         {
	  size_t strlen_name;

          if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
             continue; /* skip . & .. */

	  strlen_name=strlen(ent->d_name);
          if(strlen_name>9 && !strcmp(ent->d_name+strlen_name-9,"-cert.pem"))
            {
	     char *server=strndup(ent->d_name,strlen_name-9);

             CYGWIN_REVERT_COLONS(server);

             HTMLMessageBody(fd,"CertIndex-Body",
                             "type","server",
                             "name",server,
                             NULL);

             free(server);
            }
         }
       while(out_err!=-1 && (ent=readdir(dir)));
      }
    closedir(dir);
   }

 if(out_err==-1) return;

 /* Read in all of the certificates in the fake directory. */

 dir=opendir("certificates/fake");
 if(dir)
   {
    ent=readdir(dir);
    if(ent)
      {
       do
         {
	  size_t strlen_name;

          if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
             continue; /* skip . & .. */

	  strlen_name=strlen(ent->d_name);
          if(strlen_name>9 && !strcmp(ent->d_name+strlen_name-9,"-cert.pem"))
            {
	     char *fake=strndup(ent->d_name,strlen_name-9);

	     CYGWIN_REVERT_COLONS(fake);

             HTMLMessageBody(fd,"CertIndex-Body",
                             "type","fake",
                             "name",fake,
                             NULL);

             free(fake);
            }
         }
       while(out_err!=-1 && (ent=readdir(dir)));
      }
    closedir(dir);
   }

 if(out_err==-1) return;

 /* Read in all of the certificates in the real directory. */

 dir=opendir("certificates/real");
 if(dir)
   {
    ent=readdir(dir);
    if(ent)
      {
       do
         {
	  size_t strlen_name;

          if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
             continue; /* skip . & .. */

	  strlen_name=strlen(ent->d_name);
          if(strlen_name>9 && !strcmp(ent->d_name+strlen_name-9,"-cert.pem"))
            {
	     char *real=strndup(ent->d_name,strlen_name-9);

	     CYGWIN_REVERT_COLONS(real);

             HTMLMessageBody(fd,"CertIndex-Body",
                             "type","real",
                             "name",real,
                             NULL);

             free(real);
            }
         }
       while(out_err!=-1 && (ent=readdir(dir)));
      }
    closedir(dir);
   }

 if(out_err==-1) return;

 /* List all of the trusted certificates */

 for(i=0;i<n_trusted_x509_crts;i++)
   {
     size_t size=256;
     int try=0,err;
     for(;;) {
       char dn[size];

       if((err=gnutls_x509_crt_get_dn(trusted_x509_crts[i],dn,&size))) {
	 if(err==GNUTLS_E_SHORT_MEMORY_BUFFER && ++try<2 && size<=MAXDYNBUFSIZE)
	   continue; /* retry with resized buffer */

	 snprintf(dn,size,"*** Error: %s ***",gnutls_strerror(err));
       }

       HTMLMessageBody(fd,"CertIndex-Body",
		       "type","trusted",
		       "name",dn,
		       NULL);
       break;
     }

    if(out_err==-1) return;
   }

 HTMLMessageBody(fd,"CertIndex-Tail",
                 NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Display the certificate for the certificate authority.

  int fd The file descriptor to write the output to.

  int download If set true then download the raw certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static void CertificatesRootPage(int fd,int download)
{
 static char *certfile="certificates/root/root-cert.pem";

 if(download)
   {
    int cert_fd;
    char buffer[IO_BUFFER_SIZE];
    ssize_t nbytes;

    cert_fd=open(certfile,O_RDONLY|O_BINARY);

    if(cert_fd<0)
      {
       PrintMessage(Warning,"Could not open certificate file '%s' for writing [%!s].",certfile);
       HTMLMessage(fd,500,"WWWOFFLE Certificate Page Error",NULL,"ServerError",
                   "error","Cannot open specified certificate file",
                   NULL);
       return;
      }

    HTMLMessageHead(fd,200,"WWWOFFLE Root Certificate",
                    "Content-Type",WhatMIMEType("*.pem"),
                    NULL);

    if(out_err!=-1 && !head_only)
      while((nbytes=read(cert_fd,buffer,IO_BUFFER_SIZE))>0)
	write_data(fd,buffer,nbytes);

    close(cert_fd);
   }
 else
    load_display_certificate(fd,certfile,"root","root");
}


/*++++++++++++++++++++++++++++++++++++++
  Display the certificate for one of the WWWOFFLE server aliases.

  int fd The file descriptor to write the output to.

  URL *Url The exact URL given to reach this page, specifies the server certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static void CertificatesServerPage(int fd,URL *Url)
{
 char *name=Url->args;
 char certfile[strlen(name)+sizeof("certificates/server/-cert.pem")];

 sprintf(certfile,"certificates/server/%s-cert.pem",name);

 CYGWIN_REPLACE_COLONS(certfile);

 load_display_certificate(fd,certfile,"server",name);
}


/*++++++++++++++++++++++++++++++++++++++
  Display the fake certificate for one of the cached servers.

  int fd The file descriptor to write the output to.

  URL *Url The exact URL given to reach this page, specifies the fake certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static void CertificatesFakePage(int fd,URL *Url)
{
 char *name=Url->args;
 char certfile[strlen(name)+sizeof("certificates/fake/-cert.pem")];

 sprintf(certfile,"certificates/fake/%s-cert.pem",name);

 CYGWIN_REPLACE_COLONS(certfile);

 load_display_certificate(fd,certfile,"fake",name);
}


/*++++++++++++++++++++++++++++++++++++++
  Display the real certificate for one of the cached pages.

  int fd The file descriptor to write the output to.

  URL *Url The exact URL given to reach this page, specifies the real certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static void CertificatesRealPage(int fd,URL *Url)
{
 gnutls_x509_crt_t crtbuf[MAXCERTLIST+1],*crt_list,trusted;
 int i;
 char *name=Url->args;
 char certfile[strlen(name)+sizeof("certificates/real/-cert.pem")];

 sprintf(certfile,"certificates/real/%s-cert.pem",name);

 CYGWIN_REPLACE_COLONS(certfile);

 crt_list=LoadCertificates(certfile,crtbuf,MAXCERTLIST+1);

 if(!crt_list || !crt_list[0])
   {
    HTMLMessage(fd,500,"WWWOFFLE Certificate Page Error",NULL,"ServerError",
                "error","Cannot open specified certificate file",
                NULL);
    return;
   }

 HTMLMessageHead(fd,200,"WWWOFFLE Certificate Information",
                 NULL);

 if(out_err==-1 || head_only) goto cleanup_return;

 HTMLMessageBody(fd,"CertInfo-Head",
                 "type","real",
                 "name",name,
                 NULL);

 if(out_err==-1) goto cleanup_return;

 for(i=0;crt_list[i];i++) {
   display_certificate(fd,crt_list[i]);
   if(out_err==-1) goto cleanup_return;
 }

 /* Check if certificate is trusted */

 trusted=VerifyCertificates(name,crt_list);

 {
   size_t size=256;
   int try=0,err;
   for(;;) {
     char dn[size];

     if(trusted) {
       if((err=gnutls_x509_crt_get_dn(trusted,dn,&size))) {
	 if(err==GNUTLS_E_SHORT_MEMORY_BUFFER && ++try<2 && size<=MAXDYNBUFSIZE)
	   continue; /* retry with resized buffer */

	 snprintf(dn,size,"*** Error: %s ***",gnutls_strerror(err));
       }
     }
     else
       *dn=0;

     HTMLMessageBody(fd,"CertInfo-Tail",
		     "trustedby",dn,
		     NULL);
     break;
   }
 }

cleanup_return:
 for(i=0;crt_list[i];i++)
   gnutls_x509_crt_deinit(crt_list[i]);
}


/*++++++++++++++++++++++++++++++++++++++
  Display the certificate for one of the trusted certificate authorities.

  int fd The file descriptor to write the output to.

  URL *Url The exact URL given to reach this page, specifies the trusted certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static void CertificatesTrustedPage(int fd,URL *Url)
{
 int i;
 char *name=URLDecodeFormArgs(Url->args);
 size_t bufsize=strlen(name)+1;
 char dn[bufsize];

 for(i=0;i<n_trusted_x509_crts;i++)
   {
    size_t size=bufsize;

    if(!gnutls_x509_crt_get_dn(trusted_x509_crts[i],dn,&size) && !strcmp(name,dn))
      {
       HTMLMessageHead(fd,200,"WWWOFFLE Certificate Information",
                       NULL);
       if(out_err==-1 || head_only) goto cleanup_return;
       HTMLMessageBody(fd,"CertInfo-Head",
                       "type","trusted",
                       "name",name,
                       NULL);

       if(out_err==-1) goto cleanup_return;

       display_certificate(fd,trusted_x509_crts[i]);

       if(out_err!=-1)
	 HTMLMessageBody(fd,"CertInfo-Tail",
			 NULL);

       goto cleanup_return;
      }
   }

 HTMLMessage(fd,500,"WWWOFFLE Certificate Page Error",NULL,"ServerError",
             "error","Cannot find specified certificate",
             NULL);

cleanup_return:
 free(name);
}


/*++++++++++++++++++++++++++++++++++++++
  Load a certificate from a file and display the information about it.

  int fd The file descriptor to write the output to.

  char *certfile The name of the file containing the certificate.

  char *type The type of the certificate.

  char *name The name of the server, fake, real or trusted certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static void load_display_certificate(int fd,char *certfile,char *type,char *name)
{
 gnutls_x509_crt_t crt;

 crt=LoadCertificate(certfile);

 if(!crt)
   {
    HTMLMessage(fd,500,"WWWOFFLE Certificate Page Error",NULL,"ServerError",
                "error","Cannot open specified certificate file",
                NULL);
    return;
   }

 HTMLMessageHead(fd,200,"WWWOFFLE Certificate Information",
                 NULL);
 if(out_err==-1 || head_only) goto cleanup_return;
 HTMLMessageBody(fd,"CertInfo-Head",
                 "type",type,
                 "name",name,
                 NULL);

 if(out_err==-1) goto cleanup_return;

 display_certificate(fd,crt);

 if(out_err!=-1)
   HTMLMessageBody(fd,"CertInfo-Tail",
		   NULL);

cleanup_return:
 gnutls_x509_crt_deinit(crt);
}


/*++++++++++++++++++++++++++++++++++++++
  Display the information about a certificate.

  int fd The file descriptor to write the output to.

  gnutls_x509_crt_t crt The certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static void display_certificate(int fd,gnutls_x509_crt_t crt)
{
 gnutls_datum_t txt={NULL,0};
 char *dn,*issuer_dn,*key_ca,*txtdata;
 time_t activation,expiration;
 char dn_buf[256],issuer_dn_buf[256];
 char activation_str[MAXDATESIZE],expiration_str[MAXDATESIZE];
 char key_algo[80],
   key_usage[sizeof(     "Digital Signature"
		    ", " "Non-repudiation"
		    ", " "Key Encipherment"
		    ", " "Data Encipherment"
		    ", " "Key Agreement"
		    ", " "Key Cert Sign"
		    ", " "CRL Sign"
		    ", " "Encipher Only"
		    ", " "Decipher Only")];
 unsigned int bits,usage,critical;
 int algo;
 int err;

 /* Certificate name */

 RETRY_FUNC_WITH_ALLOCA_BUFFER(gnutls_x509_crt_get_dn,crt,dn_buf,dn);

 /* Issuer's name */

 RETRY_FUNC_WITH_ALLOCA_BUFFER(gnutls_x509_crt_get_issuer_dn,crt,issuer_dn_buf,issuer_dn);

 /* Activation time */

 activation=gnutls_x509_crt_get_activation_time(crt);

 if(activation==-1)
   strcpy(activation_str,"Error");
 else
   RFC822Date_r(activation,1,activation_str);

 /* Expiration time */

 expiration=gnutls_x509_crt_get_expiration_time(crt);

 if(expiration==-1)
   strcpy(expiration_str,"Error");
 else
   RFC822Date_r(expiration,1,expiration_str);

 /* Algorithm type. */

 algo=gnutls_x509_crt_get_pk_algorithm(crt,&bits);

 if(algo<0)
   strcpy(key_algo,"Error");
 else
   snprintf(key_algo,sizeof(key_algo),"%s (%u bits)",gnutls_pk_algorithm_get_name(algo),bits);

 /* Key usage. */

 err=gnutls_x509_crt_get_key_usage(crt,&usage,&critical);

 if(err==GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
    strcpy(key_usage,"Unknown");
 else if(err<0)
    strcpy(key_usage,"Error");
 else
   {
    char *p=key_usage;
    *p=0;

    if(usage&GNUTLS_KEY_DIGITAL_SIGNATURE) p=stpcpy(p,"Digital Signature");
    if(usage&GNUTLS_KEY_NON_REPUDIATION)   p=stpcpy(p>key_usage?stpcpy(p,", "):p,"Non-repudiation");
    if(usage&GNUTLS_KEY_KEY_ENCIPHERMENT)  p=stpcpy(p>key_usage?stpcpy(p,", "):p,"Key Encipherment");
    if(usage&GNUTLS_KEY_DATA_ENCIPHERMENT) p=stpcpy(p>key_usage?stpcpy(p,", "):p,"Data Encipherment");
    if(usage&GNUTLS_KEY_KEY_AGREEMENT)     p=stpcpy(p>key_usage?stpcpy(p,", "):p,"Key Agreement");
    if(usage&GNUTLS_KEY_KEY_CERT_SIGN)     p=stpcpy(p>key_usage?stpcpy(p,", "):p,"Key Cert Sign");
    if(usage&GNUTLS_KEY_CRL_SIGN)          p=stpcpy(p>key_usage?stpcpy(p,", "):p,"CRL Sign");
    if(usage&GNUTLS_KEY_ENCIPHER_ONLY)     p=stpcpy(p>key_usage?stpcpy(p,", "):p,"Encipher Only");
    if(usage&GNUTLS_KEY_DECIPHER_ONLY)     p=stpcpy(p>key_usage?stpcpy(p,", "):p,"Decipher Only");
   }

 /* Certificate authority */

 err=gnutls_x509_crt_get_ca_status(crt,&critical);

 if(err>0)
    key_ca="Yes";
 else if(err==0)
    key_ca="No";
 else if(err==GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
    key_ca="Unknown";
 else
    key_ca="Error";

 /* Formatted certificate */

 if((err=gnutls_x509_crt_print(crt,GNUTLS_X509_CRT_FULL,&txt))<0) {
   PrintMessage(Warning,"Could not get formatted certificate [%s].",gnutls_strerror(err));
   txtdata="*** gnutls certificate formatting failed ***";
 }
 else
   txtdata=(char *)txt.data;

 /* Output the information. */

 HTMLMessageBody(fd,"CertInfo-Body",
                 "dn",dn,
                 "issuer_dn",issuer_dn,
                 "activation",activation_str,
                 "expiration",expiration_str,
                 "key_algo",key_algo,
                 "key_usage",key_usage,
                 "key_ca",key_ca,
                 "info",txtdata,
                 NULL);

 /* Tidy up and exit */

 free(txt.data);
}


#endif /* USE_GNUTLS */
