TITLE WWWOFFLE - Fichier de configuration - Version 2.7
HEAD
<h2><a name="Introduction">Introduction</a></h2> Le fichier de configuration (wwwoffle.conf) spécifie tous les paramètres qui contrôlent l'activité du serveur proxy. Le fichier est divisé en sections décrites ci-dessous contenant chacune une série de paramètres. Le fichier CHANGES.CONF explique les changements de ce fichier de configuration par rapport aux versions précédentes. <p> Le fichier est divisé en sections, chacune pouvant être vide ou contenir une ou plusieurs lignes d'information de configuration. Les sections sont nommées, et leur ordre d'apparition dans le fichier n'est pas important. <p> Le format général de chacune des sections est le même. Le nom de la section est sur une ligne, et en marque le début. Le contenu de la section est délimité par une paire de lignes contenant seulement les caractères '{' et '}', ou '[' et ']'. Quand la paire '{' et '}' est utilisée, les lignes encloses contiennent des informations de configuration. Quand la paire '[' et ']' est utilisée, il doit y avoir à l'intérieur de cette dernière une seule ligne non vide, contenant le nom d'un fichier (dans le même répertoire) contenant les lignes de configuration de cette section. <p> Les commentaires sont signalés par le caractère '#' au début de la ligne, et sont ignorés. Les lignes vides sont aussi permises et ignorées. <p> Les entités <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> (<a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> en abrégé) et <a href="/configuration/#WILDCARD">WILDCARD</a> ont une signification particulière dans le fichier de configuration, et sont décrites à la fin. Toute entité enclose entre parenthèses '(' et ')' dans les descriptions signifie un paramètre fourni par l'utilisateur, tout ce qui est entre crochets '[' et ']' est optionnel, et la barre verticale '|' indique une alternative. Certaines options s'appliquent seulement à des URL, ceci est précisé par une <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> enclose entre '&lt;' &amp; '&gt;' dans l'option, la première <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> qui correspond au motif est utilisée. Si aucune <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> n'est donnée, alors toute URL correspond.
SECTION StartUp
Cette section contient les paramètres utilisés au lancement du programme, les changements éventuels sont ignorés si la configuration est relue pendant l'exécution.
ITEM bind-ipv4
bind-ipv4 = (hostname) | (ip-address) | none
Spécifie le nom d'hôte ou l'adresse IP où lier les sockets proxy HTTP et port de contrôle WWWOFFLE utilisant IPv4 (par défaut '0.0.0.0'). Si 'none' est indiqué, alors aucun socket IPv4 n'est utilisé.
ITEM bind-ipv6
bind-ipv6 = (hostname) | (ip-address) | none
Spécifie le nom d'hôte ou l'adresse IP où lier les sockets proxy HTTP et port de contrôle WWWOFFLE utilisant IPv6 (par défaut '::'). Si 'none' est indiqué, alors aucun socket IPV6 n'est utilisé. L'option de compilation IPv6 est requise.
ITEM http-port
http-port = (port)
Un entier indiquant le port du serveur proxy (8080 par défaut).
ITEM wwwoffle-port
wwwoffle-port = (port)
Un entier indiquant le port de contrôle WWWOFFLE (8081 par défaut).
ITEM spool-dir
spool-dir = (dir)
Le chemin complet du répertoire de cache (répertoire de spool) (défaut=/var/spool/wwwoffle).
ITEM run-uid
run-uid = (user) | (uid)
Le nom d'utilisateur ou le numéro UID sous lequel le serveur WWWOFFLE est lancé (défaut=aucun). Cette option n'est pas applicable sous win32 et ne fonctionne que si le serveur est lancé par l'utilisateur root sous UNIX.
ITEM run-gid
run-gid = (group) | (gid)
Le groupe ou le numéro GID sous lequel le serveur WWWOFFLE est lancé (défaut=aucun). Cette option n'est pas applicable sous win32, et ne fonctionne que si le serveur est lancé par l'utilisateur root sous UNIX.
ITEM use-syslog
use-syslog = yes | no
Indique si le service syslog est utilisé pour les messages (défaut=yes).
ITEM password
password = (word)
Le mot de passe utilisé pour l'authentification des pages de contrôle, pour l'effacement des pages mémorisées, etc. (défaut=aucun). Pour sécuriser la configuration, la lecture du fichier de configuration doit être réservée aux utilisateurs autorisés.
ITEM max-servers
max-servers = (integer)
Le nombre maximum de processus serveurs lancés en ligne et pour le rapatriement automatique (défaut=8).
ITEM max-fetch-servers
max-fetch-servers = (integer)
Le nombre maximum de serveurs lancés pour le rapatriement automatique des pages demandées en mode hors-ligne (défaut=4). Cette valeur doit être inférieure à max-servers pour permettre l'usage interactif simultané.
SECTION Options
Options contrôlant le fonctionnement du programme
ITEM log-level
log-level = debug | info | important | warning | fatal
Le niveau minimum de message syslog ou stderr (défaut=important).
ITEM socket-timeout
socket-timeout = (time)
Le temps d'attente en secondes des données sur un socket avant abandon par WWWOFFLE (défaut=120).
ITEM dns-timeout
dns-timeout = (time)
Le temps d'attente en secondes d'une requête DNS (Domain Name Service) avant abandon par WWWOFFLE (défaut=60).
ITEM connect-timeout
connect-timeout = (time)
Le temps d'attente en secondes pour obtenir un socket avant abandon par WWWOFFLE (défaut=30).
ITEM connect-retry
connect-retry = yes | no
Si une connexion à un serveur distant ne peut être obtenue, alors WWWOFFLE essaiera encore après un court délai (défaut=no).
ITEM ssl-allow-port
ssl-allow-port = (integer)
Un numéro de port autorisé pour les connexions SSL (Secure Socket Layer), par ex. https. Cette option devrait être fixée à 443 pour autoriser https, il peut y avoir plusieurs lignes ssl pour autoriser d'autres ports si besoin.
ITEM dir-perm
dir-perm = (octal int)
Les permissions de répertoires pour la création des répertoires de spool (défaut=0755). Cette option écrase le umask de l'utilisateur, et doit être octale, commençant par un zéro '0'.
ITEM file-perm
file-perm = (octal int)
Les permissions de fichiers pour la création des fichiers de spool (défaut=0644). Cette option écrase le umask de l'utilisateur, et doit être octale, commençant par un zéro '0'.
ITEM run-online
run-online = (filename)
Le nom d'un programme à lancer quand WWWOFFLE est commuté en mode en ligne (défaut=aucun). Ce programme est lancé avec un seul paramètre, fixé au nom du mode, "online".
ITEM run-offline
run-offline = (filename)
Le nom du programme à lancer quand WWWOFFLE est commuté en mode hors-ligne (défaut=aucun). Ce programme est lancé avec un seul paramètre, le nom du mode, "offline".
ITEM run-autodial
run-autodial = (filename)
Le nom d'un programme à lancer quand WWWOFFLE est commuté en mode automatique (défaut=aucun). Le programme est lancé avec un seul paramètre, le nom du mode, "autodial".
ITEM run-fetch
run-fetch = (filename)
Le nom du programme à lancer quand WWWOFFLE démarre ou arrête le rapatriement automatique (défaut=aucun). Ce programme est lancé avec deux paramètres, le premier le mot "fetch", et le second l'un des mots "start" ou "stop".
ITEM lock-files
lock-files = yes | no
Active l'usage des fichiers verrous pour empêcher plus d'un processus WWWOFFLE de rapatrier simultanément la même URL (défaut=no).
ITEM reply-compressed-data
reply-compressed-data = yes | no
Si les réponses faites au navigateur doivent contenir des données compressées quand demandé (défaut=no). Nécessite l'option de compilation zlib.
SECTION OnlineOptions
Options contrôlant le comportement en ligne de WWWOFFLE.
ITEM request-changed
[<URL-SPEC>] request-changed = (time)
En ligne, les pages ne seront rapatriées que si la version mémorisée est plus ancienne que le temps indiqué en secondes (défaut=600). Une valeur négative indique que les pages mémorisées sont toujours utilisées en ligne. Des temps longs peuvent être indiqués par les suffixes 'm', 'h', 'd' ou 'w' pour minute, heure, jour ou semaine (par ex. 10m=600).
ITEM request-changed-once
[<URL-SPEC>] request-changed-once = yes | no
En ligne, les pages ne seront rapatriées qu'une seule fois par session (défaut=yes). Cette option a priorité sur l'option request-changed.
ITEM request-expired
[<URL-SPEC>] request-expired = yes | no
En ligne, les pages périmées seront rafraîchies (défaut=no). Cette option a priorité sur les options request-changed et request-changed-once.
ITEM request-no-cache
[<URL-SPEC>] request-no-cache = yes | no
En ligne, les pages à ne pas cacher seront toujours redemandées (défaut=no). Cette option a priorité sur les options request-changed et request-changed-once.
ITEM try-without-password
[<URL-SPEC>] try-without-password = yes | no
Si une requête demande une page utilisant un nom et un mot de passe, alors une requête de la même page sera faite sans (défaut=yes). Ceci autorise la requête d'une page sans mot de passe à être redirigée vers la version avec.
ITEM intr-download-keep
[<URL-SPEC>] intr-download-keep = yes | no
Si le navigateur ferme la connexion en ligne, alors la page incomplète sera conservée (défaut=no).
ITEM intr-download-size
[<URL-SPEC>] intr-download-size = (integer)
Si le navigateur ferme la connexion en ligne, la page devrait continuer à être rapatriée si sa taille est inférieure à celle indiquée en kilo-octets (défaut=1).
ITEM intr-download-percent
[<URL-SPEC>] intr-download-percent = (integer)
Si le navigateur ferme la connexion en ligne, la page devrait continuer à être rapatriée si le pourcentage indiqué est atteint (défaut=80).
ITEM timeout-download-keep
[<URL-SPEC>] timeout-download-keep = yes | no
Si la connexion au serveur est abandonnée, la page incomplète doit être conservée (défaut=no).
ITEM request-compressed-data
[<URL-SPEC>] request-compressed-data = yes | no
Si les requêtes aux serveurs doivent demander des données compressées (défaut=yes). Nécessite l'option de compilation zlib.
SECTION OfflineOptions
Options contrôlant le comportement hors ligne de WWWOFFLE.
ITEM pragma-no-cache
[<URL-SPEC>] pragma-no-cache = yes | no
Indique s'il faut rafraîchir une copie si la requête du navigateur a l'option 'Pragma: no-cache' (défaut=yes). Cette option doit être à 'no' si hors ligne toutes les pages sont redemandées par un navigateur défectueux.
ITEM confirm-requests
[<URL-SPEC>] confirm-requests = yes | no
Indique s'il faut renvoyer une page de confirmation au lieu d'enregistrer automatiquement les demandes faites hors ligne (défaut=no).
ITEM dont-request
[<URL-SPEC>] dont-request = yes | no
Ne pas demander hors ligne une URL de ce modèle (défaut=no).
SECTION FetchOptions
Options contrôlant le rapatriement de pages demandées hors ligne.
ITEM stylesheets
[<URL-SPEC>] stylesheets = yes | no
Rapatriement des feuilles de style (défaut=no).
ITEM images
[<URL-SPEC>] images = yes | no
Rapatriement des images (défaut=no).
ITEM webbug-images
[<URL-SPEC>] webbug-images = yes | no
Rapatriement des imagettes d'un seul pixel, nécessite que l'option image soit aussi activée (défaut=yes). Cette option est conçue pour être utilisée avec l'option replace-webbug-images de la section ModifyHTML.
ITEM frames
[<URL-SPEC>] frames = yes | no
Rapatriement des cadres (défaut=no).
ITEM scripts
[<URL-SPEC>] scripts = yes | no
Rapatriement des scripts (par ex. Javascript) (défaut=no).
ITEM objects
[<URL-SPEC>] objects = yes | no
Rapatriement des objets (par ex. fichier de classe Java) (défaut=no).
SECTION IndexOptions
Options contrôlant l'affichage des index.
ITEM no-lasttime-index
no-lasttime-index = yes | no
Supprime la création des index des sessions précédentes (défaut=no).
ITEM cycle-indexes-daily
cycle-indexes-daily = yes | no
Rotation quotidienne des index des sessions et demandes précédentes au lieu de pour chaque connexion (défaut=no).
ITEM list-outgoing
<URL-SPEC> list-outgoing = yes | no
Indique s'il faut afficher ce type d'URL dans les demandes (défaut=yes).
ITEM list-latest
<URL-SPEC> list-latest = yes | no
Indique s'il faut afficher ce type d'URL dans les sessions et demandes précédentes (défaut=yes).
ITEM list-monitor
<URL-SPEC> list-monitor = yes | no
Indique s'il faut afficher ce type d'URL dans la liste des pages à surveiller périodiquement (défaut=yes).
ITEM list-host
<URL-SPEC> list-host = yes | no
Indique s'il faut afficher ce type d'URL dans les listes par site (défaut=yes).
ITEM list-any
<URL-SPEC> list-any = yes | no
Indique s'il faut afficher ce type d'URL dans toutes les listes (défaut=yes).
SECTION ModifyHTML
Options contrôlant la modification du HTML mémorisé.
ITEM enable-modify-html
[<URL-SPEC>] enable-modify-html = yes | no
Active la modification du HTML dans cette section (défaut=no). Sans cette option, les suivantes resteront sans effet. Avec cette option, il y aura un petit ralentissement.
ITEM add-cache-info
[<URL-SPEC>] add-cache-info = yes | no
À la fin des pages mémorisées apparaîtra la date et quelques liens (défaut=no).
ITEM anchor-cached-begin
[<URL-SPEC>] anchor-cached-begin = (HTML code) |
Ce code HTML sera inséré avant les liens des pages mémorisées (défaut="").
ITEM anchor-cached-end
[<URL-SPEC>] anchor-cached-end = (HTML code) |
Ce code HTML sera inséré après les liens des pages mémorisées (défaut="").
ITEM anchor-requested-begin
[<URL-SPEC>] anchor-requested-begin = (HTML code) |
Ce code HTML sera inséré avant les liens vers des pages demandées (défaut="").
ITEM anchor-requested-end
[<URL-SPEC>] anchor-requested-end = (HTML code) |
Ce code HTML sera inséré après les liens vers des pages demandées (défaut="").
ITEM anchor-not-cached-begin
[<URL-SPEC>] anchor-not-cached-begin = (HTML code) |
Ce code HTML sera inséré avant les liens vers des pages ni présentes ni demandées (défaut="").
ITEM anchor-not-cached-end
[<URL-SPEC>] anchor-not-cached-end = (HTML code) |
Ce code HTML sera inséré après les liens vers des pages ni présentes ni demandées (défaut="").
ITEM disable-script
[<URL-SPEC>] disable-script = yes | no
Supprime tous les scripts et événements (défaut=no).
ITEM disable-applet
[<URL-SPEC>] disable-applet = yes | no
Supprime toutes les applets Java (défaut=no).
ITEM disable-style
[<URL-SPEC>] disable-style = yes | no
Supprime toutes les feuilles de style et leurs références (défaut=no).
ITEM disable-blink
[<URL-SPEC>] disable-blink = yes | no
Supprime les balises de clignotement (défaut=no).
ITEM disable-flash
[<URL-SPEC>] disable-flash = yes | no
Supprime les animations Shockwave Flash (défaut=no).
ITEM disable-meta-refresh
[<URL-SPEC>] disable-meta-refresh = yes | no
Supprime les balises d'en-tête HTML qui redirigent le navigateur vers une autre page après un délai (défaut=no).
ITEM disable-meta-refresh-self
[<URL-SPEC>] disable-meta-refresh-self = yes | no
Supprime les balises d'en-tête HTML qui indiquent au navigateur de recharger la même page après un délai (défaut=no).
ITEM disable-dontget-links
[<URL-SPEC>] disable-dontget-links = yes | no
Supprime les liens vers une URL de la section DontGet (défaut=no).
ITEM disable-dontget-iframes
[<URL-SPEC>] disable-dontget-iframes = yes | no
Supprime les liens des URL de cadres de la section DontGet (défaut=no).
ITEM replace-dontget-images
[<URL-SPEC>] replace-dontget-images = yes | no
Remplace les URL d'images de la section DontGet par une URL fixe (défaut=no).
ITEM replacement-dontget-image
[<URL-SPEC>] replacement-dontget-image = (URL)
L'image de remplacement à utiliser pour les URL de la section DontGet (défaut=/local/dontget/replacement.gif).
ITEM replace-webbug-images
[<URL-SPEC>] replace-webbug-images = yes | no
Remplace les URL d'imagettes d'un pixel par une URL fixe (défaut=no). Cette option est conçue pour être utilisée avec l'option webbug-images de la section FetchOptions.
ITEM replacement-webbug-image
[<URL-SPEC>] replacement-webbug-image = (URL)
L'image de remplacement des imagettes d'un pixel (défaut=/local/dontget/replacement.gif).
ITEM disable-animated-gif
[<URL-SPEC>] disable-animated-gif = yes | no
Supprime l'animation des images GIF animées (défaut=no).
ITEM demoronise-ms-chars
[<URL-SPEC>] demoronise-ms-chars = yes | no
Remplace quelques caractères étranges insérés par quelques applications Microsoft par des caractères que la majorité des navigateurs peut afficher (défaut=no). Cette idée provient du script Perl Demoroniser, du domaine public.
SECTION LocalHost
Une liste de noms sous lequel l'hôte du serveur WWWOFFLE peut être connu. Ceci permet d'éviter que ce dernier ne se contacte lui-même sous un autre nom.
ITEM 
(host)
Un nom ou une adresse IP qui avec le numéro de port (cf. section StartUp) indique le serveur proxy WWWOFFLE. Les noms doivent correspondre exactement, ce n'est pas un patron modèle. Le premier nommé est utilisé comme nom du serveur pour plusieurs choses et doit donc être un nom fonctionnel pour tous les clients du réseau. Aucun nom ainsi indiqué n'abrite de page mémorisée ou rapatriée par le proxy.
SECTION LocalNet
Une liste de noms dont les serveurs web sont toujours accessibles même hors ligne, et dont les pages ne sont pas mémorisées par WWWOFFLE car sur le réseau local.
ITEM 
(host)
Un nom nom ou adresse IP toujours accessible et dont les pages ne sont pas mémorisées par WWWOFFLE. La reconnaissance de ce nom ou adresse utilise un patron modèle. Un hôte peut être exclu en le préfixant par un point d'exclamation '!', tous les alias et adresses IP possibles sont aussi requis. Toutes ces entrées sont supposées toujours accessibles même hors ligne. Aucun des hôtes ainsi mentionnés n'a de page mémorisée.
SECTION AllowedConnectHosts
Une liste de clients autorisés à se connecter au serveur.
ITEM 
(host)
Un nom d'hôte ou une adresse IP autorisé à se connecter au serveur. La reconnaissance de ce nom ou adresse utilise un patron modèle. Un hôte peut être exclu en le préfixant par un point d'interrogation '!', tous les alias et adresses IP sont aussi requis. Tous les hôtes de la section LocalHost sont aussi autorisés.
SECTION AllowedConnectUsers
Une liste des utilisateurs autorisés à se connecter et leurs mots de passe.
ITEM 
(username):(password)
Le nom (login) et le mot de passe des utilisateurs autorisés à se connecter au serveur. Si cette section est laissée vide, il n'y a pas d'authentification. Le nom et le mot de passe sont inscrits en clair. Ceci requiert l'utilisation de navigateurs respectant le standard d'authentification HTTP/1.1
SECTION DontCache
Une liste d'URL non mémorisées par WWWOFFLE.
ITEM 
[!]URL-SPECIFICATION
Ne mémorise aucune URL correspondant à ce modèle. L'<a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> peut être exclue pour autoriser la mémorisation. Les URL ne seront pas enregistrées hors ligne.
SECTION DontGet
Une liste d'URL qui ne seront pas rapatriées par WWWOFFLE (parce qu'elles ne contiennent que des publicités, par exemple).
ITEM 
[!]URL-SPECIFICATION
Ne pas rapatrier une URL conforme à ce modèle. L'exclusion permet le rapatriement.
ITEM replacement
[<URL-SPEC>] replacement = (URL)
L'URL de remplacement des URL conformes à <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a>, à la place du message d'erreur standard (défaut=none). Les URL du répertoire /local/dontget/ sont suggérées pour cet office (par ex. replacement.gif, replacement.png qui sont des images d'un seul pixel transparent, ou replacement.js qui est un fichier javascript vide).
ITEM get-recursive
<URL-SPEC> get-recursive = yes | no
Indique si les URL de ce modèle doit être rapatriées récursivement (défaut=yes).
ITEM location-error
<URL-SPEC> location-error = yes | no
Quand une réponse d'URL contient un en-tête 'Location' qui redirige vers une URL à ne pas rapatrier (indiquée dans cette section), alors la réponse est modifiée en message d'erreur (défaut=no). Ceci empêchera un proxy de fournisseur d'accès de rediriger les utilisateurs vers des publicités si elles sont mentionnées dans cette section.
SECTION DontCompress
Une liste de types MIME et d'extensions de nom de fichiers à ne pas compresser par WWWOFFLE (parce qu'elles sont déjà compressées, ou n'en valent pas la peine). Requiert l'option de compilation zlib.
ITEM mime-type
mime-type = (mime-type)/(subtype)
Le type MIME d'une URL à ne pas comprimer dans le cache ou en servant des pages compressées aux navigateurs.
ITEM file-ext
file-ext = .(file-ext)
Une extension de fichier à ne pas demander compressé à un serveur.
SECTION CensorHeader
Une liste d'en-têtes HTTP à enlever des requêtes aux serveurs web et les réponses qui en reviennent.
ITEM 
[<URL-SPEC>] (header) = yes | no | (string)
Un nom d'en-tête (par ex. From, Cookie, Set-Cookie, User-Agent) et la chaîne de remplacement (défaut=no). L'en-tête est sensible à la casse, et ne doit pas se terminer par un deux-points ':'. La valeur "no" signifie que cet en-tête n'est pas modifié, "yes" ou pas de chaîne peut être utilisé pour supprimer cet en-tête, et une chaîne remplace la valeur de cet en-tête. Seuls les en-têtes trouvés sont remplacés, aucun nouvel en-tête n'est ajouté.
ITEM referer-self
[<URL-SPEC>] referer-self = yes | no
Met l'en-tête Referer à la même valeur que l'URL demandée (défaut=no).
ITEM referer-self-dir
[<URL-SPEC>] referer-self-dir = yes | no
Met l'en-tête Referer au nom du répertoire de l'URL demandée (défaut=no). Cette option est prioritaire sur l'option referer-self.
SECTION FTPOptions
Options utilisées pour le protocole FTP.
ITEM anon-username
anon-username = (string)
Le nom d'utilisateur FTP anonyme utilisé (défaut=anonymous).
ITEM anon-password
anon-password = (string)
Le mot de passe à utiliser pour le FTP anonyme (défaut déterminé à l'exécution). Si on utilise un coupe-feu, la valeur peut être invalide pour le serveur FTP, et doit être remplacée.
ITEM auth-username
<URL-SPEC> auth-username = (string)
Le nom d'utilisateur sur ces serveurs au lieu de l'anonyme par défaut.
ITEM auth-password
<URL-SPEC> auth-password = (string)
Le mot de passe à utiliser au lieu de celui par défaut.
SECTION MIMETypes
Types MIME à utiliser en servant des fichiers qui ne sont pas rapatriés sous le protocole HTTP, ou pour les fichiers du serveur web incorporé.
ITEM default
default = (mime-type)/(subtype)
Type MIME par défaut (défaut=text/plain).
ITEM 
.(file-ext) = (mime-type)/(subtype)
Type MIME associé à une extension. Le point '.' initial doit être présent. Si plus d'une extension convient, la plus longue est choisie.
SECTION Proxy
Les noms des serveurs proxys externes à utiliser.
ITEM proxy
[<URL-SPEC>] proxy = (host[:port])
Le nom d'hôte et le port du proxy.
ITEM auth-username
<URL-SPEC> auth-username = (string)
Le nom d'utilisateur à utiliser. La spécification <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> correspond ici au serveur proxy, et non à l'URL demandée.
ITEM auth-password
<URL-SPEC> auth-password = (string)
Le mot de passe à utiliser. La spécification <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> correspond ici au serveur proxy, et non à l'URL demandée.
ITEM ssl
[<URL-SPEC>] ssl = (host[:port])
Un serveur proxy utilisé pour les connexions SSL (Secure Socket Layer), par ex. https. Noter que seule la partie hôte  de <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> est vérifiée, et le reste doit être remplacé par des jokers '*'.
SECTION Alias
Une liste d'alias de remplacement de serveurs et chemins. Aussi pour les serveurs connus sous deux noms.
ITEM 
URL-SPECIFICATION = URL-SPECIFICATION
Toute requête correspondant à la première <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> sera remplacée par la seconde. La correspondance doit être exacte, il n'y a pas de patron, les arguments de l'URL sont ignorés.
SECTION Purge
La méthode pour déterminer les pages à purger, l'âge par défaut, l'âge spécifique à l'hôte des pages en jours, et la taille maximum du cache.
ITEM use-mtime
use-mtime = yes | no
La méthode utilisée pour décider des fichiers à purger, dernier accès (atime) ou dernière modification (ctime) (défaut=no).
ITEM max-size
max-size = (size)
La taille maximale du cache en méga-octets après la purge (défaut=0). Une valeur nulle signifie pas de limite. Si cette option et min-free sont toutes deux utilisées, la plus petite taille de cache est choisie. Cette option tient compte des URL jamais purgées pour mesurer la taille du cache, mais ne les supprime pas.
ITEM min-free
min-free = (size)
La taille minimale d'espace libre en méga-octets après la purge (défaut=0). Une taille nulle signifie aucune limite pour l'espace libre. Si cette option et l'option max-size sont toutes deux utilisées, la plus petite taille de cache est choisie. Cette option tient compte des URL jamais purgées pour le décompte, mais ne les supprime pas.
ITEM use-url
use-url = yes | no
Si 'yes' alors utilise l'URL pour décider de l'âge, sinon utilise seulement le protocole et l'hôte (défaut=no).
ITEM del-dontget
del-dontget = yes | no
Si 'yes' alors supprime les pages correspondant à la section DontGet (défaut=no).
ITEM del-dontcache
del-dontcache = yes | no
Si 'yes' alors supprime les pages correspondant à la section DontCache (défaut=no).
ITEM age
[<URL-SPEC>] age = (age)
L'âge maximum en jours dans le cache pour les URL correspondant à <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> (défaut=14). Un âge nul signifie ne pas garder, et négatif ne pas effacer. <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> correspond seulement au protocole et à l'hôte, sauf si l'option use-url est activée. Des temps plus longs peuvent être indiqués par les suffixes 'w', 'm' ou 'y' pour semaine, mois et année (par ex. 2w=14).
ITEM compress-age
[<URL-SPEC>] compress-age = (age)
L'âge maximum de stockage sans compression dans le cache pour les URL correspondant à <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> (défaut=-1). Requiert l'option de compilation zlib. L'âge a la même signification que pour l'option age.
TAIL
<h2><a name="WILDCARD">WILDCARD</a></h2> Une correspondance joker utilise le caractère '*' pour représenter un groupe quelconque de caractères. <p> Ceci est fondamentalement identique aux expressions en ligne de commande des shells DOS ou UNIX, excepté le fait que '*' correspond aussi au caractère '/'. <p> Par exemple,<dl><dt>*.gif<dd>correspond à foo.gif et bar.gif</dl><dl><dt>*.foo.com<dd>correspond à www.foo.com et ftp.foo.com</dl><dl><dt>/foo/*<dd>correspond à /foo/bar.html et /foo/bar/foobar.html</dl><h2><a name="URL-SPECIFICATION">URL-SPECIFICATION</a></h2> En indiquant un hôte, un protocole et un chemin dans de nombreuses sections, une <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> peut être utilisée, c'est un moyen de reconnaître une URL. <p> Pour cette explication, une URL peut être composée de cinq parties.<dl><dt>proto<dd>Le protocole utilisé (par ex. 'http', 'ftp')</dl><dl><dt>host<dd>Le nom du serveur (par ex. 'www.gedanken.demon.co.uk').</dl><dl><dt>port<dd>Le numéro de port sur le serveur (par ex. 80 pour le HTTP).</dl><dl><dt>path<dd>Le chemin sur le serveur (par ex. '/bar.html') ou un nom de répertoire (par ex. '/foo/').</dl><dl><dt>args<dd>Arguments de l'URL pour les scripts CGI, etc. (par ex. 'search=foo').</dl> <p> Par exemple, prenons la page d'accueil de WWWOFFLE, http://www.gedanken.demon.co.uk/wwwoffle/ <p> Le protocole est 'http', l'hôte 'www.gedanken.demon.co.uk', le port est celui par défaut (ici, 80), et le chemin est '/wwwoffle/'. <p> En général, on écrira (proto)://(host)[:(port)][/(path)[?(args)]] <p> où les crochets [] indiquent une partie optionnelle, et les parenthèses () un nom ou un numéro fourni par l'utilisateur. <p> Ci dessous quelques exemples de <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> :<dl><dt>*://*/*<dd>Tout protocole, tout hôte, tout port, tout chemin, tous arguments (ce qui revient à 'default').</dl><dl><dt>*://*/(path)<dd>Tout protocole, tout hôte, tout port, chemin précisé, tous arguments.</dl><dl><dt>*://*/*.(ext)<dd>Tout protocole, tout hôte, tout port, extension précisée, tous arguments.</dl><dl><dt>*://*/*?<dd>Tout protocole, tout hôte, tout port, tout chemin, pas d'arguments.</dl><dl><dt>*://*/(path)?*<dd>Tout protocole, tout hôte, tout port, chemin précisé, tous arguments.</dl><dl><dt>*://(host)/*<dd>Tout protocole, hôte précisé, tout port, tout chemin, tous arguments.</dl><dl><dt>(proto)://*/*<dd>Protocole précisé, tout hôte, tout port, tout chemin, tous arguments.</dl> <p> (proto)://(host)/*  Protocole et hôte précisés, tout port, tout chemin, tous arguments. <p> (proto)://(host):/* Protocole et hôte précisés, port par défaut, tout chemin, tous arguments. <p> *://(host):(port)/* Tout protocole, hôte et port précisés, tout chemin, tous arguments. <p> La correspondance des hôtes, chemins et arguments utilise les jokers décrits ci-dessus. <p> Dans quelques sections acceptant les <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a>, celles-ci peuvent être exclues en les préfixant d'un point d'exclamation '!'. Cela signifie que la comparaison renverra la valeur logique contraire à celle renvoyée sans le '!'. Si toutes les <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> sont exclues, et qu'on rajoute '*://*/*' à la fin, le sens de la section est renversé.
