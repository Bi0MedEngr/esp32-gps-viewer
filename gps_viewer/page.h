#pragma once
#include <pgmspace.h>

static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>GPS Viewer</title>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css">
<style>
:root{--bg:#f4f5f7;--card:#fff;--ink:#1b1f24;--mute:#68707c;--line:#e1e4e8;--acc:#0b6bcb;--ok:#1a7f37;--warn:#b26b00;--bad:#c62828;--mono:ui-monospace,SFMono-Regular,Menlo,monospace}
@media (prefers-color-scheme:dark){:root{--bg:#0f1216;--card:#171b21;--ink:#e6e9ee;--mute:#8b94a1;--line:#272d36;--acc:#4ea1ff;--ok:#3fb950;--warn:#e3a008;--bad:#f85149}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.4 -apple-system,system-ui,Segoe UI,Roboto,sans-serif}
header{display:flex;align-items:center;gap:12px;padding:12px 16px;border-bottom:1px solid var(--line);background:var(--card);position:sticky;top:0;z-index:1000}
h1{font-size:17px;margin:0;font-weight:650}.sp{flex:1}
.pill{padding:3px 10px;border-radius:99px;font-size:13px;font-weight:600;border:1px solid currentColor}
.dot{width:9px;height:9px;border-radius:50%;background:var(--mute);display:inline-block}
main{max-width:1100px;margin:0 auto;padding:16px;display:grid;gap:16px;grid-template-columns:1fr}
@media(min-width:900px){main{grid-template-columns:1.1fr 1fr}.wide{grid-column:1/-1}}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px 16px;min-width:0}
.card h2{font-size:12px;letter-spacing:.06em;text-transform:uppercase;color:var(--mute);margin:0 0 10px;font-weight:600}
.coord{font:600 clamp(22px,5vw,34px)/1.2 var(--mono);letter-spacing:-.01em;word-break:break-all}
.coord small{font:500 12px/1 -apple-system,system-ui,sans-serif;color:var(--mute);display:block;margin-bottom:2px;letter-spacing:0}
.row{display:flex;flex-wrap:wrap;gap:8px;margin-top:12px;align-items:center}
button,select,a.btn{font:inherit;font-size:13px;padding:6px 11px;border-radius:7px;border:1px solid var(--line);background:var(--bg);color:var(--ink);cursor:pointer;text-decoration:none}
button:hover,a.btn:hover{border-color:var(--acc)}button.on{background:var(--acc);color:#fff;border-color:var(--acc)}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(130px,1fr));gap:10px}
@media(max-width:600px){main{padding:12px;gap:12px}.card{padding:12px}.grid{grid-template-columns:1fr 1fr;gap:8px}.st b{font-size:16px}#map{height:300px}button,select,a.btn{min-height:40px;font-size:15px}select{font-size:16px}#sky{width:100%;height:auto}}
.st{border:1px solid var(--line);border-radius:8px;padding:8px 10px}.st b{display:block;font:600 18px var(--mono)}.st span{font-size:12px;color:var(--mute)}
#map{height:380px;border-radius:8px;border:1px solid var(--line)}
#mapmsg{color:var(--mute);font-size:13px;padding:40px 0;text-align:center;display:none}
.sky{display:flex;gap:16px;flex-wrap:wrap;align-items:flex-start}#sky{width:260px;height:260px;max-width:100%}
#bars{flex:1;min-width:220px;display:flex;align-items:flex-end;gap:4px;height:180px;border-bottom:1px solid var(--line);padding-top:16px}
.bar{flex:1;max-width:28px;display:flex;flex-direction:column;align-items:center;justify-content:flex-end;height:100%;font:11px var(--mono);color:var(--mute)}
.bar i{display:block;width:100%;border-radius:3px 3px 0 0;background:var(--mute)}.bar.used i{background:var(--ok)}
.bar em{font-style:normal;margin-top:3px}
pre{font:12px/1.45 var(--mono);background:var(--bg);border:1px solid var(--line);border-radius:8px;padding:10px;height:260px;overflow:auto;margin:0;white-space:pre-wrap;word-break:break-all}
.hint{color:var(--mute);font-size:13px;margin:8px 0 0}
details summary{cursor:pointer;color:var(--mute);font-size:12px;letter-spacing:.06em;text-transform:uppercase;font-weight:600}
</style></head><body>
<header><h1>GPS Viewer</h1><span class="pill" id="pill">connecting</span><span class="sp"></span><span class="dot" id="dot" title="link to ESP32"></span></header>
<main>
<section class="card">
  <h2>Position</h2>
  <div class="coord"><small>Latitude</small><span id="lat">--</span></div>
  <div class="coord" style="margin-top:8px"><small>Longitude</small><span id="lon">--</span></div>
  <div class="row">
    <select id="fmt"><option value="dd">Decimal degrees</option><option value="ddm">Deg + decimal min</option><option value="dms">Deg min sec</option></select>
    <button id="copy">Copy</button>
    <a class="btn" id="gmap" target="_blank" rel="noopener">Google Maps</a>
  </div>
  <p class="hint" id="why"></p>
</section>
<section class="card">
  <h2>Fix</h2>
  <div class="grid">
    <div class="st"><span>Altitude (MSL)</span><b id="alt">--</b></div>
    <div class="st"><span>Speed</span><b id="spd">--</b></div>
    <div class="st"><span>Course</span><b id="crs">--</b></div>
    <div class="st"><span>Sats used / seen</span><b id="sats">--</b></div>
    <div class="st"><span>HDOP / PDOP</span><b id="dop">--</b></div>
    <div class="st"><span>Est. accuracy</span><b id="acc">--</b></div>
    <div class="st"><span>UTC</span><b id="utc">--</b></div>
    <div class="st"><span>Fix age</span><b id="age">--</b></div>
    <div class="st"><span>Track distance</span><b id="dist">--</b></div>
  </div>
</section>
<section class="card wide">
  <h2>Map</h2>
  <div id="map"></div><div id="mapmsg">The map needs internet on this device. On the ESP32-GPS access point you get coordinates only.</div>
  <div class="row">
    <button id="follow" class="on">Follow</button>
    <button id="layer">Satellite</button>
    <button id="clear">Clear track</button>
    <button id="gpx">Download GPX</button>
    <button id="csv">Download CSV</button>
    <span class="hint" id="npts" style="margin:0"></span>
  </div>
</section>
<section class="card wide">
  <h2>Satellites</h2>
  <div class="sky"><svg id="sky" viewBox="-110 -110 220 220"></svg><div id="bars"></div></div>
  <p class="hint">Bars are signal strength (C/N0, dB-Hz). Green = used in the fix, grey = tracked or only predicted. Sky plot: center is overhead, ring = 45 deg elevation, north up.</p>
</section>
<section class="card wide">
  <details id="rawd"><summary>Raw NMEA</summary>
  <pre id="raw" style="margin-top:10px"></pre>
  <p class="hint" id="rxinfo"></p></details>
</section>
</main>
<script>
const $=id=>document.getElementById(id);
let D=null,map=null,mk=null,circ=null,line=null,street=null,sat=null,useSat=false,follow=true;
let track=[],lastKey='',rawSeq=0,rawLines=[],okStreak=0;
function fmtCoord(v,pos,neg){
  if(v==null)return'--';const h=v<0?neg:pos,a=Math.abs(v),f=$('fmt').value;
  if(f==='dd')return v.toFixed(7);
  const d=Math.floor(a),m=(a-d)*60;
  if(f==='ddm')return d+'° '+m.toFixed(4)+"' "+h;
  const mi=Math.floor(m),s=(m-mi)*60;return d+'° '+mi+"' "+s.toFixed(2)+'" '+h;
}
function hav(a,b){const R=6371008.8,r=Math.PI/180,dLa=(b[0]-a[0])*r,dLo=(b[1]-a[1])*r;
  const x=Math.sin(dLa/2)**2+Math.cos(a[0]*r)*Math.cos(b[0]*r)*Math.sin(dLo/2)**2;return 2*R*Math.asin(Math.sqrt(x));}
function iso(d){if(!d||!d.time||d.time.length<6)return null;const t=d.time;
  let s=t.slice(0,2)+':'+t.slice(2,4)+':'+t.slice(4,6);
  if(d.date&&d.date.length===6)return '20'+d.date.slice(4,6)+'-'+d.date.slice(2,4)+'-'+d.date.slice(0,2)+'T'+s+'Z';return s;}
function initMap(){
  if(map)return;
  if(!window.L){$('map').style.display='none';$('mapmsg').style.display='block';return;}
  $('map').style.display='';$('mapmsg').style.display='none';
  map=L.map('map',{zoomControl:true}).setView([20,0],2);
  street=L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19,attribution:'&copy; OpenStreetMap'}).addTo(map);
  sat=L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}',{maxZoom:19,attribution:'Esri World Imagery'});
  line=L.polyline([],{color:'#0b6bcb',weight:3}).addTo(map);
  map.on('dragstart',()=>{follow=false;$('follow').classList.remove('on')});
}
function setPill(t,c){const p=$('pill');p.textContent=t;p.style.color=c;}
function render(d){
  const css=getComputedStyle(document.documentElement),ok=css.getPropertyValue('--ok'),warn=css.getPropertyValue('--warn'),bad=css.getPropertyValue('--bad');
  const live=d.dataAge!=null&&d.dataAge<3000,fix=live&&d.haveLoc&&d.locAge<3000;
  if(!live){setPill('no data from GPS',bad);}
  else if(!fix){setPill('searching · '+d.sats.length+' in view',warn);}
  else setPill((d.mode===3?'3D fix':d.mode===2?'2D fix':'fix')+(d.fixQ===2?' (DGPS)':''),ok);
  let why='';
  if(d.rxBytes===0)why='Nothing received on GPIO 18. Check GPS TX to GPIO 18, GND, and that the GPS LED/power is on.';
  else if(!d.baudLocked)why='Bytes arriving but no valid NMEA yet; trying '+d.baud+' baud.';
  else if(!live)why='GPS stopped sending '+Math.round(d.dataAge/1000)+' s ago.';
  else if(!fix)why='Receiving NMEA, no fix yet. A cold start takes 30 s to a few minutes and needs open sky (a window at best).'+(d.haveLoc?' Showing last known position.':'');
  else if(d.locAge>=3000)why='Fix is stale.';
  $('why').textContent=why;
  $('lat').textContent=d.haveLoc?fmtCoord(d.lat,'N','S'):'--';
  $('lon').textContent=d.haveLoc?fmtCoord(d.lon,'E','W'):'--';
  $('gmap').href=d.haveLoc?'https://maps.google.com/?q='+d.lat+','+d.lon:'#';
  $('alt').textContent=d.alt!=null&&fix?d.alt.toFixed(1)+' m':'--';
  $('spd').textContent=d.spd!=null&&fix?d.spd.toFixed(1)+' km/h':'--';
  $('crs').textContent=d.crs!=null&&fix&&d.spd>2?d.crs.toFixed(0)+'°':'--';
  $('sats').textContent=(d.satsUsed||0)+' / '+d.sats.length;
  $('dop').textContent=(d.hdop!=null?d.hdop.toFixed(1):'--')+' / '+(d.pdop!=null?d.pdop.toFixed(1):'--');
  const accM=d.hdop!=null&&fix?d.hdop*2.5:null;
  $('acc').textContent=accM!=null?'±'+accM.toFixed(1)+' m':'--';
  const t=iso(d);$('utc').textContent=t?t.replace(/^.*T/,'').replace('Z',''):'--';
  $('age').textContent=d.locAge!=null?(d.locAge<1000?'<1 s':Math.round(d.locAge/1000)+' s'):'--';
  if(fix){
    const key=d.time+d.lat+d.lon;
    if(key!==lastKey){lastKey=key;track.push({lat:d.lat,lon:d.lon,alt:d.alt,t:t,hdop:d.hdop,spd:d.spd});if(track.length>20000)track.shift();if(line)line.addLatLng([d.lat,d.lon]);}
  }
  let dist=0;for(let i=1;i<track.length;i++)dist+=hav([track[i-1].lat,track[i-1].lon],[track[i].lat,track[i].lon]);
  $('dist').textContent=track.length>1?(dist<1000?dist.toFixed(0)+' m':(dist/1000).toFixed(2)+' km'):'--';
  $('npts').textContent=track.length+(track.length===1?' point':' points');
  if(map&&d.haveLoc){
    const ll=[d.lat,d.lon];
    if(!mk){mk=L.circleMarker(ll,{radius:7,color:'#fff',weight:2,fillColor:'#0b6bcb',fillOpacity:1}).addTo(map);circ=L.circle(ll,{radius:accM||10,color:'#0b6bcb',weight:1,fillOpacity:.12}).addTo(map);map.setView(ll,18);}
    else{mk.setLatLng(ll);circ.setLatLng(ll);if(accM)circ.setRadius(accM);if(follow)map.panTo(ll,{animate:true});}
  }
  drawSats(d);
}
function drawSats(d){
  const ns='http://www.w3.org/2000/svg',svg=$('sky'),mute=getComputedStyle(document.documentElement).getPropertyValue('--mute'),line=getComputedStyle(document.documentElement).getPropertyValue('--line');
  let h='<circle r="100" fill="none" stroke="'+line+'"/><circle r="50" fill="none" stroke="'+line+'"/><line x1="-100" x2="100" stroke="'+line+'"/><line y1="-100" y2="100" stroke="'+line+'"/>';
  h+='<text y="-103" text-anchor="middle" font-size="9" fill="'+mute+'">N</text><text x="104" y="3" font-size="9" fill="'+mute+'">E</text>';
  for(const s of d.sats){if(s[1]<0||s[2]<0)continue;const r=100*(90-s[1])/90,a=s[2]*Math.PI/180,x=r*Math.sin(a),y=-r*Math.cos(a);
    const c=s[4]?getComputedStyle(document.documentElement).getPropertyValue('--ok'):mute;
    h+='<circle cx="'+x.toFixed(1)+'" cy="'+y.toFixed(1)+'" r="7" fill="'+c+'" opacity="'+(s[4]?1:s[3]>0?.75:.35)+'"/><text x="'+x.toFixed(1)+'" y="'+(y+3).toFixed(1)+'" text-anchor="middle" font-size="7.5" fill="#fff" font-family="monospace">'+s[0]+'</text>';}
  svg.innerHTML=h;
  const sorted=[...d.sats].sort((a,b)=>a[0]-b[0]);
  $('bars').innerHTML=sorted.map(s=>{const v=Math.max(0,s[3]);return '<div class="bar'+(s[4]?' used':'')+'"><span>'+(s[3]>0?s[3]:'')+'</span><i style="height:'+Math.min(100,v*2)+'%"></i><em>'+s[0]+'</em></div>'}).join('')||'<span class="hint">no satellites reported yet</span>';
}
async function poll(){
  try{const c=new AbortController(),to=setTimeout(()=>c.abort(),2500);
    const r=await fetch('/api/gps',{signal:c.signal,cache:'no-store'});clearTimeout(to);D=await r.json();
    $('dot').style.background=getComputedStyle(document.documentElement).getPropertyValue('--ok');render(D);
    if($('rawd').open)await pollRaw();
  }catch(e){$('dot').style.background=getComputedStyle(document.documentElement).getPropertyValue('--bad');setPill('ESP32 not reachable',getComputedStyle(document.documentElement).getPropertyValue('--bad'));}
  setTimeout(poll,1000);
}
async function pollRaw(){
  const r=await fetch('/api/nmea?since='+rawSeq,{cache:'no-store'});const txt=await r.text();const ls=txt.split('\n');
  rawSeq=parseInt(ls.shift())||rawSeq;rawLines=rawLines.concat(ls.filter(Boolean)).slice(-300);
  const p=$('raw'),atEnd=p.scrollTop+p.clientHeight>=p.scrollHeight-20;p.textContent=rawLines.join('\n');if(atEnd)p.scrollTop=p.scrollHeight;
  if(D)$('rxinfo').textContent=D.baud+' baud · '+D.rxBytes+' bytes · '+D.sentences+' good sentences · '+D.badChecksum+' bad checksums';
}
function dl(name,mime,text){const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([text],{type:mime}));a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000);}
function stamp(){return new Date().toISOString().slice(0,19).replace(/[:T]/g,'-');}
$('gpx').onclick=()=>{let g='<?xml version="1.0" encoding="UTF-8"?>\n<gpx version="1.1" creator="ESP32 GPS Viewer" xmlns="http://www.topografix.com/GPX/1/1"><trk><name>ESP32 track</name><trkseg>\n';
  for(const p of track)g+='<trkpt lat="'+p.lat+'" lon="'+p.lon+'">'+(p.alt!=null?'<ele>'+p.alt+'</ele>':'')+(p.t&&p.t.includes('T')?'<time>'+p.t+'</time>':'')+(p.hdop!=null?'<hdop>'+p.hdop+'</hdop>':'')+'</trkpt>\n';
  dl('gps-track-'+stamp()+'.gpx','application/gpx+xml',g+'</trkseg></trk></gpx>\n');};
$('csv').onclick=()=>dl('gps-track-'+stamp()+'.csv','text/csv','utc,lat,lon,alt_m,hdop,speed_kmh\n'+track.map(p=>[p.t,p.lat,p.lon,p.alt,p.hdop,p.spd].map(v=>v==null?'':v).join(',')).join('\n')+'\n');
$('clear').onclick=()=>{track=[];if(line)line.setLatLngs([]);};
$('follow').onclick=e=>{follow=!follow;e.target.classList.toggle('on',follow);if(follow&&map&&D&&D.haveLoc)map.panTo([D.lat,D.lon]);};
$('layer').onclick=e=>{if(!map)return;useSat=!useSat;if(useSat){map.removeLayer(street);sat.addTo(map);}else{map.removeLayer(sat);street.addTo(map);}e.target.textContent=useSat?'Street':'Satellite';};
$('fmt').onchange=()=>{try{localStorage.gpsFmt=$('fmt').value}catch(e){}if(D)render(D);};
$('copy').onclick=()=>{if(!D||!D.haveLoc)return;const s=$('fmt').value==='dd'?D.lat.toFixed(7)+', '+D.lon.toFixed(7):$('lat').textContent+', '+$('lon').textContent;
  const done=()=>{$('copy').textContent='Copied';setTimeout(()=>$('copy').textContent='Copy',1200)};
  if(navigator.clipboard&&window.isSecureContext)navigator.clipboard.writeText(s).then(done);else{const t=document.createElement('textarea');t.value=s;t.setAttribute('readonly','');t.style.position='fixed';t.style.opacity='0';document.body.appendChild(t);t.select();t.setSelectionRange(0,999);document.execCommand('copy');t.remove();done();}};
try{if(localStorage.gpsFmt)$('fmt').value=localStorage.gpsFmt}catch(e){}
const ls=document.createElement('script');ls.src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js';ls.onload=ls.onerror=initMap;document.head.appendChild(ls);
setTimeout(initMap,8000);poll();
</script>
</body></html>)HTML";
