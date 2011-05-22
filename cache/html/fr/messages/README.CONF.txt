TITLE WWWOFFLE - Fichier de configuration - Version 2.7
HEAD
<h2><a name="Introduction">Introduction</a></h2> Le fichier de configuration (wwwoffle.conf) sp�cifie tous les param�tres qui contr�lent l'activit� du serveur proxy. Le fichier est divis� en sections d�crites ci-dessous contenant chacune une s�rie de param�tres. Le fichier CHANGES.CONF explique les changements de ce fichier de configuration par rapport aux versions pr�c�dentes. <p> Le fichier est divis� en sections, chacune pouvant �tre vide ou contenir une ou plusieurs lignes d'information de configuration. Les sections sont nomm�es, et leur ordre d'apparition dans le fichier n'est pas important. <p> Le format g�n�ral de chacune des sections est le m�me. Le nom de la section est sur une ligne, et en marque le d�but. Le contenu de la section est d�limit� par une paire de lignes contenant seulement les caract�res '{' et '}', ou '[' et ']'. Quand la paire '{' et '}' est utilis�e, les lignes encloses contiennent des informations de configuration. Quand la paire '[' et ']' est utilis�e, il doit y avoir � l'int�rieur de cette derni�re une seule ligne non vide, contenant le nom d'un fichier (dans le m�me r�pertoire) contenant les lignes de configuration de cette section. <p> Les commentaires sont signal�s par le caract�re '#' au d�but de la ligne, et sont ignor�s. Les lignes vides sont aussi permises et ignor�es. <p> Les entit�s <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> (<a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> en abr�g�) et <a href="/configuration/#WILDCARD">WILDCARD</a> ont une signification particuli�re dans le fichier de configuration, et sont d�crites � la fin. Toute entit� enclose entre parenth�ses '(' et ')' dans les descriptions signifie un param�tre fourni par l'utilisateur, tout ce qui est entre crochets '[' et ']' est optionnel, et la barre verticale '|' indique une alternative. Certaines options s'appliquent seulement � des URL, ceci est pr�cis� par une <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> enclose entre '&lt;' &amp; '&gt;' dans l'option, la premi�re <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> qui correspond au motif est utilis�e. Si aucune <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> n'est donn�e, alors toute URL correspond.
SECTION StartUp
Cette section contient les param�tres utilis�s au lancement du programme, les changements �ventuels sont ignor�s si la configuration est relue pendant l'ex�cution.
ITEM bind-ipv4
bind-ipv4 = (hostname) | (ip-address) | none
Sp�cifie le nom d'h�te ou l'adresse IP o� lier les sockets proxy HTTP et port de contr�le WWWOFFLE utilisant IPv4 (par d�faut '0.0.0.0'). Si 'none' est indiqu�, alors aucun socket IPv4 n'est utilis�.
ITEM bind-ipv6
bind-ipv6 = (hostname) | (ip-address) | none
Sp�cifie le nom d'h�te ou l'adresse IP o� lier les sockets proxy HTTP et port de contr�le WWWOFFLE utilisant IPv6 (par d�faut '::'). Si 'none' est indiqu�, alors aucun socket IPV6 n'est utilis�. L'option de compilation IPv6 est requise.
ITEM http-port
http-port = (port)
Un entier indiquant le port du serveur proxy (8080 par d�faut).
ITEM wwwoffle-port
wwwoffle-port = (port)
Un entier indiquant le port de contr�le WWWOFFLE (8081 par d�faut).
ITEM spool-dir
spool-dir = (dir)
Le chemin complet du r�pertoire de cache (r�pertoire de spool) (d�faut=/var/spool/wwwoffle).
ITEM run-uid
run-uid = (user) | (uid)
Le nom d'utilisateur ou le num�ro UID sous lequel le serveur WWWOFFLE est lanc� (d�faut=aucun). Cette option n'est pas applicable sous win32 et ne fonctionne que si le serveur est lanc� par l'utilisateur root sous UNIX.
ITEM run-gid
run-gid = (group) | (gid)
Le groupe ou le num�ro GID sous lequel le serveur WWWOFFLE est lanc� (d�faut=aucun). Cette option n'est pas applicable sous win32, et ne fonctionne que si le serveur est lanc� par l'utilisateur root sous UNIX.
ITEM use-syslog
use-syslog = yes | no
Indique si le service syslog est utilis� pour les messages (d�faut=yes).
ITEM password
password = (word)
Le mot de passe utilis� pour l'authentification des pages de contr�le, pour l'effacement des pages m�moris�es, etc. (d�faut=aucun). Pour s�curiser la configuration, la lecture du fichier de configuration doit �tre r�serv�e aux utilisateurs autoris�s.
ITEM max-servers
max-servers = (integer)
Le nombre maximum de processus serveurs lanc�s en ligne et pour le rapatriement automatique (d�faut=8).
ITEM max-fetch-servers
max-fetch-servers = (integer)
Le nombre maximum de serveurs lanc�s pour le rapatriement automatique des pages demand�es en mode hors-ligne (d�faut=4). Cette valeur doit �tre inf�rieure � max-servers pour permettre l'usage interactif simultan�.
SECTION Options
Options contr�lant le fonctionnement du programme
ITEM log-level
log-level = debug | info | important | warning | fatal
Le niveau minimum de message syslog ou stderr (d�faut=important).
ITEM socket-timeout
socket-timeout = (time)
Le temps d'attente en secondes des donn�es sur un socket avant abandon par WWWOFFLE (d�faut=120).
ITEM dns-timeout
dns-timeout = (time)
Le temps d'attente en secondes d'une requ�te DNS (Domain Name Service) avant abandon par WWWOFFLE (d�faut=60).
ITEM connect-timeout
connect-timeout = (time)
Le temps d'attente en secondes pour obtenir un socket avant abandon par WWWOFFLE (d�faut=30).
ITEM connect-retry
connect-retry = yes | no
Si une connexion � un serveur distant ne peut �tre obtenue, alors WWWOFFLE essaiera encore apr�s un court d�lai (d�faut=no).
ITEM ssl-allow-port
ssl-allow-port = (integer)
Un num�ro de port autoris� pour les connexions SSL (Secure Socket Layer), par ex. https. Cette option devrait �tre fix�e � 443 pour autoriser https, il peut y avoir plusieurs lignes ssl pour autoriser d'autres ports si besoin.
ITEM dir-perm
dir-perm = (octal int)
Les permissions de r�pertoires pour la cr�ation des r�pertoires de spool (d�faut=0755). Cette option �crase le umask de l'utilisateur, et doit �tre octale, commen�ant par un z�ro '0'.
ITEM file-perm
file-perm = (octal int)
Les permissions de fichiers pour la cr�ation des fichiers de spool (d�faut=0644). Cette option �crase le umask de l'utilisateur, et doit �tre octale, commen�ant par un z�ro '0'.
ITEM run-online
run-online = (filename)
Le nom d'un programme � lancer quand WWWOFFLE est commut� en mode en ligne (d�faut=aucun). Ce programme est lanc� avec un seul param�tre, fix� au nom du mode, "online".
ITEM run-offline
run-offline = (filename)
Le nom du programme � lancer quand WWWOFFLE est commut� en mode hors-ligne (d�faut=aucun). Ce programme est lanc� avec un seul param�tre, le nom du mode, "offline".
ITEM run-autodial
run-autodial = (filename)
Le nom d'un programme � lancer quand WWWOFFLE est commut� en mode automatique (d�faut=aucun). Le programme est lanc� avec un seul param�tre, le nom du mode, "autodial".
ITEM run-fetch
run-fetch = (filename)
Le nom du programme � lancer quand WWWOFFLE d�marre ou arr�te le rapatriement automatique (d�faut=aucun). Ce programme est lanc� avec deux param�tres, le premier le mot "fetch", et le second l'un des mots "start" ou "stop".
ITEM lock-files
lock-files = yes | no
Active l'usage des fichiers verrous pour emp�cher plus d'un processus WWWOFFLE de rapatrier simultan�ment la m�me URL (d�faut=no).
ITEM reply-compressed-data
reply-compressed-data = yes | no
Si les r�ponses faites au navigateur doivent contenir des donn�es compress�es quand demand� (d�faut=no). N�cessite l'option de compilation zlib.
SECTION OnlineOptions
Options contr�lant le comportement en ligne de WWWOFFLE.
ITEM request-changed
[<URL-SPEC>] request-changed = (time)
En ligne, les pages ne seront rapatri�es que si la version m�moris�e est plus ancienne que le temps indiqu� en secondes (d�faut=600). Une valeur n�gative indique que les pages m�moris�es sont toujours utilis�es en ligne. Des temps longs peuvent �tre indiqu�s par les suffixes 'm', 'h', 'd' ou 'w' pour minute, heure, jour ou semaine (par ex. 10m=600).
ITEM request-changed-once
[<URL-SPEC>] request-changed-once = yes | no
En ligne, les pages ne seront rapatri�es qu'une seule fois par session (d�faut=yes). Cette option a priorit� sur l'option request-changed.
ITEM request-expired
[<URL-SPEC>] request-expired = yes | no
En ligne, les pages p�rim�es seront rafra�chies (d�faut=no). Cette option a priorit� sur les options request-changed et request-changed-once.
ITEM request-no-cache
[<URL-SPEC>] request-no-cache = yes | no
En ligne, les pages � ne pas cacher seront toujours redemand�es (d�faut=no). Cette option a priorit� sur les options request-changed et request-changed-once.
ITEM try-without-password
[<URL-SPEC>] try-without-password = yes | no
Si une requ�te demande une page utilisant un nom et un mot de passe, alors une requ�te de la m�me page sera faite sans (d�faut=yes). Ceci autorise la requ�te d'une page sans mot de passe � �tre redirig�e vers la version avec.
ITEM intr-download-keep
[<URL-SPEC>] intr-download-keep = yes | no
Si le navigateur ferme la connexion en ligne, alors la page incompl�te sera conserv�e (d�faut=no).
ITEM intr-download-size
[<URL-SPEC>] intr-download-size = (integer)
Si le navigateur ferme la connexion en ligne, la page devrait continuer � �tre rapatri�e si sa taille est inf�rieure � celle indiqu�e en kilo-octets (d�faut=1).
ITEM intr-download-percent
[<URL-SPEC>] intr-download-percent = (integer)
Si le navigateur ferme la connexion en ligne, la page devrait continuer � �tre rapatri�e si le pourcentage indiqu� est atteint (d�faut=80).
ITEM timeout-download-keep
[<URL-SPEC>] timeout-download-keep = yes | no
Si la connexion au serveur est abandonn�e, la page incompl�te doit �tre conserv�e (d�faut=no).
ITEM request-compressed-data
[<URL-SPEC>] request-compressed-data = yes | no
Si les requ�tes aux serveurs doivent demander des donn�es compress�es (d�faut=yes). N�cessite l'option de compilation zlib.
SECTION OfflineOptions
Options contr�lant le comportement hors ligne de WWWOFFLE.
ITEM pragma-no-cache
[<URL-SPEC>] pragma-no-cache = yes | no
Indique s'il faut rafra�chir une copie si la requ�te du navigateur a l'option 'Pragma: no-cache' (d�faut=yes). Cette option doit �tre � 'no' si hors ligne toutes les pages sont redemand�es par un navigateur d�fectueux.
ITEM confirm-requests
[<URL-SPEC>] confirm-requests = yes | no
Indique s'il faut renvoyer une page de confirmation au lieu d'enregistrer automatiquement les demandes faites hors ligne (d�faut=no).
ITEM dont-request
[<URL-SPEC>] dont-request = yes | no
Ne pas demander hors ligne une URL de ce mod�le (d�faut=no).
SECTION FetchOptions
Options contr�lant le rapatriement de pages demand�es hors ligne.
ITEM stylesheets
[<URL-SPEC>] stylesheets = yes | no
Rapatriement des feuilles de style (d�faut=no).
ITEM images
[<URL-SPEC>] images = yes | no
Rapatriement des images (d�faut=no).
ITEM webbug-images
[<URL-SPEC>] webbug-images = yes | no
Rapatriement des imagettes d'un seul pixel, n�cessite que l'option image soit aussi activ�e (d�faut=yes). Cette option est con�ue pour �tre utilis�e avec l'option replace-webbug-images de la section ModifyHTML.
ITEM frames
[<URL-SPEC>] frames = yes | no
Rapatriement des cadres (d�faut=no).
ITEM scripts
[<URL-SPEC>] scripts = yes | no
Rapatriement des scripts (par ex. Javascript) (d�faut=no).
ITEM objects
[<URL-SPEC>] objects = yes | no
Rapatriement des objets (par ex. fichier de classe Java) (d�faut=no).
SECTION IndexOptions
Options contr�lant l'affichage des index.
ITEM no-lasttime-index
no-lasttime-index = yes | no
Supprime la cr�ation des index des sessions pr�c�dentes (d�faut=no).
ITEM cycle-indexes-daily
cycle-indexes-daily = yes | no
Rotation quotidienne des index des sessions et demandes pr�c�dentes au lieu de pour chaque connexion (d�faut=no).
ITEM list-outgoing
<URL-SPEC> list-outgoing = yes | no
Indique s'il faut afficher ce type d'URL dans les demandes (d�faut=yes).
ITEM list-latest
<URL-SPEC> list-latest = yes | no
Indique s'il faut afficher ce type d'URL dans les sessions et demandes pr�c�dentes (d�faut=yes).
ITEM list-monitor
<URL-SPEC> list-monitor = yes | no
Indique s'il faut afficher ce type d'URL dans la liste des pages � surveiller p�riodiquement (d�faut=yes).
ITEM list-host
<URL-SPEC> list-host = yes | no
Indique s'il faut afficher ce type d'URL dans les listes par site (d�faut=yes).
ITEM list-any
<URL-SPEC> list-any = yes | no
Indique s'il faut afficher ce type d'URL dans toutes les listes (d�faut=yes).
SECTION ModifyHTML
Options contr�lant la modification du HTML m�moris�.
ITEM enable-modify-html
[<URL-SPEC>] enable-modify-html = yes | no
Active la modification du HTML dans cette section (d�faut=no). Sans cette option, les suivantes resteront sans effet. Avec cette option, il y aura un petit ralentissement.
ITEM add-cache-info
[<URL-SPEC>] add-cache-info = yes | no
� la fin des pages m�moris�es appara�tra la date et quelques liens (d�faut=no).
ITEM anchor-cached-begin
[<URL-SPEC>] anchor-cached-begin = (HTML code) |
Ce code HTML sera ins�r� avant les liens des pages m�moris�es (d�faut="").
ITEM anchor-cached-end
[<URL-SPEC>] anchor-cached-end = (HTML code) |
Ce code HTML sera ins�r� apr�s les liens des pages m�moris�es (d�faut="").
ITEM anchor-requested-begin
[<URL-SPEC>] anchor-requested-begin = (HTML code) |
Ce code HTML sera ins�r� avant les liens vers des pages demand�es (d�faut="").
ITEM anchor-requested-end
[<URL-SPEC>] anchor-requested-end = (HTML code) |
Ce code HTML sera ins�r� apr�s les liens vers des pages demand�es (d�faut="").
ITEM anchor-not-cached-begin
[<URL-SPEC>] anchor-not-cached-begin = (HTML code) |
Ce code HTML sera ins�r� avant les liens vers des pages ni pr�sentes ni demand�es (d�faut="").
ITEM anchor-not-cached-end
[<URL-SPEC>] anchor-not-cached-end = (HTML code) |
Ce code HTML sera ins�r� apr�s les liens vers des pages ni pr�sentes ni demand�es (d�faut="").
ITEM disable-script
[<URL-SPEC>] disable-script = yes | no
Supprime tous les scripts et �v�nements (d�faut=no).
ITEM disable-applet
[<URL-SPEC>] disable-applet = yes | no
Supprime toutes les applets Java (d�faut=no).
ITEM disable-style
[<URL-SPEC>] disable-style = yes | no
Supprime toutes les feuilles de style et leurs r�f�rences (d�faut=no).
ITEM disable-blink
[<URL-SPEC>] disable-blink = yes | no
Supprime les balises de clignotement (d�faut=no).
ITEM disable-flash
[<URL-SPEC>] disable-flash = yes | no
Supprime les animations Shockwave Flash (d�faut=no).
ITEM disable-meta-refresh
[<URL-SPEC>] disable-meta-refresh = yes | no
Supprime les balises d'en-t�te HTML qui redirigent le navigateur vers une autre page apr�s un d�lai (d�faut=no).
ITEM disable-meta-refresh-self
[<URL-SPEC>] disable-meta-refresh-self = yes | no
Supprime les balises d'en-t�te HTML qui indiquent au navigateur de recharger la m�me page apr�s un d�lai (d�faut=no).
ITEM disable-dontget-links
[<URL-SPEC>] disable-dontget-links = yes | no
Supprime les liens vers une URL de la section DontGet (d�faut=no).
ITEM disable-dontget-iframes
[<URL-SPEC>] disable-dontget-iframes = yes | no
Supprime les liens des URL de cadres de la section DontGet (d�faut=no).
ITEM replace-dontget-images
[<URL-SPEC>] replace-dontget-images = yes | no
Remplace les URL d'images de la section DontGet par une URL fixe (d�faut=no).
ITEM replacement-dontget-image
[<URL-SPEC>] replacement-dontget-image = (URL)
L'image de remplacement � utiliser pour les URL de la section DontGet (d�faut=/local/dontget/replacement.gif).
ITEM replace-webbug-images
[<URL-SPEC>] replace-webbug-images = yes | no
Remplace les URL d'imagettes d'un pixel par une URL fixe (d�faut=no). Cette option est con�ue pour �tre utilis�e avec l'option webbug-images de la section FetchOptions.
ITEM replacement-webbug-image
[<URL-SPEC>] replacement-webbug-image = (URL)
L'image de remplacement des imagettes d'un pixel (d�faut=/local/dontget/replacement.gif).
ITEM disable-animated-gif
[<URL-SPEC>] disable-animated-gif = yes | no
Supprime l'animation des images GIF anim�es (d�faut=no).
ITEM demoronise-ms-chars
[<URL-SPEC>] demoronise-ms-chars = yes | no
Remplace quelques caract�res �tranges ins�r�s par quelques applications Microsoft par des caract�res que la majorit� des navigateurs peut afficher (d�faut=no). Cette id�e provient du script Perl Demoroniser, du domaine public.
SECTION LocalHost
Une liste de noms sous lequel l'h�te du serveur WWWOFFLE peut �tre connu. Ceci permet d'�viter que ce dernier ne se contacte lui-m�me sous un autre nom.
ITEM 
(host)
Un nom ou une adresse IP qui avec le num�ro de port (cf. section StartUp) indique le serveur proxy WWWOFFLE. Les noms doivent correspondre exactement, ce n'est pas un patron mod�le. Le premier nomm� est utilis� comme nom du serveur pour plusieurs choses et doit donc �tre un nom fonctionnel pour tous les clients du r�seau. Aucun nom ainsi indiqu� n'abrite de page m�moris�e ou rapatri�e par le proxy.
SECTION LocalNet
Une liste de noms dont les serveurs web sont toujours accessibles m�me hors ligne, et dont les pages ne sont pas m�moris�es par WWWOFFLE car sur le r�seau local.
ITEM 
(host)
Un nom nom ou adresse IP toujours accessible et dont les pages ne sont pas m�moris�es par WWWOFFLE. La reconnaissance de ce nom ou adresse utilise un patron mod�le. Un h�te peut �tre exclu en le pr�fixant par un point d'exclamation '!', tous les alias et adresses IP possibles sont aussi requis. Toutes ces entr�es sont suppos�es toujours accessibles m�me hors ligne. Aucun des h�tes ainsi mentionn�s n'a de page m�moris�e.
SECTION AllowedConnectHosts
Une liste de clients autoris�s � se connecter au serveur.
ITEM 
(host)
Un nom d'h�te ou une adresse IP autoris� � se connecter au serveur. La reconnaissance de ce nom ou adresse utilise un patron mod�le. Un h�te peut �tre exclu en le pr�fixant par un point d'interrogation '!', tous les alias et adresses IP sont aussi requis. Tous les h�tes de la section LocalHost sont aussi autoris�s.
SECTION AllowedConnectUsers
Une liste des utilisateurs autoris�s � se connecter et leurs mots de passe.
ITEM 
(username):(password)
Le nom (login) et le mot de passe des utilisateurs autoris�s � se connecter au serveur. Si cette section est laiss�e vide, il n'y a pas d'authentification. Le nom et le mot de passe sont inscrits en clair. Ceci requiert l'utilisation de navigateurs respectant le standard d'authentification HTTP/1.1
SECTION DontCache
Une liste d'URL non m�moris�es par WWWOFFLE.
ITEM 
[!]URL-SPECIFICATION
Ne m�morise aucune URL correspondant � ce mod�le. L'<a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> peut �tre exclue pour autoriser la m�morisation. Les URL ne seront pas enregistr�es hors ligne.
SECTION DontGet
Une liste d'URL qui ne seront pas rapatri�es par WWWOFFLE (parce qu'elles ne contiennent que des publicit�s, par exemple).
ITEM 
[!]URL-SPECIFICATION
Ne pas rapatrier une URL conforme � ce mod�le. L'exclusion permet le rapatriement.
ITEM replacement
[<URL-SPEC>] replacement = (URL)
L'URL de remplacement des URL conformes � <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a>, � la place du message d'erreur standard (d�faut=none). Les URL du r�pertoire /local/dontget/ sont sugg�r�es pour cet office (par ex. replacement.gif, replacement.png qui sont des images d'un seul pixel transparent, ou replacement.js qui est un fichier javascript vide).
ITEM get-recursive
<URL-SPEC> get-recursive = yes | no
Indique si les URL de ce mod�le doit �tre rapatri�es r�cursivement (d�faut=yes).
ITEM location-error
<URL-SPEC> location-error = yes | no
Quand une r�ponse d'URL contient un en-t�te 'Location' qui redirige vers une URL � ne pas rapatrier (indiqu�e dans cette section), alors la r�ponse est modifi�e en message d'erreur (d�faut=no). Ceci emp�chera un proxy de fournisseur d'acc�s de rediriger les utilisateurs vers des publicit�s si elles sont mentionn�es dans cette section.
SECTION DontCompress
Une liste de types MIME et d'extensions de nom de fichiers � ne pas compresser par WWWOFFLE (parce qu'elles sont d�j� compress�es, ou n'en valent pas la peine). Requiert l'option de compilation zlib.
ITEM mime-type
mime-type = (mime-type)/(subtype)
Le type MIME d'une URL � ne pas comprimer dans le cache ou en servant des pages compress�es aux navigateurs.
ITEM file-ext
file-ext = .(file-ext)
Une extension de fichier � ne pas demander compress� � un serveur.
SECTION CensorHeader
Une liste d'en-t�tes HTTP � enlever des requ�tes aux serveurs web et les r�ponses qui en reviennent.
ITEM 
[<URL-SPEC>] (header) = yes | no | (string)
Un nom d'en-t�te (par ex. From, Cookie, Set-Cookie, User-Agent) et la cha�ne de remplacement (d�faut=no). L'en-t�te est sensible � la casse, et ne doit pas se terminer par un deux-points ':'. La valeur "no" signifie que cet en-t�te n'est pas modifi�, "yes" ou pas de cha�ne peut �tre utilis� pour supprimer cet en-t�te, et une cha�ne remplace la valeur de cet en-t�te. Seuls les en-t�tes trouv�s sont remplac�s, aucun nouvel en-t�te n'est ajout�.
ITEM referer-self
[<URL-SPEC>] referer-self = yes | no
Met l'en-t�te Referer � la m�me valeur que l'URL demand�e (d�faut=no).
ITEM referer-self-dir
[<URL-SPEC>] referer-self-dir = yes | no
Met l'en-t�te Referer au nom du r�pertoire de l'URL demand�e (d�faut=no). Cette option est prioritaire sur l'option referer-self.
SECTION FTPOptions
Options utilis�es pour le protocole FTP.
ITEM anon-username
anon-username = (string)
Le nom d'utilisateur FTP anonyme utilis� (d�faut=anonymous).
ITEM anon-password
anon-password = (string)
Le mot de passe � utiliser pour le FTP anonyme (d�faut d�termin� � l'ex�cution). Si on utilise un coupe-feu, la valeur peut �tre invalide pour le serveur FTP, et doit �tre remplac�e.
ITEM auth-username
<URL-SPEC> auth-username = (string)
Le nom d'utilisateur sur ces serveurs au lieu de l'anonyme par d�faut.
ITEM auth-password
<URL-SPEC> auth-password = (string)
Le mot de passe � utiliser au lieu de celui par d�faut.
SECTION MIMETypes
Types MIME � utiliser en servant des fichiers qui ne sont pas rapatri�s sous le protocole HTTP, ou pour les fichiers du serveur web incorpor�.
ITEM default
default = (mime-type)/(subtype)
Type MIME par d�faut (d�faut=text/plain).
ITEM 
.(file-ext) = (mime-type)/(subtype)
Type MIME associ� � une extension. Le point '.' initial doit �tre pr�sent. Si plus d'une extension convient, la plus longue est choisie.
SECTION Proxy
Les noms des serveurs proxys externes � utiliser.
ITEM proxy
[<URL-SPEC>] proxy = (host[:port])
Le nom d'h�te et le port du proxy.
ITEM auth-username
<URL-SPEC> auth-username = (string)
Le nom d'utilisateur � utiliser. La sp�cification <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> correspond ici au serveur proxy, et non � l'URL demand�e.
ITEM auth-password
<URL-SPEC> auth-password = (string)
Le mot de passe � utiliser. La sp�cification <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> correspond ici au serveur proxy, et non � l'URL demand�e.
ITEM ssl
[<URL-SPEC>] ssl = (host[:port])
Un serveur proxy utilis� pour les connexions SSL (Secure Socket Layer), par ex. https. Noter que seule la partie h�te  de <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> est v�rifi�e, et le reste doit �tre remplac� par des jokers '*'.
SECTION Alias
Une liste d'alias de remplacement de serveurs et chemins. Aussi pour les serveurs connus sous deux noms.
ITEM 
URL-SPECIFICATION = URL-SPECIFICATION
Toute requ�te correspondant � la premi�re <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> sera remplac�e par la seconde. La correspondance doit �tre exacte, il n'y a pas de patron, les arguments de l'URL sont ignor�s.
SECTION Purge
La m�thode pour d�terminer les pages � purger, l'�ge par d�faut, l'�ge sp�cifique � l'h�te des pages en jours, et la taille maximum du cache.
ITEM use-mtime
use-mtime = yes | no
La m�thode utilis�e pour d�cider des fichiers � purger, dernier acc�s (atime) ou derni�re modification (ctime) (d�faut=no).
ITEM max-size
max-size = (size)
La taille maximale du cache en m�ga-octets apr�s la purge (d�faut=0). Une valeur nulle signifie pas de limite. Si cette option et min-free sont toutes deux utilis�es, la plus petite taille de cache est choisie. Cette option tient compte des URL jamais purg�es pour mesurer la taille du cache, mais ne les supprime pas.
ITEM min-free
min-free = (size)
La taille minimale d'espace libre en m�ga-octets apr�s la purge (d�faut=0). Une taille nulle signifie aucune limite pour l'espace libre. Si cette option et l'option max-size sont toutes deux utilis�es, la plus petite taille de cache est choisie. Cette option tient compte des URL jamais purg�es pour le d�compte, mais ne les supprime pas.
ITEM use-url
use-url = yes | no
Si 'yes' alors utilise l'URL pour d�cider de l'�ge, sinon utilise seulement le protocole et l'h�te (d�faut=no).
ITEM del-dontget
del-dontget = yes | no
Si 'yes' alors supprime les pages correspondant � la section DontGet (d�faut=no).
ITEM del-dontcache
del-dontcache = yes | no
Si 'yes' alors supprime les pages correspondant � la section DontCache (d�faut=no).
ITEM age
[<URL-SPEC>] age = (age)
L'�ge maximum en jours dans le cache pour les URL correspondant � <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> (d�faut=14). Un �ge nul signifie ne pas garder, et n�gatif ne pas effacer. <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> correspond seulement au protocole et � l'h�te, sauf si l'option use-url est activ�e. Des temps plus longs peuvent �tre indiqu�s par les suffixes 'w', 'm' ou 'y' pour semaine, mois et ann�e (par ex. 2w=14).
ITEM compress-age
[<URL-SPEC>] compress-age = (age)
L'�ge maximum de stockage sans compression dans le cache pour les URL correspondant � <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> (d�faut=-1). Requiert l'option de compilation zlib. L'�ge a la m�me signification que pour l'option age.
TAIL
<h2><a name="WILDCARD">WILDCARD</a></h2> Une correspondance joker utilise le caract�re '*' pour repr�senter un groupe quelconque de caract�res. <p> Ceci est fondamentalement identique aux expressions en ligne de commande des shells DOS ou UNIX, except� le fait que '*' correspond aussi au caract�re '/'. <p> Par exemple,<dl><dt>*.gif<dd>correspond � foo.gif et bar.gif</dl><dl><dt>*.foo.com<dd>correspond � www.foo.com et ftp.foo.com</dl><dl><dt>/foo/*<dd>correspond � /foo/bar.html et /foo/bar/foobar.html</dl><h2><a name="URL-SPECIFICATION">URL-SPECIFICATION</a></h2> En indiquant un h�te, un protocole et un chemin dans de nombreuses sections, une <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> peut �tre utilis�e, c'est un moyen de reconna�tre une URL. <p> Pour cette explication, une URL peut �tre compos�e de cinq parties.<dl><dt>proto<dd>Le protocole utilis� (par ex. 'http', 'ftp')</dl><dl><dt>host<dd>Le nom du serveur (par ex. 'www.gedanken.demon.co.uk').</dl><dl><dt>port<dd>Le num�ro de port sur le serveur (par ex. 80 pour le HTTP).</dl><dl><dt>path<dd>Le chemin sur le serveur (par ex. '/bar.html') ou un nom de r�pertoire (par ex. '/foo/').</dl><dl><dt>args<dd>Arguments de l'URL pour les scripts CGI, etc. (par ex. 'search=foo').</dl> <p> Par exemple, prenons la page d'accueil de WWWOFFLE, http://www.gedanken.demon.co.uk/wwwoffle/ <p> Le protocole est 'http', l'h�te 'www.gedanken.demon.co.uk', le port est celui par d�faut (ici, 80), et le chemin est '/wwwoffle/'. <p> En g�n�ral, on �crira (proto)://(host)[:(port)][/(path)[?(args)]] <p> o� les crochets [] indiquent une partie optionnelle, et les parenth�ses () un nom ou un num�ro fourni par l'utilisateur. <p> Ci dessous quelques exemples de <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> :<dl><dt>*://*/*<dd>Tout protocole, tout h�te, tout port, tout chemin, tous arguments (ce qui revient � 'default').</dl><dl><dt>*://*/(path)<dd>Tout protocole, tout h�te, tout port, chemin pr�cis�, tous arguments.</dl><dl><dt>*://*/*.(ext)<dd>Tout protocole, tout h�te, tout port, extension pr�cis�e, tous arguments.</dl><dl><dt>*://*/*?<dd>Tout protocole, tout h�te, tout port, tout chemin, pas d'arguments.</dl><dl><dt>*://*/(path)?*<dd>Tout protocole, tout h�te, tout port, chemin pr�cis�, tous arguments.</dl><dl><dt>*://(host)/*<dd>Tout protocole, h�te pr�cis�, tout port, tout chemin, tous arguments.</dl><dl><dt>(proto)://*/*<dd>Protocole pr�cis�, tout h�te, tout port, tout chemin, tous arguments.</dl> <p> (proto)://(host)/*  Protocole et h�te pr�cis�s, tout port, tout chemin, tous arguments. <p> (proto)://(host):/* Protocole et h�te pr�cis�s, port par d�faut, tout chemin, tous arguments. <p> *://(host):(port)/* Tout protocole, h�te et port pr�cis�s, tout chemin, tous arguments. <p> La correspondance des h�tes, chemins et arguments utilise les jokers d�crits ci-dessus. <p> Dans quelques sections acceptant les <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a>, celles-ci peuvent �tre exclues en les pr�fixant d'un point d'exclamation '!'. Cela signifie que la comparaison renverra la valeur logique contraire � celle renvoy�e sans le '!'. Si toutes les <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> sont exclues, et qu'on rajoute '*://*/*' � la fin, le sens de la section est renvers�.
