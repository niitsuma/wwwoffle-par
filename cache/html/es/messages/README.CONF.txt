TITLE WWWOFFLE - World Wide Web Offline Explorer - Versión 2.6
HEAD
<h2><a name="Introduction">Introduction</a></h2> El fichero de configuración (wwwoffle.conf) especifica todos los parámetros que controlan el funcionamiento del servidor proxy.  El fichero está dividido en secciones que contienen una serie de parámetros que se describen más abajo.  El fichero CHANGES.CONF explica los cambios en el fichero de configuración desde las versiones anteriores. <p> El fichero está dividido en secciones, cada una de ellas puede estar vacía o puede contener una o más líneas con información sobre la configuración.  Las secciones son nombradas en el orden en el que aparecen en el fichero de configuración, pero este orden no es importante, <p> El formato general de cada sección es el mismo. El nombre de cada sección está sólo en una línea para marcar su comienzo. Los contenidos de la sección están contenidos entre dos líneas que contienen los caracteres '{' y '}' o '[' y ']'. Cuando se usan los caracteres '{' y '}' las líneas entre ellos contienen información de la configuración. Cuando se usan los caracteres '[' y ']' debe haber una sola línea no vacía que contenga en nombre del fichero ( en el mismo directorio ) que contenga la información de configuración. <p> Los comentarios se marcan con el carácter '#' al principio de la línea. También se permiten las líneas en blanco. Ambos casos son ignorados. <p> Las frases <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> (o <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> para acortar) y COMODÍN tienen diferentes significados en el fichero de configuración y son descritos al final.  Cualquier objeto encerrado entre '(' y ')' en las descripciones significa que es un parámetro suministrado por el usuario. Cualquiera encerrado entre '[' y ']' es opcional. El símbolo '|' se usa para mostrar una serie de alternativas.  Algunas de las opciones sólo de aplican específicamente a URLs. Esto se indica en la opción por una <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> encerrada entre '&lt;' y '&gt;'. La primera <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> que corresponda será usada. Si no se da ninguna <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> entonces se usarán todas las URLs
SECTION StartUp
Esta sección contiene los parámetros que se usan cuando el programa comienza, estos cambios serán ignorados si el programa es releído mientras el programa está ejecutándose.
ITEM http-port
http-port = (puerto)
Un entero especificando el puerto para el proxy HTTP  (por defecto=8080). 
ITEM wwwoffle-port
wwwoffle-port = (puerto)
Un entero especificando el puerto para el control de las conexiones  de WWWOFFLE (por defecto=8081). 
ITEM spool-dir
spool-dir = (dir)
El nombre del directorio almacén (por defecto=/var/spool/wwwoffle). 
ITEM run-uid
run-uid = (usuario) | (uid) 
El nombre o número de usuario con el que ejecutar el servidor  wwwoffled (por defecto=ninguno).  Esta opción no se puede aplicar a win32. Sólo funciona en UNIX si el servidor es inicializado por root
ITEM run-gid
run-gid = (grupo) | (gid) 
El nombre o número del grupo con el que ejecutar el servidor  wwwoffled (por defecto=ninguno). Esta opción no se puede aplicar a win32. Sólo funciona en UNIX si el servidor es inicializado por root
ITEM use-syslog
use-syslog = yes | no 
Usar syslog para guardar los mensajes (por defecto=yes).
ITEM password
password = (palabra)
La contraseña usada para la autentificación de las páginas de  control, para borrar páginas almacenadas, etc... (por defecto=ninguna). Para que la contraseña este segura el fichero de configuración debe estar asegurado para que sólo personal autorizado tenga acceso a él.
ITEM max-servers
max-servers = (entero)
El número máximo de procesos servidores que se ejecutarán para descarga en modo conectado y descarga automática (por defecto=8).
ITEM max-fetch-servers
max-fetch-servers = (entero)
El número máximo de procesos servidores que se ejecutan para descargar  páginas que fueron marcadas en modo desconectado (por defecto=4). Este valor debe ser menor que max-servers o no será capaz de usar WWWOFFLE interactivamente mientras se descargar páginas.
SECTION Options
Opciones que controlan como funciona el programa.
ITEM log-level
log-level = debug | info | important | warning | fatal
Guarda los mensajes con esta o más alta prioridad  (por defecto=important).
ITEM socket-timeout
socket-timeout = (tiempo)
El tiempo en segundos que WWWOFFLE esperará en una conexión por socket antes de abandonar (por defecto=120).
ITEM dns-timeout
dns-timeout = (tiempo)
El tiempo en segundos que WWWOFFLE esperará en una conexión con el DNS (Servidor de Nombres de Dominios) antes de abandonar (por defecto=60).
ITEM connect-timeout
connect-timeout = (tiempo)
El tiempo en segundos que WWWOFFLE esperará a que se establezca  una conexión por socket antes de abandonar (por defecto=30).
ITEM connect-retry
connect-retry = yes | no
Si una conexión no se puede establecer WWWOFFLE lo intentará tras esperar un tiempo prudencial (por defecto=no).
ITEM request-changed
request-changed = (tiempo) 
Mientras se está conectado las páginas solo serán descargadas si la  versión almacenada es más vieja que la especificada. tiempo en  segundos (por defecto=600).
ITEM request-changed-once
request-changed-once = yes | no 
Mientras se está conectado las páginas solo serán descargadas si la  versión almacenada no ha sido ya recogida una vez en esta sesión  (por defecto=yes).
ITEM pragma-no-cache
pragma-no-cache = yes | no 
Pedir otra copia de la página si la petición tiene  'Pragma: no-cache' (por defecto=yes).
ITEM confirm-requests
confirm-requests = yes | no 
Devolver una página que requiere la confirmación del usuario en  vez de grabar automáticamente las peticiones hechas desconectado  (por defecto=no).
ITEM socket-timeout
socket-timeout = <tiempo> 
El tiempo en segundos que WWWOFFLE esperará los datos hasta que se  deje una conexión (por defecto=120).
ITEM connect-retry
connect-retry = yes | no 
Si una conexión a un servidor remoto no se ha podido realizar  realizarla tras un corto período de tiempo. (por defecto=no).
ITEM ssl-allow-port
ssl-allow-port = (entero) 
Un número de puerto que tiene permitida la conexión mediante  conexiones de Capa de Conexión Segura (SSL), p. e. https. Esta opción debería ser 443 para permitir https. También puede haber más de una entrada de puertos ssl si se necesita.
ITEM dir-perm
dir-perm = (entero_octal)
Los permisos que se usarán para crear los directorios almacén (por defecto=0755).  Estas opciones invalidan los valores de umask del usuario y deben estar en octal empezando con un cero (0).
ITEM file-perm
file-perm = (entero octal)
Los permisos que se usarán para crear los ficheros almacén (por defecto=0644).  Estas opciones invalidan los valores de umask del usuario y deben estar en octal empezando con un cero (0).
ITEM run-online
run-online = (fichero)
El nombre de un programa que se ejecute cuando se cambie a modo  conectado.  (por defecto=ninguno).  El programa se inicia con un sólo parámetro con el modo actual, "online" (conectado).
ITEM run-offline
run-offline = (fichero)
El nombre de un programa que se ejecute cuando se cambie a modo desconectado. (por defecto=ninguno).   El programa se inicia con un sólo parámetro con el modo actual, "offline" (desconectado).
ITEM run-autodial
run-autodial = (fichero)
El nombre de un programa que se ejecute cuando se cambie a modo Auto llamada.  (por defecto=ninguno).  El programa se inicia con un  sólo parámetro con el modo actual, "autodial" (auto-llamada).
ITEM run-fetch
run-fetch = (fichero)
El nombre de un programa que se ejecute cuando se arranque o se pare el modo de recogida (por defecto=ninguno).  El programa se inicia  con dos parámetros, el primero es la palabra "fetch" (recogida) y la segunda es "start" (comienzo) o "stop" (parada).
ITEM lock-files
lock-files = yes | no
Activa el uso de ficheros de bloqueo para impedir que más de un proceso WWWOFFLE baje la misma URL al mismo tiempo (por defecto=no).
SECTION OnlineOptions
Opciones que controlan como se comporta WWWOFFLE cuando está conectado.
ITEM request-changed
[<URL-SPEC>] request-changed = (tiempo)
Mientras está conectado las páginas sólo serán recogidas si la versión de la caché es más antigua que el tiempo especificado en segundos (por defecto=600).  Si escribe un valor negativo indicará  que las páginas almacenadas se usarán siempre mientras está conectado.  Se pueden especificar tiempos más largos con los sufijos 'm', 'h', 'd' o 'w' para minutos, horas, días o semanas  (p.e. 10m=600)
ITEM request-changed-once
[<URL-SPEC>] request-changed-once = yes | no
Mientas está conectado las páginas sólo serán recogidas si la versión almacenada no ha sido ya recogida en esta sesión  (por defecto=yes).  Esta opción toma preferencia sobre la opción request-changed.
ITEM request-expired
[<URL-SPEC>] request-expired = yes | no
Mientras está conectado las páginas que han expirado serán pedidas de nuevo (por defecto=no).  Esta opción toma preferencia sobre las  opciones request-changed y request-changed-once.
ITEM request-no-cache
[<URL-SPEC>] request-no-cache = yes | no
Mientras está conectado las páginas que no se almacenarán serán pedidas de nuevo (por defecto=no).  Esta opción toma preferencia sobre las opciones request-changed y request-changed-one.
ITEM try-without-password
[<URL-SPEC>] try-without-password = yes | no
Si se realiza una petición de una página que contiene un usuario y una contraseña se realizará la petición sin especificar el usuario y contraseña.  Esto permite que las páginas sin contraseña redirijan al navegador a la versión con contraseña de la página.
ITEM intr-download-keep
[<URL-SPEC>] intr-download-keep = yes | no
Si el navegador cierra la conexión mientras está conectado se guardará la página incompleta (por defecto=no).
ITEM intr-download-size
[<URL-SPEC>] intr-download-size = (entero)
Si el navegador cierra la conexión mientras está conectado se continuará la descarga si es menor que el tamaño en KB (por defecto=1).
ITEM intr-download-percent
[<URL-SPEC>] intr-download-percent = (entero)
Si el navegador cierra la conexión mientras está conectado se continuará la descarga si se ha completado más que el porcentaje especificado (por defecto=80).
ITEM timeout-download-keep
[<URL-SPEC>] timeout-download-keep = yes | no
Si la conexión del servidor agota el tiempo de espera mientras descarga una página, esta página incompleta se guardará. (por defecto=no).
SECTION OfflineOptions
Opciones que controlan como se comporta WWWOFLE cuando está desconectado.
ITEM pragma-no-cache
[<URL-SPEC>] pragma-no-cache = yes | no
Especifica si se ha de pedir una nueva copia de una página si la petición del navegador tenía la cabecera 'Pragma: no-cache' (por defecto=yes).  Esta opción debería ponerse a 'no' si cuando esté desconectado todas las páginas son vueltas a pedir por un  navegador 'roto'
ITEM confirm-requests
[<URL-SPEC>] confirm-requests = yes | no
Especifica si volver a una página requiere confirmación del usuario en vez de grabar la petición automáticamente cuando se está  desconectado. (por defecto=no).
ITEM dont-request
[<URL-SPEC>] dont-request = yes | no
No pedir ninguna URL que corresponda con estas mientras se está desconectado. (por defecto=no).
SECTION FetchOptions
La opciones que controlan la recogida de páginas que se pidieron estando  desconectado.
ITEM stylesheets
stylesheets = yes | no
Si se quieren descargar Hojas de Estilo. (por defecto=no).
ITEM images
images = yes | no
Si se quieren descargar Imágenes. (por defecto=no).
ITEM frames
frames = yes | no
Si se quieren descargar Marcos. (por defecto=no).
ITEM scripts
scripts = yes | no
Si se quieren descargar guiones (p.e. Javascript). (por defecto=no).
ITEM objects
objects = yes | no 
Si se quieren descargar objetos (p.e. Ficheros de clases Java). (por defecto=no).
SECTION IndexOptions

ITEM no-lasttime-index
no-lasttime-index = yes | no 
Deshabilita la creación de los índices Última Vez/Vez Anterior (por defecto=no).
ITEM list-outgoing
<URL-SPEC> list-outgoing = yes | no
Elije si la URL se mostrará en el directorio de peticiones salientes. (por defecto=yes).
ITEM list-latest
<URL-SPEC> list-latest = yes | no
Elije si la URL se mostrará en los índices Última Vez/Vez Anterior y Última Salida/Salida Anterior (por defecto=yes).
ITEM list-monitor
<URL-SPEC> list-monitor = yes | no
elije si la URL se mostrará en el índice de páginas monitorizadas. (por defecto=yes).
ITEM list-host
<URL-SPEC> list-host = yes | no
Elije si la URL se mostrará en los índices de servidores  (por defecto=yes).
ITEM list-any
<URL-SPEC> list-any = yes | no
Elije si la URL se mostrará en alguno de los índices  (por defecto=yes).
SECTION ModifyHTML
Opciones que controlan como el HTML almacenado en la caché es modificado.
ITEM enable-modify-html
[<URL-SPEC>] enable-modify-html = yes | no  
Activar las modificaciones en esta sección (por defecto=no). Con esta opción desactivada las demás opciones sobre HTML no tendrán ningún efecto. Con esta opción activada hay una pequeña penalización en velocidad.
ITEM add-cache-info
[<URL-SPEC>] add-cache-info = yes | no 
Añadir al final de todas las  páginas almacenadas la fecha en la  que esa página fue recogida y algunos botones (por defecto=no).
ITEM anchor-cached-begin
[<URL-SPEC>] anchor-cached-begin = (HTML code) | 
Los enlaces que son almacenados tienen el código HTML especificado  insertado al principio (por defecto="").
ITEM anchor-cached-end
[<URL-SPEC>] anchor-cached-end = (HTML code) | 
Los enlaces que son almacenados tienen el código HTML especificado  insertado al final (por defecto="").
ITEM anchor-requested-begin
[<URL-SPEC>] anchor-requested-begin = (HTML code) | 
Los enlaces que han sido pedidos para descarga tienen el código HTML  especificado insertado al principio.(por defecto="").
ITEM anchor-requested-end
[<URL-SPEC>] anchor-requested-end = (HTML code) | 
Los enlaces que han sido pedidos para descarga tienen el código HTML  especificado insertado al final.(por defecto="").
ITEM anchor-not-cached-begin
[<URL-SPEC>] anchor-not-cached-begin = (HTML code) |·
Los enlaces de las páginas que no se guardarán en la caché o no serán pedidas tienen el siguiente código insertado al principio.  (por defecto="")
ITEM anchor-not-cached-end
[<URL-SPEC>] anchor-not-cached-end = (HTML code) |·
Los enlaces de las páginas que no se guardarán en la caché o no serán pedidas tienen el siguiente código insertado al final.  (por defecto="")
ITEM disable-script
[<URL-SPEC>] disable-script = yes | no
Quita todos los guiones (por defecto=no).
ITEM disable-blink
[<URL-SPEC>] disable-blink = yes | no
Quita la etiqueta de parpadeo (&lt;blink&gt;) (por defecto=no).
ITEM disable-meta-refresh
[<URL-SPEC>] disable-meta-refresh = yes | no
Quita cualquier etiqueta de tipo "meta" en la cabecera HTML que redirija al navegador a recargar la página tras un espacio de tiempo (por defecto=no).
ITEM demoronise-ms-chars
[<URL-SPEC>] demoronise-ms-chars = yes | no
Reemplaza los caracteres extraños que alguna aplicaciones de Microsoft ponen en el HTML con caracteres equivalentes que la mayoría  de los navegadores pueden mostrar (por defecto=no).  La idea viene del guión Perl de dominio publico "Demoroniser" (Desidiotizador)
ITEM disable-animated-gif
[<URL-SPEC>] disable-animated-gif = yes | no
Desactiva la animación de los ficheros GIF (por defecto=no).
SECTION LocalHost
Una lista de huéspedes que el servidor ejecutando wwwoffled puede ser conocido.  Esto es así para que el proxy no necesite contactar el mismo para conseguir las páginas locales del servidor en el caso de que tenga diferentes nombres.
ITEM 
(servidor) 
Un nombre de servidor o dirección IP que en conexión con el número  de puerto (en la sección Startup) especifica el servidor HTTP WWWOFFLE. Los nombres de servidores tienen que coincidir exactamente, no use comodines..  El primer nombre del servidor tendrá diferentes usos por lo que debería ser un nombre que funcione desde todos los clientes de la red.  Ninguno de los servidores nombrados aquí serán almacenados o recogidos a través del proxy.
SECTION LocalNet
Una lista de servidores que tienen accesible el servicio web incluso cuando está desconectado y WWWOFFLE no debe almacenar porque están en una red local.
ITEM 
(servidor)
Un nombre de servidor o dirección IP que está siempre disponible y WWWOFFLE no almacenará. La correspondencia de nombres de servidores usa comodines.  Puede excluir un servidor añadiendo el símbolo '!' al principio del nombre. También se necesitan todos los posibles alias y direcciones IP del servidor.  Se asumirá que todas las  entradas que escriba aquí estarán accesibles cuando esté desconectado. Ninguno de estos servidores será almacenados o recogidos a través del proxy.
SECTION AllowedConnectHosts
Una lista de los huéspedes que tienen permitida la conexión al servidor.
ITEM 
(huésped)
Un huésped o dirección IP que tiene permitida la conexión al servidor. La correspondencia de nombres de servidores usa comodines.  Puede excluir un huésped añadiendo el símbolo '!' al principio del nombre.  También se necesitan todos los posibles alias y direcciones IP  del huésped.  Todos los nombres de servidor nombrados en las sección LocalHost también tienen permitida la conexión.
SECTION AllowedConnectUsers
Una lista de los usuarios y sus contraseñas, que tienen permitida la conexión  con el servidor.
ITEM 
(usuario):(contraseña)
El nombre de usuario y la contraseña de los usuarios que tienen  permitida la conexión con el servidor. Si la sección se deja vacía  no se realiza autentificación por contraseña.  El nombre de usuario  y contraseñas se almacenan en formato de texto plano.  Esta opción  requiere el uso de navegadores que soporten el estándar HTTP/1.1 de  autentificación frente al proxy.
SECTION DontCache
Una lista de URLs que WWWOFFLE no almacenará.
ITEM 
[!]URL-SPECIFICATION
No almacenar ninguna URL que coincida con esto.  La <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> puede ser negada para permitir que las coincidencias sean almacenadas.  Las URLs no se pedirán si está  desconectado
SECTION DontGet
Una lista de URLs que WWWOFFLE no debe descargar (porque solo contienen  publicidad o basura, por ejemplo).
ITEM 
[!]URL-SPECIFICATION
No descargar ninguna URL que coincida con esto.  La <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> puede ser negada para permitir descargar las coincidencias.
ITEM replacement
[<URL-SPEC>] replacement = (URL)
La URL a usar para reemplazar cualquier URL que coincida con la <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> en vez de usar el mensaje de error estándar (por defecto=ninguna).  Se sugiere como reemplazo la URL /local/dontget/replacement.gif (un gif transparente de 1x1 pixel).
ITEM get-recursive
<URL-SPEC> get-recursive = yes | no
Elija si las URLs que coincidan serán recogidas en una recogida recursiva (por defecto=yes).
ITEM location-error
<URL-SPEC> location-error = yes | no
Cuando la respuesta de la URL contiene una cabecera 'Location' () que redirige a una página especificada en esta sección la respuesta es modificada para mostrar un mensaje de error en vez de la página  (por defecto=no).  Esto parará la redirección de los proxies de los ISPs a anuncios si las URLs de los anuncios están en esta sección.
SECTION CensorHeader
Una lista de cabeceras HTTP que se deben quitar de la petición hecha al  servidor web y de las respuestas que vuelvan de vuelta.
ITEM 
[<URL-SPEC>] (cabecera) = yes | no | (cadena)
Una campo de la cabecera, (p.e. From, Cookie, Set-Cookie User-Agent)  y la cadena para reemplazar el valor de la cabecera (por defecto=no). La cabecera distingue mayúsculas de minúsculas y no contiene un ':' al final.  El valor "no" significa que la cabecera no se modificará. Los valores "yes" o no poner una cadena pueden usarse para eliminar  la cabecera. También puede usar una cadena para reemplazar la cabecera.  Esta opción sólo reemplaza cabeceras si las encuentra, no  añade ninguna nueva.
ITEM referer-self
[<URL-SPEC>] referer-self = yes | no 
Pone la cabecera Referer al mismo valor que la URL que se ha pedido (por defecto = no).
ITEM referer-self-dir
[<URL-SPEC>] referer-self-dir = yes | no 
Pone la cabecera Referer al nombre del directorio en el que se encuentra la URL que se ha pedido (por defecto = no).  Esta opción toma preferencia sobre referer-self.
SECTION FTPOptions
Opciones a usar cuando se descargan ficheros usando el protocolo ftp.
ITEM anon-username
anon-username = (cadena)
El nombre de usuario a usar para ftp anónimo (por defecto=anonymous).
ITEM anon-password
anon-password = (cadena)
La contraseña a usar para ftp anónimo  (por defecto=se determina en tiempo de ejecución).   Si está usando un cortafuegos puede contener valores no válidos para el servidor FTP y debe ser puesto a un valor diferente.
ITEM auth-username
[<URL-SPEC>] auth-username = (cadena)
Un nombre de usuario a usar en un servidor en vez de usuario anónimo  por defecto.
ITEM auth-password
[<URL-SPEC>] auth-password = (cadena)
La contraseña a usar en un servidor en vez de la contraseña anónima por defecto.
SECTION MIMETypes
Tipos MIME a usar cuando se sirven ficheros que no han sido recogidos usando HTTP o para ficheros en el servidor web incorporado.
ITEM default
default = (tipo-mime)/(subtipo)
El tipo MIME por defecto. (por defecto=text/plain).
ITEM 
.(ext-fichero) = (tipo-mime)/(subtipo) 
El tipo MIME asociado con la extensión de un fichero. El punto (.) debe ser incluido en la extensión de fichero. Si coincide más de una extensión se usará la más larga.
SECTION Proxy
Contiene los nombre de los proxies HTTP (u otros) a usar externamente a la  máquina local con WWWOFFLE.
ITEM proxy
[<URL-SPEC>] proxy = (servidor[:puerto])
El huésped y el puerto a usar como proxy por defecto.
ITEM auth-username
<URL-SPEC> auth-username = (cadena)
El nombre de usuario a usar en un servidor proxy para autentificar  WWWOFFLE frente a él. La <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> en este caso se refiere al proxy, no a la URL que se está recogiendo.
ITEM auth-password
<URL-SPEC> auth-password = (cadena)
La contraseña a usar en el servidor proxy para autentificar WWWOFFLE frente a él. La <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> en este caso se refiere al proxy y no a la URL que se está recogiendo.
ITEM ssl
[<URL-SPEC>] ssl = (servidor[:puerto])
Un servidor proxy que se debe usar para conexiones de Capa de Conexión Segura (SSL) p.e. https. Note que de la <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> sólo se comprueba el nombre de servidor. Las otras partes deben ser comodines (*).
SECTION Alias
Una lista de alias que son usados para reemplazar el nombre del servidor y el  camino con otro nombre de servidor y camino. También para servidores que son conocidos por dos nombres.
ITEM 
URL-SPECIFICATION = URL-SPECIFICATION
Cualquier petición que coincida con la primera <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> es  reemplazada por la segunda <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a>. Las <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> deben coincidir exactamente. No es una correspondencia por comodines. Los argumentos de la URL son ignorados.
SECTION Purge
El método que determina que páginas eliminar, la edad por defecto, la edad de  las páginas de un servidor determinado en días y el tamaño máximo de la caché.
ITEM use-mtime
use-mtime = yes | no 
El método a usar para decidir que ficheros purgar, el tiempo de  acceso (atime) o el tiempo de última modificación (mtime) (por defecto=no).
ITEM max-size
max-size = (tamaño)
El tamaño máximo de la caché en MB después de purgar (por defecto=0). Un tamaño máximo de caché de 0 significa que no hay límite en tamaño.  Si se usa esta opción y la opción min-free se elegirá el menor tamaño de caché.  Esta opción, al calcular el tamaño de la caché, tiene en cuenta las URLs que nunca son purgadas pero no las  eliminará.
ITEM min-free
min-free = (size)
El espacio mínimo libre en disco en MB después de purgar  (por defecto=0).  Un tamaño mínimo de disco de 0 significa que no hay límite de espacio libre. Si se usa esta opción y la opción max-free se elegirá el menor tamaño de caché.  Esta opción tiene en cuenta las URLs que nunca son purgadas pero no las eliminará.
ITEM use-url
use-url = yes | no 
si es verdad se usa la URL para decidir en la edad de purga, si  no se usa el servidor y el protocolo. (por defecto=no).
ITEM del-dontget
del-dontget = yes | no
Si es verdad se borran las URLs que coinciden con las entradas de la sección DontGet (por defecto=no).
ITEM del-dontcache
del-dontcache = yes | no
Si es verdad se borran las URLs que coinciden con las entradas  de la sección DontCache (por defecto=no).
ITEM age
[<URL-SPEC>] age = (edad)
La edad máxima en la caché para las URLs que coinciden con esto  (por defecto=14).  Una edad de cero significa no guardar. Un valor  negativo no borrar.  La <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> corresponde sólo con el protocolo y servidor a menos que se haya especificado como verdadera la opción use-url.  Se pueden especificar tiempos más largo con los sufijos 'w', 'm' o 'y' para semanas, meses o años (p. e. 2w=14).
SECTION COMODINES
Un comodín es usar el carácter '*' para representar cualquier grupo de  caracteres. <p> Es básicamente la misma expresión de correspondencia de ficheros de la línea de comandos de DOS o la shell de UNIX, excepto que el carácter '*' puede aceptar el carácter '/'. <p> Por ejemplo <p> *.gif      corresponde con  foo.gif y bar.gif *.foo.com  corresponde con  www.foo.com y ftp.foo.com /foo/*     corresponde con  /foo/bar.html y /foo/bar/foobar.html
SECTION URL-SPECIFICATION
Cuando se especifica un servidor, un protocolo y una ruta, en muchas secciones se puede usar una <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a>, que es una forma de reconocer una URL <p> Para esta explicación consideramos que una URL puede constar de cinco partes. <p> proto           El protocolo que usa (p.e. 'http', 'ftp') servidor        El nombre de servidor (p.e. 'www.gedanken.demon.co.uk'). puerto          El número de puerto en el servidor (p.e. por defecto 80 para HTTP). ruta            La ruta en el servidor (p.e. '/bar.html') o un nombre de  directorio (p.e. '/foo/'). argumentos      Argumentos opciones de la URL usados por guiones CGIs etc... (p. e.) 'search=foo'). <p> Por ejemplo, en la página de WWWOFFLE: http://www.gedanken.demon.co.uk/wwwoffle/ El protocolo es 'http', el servidor es 'www.gedanken.demon.co.uk', el puerto es el predeterminado (en este caso 80), y la ruta es '/wwwoffle/'. <p> En general se escribe como
ITEM 
(proto)://(servidor)[:(puerto)]/(ruta)[?(argumentos)]
Donde [] indica una característica opcional, y () indica un nombre o número proporcionado por el usuario. <p> Alguna opciones comunes de <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> son las siguientes: <p> *://*/*             Cualquier protocolo, cualquier servidor, cualquier ruta (Es lo mismo que decir 'el predeterminado'). <p> *://*/(ruta)        Cualquier protocolo, cualquier servidor,  cualquier puerto, una ruta, cualquier argumento. <p> *://*/*.(ext)       Cualquier protocolo, cualquier servidor, cualquier puerto, una ruta, cualquier argumento. <p> *://*/*?            Cualquier protocolo, cualquier servidor, cualquier ruta, Ningún argumento. <p> *://(servidor)/*    Cualquier protocolo, un servidor, cualquier puerto, cualquier ruta, cualquier argumento.
ITEM 
(proto)://*/*       Un protocolo, cualquier servidor, cualquier puerto,
cualquier ruta, cualquier argumento.
ITEM 
(proto)://(servidor)/* Un protocolo, un servidor, cualquier puerto, 
cualquier ruta, cualquier argumento
ITEM 
(proto)://(servidor):/* Un protocolo, un servidor, puerto predeterminado,
cualquier ruta, cualquier argumento. <p> *://(servidor):(puerto)/* Cualquier protocolo, un servidor, un puerto,  cualquier ruta, cualquier argumento. <p> La correspondencia del servidor y la ruta usa los comodines descritos arriba. <p> En algunas secciones se acepta que la <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> este negada añadiendo el carácter '!' al comienzo. Esto significa que la comparación entre la URL y la <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> devolverá el valor lógico opuesto al que devolvería sin el carácter '!'. Si todas las ESPECIFICACIONES-URL  de la sección están negadas y se añade '*://*/*' al final, el sentido de la  sección entera será negado.
