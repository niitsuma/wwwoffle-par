TITLE WWWOFFLE - Configuration File - Version 2.7
HEAD
<h2><a name="Introduction">Einf&uuml;hrung</a></h2> Die Konfigurations-Datei (wwwoffle.conf) beinhaltet alle Parameter, die den Betrieb des Proxy-Sservers steuern.  Diese Datei ist in Abschnitte aufgeteilt, die jeweils eine Reihe von Parametern wie unten beschrieben beinhalten. Die Datei CHANGES.CONF erl&auml;utert die &Auml;nderungen in der Konfigurationsdatei zwischen dieser Version des Programms und fr&uuml;heren Versionen.<p> Die Datei ist in Abschnitte unterteilt, die jeweils leer sein k&ouml;nnen oder einen oder mehrere Zeilen Konfigurations-Informationen beinhalten k&ouml;nnen. Die Abschnitte sind benannt und die Reihenfolge in der sie in der Datei erscheinen ist unwichtig.<p> Das allgemeine Format der jeweiligen Abschnitte is das gleiche. Der Name des Abschnitts steht in einer eigenen Zeile um des Beginn des Abschnitts zu markieren. Der Inhalt des Abschnits ist durch ein Paar von Zeilen eingeschlossen, die nur die Zeichen '{' und '}'  oder '[' und ']' enthalten.  Wenn die Zeichen '{' und '}' verwendet werden, sind die Zeilen dazwischen Konfigurations-Informationen.  Werden die Zeichen '[' und ']' verwendet, dann darf dazwischen nur eine einzige nicht leere Zeile stehen, die den Namen der Datei (im gleichen Verzeichnis) enth&auml;lt, in der die Konfigurations-Informationen dieses Abschnitts gespeichert sind.<p> Kommentare werden durch das Zeichen '#' am Anfang der Zeile gekennzeichnet und werden ignoriert. Leerzeilen sind ebenfalls zul&auml;ssig und werden ignoriert. <p> Die Ausdr&uuml;cke <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> (oder kurz <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a>) und <a href="/configuration/#WILDCARD">WILDCARD</a> haben besondere Bedeutungen in der Konfigurations-Datei und sind am Ende der Datei beschrieben.  Jede in in '(' und ')' eingeschlossene Option in den Beschreibungen meint, dass ein Parameter vom Benutzer angegeben werden soll, alle zwischen '[' and ']' ist optional, das Symbol '|' wird zur Kennzeichnung alternativer M&ouml;glichkeiten verwendet.  Einige Optionen sind nur auf bestimmte URLs anzuwenden, dies wird durch ein <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> von '&lt;' &amp; '&gt;' eingeschlossen in der Option gekennzeichnet, die erste passende <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a> wird verwendet.  Ist keine <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a> gegeben, passt dazu jede URL.
SECTION StartUp
Dieser Abschnitt enth&auml;lt die Parameter, die w&auml;hrend des Programm-Starts verwendet werden, &Auml;nderungen and diesem Abschnitts werden igoriert, wenn die Konfigurations-Datei neu eingelesen wird w&auml;hrend das Programm l&auml;uft.
ITEM bind-ipv4
bind-ipv4 = (hostname) | (ip-address) | none
Geben Sie den Hostnamen oder die IP-Adresse, an die der HTTP-Proxy und der WWWOFFLE-Steuerungs-Port unter Verwendung von IPv4 gebunden werden soll (Standard='0.0.0.0'). Wir hier 'none' angegeben, so wird kein IPv4-Socket gebunden. Wird der Standardwert ge&auml;ndert, so mu&szlig; evtl. der erste Eintrag im Abschnitt Localhost auch passend ge&auml;ndert werden.
ITEM bind-ipv6
bind-ipv6 = (hostname) | (ip-address) | none
Geben Sie den Hostnamen oder die IP-Adresse, an die der HTTP-Proxy und der WWWOFFLE-Steuerungs-Port unter Verwendung von IPv6 gebunden werden soll (Standard='::'). Wird hier 'none' angegeben, so wird kein IPv6-Socket gebunden. Diese Option ben&ouml;tigt die IPv6-Option beim compilieren. Wird der Standardwert ge&auml;ndert, so mu&szlig; evtl. der erste Eintrag im Abschnit Localhost auch passend ge&auml;ndert werden.
ITEM http-port
http-port = (port)
Geben Sie eine Integer-Zahl f&uuml;r die Port-Nummer f&uuml;r den HTTP-Proxy an (Standard:8080). Dies ist die Port-Nummer, die auch im Browser angegeben werden muss, um eine Verbindung zum WWWOFFLE-Proxy zu bekommen.
ITEM wwwoffle-port
wwwoffle-port = (port)
Eine Integer-Zahl, die die Port-Nummer, die f&uuml;r die WWWOFFLE-Steuerungs-Verbindungen verwendet wird festlegt. (Standard:8081)
ITEM spool-dir
spool-dir = (dir)
Der komplette Pfadname des Top-Level-Cache-Verzeichnisses (Spool-Verzeichnis) (Standard:/var/spool/wwwoffle oder wasimmer verwendet wurde, als das Programm compiliert wurde).
ITEM run-uid
run-uid = (user) | (uid)
Der Benutzname oder die numerische UID, auf die gewechselt werden soll, wenn der Server gestartet wird (Standard:keine). Diese Option ist nicht f&uuml;r die Win32-Version anwendbar und funktioniert nur, wenn der Server unter UNIX als root gestartet wurde.
ITEM run-gid
run-gid = (group) | (gid)
Der Gruppenname oder die numerische GID, auf die gewechselt werden soll, wenn der Server gestartet wird (Standard:keine). Diese Option ist nicht f&uuml;r die Win32-Version anwendbar und funktioniert nur, wenn der Server unter UNIX als root gestartet wurde.
ITEM use-syslog
use-syslog = yes | no
Legt fest, ob der Server von Syslog f&uuml;r Meldungen Gebrauch machen soll (yes) oder nicht (no) (Standard:yes)
ITEM password
password = (word)
Dass Passwort, da&szlig; zur Authentifizierung f&uuml;r die Steuerungs-Seiten, f&uuml;r das L&ouml;schen gespeicherter Seiten usw. ben&ouml;tigt wird (Standard:keins). Damit dieses Passwort in der Konfigurations-Datei sicher ist, muss die Zugriffsrechte der Datei so eingestellt werden, dass nur authorisierte Benutzer sie lesen k&ouml;nnen.
ITEM max-servers
max-servers = (integer)
Die maximale Anzahl der Server-Prozesse, die bei Online und automatischen Abrufen gestartet werden (Standard:8)
ITEM max-fetch-servers
max-fetch-servers = (integer)
Die maximale Anzahld der Server-Prozesse, die bei Start des Abrufs der im Offline-Modus markierten Seiten gestartet werden (Standard:4) Dieser Wert mu&szlig; kleiner als die max.Anzahl der Server sein, oder Sie sind nicht Lage WWWOFFLE w&auml;hrend des Abrufs interaktiv zu nutzen.
SECTION Options
Optionen, die steuern, wie das Programm arbeitet
ITEM log-level
log-level = debug | info | important | warning | fatal
Der minimale Log-Level f&uuml;r Meldungen in Syslog oder stderr (Standard:important)
ITEM socket-timeout
socket-timeout = (time)
Die Zeit in Sekunden, die WWWOFFLE maximal auf Daten von einer Verbindung wartet, bevor der Vorgang aufgegeben wird (Standard:120)
ITEM dns-timeout
dns-timeout = (time)
Die Zeit in Sekunden, die WWWOFFLE auf die Aufl&ouml;sung eines Hostnamens (DNS=Domain Name Service) wartet, bevor der Vorgang aufgegeben wird (Standard:60)
ITEM connect-timeout
connect-timeout = (time)
Doe Zeit in Sekunden, die WWWOFFLE auf den Verbindungsaufbau wartet, bevor der Vorgang aufgegeben wird (Standard=30)
ITEM connect-retry
connect-retry = yes | no
Falls die Verbindung zu einem entfernten Server nihct hergestellt werden kann, soll WWWOFFLE nach eine kurzen Pause es erneut versuchen (Standard=no - also nein)
ITEM ssl-allow-port
ssl-allow-port = (integer)
Eine Port-Nummer &uuml;ber die verschl&uuml;sselte Secure Socket Layer (SSL)-Verbindungen, also https &uuml;ber den Proxy laufen d&uuml;rfen. Diese Options sollte auf 443 gesetzt werden, um https zu erlauben, es darf mehr als einen ssl-port-Eintag geben, wenn andere Ports ben&ouml;tigt werden.
ITEM dir-perm
dir-perm = (octal int)
Die Verzeichnis-Zugriffsrechte, die beim Erstellen von Spool-Verzeichnissen verwendet werden (Standard: 0755). Diese Option &uuml;bergeht die umask-Einstellungen des Benutzers und muss als Oktal-Wert beginnend mit '0' angegeben werden.
ITEM file-perm
file-perm = (octal int)
Die Datei-Zugriffsrechte, die beim Erstellen von Spool-Files verwendet werden (Standard: 0644). Diese Option &uuml;bergeht die umask-Einstellungen des Benutzers und muss als Oktal-Wert beginnen mit '0' angegeben werden.
ITEM run-online
run-online = (filename)
Der Name eines Programms, das gestartet werden soll, wenn WWWOFFLE in den online-Modus geschaltet wird (Standard:keins). Das Programm wird mit einem einzigen Parameter gestartet, dem aktuellen Modus-Namen "online".
ITEM run-offline
run-offline = (filename)
Der Name eines Programms, das gestartet werden soll, wenn WWWOFFLE in den offline-Modus geschaltet wird (Standard:keins). Das Programm wird mit einem einzigen Parameter gestartet, dem aktuellen Modus-Namen "offline".
ITEM run-autodial
run-autodial = (filename)
Der Name eines Programms, das gestartet werden soll, wenn WWWOFFLE in den autodial-Modus geschaltet wird (Standard:keins). Das Programm wird mit einem einzigen Parameter gestartet, dem aktuellen Modus-Namen "autodial".
ITEM run-fetch
run-fetch = (filename)
Der Namen eines Programms, das gestartet werden soll wenn WWWOFFLE mit den Abruf von Bestellungen beginnt oder beendet. Das Programm wird mit zwei Parametern gestartet, der erste ist das Wort "fetch" und der zweite ist "start" oder "stop".
ITEM lock-files
lock-files = yes | no
Schalter zur Verwendung eines Lock-Files, um zu verhindern, dass mehr als ein WWWOFFLE-Prozess die gleiche URL zur gleichen Zeit abruft (Standard=no - kein Lockfile)
ITEM reply-compressed-data
reply-compressed-data = yes | no
Ob die Antworten die an den Browser gegeben werden, auf Anfrage komprimierte Daten enthalten sollen (Standard:no = Nein). Ben&ouml;tigt die zlib-Option beim Compilieren.
ITEM exec-cgi
exec-cgi = (pathname)
Schaltet die Verwendung von CGI-Scripts f&uuml;r die lokalen Seiten auf dem WWWOFFLE-Server, die der angegeben Pfad Wildcard entsprechen (Standard:keine)
SECTION OnlineOptions
Optionen, die das Verhalten von WWWOFFLE im Online-Modus steueren
ITEM request-changed
[<URL-SPEC>] request-changed = (time)
Im Online-Modus werden Seiten nur dann wirklich aus dem Netz geholt, wenn die Version im Cache &auml;lter als die angegebene Zeit in Sekunden ist (Standard=600). Wird dieser Wert auf einen negativen Wert gesetzt, zeigt dies an, dass die Seiten im Cache immer im Online-Modus verwendet werden. L&auml;ngere Zeitangaben k&ouml;nnen durch Verwendung von 'm' (Minuten),'h (Stunden)','d' (Tage),'w' (Wochen) angegeben werden (z.B. 10m=600).
ITEM request-changed-once
[<URL-SPEC>] request-changed-once = yes | no
Im Online-Modus werden Seiten nur dann wirklich aus dem Netz geholt, wenn die Version im Cache nicht schon einmal in der gleichen Online-Sitzung abgerufen wurde. (Standard=yes - Ja). Diese Option hat gegen&uuml;ber der request-changed-Option Vorrang.
ITEM request-expired
[<URL-SPEC>] request-expired = yes | no
Im Online-Modus werden abgelaufene Seiten immer nochmal abgerufen (Standard=no - nein). Diese Option hat gegen&uuml;ber der request-changed-Option und der request-changed-once-Option Vorrang.
ITEM request-no-cache
[<URL-SPEC>] request-no-cache = yes | no
Im Online-Modus werden Seiten die nicht im Cache gespeichert werden sollen immer neu abgerufen (Standard=no nein). Diese Option hat gegen&uuml;ber den request-changed-  und  reqest-changed-once-Optionen Vorrang.
ITEM request-redirection
[<URL-SPEC>] request-redirection = yes | no
Im Online-Modus werden Seiten die den Browser zeitweile auf eine andere URL umleiten neu abgerufen (Standard=no - nein). Diese Option hat gegen&uuml;ber den request-changed-  und  reqest-changed-once-Optionen Vorrang.
ITEM try-without-password
[<URL-SPEC>] try-without-password = yes | no
Wird eine Seite die einen Usernamen und ein Passwort enth&auml;lt abgerufen, dann wird ein Abruf der gleichen Seite ohne angegebenen Usernamen und Passwort durchgef&uuml;hrt. (Standard=yes - Ja). Dies erlaubt es f&uuml;r Anfragen nach der Seite ohne ein Passwort den Browser auf die Seiten mit Passwort umzuleiten.
ITEM intr-download-keep
[<URL-SPEC>] intr-download-keep = yes | no
Wenn im Online-Modus der Browser die Verbindung schliesst, soll die gerade heruntergeladene, unvollst&auml;ndige Seite erhalten bleiben. (Standard: no - nein)
ITEM intr-download-size
[<URL-SPEC>] intr-download-size = (integer)
Wenn im Online-Modus der Browser die Verbindung schliesst, wird die Seite weitergeladen, wenn sie kleiner als diese Gr&ouml;sse in kB ist. (Standard=1).
ITEM intr-download-percent
[<URL-SPEC>] intr-download-percent = (integer)
Wenn im Online-Modus der Browser die Verbindung schliesst, wird die Seite weitergeladen, wenn sie zu mehr als dieser Prozentangabe bereits vollst&auml;ndig ist (Standard=80)
ITEM timeout-download-keep
[<URL-SPEC>] timeout-download-keep = yes | no
Wenn die Server-Verbindung w&auml;hrend eines Lesezugriffs zu lange keine Daten sendet (timeout), soll die gerade heruntergeladene, unvollst&auml;ndige Seite erhalten bleiben (Standard=no - nein).
ITEM request-compressed-data
[<URL-SPEC>] request-compressed-data = yes | no
Ob die Anfragen die an die Server geschickt werden, komprimierte Daten anfordern solllen (Standard=yes - ja). Ben&ouml;tigt die zlib-Option beim Compilieren.
SECTION OfflineOptions
Optionen, die das Verhalten von WWWOFFLE im Offline-Modus steuern
ITEM pragma-no-cache
[<URL-SPEC>] pragma-no-cache = yes | no
Ob eine neue Kopie der Seite abgerufen werden soll, wenn der Browser in der Anfrage 'Pragma: no-cache' gesetzt hat (Standard=yes - Ja). Diese Option sollte auf 'no' (Nein) gesetzt werden, wenn beim Offline-Browsen alle Seiten von einem "fehlerhaften" Browser neu angefragt werden.
ITEM confirm-requests
[<URL-SPEC>] confirm-requests = yes | no
Ob im Offline-Modus jeweils eine Seite erscheint, die um Benutzer-Best&auml;tigung bittet, anstatt automatisch Bestellungen zu speichern. (Standard=no - nein)
ITEM dont-request
[<URL-SPEC>] dont-request = yes | no
Seiten die dieser URL-Spezifikation entsprechen im Offline-Modus nicht abrufen. (Standard=keine)
SECTION FetchOptions
Optionen die steuern, was beim Abruf offline bestellter Seiten heruntergeladen wird.
ITEM stylesheets
[<URL-SPEC>] stylesheets = yes | no
Ob Style-Sheets abgerufen werden sollen (Standard=no - nein)
ITEM images
[<URL-SPEC>] images = yes | no
Ob Bilder abgerufen werden sollen (Standard=no - nein)
ITEM webbug-images
[<URL-SPEC>] webbug-images = yes | no
Ob Bilder, die nur 1x1 -Pixel gro&szlig; sind, auch abgerufen werden sollen, ben&ouml;tigt auch die Auswahl der images-Option. (Standard=yes - ja). Diese Option ist zur Verwendung in Verbindung mit der replace-webbug-images-Option im ModfiyHTML-Abschnitt gedacht.
ITEM frames
[<URL-SPEC>] frames = yes | no
Ob Frames abgerufen werden sollen (Standard=no - nein)
ITEM scripts
[<URL-SPEC>] scripts = yes | no
Ob Scripte (z.B. Javascript) abgerufen werden sollen (Standard=no - nein)
ITEM objects
[<URL-SPEC>] objects = yes | no
Ob Objekte (z.B. Java class files) abgerufen werden sollen (Standard=no - nein)
SECTION IndexOptions
Option die steuern, was in den Seiten-Verzeichnissen angezeigt wird.
ITEM no-lasttime-index
no-lasttime-index = yes | no
Deaktiviert die Erstellung von Verzeichnissen der letzten/vorherigen Online-Sitzung (Standard=no - nein)
ITEM cycle-indexes-daily
cycle-indexes-daily = yes | no
Zyklus f&uuml;r die Z&auml;hlung letzte/vorherige Onlinesitzungen-Verzeichnisse wechselt nur t&auml;glich anstatt mit jeder Online-Situng oder jedem Abruf (Standard=yes).
ITEM list-outgoing
<URL-SPEC> list-outgoing = yes | no
W&auml;hlt, ob die URL im Verzeichnis der Bestellung aufgef&uuml;hrt werden soll (Standard=yes - ja)
ITEM list-latest
<URL-SPEC> list-latest = yes | no
W&auml;hlt, ob die URL im Verzeichnis der letzten/vorherigen Onlinesitzung  und  der letzten/vorherigen Bestllungen aufgef&uuml;hrt werden soll. (Standard=yes - ja)
ITEM list-monitor
<URL-SPEC> list-monitor = yes | no
W&auml;hlt, ob die URL im Verzeichniss der abonnierten URLs aufgef&uuml;hrt werden soll (Standard=yes - ja)
ITEM list-host
<URL-SPEC> list-host = yes | no
W&auml;hlt, ob die URL im Verzeichnis der Hosts aufgef&uuml;hrt werden soll (Standard=yes - ja)
ITEM list-any
<URL-SPEC> list-any = yes | no
W&auml;hlt, ob die URL in irgendeinem der Verzeichnisse aufgef&uuml;hrt werden soll (Standard=yes)
SECTION ModifyHTML
Optionen die steuern, in welcher Weise der HTML-Code, der im Cache gespeichert ist, ver&auml;ndert wird.
ITEM enable-modify-html
[<URL-SPEC>] enable-modify-html = yes | no
Aktivieren der HTML-Ver&auml;nderungen dieses Abschnitts (Standard=no - nein). Ist diese Option nicht aktiviert, haben die folgenden HTML-Optionen keinerlei Effekt. Ist diese Option aktiviert, ist WWWOFFLE ein bisschen langsamer.
ITEM enable-modify-online
[<URL-SPEC>] enable-modify-online = yes | no
Aktivieren der Ver&auml;nderungen dieses Abschnitts auch im Online-Modus so wie im Offline-Modus (Standard=no - nein). Dies verursacht, dass die HTML-Seite oder die GIF-Datei nicht im Browser erscheint, bevor WWWOFFLE nicht alles abgearbeitet hat. Dies trifft nicht auf Seiten zu, die nicht im Cache gespeichert werden.
ITEM add-cache-info
[<URL-SPEC>] add-cache-info = yes | no
Am unteren Ende aller &uuml;bertragenen Seiten wird das Datum, an dem die Seite im Cache gespeichert wurde und einige Navigations-Elemente hinzugef&uuml;gt (Standard=nein).
ITEM anchor-cached-begin
[<URL-SPEC>] anchor-cached-begin = (HTML code) | 
Links in der &uuml;bertragenen Seite, die im Cache gespeichert sind, erhalten den angegebenen HTML-Code vorangestellt. (Standard="").
ITEM anchor-cached-end
[<URL-SPEC>] anchor-cached-end = (HTML code) | 
Links in der &uuml;bertragenen Seite, die im Cache gespeichert sind, erhalten den angegebenen HTML-Code angeh&auml;ngt. (Standard="").
ITEM anchor-requested-begin
[<URL-SPEC>] anchor-requested-begin = (HTML code) | 
Links in der &uuml;bertragenen Seite, die bereits zum Download bestellt sind, erhalten den angegebenen HTML-Code vorangestellt (Standard="").
ITEM anchor-requested-end
[<URL-SPEC>] anchor-requested-end = (HTML code) | 
Links in der &uuml;bertragenen Seite, die bereits zum Download bestellt sind, erhalten den angegebenen HTML-Code angeh&auml;ngt (Standard="").
ITEM anchor-not-cached-begin
[<URL-SPEC>] anchor-not-cached-begin = (HTML code) | 
Links in der &uuml;bertragenen Seite, die nicht im Cache gespeichert oder bestellt sind, erhalten den angegebenen HTML-Code vorangestellt (Standard="").
ITEM anchor-not-cached-end
[<URL-SPEC>] anchor-not-cached-end = (HTML code) | 
Links in der &uuml;bertragenen Seite, die nicht im Cache gespeichert oder bestellt sind, erhalten den angegebenen HTML-Code angeh&auml;ngt (Standard="").
ITEM disable-script
[<URL-SPEC>] disable-script = yes | no
Entfernt alle Skripte (Standard=no - nein)
ITEM disable-applet
[<URL-SPEC>] disable-applet = yes | no
Entfernt alle Java-Applets (Standard=no - nein)
ITEM disable-style
[<URL-SPEC>] disable-style = yes | no
Entfernt alle Style-Sheets und Style-Verweise (Standard=no - nein)
ITEM disable-blink
[<URL-SPEC>] disable-blink = yes | no
Entfernt den &lt;blink&gt;-tag (Standard=no - nein).
ITEM disable-flash
[<URL-SPEC>] disable-flash = yes | no
Entfernt Shochwave-Flash-Animationen (Standard=no - nein).
ITEM disable-meta-refresh
[<URL-SPEC>] disable-meta-refresh = yes | no
Entfernt alle Meta-Tags im HTML-Header, die den Browser nach einer Zeit auf eine andere Seite umleiten. (Standard=no - nein).
ITEM disable-meta-refresh-self
[<URL-SPEC>] disable-meta-refresh-self = yes | no
Entfernt alle Meta-Tags im HTML-Header, die den Browser nach einer Zeit dazu veranlassen, die gleiche Seite erneut zu laden (Standard=no - nein).
ITEM disable-dontget-links
[<URL-SPEC>] disable-dontget-links = yes | no
Deaktiviert alle Links auf URLS, die im DontGet-Abschnitt der Konfigurations-Datei aufgef&uuml;hrt sind (Standard=no - nein).
ITEM disable-dontget-iframes
[<URL-SPEC>] disable-dontget-iframes = yes | no
Deaktivert Inline-Frames-(iframe)-URLs, die im DontGet-Abschnitt der Konfigurations-Datei aufgef&uuml;hrt sind (Standard=no - nein).
ITEM replace-dontget-images
[<URL-SPEC>] replace-dontget-images = yes | no
Ersetzt Bild-URLs, die im DontGet-Abschnitt der Konfigurations-Datei aufgef&uuml;hrt werden mit eine statischen URL (Standard=no - nein)
ITEM replacement-dontget-image
[<URL-SPEC>] replacement-dontget-image = (URL)
Die Ersatz-Bild-Datei zur Verwendung f&uuml;r URLs, die im DontGet-Abschnitt der Konfigurations-Datei aufgef&uuml;hrt sind. (Standard=/local/dontget/replacement.gif).
ITEM replace-webbug-images
[<URL-SPEC>] replace-webbug-images = yes | no
Ersetzt die URLs, die ein 1x1 pixel-Bild enthalten durch eine statische URL (Standard=no - nein). Diese Option ist gedacht zur Verwendung in Verbindung mit der webbug-images-Option im FetchOptions-Abschnitt.
ITEM replacement-webbug-image
[<URL-SPEC>] replacement-webbug-image = (URL)
Die Ersatz-Bild-Datei zur Verwendung f&uuml;r Bilder, die ein 1x1-Pixel Bild sind. (Standard=/local/dontget/replacement.gif).
ITEM disable-animated-gif
[<URL-SPEC>] disable-animated-gif = yes | no
Deaktiviert die Animationen animierter GIF-Dateien (Standard=no - nein).
ITEM demoronise-ms-chars
[<URL-SPEC>] demoronise-ms-chars = yes | no
Ersetzt einige merkw&uuml;rdige Zeichen, die einige Microsoft-Anwendungen in HTML-Code verwenden, mit Entsprechungen, die von den meisten Browsern dargestellt werden k&ouml;nnen (Standard=no - nein). Die Idee hierzu kommt von dem Public-Domain Demoroniser-perl Script.
SECTION LocalHost
Eine Liste der Hostnamen unter denen der Host, auf dem der WWWOFFLE-Server l&auml;uft, bekannt ist. Diese Option ist daf&uuml;r, dass der Proxy sich nicht selbst kontaktieren muss, wenn die Anfrage einen anderen Namen f&uuml;r den selben Server verwendet.
ITEM 
(host)
Ein Hostname oder eine IP-Adresse, die in Verbindung mit der Port-Nummer (im Startup-Abschnitt) den WWWOFFLE-Proxy-HTTP-Server angibt. Der Hostname muss exakt passen, es ist KEINE <a href="/configuration/#WILDCARD">WILDCARD</a>.  Der erste benannte Host wird als Server-Name f&uuml;r verschiedene Funktionen verwendet, es muss sich daher um einen Namen handeln, der f&uuml;r jeden Client-Host funktioniert. Keiner der hier benannten Hosts wird im Cache gespeichert oder &uuml;ber ein Proxy abgerufen.
SECTION LocalNet
Eine Liste der Hostnamen deren Web-Server immer zugreifbar sind, auch im Offline-Modus und die nicht im WWWOFFLE-Cache gespeichert werden sollen, da sie im lokalen Netzwerk sind.
ITEM 
(host)
Ein Hostname oder eine IP-Adresse, die immer verf&uuml;gbar ist und nicht im WWWOFFLE-Cache gespeichert werden soll. Beim Hostname d&uuml;rfen <a href="/configuration/#WILDCARD">WILDCARD</a>s verwendet werden.  Ein Host kann ausgeschlossen werden durch Einf&uuml;gen eines '!' an den Anfang des Namens, alle m&ouml;glichen Aliases und IP-Adressen f&uuml;r den Host m&uuml;ssen ebenfalls angegeben werden.  Zu allen hier eingetragenen Hosts wird angenommen, dass sie erreichbar sind - auch im Offline-Modus. Keiner der hier genannten Hosts wird im Cache gespeichert oder &uuml;ber ein Proxy abgerufen.
SECTION AllowedConnectHosts
Eine Liste der Client-Hostnamen, denen es erlaubt ist, zum WWWOFFLE-Server Verbindungen aufzubauen.
ITEM 
(host)
Ein Hostname oder eine IP-Adresse, der es erlaubt ist, eine Verbindung zum Sever aufzubauen. Beim Hostnamen d&uuml;rfen <a href="/configuration/#WILDCARD">WILDCARD</a>s verwendet werden. Ein Host kann ausgeschlossen werden durch Einf&uuml;gen eines '!' an den Anfang des Namens, alle m&ouml;glichen Aliases und IP-Adressen f&uuml;r den Host m&uuml;ssen ebenfalls angegeben werden. Alle im Abschnitt LocalHost genannten Hosts d&uuml;rfen ebenfalls eine Verbindung herstellen.
SECTION AllowedConnectUsers
Eine Liste der User die eine Verbindung zum Server herstellen d&uuml;rfen un ihre Passw&ouml;rter.
ITEM 
(username):(password)
Der Username und das Passwort der User, die eine Verbindung zum Server herstellen d&uuml;rfen. Is dieser Abschnitt leer, wird keine User-Authentifizierung durchgef&uuml;hrt. Der Username und das Passwort werden beide in unverschl&uuml;sselter Form gespeichert. Die Option setzt die Verwendung eines Browser voraus, der den HTTP/1.1-Proxy-Authentifizierungs-Standard verarbeitet.
SECTION DontCache
Eine Liste von URLs, die nicht im WWWOFFLE-Cache gespeichert werden sollen.
ITEM 
[!]URL-SPECIFICATION
Speichere keine URLs die zur angegebenen URL-Spezifikation passen. Die <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a> kann umgekehrt werden um passende URLs im Cache zu speichern (Angabe von '!').  Die URLs, die nicht im Cache gespeichert werden sollen, werden nicht offline bestellt.
SECTION DontGet
Eine Liste von URLs, die nicht von WWWOFFLE abgerufen werden soll (weil sie nur M&uuml;ll-Werbung enthalten zum Beispiel)
ITEM 
[!]URL-SPECIFICATION
Hole keine URLs, die zur angegebenen URL-Spezifikation passen. Die <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a> kann umgekehrt werden, um din passenden URLs zuzulassen (Angabe von '!')
ITEM replacement
[<URL-SPEC>] replacement = (URL)
Die URL die zum Ersetzen jeglicher URLs die zur angegebenen <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a> passen anstelle einer Standard-Fehler-Meldung (Standard=keine).  Die URLs in /local/dontget/ sind vorgeschlagene Ersatzdateien (z.B. replacement.gif oder replacement.png, die 1x1 pixel transparente Bilder sind, oder replacement.js, welches eine leeres Javascript-Datei ist).
ITEM get-recursive
<URL-SPEC> get-recursive = yes | no
W&auml;hlt ob die hierzu passenden URLs beim rekursiven Abruf abgerufen werden (Standard=yes ja).
ITEM location-error
<URL-SPEC> location-error = yes | no
Wenn die Antwort auf eine URL einen 'Location'-Header enth&auml;lt, der auf eine URL umleitet, die nicht (wie in diesem Abschnitt aufgef&uuml;hrt) abgerufen werden soll, wird die Antwort stattdessen in eine Fehlermeldung gewandelt (standard=no - nein). Dies verhindert, dass ISP-Proxies ihre Benutzer auf Werbeseiten umleiten, wenn diese Werbe-URLs in diesem Abschnitt gelistet werden.
SECTION DontCompress
Eine Liste der MIME-Typen und Datei-Erweiterungen, die nicht von WWWOFFLE komprimiert werden sollen (weil sie bereits komprimiert sind oder es nicht wert sind komprimiert zu werden). Dies ben&ouml;tigt die zlib-Option beim Compilieren.
ITEM mime-type
mime-type = (mime-type)/(subtype)
Der MIME-Typ einer URL, die nicht komprimiert werden soll im Cache oder beim &uuml;bertragen komprimierter Seiten zum Browser.
ITEM file-ext
file-ext = .(file-ext)
Die Datei-Erweiterung einer URL, die nicht komprimiert vom Server angefordert werden soll.
SECTION CensorHeader
Eine Liste der HTTP-Header-Zeilen, die von den Anfragen, die an die Webserver gehen, entfernt werden sollen, sowie von den Antworten, die von ihnen zur&uuml;ckkommen.
ITEM 
[<URL-SPEC>] (header) = yes | no | (string)
Ein Header-Feld-Name (z.B. From, Cookie, Set-Cookie, User-Agent) und der String, mit dem dieser Header ersetzt werden soll (Standard=keiner). Der Header ber&uuml;cksichtigt Gross-/Kleinschreibung und hat kein ':' am Ende. Ein Wert von 'no' meint, der Header wird nicht ver&auml;ndert, "yes" oder kein String kann verwendet werden, um den Header zu entfernen oder ein String kann verwendet werden, um den Header zu ersetzen. Dies ersetzt nur vorgefundene Header, es werden keine neuen erzeugt. Eine Option f&uuml;r 'Referer' an dieser Stelle hat Vorrang gegen&uuml;ber den referer-self- und referer-self-dir-Optionen.
ITEM referer-self
[<URL-SPEC>] referer-self = yes | no
Setzt den Referer-Header auf die gleiche URL, die angefordert wird (Standard=no - nein). Diese Option f&uuml;gt einen Referer-Header hinzu, wenn in der Original-Anfrage keiner vorhanden ist.
ITEM referer-self-dir
[<URL-SPEC>] referer-self-dir = yes | no
Setzt den Referer-Header auf den Verzeichnisnamen der URL, die angefordert wird (Standard=no - nein). Diese Option f&uuml;gt einen Referer-Header hinzu, wenn in der Original-Anfrage keiner vorhanden ist. Die Option hat Vorrang gegegen&uuml;ber der referer-self-Option.
SECTION FTPOptions
Optionen zur Verwendung beim Abruf von Dateien &uuml;ber das ftp-Protokoll.
ITEM anon-username
anon-username = (string)
Der Username zur Verwendung f&uuml;r anonymen ftp-Zugriff (Standard=anonymous).
ITEM anon-password
anon-password = (string)
Das Passwort zur Verwendung f&uuml;r anonymen ftp-Zugriff (Standard wird zur Laufzeit festgestellt). Bei Verwendung eines Firewalls kann dies einen Wert enthalten, der f&uuml;r den FTP-Server nicht g&uuml;ltig ist und es muss m&ouml;glicheweise ein anderer Wert gesetzt werden. 
ITEM auth-username
<URL-SPEC> auth-username = (string)
Der Username zu Verwendung auf einem Host anstelle des Standard-Usernamen f&uuml;r anonyme Zugriffe.
ITEM auth-password
<URL-SPEC> auth-password = (string)
Dass Passwort zur Verwendung auf einem Host anstelle des Standard-Passwort f&uuml;r anonyme Zugriffe.
SECTION MIMETypes
MIME-Typen zur Verwendung beim Liefern von Dateien, die nicht &uuml;ber HTTP angefordert wurden oder f&uuml;r Dateien vom eingauten Web-Servers.
ITEM default
default = (mime-type)/(subtype)
Der Standard-MIME-Typ (Standard=text/plain).
ITEM 
.(file-ext) = (mime-type)/(subtype)
Der MIME-Typ, der zu einer Dateierweiterung assoziiert werden soll. Der '.' muss in der Dateierweiterung mitangegeben werden. Wenn mehr als eine Erweiterung passt, wird die l&auml;ngste verwendet.
SECTION Proxy
Dieser Abschnitt beinhaltet die Namen der HTTP-Proxies (oder andere)  zur externen Verwendung f&uuml;r den WWWOFFLE-Server
ITEM proxy
[<URL-SPEC>] proxy = (host[:port])
Der Hostname und Port zur Verwendung als Proxy f&uuml;r die angegebene URL-Spezifikation.
ITEM auth-username
<URL-SPEC> auth-username = (string)
Der Username zur Verwendung auf einem Proxy-Host zur Authentifizierung von WWWOFFLE. Die <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a> bezieht sich in diesem Fall auf den Proxy und nicht die angeforderte URL.
ITEM auth-password
<URL-SPEC> auth-password = (string)
Das Passwort zur Verwendung auf einem Proxy-Host zur Authentifizierung von WWWOFFLE. Die <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a> bezieht sich in diesem Fall auf den Proxy und nicht die angeforderte URL.
ITEM ssl
[<URL-SPEC>] ssl = (host[:port])
Ein Proxy-Server der f&uuml;r Secure Socket Layer (SSL)-Verbindungen verwendet werden soll (https). Beachten Sie, dass f&uuml;r die &lt;<a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a>&gt;, dass nur der Host gepr&uuml;ft wird und die anderen Teile '*' sein m&uuml;ssen <a href="/configuration/#WILDCARD">WILDCARD</a>s.
SECTION Alias
Eine Liste von Alias-Namen, die verwendet werden um Server-Namen und Pfade in andere Server-Namen und Pfade zu ersetzen.
ITEM 
URL-SPECIFICATION = URL-SPECIFICATION
Jegliche Anfragen, die zur ersten <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a> passen, werden durch die zweite <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a> ersetzt.  Die erste <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a> ist eine Wildcard f&uuml;r Protokoll und Port, der Pfad muss exakt mit dem Start der angefragten URL &uuml;bereinstimmen und beinhaltet s&auml;mtliche Unterverzeichnisse. 
SECTION Purge
Die Methode zum Feststellen, welche Seiten gel&ouml;scht werden sollen, das voreingestellte maximale Alter, das Host-spezifische maximale Alter der Seiten in Tagen und die maximale Cache-Gr&ouml;sse.
ITEM use-mtime
use-mtime = yes | no
Die zu verwendende Methode zur Entscheidung welche Dateien gel&ouml;scht werden sollen, nach letzter Zugriffs-Zeit (atime) oder letzter &Auml;nderung (modification time) (mtime) (Standard=no nein, nicht mtime verwenden)
ITEM max-size
max-size = (size)
Die maximale Gr&ouml;sse des Cache im MB nach dem L&ouml;schen ("purge") (Standard=0). Eine maximale Cache-Gr&ouml;sse von 0 meint, es gibt keine Gr&ouml;ssenbeschr&auml;nkung.  Wird diese Option zusammen mit der min-free-Option verwendet, wird die kleinere Cache-Gr&ouml;sse verwendet. Diese Option ber&uuml;cksichtig die URLs, die nie gel&ouml;scht werden sollen, beim Ermitteln der Cache-Gr&ouml;sse aber l&ouml;scht diese nicht.
ITEM min-free
min-free = (size)
Der minimale freie Disk-Speicherplatz in MB nach dem L&ouml;schen (Standard=0). Ein minimaler freier Speicherplatz von 0 meint, dass es keine Mindestanforderungen f&uuml;r den freien Speicher gibt. Wird diese Option zusammen mit der max-size-Option benutzt, wird die kleiner Cache-Gr&ouml;sse verwendet. Diese Option ber&uuml;cksichtigt die URLs, die nie gel&ouml;scht werden sollen, beim Ermitteln der Cache-Gr&ouml;sse aber l&ouml;scht diese nicht.
ITEM use-url
use-url = yes | no
Wenn auf 'yes' gesetzt, wird die URL zur Feststellung des L&ouml;sch-Alters verwendet, andernfalls nur das Protokoll und der Host (Standard=no - nur Protokoll und Host)
ITEM del-dontget
del-dontget = yes | no
Wenn auf 'yes' gesetzt, werden URLs die im DontGet-Abschnitt aufgef&uuml;hrt werden gel&ouml;scht. (Standard=no)
ITEM del-dontcache
del-dontcache = yes | no
Wenn auf 'yes' gesetzt, wedren URLs, die zu Eintr&auml;gen im DontCache-Abschnitt passen, gel&ouml;scht. (Standard=no)
ITEM age
[<URL-SPEC>] age = (age)
Das maximale Alter in Tagen der im Cache gespeicherten Seiten f&uuml;r URLs die zur angegebenen URL-Spezifikation passen (Standard=14). Ein Alter von 0 meint beim 'Purge' l&ouml;schen, ein negativer Wert meint nicht l&ouml;schen. Die <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a> wird nur auf Protokoll und Host gepr&uuml;ft, ausser die  use-url-Option ist auf 'yes' gesetzt. L&auml;ngere Zeitr&auml;ume k&ouml;nnen durch 'w (Wochen)', 'm (Monate)' oder 'y (Jahre)' angegeben werden (z.B. 2w=14).
ITEM compress-age
[<URL-SPEC>] compress-age = (age)
Das maximal Alter im Cache f&uuml;r URLs, die zur Spezifikation passen, zum unkomprimierten Speichern. Ben&ouml;tigt die zlib-Option beim Compilieren. Das Alter, das angegeben wird, hat die gleiche Bedeutung wie bei der age-Option.
TAIL
<h2><a name="WILDCARD">WILDCARD</a></h2> Eine <a href="/configuration/#WILDCARD">WILDCARD</a> ist eine Spezifikation die das Zeichen '*' als Ersart f&uuml;r eine Gruppe von Zeichen verwendet.<p> Das ist im Grunde das gleiche, wie bei einem Kommado-Zeilen-Datei-Ausdruck in DOS oder der UNIX-Shell,  au&szlig;er das ein '*' auch das Zeichen '/' beinhalten kann. <p> Zum Beispiel <dl><dt>*.gif<dd>passt auf  foo.gif und bar.gif</dl><dl><dt>*.foo.com<dd>passt auf  www.foo.com und ftp.foo.com</dl><dl><dt>/foo/*<dd>passt auf  /foo/bar.html und /foo/bar/foobar.html</dl><h2><a name="URL-SPECIFICATION">URL-SPEZIFIKATION</a></h2> Bei der Angabe eines Hosts und eines Protokolls und eines Pfades kann in vielen Abschnitten eine <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a> verwendet werden, dies ist ein Weg zur Erkennung einer URL. <p> F&uuml;r den Gebrauch in dieser Erkl&auml;rung wird angenommen eine URL besteht aus f&uuml;nf Teilen<dl><dt>proto<dd>Das verwendete Protokoll (z.B. 'http', 'ftp')</dl><dl><dt>host<dd>Der Server-Hostname (z.B. 'www.gedanken.demon.co.uk').</dl><dl><dt>port<dd>Die Portnummer auf diesem Host (z.B. der Standard 80 f&uuml;r HTTP).</dl><dl><dt>path<dd>Der Pfadname auf dem Host (z.B. '/bar.html') oder ein Verzeichnisname  (z.B. '/foo/').</dl><dl><dt>args<dd>m&ouml;gliche Argumente zur URL f&uuml;r CGI-Scripts u.&auml;.  (z.B. 'search=foo').</dl> <p> Zum Beispiel die WWWOFFLE-Homepage http://www.gedanken.demon.co.uk/wwwoffle/ Das Protokoll ist 'http', Der Host ist 'www.gedanken.demon.co.uk', die Portnummer ist der Standard-Port (in diesem Fall 80), und der Pfadname ist '/wwwoffle/'. <p> Im allgemeinen wird dies geschrieben als (proto)://(host)[:(port)]/[(path)][?(args)] <p> Wobei [] eine m&ouml;gliche Angabe anzeigt, und () einen vom Benutzer angebenen Namen oder Nummer kennzeichnet. <p> Einige Beispiel-<a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a>en folgen:<dl><dt>*://*/*<dd>Beliebiges Protokoll, beliebiger Host, beliebiger Port, beliebiger Pfad, beliebiege Argumente (Das ist das gleiche wie 'default').</dl><dl><dt>*://*/(path)<dd>Beliebiges Protokoll, beliebiger Host, beliebiger Port, benannter Pfad, beliebige Argumente</dl><dl><dt>*://*/*?<dd>Beliebiges Protokoll, beliebiger Host, beliebiger Port, beliebiger Pfad, KEINE Argumente</dl><dl><dt>*://*/(path)?*<dd>Beliebiges Protokoll, beliebiger Host, beliebiger Port, benannter Pfad, beliebige Argumente</dl><dl><dt>*://(host)<dd>beliebiges Protokoll, benannter Host, beliebiger Port, beliebiger Pfad, beliebige Argumente</dl><dl><dt>(proto)://*/*<dd>Benanntes Protokoll, beliebiger Host, beliebiger Pfad, beliebige Argumente</dl> <p> (proto)://(host)/*  Benanntes Protokoll, benannter Host, beliebiger Port, beliebiger Pfad, beliebige Argumente <p> (proto)://(host):/* Benanntes Protokoll, benannter Host, Standard-Port, beliebiger Pfad und beliebige Argumente<p> *://(host):(port)/* Beliebiges Protokoll, benannter Host, benannter port, beliebiger Pfad, beliebiger Argumente <p> Bei der Beschreibung des Hosts, des Pfads und der Argumente k&ouml;nnen <a href="/configuration/#WILDCARD">WILDCARD</a>s verwendet werden, wie oben beschrieben. <p> In einigen Abschnitten die <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a>en akzeptieren, k&ouml;nnen diese umgekehrt werden durch Einf&uuml;gen des Zeichens '!' vor der URL-Spezifikationt.  Das meint das der Vergleich einer URL mit der <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a> den logisch umgekehrten Wert zur&uuml;ckgibt, als was ohne das '!' zur&uuml;ckgegeben werden w&uuml;rde.  Wenn alle der <a href="/configuration/#URL-SPECIFICATION">URL-SPEZIFIKATION</a>en in einem Abschnitt negiert werden und '*://*/*' wird am Ende hinzugef&uuml;gt, dann wird der gesamte Sinn des Abschnitts umgekehrt.
