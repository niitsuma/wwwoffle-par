/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/configdata.c 2.130 2002/08/21 19:47:52 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7e.
  Configuration data functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <unistd.h>

#include "wwwoffle.h"
#include "misc.h"
#include "configpriv.h"
#include "config.h"
#include "sockets.h"
#include "errors.h"


#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* StartUp section */

/*+ The IP address to bind for IPv4. +*/
ConfigItem Bind_IPv4;

/*+ The IP address to bind for IPv6. +*/
ConfigItem Bind_IPv6;

/*+ The port number to use for the HTTP proxy port. +*/
ConfigItem HTTP_Port;

/*+ The port number to use for the wwwoffle port. +*/
ConfigItem WWWOFFLE_Port;

/*+ The spool directory. +*/
ConfigItem SpoolDir;

/*+ The user id for wwwoffled or -1 for none. +*/
ConfigItem WWWOFFLE_Uid;

/*+ The group id for wwwoffled or -1 for none. +*/
ConfigItem WWWOFFLE_Gid;

/*+ Whether to use the syslog facility or not. +*/
ConfigItem UseSyslog;

/*+ The password required for demon configuration. +*/
ConfigItem PassWord;

/*+ Maximum number of servers  +*/
ConfigItem MaxServers;          /*+ in total. +*/
ConfigItem MaxFetchServers;     /*+ for fetching. +*/

/*+ The item definitions in the StartUp section. +*/
static ConfigItemDef startup_itemdefs[]={
 {"bind-ipv4"        ,&Bind_IPv4      ,0,0,Fixed,HostOrNone        ,"0.0.0.0"   },
 {"bind-ipv6"        ,&Bind_IPv6      ,0,0,Fixed,HostOrNone        ,"::"        },
 {"http-port"        ,&HTTP_Port      ,0,0,Fixed,PortNumber        ,NULL        },  /* 2 see InitConfigurationFile() */
 {"wwwoffle-port"    ,&WWWOFFLE_Port  ,0,0,Fixed,PortNumber        ,NULL        },  /* 3 see InitConfigurationFile() */
 {"spool-dir"        ,&SpoolDir       ,0,0,Fixed,PathName          ,DEF_SPOOLDIR},
 {"run-uid"          ,&WWWOFFLE_Uid   ,0,0,Fixed,UserId            ,"-1"        },
 {"run-gid"          ,&WWWOFFLE_Gid   ,0,0,Fixed,GroupId           ,"-1"        },
 {"use-syslog"       ,&UseSyslog      ,0,0,Fixed,Boolean           ,"yes"       },
 {"password"         ,&PassWord       ,0,0,Fixed,String            ,NULL        },
 {"max-servers"      ,&MaxServers     ,0,0,Fixed,CfgMaxServers     ,NULL        },  /* 9 see InitConfigurationFile() */
 {"max-fetch-servers",&MaxFetchServers,0,0,Fixed,CfgMaxFetchServers,NULL        }   /*10 see InitConfigurationFile() */
};

/*+ The StartUp section. +*/
static ConfigSection startup_section={"StartUp",
                                      sizeof(startup_itemdefs)/sizeof(ConfigItemDef),
                                      startup_itemdefs};


/* Options Section */

/*+ The level of error logging +*/
ConfigItem LogLevel;

/*+ The amount of time that a socket connection will wait for data. +*/
ConfigItem SocketTimeout;

/*+ The amount of time that a DNS loookup will wait. +*/
ConfigItem DNSTimeout;

/*+ The amount of time that a socket will wait for the intial connection. +*/
ConfigItem ConnectTimeout;

/*+ The option to retry a failed connection. +*/
ConfigItem ConnectRetry;

/*+ The list of allowed SSL port numbers. +*/
ConfigItem SSLAllowPort;

/*+ The permissions for creation of +*/
ConfigItem DirPerm;             /*+ directories. +*/
ConfigItem FilePerm;            /*+ files. +*/

/*+ The name of a progam to run when changing mode to +*/
ConfigItem RunOnline;           /*+ online. +*/
ConfigItem RunOffline;          /*+ offline. +*/
ConfigItem RunAutodial;         /*+ auto dial. +*/
ConfigItem RunFetch;            /*+ fetch (start or stop). +*/

/*+ The option to have lock files to stop some problems. +*/
ConfigItem LockFiles;

/*+ The option to reply with compressed content encoding. +*/
ConfigItem ReplyCompressedData;

/*+ The paths or file extensions that are allowed to be used for CGIs. +*/
ConfigItem ExecCGI;

/*+ The item definitions in the Options section. +*/
static ConfigItemDef options_itemdefs[]={
 {"log-level"            ,&LogLevel           ,0,0,Fixed,CfgLogLevel,"Important"},
 {"socket-timeout"       ,&SocketTimeout      ,0,0,Fixed,TimeSecs   ,"2m"       },
 {"dns-timeout"          ,&DNSTimeout         ,0,0,Fixed,TimeSecs   ,"1m"       },
 {"connect-timeout"      ,&ConnectTimeout     ,0,0,Fixed,TimeSecs   ,"30"       },
 {"connect-retry"        ,&ConnectRetry       ,0,0,Fixed,Boolean    ,"no"       },
 {"ssl-allow-port"       ,&SSLAllowPort       ,0,1,Fixed,PortNumber ,NULL       },
 {"dir-perm"             ,&DirPerm            ,0,0,Fixed,FileMode   ,NULL       }, /* 6 see InitConfigurationFile() */
 {"file-perm"            ,&FilePerm           ,0,0,Fixed,FileMode   ,NULL       }, /* 7 see InitConfigurationFile() */
 {"run-online"           ,&RunOnline          ,0,0,Fixed,PathName   ,NULL       },
 {"run-offline"          ,&RunOffline         ,0,0,Fixed,PathName   ,NULL       },
 {"run-autodial"         ,&RunAutodial        ,0,0,Fixed,PathName   ,NULL       },
 {"run-fetch"            ,&RunFetch           ,0,0,Fixed,PathName   ,NULL       },
 {"lock-files"           ,&LockFiles          ,0,0,Fixed,Boolean    ,"no"       },
 {"reply-compressed-data",&ReplyCompressedData,0,0,Fixed,Boolean    ,"no"       },
 {"exec-cgi"             ,&ExecCGI            ,0,1,Fixed,Url        ,NULL       }
};

/*+ Options section. +*/
static ConfigSection options_section={"Options",
                                      sizeof(options_itemdefs)/sizeof(ConfigItemDef),
                                      options_itemdefs};


/* OnlineOptions section. */

/*+ The maximum age of a cached page to use in preference while online. +*/
ConfigItem RequestChanged;

/*+ The option to only request changes to a page once per session online. +*/
ConfigItem RequestChangedOnce;

/*+ The option to re-request pages that have expired. +*/
ConfigItem RequestExpired;

/*+ The option to re-request pages that have the no-cache flag set. +*/
ConfigItem RequestNoCache;

/*+ The option to re-request pages that have status code 302 (temporary redirection).+*/
ConfigItem RequestRedirection;

/*+ The option to try and get the requested URL without a password as well as with. +*/
ConfigItem TryWithoutPassword;

/*+ The option to keep downloads that are interrupted by the user. +*/
ConfigItem IntrDownloadKeep;

/*+ The option to keep on downloading interrupted pages if +*/
ConfigItem IntrDownloadSize;           /*+ smaller than a given size. +*/
ConfigItem IntrDownloadPercent;        /*+ more than a given percentage complete. +*/

/*+ The option to keep downloads that time out. +*/
ConfigItem TimeoutDownloadKeep;

/*+ The option to request compressed content encoding. +*/
ConfigItem RequestCompressedData;

/*+ The item definitions in the OnlineOptions section. +*/
static ConfigItemDef onlineoptions_itemdefs[]={
 {"request-changed"        ,&RequestChanged       ,1,0,Fixed,TimeSecs  ,"10m"},
 {"request-changed-once"   ,&RequestChangedOnce   ,1,0,Fixed,Boolean   ,"yes"},
 {"request-expired"        ,&RequestExpired       ,1,0,Fixed,Boolean   ,"no" },
 {"request-no-cache"       ,&RequestNoCache       ,1,0,Fixed,Boolean   ,"no" },
 {"request-redirection"    ,&RequestRedirection   ,1,0,Fixed,Boolean   ,"no" },
 {"try-without-password"   ,&TryWithoutPassword   ,1,0,Fixed,Boolean   ,"yes"},
 {"intr-download-keep"     ,&IntrDownloadKeep     ,1,0,Fixed,Boolean   ,"no" },
 {"intr-download-size"     ,&IntrDownloadSize     ,1,0,Fixed,FileSize  ,"1"  },
 {"intr-download-percent"  ,&IntrDownloadPercent  ,1,0,Fixed,Percentage,"80" },
 {"timeout-download-keep"  ,&TimeoutDownloadKeep  ,1,0,Fixed,Boolean   ,"no" },
 {"request-compressed-data",&RequestCompressedData,1,0,Fixed,Boolean   ,"yes"}
};

/*+ OnlineOptions section. +*/
static ConfigSection onlineoptions_section={"OnlineOptions",
                                            sizeof(onlineoptions_itemdefs)/sizeof(ConfigItemDef),
                                            onlineoptions_itemdefs};


/* OfflineOptions section. */

/*+ The option to allow or ignore the 'Pragma: no-cache' request. +*/
ConfigItem PragmaNoCache;

/*+ The option to not automatically make requests while offline but to need confirmation. +*/
ConfigItem ConfirmRequests;

/*+ The list of URLs not to request. +*/
ConfigItem DontRequestOffline;

/*+ The item definitions in the OfflineOptions section. +*/
static ConfigItemDef offlineoptions_itemdefs[]={
 {"pragma-no-cache" ,&PragmaNoCache     ,1,0,Fixed,Boolean,"yes"},
 {"confirm-requests",&ConfirmRequests   ,1,0,Fixed,Boolean,"no" },
 {"dont-request"    ,&DontRequestOffline,1,0,Fixed,Boolean,"no" }
};

/*+ OfflineOptions section. +*/
static ConfigSection offlineoptions_section={"OfflineOptions",
                                             sizeof(offlineoptions_itemdefs)/sizeof(ConfigItemDef),
                                             offlineoptions_itemdefs};


/* FetchOptions section */

/*+ The option to also fetch style sheets. +*/
ConfigItem FetchStyleSheets;

/*+ The option to also fetch images. +*/
ConfigItem FetchImages;

/*+ The option to also fetch webbug images. +*/
ConfigItem FetchWebbugImages;

/*+ The option to only fetch images from the same host. +*/
ConfigItem FetchSameHostImages;

/*+ The option to also fetch frames. +*/
ConfigItem FetchFrames;

/*+ The option to also fetch scripts. +*/
ConfigItem FetchScripts;

/*+ The option to also fetch objects. +*/
ConfigItem FetchObjects;

/*+ The item definitions in the FetchOptions section. +*/
static ConfigItemDef fetchoptions_itemdefs[]={
 {"stylesheets"          ,&FetchStyleSheets   ,1,0,Fixed,Boolean,"no" },
 {"images"               ,&FetchImages        ,1,0,Fixed,Boolean,"no" },
 {"webbug-images"        ,&FetchWebbugImages  ,1,0,Fixed,Boolean,"yes"},
 {"only-same-host-images",&FetchSameHostImages,1,0,Fixed,Boolean,"no" },
 {"frames"               ,&FetchFrames        ,1,0,Fixed,Boolean,"no" },
 {"scripts"              ,&FetchScripts       ,1,0,Fixed,Boolean,"no" },
 {"objects"              ,&FetchObjects       ,1,0,Fixed,Boolean,"no" }
};

/*+ The FetchOptions section. +*/
static ConfigSection fetchoptions_section={"FetchOptions",
                                           sizeof(fetchoptions_itemdefs)/sizeof(ConfigItemDef),
                                           fetchoptions_itemdefs};


/* IndexOptions section */

/*+ The option to disable the lasttime/prevtime indexes. +*/
ConfigItem NoLasttimeIndex;

/*+ The option to cycle the last time directories daily. +*/
ConfigItem CycleIndexesDaily;

/*+ The choice of URLs to list in the outgoing index. +*/
ConfigItem IndexListOutgoing;

/*+ The choice of URLs to list in the lastime/prevtime and lastout/prevout indexes. +*/
ConfigItem IndexListLatest;

/*+ The choice of URLs to list in the monitor index. +*/
ConfigItem IndexListMonitor;

/*+ The choice of URLs to list in the host indexes. +*/
ConfigItem IndexListHost;

/*+ The choice of URLs to list in any index. +*/
ConfigItem IndexListAny;

/*+ The item definitions in the IndexOptions section. +*/
static ConfigItemDef indexoptions_itemdefs[]={
 {"no-lasttime-index"  ,&NoLasttimeIndex  ,0,0,Fixed,Boolean,"no" },
 {"cycle-indexes-daily",&CycleIndexesDaily,0,0,Fixed,Boolean,"no" },
 {"list-outgoing"      ,&IndexListOutgoing,1,0,Fixed,Boolean,"yes"},
 {"list-latest"        ,&IndexListLatest  ,1,0,Fixed,Boolean,"yes"},
 {"list-monitor"       ,&IndexListMonitor ,1,0,Fixed,Boolean,"yes"},
 {"list-host"          ,&IndexListHost    ,1,0,Fixed,Boolean,"yes"},
 {"list-any"           ,&IndexListAny     ,1,0,Fixed,Boolean,"yes"}
};

/*+ The IndexOptions section. +*/
static ConfigSection indexoptions_section={"IndexOptions",
                                           sizeof(indexoptions_itemdefs)/sizeof(ConfigItemDef),
                                           indexoptions_itemdefs};


/* ModifyHTML section */

/*+ The option to turn on the modifications in this section. +*/
ConfigItem EnableHTMLModifications;

/*+ The option to turn on the modifications when online. +*/
ConfigItem EnableModificationsOnline;

/*+ The option of a tag that can be added to the bottom of the spooled pages with the date and some buttons. +*/
ConfigItem AddCacheInfo;

/*+ The options to modify the anchor tags in the HTML. +*/
ConfigItem AnchorModifyBegin[3];
ConfigItem AnchorModifyEnd[3];

/*+ The option to disable scripts and scripted actions. +*/
ConfigItem DisableHTMLScript;

/*+ The option to disable Java applets. +*/
ConfigItem DisableHTMLApplet;

/*+ The option to disable stylesheets and style references. +*/
ConfigItem DisableHTMLStyle;

/*+ The option to disable the <blink> tag. +*/
ConfigItem DisableHTMLBlink;

/*+ The option to disable Shockwave Flash animations. +*/
ConfigItem DisableHTMLFlash;

/*+ The option to disable any <meta http-equiv=refresh content=""> tags. +*/
ConfigItem DisableHTMLMetaRefresh;

/*+ The option to disable any <meta http-equiv=refresh content=""> tags that refer to the same URL. +*/
ConfigItem DisableHTMLMetaRefreshSelf;

/*+ The option to disable links (anchors) to pages in the DontGet list. +*/
ConfigItem DisableHTMLDontGetAnchors;

/*+ The option to disable inline frames (iframes) with URLs in the DontGet list. +*/
ConfigItem DisableHTMLDontGetIFrames;

/*+ The option to replace images that are in the DontGet list. +*/
ConfigItem ReplaceHTMLDontGetImages;

/*+ The URL to use as a replacement for the images that are in the DontGet list. +*/
ConfigItem ReplacementHTMLDontGetImage;

/*+ The option to replace images that are 1 pixel square. +*/
ConfigItem ReplaceHTMLWebbugImages;

/*+ The URL to use as a replacement for the images that are 1 pixel square. +*/
ConfigItem ReplacementHTMLWebbugImage;

/*+ The option to demoronise MS characters. +*/
ConfigItem DemoroniseMSChars;

/*+ The option to disable animated GIFs. +*/
ConfigItem DisableAnimatedGIF;

/*+ The item definitions in the ModifyHTMLOptions section. +*/
static ConfigItemDef modifyhtml_itemdefs[]={
 {"enable-modify-html"       ,&EnableHTMLModifications    ,1,0,Fixed,Boolean,"no"},
 {"enable-modify-online"     ,&EnableModificationsOnline  ,1,0,Fixed,Boolean,"no"},
 {"add-cache-info"           ,&AddCacheInfo               ,1,0,Fixed,Boolean,"no"},
 {"anchor-cached-begin"      ,&AnchorModifyBegin[0]       ,1,0,Fixed,String ,NULL},
 {"anchor-cached-end"        ,&AnchorModifyEnd[0]         ,1,0,Fixed,String ,NULL},
 {"anchor-requested-begin"   ,&AnchorModifyBegin[1]       ,1,0,Fixed,String ,NULL},
 {"anchor-requested-end"     ,&AnchorModifyEnd[1]         ,1,0,Fixed,String ,NULL},
 {"anchor-not-cached-begin"  ,&AnchorModifyBegin[2]       ,1,0,Fixed,String ,NULL},
 {"anchor-not-cached-end"    ,&AnchorModifyEnd[2]         ,1,0,Fixed,String ,NULL},
 {"disable-script"           ,&DisableHTMLScript          ,1,0,Fixed,Boolean,"no"},
 {"disable-applet"           ,&DisableHTMLApplet          ,1,0,Fixed,Boolean,"no"},
 {"disable-style"            ,&DisableHTMLStyle           ,1,0,Fixed,Boolean,"no"},
 {"disable-blink"            ,&DisableHTMLBlink           ,1,0,Fixed,Boolean,"no"},
 {"disable-flash"            ,&DisableHTMLFlash           ,1,0,Fixed,Boolean,"no"},
 {"disable-meta-refresh"     ,&DisableHTMLMetaRefresh     ,1,0,Fixed,Boolean,"no"},
 {"disable-meta-refresh-self",&DisableHTMLMetaRefreshSelf ,1,0,Fixed,Boolean,"no"},
 {"disable-dontget-links"    ,&DisableHTMLDontGetAnchors  ,1,0,Fixed,Boolean,"no"},
 {"disable-dontget-iframes"  ,&DisableHTMLDontGetIFrames  ,1,0,Fixed,Boolean,"no"},
 {"replace-dontget-images"   ,&ReplaceHTMLDontGetImages   ,1,0,Fixed,Boolean,"no"},
 {"replacement-dontget-image",&ReplacementHTMLDontGetImage,1,0,Fixed,Url    ,"/local/dontget/replacement.gif"},
 {"replace-webbug-images"    ,&ReplaceHTMLWebbugImages    ,1,0,Fixed,Boolean,"no"},
 {"replacement-webbug-image" ,&ReplacementHTMLWebbugImage ,1,0,Fixed,Url    ,"/local/dontget/replacement.gif"},
 {"demoronise-ms-chars"      ,&DemoroniseMSChars          ,1,0,Fixed,Boolean,"no"},
 {"disable-animated-gif"     ,&DisableAnimatedGIF         ,1,0,Fixed,Boolean,"no"}
};

/*+ The ModifyHTML section. +*/
static ConfigSection modifyhtml_section={"ModifyHTML",
                                         sizeof(modifyhtml_itemdefs)/sizeof(ConfigItemDef),
                                         modifyhtml_itemdefs};


/* LocalHost section */

/*+ The list of localhost hostnames. +*/
ConfigItem LocalHost;

/*+ The item definitions in the LocalHost section. +*/
static ConfigItemDef localhost_itemdefs[]={
 {"",&LocalHost,0,1,Host,None,NULL}
};

/*+ The LocalHost section. +*/
static ConfigSection localhost_section={"LocalHost",
                                        sizeof(localhost_itemdefs)/sizeof(ConfigItemDef),
                                        localhost_itemdefs};


/* LocalNet section */

/*+ The list of local network hostnames. +*/
ConfigItem LocalNet;

/*+ The item definitions in the LocalNet section. +*/
static ConfigItemDef localnet_itemdefs[]={
 {"",&LocalNet,0,1,Host,None,NULL}
};

/*+ The LocalNet section. +*/
static ConfigSection localnet_section={"LocalNet",
                                       sizeof(localnet_itemdefs)/sizeof(ConfigItemDef),
                                       localnet_itemdefs};


/* AllowedConnectHosts section */

/*+ The list of allowed hostnames. +*/
ConfigItem AllowedConnectHosts;

/*+ The item definitions in the AllowedConnectHosts section. +*/
static ConfigItemDef allowedconnecthosts_itemdefs[]={
 {"",&AllowedConnectHosts,0,1,Host,None,NULL}
};

/*+ The AllowedConnectHosts section. +*/
static ConfigSection allowedconnecthosts_section={"AllowedConnectHosts",
                                                  sizeof(allowedconnecthosts_itemdefs)/sizeof(ConfigItemDef),
                                                  allowedconnecthosts_itemdefs};


/* AllowedConnectUsers section */

/*+ The list of allowed usernames and paswords. +*/
ConfigItem AllowedConnectUsers;

/*+ The item definitions in the AllowedConnectUsers section. +*/
static ConfigItemDef allowedconnectusers_itemdefs[]={
 {"",&AllowedConnectUsers,0,1,UserPass,None,NULL}
};

/*+ The AllowedConnectUsers section. +*/
static ConfigSection allowedconnectusers_section={"AllowedConnectUsers",
                                                  sizeof(allowedconnectusers_itemdefs)/sizeof(ConfigItemDef),
                                                  allowedconnectusers_itemdefs};


/* DontCache section */

/*+ The list of URLs not to cache. +*/
ConfigItem DontCache;

/*+ The item definitions in the DontCache section. +*/
static ConfigItemDef dontcache_itemdefs[]={
 {"",&DontCache,0,1,UrlSpecification,None,NULL}
};

/*+ The DontCache section. +*/
static ConfigSection dontcache_section={"DontCache",
                                        sizeof(dontcache_itemdefs)/sizeof(ConfigItemDef),
                                        dontcache_itemdefs};


/* DontGet section */

/*+ The list of URLs not to get. +*/
ConfigItem DontGet;

/*+ The replacement URL. +*/
ConfigItem DontGetReplacementURL;

/*+ The list of URLs not to get recursively. +*/
ConfigItem DontGetRecursive;

/*+ The option to treat location headers to not got pages as errors. +*/
ConfigItem DontGetLocation;

/*+ The item definitions in the DontGet section. +*/
static ConfigItemDef dontget_itemdefs[]={
 {"replacement"   ,&DontGetReplacementURL,1,0,Fixed           ,Url    ,NULL },
 {"get-recursive" ,&DontGetRecursive     ,1,0,Fixed           ,Boolean,"yes"},
 {"location-error",&DontGetLocation      ,1,0,Fixed           ,Boolean,"no" },
 {""              ,&DontGet              ,0,1,UrlSpecification,None   ,NULL }
};

/*+ The DontGet section. +*/
static ConfigSection dontget_section={"DontGet",
                                      sizeof(dontget_itemdefs)/sizeof(ConfigItemDef),
                                      dontget_itemdefs};


/* DontCompress section */

/*+ The list of MIME types not to compress. +*/
ConfigItem DontCompressMIME;

/*+ The list of file extensions not to compress. +*/
ConfigItem DontCompressExt;

/*+ The item definitions in the DontCompress section. +*/
static ConfigItemDef dontcompress_itemdefs[]={
 {"mime-type",&DontCompressMIME,0,1,Fixed,MIMEType,NULL},
 {"file-ext" ,&DontCompressExt ,0,1,Fixed,FileExt ,NULL}
};

/*+ The DontCompress section. +*/
static ConfigSection dontcompress_section={"DontCompress",
                                           sizeof(dontcompress_itemdefs)/sizeof(ConfigItemDef),
                                           dontcompress_itemdefs};


/* CensorHeader section */

/*+ The list of censored headers. +*/
ConfigItem CensorHeader;

/*+ Flags to cause the referer header to be mangled. +*/
ConfigItem RefererSelf;
ConfigItem RefererSelfDir;

/*+ The item definitions in the censor headers section. +*/
static ConfigItemDef censorheader_itemdefs[]={
 {"referer-self"    ,&RefererSelf   ,1,0,Fixed ,Boolean,"no"},
 {"referer-self-dir",&RefererSelfDir,1,0,Fixed ,Boolean,"no"},
 {""                ,&CensorHeader  ,1,1,String,String ,NULL}
};

/*+ The CensorHeader section. +*/
static ConfigSection censorheader_section={"CensorHeader",
                                           sizeof(censorheader_itemdefs)/sizeof(ConfigItemDef),
                                           censorheader_itemdefs};


/* FTPOptions section */

/*+ The anon-ftp username. +*/
ConfigItem FTPUserName;

/*+ The anon-ftp password. +*/
ConfigItem FTPPassWord;

/*+ The information that is needed to allow non-anonymous access, +*/
ConfigItem FTPAuthUser;         /*+ username +*/
ConfigItem FTPAuthPass;         /*+ password +*/

/*+ The item definitions in the FTPOptions section. +*/
static ConfigItemDef ftpoptions_itemdefs[]={
 {"anon-username",&FTPUserName,0,0,Fixed,String,"anonymous"},
 {"anon-password",&FTPPassWord,0,0,Fixed,String,NULL       }, /* 2 see InitConfigurationFile() */
 {"auth-username",&FTPAuthUser,1,0,Fixed,String,NULL       },
 {"auth-password",&FTPAuthPass,1,0,Fixed,String,NULL       }
};

/*+ The FTPOptions section. +*/
static ConfigSection ftpoptions_section={"FTPOptions",
                                         sizeof(ftpoptions_itemdefs)/sizeof(ConfigItemDef),
                                         ftpoptions_itemdefs};


/* MIMETypes section */

/*+ The default MIME type. +*/
ConfigItem DefaultMIMEType;

/*+ The list of MIME types. +*/
ConfigItem MIMETypes;

/*+ The item definitions in the FTPOptions section. +*/
static ConfigItemDef mimetypes_itemdefs[]={
 {"default",&DefaultMIMEType,0,0,Fixed  ,MIMEType,"text/plain"},
 {""       ,&MIMETypes      ,0,1,FileExt,MIMEType,NULL        }
};

/*+ The MIMETypes section. +*/
static ConfigSection mimetypes_section={"MIMETypes",
                                        sizeof(mimetypes_itemdefs)/sizeof(ConfigItemDef),
                                        mimetypes_itemdefs};


/* Proxy section */

/*+ The list of hostnames and proxies. +*/
ConfigItem Proxies;

/*+ The information that is needed to allow authorisation headers to be added, +*/
ConfigItem ProxyAuthUser;       /*+ username +*/
ConfigItem ProxyAuthPass;       /*+ password +*/

/*+ The SSL proxy to use. +*/
ConfigItem SSLProxy;

/*+ The item defintions in the Proxy section. +*/
static ConfigItemDef proxy_itemdefs[]={
 {"auth-username",&ProxyAuthUser,1,0,Fixed,String           ,NULL},
 {"auth-password",&ProxyAuthPass,1,0,Fixed,String           ,NULL},
 {"ssl"          ,&SSLProxy     ,1,0,Fixed,HostAndPortOrNone,NULL},
 {"proxy"        ,&Proxies      ,1,0,Fixed,HostAndPortOrNone,NULL}
};

/*+ The Proxy section. +*/
static ConfigSection proxy_section={"Proxy",
                                    sizeof(proxy_itemdefs)/sizeof(ConfigItemDef),
                                    proxy_itemdefs};


/* Alias section */

/*+ The list of protocols/hostnames and their aliases. +*/
ConfigItem Aliases;

/*+ The item definitions in the Alias section. +*/
static ConfigItemDef alias_itemdefs[]={
 {"",&Aliases,0,1,UrlSpecification,UrlSpecification,NULL}
};

/*+ The Alias section. +*/
static ConfigSection alias_section={"Alias",
                                    sizeof(alias_itemdefs)/sizeof(ConfigItemDef),
                                    alias_itemdefs};


/* Purge section */

/*+ A flag to indicate that the modification time is used instead of the access time. +*/
ConfigItem PurgeUseMTime;

/*+ The maximum allowed size of the cache. +*/
ConfigItem PurgeCacheSize;

/*+ The minimum allowed free disk space. +*/
ConfigItem PurgeDiskFree;

/*+ A flag to indicate if the whole URL is used to choose the purge age. +*/
ConfigItem PurgeUseURL;

/*+ A flag to indicate if the DontGet hosts are to be purged. +*/
ConfigItem PurgeDontGet;

/*+ A flag to indicate if the DontCache hosts are to be purged. +*/
ConfigItem PurgeDontCache;

/*+ The list of hostnames and purge ages. +*/
ConfigItem PurgeAges;

/*+ The list of hostnames and purge compress ages. +*/
ConfigItem PurgeCompressAges;

/*+ The item definitions in the Purge section. +*/
static ConfigItemDef purge_itemdefs[]={
 {"use-mtime"    ,&PurgeUseMTime      ,0,0,Fixed,Boolean  ,"no"},
 {"age"          ,&PurgeAges          ,1,0,Fixed,AgeDays  ,"2w"},
 {"compress-age" ,&PurgeCompressAges  ,1,0,Fixed,AgeDays  ,"-1"},
 {"max-size"     ,&PurgeCacheSize     ,0,0,Fixed,CacheSize,"0" },
 {"min-free"     ,&PurgeDiskFree      ,0,0,Fixed,CacheSize,"0" },
 {"use-url"      ,&PurgeUseURL        ,0,0,Fixed,Boolean  ,"no"},
 {"del-dontget"  ,&PurgeDontGet       ,0,0,Fixed,Boolean  ,"no"},
 {"del-dontcache",&PurgeDontCache     ,0,0,Fixed,Boolean  ,"no"}
};

/*+ The Purge section. +*/
static ConfigSection purge_section={"Purge",
                                    sizeof(purge_itemdefs)/sizeof(ConfigItemDef),
                                    purge_itemdefs};


/* Whole file */

/*+ The list of sections (NULL terminated). +*/
static ConfigSection *sections[]={&startup_section,
                                  &options_section,
                                  &onlineoptions_section,
                                  &offlineoptions_section,
                                  &fetchoptions_section,
                                  &indexoptions_section,
                                  &modifyhtml_section,
                                  &localhost_section,
                                  &localnet_section,
                                  &allowedconnecthosts_section,
                                  &allowedconnectusers_section,
                                  &dontcache_section,
                                  &dontget_section,
                                  &dontcompress_section,
                                  &censorheader_section,
                                  &ftpoptions_section,
                                  &mimetypes_section,
                                  &proxy_section,
                                  &alias_section,
                                  &purge_section};

/*+ The whole file +*/
ConfigFile CurrentConfig={DEF_CONFDIR "/wwwoffle.conf",sizeof(sections)/sizeof(ConfigSection*),sections};


/*++++++++++++++++++++++++++++++++++++++
  Return the name of the configuration file.

  char *ConfigurationFileName Returns the filename.
  ++++++++++++++++++++++++++++++++++++++*/

char *ConfigurationFileName(void)
{
 return(CurrentConfig.name);
}


/*++++++++++++++++++++++++++++++++++++++
  Initialise the configuration file information to the default values.

  char *name The name of the configuration file.
  ++++++++++++++++++++++++++++++++++++++*/

void InitConfigurationFile(char *name)
{
 static char startup[4][8],options[2][8];

 if(name)
    CurrentConfig.name=name;

 if(*CurrentConfig.name!='/')
   {
    static char cwd[PATH_MAX+1];

    if(getcwd(cwd,PATH_MAX))
      {
       strcat(cwd,"/");
       strcat(cwd,CurrentConfig.name);

       CurrentConfig.name=cwd;
      }
   }

 /* Default values that cannot be set at compile time. */

 sprintf(startup[0],"%d",DEF_HTTP_PORT);         startup_itemdefs[ 2].def_val=startup[0];
 sprintf(startup[1],"%d",DEF_WWWOFFLE_PORT);     startup_itemdefs[ 3].def_val=startup[1];
 sprintf(startup[2],"%d",DEF_MAX_SERVERS);       startup_itemdefs[ 9].def_val=startup[2];
 sprintf(startup[3],"%d",DEF_MAX_FETCH_SERVERS); startup_itemdefs[10].def_val=startup[3];

 sprintf(options[0],"0%o",DEF_DIR_PERM);         options_itemdefs[6].def_val=options[0];
 sprintf(options[1],"0%o",DEF_FILE_PERM);        options_itemdefs[7].def_val=options[1];

 ftpoptions_itemdefs[1].def_val=DefaultFTPPassWord();

 /* Convert the default values. */

 DefaultConfigFile();

 LoggingLevel=ConfigInteger(LogLevel);

 SetDNSTimeout(ConfigInteger(DNSTimeout));
 SetConnectTimeout(ConfigInteger(ConnectTimeout));
 set_read_timeout(ConfigInteger(SocketTimeout));
}


/*++++++++++++++++++++++++++++++++++++++
  Read in the configuration file.

  int fd The file descriptor to write the error message to.
  ++++++++++++++++++++++++++++++++++++++*/

int ReadConfigurationFile(int fd)
{
 char *errmsg;

 errmsg=ReadConfigFile();

 if(errmsg)
   {
    if(fd!=-1)
       write_string(fd,errmsg);
    free(errmsg);

    return(1);
   }
 else
   {
    LoggingLevel=ConfigInteger(LogLevel);

    SetDNSTimeout(ConfigInteger(DNSTimeout));
    SetConnectTimeout(ConfigInteger(ConnectTimeout));
    set_read_timeout(ConfigInteger(SocketTimeout));

    return(0);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Finish with the configuration file (clear memory before eciting).
  ++++++++++++++++++++++++++++++++++++++*/

void FinishConfigurationFile(void)
{
 PurgeConfigFile();

 free(ftpoptions_itemdefs[1].def_val);
}
