// AUTO-GENERATED from the pueski static site by scripts/gen-landing.js — do not edit by hand.
// @ts-nocheck
/* The original pueski page script runs verbatim below, wrapped so every global
   listener / rAF / timer it registers is tracked and torn down when the React
   route unmounts (otherwise the cursor handler etc. would leak onto other pages). */
export function initLanding(): () => void {
  const _cleanups = [];
  const _rafIds = [];
  const _intervalIds = [];
  const _timeoutIds = [];

  const _origDocAdd = document.addEventListener;
  const _origDocRem = document.removeEventListener;
  const _origWinAdd = window.addEventListener;
  const _origWinRem = window.removeEventListener;
  const _origRaf = window.requestAnimationFrame;
  const _origCaf = window.cancelAnimationFrame;
  const _origSi = window.setInterval;
  const _origCi = window.clearInterval;
  const _origSt = window.setTimeout;
  const _origCt = window.clearTimeout;

  document.addEventListener = function (t, f, o) {
    _origDocAdd.call(document, t, f, o);
    _cleanups.push(() => _origDocRem.call(document, t, f, o));
  };
  window.addEventListener = function (t, f, o) {
    _origWinAdd.call(window, t, f, o);
    _cleanups.push(() => _origWinRem.call(window, t, f, o));
  };
  window.requestAnimationFrame = function (cb) {
    const id = _origRaf.call(window, cb);
    _rafIds.push(id);
    return id;
  };
  window.setInterval = function (fn, ms) {
    const id = _origSi.call(window, fn, ms);
    _intervalIds.push(id);
    return id;
  };
  window.setTimeout = function (fn, ms) {
    const id = _origSt.call(window, fn, ms);
    _timeoutIds.push(id);
    return id;
  };

  try {
/* ===================== BEGIN VERBATIM PUESKI SCRIPT ===================== */

/* ============================================================
   CURSOR
   ============================================================ */
const dot = document.getElementById('cur-dot');
const ring = document.getElementById('cur-ring');
const crH = document.getElementById('cur-cross-h');
const crV = document.getElementById('cur-cross-v');
let mx=0,my=0,rx=0,ry=0;
document.addEventListener('mousemove', e=>{
  mx=e.clientX; my=e.clientY;
  dot.style.left=mx+'px'; dot.style.top=my+'px';
  crH.style.top=my+'px'; crV.style.left=mx+'px';
});
(function animRing(){
  rx+=(mx-rx)*.1; ry+=(my-ry)*.1;
  ring.style.left=rx+'px'; ring.style.top=ry+'px';
  requestAnimationFrame(animRing);
})();
document.addEventListener('mousedown', ()=>{ dot.style.width='8px'; dot.style.height='8px'; ring.style.width='20px'; ring.style.height='20px'; });
document.addEventListener('mouseup', ()=>{ dot.style.width='5px'; dot.style.height='5px'; ring.style.width='32px'; ring.style.height='32px'; });
document.querySelectorAll('button,a,.cap-card,.phase-tab,.risk-hd,.svc-card,.about-hl,.ref-metric,.plug-card,.client-cell,.contact-email,.sl-card,.sl-dl,.sl-feat').forEach(el=>{
  el.addEventListener('mouseenter',()=>{ ring.style.width='50px'; ring.style.height='50px'; ring.style.borderColor='rgba(0, 212, 255, .7)'; });
  el.addEventListener('mouseleave',()=>{ ring.style.width='32px'; ring.style.height='32px'; ring.style.borderColor='rgba(0, 212, 255, .45)'; });
});

/* ============================================================
   BACKGROUND CANVAS
   ============================================================ */
const bgC = document.getElementById('bg-canvas');
const bgX = bgC.getContext('2d');
let W,H;
function resizeBg(){ W=bgC.width=innerWidth; H=bgC.height=innerHeight; }
resizeBg(); addEventListener('resize', resizeBg);

class Particle {
  constructor(init){
    this.x=Math.random()*W;
    this.y=init?Math.random()*H:H+4;
    this.vy=-Math.random()*.4-.1;
    this.vx=(Math.random()-.5)*.08;
    this.life=0; this.max=Math.random()*500+200;
    this.r=Math.random()*.8+.3;
    this.cyan=Math.random()>.8;
  }
  tick(){ this.x+=this.vx; this.y+=this.vy; this.life++; }
  dead(){ return this.life>this.max||this.y<-4; }
  draw(){
    const a=Math.sin(this.life/this.max*Math.PI)*(this.cyan?.55:.25);
    bgX.globalAlpha=a;
    bgX.fillStyle=this.cyan?'#00d4ff':'#173f4f';
    bgX.beginPath(); bgX.arc(this.x,this.y,this.r,0,Math.PI*2); bgX.fill();
  }
}
const particles=[]; for(let i=0;i<150;i++) particles.push(new Particle(true));

function drawGrid(){
  bgX.strokeStyle='rgba(0, 212, 255, .018)'; bgX.lineWidth=1;
  const s=64;
  for(let x=0;x<W;x+=s){ bgX.beginPath(); bgX.moveTo(x,0); bgX.lineTo(x,H); bgX.stroke(); }
  for(let y=0;y<H;y+=s){ bgX.beginPath(); bgX.moveTo(0,y); bgX.lineTo(W,y); bgX.stroke(); }
}
function bgLoop(){
  bgX.clearRect(0,0,W,H); drawGrid();
  for(let i=particles.length-1;i>=0;i--){
    particles[i].tick();
    particles[i].draw();
    if(particles[i].dead()) particles[i]=new Particle(false);
  }
  bgX.globalAlpha=1;
  requestAnimationFrame(bgLoop);
}
bgLoop();

/* Mouse parallax on cards */
document.addEventListener('mousemove',e=>{
  document.querySelectorAll('.cap-card,.svc-card,.proj-card').forEach(c=>{
    const r=c.getBoundingClientRect();
    c.style.setProperty('--mx',((e.clientX-r.left)/r.width*100).toFixed(1)+'%');
    c.style.setProperty('--my',((e.clientY-r.top)/r.height*100).toFixed(1)+'%');
  });
});

/* ============================================================
   INTRO
   ============================================================ */
// Percentage counter
let pct=0;
const pctEl=document.getElementById('intro-pct');
const pctIv=setInterval(()=>{
  pct=Math.min(pct+(Math.random()*4+1),100);
  pctEl.textContent=String(Math.floor(pct)).padStart(3,'0')+'%';
  if(pct>=100){ clearInterval(pctIv); setTimeout(skipIntro,400); }
},40);

function skipIntro(){
  clearInterval(pctIv);
  var _intro=document.getElementById('intro'); if(!_intro) return; _intro.classList.add('out');
  setTimeout(()=>{
    document.getElementById('intro')?.remove();
    animCounters();
    observeReveal();
  }, 900);
}
// Auto-fire
setTimeout(skipIntro, 4800);

/* ============================================================
   HUD CLOCK
   ============================================================ */
function tickClock(){
  const n=new Date();
  document.getElementById('nav-time').textContent=
    `SYS:READY // ${String(n.getHours()).padStart(2,'0')}:${String(n.getMinutes()).padStart(2,'0')}:${String(n.getSeconds()).padStart(2,'0')}`;
}
setInterval(tickClock,1000); tickClock();

/* ============================================================
   MARQUEE
   ============================================================ */
const mqItems=['CH-1 Chorus','CP-1 Compressor','DL-1 Stereo delay','EQ-8 Equalizer','LT-1 LHigh end Limiter','LUPO Synthesizer','TS-1 Transient Shaper','CRV-1 Convolution Reverb','Mix Workflow','Master Bus Ready','Studio Session Stable','Modern Plugin UI','Precision DSP','Creative Sound Design','Low-Latency Engine'];
const mqEl=document.getElementById('mqi');
[...mqItems,...mqItems].forEach(t=>{ const d=document.createElement('div'); d.className='mq-item'; d.textContent=t; mqEl.appendChild(d); });

/* ============================================================
   COUNTERS
   ============================================================ */
function animCounters(){
  document.querySelectorAll('.counter').forEach(el=>{
    const t=+el.dataset.t; let v=0; const step=t/80;
    const iv=setInterval(()=>{ v=Math.min(v+step,t); el.textContent=Math.floor(v); if(v>=t)clearInterval(iv); },14);
  });
}

/* ============================================================
   THREE.JS WIREFRAME SPHERE
   ============================================================ */
(function initWF(){
  const canvas=document.getElementById('wf-canvas');
  if(!canvas||typeof THREE==='undefined') return;
  const R=new THREE.WebGLRenderer({canvas,antialias:true,alpha:true});
  R.setClearColor(0,0); R.setPixelRatio(devicePixelRatio);
  const S=new THREE.Scene();
  const cam=new THREE.PerspectiveCamera(52,1,.1,100);
  cam.position.set(0,0,3.8);

  function resize(){
    const w=canvas.parentElement.clientWidth, h=canvas.parentElement.clientHeight;
    R.setSize(w,h,false); cam.aspect=w/h; cam.updateProjectionMatrix();
  }
  resize(); addEventListener('resize',resize);

  const C=0x00d4ff;
  const matWF=new THREE.MeshBasicMaterial({color:C,wireframe:true,transparent:true,opacity:.16});
  const matE=new THREE.LineBasicMaterial({color:C,transparent:true,opacity:.5});
  const matOuter=new THREE.MeshBasicMaterial({color:C,wireframe:true,transparent:true,opacity:.05});
  const matOE=new THREE.LineBasicMaterial({color:C,transparent:true,opacity:.12});

  // Inner ico
  const icoG=new THREE.IcosahedronGeometry(1,1);
  const ico=new THREE.Mesh(icoG,matWF);
  const eL=new THREE.LineSegments(new THREE.EdgesGeometry(icoG),matE);
  ico.add(eL); S.add(ico);

  // Outer ico
  const outerG=new THREE.IcosahedronGeometry(1.6,1);
  const outer=new THREE.Mesh(outerG,matOuter);
  const oeL=new THREE.LineSegments(new THREE.EdgesGeometry(outerG),matOE);
  outer.add(oeL); S.add(outer);

  // Rings
  function addRing(r,ox,oz,a){
    const g=new THREE.TorusGeometry(r,.003,2,120);
    const m=new THREE.MeshBasicMaterial({color:C,transparent:true,opacity:a});
    const t=new THREE.Mesh(g,m); t.rotation.x=ox; t.rotation.z=oz; S.add(t); return t;
  }
  const r1=addRing(2,.0,0,.2);
  const r2=addRing(1.75,Math.PI/2.5,.3,.14);
  const r3=addRing(1.5,.8,-.4,.1);

  // Orbiting dots
  function addDot(sz,op){
    const g=new THREE.SphereGeometry(sz,8,8);
    const m=new THREE.MeshBasicMaterial({color:C,transparent:true,opacity:op});
    return new THREE.Mesh(g,m);
  }
  const d1=addDot(.05,1); S.add(d1);
  const d2=addDot(.035,.75); S.add(d2);
  const d3=addDot(.025,.5); S.add(d3);

  let tRY=0,tRX=0;
  addEventListener('mousemove',e=>{
    tRY=(e.clientX/innerWidth-.5)*.9;
    tRX=(e.clientY/innerHeight-.5)*.45;
  });

  let tt=0;
  function loop(){
    requestAnimationFrame(loop);
    tt+=.007;
    ico.rotation.y+=(tRY*.35-ico.rotation.y)*.04+.0025;
    ico.rotation.x+=(tRX*.2-ico.rotation.x)*.04+.0008;
    outer.rotation.y=-ico.rotation.y*.5+tt*.12;
    outer.rotation.x=ico.rotation.x*.3-tt*.04;
    r1.rotation.z=tt*.18;
    r2.rotation.y=tt*.25;
    r3.rotation.x=r3.rotation.x+.003;

    const b=1+Math.sin(tt*.7)*.045;
    ico.scale.setScalar(b);
    matE.opacity=.38+Math.sin(tt*1.1)*.12;

    d1.position.set(Math.cos(tt*.9)*2, Math.sin(tt*.9)*.35, Math.sin(tt*.45)*.5);
    d2.position.set(Math.cos(tt*.6+2)*1.75, Math.sin(tt*1.2)*.28, Math.sin(tt*.6+2)*1.75);
    d3.position.set(Math.cos(tt*1.3+4)*1.5, Math.sin(tt*.5)*.4, Math.sin(tt*1.3+4)*-.8);

    R.render(S,cam);
  }
  loop();
})();

/* ============================================================
   WAVEFORM
   ============================================================ */
const wfEl=document.getElementById('wavefm');
[4,7,13,9,17,11,5,15,8,11,13,5].forEach(h=>{ const b=document.createElement('div'); b.className='wb'; b.style.height=h+'px'; wfEl.appendChild(b); });
let wfTimer=null, playing=false, aud=null;

function animWF(){
  const bars=wfEl.querySelectorAll('.wb');
  let i=0;
  wfTimer=setInterval(()=>{ bars.forEach(b=>b.classList.remove('a')); bars[i%bars.length].classList.add('a'); i++; },100);
}

/* ============================================================
   AUDIO
   ============================================================ */
function fmtT(s){ const m=Math.floor(s/60); return `${m}:${String(Math.floor(s%60)).padStart(2,'0')}`; }

function loadAudio(src,name){
  if(aud){ aud.pause(); aud=null; }
  aud=new Audio(src);
  aud.volume=.7;
  aud.addEventListener('timeupdate',()=>{
    if(!aud.duration)return;
    const p=(aud.currentTime/aud.duration*100).toFixed(1);
    document.getElementById('ply-fill').style.width=p+'%';
    document.getElementById('ply-cur').textContent=fmtT(aud.currentTime);
  });
  aud.addEventListener('loadedmetadata',()=>{ document.getElementById('ply-dur').textContent=fmtT(aud.duration); });
  aud.addEventListener('ended',()=>{ playing=false; document.getElementById('ply-btn').textContent='▶'; document.getElementById('ply-btn').classList.remove('on'); clearInterval(wfTimer); });
  document.getElementById('ply-title').textContent=name.toUpperCase().replace(/\.[^.]+$/,'');
}

// Try auto-load music.mp3
window.addEventListener('load',()=>{
  const t=new Audio('/landing/audio/music.mp3');
  t.addEventListener('canplaythrough',()=>{ loadAudio('/landing/audio/music.mp3','music'); },{once:true});
  t.addEventListener('error',()=>{}); t.load();
});
document.getElementById('aud-file').addEventListener('change',e=>{
  const f=e.target.files[0]; if(!f) return;
  loadAudio(URL.createObjectURL(f),f.name);
  if(!playing) togglePlay();
});

function togglePlay(){
  if(!aud) return;
  if(playing){ aud.pause(); playing=false; document.getElementById('ply-btn').textContent='▶'; document.getElementById('ply-btn').classList.remove('on'); clearInterval(wfTimer); }
  else { aud.play(); playing=true; document.getElementById('ply-btn').textContent='⏸'; document.getElementById('ply-btn').classList.add('on'); animWF(); }
}
function setVol(v){ if(aud) aud.volume=+v; }
document.getElementById('ply-bar').addEventListener('click',e=>{
  if(!aud||!aud.duration) return;
  const r=e.currentTarget.getBoundingClientRect();
  aud.currentTime=((e.clientX-r.left)/r.width)*aud.duration;
});

// Autoplay on first interaction
let musicStarted=false;
function startMusic(){
  if(musicStarted||!aud) return;
  musicStarted=true;
  setTimeout(()=>{
    aud.play().then(()=>{ playing=true; document.getElementById('ply-btn').textContent='⏸'; document.getElementById('ply-btn').classList.add('on'); animWF(); }).catch(()=>{ musicStarted=false; });
  },200);
  document.removeEventListener('click',startMusic);
  document.removeEventListener('keydown',startMusic);
}
document.addEventListener('click',startMusic);
document.addEventListener('keydown',startMusic);

/* ============================================================
   SFX
   ============================================================ */
const sfx={in:null,out:null,click:null};
function sfxLoad(k,src,name){
  const a=new Audio(src); a.volume=.55;
  a.addEventListener('canplaythrough',()=>{
    sfx[k]=a;
    document.getElementById('sd-'+k).classList.add('loaded');
    document.getElementById('sf-'+k).textContent=name;
  },{once:true});
  a.addEventListener('error',()=>{},{once:true}); a.load();
}
function sfxPlay(k){
  if(!sfx[k]) return;
  const a=sfx[k].cloneNode(); a.volume=.55;
  const dot=document.getElementById('sd-'+k);
  dot.classList.add('playing');
  a.play().catch(()=>{});
  a.addEventListener('ended',()=>dot.classList.remove('playing'));
}
['in','out','click'].forEach(k=>{
  const fn=k==='in'?'slide_in.mp3':k==='out'?'slide_out.mp3':'click.mp3';
  sfxLoad(k,'/landing/audio/'+fn,fn);
  document.getElementById('sfi-'+k).addEventListener('change',e=>{
    const f=e.target.files[0]; if(!f) return;
    sfxLoad(k,URL.createObjectURL(f),f.name);
  });
});

/* ============================================================
   DATA
   ============================================================ */
const CAPS=[
  {ico:'',n:'01',t:'CH-1 Chorus',s:'Breite und Bewegung',d:'Klassischer Chorus mit modernem Stereo-Bild. Von subtiler Verdichtung bis zu breiten, lebendigen Modulationen mit musikalisch abgestimmten Bereichen.'},
  {ico:'',n:'02',t:'CP-1 Compressor',s:'Kontrolle mit Charakter',d:'Transparente bis charaktervolle Dynamikbearbeitung. Kurze Attack-Zeiten für Punch, sanfte Release-Kurven für musikalisches Pumping ohne Artefakte.'},
  {ico:'',n:'03',t:'EQ-8 Equalizer',s:'Präzise Frequenzarbeit',d:'Mehrband-EQ für chirurgische Korrekturen und tonale Formung. Saubere Filtercharakteristik und schnelles visuelles Feedback für Mix-Entscheidungen.'},
  {ico:'',n:'04',t:'LUPO Synthesizer',s:'Klangquelle mit Biss',d:'Flexibler Synth für Leads, Basses und Texturen. Direkter Zugriff auf Klangformung mit inspirierender Oberfläche und performanter Engine.'},
  {ico:'',n:'05',t:'CRV-1 Convolution Reverb',s:'Faltungshall mit Charakter',d:'Convolution-Reverb mit echten Impulsantworten für authentische Räume und natürliche Hallfahnen — von kleinen Rooms über große Hallen bis zu kreativen IRs.'},
  {ico:'',n:'06',t:'TS-1 Transient Shaper',s:'Attack und Sustain',d:'Präzise Kontrolle über Transienten — mehr Punch oder mehr Sustain per Knopfdruck. Funktioniert auf Drums, Percussion, Bass und nahezu jeder anderen Spur ohne Phasenprobleme.'},
  {ico:'',n:'07',t:'Bundle Workflow',s:'Einheitliche Bedienlogik',d:'Alle Plugins folgen einer gemeinsamen Bedienphilosophie. Parameter greifen schnell, Presets sind praxisnah und der Wechsel zwischen Effekten bleibt flüssig.'},
  {ico:'',n:'08',t:'Performance',s:'Für große Sessions',d:'Optimiert für stabile Projektsessions mit vielen Instanzen. Fokus auf effiziente DSP-Routinen und reproduzierbares Verhalten unter Last.'},
  {ico:'',n:'09',t:'Praxis-Testing',s:'Hörtests im Kontext',d:'Jeder Build wird in realen Mix- und Produktionsszenarien getestet. Entscheidungen basieren auf Klang und Workflow, nicht nur auf Messwerten.'},
  {ico:'',n:'09',t:'Bundle Release',s:'Sommer 2026',d:'Das komplette Paket erscheint im Sommer als Bundle. Der finale Verkaufspreis wird nach Abschluss der letzten Release-Tests bekanntgegeben.'},
];
const PHASES=[
  {n:'Konzept',s:'Sound Direction',c:'#0080ff',d:'Definition des klanglichen Ziels pro Plugin: musikalische Aufgabe, bevorzugte Einsatzfälle und gewünschtes Bediengefühl im Produktionsalltag.',t:['Use Cases','Control Layout','Gain Staging','Preset-Konzept','UX-Ziele']},
  {n:'DSP Design',s:'Algorithmik',c:'#00d4ff',d:'Entwicklung und Abstimmung der Kernalgorithmen mit Fokus auf musikalisches Verhalten, stabile Parameterfahrten und reproduzierbare Ergebnisse.',t:['Filterdesign','Dynamikmodelle','Modulation','Zeitbasierte FX','Numerische Stabilität']},
  {n:'UI & Workflow',s:'Produktivität',c:'#00ffee',d:'Schneller Zugriff auf alle relevanten Parameter mit konsistenter Oberfläche. Ziel ist ein Workflow, der kreative Entscheidungen unterstützt statt verlangsamt.',t:['GUI-Komposition','Visual Feedback','Macro-Controls','Preset Browser','Accessibility']},
  {n:'Validation',s:'Studio-Tests',c:'#ffaa00',d:'Intensive Tests in echten Sessions: CPU-Last, Latenzverhalten, Automationsstabilität und Host-Kompatibilität über verschiedene DAWs hinweg.',t:['CPU Profiling','Latency Checks','Automation Tests','DAW Matrix','Regression Tests']},
  {n:'Release',s:'Bundle Launch',c:'#00ff88',d:'Finale Qualitätsfreigabe, Packaging und Dokumentation für den Bundle-Launch im Sommer. Preisfestlegung erfolgt nach Abschluss der finalen Testphase.',t:['Final QA','Installer','Handbuch','Preset Pack','Launch Assets']},
];
const RISKS=[
  {n:'Klangliche Maskierung',lv:'Hoch',lc:'lh',c:'#dd0022',d:'Dichte Effekte können zentrale Mix-Informationen verdecken. Parameterbereiche werden so ausgelegt, dass musikalische Kontrolle auch in komplexen Arrangements erhalten bleibt.'},
  {n:'Überkompression',lv:'Hoch',lc:'lh',c:'#dd0022',d:'Zu aggressive Dynamikbearbeitung zerstört Transienten und Groove. CP-1 wird mit klaren Sweet-Spots und kontrolliertem Knee-Verhalten abgestimmt.'},
  {n:'CPU-Spitzen',lv:'Mittel',lc:'lm',c:'#ffaa00',d:'Mehrere Instanzen in großen Sessions können Lastspitzen erzeugen. Kontinuierliches Profiling und Optimierung sind feste Bestandteile des Release-Prozesses.'},
  {n:'Preset-Überfrachtung',lv:'Mittel',lc:'lm',c:'#ffaa00',d:'Zu viele Presets ohne klare Struktur verlangsamen den Workflow. Fokus liegt auf kuratierten, schnell nutzbaren Startpunkten pro Anwendung.'},
  {n:'Host-Inkompatibilität',lv:'Strukturell',lc:'ls',c:'#00d4ff',d:'Unterschiedliches Verhalten zwischen DAWs muss früh erkannt werden. Deshalb werden alle Builds gegen eine definierte DAW-Matrix gegengeprüft.'},
  {n:'Release Timing',lv:'Strukturell',lc:'ls',c:'#00d4ff',d:'Das Bundle ist für den Sommer geplant. Finale Termin- und Preisfreigabe erfolgen erst nach Abschluss der letzten QA-Runde.'},
];

/* ============================================================
   RENDER CAPS
   ============================================================ */
const cg=document.getElementById('caps-grid');
let selCap=-1;
CAPS.forEach((c,i)=>{
  const el=document.createElement('div');
  el.className='cap-card rev'+(i%3===1?' rd1':i%3===2?' rd2':'');
  el.innerHTML=`<div class="card-num">${c.n} // ${c.t.toUpperCase().replace(' ','').slice(0,8)}</div><div class="card-ico">${c.ico}</div><div class="card-t">${c.t}</div><div class="card-s">${c.s}</div><div class="card-arr"><span>Details</span><span>›</span></div>`;
  el.onclick=()=>{
    sfxPlay('in');
    const det=document.getElementById('detail');
    if(selCap===i){ closeDetail(); return; }
    selCap=i;
    document.querySelectorAll('.cap-card').forEach(x=>x.classList.remove('sel'));
    el.classList.add('sel');
    document.getElementById('det-title').textContent=c.t;
    document.getElementById('det-body').textContent=c.d;
    det.classList.remove('open');
    void det.offsetHeight;
    det.classList.add('open');
    det.scrollIntoView({behavior:'smooth',block:'nearest'});
  };
  cg.appendChild(el);
});
function closeDetail(){ selCap=-1; document.querySelectorAll('.cap-card').forEach(x=>x.classList.remove('sel')); document.getElementById('detail').classList.remove('open'); sfxPlay('out'); }

/* ============================================================
   RENDER PHASES
   ============================================================ */
const ptabs=document.getElementById('phase-tabs');
PHASES.forEach((p,i)=>{
  const b=document.createElement('button');
  b.className='phase-tab'+(i===0?' on':'');
  b.innerHTML=`<div class="tab-n">0${i+1}</div><div class="tab-dot" style="background:${p.c};box-shadow:0 0 6px ${p.c}"></div><div class="tab-name">${p.n}</div><div class="tab-sub">${p.s}</div>`;
  b.onclick=()=>{ sfxPlay('click'); document.querySelectorAll('.phase-tab').forEach(x=>x.classList.remove('on')); b.classList.add('on'); renderPhase(p); };
  ptabs.appendChild(b);
});
function renderPhase(p){
  const el=document.getElementById('phase-panel');
  el.style.animation='none'; el.offsetHeight; el.style.animation='secIn .3s ease';
  el.innerHTML=`<h3>${p.n}</h3><p>${p.d}</p><div class="phase-tags">${p.t.map(x=>`<span class="phase-tag">${x}</span>`).join('')}</div>`;
}
renderPhase(PHASES[0]);

/* ============================================================
   RENDER RISKS
   ============================================================ */
const rl=document.getElementById('risk-list');
RISKS.forEach(r=>{
  const row=document.createElement('div'); row.className='risk-row';
  row.innerHTML=`<div class="risk-hd"><div class="risk-pip" style="background:${r.c};box-shadow:0 0 5px ${r.c}"></div><div class="risk-name">${r.n}</div><span class="risk-badge ${r.lc}">${r.lv}</span><span class="risk-chv">›</span></div><div class="risk-body"><p>${r.d}</p></div>`;
  row.querySelector('.risk-hd').onclick=()=>{
    sfxPlay('click');
    const open=row.classList.contains('open');
    row.classList.toggle('open',!open);
    row.querySelector('.risk-body').classList.toggle('open',!open);
  };
  rl.appendChild(row);
});

/* ============================================================
   NAVIGATION
   ============================================================ */
function showSec(id, btn){
  sfxPlay('out');
  document.querySelectorAll('.section').forEach(s=>{ s.classList.remove('on'); });
  document.querySelectorAll('.nav-link').forEach(b=>b.classList.remove('on'));
  document.getElementById('sec-'+id).classList.add('on');
  // Mark matching links in both desktop nav and mobile drawer
  document.querySelectorAll('.nav-link').forEach(b=>{
    const cb = b.getAttribute('onclick')||'';
    if(cb.includes("'"+id+"'")) b.classList.add('on');
  });
  scrollTo({top:0,behavior:'smooth'});
  setTimeout(()=>{ sfxPlay('in'); observeReveal(); animCounters(); }, 100);
}

function toggleBurger(){
  const burger=document.getElementById('nav-burger');
  const drawer=document.getElementById('nav-drawer');
  const open=burger.classList.toggle('open');
  drawer.classList.toggle('open', open);
  burger.setAttribute('aria-expanded', open);
}
function closeBurger(){
  document.getElementById('nav-burger').classList.remove('open');
  document.getElementById('nav-drawer').classList.remove('open');
  document.getElementById('nav-burger').setAttribute('aria-expanded','false');
}

/* ============================================================
   SCROLL REVEAL
   ============================================================ */
function observeReveal(){
  const obs=new IntersectionObserver(es=>{
    es.forEach(e=>{ if(e.isIntersecting){ e.target.classList.add('vis'); obs.unobserve(e.target); } });
  },{threshold:.08});
  document.querySelectorAll('.rev:not(.vis)').forEach(el=>obs.observe(el));
}
observeReveal();

    // Expose handlers referenced by inline onclick/oninput in the markup. MUST be
    // inside this try block: in strict-mode ES modules, function declarations in a
    // block are block-scoped, so the pueski script's functions are only visible here.
    Object.assign(window, {
      skipIntro, showSec, toggleBurger, closeBurger, sfxPlay, togglePlay, setVol, closeDetail,
    });
/* ====================== END VERBATIM PUESKI SCRIPT ====================== */
  } catch (e) {
    console.error("landing init error", e);
  }

  return function teardown() {
    window.requestAnimationFrame = _origRaf;
    window.setInterval = _origSi;
    window.setTimeout = _origSt;
    document.addEventListener = _origDocAdd;
    window.addEventListener = _origWinAdd;
    _rafIds.forEach((id) => { try { _origCaf.call(window, id); } catch (e) {} });
    _intervalIds.forEach((id) => { try { _origCi.call(window, id); } catch (e) {} });
    _timeoutIds.forEach((id) => { try { _origCt.call(window, id); } catch (e) {} });
    _cleanups.forEach((fn) => { try { fn(); } catch (e) {} });
    ["skipIntro","showSec","toggleBurger","closeBurger","sfxPlay","togglePlay","setVol","closeDetail"]
      .forEach((k) => { try { delete window[k]; } catch (e) {} });
  };
}
