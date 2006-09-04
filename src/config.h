/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/config.h 2.103 2004/11/22 19:13:55 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8e.
  Configuration file management functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef CONFIG_H
#define CONFIG_H    /*+ To stop multiple inclusions. +*/

#include "misc.h"

#ifndef CONFIGPRIV_H

/*+ A description of an entry in a section of the config file. +*/
typedef struct _ConfigItem *ConfigItem;

#endif

/* Global functions for accessing the configuration file information. */

char /*@observer@*/ *ConfigurationFileName(void);
void InitConfigurationFile(/*@null@*/ char *name);
void FinishConfigurationFile(void);
int ReadConfigurationFile(int fd);

void DumpConfigFile(int fd);

int ConfigInteger(/*@null@*/ ConfigItem item);
#define ConfigBoolean(item) (ConfigInteger(item)!=0)
/*@observer@*/ /*@null@*/ char *ConfigString(/*@null@*/ ConfigItem item);

int ConfigIntegerURL(/*@null@*/ ConfigItem item,/*@null@*/ URL *Url);
int ConfigIntegerProtoHostPort(/*@null@*/ ConfigItem item,char *proto,char *hostport);
#define ConfigBooleanURL(item,Url) (ConfigIntegerURL(item,Url)!=0)
#define ConfigBooleanProtoHostPort(item,proto,hostport) (ConfigIntegerProtoHostPort(item,proto,hostport)!=0)
/*@observer@*/ /*@null@*/ char *ConfigStringURL(/*@null@*/ ConfigItem item,/*@null@*/ URL *Url);
/*@observer@*/ /*@null@*/ char *ConfigStringProtoHostPort(/*@null@*/ ConfigItem item,char *proto,char *hostport);

int ConfigBooleanMatchURL(/*@null@*/ ConfigItem item,URL *Url);
int ConfigBooleanMatchProtoHostPort(/*@null@*/ ConfigItem item,char *proto,char *hostport);
char *ConfigHeaderMatch(ConfigItem item, URL *Url, Header *header);


/* StartUp section */

/*+ The IP address to bind for IPv4. +*/
extern /*@null@*/ ConfigItem Bind_IPv4;

/*+ The IP address to bind for IPv6. +*/
extern /*@null@*/ ConfigItem Bind_IPv6;

/*+ The port number to use for the HTTP proxy port. +*/
extern /*@null@*/ ConfigItem HTTP_Port;

/*+ The port number to use for the wwwoffle port. +*/
extern /*@null@*/ ConfigItem WWWOFFLE_Port;

/*+ The spool directory. +*/
extern /*@null@*/ ConfigItem SpoolDir;

/*+ The user id for wwwoffled or -1 for none. +*/
extern /*@null@*/ ConfigItem WWWOFFLE_Uid;

/*+ The group id for wwwoffled or -1 for none. +*/
extern /*@null@*/ ConfigItem WWWOFFLE_Gid;

/*+ Whether to use the syslog facility or not. +*/
extern /*@null@*/ ConfigItem UseSyslog;

/*+ The password required for demon configuration. +*/
extern /*@null@*/ ConfigItem PassWord;

/*+ Maximum number of servers +*/
extern /*@null@*/ ConfigItem MaxServers,          /*+ in total. +*/
                             MaxFetchServers;     /*+ for fetching. +*/


/* Options Section */

/*+ The level of error logging +*/
extern /*@null@*/ ConfigItem LogLevel;

/*+ The amount of time that a socket connection will wait for data. +*/
extern /*@null@*/ ConfigItem SocketTimeout;

/*+ The amount of time that a DNS loookup will wait. +*/
extern /*@null@*/ ConfigItem DNSTimeout;

/*+ The amount of time that a socket will wait for the intial connection. +*/
extern /*@null@*/ ConfigItem ConnectTimeout;

/*+ The option to retry a failed connection. +*/
extern /*@null@*/ ConfigItem ConnectRetry;

/*+ The list of allowed SSL port numbers. +*/
extern /*@null@*/ ConfigItem SSLAllowPort;

/*+ The permissions for creation of +*/
extern /*@null@*/ ConfigItem DirPerm,             /*+ directories. +*/
                             FilePerm;            /*+ files. +*/

/*+ The name of a progam to run when changing mode to +*/
extern /*@null@*/ ConfigItem RunOnline,           /*+ online. +*/
                             RunOffline,          /*+ offline. +*/
                             RunAutodial,         /*+ auto dial. +*/
                             RunFetch;            /*+ fetch (start or stop). +*/

/*+ The option to have lock files to stop some problems. +*/
extern /*@null@*/ ConfigItem LockFiles;

/*+ The option to reply to the browser with compressed content encoding. +*/
extern /*@null@*/ ConfigItem ReplyCompressedData;

/*+ The option to reply to the browser with chunked transfer encoding. +*/
extern /*@null@*/ ConfigItem ReplyChunkedData;

/*+ The paths or file extensions that are allowed to be used for CGIs. +*/
extern /*@null@*/ ConfigItem ExecCGI;


/* OnlineOptions section. */

/*+ The option to allow or ignore the 'Pragma: no-cache' request when online. +*/
extern /*@null@*/ ConfigItem PragmaNoCacheOnline;

/*+ The option to allow or ignore the 'Cache-Control: no-cache' request online. +*/
extern /*@null@*/ ConfigItem CacheControlNoCacheOnline;

/*+ The option to allow or ignore the 'Cache-Control: max-age=0' request online. +*/
extern /*@null@*/ ConfigItem CacheControlMaxAge0Online;

/*+ The maximum age of a cached page to use in preference while online. +*/
extern /*@null@*/ ConfigItem RequestChanged;

/*+ The option to only request changes to a page once per session online. +*/
extern /*@null@*/ ConfigItem RequestChangedOnce;

/*+ The option to re-request pages that have expired. +*/
extern /*@null@*/ ConfigItem RequestExpired;

/*+ The option to re-request pages that have the no-cache flag set. +*/
extern /*@null@*/ ConfigItem RequestNoCache;

/*+ The option to re-request pages that have status code 302 (temporary redirection).+*/
extern /*@null@*/ ConfigItem RequestRedirection;

/*+ The option to re-request pages with a conditional request.+*/
extern /*@null@*/ ConfigItem RequestConditional;

/*+ The option to use Etags as a cache validator.+*/
extern /*@null@*/ ConfigItem ValidateWithEtag;

/*+ The option to try and get the requested URL without a password as well as with. +*/
extern /*@null@*/ ConfigItem TryWithoutPassword;

/*+ The option to keep downloads that are interrupted by the user. +*/
extern /*@null@*/ ConfigItem IntrDownloadKeep;

/*+ The option to keep on downloading interrupted pages if +*/
extern /*@null@*/ ConfigItem IntrDownloadSize,           /*+ smaller than a given size. +*/
                             IntrDownloadPercent;        /*+ more than a given percentage complete. +*/

/*+ The option to keep downloads that time out. +*/
extern /*@null@*/ ConfigItem TimeoutDownloadKeep;

/*+ The option to keep previously cached pages instead of overwriting
    them with an error message from the remote server. +*/
extern /*@null@*/ ConfigItem KeepCacheIfNotFound;

/*+ The option to keep previously cached pages instead of overwriting
    them with a page that has a header line that matches a given pattern. +*/
extern /*@null@*/ ConfigItem KeepCacheIfHeaderMatches;

/*+ The option to request from the server compressed content encoding. +*/
extern /*@null@*/ ConfigItem RequestCompressedData;

/*+ The option to request from the server chunked transfer encoding. +*/
extern /*@null@*/ ConfigItem RequestChunkedData;


/* OfflineOptions section */

/*+ The option to allow or ignore the 'Pragma: no-cache' request when offline. +*/
extern /*@null@*/ ConfigItem PragmaNoCacheOffline;

/*+ The option to allow or ignore the 'Cache-Control: no-cache' request offline. +*/
extern /*@null@*/ ConfigItem CacheControlNoCacheOffline;

/*+ The option to allow or ignore the 'Cache-Control: max-age=0' request offline. +*/
extern /*@null@*/ ConfigItem CacheControlMaxAge0Offline;

/*+ The option to not automatically make requests while offline but to need confirmation. +*/
extern /*@null@*/ ConfigItem ConfirmRequests;

/*+ The list of URLs not to request. +*/
extern /*@null@*/ ConfigItem DontRequestOffline;


/* FetchOptions section */

/*+ The option to also fetch style sheets. +*/
extern /*@null@*/ ConfigItem FetchStyleSheets;

/*+ The option to also fetch images. +*/
extern /*@null@*/ ConfigItem FetchImages;

/*+ The option to also fetch webbug images. +*/
extern /*@null@*/ ConfigItem FetchWebbugImages;

/*+ The option to also fetch icon images (favourite/shortcut icons). +*/
extern /*@null@*/ ConfigItem FetchIconImages;

/*+ The option to only fetch images from the same host. +*/
extern /*@null@*/ ConfigItem FetchSameHostImages;

/*+ The option to also fetch frames. +*/
extern /*@null@*/ ConfigItem FetchFrames;

/*+ The option to also fetch scripts. +*/
extern /*@null@*/ ConfigItem FetchScripts;

/*+ The option to also fetch objects. +*/
extern /*@null@*/ ConfigItem FetchObjects;


/* IndexOptions section */

/*+ The option to enable/disable the lasttime/prevtime/lastout/prevout indexes. +*/
extern /*@null@*/ ConfigItem CreateHistoryIndexes;

/*+ The option to cycle the last time directories daily. +*/
extern /*@null@*/ ConfigItem CycleIndexesDaily;

/*+ The choice of URLs to list in the outgoing index. +*/
extern /*@null@*/ ConfigItem IndexListOutgoing;

/*+ The choice of URLs to list in the lastime/prevtime and lastout/prevout indexes. +*/
extern /*@null@*/ ConfigItem IndexListLatest;

/*+ The choice of URLs to list in the monitor index. +*/
extern /*@null@*/ ConfigItem IndexListMonitor;

/*+ The choice of URLs to list in the host indexes. +*/
extern /*@null@*/ ConfigItem IndexListHost;

/*+ The choice of URLs to list in any index. +*/
extern /*@null@*/ ConfigItem IndexListAny;


/* ModifyHTML section */

/*+ The option to turn on the modifications in this section. +*/
extern /*@null@*/ ConfigItem EnableHTMLModifications;

/*+ The option to turn on the modifications when online. +*/
extern /*@null@*/ ConfigItem EnableModificationsOnline;

/*+ The option of a tag that can be added to the bottom of the spooled pages with the date and some buttons. +*/
extern /*@null@*/ ConfigItem AddCacheInfo;

/*+ An optional local file that can be inserted as a footer. +*/
extern /*@null@*/ ConfigItem InsertFile;

/*+ The options to modify the anchor tags in the HTML +*/
extern ConfigItem AnchorModifyBegin[3], /*+ (before the start tag). +*/
                  AnchorModifyEnd[3];   /*+ (after the end tag). +*/

/*+ The option to disable scripts and scripted actions. +*/
extern /*@null@*/ ConfigItem DisableHTMLScript;

/*+ The option to disable external scripts with URLs in the DontGet list. +*/
extern /*@null@*/ ConfigItem DisableHTMLDontGetScript;

/*+ The option to disable Java applets. +*/
extern /*@null@*/ ConfigItem DisableHTMLApplet;

/*+ The option to disable stylesheets and style references. +*/
extern /*@null@*/ ConfigItem DisableHTMLStyle;

/*+ The option to disable the <blink> tag. +*/
extern /*@null@*/ ConfigItem DisableHTMLBlink;

/*+ The option to disable the <marquee> tag. +*/
extern /*@null@*/ ConfigItem DisableHTMLMarquee;

/*+ The option to disable Shockwave Flash animations. +*/
extern /*@null@*/ ConfigItem DisableHTMLFlash;

/*+ The option to disable (or modify) any <meta http-equiv=Refresh content=""> tags. +*/
extern /*@null@*/ ConfigItem DisableHTMLMetaRefresh;

/*+ The option to disable any <meta http-equiv=Refresh content=""> tags that refer to the same URL. +*/
extern /*@null@*/ ConfigItem DisableHTMLMetaRefreshSelf;

/*+ The time to use as a replacement for the refresh time. +*/
extern ConfigItem ReplacementHTMLMetaRefreshTime;

/*+ The option to disable any <meta http-equiv=Set-Cookie content=""> tags. +*/
extern /*@null@*/ ConfigItem DisableHTMLMetaSetCookie;

/*+ The option to disable links (anchors) to pages in the DontGet list. +*/
extern /*@null@*/ ConfigItem DisableHTMLDontGetAnchors;

/*+ The option to disable inline frames (iframes) with URLs in the DontGet list. +*/
extern /*@null@*/ ConfigItem DisableHTMLDontGetIFrames;

/*+ The option to replace images that are in the DontGet list. +*/
extern /*@null@*/ ConfigItem ReplaceHTMLDontGetImages;

/*+ The URL to use as a replacement for the images that are in the DontGet list. +*/
extern /*@null@*/ ConfigItem ReplacementHTMLDontGetImage;

/*+ The option to replace images that are 1 pixel square. +*/
extern /*@null@*/ ConfigItem ReplaceHTMLWebbugImages;

/*+ The URL to use as a replacement for the images that are 1 pixel square. +*/
extern /*@null@*/ ConfigItem ReplacementHTMLWebbugImage;

/*+ The option to demoronise MS characters. +*/
extern /*@null@*/ ConfigItem DemoroniseMSChars;

/*+ The option to fix cyrillic pages written in koi8-r mixed with cp1251. +*/
extern ConfigItem /*@null@*/ FixMixedCyrillic;

/*+ The option to disable animated GIFs. +*/
extern /*@null@*/ ConfigItem DisableAnimatedGIF;


/* LocalHost section */

/*+ The list of localhost hostnames. +*/
extern /*@null@*/ ConfigItem LocalHost;


/* LocalNet section */

/*+ The list of local network hostnames. +*/
extern /*@null@*/ ConfigItem LocalNet;


/* AllowedConnectHosts section */

/*+ The list of allowed hostnames. +*/
extern /*@null@*/ ConfigItem AllowedConnectHosts;


/* AllowedConnectUsers section */

/*+ The list of allowed usernames and paswords. +*/
extern /*@null@*/ ConfigItem AllowedConnectUsers;


/* DontCache section */

/*+ The list of URLs not to cache. +*/
extern /*@null@*/ ConfigItem DontCache;


/* DontGet section */

/*+ The list of URLs not to get. +*/
extern /*@null@*/ ConfigItem DontGet;

/*+ The replacement URL. +*/
extern /*@null@*/ ConfigItem DontGetReplacementURL;

/*+ The list of URLs not to get recursively. +*/
extern /*@null@*/ ConfigItem DontGetRecursive;

/*+ The option to treat location headers to not got pages as errors. +*/
extern /*@null@*/ ConfigItem DontGetLocation;


/* DontCompress section */

/*+ The list of MIME types not to compress. +*/
extern /*@null@*/ ConfigItem DontCompressMIME;

/*+ The list of file extensions not to compress. +*/
extern /*@null@*/ ConfigItem DontCompressExt;


/* CensorIncomingHeader and CensorOutgoingHeader sections */

/*+ The list of censored headers. +*/
extern /*@null@*/ ConfigItem CensorIncomingHeader;
extern /*@null@*/ ConfigItem CensorOutgoingHeader;

/*+ Flags to cause Set-Cookie headers to be mangled. +*/
extern /*@null@*/ ConfigItem SessionCookiesOnly;

/*+ Flags to cause the 'Referer' header to be mangled +*/
extern /*@null@*/ ConfigItem RefererSelf,    /*+ to point to itself. +*/
                             RefererSelfDir; /*+ to point to the parent directory. +*/

/*+ A flag to cause a 'User-Agent' header always to be added. +*/
extern /*@null@*/ ConfigItem ForceUserAgent;


/* FTPOptions section */

/*+ The anon-ftp username. +*/
extern /*@null@*/ ConfigItem FTPUserName;

/*+ The anon-ftp password. +*/
extern /*@null@*/ ConfigItem FTPPassWord;

/*+ The information that is needed to allow non-anonymous access, +*/
extern /*@null@*/ ConfigItem FTPAuthUser,         /*+ username. +*/
                             FTPAuthPass;         /*+ password. +*/


/* MIMETypes section */

/*+ The default MIME type. +*/
extern /*@null@*/ ConfigItem DefaultMIMEType;

/*+ The list of MIME types. +*/
extern /*@null@*/ ConfigItem MIMETypes;


/* Proxy section */

/*+ The list of hostnames and proxies. +*/
extern /*@null@*/ ConfigItem Proxies;

/*+ The information that is needed to allow authorisation headers to be added, +*/
extern /*@null@*/ ConfigItem ProxyAuthUser,       /*+ username. +*/
                             ProxyAuthPass;       /*+ password. +*/

/*+ The SSL proxy to use. +*/
extern /*@null@*/ ConfigItem SSLProxy;

/*+ The SOCKS proxy to use. +*/
extern /*@null@*/ ConfigItem SocksProxy;

/*+ Flags to indicate whether destination host names should be resolved remotely by SOCKS server. +*/
extern /*@null@*/ ConfigItem SocksRemoteDNS;


/* Alias section */

/*+ The list of protocols/hostnames and their aliases. +*/
extern /*@null@*/ ConfigItem Aliases;


/* Purge section */

/*+ A flag to indicate that the modification time is used instead of the access time. +*/
extern /*@null@*/ ConfigItem PurgeUseMTime;

/*+ The maximum allowed size of the cache. +*/
extern /*@null@*/ ConfigItem PurgeMaxSize;

/*+ The minimum allowed free disk space. +*/
extern /*@null@*/ ConfigItem PurgeMinFree;

/*+ A flag to indicate if the whole URL is used to choose the purge age. +*/
extern /*@null@*/ ConfigItem PurgeUseURL;

/*+ A flag to indicate if the DontGet hosts are to be purged. +*/
extern /*@null@*/ ConfigItem PurgeDontGet;

/*+ A flag to indicate if the DontCache hosts are to be purged. +*/
extern /*@null@*/ ConfigItem PurgeDontCache;

/*+ The list of hostnames and purge ages. +*/
extern /*@null@*/ ConfigItem PurgeAges;

/*+ The list of hostnames and purge compress ages. +*/
extern /*@null@*/ ConfigItem PurgeCompressAges;


/* Options Section */

int IsSSLAllowedPort(int port);
int IsCGIAllowed(char *path);


/* LocalHost Section */

char /*@special@*/ *GetLocalHost(int port) /*@defines result@*/;
int IsLocalHostPort(char *hostport);


/* LocalNet Section */

int IsLocalNetHost(char *host);
int IsLocalNetHostPort(char *hostport);


/* AllowedConnectHosts Section */

int IsAllowedConnectHost(char *host);


/* AllowedConnectUsers Section */

/*@null@*/ char *IsAllowedConnectUser(/*@null@*/ char *userpass);


/* DontCompress section */

int NotCompressed(/*@null@*/ char *mime_type,/*@null@*/ char *path);


/* CensorHeader Section */

char *CensoredHeader(ConfigItem confitem,URL *Url,char *key,/*@returned@*/ char *val);


/* MIMETypes Section */

char /*@observer@*/ *WhatMIMEType(char *path);


/* Alias Section */

int IsAliased(char *proto,char *hostport,char *path,/*@out@*/ char **new_proto,/*@out@*/ char **new_hostport,/*@out@*/ char **new_path);


#endif /* CONFIG_H */
