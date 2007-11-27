/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/certificates.c 1.32 2007/07/08 17:52:39 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9c.
  Certificate handling functions.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 2005,06 Andrew M. Bishop
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
#include <errno.h>

#if USE_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#endif

#include "wwwoffle.h"
#include "errors.h"
#include "io.h"
#include "config.h"
#include "certificates.h"


/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif

#if defined(__CYGWIN__)
#define CYGWIN_REPLACE_COLONS(str) {char *_p; for(_p=(str);*_p;++_p) if(*_p==':') *_p='!';}
#else
#define CYGWIN_REPLACE_COLONS(str)
#endif

#if USE_GNUTLS

/*+ The number of bits for Diffie Hellman key exchange. +*/
#define DH_BITS  1024

/*+ The number of bits for RSA private keys. +*/
#define RSA_BITS 512

#define cleanup_return(val) {retval=(val); goto cleanup_return;}


/* Local functions */

static gnutls_certificate_credentials_t /*@only@*/ /*@null@*/ GetCredentials(const char *hostname,int server);

static int CreateCertificate(const char *filename,/*@null@*/ const char *fake_hostname,/*@null@*/ const char *server_hostname,gnutls_x509_privkey_t key);
static int CreatePrivateKey(const char *filename);

static int SaveCertificates(gnutls_x509_crt_t *crt_list,int n_crts,const char *filename);

static gnutls_x509_privkey_t /*@only@*/ LoadPrivateKey(const char *filename);


/*+ A flag to indicate if the gnutls library has been initialised yet. +*/
static int initialised=0;


/*+ The trusted root certificate authority certificates. +*/
gnutls_x509_crt_t /*@null@*/ *trusted_x509_crts=NULL;

/*+ The number of trusted root certificate authority certificates. +*/
int n_trusted_x509_crts=0;


/*+ The WWWOFFLE root certificate authority certificate. +*/
static /*@only@*/ gnutls_x509_crt_t root_crt;

/*+ The WWWOFFLE root certificate authority private key. +*/
static /*@only@*/ gnutls_x509_privkey_t root_privkey;

/*+ The WWWOFFLE Diffie Hellman parameters for key exchange. +*/
static /*@null@*/ gnutls_dh_params_t dh_params=NULL;

/*+ The WWWOFFLE RSA parameters for key exchange. +*/
static /*@null@*/ gnutls_rsa_params_t rsa_params=NULL;


/*++++++++++++++++++++++++++++++++++++++
  Load in the WWWOFFLE root certificate authority certificate and private key.

  int LoadRootCredentials Returns zero if OK or something else in case of an error.
  ++++++++++++++++++++++++++++++++++++++*/

int LoadRootCredentials(void)
{
 int err,tryagain=0;
 struct stat buf;
 time_t activation,expiration,now;

 /* Check that the gnutls library hasn't already been initialised. */

 if(initialised)
    PrintMessage(Fatal,"Loading root credentials can only be performed once.");
 else {
   /* Initialise the gnutls library if it hasn't been done yet. */

   gnutls_global_init();
   initialised=1;
 }

 /* Create the certificates directory if needed */

 if(stat("certificates",&buf))
   {
    PrintMessage(Inform,"Directory '%s' does not exist [%!s]; creating one.","certificates");
    if(mkdir("certificates",(mode_t)ConfigInteger(DirPerm)))
       PrintMessage(Fatal,"Cannot create directory '%s' [%!s]; check permissions of specified directory.","certificates");
   }
 else
    if(!S_ISDIR(buf.st_mode))
       PrintMessage(Fatal,"The file '%s' is not a directory; delete problem file and start WWWOFFLE again to recreate it.","certificates");

 /* Create the certificates/root directory if needed */

 if(stat("certificates/root",&buf))
   {
    PrintMessage(Inform,"Directory '%s' does not exist [%!s]; creating one.","certificates/root");
    if(mkdir("certificates/root",(mode_t)ConfigInteger(DirPerm)))
       PrintMessage(Fatal,"Cannot create directory '%s' [%!s]; check permissions of specified directory.","certificates/root");
   }
 else
    if(!S_ISDIR(buf.st_mode))
      PrintMessage(Fatal,"The file '%s' is not a directory; delete problem file and start WWWOFFLE again to recreate it.","certificates/root");

 /* Read in the root private key in this directory. */

 readagain:

 if(tryagain || stat("certificates/root/root-key.pem",&buf))
   {
    if(!tryagain)
      PrintMessage(Warning,"The WWWOFFLE root CA private key file 'certificates/root/root-key.pem' does not exist; creating it.");

    if(CreatePrivateKey("certificates/root/root-key.pem"))
       PrintMessage(Fatal,"Could not create the WWWOFFLE root CA private key file 'certificates/root/root-key.pem'; check permissions of specified directory.");
   }

 if(stat("certificates/root/root-key.pem",&buf))
    PrintMessage(Fatal,"The WWWOFFLE root CA private key file 'certificates/root/root-key.pem' still does not exist; check permissions of specified directory.");

 if(buf.st_mode&(S_IRWXG|S_IRWXO))
    PrintMessage(Fatal,"The WWWOFFLE root CA private key file 'certificates/root/root-key.pem' is readable by others; delete problem file and start WWWOFFLE again to recreate it.");

 if(!S_ISREG(buf.st_mode))
    PrintMessage(Fatal,"The WWWOFFLE root CA private key file 'certificates/root/root-key.pem' is not a regular file; delete problem file and start WWWOFFLE again to recreate it.");

 root_privkey=LoadPrivateKey("certificates/root/root-key.pem");
 if(!root_privkey)
    PrintMessage(Fatal,"The WWWOFFLE root CA private key file 'certificates/root/root-key.pem' cannot be loaded; delete problem file and start WWWOFFLE again to recreate it.");

 /* Read in the root certificate in this directory. */

 if(tryagain || stat("certificates/root/root-cert.pem",&buf))
   {
    if(!tryagain)
      PrintMessage(Warning,"The WWWOFFLE root CA certificate file 'certificates/root/root-cert.pem' does not exist; creating it.");

    if(CreateCertificate("certificates/root/root-cert.pem",NULL,NULL,root_privkey))
       PrintMessage(Fatal,"Could not create the WWWOFFLE root CA certificate file 'certificates/root/root-cert.pem'; check permissions of specified directory.");
   }

 if(stat("certificates/root/root-cert.pem",&buf))
    PrintMessage(Fatal,"The WWWOFFLE root CA certificate file 'certificates/root/root-cert.pem' still does not exist; check permissions of specified directory.");

 if(!S_ISREG(buf.st_mode))
    PrintMessage(Fatal,"The WWWOFFLE root CA certificate file 'certificates/root/root-cert.pem' is not a regular file; delete problem file and start WWWOFFLE again to recreate it.");

 root_crt=LoadCertificate("certificates/root/root-cert.pem");
 if(!root_crt)
    PrintMessage(Fatal,"The WWWOFFLE root CA certificate file 'certificates/root/root-cert.pem' cannot be loaded; delete problem file and start WWWOFFLE again to recreate it.");

 /* Check for expired root certificate and replace it */

 now=time(NULL);
 activation=gnutls_x509_crt_get_activation_time(root_crt);
 expiration=gnutls_x509_crt_get_expiration_time(root_crt);

 if(activation==-1 || expiration==-1 || activation>now || expiration<now)
   {
     if(++tryagain<2) {
       PrintMessage(Warning,"The WWWOFFLE root CA certificate has expired; replacing it.");

       if(unlink("certificates/root/root-cert.pem"))
	 PrintMessage(Fatal,"Cannot delete the expired WWWOFFLE root CA certificate; check permissions of specified file/directory.");

       if(unlink("certificates/root/root-key.pem"))
	 PrintMessage(Fatal,"Cannot delete the expired WWWOFFLE root CA private key file; check permissions of specified file/directory.");

       goto readagain;
     }
     else
       PrintMessage(Fatal,"Could not create a valid WWWOFFLE root CA certificate after %d attempts.",tryagain);
   }

 /* Initialise the Diffie Hellman and RSA parameters. */

 err=gnutls_dh_params_init(&dh_params);
 if(err<0)
    PrintMessage(Warning,"Could not initialise Diffie Hellman parameters [%s].",gnutls_strerror(err));
 else
   {
    err=gnutls_dh_params_generate2(dh_params,DH_BITS);
    if(err<0)
       PrintMessage(Warning,"Could not generate Diffie Hellman parameters [%s].",gnutls_strerror(err));
   }

 /* RSA parameters generation is slow and not needed if Diffie Hellman is used. */

#if 0
 err=gnutls_rsa_params_init(&rsa_params);
 if(err<0)
    PrintMessage(Warning,"Could not initialise RSA parameters [%s].",gnutls_strerror(err));
 else
   {
    err=gnutls_rsa_params_generate2(rsa_params,RSA_BITS);
    if(err<0)
       PrintMessage(Warning,"Could not generate RSA parameters [%s].",gnutls_strerror(err));
   }
#endif

 if(!dh_params && !rsa_params)
    PrintMessage(Fatal,"Could not create Diffie Hellman or RSA parameters for key exchange; check for earlier gnutls warning messages.");

 return(0);
}    


static void remove_certificate(gnutls_x509_crt_t *cert,const char *reason)
{
  if(StderrLevel>=0 && StderrLevel<=Debug) {
    size_t size=256;
    int try=0,err;
    for(;;) {
      char dn[size];

      if((err=gnutls_x509_crt_get_dn(*cert,dn,&size))) {
	if(err==GNUTLS_E_SHORT_MEMORY_BUFFER && ++try<2 && size<=MAXDYNBUFSIZE)
	  continue; /* retry with resized buffer */

	snprintf(dn,size,"[Can't get certificate name: %s]",gnutls_strerror(err));
      }

      PrintMessage(Debug,"Removed certificate from trusted list%s%s: %s",(reason && *reason)?" because ":"",reason?reason:"",dn);
      break;
    }
  }

  gnutls_x509_crt_deinit(*cert);
  *cert=NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Load in the trusted root certificate authority certificate credentials.

  int LoadTrustedCertificates Returns zero if OK or something else in case of an error.
  ++++++++++++++++++++++++++++++++++++++*/

int LoadTrustedCertificates(void)
{
 struct stat buf;
 DIR *dir;
 struct dirent* ent;

 /* Check that the gnutls library has already been initialised. */

 if(!initialised)
    PrintMessage(Fatal,"Loading root credentials must be the first certificate handling function.");

 /* Create the certificates directory if needed */

 if(stat("certificates",&buf))
   {
    PrintMessage(Inform,"Directory '%s' does not exist [%!s]; creating one.","certificates");
    if(mkdir("certificates",(mode_t)ConfigInteger(DirPerm)))
      {PrintMessage(Warning,"Cannot create directory '%s' [%!s].","certificates");return(1);}
   }
 else
    if(!S_ISDIR(buf.st_mode))
      {PrintMessage(Warning,"The file '%s' is not a directory.","certificates");return(1);}

 /* Create the trusted directory if needed */

 if(stat("certificates/trusted",&buf))
   {
    PrintMessage(Inform,"Directory '%s' does not exist [%!s]; creating one.","certificates/trusted");
    if(mkdir("certificates/trusted",(mode_t)ConfigInteger(DirPerm)))
      {PrintMessage(Warning,"Cannot create directory '%s' [%!s].","certificates/trusted");return(2);}
   }
 else
    if(!S_ISDIR(buf.st_mode))
      {PrintMessage(Warning,"The file '%s' is not a directory.","certificates/trusted");return(2);}

 /* Read in all of the certificates in this directory. */

 dir=opendir("certificates/trusted");
 if(!dir)
   {PrintMessage(Warning,"Cannot open directory 'certificates/trusted' [%!s]; no trusted CA certificates loaded.");return(3);}
 else
   {
    ent=readdir(dir);
    if(!ent)
      {PrintMessage(Warning,"Cannot read directory 'certificates/trusted' [%!s]; no trusted CA certificates loaded.");return(3);}
    else
      {
       do
         {
          if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
             continue; /* skip . & .. */

	  {
	    gnutls_x509_crt_t crtbuf[MAXCERTLIST+1],*crts;
	    char filename[strlen(ent->d_name)+sizeof("certificates/trusted/")];

	    sprintf(filename,"certificates/trusted/%s",ent->d_name);

	    crts=LoadCertificates(filename,crtbuf,MAXCERTLIST+1);

	    if(!crts || !crts[0])
	      PrintMessage(Warning,"Error reading trusted certificate '%s'; skipping this certificate.",filename);
	    else
	      {
		int i;

		i=0;
		while(crts[i])
		  i++;

		trusted_x509_crts=(gnutls_x509_crt_t*)realloc((void*)trusted_x509_crts,(n_trusted_x509_crts+i)*sizeof(gnutls_x509_crt_t));

		i=0;
		while(crts[i])
		  trusted_x509_crts[n_trusted_x509_crts++]=crts[i++];
	      }
	  }
         }
       while((ent=readdir(dir)));
      }

    closedir(dir);
   }

 PrintMessage(Inform,"Read in %d trusted certificates.",n_trusted_x509_crts);

 /* Check the certificates. */

 if(n_trusted_x509_crts)
   {
    unsigned char **key=(unsigned char**)malloc(n_trusted_x509_crts*sizeof(unsigned char*));
    time_t now;
    int i,j;

    /* Remove the duplicates based on the public key id. */

    for(i=0;i<n_trusted_x509_crts;i++)
      {
       size_t key_size;
       int err;

       key_size=40;
       key[i]=(unsigned char*)malloc(key_size);

       err=gnutls_x509_crt_get_key_id(trusted_x509_crts[i],0,key[i],&key_size);
       if(err==GNUTLS_E_SHORT_MEMORY_BUFFER)
         {
          key[i]=(unsigned char*)realloc((void*)key[i],key_size);
          err=gnutls_x509_crt_get_key_id(trusted_x509_crts[i],0,key[i],&key_size);
         }

       if(err<0)
         {
          free(key[i]);
          key[i]=NULL;
          remove_certificate(&trusted_x509_crts[i],"gnutls could not get public key id");
         }
       else
         {
          if(key_size<40)
             memset(key[i]+key_size,0,40-key_size);

          for(j=0;j<i;j++)
             if(key[j] && !memcmp(key[i],key[j],40))
               {
                free(key[i]);
                key[i]=NULL;
                remove_certificate(&trusted_x509_crts[i],"the public key id is identical to that of an earlier certificate");
                break;
               }
         }
      }

    for(i=0;i<n_trusted_x509_crts;i++)
       if(key[i])
          free(key[i]);

    free(key);

    /* Check for expired trusted certificates */

    now=time(NULL);

    for(i=0;i<n_trusted_x509_crts;i++)
       if(trusted_x509_crts[i])
         {
          time_t activation=gnutls_x509_crt_get_activation_time(trusted_x509_crts[i]);
          time_t expiration=gnutls_x509_crt_get_expiration_time(trusted_x509_crts[i]);
	  char *reason=NULL;

          if((activation==-1 && (reason="gnutls could not get activation date")) ||
	     (expiration==-1 && (reason="gnutls could not get expiration date")) ||
	     (activation>now && (reason="the certificate is not yet activated")) ||
	     (expiration<now && (reason="the certificate has expired")))
            {
	      remove_certificate(&trusted_x509_crts[i],reason);
            }
         }

    /* Remove the empty spaces in the array. */

    for(i=0;i<n_trusted_x509_crts;i++) {
      if(!trusted_x509_crts[i]) {
	for(j=i++;i<n_trusted_x509_crts;i++)
	  if(trusted_x509_crts[i])
	    trusted_x509_crts[j++]=trusted_x509_crts[i];

	n_trusted_x509_crts=j;
	trusted_x509_crts=(gnutls_x509_crt_t*)realloc((void*)trusted_x509_crts,n_trusted_x509_crts*sizeof(gnutls_x509_crt_t));
	break;
      }
    }

    PrintMessage(Inform,"There are %d unique, valid, trusted certificates.",n_trusted_x509_crts);
   }

 /* FIXME : We don't handle trusted CRLs */

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Free the certificates and private keys that have been loaded.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeLoadedCredentials(void)
{
 int i;

 if(!initialised)
    return;

 if(trusted_x509_crts) {
   for(i=0;i<n_trusted_x509_crts;i++)
     gnutls_x509_crt_deinit(trusted_x509_crts[i]);

   free(trusted_x509_crts);
 }

 if(root_crt)
    gnutls_x509_crt_deinit(root_crt);

 if(root_privkey)
    gnutls_x509_privkey_deinit(root_privkey);

 if(dh_params)
    gnutls_dh_params_deinit(dh_params);

 if(rsa_params)
    gnutls_rsa_params_deinit(rsa_params);

 gnutls_global_deinit();
}


/*++++++++++++++++++++++++++++++++++++++
  Get a fake certificate and private key for a host.

  gnutls_certificate_credentials_t GetFakeCredentials Returns the credentials or NULL on error.

  const char *hostname The hostname to load or create the credentials for.
  ++++++++++++++++++++++++++++++++++++++*/

gnutls_certificate_credentials_t GetFakeCredentials(const char *hostname)
{
 /* Check that the gnutls library has already been initialised. */

 if(!initialised)
    PrintMessage(Fatal,"Loading root credentials must be the first certificate handling function.");

 /* Return the credentials */

 return(GetCredentials(hostname,0));
}


/*++++++++++++++++++++++++++++++++++++++
  Get a certificate and private key for a server host.

  gnutls_certificate_credentials_t GetServerCredentials Returns the credentials or NULL on error.

  const char *hostname The hostname to load or create the credentials for.
  ++++++++++++++++++++++++++++++++++++++*/

gnutls_certificate_credentials_t GetServerCredentials(const char *hostname)
{
 /* Check that the gnutls library has already been initialised. */

 if(!initialised)
    PrintMessage(Fatal,"Loading root credentials must be the first certificate handling function.");

 /* Return the credentials */

 return(GetCredentials(hostname,1));
}


/*++++++++++++++++++++++++++++++++++++++
  Get a set of credentials for a client to use.

  gnutls_certificate_credentials_t GetClientCredentials Returns the credentials or NULL on error.
  ++++++++++++++++++++++++++++++++++++++*/

gnutls_certificate_credentials_t GetClientCredentials(void)
{
 gnutls_certificate_credentials_t cred;
 int err;

 /* Check that the gnutls library has already been initialised. */

 if(!initialised)
    PrintMessage(Fatal,"Loading root credentials must be the first certificate handling function.");

 /* Create a set of initialised, but empty credentials. */

 err=gnutls_certificate_allocate_credentials(&cred);
 if(err<0)
   {PrintMessage(Warning,"Could not allocate credentials for client [%s].",gnutls_strerror(err));return(NULL);}

 return(cred);
}


/*++++++++++++++++++++++++++++++++++++++
  Free a set of credentials from the GetCredentials() function.

  gnutls_certificate_credentials_t cred The credentials to destroy.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeCredentials(gnutls_certificate_credentials_t cred)
{
 if(!initialised)
    return;

 gnutls_certificate_free_credentials(cred);
}


/*++++++++++++++++++++++++++++++++++++++
  Store the real server's certificate.

  int PutRealCertificate Returns 0 if OK.

  gnutls_session_t session The session information.

  const char *hostname The name and port number of the host being connected to.
  ++++++++++++++++++++++++++++++++++++++*/

int PutRealCertificate(gnutls_session_t session,const char *hostname)
{
 struct stat buf;
 const gnutls_datum_t *raw_crt_list;
 gnutls_x509_crt_t *crt_list;
 unsigned int n_crts,i;
 int err,retval;

 /* Create the certificates/real directory if needed */

 if(stat("certificates/real",&buf))
   {
    PrintMessage(Inform,"Directory '%s' does not exist [%!s]; creating one.","certificates/real");
    if(mkdir("certificates/real",(mode_t)ConfigInteger(DirPerm)))
      {PrintMessage(Warning,"Cannot create directory '%s' [%!s].","certificates/real");return(2);}
   }
 else
    if(!S_ISDIR(buf.st_mode))
      {PrintMessage(Warning,"The file '%s' is not a directory.","certificates/real");return(2);}

 /* Get the certificate chain from the server. */

 raw_crt_list=gnutls_certificate_get_peers(session,&n_crts);
 if(!raw_crt_list)
   {PrintMessage(Warning,"Could not get peer's certificate.");return(1);}

 crt_list=(gnutls_x509_crt_t*)malloc(n_crts*sizeof(gnutls_x509_crt_t));

 for(i=0;i<n_crts;i++)
   {
    err=gnutls_x509_crt_init(&crt_list[i]);
    if(err<0)
      {PrintMessage(Warning,"Could not initialise certificate [%s].",gnutls_strerror(err));n_crts=i;cleanup_return(3);}

    err=gnutls_x509_crt_import(crt_list[i],&raw_crt_list[i],GNUTLS_X509_FMT_DER);
    if(err<0)
      {PrintMessage(Warning,"Could not import certificate [%s].",gnutls_strerror(err));n_crts=i+1;cleanup_return(3);}
   }

 /* Write out the certificates */
 {
   char filename[strlen(hostname)+sizeof("certificates/real/-cert.pem")];

   sprintf(filename,"certificates/real/%s-cert.pem",hostname);
   CYGWIN_REPLACE_COLONS(filename);
   retval=SaveCertificates(crt_list,n_crts,filename);
 }

cleanup_return:
 for(i=0;i<n_crts;i++)
    gnutls_x509_crt_deinit(crt_list[i]);

 free(crt_list);

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Verify the certificates for a host against the trusted certificates.

  gnutls_x509_crt_t VerifyCertificates Returns the trusted certificate that validates the host.

  const char *hostname The name of the host to check.

  gnutls_x509_crt_t *certlist The (NULL-terminated) list of certificates for the host to check.
  ++++++++++++++++++++++++++++++++++++++*/

gnutls_x509_crt_t VerifyCertificates(const char *hostname,gnutls_x509_crt_t *certlist)
{
 int i=0;
 unsigned int result;
 time_t activation,expiration,now;
 gnutls_x509_crt_t cert,issuer;

 if(!certlist)
    return(NULL);

 /* Check the first certificate in the chain */

 cert=*certlist;

 if(!cert)
    return(NULL);

 if(!gnutls_x509_crt_check_hostname(cert,hostname))
   {PrintMessage(Debug,"The owner of the first certificate in the list does not match hostname '%s'.",hostname);return NULL;}

 now=time(NULL);
 activation=gnutls_x509_crt_get_activation_time(cert);
 expiration=gnutls_x509_crt_get_expiration_time(cert);

 if(activation==-1 || expiration==-1 || activation>now || expiration<now)
   {PrintMessage(Debug,"Certificate is expired for first certificate in list.");return NULL;}

 /* Check the certificates in the chain */

 while((issuer=certlist[++i]))
   {
    activation=gnutls_x509_crt_get_activation_time(issuer);
    expiration=gnutls_x509_crt_get_expiration_time(issuer);

    if(activation==-1 || expiration==-1 || activation>now || expiration<now)
      {PrintMessage(Debug,"Certificate is expired for certificate %d in list.",i+1);return NULL;}

    /* Allow the relatively insecure MD5 and MD2 algorithms. */
    gnutls_x509_crt_verify(cert,&issuer,1,GNUTLS_VERIFY_ALLOW_SIGN_RSA_MD5|
                                          GNUTLS_VERIFY_ALLOW_SIGN_RSA_MD2,&result);

    if(result&GNUTLS_CERT_INVALID)
      {PrintMessage(Debug,"Certificate is not verified for certificate %d in list [%d].",i+1,result);return NULL;}

    cert=issuer;
   }

 /* Check the last certificate in the chain. */

 if(i>1 && gnutls_x509_crt_check_issuer(cert,cert)>0)
    cert=certlist[i-2];

 for(i=0;i<n_trusted_x509_crts;i++)
   {
    /* Allow the relatively insecure MD5 and MD2 algorithms. */
    gnutls_x509_crt_verify(cert,&trusted_x509_crts[i],1,GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT|
                                                        GNUTLS_VERIFY_ALLOW_SIGN_RSA_MD5|
                                                        GNUTLS_VERIFY_ALLOW_SIGN_RSA_MD2,&result);

    if(!(result&GNUTLS_CERT_INVALID))
      {
       return trusted_x509_crts[i];
      }
   }

 PrintMessage(Debug,"Certificate is not trusted by any in the trusted list.");

 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Get a certificate and private key for a server or fake host.

  gnutls_certificate_credentials_t GetCredentials Returns the credentials or NULL on error.

  const char *hostname The hostname to load or create the credentials for.

  int server Set to 1 if this is for a server, 0 if it is for fake credentials.
  ++++++++++++++++++++++++++++++++++++++*/

static gnutls_certificate_credentials_t GetCredentials(const char *hostname,int server)
{
 struct stat buf;
 gnutls_certificate_credentials_t cred=NULL;
 gnutls_x509_crt_t crt=NULL;
 gnutls_x509_privkey_t privkey=NULL;
 char *dirname;
 int err,count;

 /* Create the certificates/server or certificates/fake directory if needed */

 if(server)
    dirname="certificates/server";
 else
    dirname="certificates/fake";

 if(stat(dirname,&buf))
   {
    PrintMessage(Inform,"Directory '%s' does not exist [%!s]; creating one.",dirname);
    if(mkdir(dirname,(mode_t)ConfigInteger(DirPerm)))
      {PrintMessage(Warning,"Cannot create directory '%s' [%!s].",dirname);return(NULL);}
   }
 else
    if(!S_ISDIR(buf.st_mode))
      {PrintMessage(Warning,"The file '%s' is not a directory.",dirname);return(NULL);}

 {
   time_t activation,expiration,now;
   unsigned int result; int tryagain=0;
   char keyfilename[strlen(hostname)+sizeof("certificates/server/-key.pem")];
   char crtfilename[strlen(hostname)+sizeof("certificates/server/-cert.pem")];

   /* Calculate the filenames for the key and certificate. */

   sprintf(keyfilename,"%s/%s-key.pem",dirname,hostname);
   CYGWIN_REPLACE_COLONS(keyfilename);
   sprintf(crtfilename,"%s/%s-cert.pem",dirname,hostname);
   CYGWIN_REPLACE_COLONS(crtfilename);

   /* Read in the private key in this directory. */

 readagain:

   if(tryagain || stat(keyfilename,&buf))
     {
       if(!tryagain)
	 PrintMessage(Warning,"The WWWOFFLE private key file '%s' does not exist; creating it.",keyfilename);

       if(CreatePrivateKey(keyfilename))
	 {PrintMessage(Warning,"Could not create the WWWOFFLE private key file '%s'.",keyfilename);goto finished;}
     }

   count=0;
   while(!stat(keyfilename,&buf) && buf.st_size==0)
     {
       /* File exists but is zero size.  Wait up to a minute for it to be created.  Secret key creation can be slow. */

       sleep(10);
       if(++count==6)
	 break;
     }

   if(stat(keyfilename,&buf) || buf.st_size==0)
     {PrintMessage(Warning,"The WWWOFFLE private key file '%s' still does not exist or is empty.",keyfilename);goto finished;}

   if(buf.st_mode&(S_IRWXG|S_IRWXO) || !S_ISREG(buf.st_mode))
     {PrintMessage(Warning,"The WWWOFFLE private key file '%s' is not a regular file or is readable by others.",keyfilename);goto finished;}

   if(privkey) gnutls_x509_privkey_deinit(privkey);
   privkey=LoadPrivateKey(keyfilename);
   if(!privkey)
     {PrintMessage(Warning,"The WWWOFFLE private key file '%s' cannot be loaded.",keyfilename);goto finished;}

   /* Read in the certificate in this directory. */

   if(tryagain || stat(crtfilename,&buf))
     {
       if(!tryagain)
	 PrintMessage(Warning,"The WWWOFFLE certificate file '%s' does not exist; creating it.",crtfilename);

       if(server)
	 err=CreateCertificate(crtfilename,NULL,hostname,privkey);
       else
	 err=CreateCertificate(crtfilename,hostname,NULL,privkey);

       if(err)
	 {PrintMessage(Warning,"Could not create the WWWOFFLE certificate file '%s'.",crtfilename);goto finished;}
     }

   if(!stat(crtfilename,&buf) && buf.st_size==0)
     {
       /* File exists but is zero size.  Wait up to 10 seconds for it to be created.  Certificate creation is fast. */

       sleep(10);
     }

   if(stat(crtfilename,&buf) || buf.st_size==0)
     {PrintMessage(Warning,"The WWWOFFLE certificate file '%s' still does not exist or is empty.",crtfilename);goto finished;}

   if(!S_ISREG(buf.st_mode))
     {PrintMessage(Warning,"The WWWOFFLE certificate file '%s' is not a regular file.",crtfilename);goto finished;}

   if(crt) gnutls_x509_crt_deinit(crt);
   crt=LoadCertificate(crtfilename);
   if(!crt)
     {PrintMessage(Warning,"The WWWOFFLE certificate file '%s' cannot be loaded.",crtfilename);goto finished;}

   /* Check for expired certificate and replace it */

   now=time(NULL);
   activation=gnutls_x509_crt_get_activation_time(crt);
   expiration=gnutls_x509_crt_get_expiration_time(crt);

   if(activation==-1 || expiration==-1 || activation>now || expiration<now)
     {
       if(++tryagain<2) {
	 PrintMessage(Warning,"The WWWOFFLE %s certificate file for '%s' has expired; replacing it.",server?"server":"fake",hostname);

	 if(unlink(crtfilename))
	   {PrintMessage(Warning,"Cannot delete the expired WWWOFFLE %s certificate file for '%s'.",server?"server":"fake",hostname); goto finished;}

	 if(unlink(keyfilename))
	   {PrintMessage(Warning,"Cannot delete the expired WWWOFFLE %s private key file for '%s'.",server?"server":"fake",hostname); goto finished;}

	 goto readagain;
       }
       else
	 PrintMessage(Fatal,"Could not create a valid WWWOFFLE %s certificate file for '%s' after %d attempts.",server?"server":"fake",hostname,tryagain);
     }

   /* Check that the CA for the certificate is the current root CA or replace it */

   gnutls_x509_crt_verify(crt,&root_crt,1,0,&result);

   if(result&GNUTLS_CERT_INVALID)
     {
       if(++tryagain<2) {
	 PrintMessage(Warning,"The WWWOFFLE %s certificate file for '%s' has the wrong issuer; replacing it.",server?"server":"fake",hostname);

	 if(unlink(crtfilename))
	   {PrintMessage(Warning,"Cannot delete the expired WWWOFFLE %s certificate file for '%s'.",server?"server":"fake",hostname); goto finished;}

	 if(unlink(keyfilename))
	   {PrintMessage(Warning,"Cannot delete the expired WWWOFFLE %s private key file for '%s'.",server?"server":"fake",hostname); goto finished;}

	 goto readagain;
       }
       else
	 PrintMessage(Fatal,"Could not create a valid WWWOFFLE %s certificate file for '%s' after %d attempts.",server?"server":"fake",hostname,tryagain);
     }
 }

 /* Create the credentials from the certificate, private key and Diffie Hellman parameters. */

 err=gnutls_certificate_allocate_credentials(&cred);
 if(err<0)
   {PrintMessage(Warning,"Could not allocate %s credentials for '%s' [%s].",server?"server":"fake",hostname,gnutls_strerror(err));cred=NULL;goto finished;}

 err=gnutls_certificate_set_x509_key(cred,&crt,1,privkey);
 if(err<0)
   {PrintMessage(Warning,"Could not set private key for %s credentials for '%s' [%s].",server?"server":"fake",hostname,gnutls_strerror(err));gnutls_certificate_free_credentials(cred);cred=NULL;goto finished;}

 if(dh_params)
    gnutls_certificate_set_dh_params(cred,dh_params);

 if(rsa_params)
    gnutls_certificate_set_rsa_export_params(cred,rsa_params);

 /* Finished so cleanup and return */

 finished:

 if(crt)
    gnutls_x509_crt_deinit(crt);

 if(privkey)
    gnutls_x509_privkey_deinit(privkey);

 return(cred);
}


/*++++++++++++++++++++++++++++++++++++++
  Create and save a certificate signed by the WWWOFFLE root CA private key.

  int CreateCertificate Returns zero if OK or something else in case of an error. 

  const char *filename The name of the file to save the certificate in.

  const char *fake_hostname The name of the host that this certificate is faked for (or NULL if faking host).

  const char *server_hostname The name of the WWWOFFLE server that this certificate is for (or NULL if creating server).

  gnutls_x509_privkey_t key The private key that is associated with this certificate.
  ++++++++++++++++++++++++++++++++++++++*/

static int CreateCertificate(const char *filename,const char *fake_hostname,const char *server_hostname,gnutls_x509_privkey_t key)
{
 gnutls_x509_crt_t crt;
 unsigned char buffer[4*RSA_BITS]; /* works for 256 bit keys or longer. */
 size_t buffer_size;
 int fd,err,retval=0;
 const char *errmsg_hostname;

 if(fake_hostname)
    errmsg_hostname=fake_hostname;
 else if(server_hostname)
    errmsg_hostname=server_hostname;
 else
    errmsg_hostname="WWWOFFLE";

 /* Create the file for the certificate. */

 fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC|O_EXCL|O_BINARY,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
 if(fd<0)
   {PrintMessage(Warning,"Could not open certificate file '%s' for writing [%!s].",filename);return(-1);}
 close(fd);

 /* Create the certificate. */

 err=gnutls_x509_crt_init(&crt);
 if(err<0)
   {PrintMessage(Warning,"Could not initialise certificate for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));return(1);}

 /* Set the subject information */

 err=gnutls_x509_crt_set_dn_by_oid(crt,GNUTLS_OID_X520_ORGANIZATION_NAME,0,"WWWOFFLE",8);
 if(err<0)
   {PrintMessage(Warning,"Could not set the certificate organization name for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(2);}

 if(fake_hostname)
    err=gnutls_x509_crt_set_dn_by_oid(crt,GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME,0,"Fake Certificate",16);
 else if(server_hostname)
    err=gnutls_x509_crt_set_dn_by_oid(crt,GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME,0,"Server Certificate",18);
 else
    err=gnutls_x509_crt_set_dn_by_oid(crt,GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME,0,"Certificate Authority",21);

 if(err<0)
   {PrintMessage(Warning,"Could not set the certificate organization unit name for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(3);}

 if(fake_hostname)
    err=gnutls_x509_crt_set_dn_by_oid(crt,GNUTLS_OID_X520_COMMON_NAME,0,fake_hostname,strlen(fake_hostname));
 else if(server_hostname)
    err=gnutls_x509_crt_set_dn_by_oid(crt,GNUTLS_OID_X520_COMMON_NAME,0,server_hostname,strlen(server_hostname));
 else
    err=gnutls_x509_crt_set_dn_by_oid(crt,GNUTLS_OID_X520_COMMON_NAME,0,"WWWOFFLE",8);

 if(err<0)
   {PrintMessage(Warning,"Could not set the certificate common name for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(4);}

 /* Set the key usage */

 if(fake_hostname || server_hostname)
   {
    err=gnutls_x509_crt_set_key_usage(crt,GNUTLS_KEY_KEY_ENCIPHERMENT);
    if(err<0)
      {PrintMessage(Warning,"Could not set the certificate for encipherment for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(5);}

    err=gnutls_x509_crt_set_ca_status(crt,0);
    if(err<0)
      {PrintMessage(Warning,"Could not set the certificate as not a CA for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(6);}
   }
 else
   {
    err=gnutls_x509_crt_set_key_usage(crt,GNUTLS_KEY_KEY_CERT_SIGN);
    if(err<0)
      {PrintMessage(Warning,"Could not set the certificate for signing for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(7);}

    err=gnutls_x509_crt_set_ca_status(crt,1);
    if(err<0)
      {PrintMessage(Warning,"Could not set the certificate as a CA for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(8);}
   }

 /* Set the key serial number (unique) */

 sprintf((char*)buffer,"%08lx%08lx",(unsigned long)time(NULL),(unsigned long)getpid());

 err=gnutls_x509_crt_set_serial(crt,buffer,strlen((char*)buffer));
 if(err<0)
   {PrintMessage(Warning,"Could not set the certificate serial number for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(9);}

 /* Set the request version */

 err=gnutls_x509_crt_set_version(crt,1);
 if(err<0)
   {PrintMessage(Warning,"Could not set the certificate version for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(10);}

 /* Set the activation and expiration times */

 err=gnutls_x509_crt_set_activation_time(crt,time(NULL));
 if(err<0)
   {PrintMessage(Warning,"Could not set the certificate activation time for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(11);}

 err=gnutls_x509_crt_set_expiration_time(crt,time(NULL)+365*24*3600);
 if(err<0)
   {PrintMessage(Warning,"Could not set the certificate expiration time for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(12);}

 /* Associate the request with the private key */

 err=gnutls_x509_crt_set_key(crt,key);
 if(err<0)
   {PrintMessage(Warning,"Could not set the certificate key for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(13);}

 buffer_size=sizeof(buffer);

 err=gnutls_x509_privkey_get_key_id(key,0,buffer,&buffer_size);
 if(err==0)
    err=gnutls_x509_crt_set_subject_key_id(crt,(void *)buffer,buffer_size);

 if(err<0)
   {PrintMessage(Warning,"Could not set the certificate subject key for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(14);}

 /* Sign the certificate */

 if(fake_hostname || server_hostname)
    err=gnutls_x509_crt_sign(crt,root_crt,root_privkey);
 else
    err=gnutls_x509_crt_sign(crt,crt,key);

 if(err<0)
   {PrintMessage(Warning,"Could not sign the certificate for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(15);}

 /* Export the certificate */

 buffer_size=sizeof(buffer);

 err=gnutls_x509_crt_export(crt,GNUTLS_X509_FMT_PEM,buffer,&buffer_size);
 if(err<0)
   {PrintMessage(Warning,"Could not export the certificate for '%s' [%s].",errmsg_hostname,gnutls_strerror(err));cleanup_return(16);}

 /* Save it to a file. */

 fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
 if(fd<0)
   {PrintMessage(Warning,"Could not open certificate file '%s' for writing [%!s].",filename);cleanup_return(-2);}

 if(write_all(fd,(char*)buffer,buffer_size)!=buffer_size)
   {PrintMessage(Warning,"Could not write certificate file '%s' [%!s].",filename);retval=-2;}
 if(close(fd))
   {PrintMessage(Warning,"Could not close certificate file '%s' after writing [%!s].",filename);retval=-2;}

cleanup_return:
 gnutls_x509_crt_deinit(crt);
 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Create and save a private key.

  int CreatePrivateKey Returns zero if OK or something else in case of an error.

  const char *filename The name of the file to save the private key in.
  ++++++++++++++++++++++++++++++++++++++*/

static int CreatePrivateKey(const char *filename)
{
 gnutls_x509_privkey_t privkey;
 unsigned char buffer[2*RSA_BITS]; /* works for 256 bit keys or longer. */
 size_t buffer_size=sizeof(buffer);
 int fd,err,retval=0;

 /* Create the file for the certificate. */

 fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC|O_EXCL|O_BINARY,S_IRUSR|S_IWUSR);
 if(fd<0)
   {PrintMessage(Warning,"Could not open private key file '%s' for writing [%!s].",filename);return(-1);}
 close(fd);

 PrintMessage(Inform,"Creating private key, this may take a long time.");

 /* Create the private key. */

 err=gnutls_x509_privkey_init(&privkey);
 if(err<0)
   {PrintMessage(Warning,"Could not initialise private key [%s].",gnutls_strerror(err));return(1);}

 err=gnutls_x509_privkey_generate(privkey,GNUTLS_PK_RSA,RSA_BITS,0);
 if(err<0)
   {PrintMessage(Warning,"Could not generate private key [%s].",gnutls_strerror(err));cleanup_return(2);}

 /* Export the key */

 err=gnutls_x509_privkey_export(privkey,GNUTLS_X509_FMT_PEM,buffer,&buffer_size);
 if(err<0)
   {PrintMessage(Warning,"Could not export private key [%s].",gnutls_strerror(err));cleanup_return(3);}

 /* Save it to a file. */

 fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,S_IRUSR|S_IWUSR);
 if(fd<0)
   {PrintMessage(Warning,"Could not open private key file '%s' for writing [%!s].",filename);cleanup_return(-2);}

 if(write_all(fd,(char*)buffer,buffer_size)!=buffer_size)
   {PrintMessage(Warning,"Could not write private key file '%s' [%!s].",filename);retval=-2;}

 if(close(fd))
   {PrintMessage(Warning,"Could not close private key file '%s' after writing [%!s].",filename);retval=-2;}

cleanup_return:
 gnutls_x509_privkey_deinit(privkey);
 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Load in a single certificate from a file (the first if multiple).

  gnutls_x509_crt_t LoadCertificate Returns the loaded certificate.

  const char *filename The name of the file to load the certificate from.
  ++++++++++++++++++++++++++++++++++++++*/

gnutls_x509_crt_t LoadCertificate(const char *filename)
{
 gnutls_x509_crt_t crtbuf[MAXCERTLIST+1],*crt_list;
 int i=0;

 crt_list=LoadCertificates(filename,crtbuf,MAXCERTLIST+1);

 if(!crt_list || !crt_list[0])
    return(NULL);

 while(crt_list[++i])
    gnutls_x509_crt_deinit(crt_list[i]);

 return(crt_list[0]);
}


/*++++++++++++++++++++++++++++++++++++++
  Load in a set of certificates from a file.

  gnutls_x509_crt_t *LoadCertificates Returns the loaded certificates (NULL terminated list).

  const char *filename The name of the file to load the certificates from.
  ++++++++++++++++++++++++++++++++++++++*/

gnutls_x509_crt_t *LoadCertificates(const char *filename,gnutls_x509_crt_t *crtbuf,unsigned int buflen)
{
 unsigned int n_crt;
 int fd,err;
 struct stat buf;
 unsigned char *buffer;
 size_t buffer_size;
 gnutls_datum_t datum;
 gnutls_x509_crt_t *retval=NULL;

 if(!buflen)
   return NULL;

 /* Load the certificates from the file. */

 fd=open(filename,O_RDONLY|O_BINARY);
 if(fd<0)
   {PrintMessage(Warning,"Could not open certificate file '%s' for reading [%!s].",filename);return(NULL);}

 if(fstat(fd,&buf))
   {PrintMessage(Warning,"Could not determine length of certificate file '%s' [%!s].",filename); goto close_return;}

 buffer_size=buf.st_size;
 buffer=(unsigned char*)malloc(buffer_size);
 if(!buffer)
   {PrintMessage(Warning,"Could not allocate buffer for certificate file '%s'.",filename); goto close_return;}

 if(read_all(fd,(char*)buffer,buffer_size)!=buffer_size)
   {PrintMessage(Warning,"Could not read certificate file '%s' [%!s].",filename); goto cleanup_return;}

 datum.data=buffer;
 datum.size=buffer_size;
 n_crt=buflen-1;

 err=gnutls_x509_crt_list_import(crtbuf,&n_crt,&datum,GNUTLS_X509_FMT_PEM,GNUTLS_X509_CRT_LIST_IMPORT_FAIL_IF_EXCEED);
 if(err<0)
   {PrintMessage(Warning,"Could not import certificates [%s].",gnutls_strerror(err)); goto cleanup_return;}

 crtbuf[err]=NULL;
 retval=crtbuf;

cleanup_return:
 free(buffer);
close_return:
 close(fd);

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Save a set of certificates to a file.

  int SaveCertificates Return 0 if OK or another value if an error.

  gnutls_x509_crt_t *crt_list The certificates to save.

  int n_crts The number of certificates in the list.

  const char *filename The name of the file to save the certificates to.
  ++++++++++++++++++++++++++++++++++++++*/

static int SaveCertificates(gnutls_x509_crt_t *crt_list,int n_crts,const char *filename)
{
  int fd,i;

 /* Save the certificates to the file. */

 fd=open(filename,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
 if(fd<0)
   {PrintMessage(Warning,"Could not open certificate file '%s' for writing [%!s].",filename);return(1);}

 for(i=0;i<n_crts;i++)
   {
     size_t buffer_size=4096;
     int try=0,err;

     do {
       char buffer[buffer_size];
       if(!(err=gnutls_x509_crt_export(crt_list[i],GNUTLS_X509_FMT_PEM,buffer,&buffer_size)))
	 if(write_all(fd,buffer,buffer_size)!=buffer_size)
	   {PrintMessage(Warning,"Could not write certificate to file '%s' [%!s].",filename);close(fd);return(3);}
     }
     while(err==GNUTLS_E_SHORT_MEMORY_BUFFER && ++try<2 && buffer_size<=MAXDYNBUFSIZE);

     if(err)
       {PrintMessage(Warning,"Could not export certificate [%s].",gnutls_strerror(err));close(fd);return(2);}
   }

 if(close(fd))
   {PrintMessage(Warning,"Could not close file '%s' after writing certificate [%!s].",filename);return(3);}

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Load in a private key from a file.

  gnutls_x509_privkey_t LoadPrivateKey Returns the loaded private key.

  const char *filename The name of the file to load the private key from.
  ++++++++++++++++++++++++++++++++++++++*/

static gnutls_x509_privkey_t LoadPrivateKey(const char *filename)
{
 gnutls_x509_privkey_t privkey,retval=NULL;
 struct stat buf;
 unsigned char *buffer;
 size_t buffer_size;
 gnutls_datum_t datum;
 int fd,err;

 /* Load the private key from the file. */

 fd=open(filename,O_RDONLY|O_BINARY);
 if(fd<0)
   {PrintMessage(Warning,"Could not open private key file '%s' for reading [%!s].",filename);return(NULL);}

 if(fstat(fd,&buf))
   {PrintMessage(Warning,"Could not determine length of private key file '%s' [%!s].",filename); goto close_return;}

 buffer_size=buf.st_size;
 buffer=(unsigned char*)malloc(buffer_size);
 if(!buffer)
   {PrintMessage(Warning,"Could not allocate buffer for private key file '%s'.",filename); goto close_return;}

 if(read_all(fd,(char*)buffer,buffer_size)!=buffer_size)
   {PrintMessage(Warning,"Could not read private key file '%s' [%!s].",filename); goto cleanup_return;}

 datum.data=buffer;
 datum.size=buffer_size;

 err=gnutls_x509_privkey_init(&privkey);
 if(err<0)
   {PrintMessage(Warning,"Could not initialise private key [%s].",gnutls_strerror(err)); goto cleanup_return;}

 err=gnutls_x509_privkey_import(privkey,&datum,GNUTLS_X509_FMT_PEM);
 if(err<0) {
   PrintMessage(Warning,"Could not import private key [%s].",gnutls_strerror(err));
   gnutls_x509_privkey_deinit(privkey);
 }
 else
   retval=privkey;

cleanup_return:
 free(buffer);
close_return:
 close(fd);

 return(retval);
}

#endif /* USE_GNUTLS */
