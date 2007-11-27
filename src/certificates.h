/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/certificates.h 1.8 2007/04/23 09:27:42 amb Exp $

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


#ifndef CERTIFICATES_H
#define CERTIFICATES_H    /*+ To stop multiple inclusions. +*/

#if USE_GNUTLS

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

/*+ The maximum number of certificates that can be loaded from a file. +*/
#define MAXCERTLIST 256

/* In certificates.c */

int LoadTrustedCertificates(void);
int LoadRootCredentials(void);

void FreeLoadedCredentials(void);

gnutls_certificate_credentials_t /*@null@*/ /*@only@*/ GetFakeCredentials(const char *hostname);
gnutls_certificate_credentials_t /*@null@*/ /*@only@*/ GetServerCredentials(const char *hostname);
gnutls_certificate_credentials_t /*@null@*/ /*@only@*/ GetClientCredentials(void);

void FreeCredentials(gnutls_certificate_credentials_t cred);

int PutRealCertificate(gnutls_session_t session,const char *hostname);

gnutls_x509_crt_t /*@null@*/ /*@observer@*/ VerifyCertificates(const char *hostname,gnutls_x509_crt_t *certlist);

gnutls_x509_crt_t /*@only@*/ /*@null@*/ LoadCertificate(const char *filename);
gnutls_x509_crt_t /*@observer@*/ *LoadCertificates(const char *filename,gnutls_x509_crt_t *crtbuf,unsigned int buflen);


#endif /* USE_GNUTLS */

#endif /* CERTIFICATES_H */
