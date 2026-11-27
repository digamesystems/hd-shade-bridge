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
/* `hidden` must win over any display rule (the time-zone banner is display:flex). */
[hidden]{display:none!important}
body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main{max-width:760px;margin:0 auto;padding:16px 16px 40px}
header{display:flex;flex-wrap:wrap;align-items:baseline;justify-content:space-between;gap:4px 12px;margin:4px 0 12px}
h1{font-size:21px;margin:0;letter-spacing:-.01em}
.net{color:var(--mute);font-size:13px}
.banner{background:var(--warn-bg);color:var(--warn);border-radius:10px;padding:8px 12px;font-size:13px;margin-bottom:14px}
h2{font-size:12px;font-weight:600;text-transform:uppercase;letter-spacing:.07em;color:var(--mute);margin:22px 0 8px}
.h2row{display:flex;align-items:center;justify-content:space-between;margin:22px 0 8px}
.h2row h2{margin:0}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:12px 14px}
.row{display:flex;align-items:center;gap:8px}
.card h3{font-size:16px;margin:0;flex:1}
button{font:inherit;border:1px solid var(--line);background:var(--bg);color:var(--ink);border-radius:10px;
padding:9px 14px;min-width:66px;min-height:42px;cursor:pointer;touch-action:manipulation}
button:hover{border-color:var(--mute)}
button:active{transform:translateY(1px)}
button.ghost{background:none;min-width:0;padding:6px 10px;min-height:0;font-size:13px}
button.pri{background:var(--acc);border-color:var(--acc);color:var(--acc-ink);font-weight:600}
.cols{display:grid;gap:8px;grid-template-columns:repeat(4,minmax(0,1fr))}
.col{display:flex;flex-direction:column;align-items:center;gap:6px;padding:12px 8px}
.col h3{flex:none;font-size:15px;white-space:nowrap}
.type{font-size:11px;color:var(--mute);margin-top:-4px;text-align:center}
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
.val{font-variant-numeric:tabular-nums;font-size:13px;color:var(--mute);white-space:nowrap}
.col .val{font-size:15px;font-weight:600;color:var(--ink)}
.hint{color:var(--mute);font-size:12px;margin:10px 0 0}
.presets{display:grid;gap:8px;grid-template-columns:repeat(auto-fill,minmax(150px,1fr))}
.presets button{text-align:left;padding:10px 12px;overflow-wrap:anywhere}
.presets small{display:block;color:var(--mute);font-size:12px;margin-top:2px}
.presets .when{color:var(--acc)}
.pedit{display:flex;flex-direction:column;gap:4px;padding:10px 12px}
.pedit .pname{font-weight:600;overflow-wrap:anywhere}
.pedit small{color:var(--mute);font-size:12px}
.pedit .row{flex-wrap:wrap;gap:6px;margin-top:4px}
.pedit input[type=time]{font:inherit;color:var(--ink);background:var(--bg);border:1px solid var(--line);
border-radius:8px;padding:6px 8px}
.pedit label.on{display:flex;align-items:center;gap:4px;font-size:13px}
.pedit .del{margin-left:auto;color:var(--err);border-color:var(--err)}
.tzb{display:flex;flex-wrap:wrap;align-items:center;gap:8px;margin-bottom:8px}
.empty{color:var(--mute);font-size:13px}
details.save{margin-top:10px;padding:0}
details.save summary{padding:12px 14px;cursor:pointer;font-weight:600}
details.save form{padding:0 14px 14px}
label.name{display:block;font-size:13px;color:var(--mute)}
input[type=text]{display:block;width:100%;margin-top:4px;font:inherit;color:var(--ink);background:var(--bg);
border:1px solid var(--line);border-radius:8px;padding:9px 10px}
.pick{display:flex;flex-wrap:wrap;gap:8px 16px;margin:12px 0}
.pick label{display:flex;align-items:center;gap:6px;white-space:nowrap}
.pick label.off{color:var(--mute)}
.pick input{width:18px;height:18px;margin:0}
.err{color:var(--err);font-size:13px;min-height:1.2em;margin-top:6px}
.bar{height:6px;background:var(--line);border-radius:3px;overflow:hidden;margin:10px 0 4px}
.bar i{display:block;height:100%;width:0;background:var(--acc);transition:width .4s linear}
#now{font-weight:600}
ol{list-style:none;margin:10px 0 0;padding:10px 0 0;border-top:1px solid var(--line);
font:12px/1.55 ui-monospace,Consolas,monospace;color:var(--mute);max-height:230px;overflow:auto}
footer{margin-top:18px;text-align:center;font-size:11px;color:var(--mute);font-variant-numeric:tabular-nums}
</style></head><body><main>
<header><h1>Shade bridge</h1><span class="net" id="net">connecting…</span></header>
<div class="banner" id="dry">Dry run (<span id="why">no radio</span>): commands go through the real send
queue and the frames are printed on the serial console instead of transmitted.</div>

<h2>Groups</h2>
<div class="cols" id="groups"></div>
<p class="hint">Drag a shade's rail (or tap where you want it) and let go to send; each move takes about 1.8 s
on air. The percentage is how much of the window the fabric covers. It shows what was last sent: the shades
don't report back.</p>

<div class="h2row"><h2>Presets</h2><button class="ghost" id="edit">Edit</button></div>
<div class="banner tzb" id="tzb" hidden><span>Schedules need this bridge's time zone.</span>
<button class="ghost" id="tzset">Use this device's</button><span class="err" id="tzerr"></span></div>
<div class="presets" id="presets"></div>
<p class="hint" id="edithint" hidden>Give a preset a time and tick Daily to run it every day at that time; clear the
time to remove it. Tap Done when finished.</p>
<details class="card save" id="save">
<summary>Save current positions as a preset</summary>
<form id="saveform" autocomplete="off">
<label class="name">Name<input type="text" id="pname" maxlength="24" placeholder="e.g. Movie night"></label>
<div class="pick" id="pick"></div>
<button type="submit" class="pri">Save preset</button>
<div class="err" id="saveerr"></div>
<p class="hint">Saves the positions last sent to the ticked groups. Untick a group to leave it alone when the
preset runs.</p>
</form>
</details>

<h2>Activity</h2>
<div class="card">
<div class="row"><span id="now" style="flex:1">Idle</span><button class="ghost" id="clear">Clear queue</button></div>
<div class="bar"><i id="bar"></i></div>
<div class="val" id="qinfo">&nbsp;</div>
<div class="val" id="waiting" style="white-space:pre-line"></div>
<div class="err" id="msg"></div>
<ol id="log"></ol>
</div>
<footer id="ver"></footer>
</main>
<script>
const $=s=>document.querySelector(s);
const G=[1,2,3,4];
const touched={};
// POST a form; errors go to `errEl` (the Activity card's line by default). Returns the reply.
async function post(path,data,errEl){
  const el=errEl||$('#msg');el.textContent='';
  let j;
  try{
    const r=await fetch(path,{method:'POST',body:new URLSearchParams(data)});
    j=await r.json().catch(()=>({ok:r.ok}));
    if(!j.ok) el.textContent=j.error||('Request failed ('+r.status+')');
  }catch(e){j={ok:false};el.textContent='No response from the bridge';}
  refresh();
  return j;
}
const types={};  // group -> 'top-down' | 'bottom-up', from /api/groups
// Share of the window the fabric covers, 0..100, for rail height p (0 bottom .. 255 top).
const pct=(g,p)=>Math.round(100*(types[g]==='top-down'?255-p:p)/255);
// The rail height for exactly the whole percentage p shows, so two shades that
// read the same % get the same position (and presets saved from them share a
// frame on air). pct(g,snap(g,p)) === pct(g,p).
const snap=(g,p)=>{const r=Math.round(pct(g,p)*255/100);return types[g]==='top-down'?255-r:r;};
const cur={};    // group -> position shown (rail height 0..255), -1 unknown
function show(g,p){
  const v=$('#v'+g), f=$('#f'+g), w=$('#w'+g);
  cur[g]=p;
  if(p<0){v.textContent='?';v.title='Position unknown';f.style.height='0';
    w.removeAttribute('aria-valuenow');w.removeAttribute('aria-valuetext');return;}
  const c=pct(g,p);
  v.textContent=c+'%';
  v.title=c+'% closed (position '+p+')';
  f.style.height=c+'%';
  w.setAttribute('aria-valuenow',p);w.setAttribute('aria-valuetext',c+'% closed');
}
function send(g){touched[g]=Date.now();post('/api/pos',{group:g,pos:cur[g]});}
function column(g){
  const td=types[g]==='top-down';
  const el=document.createElement('div');el.className='card col';
  el.innerHTML='<h3>GRP '+g+'</h3><span class="type">'+(td?'Top-down':'Bottom-up')+'</span>'+
    '<span class="val" id="v'+g+'">—</span>'+
    '<div class="win" id="w'+g+'" tabindex="0" role="slider" aria-orientation="vertical" aria-valuemin="0"'+
    ' aria-valuemax="255" aria-label="GRP '+g+' position"><div class="fab '+(td?'td':'bu')+'" id="f'+g+'"></div></div>';
  const w=el.querySelector('.win');
  // Pointer: press anywhere in the window to put the rail there, drag, send on release.
  // Snapped to whole percentages (snap above).
  const at=e=>{const r=w.getBoundingClientRect();
    const f=Math.min(1,Math.max(0,(e.clientY-r.top)/r.height));
    touched[g]=Date.now();show(g,snap(g,Math.round((1-f)*255)));};
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
    e.preventDefault();show(g,snap(g,Math.min(255,Math.max(0,p))));touched[g]=Date.now();
    clearTimeout(kt);kt=setTimeout(()=>send(g),600);
  };
  return el;
}

// --- presets ---
let presets=[], editing=false;
// "G4 100%, G2 0%": each step's groups at that step's position.
const stepText=steps=>steps.map(([m,p])=>G.filter(g=>m&(1<<(g-1))).map(g=>'G'+g+' '+pct(g,p)+'%').join(', ')).join(', ');
// "22:30" -> "10:30 PM", in the phone's own clock style.
const clock=t=>{const [h,m]=t.split(':').map(Number);
  return new Date(2000,0,1,h,m).toLocaleTimeString([],{hour:'numeric',minute:'2-digit'});};
function renderPresets(){
  const box=$('#presets');box.textContent='';
  if(!presets.length){const e=document.createElement('div');e.className='empty';
    e.textContent='No presets yet. Set the shades above, then save their positions below.';box.appendChild(e);}
  presets.forEach(p=>{
    if(editing){box.appendChild(editCard(p));return;}
    const b=document.createElement('button'), s=document.createElement('small');
    b.append(p.name);s.textContent=stepText(p.steps);b.appendChild(s);
    if(p.time&&p.on){const w=document.createElement('small');w.className='when';
      w.textContent='⏰ '+clock(p.time)+' daily';b.appendChild(w);}
    b.onclick=()=>post('/api/preset',{name:p.name});
    box.appendChild(b);
  });
}
// Edit mode: a preset's daily time, on or off, and Delete.
function editCard(p){
  const c=document.createElement('div');c.className='card pedit';
  const n=document.createElement('div');n.className='pname';n.textContent=p.name;
  const s=document.createElement('small');s.textContent=stepText(p.steps);
  const row=document.createElement('div');row.className='row';
  const t=document.createElement('input');t.type='time';t.value=p.time||'';
  t.setAttribute('aria-label','Daily time for '+p.name);
  const l=document.createElement('label');l.className='on';
  const on=document.createElement('input');on.type='checkbox';on.checked=!!p.time&&p.on;on.disabled=!p.time;
  l.append(on,' Daily');
  const d=document.createElement('button');d.className='ghost del';d.textContent='Delete';
  const err=document.createElement('div');err.className='err';
  const save=async()=>{if((await post('/api/preset/schedule',{name:p.name,time:t.value,on:on.checked?1:0},err)).ok)loadPresets();};
  t.onchange=()=>{if(t.value&&!p.time)on.checked=true;save();};
  on.onchange=save;
  d.onclick=async()=>{if(!confirm('Delete preset "'+p.name+'"?'))return;
    if((await post('/api/preset/delete',{name:p.name},err)).ok)loadPresets();};
  row.append(t,l,d);c.append(n,s,row,err);
  return c;
}
// One tap: this phone's time zone (an IANA name such as America/Los_Angeles).
const myZone=(()=>{try{return Intl.DateTimeFormat().resolvedOptions().timeZone||'';}catch(e){return '';}})();
$('#tzset').textContent='Use this device’s ('+(myZone||'unknown')+')';
$('#tzset').onclick=()=>post('/api/tz',{zone:myZone},$('#tzerr'));
function loadPresets(){
  return fetch('/api/presets').then(r=>r.json()).then(list=>{presets=list;renderPresets();}).catch(()=>{});
}
$('#edit').onclick=()=>{
  editing=!editing;$('#edit').textContent=editing?'Done':'Edit';$('#edithint').hidden=!editing;renderPresets();
};
$('#pick').innerHTML=G.map(g=>'<label id="pl'+g+'"><input type="checkbox" id="pk'+g+'" checked> GRP '+g+
  ' <span class="val" id="pv'+g+'"></span></label>').join('');
// Tick boxes follow what's known: a group with no position sent can't be saved.
function updatePick(pos){
  pos.forEach((p,i)=>{
    const g=i+1, k=$('#pk'+g), known=p>=0;
    if(!known)k.checked=false;else if(k.disabled)k.checked=true;
    k.disabled=!known;$('#pl'+g).classList.toggle('off',!known);
    $('#pv'+g).textContent=known?pct(g,p)+'%':'not set';
  });
}
$('#saveform').onsubmit=async e=>{
  e.preventDefault();
  const err=$('#saveerr'), name=$('#pname').value.trim();
  let groups=0;G.forEach(g=>{if($('#pk'+g).checked)groups|=1<<(g-1);});
  if(!name){err.textContent='Enter a name';return;}
  if(!groups){err.textContent='Tick at least one group';return;}
  const data={name,groups};
  const old=presets.find(p=>p.name.toLowerCase()===name.toLowerCase());
  if(old){if(!confirm('Replace preset "'+old.name+'"?'))return;data.replace=1;}
  if((await post('/api/preset/save',data,err)).ok){
    $('#pname').value='';$('#save').open=false;loadPresets();
  }
};

fetch('/api/groups').then(r=>r.json()).catch(()=>[]).then(list=>{
  list.forEach(x=>types[x.group]=x.type);
  G.forEach(g=>$('#groups').appendChild(column(g)));
  loadPresets();  // after the types: preset steps are shown as %
  refresh();
});
$('#clear').onclick=()=>post('/api/clear',{});

const secs=ms=>(ms/1000).toFixed(1)+' s';
async function refresh(){
  let s;
  try{s=await (await fetch('/api/status')).json();}catch(e){$('#net').textContent='bridge not responding';return;}
  $('#net').textContent=s.wifi.host+' · '+s.wifi.ssid+' · '+s.wifi.rssi+' dBm · ch '+s.wifi.ch+' · '+s.wifi.ip;
  $('#dry').hidden=!s.dry_run;
  $('#tzb').hidden=!!s.tz;
  $('#ver').textContent='Shade bridge '+s.version+' · radio '+s.radio+' · '+
    (!s.tz?'time zone not set':s.time?'bridge time '+clock(s.time):'waiting for the time');
  $('#why').textContent=s.radio==='off'?'transmit turned off on serial':(s.radio||'no radio');
  s.pos.forEach((p,i)=>{
    const g=i+1, w=$('#w'+g);
    if(!w||w.classList.contains('drag')||Date.now()-(touched[g]||0)<3000) return;  // don't fight the user's finger
    show(g,p);
  });
  updatePick(s.pos);
  $('#now').textContent=s.current?'Sending: '+s.current:'Idle';
  $('#bar').style.width=s.cur_total_ms?(100*(1-s.cur_left_ms/s.cur_total_ms))+'%':'0';
  $('#qinfo').textContent=s.busy?(s.queued+' queued · '+secs(s.remaining_ms)+' to go'):' ';
  $('#waiting').textContent=(s.waiting||[]).map((l,i)=>(i+1)+'. '+l).join('\n');
  $('#log').innerHTML=s.log.slice().reverse().map(l=>'<li>'+l.replace(/</g,'&lt;')+'</li>').join('');
}
setInterval(()=>{if(!document.hidden)refresh();},1000);
</script></body></html>)HTML";
