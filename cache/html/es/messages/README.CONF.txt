TITLE WWWOFFLE - World Wide Web Offline Explorer - Versi�n 2.6
HEAD
<h2><a name="Introduction">Introduction</a></h2> El fichero de configuraci�n (wwwoffle.conf) especifica todos los par�metros que controlan el funcionamiento del servidor proxy.  El fichero est� dividido en secciones que contienen una serie de par�metros que se describen m�s abajo.  El fichero CHANGES.CONF explica los cambios en el fichero de configuraci�n desde las versiones anteriores. <p> El fichero est� dividido en secciones, cada una de ellas puede estar vac�a o puede contener una o m�s l�neas con informaci�n sobre la configuraci�n.  Las secciones son nombradas en el orden en el que aparecen en el fichero de configuraci�n, pero este orden no es importante, <p> El formato general de cada secci�n es el mismo. El nombre de cada secci�n est� s�lo en una l�nea para marcar su comienzo. Los contenidos de la secci�n est�n contenidos entre dos l�neas que contienen los caracteres '{' y '}' o '[' y ']'. Cuando se usan los caracteres '{' y '}' las l�neas entre ellos contienen informaci�n de la configuraci�n. Cuando se usan los caracteres '[' y ']' debe haber una sola l�nea no vac�a que contenga en nombre del fichero ( en el mismo directorio ) que contenga la informaci�n de configuraci�n. <p> Los comentarios se marcan con el car�cter '#' al principio de la l�nea. Tambi�n se permiten las l�neas en blanco. Ambos casos son ignorados. <p> Las frases <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> (o <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> para acortar) y COMOD�N tienen diferentes significados en el fichero de configuraci�n y son descritos al final.  Cualquier objeto encerrado entre '(' y ')' en las descripciones significa que es un par�metro suministrado por el usuario. Cualquiera encerrado entre '[' y ']' es opcional. El s�mbolo '|' se usa para mostrar una serie de alternativas.  Algunas de las opciones s�lo de aplican espec�ficamente a URLs. Esto se indica en la opci�n por una <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> encerrada entre '&lt;' y '&gt;'. La primera <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> que corresponda ser� usada. Si no se da ninguna <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> entonces se usar�n todas las URLs
SECTION StartUp
Esta secci�n contiene los par�metros que se usan cuando el programa comienza, estos cambios ser�n ignorados si el programa es rele�do mientras el programa est� ejecut�ndose.
ITEM http-port
http-port = (puerto)
Un entero especificando el puerto para el proxy HTTP  (por defecto=8080). 
ITEM wwwoffle-port
wwwoffle-port = (puerto)
Un entero especificando el puerto para el control de las conexiones  de WWWOFFLE (por defecto=8081). 
ITEM spool-dir
spool-dir = (dir)
El nombre del directorio almac�n (por defecto=/var/spool/wwwoffle). 
ITEM run-uid
run-uid = (usuario) | (uid) 
El nombre o n�mero de usuario con el que ejecutar el servidor  wwwoffled (por defecto=ninguno).  Esta opci�n no se puede aplicar a win32. S�lo funciona en UNIX si el servidor es inicializado por root
ITEM run-gid
run-gid = (grupo) | (gid) 
El nombre o n�mero del grupo con el que ejecutar el servidor  wwwoffled (por defecto=ninguno). Esta opci�n no se puede aplicar a win32. S�lo funciona en UNIX si el servidor es inicializado por root
ITEM use-syslog
use-syslog = yes | no 
Usar syslog para guardar los mensajes (por defecto=yes).
ITEM password
password = (palabra)
La contrase�a usada para la autentificaci�n de las p�ginas de  control, para borrar p�ginas almacenadas, etc... (por defecto=ninguna). Para que la contrase�a este segura el fichero de configuraci�n debe estar asegurado para que s�lo personal autorizado tenga acceso a �l.
ITEM max-servers
max-servers = (entero)
El n�mero m�ximo de procesos servidores que se ejecutar�n para descarga en modo conectado y descarga autom�tica (por defecto=8).
ITEM max-fetch-servers
max-fetch-servers = (entero)
El n�mero m�ximo de procesos servidores que se ejecutan para descargar  p�ginas que fueron marcadas en modo desconectado (por defecto=4). Este valor debe ser menor que max-servers o no ser� capaz de usar WWWOFFLE interactivamente mientras se descargar p�ginas.
SECTION Options
Opciones que controlan como funciona el programa.
ITEM log-level
log-level = debug | info | important | warning | fatal
Guarda los mensajes con esta o m�s alta prioridad  (por defecto=important).
ITEM socket-timeout
socket-timeout = (tiempo)
El tiempo en segundos que WWWOFFLE esperar� en una conexi�n por socket antes de abandonar (por defecto=120).
ITEM dns-timeout
dns-timeout = (tiempo)
El tiempo en segundos que WWWOFFLE esperar� en una conexi�n con el DNS (Servidor de Nombres de Dominios) antes de abandonar (por defecto=60).
ITEM connect-timeout
connect-timeout = (tiempo)
El tiempo en segundos que WWWOFFLE esperar� a que se establezca  una conexi�n por socket antes de abandonar (por defecto=30).
ITEM connect-retry
connect-retry = yes | no
Si una conexi�n no se puede establecer WWWOFFLE lo intentar� tras esperar un tiempo prudencial (por defecto=no).
ITEM request-changed
request-changed = (tiempo) 
Mientras se est� conectado las p�ginas solo ser�n descargadas si la  versi�n almacenada es m�s vieja que la especificada. tiempo en  segundos (por defecto=600).
ITEM request-changed-once
request-changed-once = yes | no 
Mientras se est� conectado las p�ginas solo ser�n descargadas si la  versi�n almacenada no ha sido ya recogida una vez en esta sesi�n  (por defecto=yes).
ITEM pragma-no-cache
pragma-no-cache = yes | no 
Pedir otra copia de la p�gina si la petici�n tiene  'Pragma: no-cache' (por defecto=yes).
ITEM confirm-requests
confirm-requests = yes | no 
Devolver una p�gina que requiere la confirmaci�n del usuario en  vez de grabar autom�ticamente las peticiones hechas desconectado  (por defecto=no).
ITEM socket-timeout
socket-timeout = <tiempo> 
El tiempo en segundos que WWWOFFLE esperar� los datos hasta que se  deje una conexi�n (por defecto=120).
ITEM connect-retry
connect-retry = yes | no 
Si una conexi�n a un servidor remoto no se ha podido realizar  realizarla tras un corto per�odo de tiempo. (por defecto=no).
ITEM ssl-allow-port
ssl-allow-port = (entero) 
Un n�mero de puerto que tiene permitida la conexi�n mediante  conexiones de Capa de Conexi�n Segura (SSL), p. e. https. Esta opci�n deber�a ser 443 para permitir https. Tambi�n puede haber m�s de una entrada de puertos ssl si se necesita.
ITEM dir-perm
dir-perm = (entero_octal)
Los permisos que se usar�n para crear los directorios almac�n (por defecto=0755).  Estas opciones invalidan los valores de umask del usuario y deben estar en octal empezando con un cero (0).
ITEM file-perm
file-perm = (entero octal)
Los permisos que se usar�n para crear los ficheros almac�n (por defecto=0644).  Estas opciones invalidan los valores de umask del usuario y deben estar en octal empezando con un cero (0).
ITEM run-online
run-online = (fichero)
El nombre de un programa que se ejecute cuando se cambie a modo  conectado.  (por defecto=ninguno).  El programa se inicia con un s�lo par�metro con el modo actual, "online" (conectado).
ITEM run-offline
run-offline = (fichero)
El nombre de un programa que se ejecute cuando se cambie a modo desconectado. (por defecto=ninguno).   El programa se inicia con un s�lo par�metro con el modo actual, "offline" (desconectado).
ITEM run-autodial
run-autodial = (fichero)
El nombre de un programa que se ejecute cuando se cambie a modo Auto llamada.  (por defecto=ninguno).  El programa se inicia con un  s�lo par�metro con el modo actual, "autodial" (auto-llamada).
ITEM run-fetch
run-fetch = (fichero)
El nombre de un programa que se ejecute cuando se arranque o se pare el modo de recogida (por defecto=ninguno).  El programa se inicia  con dos par�metros, el primero es la palabra "fetch" (recogida) y la segunda es "start" (comienzo) o "stop" (parada).
ITEM lock-files
lock-files = yes | no
Activa el uso de ficheros de bloqueo para impedir que m�s de un proceso WWWOFFLE baje la misma URL al mismo tiempo (por defecto=no).
SECTION OnlineOptions
Opciones que controlan como se comporta WWWOFFLE cuando est� conectado.
ITEM request-changed
[<URL-SPEC>] request-changed = (tiempo)
Mientras est� conectado las p�ginas s�lo ser�n recogidas si la versi�n de la cach� es m�s antigua que el tiempo especificado en segundos (por defecto=600).  Si escribe un valor negativo indicar�  que las p�ginas almacenadas se usar�n siempre mientras est� conectado.  Se pueden especificar tiempos m�s largos con los sufijos 'm', 'h', 'd' o 'w' para minutos, horas, d�as o semanas  (p.e. 10m=600)
ITEM request-changed-once
[<URL-SPEC>] request-changed-once = yes | no
Mientas est� conectado las p�ginas s�lo ser�n recogidas si la versi�n almacenada no ha sido ya recogida en esta sesi�n  (por defecto=yes).  Esta opci�n toma preferencia sobre la opci�n request-changed.
ITEM request-expired
[<URL-SPEC>] request-expired = yes | no
Mientras est� conectado las p�ginas que han expirado ser�n pedidas de nuevo (por defecto=no).  Esta opci�n toma preferencia sobre las  opciones request-changed y request-changed-once.
ITEM request-no-cache
[<URL-SPEC>] request-no-cache = yes | no
Mientras est� conectado las p�ginas que no se almacenar�n ser�n pedidas de nuevo (por defecto=no).  Esta opci�n toma preferencia sobre las opciones request-changed y request-changed-one.
ITEM try-without-password
[<URL-SPEC>] try-without-password = yes | no
Si se realiza una petici�n de una p�gina que contiene un usuario y una contrase�a se realizar� la petici�n sin especificar el usuario y contrase�a.  Esto permite que las p�ginas sin contrase�a redirijan al navegador a la versi�n con contrase�a de la p�gina.
ITEM intr-download-keep
[<URL-SPEC>] intr-download-keep = yes | no
Si el navegador cierra la conexi�n mientras est� conectado se guardar� la p�gina incompleta (por defecto=no).
ITEM intr-download-size
[<URL-SPEC>] intr-download-size = (entero)
Si el navegador cierra la conexi�n mientras est� conectado se continuar� la descarga si es menor que el tama�o en KB (por defecto=1).
ITEM intr-download-percent
[<URL-SPEC>] intr-download-percent = (entero)
Si el navegador cierra la conexi�n mientras est� conectado se continuar� la descarga si se ha completado m�s que el porcentaje especificado (por defecto=80).
ITEM timeout-download-keep
[<URL-SPEC>] timeout-download-keep = yes | no
Si la conexi�n del servidor agota el tiempo de espera mientras descarga una p�gina, esta p�gina incompleta se guardar�. (por defecto=no).
SECTION OfflineOptions
Opciones que controlan como se comporta WWWOFLE cuando est� desconectado.
ITEM pragma-no-cache
[<URL-SPEC>] pragma-no-cache = yes | no
Especifica si se ha de pedir una nueva copia de una p�gina si la petici�n del navegador ten�a la cabecera 'Pragma: no-cache' (por defecto=yes).  Esta opci�n deber�a ponerse a 'no' si cuando est� desconectado todas las p�ginas son vueltas a pedir por un  navegador 'roto'
ITEM confirm-requests
[<URL-SPEC>] confirm-requests = yes | no
Especifica si volver a una p�gina requiere confirmaci�n del usuario en vez de grabar la petici�n autom�ticamente cuando se est�  desconectado. (por defecto=no).
ITEM dont-request
[<URL-SPEC>] dont-request = yes | no
No pedir ninguna URL que corresponda con estas mientras se est� desconectado. (por defecto=no).
SECTION FetchOptions
La opciones que controlan la recogida de p�ginas que se pidieron estando  desconectado.
ITEM stylesheets
stylesheets = yes | no
Si se quieren descargar Hojas de Estilo. (por defecto=no).
ITEM images
images = yes | no
Si se quieren descargar Im�genes. (por defecto=no).
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
Deshabilita la creaci�n de los �ndices �ltima Vez/Vez Anterior (por defecto=no).
ITEM list-outgoing
<URL-SPEC> list-outgoing = yes | no
Elije si la URL se mostrar� en el directorio de peticiones salientes. (por defecto=yes).
ITEM list-latest
<URL-SPEC> list-latest = yes | no
Elije si la URL se mostrar� en los �ndices �ltima Vez/Vez Anterior y �ltima Salida/Salida Anterior (por defecto=yes).
ITEM list-monitor
<URL-SPEC> list-monitor = yes | no
elije si la URL se mostrar� en el �ndice de p�ginas monitorizadas. (por defecto=yes).
ITEM list-host
<URL-SPEC> list-host = yes | no
Elije si la URL se mostrar� en los �ndices de servidores  (por defecto=yes).
ITEM list-any
<URL-SPEC> list-any = yes | no
Elije si la URL se mostrar� en alguno de los �ndices  (por defecto=yes).
SECTION ModifyHTML
Opciones que controlan como el HTML almacenado en la cach� es modificado.
ITEM enable-modify-html
[<URL-SPEC>] enable-modify-html = yes | no  
Activar las modificaciones en esta secci�n (por defecto=no). Con esta opci�n desactivada las dem�s opciones sobre HTML no tendr�n ning�n efecto. Con esta opci�n activada hay una peque�a penalizaci�n en velocidad.
ITEM add-cache-info
[<URL-SPEC>] add-cache-info = yes | no 
A�adir al final de todas las  p�ginas almacenadas la fecha en la  que esa p�gina fue recogida y algunos botones (por defecto=no).
ITEM anchor-cached-begin
[<URL-SPEC>] anchor-cached-begin = (HTML code) | 
Los enlaces que son almacenados tienen el c�digo HTML especificado  insertado al principio (por defecto="").
ITEM anchor-cached-end
[<URL-SPEC>] anchor-cached-end = (HTML code) | 
Los enlaces que son almacenados tienen el c�digo HTML especificado  insertado al final (por defecto="").
ITEM anchor-requested-begin
[<URL-SPEC>] anchor-requested-begin = (HTML code) | 
Los enlaces que han sido pedidos para descarga tienen el c�digo HTML  especificado insertado al principio.(por defecto="").
ITEM anchor-requested-end
[<URL-SPEC>] anchor-requested-end = (HTML code) | 
Los enlaces que han sido pedidos para descarga tienen el c�digo HTML  especificado insertado al final.(por defecto="").
ITEM anchor-not-cached-begin
[<URL-SPEC>] anchor-not-cached-begin = (HTML code) |�
Los enlaces de las p�ginas que no se guardar�n en la cach� o no ser�n pedidas tienen el siguiente c�digo insertado al principio.  (por defecto="")
ITEM anchor-not-cached-end
[<URL-SPEC>] anchor-not-cached-end = (HTML code) |�
Los enlaces de las p�ginas que no se guardar�n en la cach� o no ser�n pedidas tienen el siguiente c�digo insertado al final.  (por defecto="")
ITEM disable-script
[<URL-SPEC>] disable-script = yes | no
Quita todos los guiones (por defecto=no).
ITEM disable-blink
[<URL-SPEC>] disable-blink = yes | no
Quita la etiqueta de parpadeo (&lt;blink&gt;) (por defecto=no).
ITEM disable-meta-refresh
[<URL-SPEC>] disable-meta-refresh = yes | no
Quita cualquier etiqueta de tipo "meta" en la cabecera HTML que redirija al navegador a recargar la p�gina tras un espacio de tiempo (por defecto=no).
ITEM demoronise-ms-chars
[<URL-SPEC>] demoronise-ms-chars = yes | no
Reemplaza los caracteres extra�os que alguna aplicaciones de Microsoft ponen en el HTML con caracteres equivalentes que la mayor�a  de los navegadores pueden mostrar (por defecto=no).  La idea viene del gui�n Perl de dominio publico "Demoroniser" (Desidiotizador)
ITEM disable-animated-gif
[<URL-SPEC>] disable-animated-gif = yes | no
Desactiva la animaci�n de los ficheros GIF (por defecto=no).
SECTION LocalHost
Una lista de hu�spedes que el servidor ejecutando wwwoffled puede ser conocido.  Esto es as� para que el proxy no necesite contactar el mismo para conseguir las p�ginas locales del servidor en el caso de que tenga diferentes nombres.
ITEM 
(servidor) 
Un nombre de servidor o direcci�n IP que en conexi�n con el n�mero  de puerto (en la secci�n Startup) especifica el servidor HTTP WWWOFFLE. Los nombres de servidores tienen que coincidir exactamente, no use comodines..  El primer nombre del servidor tendr� diferentes usos por lo que deber�a ser un nombre que funcione desde todos los clientes de la red.  Ninguno de los servidores nombrados aqu� ser�n almacenados o recogidos a trav�s del proxy.
SECTION LocalNet
Una lista de servidores que tienen accesible el servicio web incluso cuando est� desconectado y WWWOFFLE no debe almacenar porque est�n en una red local.
ITEM 
(servidor)
Un nombre de servidor o direcci�n IP que est� siempre disponible y WWWOFFLE no almacenar�. La correspondencia de nombres de servidores usa comodines.  Puede excluir un servidor a�adiendo el s�mbolo '!' al principio del nombre. Tambi�n se necesitan todos los posibles alias y direcciones IP del servidor.  Se asumir� que todas las  entradas que escriba aqu� estar�n accesibles cuando est� desconectado. Ninguno de estos servidores ser� almacenados o recogidos a trav�s del proxy.
SECTION AllowedConnectHosts
Una lista de los hu�spedes que tienen permitida la conexi�n al servidor.
ITEM 
(hu�sped)
Un hu�sped o direcci�n IP que tiene permitida la conexi�n al servidor. La correspondencia de nombres de servidores usa comodines.  Puede excluir un hu�sped a�adiendo el s�mbolo '!' al principio del nombre.  Tambi�n se necesitan todos los posibles alias y direcciones IP  del hu�sped.  Todos los nombres de servidor nombrados en las secci�n LocalHost tambi�n tienen permitida la conexi�n.
SECTION AllowedConnectUsers
Una lista de los usuarios y sus contrase�as, que tienen permitida la conexi�n  con el servidor.
ITEM 
(usuario):(contrase�a)
El nombre de usuario y la contrase�a de los usuarios que tienen  permitida la conexi�n con el servidor. Si la secci�n se deja vac�a  no se realiza autentificaci�n por contrase�a.  El nombre de usuario  y contrase�as se almacenan en formato de texto plano.  Esta opci�n  requiere el uso de navegadores que soporten el est�ndar HTTP/1.1 de  autentificaci�n frente al proxy.
SECTION DontCache
Una lista de URLs que WWWOFFLE no almacenar�.
ITEM 
[!]URL-SPECIFICATION
No almacenar ninguna URL que coincida con esto.  La <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> puede ser negada para permitir que las coincidencias sean almacenadas.  Las URLs no se pedir�n si est�  desconectado
SECTION DontGet
Una lista de URLs que WWWOFFLE no debe descargar (porque solo contienen  publicidad o basura, por ejemplo).
ITEM 
[!]URL-SPECIFICATION
No descargar ninguna URL que coincida con esto.  La <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> puede ser negada para permitir descargar las coincidencias.
ITEM replacement
[<URL-SPEC>] replacement = (URL)
La URL a usar para reemplazar cualquier URL que coincida con la <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> en vez de usar el mensaje de error est�ndar (por defecto=ninguna).  Se sugiere como reemplazo la URL /local/dontget/replacement.gif (un gif transparente de 1x1 pixel).
ITEM get-recursive
<URL-SPEC> get-recursive = yes | no
Elija si las URLs que coincidan ser�n recogidas en una recogida recursiva (por defecto=yes).
ITEM location-error
<URL-SPEC> location-error = yes | no
Cuando la respuesta de la URL contiene una cabecera 'Location' () que redirige a una p�gina especificada en esta secci�n la respuesta es modificada para mostrar un mensaje de error en vez de la p�gina  (por defecto=no).  Esto parar� la redirecci�n de los proxies de los ISPs a anuncios si las URLs de los anuncios est�n en esta secci�n.
SECTION CensorHeader
Una lista de cabeceras HTTP que se deben quitar de la petici�n hecha al  servidor web y de las respuestas que vuelvan de vuelta.
ITEM 
[<URL-SPEC>] (cabecera) = yes | no | (cadena)
Una campo de la cabecera, (p.e. From, Cookie, Set-Cookie User-Agent)  y la cadena para reemplazar el valor de la cabecera (por defecto=no). La cabecera distingue may�sculas de min�sculas y no contiene un ':' al final.  El valor "no" significa que la cabecera no se modificar�. Los valores "yes" o no poner una cadena pueden usarse para eliminar  la cabecera. Tambi�n puede usar una cadena para reemplazar la cabecera.  Esta opci�n s�lo reemplaza cabeceras si las encuentra, no  a�ade ninguna nueva.
ITEM referer-self
[<URL-SPEC>] referer-self = yes | no 
Pone la cabecera Referer al mismo valor que la URL que se ha pedido (por defecto = no).
ITEM referer-self-dir
[<URL-SPEC>] referer-self-dir = yes | no 
Pone la cabecera Referer al nombre del directorio en el que se encuentra la URL que se ha pedido (por defecto = no).  Esta opci�n toma preferencia sobre referer-self.
SECTION FTPOptions
Opciones a usar cuando se descargan ficheros usando el protocolo ftp.
ITEM anon-username
anon-username = (cadena)
El nombre de usuario a usar para ftp an�nimo (por defecto=anonymous).
ITEM anon-password
anon-password = (cadena)
La contrase�a a usar para ftp an�nimo  (por defecto=se determina en tiempo de ejecuci�n).   Si est� usando un cortafuegos puede contener valores no v�lidos para el servidor FTP y debe ser puesto a un valor diferente.
ITEM auth-username
[<URL-SPEC>] auth-username = (cadena)
Un nombre de usuario a usar en un servidor en vez de usuario an�nimo  por defecto.
ITEM auth-password
[<URL-SPEC>] auth-password = (cadena)
La contrase�a a usar en un servidor en vez de la contrase�a an�nima por defecto.
SECTION MIMETypes
Tipos MIME a usar cuando se sirven ficheros que no han sido recogidos usando HTTP o para ficheros en el servidor web incorporado.
ITEM default
default = (tipo-mime)/(subtipo)
El tipo MIME por defecto. (por defecto=text/plain).
ITEM 
.(ext-fichero) = (tipo-mime)/(subtipo) 
El tipo MIME asociado con la extensi�n de un fichero. El punto (.) debe ser incluido en la extensi�n de fichero. Si coincide m�s de una extensi�n se usar� la m�s larga.
SECTION Proxy
Contiene los nombre de los proxies HTTP (u otros) a usar externamente a la  m�quina local con WWWOFFLE.
ITEM proxy
[<URL-SPEC>] proxy = (servidor[:puerto])
El hu�sped y el puerto a usar como proxy por defecto.
ITEM auth-username
<URL-SPEC> auth-username = (cadena)
El nombre de usuario a usar en un servidor proxy para autentificar  WWWOFFLE frente a �l. La <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> en este caso se refiere al proxy, no a la URL que se est� recogiendo.
ITEM auth-password
<URL-SPEC> auth-password = (cadena)
La contrase�a a usar en el servidor proxy para autentificar WWWOFFLE frente a �l. La <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> en este caso se refiere al proxy y no a la URL que se est� recogiendo.
ITEM ssl
[<URL-SPEC>] ssl = (servidor[:puerto])
Un servidor proxy que se debe usar para conexiones de Capa de Conexi�n Segura (SSL) p.e. https. Note que de la <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> s�lo se comprueba el nombre de servidor. Las otras partes deben ser comodines (*).
SECTION Alias
Una lista de alias que son usados para reemplazar el nombre del servidor y el  camino con otro nombre de servidor y camino. Tambi�n para servidores que son conocidos por dos nombres.
ITEM 
URL-SPECIFICATION = URL-SPECIFICATION
Cualquier petici�n que coincida con la primera <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> es  reemplazada por la segunda <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a>. Las <a href="/configuration/#URL-SPECIFICATION">URL-SPEC</a> deben coincidir exactamente. No es una correspondencia por comodines. Los argumentos de la URL son ignorados.
SECTION Purge
El m�todo que determina que p�ginas eliminar, la edad por defecto, la edad de  las p�ginas de un servidor determinado en d�as y el tama�o m�ximo de la cach�.
ITEM use-mtime
use-mtime = yes | no 
El m�todo a usar para decidir que ficheros purgar, el tiempo de  acceso (atime) o el tiempo de �ltima modificaci�n (mtime) (por defecto=no).
ITEM max-size
max-size = (tama�o)
El tama�o m�ximo de la cach� en MB despu�s de purgar (por defecto=0). Un tama�o m�ximo de cach� de 0 significa que no hay l�mite en tama�o.  Si se usa esta opci�n y la opci�n min-free se elegir� el menor tama�o de cach�.  Esta opci�n, al calcular el tama�o de la cach�, tiene en cuenta las URLs que nunca son purgadas pero no las  eliminar�.
ITEM min-free
min-free = (size)
El espacio m�nimo libre en disco en MB despu�s de purgar  (por defecto=0).  Un tama�o m�nimo de disco de 0 significa que no hay l�mite de espacio libre. Si se usa esta opci�n y la opci�n max-free se elegir� el menor tama�o de cach�.  Esta opci�n tiene en cuenta las URLs que nunca son purgadas pero no las eliminar�.
ITEM use-url
use-url = yes | no 
si es verdad se usa la URL para decidir en la edad de purga, si  no se usa el servidor y el protocolo. (por defecto=no).
ITEM del-dontget
del-dontget = yes | no
Si es verdad se borran las URLs que coinciden con las entradas de la secci�n DontGet (por defecto=no).
ITEM del-dontcache
del-dontcache = yes | no
Si es verdad se borran las URLs que coinciden con las entradas  de la secci�n DontCache (por defecto=no).
ITEM age
[<URL-SPEC>] age = (edad)
La edad m�xima en la cach� para las URLs que coinciden con esto  (por defecto=14).  Una edad de cero significa no guardar. Un valor  negativo no borrar.  La <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> corresponde s�lo con el protocolo y servidor a menos que se haya especificado como verdadera la opci�n use-url.  Se pueden especificar tiempos m�s largo con los sufijos 'w', 'm' o 'y' para semanas, meses o a�os (p. e. 2w=14).
SECTION COMODINES
Un comod�n es usar el car�cter '*' para representar cualquier grupo de  caracteres. <p> Es b�sicamente la misma expresi�n de correspondencia de ficheros de la l�nea de comandos de DOS o la shell de UNIX, excepto que el car�cter '*' puede aceptar el car�cter '/'. <p> Por ejemplo <p> *.gif      corresponde con  foo.gif y bar.gif *.foo.com  corresponde con  www.foo.com y ftp.foo.com /foo/*     corresponde con  /foo/bar.html y /foo/bar/foobar.html
SECTION URL-SPECIFICATION
Cuando se especifica un servidor, un protocolo y una ruta, en muchas secciones se puede usar una <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a>, que es una forma de reconocer una URL <p> Para esta explicaci�n consideramos que una URL puede constar de cinco partes. <p> proto           El protocolo que usa (p.e. 'http', 'ftp') servidor        El nombre de servidor (p.e. 'www.gedanken.demon.co.uk'). puerto          El n�mero de puerto en el servidor (p.e. por defecto 80 para HTTP). ruta            La ruta en el servidor (p.e. '/bar.html') o un nombre de  directorio (p.e. '/foo/'). argumentos      Argumentos opciones de la URL usados por guiones CGIs etc... (p. e.) 'search=foo'). <p> Por ejemplo, en la p�gina de WWWOFFLE: http://www.gedanken.demon.co.uk/wwwoffle/ El protocolo es 'http', el servidor es 'www.gedanken.demon.co.uk', el puerto es el predeterminado (en este caso 80), y la ruta es '/wwwoffle/'. <p> En general se escribe como
ITEM 
(proto)://(servidor)[:(puerto)]/(ruta)[?(argumentos)]
Donde [] indica una caracter�stica opcional, y () indica un nombre o n�mero proporcionado por el usuario. <p> Alguna opciones comunes de <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> son las siguientes: <p> *://*/*             Cualquier protocolo, cualquier servidor, cualquier ruta (Es lo mismo que decir 'el predeterminado'). <p> *://*/(ruta)        Cualquier protocolo, cualquier servidor,  cualquier puerto, una ruta, cualquier argumento. <p> *://*/*.(ext)       Cualquier protocolo, cualquier servidor, cualquier puerto, una ruta, cualquier argumento. <p> *://*/*?            Cualquier protocolo, cualquier servidor, cualquier ruta, Ning�n argumento. <p> *://(servidor)/*    Cualquier protocolo, un servidor, cualquier puerto, cualquier ruta, cualquier argumento.
ITEM 
(proto)://*/*       Un protocolo, cualquier servidor, cualquier puerto,
cualquier ruta, cualquier argumento.
ITEM 
(proto)://(servidor)/* Un protocolo, un servidor, cualquier puerto, 
cualquier ruta, cualquier argumento
ITEM 
(proto)://(servidor):/* Un protocolo, un servidor, puerto predeterminado,
cualquier ruta, cualquier argumento. <p> *://(servidor):(puerto)/* Cualquier protocolo, un servidor, un puerto,  cualquier ruta, cualquier argumento. <p> La correspondencia del servidor y la ruta usa los comodines descritos arriba. <p> En algunas secciones se acepta que la <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> este negada a�adiendo el car�cter '!' al comienzo. Esto significa que la comparaci�n entre la URL y la <a href="/configuration/#URL-SPECIFICATION">URL-SPECIFICATION</a> devolver� el valor l�gico opuesto al que devolver�a sin el car�cter '!'. Si todas las ESPECIFICACIONES-URL  de la secci�n est�n negadas y se a�ade '*://*/*' al final, el sentido de la  secci�n entera ser� negado.
