//
// WWWOFFLE Version 2.7 - Gui�n de autoconfiguraci�n del Proxy
//
// Vea: http://www.netscape.com/eng/mozilla/2.0/relnotes/demo/proxy-live.html
//
// Deber�a reemplazar 'LOCALHOST' con el nombre del servidor del proxy si se
// necesita.
//

function FindProxyForURL(url, host)
{
 if(isPlainHostName(host))
    return "DIRECT";
 else
    if((url.substring(0, 5) == "http:") || (url.substring(0, 4) == "ftp:"))
       return "PROXY LOCALHOST; DIRECT";
    else
       return "DIRECT";
}
