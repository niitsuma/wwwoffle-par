TITLE WWWOFFLE - Configuration File - Version 2.7
HEAD
<h2><a name="Introduction">Introduction</a></h2> The configuration file (wwwoffle.conf) specifies all of the parameters that control the operation of the proxy server.  The file is split into sections each containing a series of parameters as described below.  The file CHANGES.CONF explains the changes in the configuration file between this version of the program and previous ones. <p> The file is split into sections, each of which can be empty or contain one or more lines of configuration information.  The sections are named and the order that they appear in the file is not important. <p> The general format of each of the sections is the same.  The name of the section is on a line by itself to mark the start.  The contents of the section are enclosed between a pair of lines containing only the '{' and '}' characters or the '[' and ']' characters.  When the '{' and '}' characters are used the lines between contain configuration information.  When the '[' and ']' characters are used then there must only be a single non-empty line between them that contains the name of a file (in the same directory) containing the configuration information for the section. <p> Comments are marked by a '#' character at the start of the line and they are ignored.  Blank lines are also allowed and ignored. <p> The phrases <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> (or <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> for short) and <a href="/configuration/#WILDCARD">WILDCARD</a> have specific meanings in the configuration file and are described at the end.  Any item enclosed in '(' and ')' in the descriptions means that it is a parameter supplied by the user, anything enclosed in '[' and ']' is optional, the '|' symbol is used to denote alternate choices.  Some options apply to specific URLs only, this is indicated by having a <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> enclosed between '&lt;' &amp; '&gt;' in the option, the first <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> to match is used.  If no <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> is given then it matches all URLs.
SECTION StartUp
This contains the parameters that are used when the program starts, changes to these are ignored if the configuration file is re-read while the program is running.
ITEM bind-ipv4
bind-ipv4 = (hostname) | (ip-address) | none
Specify the hostname or IP address to bind the HTTP proxy and WWWOFFLE control port sockets to using IPv4 (default='0.0.0.0').  If 'none' is specified then no IPv4 socket is bound.  If this is changed from the default value then the first entry in the LocalHost section may need to be changed to match.
ITEM bind-ipv6
bind-ipv6 = (hostname) | (ip-address) | none
Specify the hostname or IP address to bind the HTTP proxy and WWWOFFLE control port sockets to using IPv6 (default='::').  If 'none' is specified then no IPv6 socket is bound.  This requires the IPv6 compilation option.  If this is changed from the default value then the first entry in the LocalHost section may need to be changed to match.
ITEM http-port
http-port = (port)
An integer specifying the port number for the HTTP proxy to use (default=8080).  This is the port number that must be specified in the browser to connect to the WWWOFFLE proxy.
ITEM wwwoffle-port
wwwoffle-port = (port)
An integer specifying the port number for the WWWOFFLE control connections to use (default=8081).
ITEM spool-dir
spool-dir = (dir)
The full pathname of the top level cache directory (spool directory) (default=/var/spool/wwwoffle or whatever was used when the program was compiled).
ITEM run-uid
run-uid = (user) | (uid)
The username or numeric uid to change to when the WWWOFFLE server is started (default=none).  This option is not applicable to win32 and only works if the server is started by root on UNIX.
ITEM run-gid
run-gid = (group) | (gid)
The groupname or numeric gid to change to when the WWWOFFLE server is started (default=none).  This option is not applicable to win32 and only works if the server is started by root on UNIX.
ITEM use-syslog
use-syslog = yes | no
Whether to use the syslog facility for messages or not (default=yes).
ITEM password
password = (word)
The password used for authentication of the control pages, for deleting cached pages etc (default=none).  For the password to be secure the configuration file must be set so that only authorised users can read it.
ITEM max-servers
max-servers = (integer)
The maximum number of server processes that are started for online and automatic fetching (default=8).
ITEM max-fetch-servers
max-fetch-servers = (integer)
The maximum number of server processes that are started to fetch pages that were marked in offline mode (default=4).  This value must be less than max-servers or you will not be able to use WWWOFFLE interactively online while fetching.
SECTION Options
Options that control how the program works.
ITEM log-level
log-level = debug | info | important | warning | fatal
The minimum log level for messages in syslog or stderr (default=important).
ITEM socket-timeout
socket-timeout = (time)
The time in seconds that WWWOFFLE will wait for data on a socket connection before giving up (default=120).
ITEM dns-timeout
dns-timeout = (time)
The time in seconds that WWWOFFLE will wait for a DNS (Domain Name Service) lookup before giving up (default=60).
ITEM connect-timeout
connect-timeout = (time)
The time in seconds that WWWOFFLE will wait for the socket connection to be made before giving up (default=30).
ITEM connect-retry
connect-retry = yes | no
If a connection cannot be made to a remote server then WWWOFFLE should try again after a short delay (default=no).
ITEM ssl-allow-port
ssl-allow-port = (integer)
A port number that is allowed to be proxied for Secure Socket Layer (SSL) connections, e.g. https.  This option should be set to 443 to allow https, there can be more than one ssl-port entry for other ports as required.
ITEM dir-perm
dir-perm = (octal int)
The directory permissions to use when creating spool directories (default=0755).  This option overrides the umask of the user and must be in octal starting with a '0'.
ITEM file-perm
file-perm = (octal int)
The file permissions to use when creating spool files (default=0644). This option overrides the umask of the user and must be in octal starting with a '0'.
ITEM run-online
run-online = (filename)
The name of a program to run when WWWOFFLE is switched to online mode (default=none).  The program is started with a single parameter set to the current mode name "online".
ITEM run-offline
run-offline = (filename)
The name of a program to run when WWWOFFLE is switched to offline mode (default=none).  The program is started with a single parameter set to the current mode name "offline".
ITEM run-autodial
run-autodial = (filename)
The name of a program to run when WWWOFFLE is switched to autodial mode (default=none).  The program is started with a single parameter set to the current mode name "autodial".
ITEM run-fetch
run-fetch = (filename)
The name of a program to run when a WWWOFFLE fetch starts or stops (default=none).  The program is started with two parameters, the first is the word "fetch" and the second is "start" or "stop".
ITEM lock-files
lock-files = yes | no
Enable the use of lock files to stop more than one WWWOFFLE process from downloading the same URL at the same time (default=no).
ITEM reply-compressed-data
reply-compressed-data = yes | no
If the replies that are made to the browser are to contain compressed data when requested (default=no).  Requires zlib compilation option.
ITEM exec-cgi
exec-cgi = (pathname)
Enable the use of CGI scripts for the local pages on the WWWOFFLE server that match the wildcard pathname (default=none).
SECTION OnlineOptions
Options that control how WWWOFFLE behaves when it is online.
ITEM request-changed
[<URL-SPEC>] request-changed = (time)
While online pages will only be fetched if the cached version is older than this specified time in seconds (default=600).  Setting this value negative will indicate that cached pages are always used while online. Longer times can be specified with a 'm', 'h', 'd' or 'w' suffix for minutes, hours, days or weeks (e.g. 10m=600).
ITEM request-changed-once
[<URL-SPEC>] request-changed-once = yes | no
While online pages will only be fetched if the cached version has not already been fetched once this session online (default=yes).  This option takes precedence over the request-changed option.
ITEM request-expired
[<URL-SPEC>] request-expired = yes | no
While online pages that have expired will always be requested again (default=no).  This option takes precedence over the request-changed and request-changed-once options.
ITEM request-no-cache
[<URL-SPEC>] request-no-cache = yes | no
While online pages that ask not to be cached will always be requested again (default=no).  This option takes precedence over the request-changed and request-changed-once options.
ITEM request-redirection
[<URL-SPEC>] request-redirection = yes | no
While online pages that redirect the browser to another URL temporarily will be requested again. (default=no).  This option takes precedence over the request-changed and request-changed-once options.
ITEM try-without-password
[<URL-SPEC>] try-without-password = yes | no
If a request is made for a page that contains a username and password then a request is made for the same page without a username and password specified (default=yes).  This allows for requests for the page without a password to re-direct the browser to the passworded version.
ITEM intr-download-keep
[<URL-SPEC>] intr-download-keep = yes | no
If the browser closes the connection while online the currently downloaded incomplete page should be kept (default=no).
ITEM intr-download-size
[<URL-SPEC>] intr-download-size = (integer)
If the browser closes the connection while online the page should continue to download if it is smaller than this size in kB (default=1).
ITEM intr-download-percent
[<URL-SPEC>] intr-download-percent = (integer)
If the browser closes the connection while online the page should continue to download if it is more than this percentage complete (default=80).
ITEM timeout-download-keep
[<URL-SPEC>] timeout-download-keep = yes | no
If the server connection times out while reading then the currently downloaded incomplete page should be kept (default=no).
ITEM request-compressed-data
[<URL-SPEC>] request-compressed-data = yes | no
If the requests that are made to the server are to request compressed data (default=yes).  Requires zlib compilation option.
SECTION OfflineOptions
Options that control how WWWOFFLE behaves when it is offline.
ITEM pragma-no-cache
[<URL-SPEC>] pragma-no-cache = yes | no
Whether to request a new copy of a page if the request from the browser has 'Pragma: no-cache' (default=yes).  This option option should be set to 'no' if when browsing offline all pages are re-requested by a 'broken' browser.
ITEM confirm-requests
[<URL-SPEC>] confirm-requests = yes | no
Whether to return a page requiring user confirmation instead of automatically recording requests made while offline (default=no).
ITEM dont-request
[<URL-SPEC>] dont-request = yes | no
Do not request any URLs that match this when offline (default=no).
SECTION FetchOptions
Options that control what is downloaded when fetching pages that were requested while offline.
ITEM stylesheets
[<URL-SPEC>] stylesheets = yes | no
If style sheets are to be fetched (default=no).
ITEM images
[<URL-SPEC>] images = yes | no
If images are to be fetched (default=no).
ITEM webbug-images
[<URL-SPEC>] webbug-images = yes | no
If images that are 1 pixel square are also to be fetched, requires the images option to also be selected. (default=yes). This option is intended to be used in conjunction with the replace-webbug-images option in the ModifyHTML section.
ITEM only-same-host-images
[<URL-SPEC>] only-same-host-images = yes | no
If the only images that are fetched are the ones that are on the same host as the page that references them (default=no).
ITEM frames
[<URL-SPEC>] frames = yes | no
If frames are to be fetched (default=no).
ITEM scripts
[<URL-SPEC>] scripts = yes | no
If scripts (e.g. Javascript) are to be fetched (default=no).
ITEM objects
[<URL-SPEC>] objects = yes | no
If objects (e.g. Java class files) are to be fetched (default=no).
SECTION IndexOptions
Options that control what is displayed in the indexes.
ITEM no-lasttime-index
no-lasttime-index = yes | no
Disables creation of the lasttime/prevtime indexes (default=no).
ITEM cycle-indexes-daily
cycle-indexes-daily = yes | no
Cycles the lasttime/prevtime and lastout/prevout indexes daily instead of each time online or fetching (default = no).
ITEM list-outgoing
<URL-SPEC> list-outgoing = yes | no
Choose if the URL is to be listed in the outgoing index (default=yes).
ITEM list-latest
<URL-SPEC> list-latest = yes | no
Choose if the URL is to be listed in the lasttime/prevtime and lastout/prevout indexes (default=yes).
ITEM list-monitor
<URL-SPEC> list-monitor = yes | no
Choose if the URL is to be listed in the monitor index (default=yes).
ITEM list-host
<URL-SPEC> list-host = yes | no
Choose if the URL is to be listed in the host indexes (default=yes).
ITEM list-any
<URL-SPEC> list-any = yes | no
Choose if the URL is to be listed in any of the indexes (default=yes).
SECTION ModifyHTML
Options that control how the HTML that is provided from the cache is modified.
ITEM enable-modify-html
[<URL-SPEC>] enable-modify-html = yes | no
Enable the HTML modifications in this section (default=no).  With this option disabled the following HTML options will not have any effect. With this option enabled there is a small speed penalty.
ITEM enable-modify-online
[<URL-SPEC>] enable-modify-online = yes | no
Enable the modifications in this section to take place when online as well as when offline (default=no).  This will cause the HTML or GIF to not appear in the browser until WWWOFFLE has processed it all. This still does not apply to pages that are not cached.
ITEM add-cache-info
[<URL-SPEC>] add-cache-info = yes | no
At the bottom of all of the spooled pages the date that the page was cached and some navigation buttons are to be added (default=no).
ITEM anchor-cached-begin
[<URL-SPEC>] anchor-cached-begin = (HTML code) | 
Anchors (links) in the spooled page that are in the cache are to have the specified HTML inserted at the beginning (default="").
ITEM anchor-cached-end
[<URL-SPEC>] anchor-cached-end = (HTML code) | 
Anchors (links) in the spooled page that are in the cache are to have the specified HTML inserted at the end (default="").
ITEM anchor-requested-begin
[<URL-SPEC>] anchor-requested-begin = (HTML code) | 
Anchors (links) in the spooled page that have been requested for download are to have the specified HTML inserted at the beginning (default="").
ITEM anchor-requested-end
[<URL-SPEC>] anchor-requested-end = (HTML code) | 
Anchors (links) in the spooled page that have been requested for download are to have the specified HTML inserted at the end (default="").
ITEM anchor-not-cached-begin
[<URL-SPEC>] anchor-not-cached-begin = (HTML code) | 
Anchors (links) in the spooled page that are not in the cache or requested are to have the specified HTML inserted at the beginning (default="").
ITEM anchor-not-cached-end
[<URL-SPEC>] anchor-not-cached-end = (HTML code) | 
Anchors (links) in the spooled page that are not in the cache or requested are to have the specified HTML inserted at the end (default="").
ITEM disable-script
[<URL-SPEC>] disable-script = yes | no
Removes all scripts and scripted events (default=no).
ITEM disable-applet
[<URL-SPEC>] disable-applet = yes | no
Removes all Java applets (default=no).
ITEM disable-style
[<URL-SPEC>] disable-style = yes | no
Removes all stylesheets and style references (default=no).
ITEM disable-blink
[<URL-SPEC>] disable-blink = yes | no
Removes the &lt;blink&gt; tag (default=no).
ITEM disable-flash
[<URL-SPEC>] disable-flash = yes | no
Removes any Shockwave Flash animations (default=no).
ITEM disable-meta-refresh
[<URL-SPEC>] disable-meta-refresh = yes | no
Removes any meta tags in the HTML header that re-direct the browser to change to another page after an optional delay (default=no).
ITEM disable-meta-refresh-self
[<URL-SPEC>] disable-meta-refresh-self = yes | no
Removes any meta tags in the HTML header that re-direct the browser to reload the same page after a delay (default=no).
ITEM disable-dontget-links
[<URL-SPEC>] disable-dontget-links = yes | no
Disables any links to URLs that are in the DontGet section of the configuration file (default=no).
ITEM disable-dontget-iframes
[<URL-SPEC>] disable-dontget-iframes = yes | no
Disables inline frame (iframe) URLs that are in the DontGet section of the configuration file (default=no).
ITEM replace-dontget-images
[<URL-SPEC>] replace-dontget-images = yes | no
Replaces image URLs that are in the DontGet section of the configuration file with a static URL (default=no).
ITEM replacement-dontget-image
[<URL-SPEC>] replacement-dontget-image = (URL)
The replacement image to use for URLs that are in the DontGet section of the configuration file (default=/local/dontget/replacement.gif).
ITEM replace-webbug-images
[<URL-SPEC>] replace-webbug-images = yes | no
Replaces image URLs that are 1 pixel square with a static URL (default=no). This option is intended to be used in conjunction with the webbug-images option in the FetchOptions section.
ITEM replacement-webbug-image
[<URL-SPEC>] replacement-webbug-image = (URL)
The replacement image to use for images that are 1 pixel square (default=/local/dontget/replacement.gif).
ITEM disable-animated-gif
[<URL-SPEC>] disable-animated-gif = yes | no
Disables the animation in animated GIF files (default=no).
ITEM demoronise-ms-chars
[<URL-SPEC>] demoronise-ms-chars = yes | no
Replaces strange characters that some Microsoft Applications put into HTML with character equivalents that most browsers can display (default=no).  The idea for this comes from the public domain Demoroniser perl script.
SECTION LocalHost
A list of hostnames that the host running the WWWOFFLE server may be known by. This is so that the proxy does not need to contact itself if the request has a different name for the same server.
ITEM 
(host)
A hostname or IP address that in connection with the port number (in the StartUp section) specifies the WWWOFFLE proxy HTTP server.  The hostnames must match exactly, it is not a <a href="/configuration/#WILDCARD">WILDCARD</a> match.  The first named host is used as the server name for several features so must be a name that will work from any client host on the network.  None of the hosts named here are cached or fetched via a proxy.
SECTION LocalNet
A list of hostnames whose web servers are always accessible even when offline and are not to be cached by WWWOFFLE because they are on a local network.
ITEM 
(host)
A hostname or IP address that is always available and is not to be cached by WWWOFFLE.  The host name matching uses <a href="/configuration/#WILDCARD">WILDCARD</a>s.  A host can be excluded by appending a '!' to the start of the name, all possible aliases and IP addresses for the host are also required.  All entries here are assumed to be reachable even when offline.  None of the hosts named here are cached or fetched via a proxy.
SECTION AllowedConnectHosts
A list of client hostnames that are allowed to connect to the server.
ITEM 
(host)
A hostname or IP address that is allowed to connect to the server. The host name matching uses <a href="/configuration/#WILDCARD">WILDCARD</a>s.  A host can be excluded by appending a '!' to the start of the name, all possible aliases and IP addresses for the host are also required.  All of the hosts named in LocalHost are also allowed to connect.
SECTION AllowedConnectUsers
A list of the users that are allowed to connect to the server and their passwords.
ITEM 
(username):(password)
The username and password of the users that are allowed to connect to the server.  If this section is left empty then no user authentication is done.  The username and password are both stored in plaintext format.  This requires the use of browsers that handle the HTTP/1.1 proxy authentication standard.
SECTION DontCache
A list of URLs that are not to be cached by WWWOFFLE.
ITEM 
[!]URL-SPECIFICATION
Do not cache any URLs that match this.  The <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> can be negated to allow matches to be cached.  The URLs that are not cached will not be requested if offline.
SECTION DontGet
A list of URLs that are not to be got by WWWOFFLE (because they contain only junk adverts for example).
ITEM 
[!]URL-SPECIFICATION
Do not get any URLs that match this.  The <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> can be negated to allow matches to be got.
ITEM replacement
[<URL-SPEC>] replacement = (URL)
The URL to use to replace any URLs that match the <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a>s instead of using the standard error message (default=none).  The URLs in /local/dontget/ are suggested replacements (e.g. replacement.gif or replacement.png which are 1x1 pixel transparent images or replacement.js which is an empty javascript file).
ITEM get-recursive
<URL-SPEC> get-recursive = yes | no
Choose whether to get URLs that match this when doing a recursive fetch (default=yes).
ITEM location-error
<URL-SPEC> location-error = yes | no
When a URL reply contains a 'Location' header that redirects to a URL that is not got (specified in this section) then the reply is modified to be an error message instead (default=no).  This will stop ISP proxies from redirecting users to adverts if the advert URLs are in this section.
SECTION DontCompress
A list of MIME types and file extensions that are not to be compressed by WWWOFFLE (because they are already compressed or not not worth compressing). Requires zlib compilation option.
ITEM mime-type
mime-type = (mime-type)/(subtype)
The MIME type of a URL that is not to be compressed in the cache or when providing compressed pages to browsers.
ITEM file-ext
file-ext = .(file-ext)
The file extension of a URL that is not to be requested compressed from a server.
SECTION CensorHeader
A list of HTTP header lines that are to be removed from the requests sent to web servers and the replies that come back from them.
ITEM 
[<URL-SPEC>] (header) = yes | no | (string)
A header field name (e.g. From, Cookie, Set-Cookie, User-Agent) and the string to replace the header value with (default=no).  The header is case sensitive, and does not have a ':' at the end.  The value of "no" means that the header is unmodified, "yes" or no string can be used to remove the header or a string can be used to replace the header.  This only replaces headers it finds, it does not add any new ones.  An option for Referer here will take precedence over the referer-self and referer-self-dir options.
ITEM referer-self
[<URL-SPEC>] referer-self = yes | no
Sets the Referer header to the same as the URL being requested (default=no).  This will add the Referer header if none is contained in the original request.
ITEM referer-self-dir
[<URL-SPEC>] referer-self-dir = yes | no
Sets the Referer header to the directory name of the URL being requested (default=no).  This will add the Referer header if none is contained in the original request.  This option takes precedence over referer-self.
SECTION FTPOptions
Options to use when fetching files using the ftp protocol.
ITEM anon-username
anon-username = (string)
The username to use for anonymous ftp (default=anonymous).
ITEM anon-password
anon-password = (string)
The password to use for anonymous ftp (default determined at run time).  If using a firewall then this may contain a value that is not valid to the FTP server and may need to be set to a different value.
ITEM auth-username
<URL-SPEC> auth-username = (string)
The username to use on a host instead of the default anonymous username.
ITEM auth-password
<URL-SPEC> auth-password = (string)
The password to use on a host instead of the default anonymous password.
SECTION MIMETypes
MIME Types to use when serving files that were not fetched using HTTP or for files on the built-in web-server.
ITEM default
default = (mime-type)/(subtype)
The default MIME type (default=text/plain).
ITEM 
.(file-ext) = (mime-type)/(subtype)
The MIME type to associate with a file extension.  The '.' must be included in the file extension.  If more than one extension matches then the longest one is used.
SECTION Proxy
This contains the names of the HTTP (or other) proxies to use external to the WWWOFFLE server machine.
ITEM proxy
[<URL-SPEC>] proxy = (host[:port])
The hostname and port on it to use as the proxy.
ITEM auth-username
<URL-SPEC> auth-username = (string)
The username to use on a proxy host to authenticate WWWOFFLE to it. The <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> in this case refers to the proxy and not the URL being retrieved.
ITEM auth-password
<URL-SPEC> auth-password = (string)
The password to use on a proxy host to authenticate WWWOFFLE to it. The <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> in this case refers to the proxy and not the URL being retrieved.
ITEM ssl
[<URL-SPEC>] ssl = (host[:port])
A proxy server that should be used for Secure Socket Layer (SSL) connections e.g. https.  Note that for the &lt;<a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a>&gt; that only the host is checked and that the other parts must be '*' <a href="/configuration/#WILDCARD">WILDCARD</a>s.
SECTION Alias
A list of aliases that are used to replace the server name and path with another server name and path.
ITEM 
URL-SPECIFICATION = URL-SPECIFICATION
Any requests that match the first <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> are replaced by the second <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a>.  The first <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> is a wildcard match for the protocol and host/port, the path must match the start of the requested URL exactly and includes all subdirectories.
SECTION Purge
The method to determine which pages to purge, the default age the host specific maximum age of the pages in days, and the maximum cache size.
ITEM use-mtime
use-mtime = yes | no
The method to use to decide which files to purge, last access time (atime) or last modification time (mtime) (default=no).
ITEM max-size
max-size = (size)
The maximum size for the cache in MB after purging (default=0).  A maximum cache size of 0 means there is no limit to the size.  If this and the min-free options are both used the smaller cache size is chosen.  This option take into account the URLs that are never purged when measuring the cache size but will not purge them.
ITEM min-free
min-free = (size)
The minimum amount of free disk space in MB after purging (default=0). A minimum disk free of 0 means there is no limit to the free space. If this and the max-size options are both used the smaller cache size is chosen.  This option take into account the URLs that are never purged when measuring the cache size but will not purge them.
ITEM use-url
use-url = yes | no
If true then use the URL to decide on the purge age, otherwise use the protocol and host only (default=no).
ITEM del-dontget
del-dontget = yes | no
If true then delete the URLs that match the entries in the DontGet section (default=no).
ITEM del-dontcache
del-dontcache = yes | no
If true then delete the URLs that match the entries in the DontCache section (default=no).
ITEM age
[<URL-SPEC>] age = (age)
The maximum age in the cache for URLs that match this (default=14). An age of zero means not to keep, negative not to delete.  The <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> matches only the protocol and host unless use-url is set to true. Longer times can be specified with a 'w', 'm' or 'y' suffix for weeks, months or years (e.g. 2w=14).
ITEM compress-age
[<URL-SPEC>] compress-age = (age)
The maximum age in the cache for URLs that match this to be stored uncompressed (default=-1).  Requires zlib compilation option.  The age that is specified has the same meaning as for the age option.
TAIL
<h2><a name="WILDCARD">WILDCARD</a></h2> A <a href="/configuration/#WILDCARD">WILDCARD</a> match is one that uses the '*' character to represent any group of characters. <p> This is basically the same as the command line file matching expressions in DOS or the UNIX shell, except that the '*' can match the '/' character. <p> For example<dl><dt>*.gif<dd>matches  foo.gif and bar.gif</dl><dl><dt>*.foo.com<dd>matches  www.foo.com and ftp.foo.com</dl><dl><dt>/foo/*<dd>matches  /foo/bar.html and /foo/bar/foobar.html</dl><h2><a name="URL-SPECIFICATION">URL-SPECIFICATION</a></h2> When specifying a host and protocol and pathname in many of the sections a <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> can be used, this is a way of recognising a URL. <p> For the purposes of this explanation a URL is considered to be made up of five parts.<dl><dt>proto<dd>The protocol that is used (e.g. 'http', 'ftp')</dl><dl><dt>host<dd>The server hostname (e.g. 'www.gedanken.demon.co.uk').</dl><dl><dt>port<dd>The port number on the host (e.g. default of 80 for HTTP).</dl><dl><dt>path<dd>The pathname on the host (e.g. '/bar.html') or a directory name (e.g. '/foo/').</dl><dl><dt>args<dd>Optional arguments with the URL used for CGI scripts etc. (e.g. 'search=foo').</dl> <p> For example the WWWOFFLE homepage: http://www.gedanken.demon.co.uk/wwwoffle/ The protocol is 'http', the host is 'www.gedanken.demon.co.uk', the port is the default (in this case 80), and the pathname is '/wwwoffle/'. <p> In general this is written as (proto)://(host)[:(port)]/[(path)][?(args)] <p> Where [] indicates an optional feature, and () indicate a user supplied name or number. <p> Some example <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> options are the following:<dl><dt>*://*/*<dd>Any protocol, Any host, Any port, Any path, Any args (This is that same as saying 'default').</dl><dl><dt>*://*/(path)<dd>Any protocol, Any host, Any port, Named path, Any args</dl><dl><dt>*://*/*?<dd>Any protocol, Any host, Any port, Any path, No args</dl><dl><dt>*://*/(path)?*<dd>Any protocol, Any host, Any port, Named path, Any args</dl><dl><dt>*://(host)<dd>Any protocol, Named host, Any port, Any path, Any args</dl><dl><dt>(proto)://*/*<dd>Named protocol, Any host, Any port, Any path, Any args</dl> <p> (proto)://(host)/*  Named protocol, Named host, Any port, Any path, Any args <p> (proto)://(host):/* Named protocol, Named host, Default port, Any path Any args <p> *://(host):(port)/* Any protocol, Named host, Named port, Any path, Any args <p> The matching of the host, the path and the args use the <a href="/configuration/#WILDCARD">WILDCARD</a> matching that is described above. <p> In some sections that accept <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a>s they can be negated by inserting the '!' character before it.  This will mean that the comparison of a URL with the <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> will return the logically opposite value to what would be returned without the '!'.  If all of the <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a>s in a section are negated and '*://*/*' is added to the end then the sense of the whole section is negated. <p> In all sections that accept <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a>s the comparison can be made case insensitive for the path and arguments part by inserting the '~' character before it.  (The host and the protocol comparisons are always case insensitive).
