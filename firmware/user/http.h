static const char *noWifiHtml = "<html> <body> <p style='font-size:150%'> Start as a client, disable WIFI AP </p> </body </html>\r\n";
static const char *formHtml = "<html> <header> <style>h4,h5{font-size:2.4rem;line-height:1.5;margin:0}h5{font-size:1.5rem;margin:.5em 0 .5em 0}#cn{position:relative;width:100%;max-width:960px;margin:0 auto}.btn{text-transform:uppercase;height:38px;padding:0 30px;border:0;margin-top:5px;display:block}.inp{width:100%;height:38px;padding:6px 10px;border:1px solid #d1d1d1;margin-top:5px}.ac{display:none}.sd{display:block}.sl{maring:5px 0}</style> </header> <body> <div id=\"cn\"> <h4> Setting</h4> </div> </body> <script>var h=[{n:\"int\",p:\"Interval (d. 120sec)\",l:5},{t:\"c\",n:\"adv\",p:\"Advanced\",f:\"ad\",c:\"aw sd\"},{n:\"mmn\",p:\"PPM Min (d. 400)\",c:\"ac ad\",l:4},{n:\"mmx\",p:\"PPM Max (d. 2400)\",c:\"ac ad\",l:4},{t:\"s\",n:\"brc\",c:\"ac ad\"},{t:\"c\",n:\"wS\",f:\"aw\",p:\"Wifi&MQTT Setting\",},{n:\"wf\",p:\"WIFI Name\",c:\"ac aw\",l:32},{n:\"ps\",p:\"WIFI Pass\",c:\"ac aw\",l:64},{t:\"c\",n:\"ipc\",p:\"Static IP\",f:\"ai\",c:\"ac aw\"},{n:\"ip\",p:\"IP\",c:\"ac ap\",l:15},{n:\"net\",p:\"Subnet\",c:\"ac ap\",l:15},{n:\"gw\",p:\"Gateway\",c:\"ac ap\",l:15},{n:\"dn1\",p:\"DNS1\",c:\"ac ap\",l:15},{n:\"dn2\",p:\"DNS2\",c:\"ac ap\",l:15},{t:\"t\",p:\"MQTT Setting\",c:\"ac aw\"},{n:\"mId\",p:\"ID\",c:\"ac aw\",l:12},{n:\"mad\",p:\"IP or DNS\",c:\"ac aw\",l:64},{n:\"mprt\",p:\"Port(d.1883)\",c:\"ac aw\",l:5},{n:\"mtpc\",p:\"Topic\",c:\"ac aw\",l:128},{n:\"mqs\",p:\"QoS(d.1)\",c:\"ac aw\",l:1},{t:\"c\",n:\"ath\",p:\"Authentification\",f:\"aa\",c:\"ac aw\"},{n:\"mus\",p:\"User\",c:\"ac au\",l:32},{n:\"mps\",p:\"Password\",c:\"ac au\",l:64},{t:\"b\",p:\"Set \"}];function r(c,a){if(a===0){var b=document.createElement(c)}if(a===1){var b=document.createTextNode(c)}return b}var con=document.getElementById(\"cn\");var f=r(\"form\",0);con.appendChild(f);for(var i=0;i<h.length;i++){if(h[i].t===\"c\"){var e=r(\"input\",0);e.type=\"checkbox\";e.setAttribute(\"name\",h[i].n);e.className=h[i].c+\" chk\";e.addEventListener(\"click\",function(a){sh(this)});var s=r(\"span\",0);s.className=h[i].c+\" \";s.appendChild(r(h[i].p,1));f.appendChild(s)}else{if(h[i].t===\"b\"){var e=r(\"Button\",0);e.className=\"btn\";e.appendChild(r(h[i].p,1))}else{if(h[i].t===\"t\"){var e=r(\"h5\",0);e.appendChild(r(h[i].p,1));e.className=h[i].c}else{if(h[i].t===\"s\"){var e=r(\"select\",0);e.className=h[i].c+\" sl\";e.setAttribute(\"name\",h[i].n);for(var y=1;y<=4;y++){var o=r(\"option\",0);o.value=y;o.innerHTML=y;e.appendChild(o);if(y==\"1\"){o.setAttribute(\"selected\",\"selected\")}}var t=r(\"label\",0);t.className=\" ad ac\";t.appendChild(r(\"Brightness coefficient(255/x): \",1));f.appendChild(t)}else{var e=r(\"input\",0);e.setAttribute(\"name\",h[i].n);e.setAttribute(\"placeholder\",h[i].p);e.maxLength=h[i].l;e.className=h[i].c+\" inp\"}}}}f.appendChild(e);function sh(a){if(a.name===\"wS\"){var c=document.getElementsByClassName(\"aw\");for(var b=0;b<c.length;b++){c[b].style.display=\"block\"}}if(a.name===\"ipc\"){var c=document.getElementsByClassName(\"ap\");for(var b=0;b<c.length;b++){c[b].style.display=\"block\"}}if(a.name===\"ath\"){var c=document.getElementsByClassName(\"au\");for(var b=0;b<c.length;b++){c[b].style.display=\"block\"}}if(a.name===\"adv\"){var c=document.getElementsByClassName(\"ad\");for(var b=0;b<c.length;b++){c[b].style.display=\"block\"}}}};</script> <html>\r\n";