// SPDX-License-Identifier: GPL-2.0-or-later
// Bridge control page, served at /. Talks to the JSON API in web.cpp.
#pragma once
#include <Arduino.h>

static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Shade bridge</title>
<style>
:root{--bg:#f3f2ee;--card:#fff;--ink:#1c1c1a;--mute:#6a6963;--line:#e0ded6;--acc:#2d6b5a;--acc-ink:#fff;
--warn:#8a5300;--warn-bg:#fdf0d8;--err:#a3271f;--sky:#d9e9f3;--fab:#cdbb98;--fab2:#bba882;--rail:#6f5d3f}
@media (prefers-color-scheme:dark){:root{--bg:#151514;--card:#201f1d;--ink:#ebeae4;--mute:#99978f;--line:#34332f;
--acc:#62b49c;--acc-ink:#0c1d18;--warn:#f0b45a;--warn-bg:#382a13;--err:#ff8a80;--sky:#1e2a33;--fab:#6d604a;
--fab2:#5f533f;--rail:#c9b48c}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main{max-width:760px;margin:0 auto;padding:16px 16px 40px}
header{display:flex;flex-wrap:wrap;align-items:baseline;justify-content:space-between;gap:4px 12px;margin:4px 0 12px}
h1{font-size:21px;margin:0;letter-spacing:-.01em}
.net{color:var(--mute);font-size:13px}
.banner{background:var(--warn-bg);color:var(--warn);border-radius:10px;padding:8px 12px;font-size:13px;margin-bottom:14px}
h2{font-size:12px;font-weight:600;text-transform:uppercase;letter-spacing:.07em;color:var(--mute);margin:22px 0 8px}
.grid{display:grid;gap:10px;grid-template-columns:repeat(auto-fill,minmax(320px,1fr))}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:12px 14px}
.row{display:flex;align-items:center;gap:8px}
.card h3{font-size:16px;margin:0;flex:1}
button{font:inherit;border:1px solid var(--line);background:var(--bg);color:var(--ink);border-radius:10px;
padding:9px 14px;min-width:66px;min-height:42px;cursor:pointer;touch-action:manipulation}
button:hover{border-color:var(--mute)}
button:active{transform:translateY(1px)}
button.ghost{background:none;min-width:0;padding:6px 10px;min-height:0;font-size:13px}
.cols{display:grid;gap:8px;grid-template-columns:repeat(4,minmax(0,1fr))}
.col{display:flex;flex-direction:column;align-items:center;gap:6px;padding:12px 8px}
.col h3{flex:none;font-size:15px}
.col button{width:100%;min-width:0;padding:9px 4px}
.type{font-size:11px;color:var(--mute);margin-top:-4px}
.end{font-size:11px;font-weight:600;color:var(--mute);letter-spacing:.03em}
/* The window is the control: drag the fabric's rail up or down. Fabric hangs from the
   top (top-down) or rises from the bottom (bottom-up). */
.win{position:relative;width:100%;max-width:84px;height:240px;border:2px solid var(--line);border-radius:6px;
background:var(--sky);overflow:hidden;touch-action:none;cursor:ns-resize;user-select:none;-webkit-user-select:none}
.win:focus-visible{outline:2px solid var(--acc);outline-offset:2px}
.fab{position:absolute;left:0;right:0;height:0;transition:height .2s;
background:repeating-linear-gradient(0deg,var(--fab) 0 7px,var(--fab2) 7px 8px)}
.win.drag .fab{transition:none}
.fab.td{top:0;border-bottom:4px solid var(--rail)}
.fab.bu{bottom:0;border-top:4px solid var(--rail)}
/* Grip on the rail, so it reads as something to pull. */
.fab::after{content:"";position:absolute;left:50%;width:28px;height:8px;margin-left:-14px;border-radius:4px;
background:var(--rail);box-shadow:0 1px 3px rgba(0,0,0,.3)}
.fab.td::after{bottom:-6px}
.fab.bu::after{top:-6px}
.all{margin-top:8px}
.all .row button{min-width:92px}
.val{font-variant-numeric:tabular-nums;font-size:13px;color:var(--mute)}
.val b{color:var(--ink);font-weight:600}
.hint{color:var(--mute);font-size:12px;margin:10px 0 0}
.presets{display:grid;gap:8px;grid-template-columns:repeat(auto-fill,minmax(160px,1fr))}
.presets button{text-align:left;padding:10px 12px}
.presets small{display:block;color:var(--mute);font-size:12px;margin-top:2px}
.bar{height:6px;background:var(--line);border-radius:3px;overflow:hidden;margin:10px 0 4px}
.bar i{display:block;height:100%;width:0;background:var(--acc);transition:width .4s linear}
#now{font-weight:600}
#msg{color:var(--err);font-size:13px;min-height:1.2em;margin-top:6px}
ol{list-style:none;margin:10px 0 0;padding:10px 0 0;border-top:1px solid var(--line);
font:12px/1.55 ui-monospace,Consolas,monospace;color:var(--mute);max-height:230px;overflow:auto}
</style></head><body><main>
<header><h1>Shade bridge</h1><span class="net" id="net">connecting…</span></header>
<div class="banner" id="dry">Dry run: no radio is attached. Commands go through the real send queue and the
frames are printed on the serial console instead of transmitted.</div>

<h2>Groups</h2>
<div class="cols" id="groups"></div>
<div class="card all" id="allcard"></div>
<p class="hint">Drag a shade's rail (or tap where you want it) and let go to send; each move takes about 1.8 s
on air. Positions are the rail's height, 0 at the bottom to 255 at the top, and show what was last sent: the
shades don't report back.</p>

<h2>Presets</h2>
<div class="presets" id="presets"></div>

<h2>Activity</h2>
<div class="card">
<div class="row"><span id="now" style="flex:1">Idle</span><button class="ghost" id="clear">Clear queue</button></div>
<div class="bar"><i id="bar"></i></div>
<div class="val" id="qinfo">&nbsp;</div>
<div id="msg"></div>
<ol id="log"></ol>
</div>
</main>
<script>
const $=s=>document.querySelector(s);
const touched={};
function note(t){$('#msg').textContent=t||'';}
async function post(path,data){
  note('');
  try{
    const r=await fetch(path,{method:'POST',body:new URLSearchParams(data)});
    const j=await r.json().catch(()=>({ok:r.ok}));
    if(!j.ok) note(j.error||('Request failed ('+r.status+')'));
  }catch(e){note('No response from the bridge');}
  refresh();
}
function wire(el,g){
  el.querySelectorAll('button').forEach(b=>b.onclick=()=>post('/api/grp',{group:g,dir:b.dataset.d}));
}
const types={};  // group -> 'top-down' | 'bottom-up', from /api/groups
// Show a position (rail height 0..255, or -1 unknown) on a group's column.
const cur={};    // group -> position shown (rail height 0..255), -1 unknown
function show(g,p){
  const td=types[g]==='top-down', v=$('#v'+g), f=$('#f'+g), w=$('#w'+g);
  cur[g]=p;
  if(p<0){v.textContent='unknown';v.title='';f.style.height='0';w.removeAttribute('aria-valuenow');return;}
  const closed=Math.round(100*(td?255-p:p)/255);  // share of the window covered
  const txt=closed===0?'Open':closed===100?'Closed':closed+'% closed';
  v.innerHTML=closed===0||closed===100?'<b>'+txt+'</b>':'<b>'+closed+'%</b> closed';
  v.title='position '+p;
  f.style.height=closed+'%';
  w.setAttribute('aria-valuenow',p);w.setAttribute('aria-valuetext',txt);
}
function send(g){touched[g]=Date.now();post('/api/pos',{group:g,pos:cur[g]});}
function column(g){
  const td=types[g]==='top-down';
  const el=document.createElement('div');el.className='card col';
  el.innerHTML='<h3>Group '+g+'</h3><span class="type">'+(td?'Top-down':'Bottom-up')+'</span>'+
    '<span class="val" id="v'+g+'">—</span>'+
    '<button data-d="up">▲ Up</button>'+
    '<span class="end">'+(td?'OPEN':'CLOSED')+'</span>'+
    '<div class="win" id="w'+g+'" tabindex="0" role="slider" aria-orientation="vertical" aria-valuemin="0"'+
    ' aria-valuemax="255" aria-label="Group '+g+' position"><div class="fab '+(td?'td':'bu')+'" id="f'+g+'"></div></div>'+
    '<span class="end">'+(td?'CLOSED':'OPEN')+'</span>'+
    '<button data-d="down">▼ Down</button>';
  wire(el,g);
  const w=el.querySelector('.win');
  // Pointer: press anywhere in the window to put the rail there, drag, send on release.
  const at=e=>{const r=w.getBoundingClientRect();
    const f=Math.min(1,Math.max(0,(e.clientY-r.top)/r.height));
    touched[g]=Date.now();show(g,Math.round((1-f)*255));};
  w.onpointerdown=e=>{w.setPointerCapture(e.pointerId);w.classList.add('drag');at(e);};
  w.onpointermove=e=>{if(w.classList.contains('drag'))at(e);};
  const end=()=>{if(!w.classList.contains('drag'))return;w.classList.remove('drag');send(g);};
  w.onpointerup=end;w.onpointercancel=end;
  // Keyboard: arrows move the rail, PageUp/Down in bigger steps, Home/End to the limits;
  // sends 600 ms after the last key.
  let kt;
  w.onkeydown=e=>{
    const step={ArrowUp:8,ArrowRight:8,ArrowDown:-8,ArrowLeft:-8,PageUp:32,PageDown:-32}[e.key];
    let p=cur[g]>=0?cur[g]:128;
    if(step)p+=step;else if(e.key==='Home')p=255;else if(e.key==='End')p=0;else return;
    e.preventDefault();show(g,Math.min(255,Math.max(0,p)));touched[g]=Date.now();
    clearTimeout(kt);kt=setTimeout(()=>send(g),600);
  };
  return el;
}
fetch('/api/groups').then(r=>r.json()).catch(()=>[]).then(list=>{
  list.forEach(x=>types[x.group]=x.type);
  [1,2,3,4].forEach(g=>$('#groups').appendChild(column(g)));
  refresh();
});
$('#allcard').innerHTML='<div class="row"><h3>All groups</h3>'+
  '<button data-d="up">▲ Up</button><button data-d="down">▼ Down</button></div>'+
  '<p class="hint" style="margin-top:6px">Position is set per group.</p>';
wire($('#allcard'),'all');
$('#clear').onclick=()=>post('/api/clear',{});

fetch('/api/presets').then(r=>r.json()).then(list=>{
  list.forEach(p=>{
    const b=document.createElement('button');
    b.innerHTML=p.name[0].toUpperCase()+p.name.slice(1)+'<small>'+p.steps+'</small>';
    b.onclick=()=>post('/api/preset',{name:p.name});
    $('#presets').appendChild(b);
  });
});

const secs=ms=>(ms/1000).toFixed(1)+' s';
async function refresh(){
  let s;
  try{s=await (await fetch('/api/status')).json();}catch(e){$('#net').textContent='bridge not responding';return;}
  $('#net').textContent=s.wifi.ssid+' · '+s.wifi.rssi+' dBm · ch '+s.wifi.ch+' · '+s.wifi.ip;
  $('#dry').hidden=!s.dry_run;
  s.pos.forEach((p,i)=>{
    const g=i+1, w=$('#w'+g);
    if(!w||w.classList.contains('drag')||Date.now()-(touched[g]||0)<3000) return;  // don't fight the user's finger
    show(g,p);
  });
  $('#now').textContent=s.current?'Sending: '+s.current:'Idle';
  $('#bar').style.width=s.cur_total_ms?(100*(1-s.cur_left_ms/s.cur_total_ms))+'%':'0';
  $('#qinfo').textContent=s.busy?(s.queued+' queued · '+secs(s.remaining_ms)+' to go'):' ';
  $('#log').innerHTML=s.log.slice().reverse().map(l=>'<li>'+l.replace(/</g,'&lt;')+'</li>').join('');
}
setInterval(()=>{if(!document.hidden)refresh();},1000);
</script></body></html>)HTML";
