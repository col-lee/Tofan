/* ToFan portal app bundle. Generated from local ES modules. */

const __tofan_model=(()=>{
const widgets = {network:'เครือข่าย',storage:'พื้นที่จัดเก็บ',music:'กำลังเล่น',volume:'ระดับเสียง',voice:'Voice assistant',system:'ระบบ'};
const snapWidth=n=>Math.max(3,Math.min(12,Math.round((Number(n)||6)/3)*3));
const snapHeight=n=>220+Math.max(0,Math.min(5,Math.round(((Number(n)||220)-220)/80)))*80;
const defaults = () => Object.keys(widgets).map(id=>({id,w:6,h:220}));
function layout(value){
 if(!Array.isArray(value))return defaults();const seen=new Set();
 return value.filter(x=>x&&widgets[x.id]&&!seen.has(x.id)&&seen.add(x.id)).map(x=>({id:x.id,w:snapWidth(x.w),h:snapHeight(x.h)}));
}
function rect(sw,sh,w,h,mode='cover',zoom=1,panX=0,panY=0){
 if(![sw,sh,w,h,zoom].every(n=>Number.isFinite(n)&&n>0))throw Error('ขนาดภาพไม่ถูกต้อง');
 const scale=(mode==='contain'?Math.min(w/sw,h/sh):Math.max(w/sw,h/sh))*zoom;
 const width=sw*scale,height=sh*scale;
 return {x:(w-width)/2+Math.max(0,(width-w)/2)*Math.max(-1,Math.min(1,panX)),y:(h-height)/2+Math.max(0,(height-h)/2)*Math.max(-1,Math.min(1,panY)),width,height};
}
function dimensions(w,h){return Number.isInteger(w)&&Number.isInteger(h)&&w>0&&h>0&&w<=4096&&h<=4096&&w*h<=4194304;}
const bytes=n=>n>=1073741824?`${(n/1073741824).toFixed(1)} GB`:n>=1048576?`${(n/1048576).toFixed(1)} MB`:n>=1024?`${(n/1024).toFixed(1)} KB`:`${n||0} B`;
const time=n=>`${Math.floor((n||0)/60).toString().padStart(2,'0')}:${((n||0)%60).toString().padStart(2,'0')}`;

const playableAudio=name=>/\.(mp3|wav|aac|m4a|flac)$/i.test(String(name||''));
function mediaDirectory(name){
 const value=String(name||'');
 if(/\.(jpe?g|png|gif)$/i.test(value))return 'Pictures';
 if(playableAudio(value))return 'Musics';
 if(/\.(mp4|webm|mov|m4v|avi|mjpeg|mjpg)$/i.test(value))return 'Videos';
 return 'Files';
}
function directAudioNameFromUrl(value){
 try{const u=new URL(value);const part=decodeURIComponent(u.pathname.split('/').pop()||'').trim();return playableAudio(part)?part:'';}catch{return '';}
}
function blockedYouTubeUrl(value){
 try{const host=new URL(value).hostname.toLowerCase();return host==='youtu.be'||host.endsWith('.youtu.be')||host==='youtube.com'||host.endsWith('.youtube.com')||host.endsWith('.youtube-nocookie.com')||host.endsWith('.googlevideo.com');}catch{return false;}
}
return {widgets,snapWidth,snapHeight,defaults,layout,rect,dimensions,bytes,time,playableAudio,mediaDirectory,directAudioNameFromUrl,blockedYouTubeUrl};
})();

const __tofan_crop=(()=>{
const {rect,dimensions}=__tofan_model;
function cropControls({editor,screen,read,change,source}){
 const preset=document.createElement('label');preset.textContent='ขนาดภาพที่ต้องการ';
 const select=document.createElement('select');select.id='cropPreset';
 for(const [value,label] of [['screen','เต็มจออุปกรณ์'],['320x240','320 × 240'],['custom','กำหนดเอง']])select.add(new Option(label,value));
 preset.append(select);editor.prepend(preset);
 const overview=document.createElement('canvas');overview.id='cropSource';overview.className='crop-source';overview.tabIndex=0;overview.setAttribute('aria-label','ตำแหน่งครอป ลากกรอบหรือใช้ปุ่มลูกศร');
 preset.after(overview);
 const hint=document.createElement('p');hint.className='hint';hint.textContent='ลากกรอบเพื่อเลือกส่วนที่ต้องการ · ใช้ซูมเพื่อย่อกรอบ · ภาพด้านล่างคือตัวอย่างผลลัพธ์';overview.after(hint);
 const animationHint=document.createElement('p');animationHint.className='hint';animationHint.textContent='GIF / MJPEG แสดงตัวอย่างเฟรมแรก และครอปตำแหน่งเดียวกันทุกเฟรมก่อนบีบอัด';hint.after(animationHint);
 const w=editor.querySelector('#outW'),h=editor.querySelector('#outH');
 select.onchange=()=>{if(select.value!=='custom'){[w.value,h.value]=select.value==='screen'?screen():[320,240];change();}};
 for(const input of [w,h])input.addEventListener('input',()=>{select.value='custom';});
 let geometry=null;
 function render(){
  const image=source(),o=read();if(!image||!dimensions(o.w,o.h))return;
  const sw=image.videoWidth||image.naturalWidth||image.width,sh=image.videoHeight||image.naturalHeight||image.height;
  const scale=Math.min(640/sw,360/sh,1);overview.width=Math.max(1,Math.round(sw*scale));overview.height=Math.max(1,Math.round(sh*scale));
  const ctx=overview.getContext('2d'),r=rect(sw,sh,o.w,o.h,o.mode,o.zoom,o.panX,o.panY);
  const factor=overview.width/r.width,x=-r.x*factor,y=-r.y*factor,cw=o.w*factor,ch=o.h*factor;
  ctx.drawImage(image,0,0,overview.width,overview.height);ctx.fillStyle='#102c3880';ctx.fillRect(0,0,overview.width,overview.height);
  ctx.save();ctx.beginPath();ctx.rect(x,y,cw,ch);ctx.clip();ctx.drawImage(image,0,0,overview.width,overview.height);ctx.restore();
  ctx.strokeStyle='#fff';ctx.lineWidth=2;ctx.strokeRect(x,y,cw,ch);ctx.lineWidth=1;
  for(let n=1;n<3;n++){ctx.beginPath();ctx.moveTo(x+cw*n/3,y);ctx.lineTo(x+cw*n/3,y+ch);ctx.moveTo(x,y+ch*n/3);ctx.lineTo(x+cw,y+ch*n/3);ctx.stroke();}
  geometry={r,factor};
 }
 overview.onpointerdown=e=>{
  if(!geometry)return;e.preventDefault();overview.setPointerCapture(e.pointerId);
  const start=read(),g=geometry,x=e.clientX,y=e.clientY,scale=overview.width/overview.getBoundingClientRect().width;
  overview.onpointermove=e=>{const dx=(e.clientX-x)*scale/g.factor,dy=(e.clientY-y)*scale/g.factor;
   change({panX:start.panX-dx/Math.max(1,(g.r.width-start.w)/2),panY:start.panY-dy/Math.max(1,(g.r.height-start.h)/2)});
  };
 };
 overview.onpointerup=overview.onpointercancel=overview.onlostpointercapture=()=>overview.onpointermove=null;
 overview.onkeydown=e=>{const d={ArrowLeft:[.1,0],ArrowRight:[-.1,0],ArrowUp:[0,.1],ArrowDown:[0,-.1]}[e.key];if(d){e.preventDefault();const o=read();change({panX:o.panX+d[0],panY:o.panY+d[1]});}};
 return {render,select};
}
return {cropControls};
})();

const __tofan_mjpeg=(()=>{
// Raw concatenated JPEG frames. Segment lengths protect APP/EXIF data from
// being mistaken for image boundaries; entropy escapes and restart markers stay intact.
function* mjpegFrames(bytes) {
 let p=0,count=0;
 while(p<bytes.length){
  while(p<bytes.length && [0,9,10,13,32].includes(bytes[p]))p++;
  if(p===bytes.length)break;
  const start=p;
  if(bytes[p++]!==255||bytes[p++]!==216)throw Error('Invalid MJPEG frame header');
  let scan=false,done=false;
  while(p<bytes.length){
   if(scan){while(p<bytes.length&&bytes[p]!==255)p++;}
   if(bytes[p++]!==255)throw Error('Invalid JPEG marker');
   while(bytes[p]===255)p++;
   const marker=bytes[p++];
   if(scan&&(marker===0||(marker>=208&&marker<=215)))continue;
   if(marker===217){done=true;break;}
   if(marker===216||marker===undefined)throw Error('Truncated MJPEG frame');
   if(marker===1)continue;
   if(p+2>bytes.length)throw Error('Truncated JPEG segment');
   const size=bytes[p]*256+bytes[p+1];
   if(size<2||p+size>bytes.length)throw Error('Invalid JPEG segment length');
   p+=size;scan=marker===218;
  }
  if(!done)throw Error('Truncated MJPEG frame');
  count++;
  yield [start,p];
 }
 if(!count)throw Error('MJPEG contains no frames');
}
return {mjpegFrames};
})();

const __tofan_icons=(()=>{
// Font Awesome Free, served from the device. Labels remain text, never HTML.
const pages={dashboard:'gauge-high',files:'images',wifi:'wifi',settings:'sliders',ai:'robot',ota:'microchip',account:'user-gear'};
const widgets={network:'wifi',storage:'hard-drive',music:'music',volume:'volume-high',voice:'microphone',system:'memory'};
const buttons={mobileMenu:'bars',logout:'arrow-right-from-bracket',accountLogout:'arrow-right-from-bracket',addBlock:'plus',resetLayout:'rotate-left',refreshFiles:'rotate-right',screenSize:'display',resetCrop:'crop-simple',process:'wand-magic-sparkles',processVideo:'film',cancelProcess:'xmark',cancelVideoProcess:'xmark',cancelUpload:'xmark',uploadPrepared:'file-arrow-up',uploadOriginal:'upload',downloadPrepared:'download',saveColors:'palette'};
const actions={up:'arrow-up',down:'arrow-down',remove:'xmark',preview:'eye',download:'download',rename:'pen-to-square',delete:'trash'};
function decorate(element,name,iconOnly=false,end=false){
 if(!name||element.querySelector(':scope > .fa-solid'))return;
 const label=element.textContent.replace(/^[\s＋+↻↗↑↓×●○✓⌟]+/u,'').replace(/[\s⌄↗]+$/u,'');
 const icon=document.createElement('i');icon.className=`fa-solid fa-${name}`;icon.setAttribute('aria-hidden','true');
 element.replaceChildren();if(end){element.append(document.createTextNode(label),icon);}else{element.append(icon);if(!iconOnly&&label)element.append(document.createTextNode(' '+label));}
}
function startIcons(){
 let scheduled=false;
 const refresh=()=>{
  scheduled=false;
  document.querySelectorAll('aside nav a,.mobile-menu-links a').forEach(a=>decorate(a,pages[a.hash.slice(1)]));
  document.querySelectorAll('.widget').forEach(card=>decorate(card.querySelector('h3'),widgets[card.dataset.id]));
  document.querySelectorAll('button').forEach(button=>{
   let name=buttons[button.id]||actions[button.dataset.action]||actions[button.dataset.op],only=false,end=false;
   if(button.classList.contains('resize')){name='up-right-and-down-left-from-center';only=true;}
   else if(button.classList.contains('select-control')){name='chevron-down';end=true;}
   else if(button.classList.contains('file-pick-button'))name='folder-open';
   else if(button.closest('#notice,.mobile-menu-head')){name='xmark';only=true;}
   else if(button.closest('.modal-actions'))name=button.classList.contains('primary')?'check':'xmark';
   else if(button.closest('form')&&button.classList.contains('primary'))name=button.closest('#loginForm')?'right-to-bracket':button.closest('#otaFile')?'microchip':'floppy-disk';
   else if(button.closest('#otaUrl'))name='cloud-arrow-down';
   if(button.id==='mobileMenu'||button.dataset.action)only=true;
   decorate(button,name,only,end);
  });
  document.querySelectorAll('.action-modal h2').forEach(h=>decorate(h,'circle-question'));
  document.querySelectorAll('.select-modal h2').forEach(h=>decorate(h,'list-ul'));
  const connection=document.querySelector('#connection');if(connection)decorate(connection,'wifi');
  document.querySelectorAll('.state-pill').forEach(p=>{if(p.textContent.trim().startsWith('●'))decorate(p,'radio');});
 };
 const observer=new MutationObserver(()=>{if(!scheduled){scheduled=true;queueMicrotask(refresh);}});
 observer.observe(document.body,{childList:true,subtree:true});refresh();
}
return {startIcons};
})();

const __tofan_cards=(()=>{
const {snapWidth,snapHeight}=__tofan_model;
// Keep the shared snap grid, but never shrink below the actual rendered content.
function contentHeight(card){
 const css=getComputedStyle(card);
 const needed=card.querySelector('.widget-head').getBoundingClientRect().height+
  card.querySelector('[data-value]').getBoundingClientRect().height+
  parseFloat(css.paddingTop)+parseFloat(css.paddingBottom)+12;
 return Math.max(220,220+Math.ceil((needed-220)/80)*80);
}
function sizeCard(card,block,width=block.w,height=block.h){
 const grid=card.parentElement;
 let minWidth=3;
 if(window.innerWidth>1000)minWidth=Math.min(12,Math.ceil(280/(grid.clientWidth+20)*12/3)*3);
 block.w=Math.max(minWidth,snapWidth(width));
 card.style.setProperty('--span',block.w);
 block.h=Math.max(contentHeight(card),snapHeight(height));
 card.style.setProperty('--rows',(block.h+20)/80);
 card.style.setProperty('--height',block.h+'px');
 card.querySelector('.block-size').textContent=`${block.w}/12 | ${block.h} px`;
}
function observeCards(grid,blocks,onChange){
 let scheduled=false;
 const fit=()=>{scheduled=false;if(!grid.isConnected)return;let changed=false;grid.querySelectorAll('.widget').forEach(card=>{const block=blocks.find(b=>b.id===card.dataset.id);if(!block)return;const w=block.w,h=block.h;sizeCard(card,block);changed=changed||w!==block.w||h!==block.h;});if(changed)onChange();};
 const observer=new ResizeObserver(()=>{if(!scheduled){scheduled=true;requestAnimationFrame(fit);}});
 observer.observe(grid);grid.querySelectorAll('[data-value]').forEach(el=>observer.observe(el));fit();
 return ()=>observer.disconnect();
}
return {contentHeight,sizeCard,observeCards};
})();

const __tofan_mobile=(()=>{
function mobileMenu(){
 const button=document.querySelector('#mobileMenu');
 button.onclick=()=>{
  const dialog=document.createElement('dialog');dialog.className='mobile-menu';dialog.setAttribute('aria-label','เมนูหลัก');
  const header=document.createElement('div');header.className='mobile-menu-head';const title=document.createElement('h2');title.textContent='พื้นที่ของคุณ';const close=document.createElement('button');close.textContent='×';close.setAttribute('aria-label','ปิดเมนู');close.onclick=()=>dialog.close();header.append(title,close);dialog.append(header);
  const links=document.createElement('div');links.className='mobile-menu-links';
  document.querySelectorAll('aside nav a').forEach(link=>{const item=link.cloneNode(true);item.onclick=()=>dialog.close();links.append(item);});dialog.append(links);
  const status=document.createElement('p');status.className='hint';status.textContent=document.querySelector('#connection').textContent;dialog.append(status);
  dialog.onclose=()=>{button.setAttribute('aria-expanded','false');dialog.remove();button.focus();};
  document.body.append(dialog);button.setAttribute('aria-expanded','true');dialog.showModal();close.focus();
 };
}
return {mobileMenu};
})();

const __tofan_components=(()=>{
// Local, accessible dialogs and controls; no browser alert/confirm/prompt.
function modal(message,{title='ยืนยันการทำรายการ',value,confirm='ยืนยัน',cancel='ยกเลิก'}={}) {
 return new Promise(resolve=>{
  const previous=document.activeElement,d=document.createElement('dialog');d.className='action-modal';
  const heading=document.createElement('h2');heading.id='modal-title';heading.textContent=title;d.setAttribute('aria-labelledby',heading.id);
  const description=document.createElement('p');description.textContent=message;description.style.whiteSpace='pre-line';d.append(heading,description);
  let input;if(value!==undefined){input=document.createElement('input');input.value=value;input.maxLength=120;input.setAttribute('aria-label',message);d.append(input);}
  const actions=document.createElement('div');actions.className='modal-actions';const no=document.createElement('button'),yes=document.createElement('button');no.textContent=cancel;yes.textContent=confirm;yes.className='primary';actions.append(no,yes);d.append(actions);
  let result=null;no.onclick=()=>d.close();yes.onclick=()=>{if(input&&!input.value.trim()){input.focus();return;}result=input?input.value:true;d.close();};
  d.onkeydown=e=>{if(e.key==='Enter'&&e.target===input){e.preventDefault();yes.click();}};
  d.onclose=()=>{d.remove();if(previous?.isConnected)previous.focus();resolve(result);};document.body.append(d);d.showModal();(input||no).focus();input?.select();
 });
}
const ask=message=>modal(message);
const requestName=(message,value)=>modal(message,{title:'เปลี่ยนชื่อไฟล์',value,confirm:'บันทึกชื่อ'});

function enhanceControls(root=document){
 root.querySelectorAll('select:not([data-enhanced])').forEach(select=>{
  select.dataset.enhanced='true';select.classList.add('native-control');select.tabIndex=-1;select.setAttribute('aria-hidden','true');
  const button=document.createElement('button');button.type='button';button.className='select-control';button.setAttribute('aria-haspopup','dialog');
  const label=select.getAttribute('aria-label')||select.closest('label')?.firstChild?.textContent.trim()||'เลือกตัวเลือก';
  const sync=()=>{button.textContent=(select.selectedOptions[0]?.textContent||'เลือก')+'  ⌄';button.disabled=select.disabled;button.setAttribute('aria-label',label+': '+(select.selectedOptions[0]?.textContent||''));};
  const descriptor=Object.getOwnPropertyDescriptor(HTMLSelectElement.prototype,'value');Object.defineProperty(select,'value',{get(){return descriptor.get.call(this);},set(value){descriptor.set.call(this,value);sync();},configurable:true});
  select.after(button);select.addEventListener('change',sync);sync();
  button.onclick=e=>{e.preventDefault();const d=document.createElement('dialog');d.className='select-modal';d.setAttribute('aria-label',label);const h=document.createElement('h2');h.textContent=label;d.append(h);const list=document.createElement('div');list.setAttribute('role','listbox');list.setAttribute('aria-label',label);
   [...select.options].forEach(option=>{const item=document.createElement('button');item.type='button';item.className='select-option';item.textContent=option.textContent;item.disabled=option.disabled;item.setAttribute('role','option');item.setAttribute('aria-selected',String(option.selected));item.onclick=()=>{select.value=option.value;select.dispatchEvent(new Event('input',{bubbles:true}));select.dispatchEvent(new Event('change',{bubbles:true}));sync();d.close();};list.append(item);});d.append(list);
   const close=document.createElement('button');close.textContent='ปิด';close.className='quiet';close.onclick=()=>d.close();d.append(close);
   d.onkeydown=e=>{if(!['ArrowDown','ArrowUp','Home','End'].includes(e.key))return;e.preventDefault();const items=[...list.querySelectorAll('button:not(:disabled)')];let i=items.indexOf(document.activeElement);i=e.key==='Home'?0:e.key==='End'?items.length-1:(i+(e.key==='ArrowDown'?1:-1)+items.length)%items.length;items[i]?.focus();};
   d.onclose=()=>{d.remove();if(button.isConnected)button.focus();};document.body.append(d);d.showModal();(list.querySelector('[aria-selected=true]:not(:disabled)')||list.querySelector('button:not(:disabled)')||close).focus();
  };
 });
 root.querySelectorAll('input[type=file]:not([data-enhanced])').forEach(input=>{
  input.dataset.enhanced='true';input.classList.add('native-control');input.tabIndex=-1;
  const box=document.createElement('div');box.className='file-picker';const button=document.createElement('button');button.type='button';button.className='file-pick-button';button.textContent='＋ เลือกไฟล์จากเครื่อง';const name=document.createElement('span');name.className='picked-name';name.textContent='ยังไม่ได้เลือกไฟล์';name.setAttribute('aria-live','polite');box.append(button,name);input.after(box);
  button.onclick=e=>{e.preventDefault();input.click();};input.addEventListener('change',()=>{name.textContent=input.files[0]?.name||'ยังไม่ได้เลือกไฟล์';button.textContent=input.files.length?'↻ เปลี่ยนไฟล์':'＋ เลือกไฟล์จากเครื่อง';});
 });
}

let validationOpen=false;
document.addEventListener('invalid',async e=>{e.preventDefault();if(validationOpen)return;validationOpen=true;await modal(e.target.validationMessage,{title:'ตรวจสอบข้อมูลอีกนิด',confirm:'ตกลง',cancel:'ปิด'});validationOpen=false;if(e.target.isConnected){const focus=e.target.classList.contains('native-control')?e.target.nextElementSibling?.querySelector('button')||e.target.nextElementSibling:e.target;focus?.focus();}},true);
return {modal,ask,requestName,enhanceControls};
})();

const __tofan_dashboard=(()=>{
const {bytes,time}=__tofan_model;
const safe=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const percent=n=>Number.isFinite(n)?Math.max(0,Math.min(100,n)):0;
const metric=(value,sub)=>`<div class="metric">${safe(value)}</div><small>${safe(sub)}</small>`;
function bar(value,label){const n=Math.round(percent(value));return `<div class="status-caption"><span>${safe(label)}</span><b>${n}%</b></div><div class="status-track" role="progressbar" aria-label="${safe(label)}" aria-valuemin="0" aria-valuemax="100" aria-valuenow="${n}"><span style="width:${n}%"></span></div>`;}
const history=[];
function recordStatus(s){if(Number.isFinite(s.heap)){history.push(s.heap);if(history.length>45)history.shift();}}
function graph(){if(history.length<2)return '<div class="graph-empty">กำลังเก็บข้อมูล RAM…</div>';const min=Math.min(...history),max=Math.max(...history),range=Math.max(max-min,1024);const points=history.map((n,i)=>`${(i/(history.length-1)*300).toFixed(1)},${(45-(n-min)/range*38).toFixed(1)}`).join(' ');return `<svg class="sparkline" viewBox="0 0 300 52" preserveAspectRatio="none" role="img" aria-label="กราฟ RAM ว่าง ${history.length} จุดล่าสุด ช่วง ${safe(bytes(min))} ถึง ${safe(bytes(max))}"><path d="M0 48 H300 M0 25 H300" class="graph-grid"/><polyline points="${points}"/></svg><small>RAM ว่าง · ${bytes(min)}–${bytes(max)} · ${history.length} จุดล่าสุด</small>`;}
function widgetContent(id,s){switch(id){
 case'network':return metric(s.connected?s.ssid:'Access Point',s.connected?`${s.ip} · ${s.rssi} dBm`:`AP · ${s.apIP||'192.168.4.1'}`)+bar(s.connected?(s.rssi+100)*2:0,'ความแรงสัญญาณ WiFi');
 case'storage':return metric(s.sd?bytes((s.storageTotal||0)-(s.storageUsed||0)):'ไม่มี SD',s.sd?`ว่างจากทั้งหมด ${bytes(s.storageTotal)}`:'ใส่ SD card เพื่อจัดเก็บไฟล์')+bar(s.storageTotal?s.storageUsed/s.storageTotal*100:0,'พื้นที่ใช้งาน');
 case'music':return metric(s.title||'พักสักครู่',`${time(s.current)} / ${time(s.duration)} · ${s.playing?'กำลังเล่น':'หยุดอยู่'}`)+(s.duration?bar(s.current/s.duration*100,'ความคืบหน้าเพลง'):'<div class="state-pill">● '+(s.playing?'สตรีมสด · ไม่ระบุความยาว':'ยังไม่มีเพลง')+'</div>');
 case'volume':return metric(`${s.settings?.volume??'—'}%`,'ระดับเสียงของอุปกรณ์')+bar(s.settings?.volume,'ระดับเสียง');
 case'voice':return metric(s.settings?.voice?'เปิดอยู่':'ปิดอยู่','Voice assistant · inference')+`<div class="state-pill ${s.settings?.voice?'on':''}"><span class="state-dot"></span>${s.settings?.voice?'เปิดการรับคำสั่งเสียง':'พักการรับคำสั่งเสียง'}</div>`;
 case'system':return metric(`${Math.floor((s.uptime||0)/60)} นาที`,`RAM ว่าง ${bytes(s.heap)}`)+graph();
 default:return '';
}}
return {recordStatus,widgetContent};
})();

const {widgets,snapWidth,snapHeight,defaults,layout,rect,dimensions,bytes,time,playableAudio,mediaDirectory,directAudioNameFromUrl,blockedYouTubeUrl}=__tofan_model;

const {cropControls}=__tofan_crop;
const {mjpegFrames}=__tofan_mjpeg;
const {startIcons}=__tofan_icons;
const {sizeCard,observeCards}=__tofan_cards;
const {mobileMenu}=__tofan_mobile;
const {ask,requestName,enhanceControls}=__tofan_components;
const {widgetContent,recordStatus}=__tofan_dashboard;

const $=(s,root=document)=>root.querySelector(s),$$=(s,root=document)=>[...root.querySelectorAll(s)];
const escape=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const MiB=1048576,softUploadBytes=256*MiB,uploadReserveBytes=64*1024;
function storageFree(){const total=Number(status.storageTotal),used=Number(status.storageUsed);return Number.isFinite(total)&&Number.isFinite(used)&&total>=used?total-used:null;}
const pages={dashboard:['ภาพรวม','พื้นที่ของคุณ จัดวางในแบบที่ชอบ'],files:['ไฟล์และมีเดีย','เตรียมภาพให้พอดีกับจอ ทุกขั้นตอนอยู่ในเบราว์เซอร์ของคุณ'],wifi:['WiFi Manager','เชื่อมต่อและจัดการเครือข่ายของอุปกรณ์'],settings:['การตั้งค่า','สี เสียง และการทำงานที่เป็นคุณ'],ai:['AI conversation','ตั้งค่าบริการสนทนาที่อุปกรณ์ใช้งาน'],ota:['อัปเดตเฟิร์มแวร์','อัปเดตจากไฟล์หรือลิงก์โดยตรง'],account:['บัญชี','จัดการการเข้าถึงอุปกรณ์']};
let token=sessionStorage.getItem('tofan-session')||'',status={},blocks=defaults(),page='',polling=false,worker=null,original=null,processed=null,previewURL='',previewImage=null,xhr=null,videoProcessing=false,videoProcessCancelled=false;
let stopCardObserver=null,cropUI=null,lastAudioImportState='';
let panX=0,panY=0,blockKey='tofan-layout',layoutLoaded=false;
function notice(text,error=false){const n=$('#notice');n.hidden=!text;n.replaceChildren();n.className=error?'error':'';if(!text)return;const message=document.createElement('span');message.textContent=text;const close=document.createElement('button');close.type='button';close.textContent='×';close.setAttribute('aria-label','ปิดการแจ้งเตือน');close.onclick=()=>n.hidden=true;n.append(message,close);}

async function api(path,data){const response=await fetch('/api/'+path,{method:data?'POST':'GET',headers:{Authorization:`Bearer ${token}`,'X-ToFan-Client':'portal',...(data?{'Content-Type':'application/x-www-form-urlencoded'}:{})},body:data?new URLSearchParams(data):undefined});const json=await response.json();if(!response.ok){if(response.status===401&&path!=='session')signout();throw Error(json.error||`HTTP ${response.status}`);}return json;}
function attempt(fn){return async e=>{e?.preventDefault();try{await fn(e);}catch(error){notice(error.message,true);}};}
function signout(){token='';sessionStorage.removeItem('tofan-session');$('#shell').hidden=true;$('#login').hidden=false;}
async function boot(){const info=await api('session');$('#loginTitle').textContent=info.setup?'เริ่มต้นพื้นที่ของคุณ':'ยินดีต้อนรับกลับ';$('#loginHelp').textContent=info.setup?'เชื่อมต่อ WiFi ของ ToFan แล้วสร้างบัญชีผู้ดูแลครั้งแรก':'เข้าสู่ระบบเพื่อจัดการ ToFan';$('#loginForm [name=password]').minLength=info.setup?8:1;$('#loginForm button').textContent=info.setup?'สร้างบัญชีและเริ่มใช้งาน':'เข้าสู่ระบบ';if(token)await open();}
$('#loginForm').onsubmit=async e=>{e.preventDefault();const button=$('button',e.target);button.disabled=true;try{const d=await api('session',Object.fromEntries(new FormData(e.target)));token=d.token;sessionStorage.setItem('tofan-session',token);await open();}catch(err){notice(err.message,true);}finally{button.disabled=false;}};
$('#logout').onclick=attempt(async()=>{await api('logout',{});signout();});
async function open(){status=await api('status');recordStatus(status);$('#login').hidden=true;$('#shell').hidden=false;layoutLoaded=false;loadLayout();applyWebTheme();render();}
function loadLayout(){if(layoutLoaded||!status.device)return;blockKey='tofan-layout-'+status.device;try{blocks=layout(JSON.parse(localStorage.getItem(blockKey))??defaults());}catch{blocks=defaults();}layoutLoaded=true;}
function applyWebTheme(){
 const names=['--bg','--surface','--text','--accent','--muted','--selection'];
 status.settings?.colors?.forEach(([h,s,v],i)=>{s/=100;v/=100;const f=n=>{const k=(n+h/60)%6;return Math.round(255*(v-v*s*Math.max(0,Math.min(k,4-k,1))));};const rgb=[f(5),f(3),f(1)];document.documentElement.style.setProperty(names[i],`rgb(${rgb.join(',')})`);if(i===3)document.documentElement.style.setProperty('--button-text',(rgb[0]*299+rgb[1]*587+rgb[2]*114)>145000?'#243b31':'#ffffff');});
}
function saveLayout(){try{localStorage.setItem(blockKey,JSON.stringify(blocks));}catch{notice('เบราว์เซอร์ไม่อนุญาตให้จดจำ layout',true);}}
function render(){if(!token)return;stopCardObserver?.();stopCardObserver=null;videoProcessCancelled=true;if(worker){worker.terminate();worker=null;}if(previewURL){URL.revokeObjectURL(previewURL);previewURL='';}previewImage?.close?.();original=processed=previewImage=null;page=location.hash.slice(1);if(!pages[page])page='dashboard';$('nav').innerHTML=Object.entries(pages).map(([id,[name]])=>`<a href="#${id}" class="${page===id?'active':''}" ${page===id?'aria-current="page"':''}>${name}</a>`).join('');$('#pageTitle').textContent=pages[page][0];$('#subtitle').textContent=pages[page][1];notice('');({dashboard,files,wifi,settings,ai,ota,account}[page])();enhanceControls();}
window.addEventListener('hashchange',render);
const metric=(value,sub)=>`<div class="metric">${escape(value)}</div><small>${escape(sub)}</small>`;
function widgetValue(id){return widgetContent(id,status);}
function dashboard(){$('#content').innerHTML=`<div class="toolbar"><div><h2>วันนี้ พอดีกับคุณ</h2><p class="hint">ลากมุมล่างเพื่อปรับขนาด · ใช้ลูกศรเพื่อย้ายบล็อก</p></div><div class="row"><select id="addType" aria-label="ชนิดบล็อก">${Object.entries(widgets).map(([id,name])=>`<option value="${id}">${name}</option>`).join('')}</select><button id="addBlock" class="primary">+ เพิ่มบล็อก</button><button id="resetLayout">เริ่มใหม่</button></div></div><div class="grid" id="dashboard"></div>`;drawBlocks();$('#addBlock').onclick=()=>{const id=$('#addType').value;if(!blocks.some(b=>b.id===id)){blocks.push({id,w:6,h:220});saveLayout();drawBlocks();}else notice('มีบล็อกนี้อยู่แล้ว');};$('#resetLayout').onclick=()=>{blocks=defaults();saveLayout();drawBlocks();};}
function drawBlocks(){stopCardObserver?.();$('#dashboard').innerHTML=blocks.map((b,i)=>`<section class="widget" data-id="${b.id}" style="--span:${b.w};--height:${b.h}px;--rows:${(b.h+20)/80}"><div class="widget-head"><h3>${widgets[b.id]}</h3><div class="widget-actions"><button data-action="up" aria-label="ย้าย ${widgets[b.id]} ขึ้น" ${i===0?'disabled':''}>↑</button><button data-action="down" aria-label="ย้าย ${widgets[b.id]} ลง" ${i===blocks.length-1?'disabled':''}>↓</button><button data-action="remove" aria-label="ลบ ${widgets[b.id]}">×</button></div></div><div data-value="${b.id}">${widgetValue(b.id)}</div><span class="block-size">${b.w}/12 | ${b.h} px</span><button class="resize" aria-label="ปรับขนาด ${widgets[b.id]} ใช้ปุ่มลูกศร"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M8 3H3v5m13 13h5v-5M3 3l6 6m12 12-6-6"/></svg></button></section>`).join('')||'<p class="empty">เพิ่มบล็อกแรกเพื่อเริ่มจัดพื้นที่ของคุณ</p>';
 $$('.widget').forEach(el=>{const id=el.dataset.id,b=blocks.find(b=>b.id===id);$$('[data-action]',el).forEach(btn=>btn.onclick=()=>{const i=blocks.indexOf(b),a=btn.dataset.action;if(a==='remove')blocks.splice(i,1);else{const j=i+(a==='up'?-1:1);[blocks[i],blocks[j]]=[blocks[j],blocks[i]];}saveLayout();drawBlocks();});const handle=$('.resize',el);handle.onpointerdown=e=>{handle.setPointerCapture(e.pointerId);const x=e.clientX,y=e.clientY,w=b.w,h=b.h,unit=($('#dashboard').clientWidth+20)/12;$('#dashboard').classList.add('snapping');handle.onpointermove=e=>{b.w=snapWidth(w+(e.clientX-x)/unit);b.h=snapHeight(h+e.clientY-y);sizeCard(el,b);};handle.onpointerup=handle.onpointercancel=handle.onlostpointercapture=()=>{handle.onpointermove=null;$('#dashboard')?.classList.remove('snapping');saveLayout();};};handle.onkeydown=e=>{if(!e.key.startsWith('Arrow'))return;e.preventDefault();b.w=snapWidth(b.w+(e.key==='ArrowRight'?3:e.key==='ArrowLeft'?-3:0));b.h=snapHeight(b.h+(e.key==='ArrowDown'?80:e.key==='ArrowUp'?-80:0));sizeCard(el,b);saveLayout();};});stopCardObserver=observeCards($('#dashboard'),blocks,saveLayout);}
const toggle=(key,title,on)=>`<label class="switch">${title}<input type="checkbox" name="${key}" ${on?'checked':''} role="switch"></label>`;
async function saveSettings(values){await api('settings',{values:JSON.stringify(values)});notice('ส่งการตั้งค่าแล้ว รอผลการบันทึกจากอุปกรณ์');}
function wifi(){$('#content').innerHTML=`<div class="two"><form class="panel" id="wifiForm"><span class="badge">NETWORK</span><h2>เชื่อมต่อ WiFi</h2><p>บันทึกเครือข่ายสำหรับการเปิดเครื่องครั้งถัดไป</p><label>ชื่อเครือข่าย (SSID)<input name="ssid" value="${escape(status.ssid)}" maxlength="32" required autocomplete="off"></label><label>รหัสผ่าน<input name="password" type="password" maxlength="63" autocomplete="new-password" placeholder="เว้นว่างสำหรับเครือข่ายเปิด"></label><button class="primary">บันทึกและเชื่อมต่อ</button><p class="hint">เครือข่าย 2.4 GHz · หน้าเว็บยังเปิดผ่าน AP ของ ToFan ได้</p></form><section class="panel"><h2>สถานะเครือข่าย</h2><div id="wifiStatus"></div>${toggle('wifi','เชื่อมต่อ WiFi อัตโนมัติ',status.settings?.wifi)}<p class="hint">เปิดเว็บจัดการได้จาก Admin mode บนตัวเครื่อง</p></section></div>`;$('#wifiForm').onsubmit=attempt(async e=>{await api('wifi',Object.fromEntries(new FormData(e.target)));notice('บันทึกและกำลังเชื่อมต่อ กรุณารอสถานะจากเครื่อง');});$('[name=wifi]').onchange=attempt(e=>saveSettings({wifi:+e.target.checked}));updateWifi();}
function updateWifi(){if(!$('#wifiStatus'))return;$('#wifiStatus').innerHTML=metric(status.networkBusy?'กำลังเชื่อมต่อ…':status.connected?'เชื่อมต่อแล้ว':'Access Point',status.connected?`${status.ssid} · ${status.ip} · ${status.rssi} dBm`:`เปิดเว็บที่ ${status.apIP||'192.168.4.1'}`);}
function settings(){const s=status.settings||{};$('#content').innerHTML=`<div class="two"><form id="soundSettings" class="panel"><h2>เสียงและการเล่นเพลง</h2><label>ระดับเสียง <output id="volumeValue">${s.volume??50}%</output><input name="volume" type="range" min="0" max="100" value="${s.volume??50}"></label><label>ระดับต่อหนึ่งกึกของตัวหมุน<select name="volumeStep">${[2,3,4,5].map(n=>`<option ${s.volumeStep===n?'selected':''}>${n}</option>`).join('')}</select></label>${toggle('autoNext','เล่นเพลงถัดไปอัตโนมัติ',s.autoNext)}${toggle('shuffle','สุ่มเพลง',s.shuffle)}${toggle('voice','Voice assistant · เปิด inference',s.voice)}<button class="primary">บันทึกการตั้งค่า</button></form><section class="panel"><h2>Display settings</h2><p>เลือกส่วนที่ต้องการ แล้วแตะวงล้อสี</p><label>ส่วนของหน้าจอ<select id="colorRole">${['พื้นหลัง','พื้นผิว','ข้อความ','สีหลัก','ข้อความรอง','รายการที่เลือก'].map((n,i)=>`<option value="${i}">${n}</option>`).join('')}</select></label><div class="row"><canvas id="wheel" class="color-wheel" width="150" height="150" aria-label="วงล้อสี"></canvas><input id="colorHex" type="color" aria-label="เลือกสีแบบตัวเลข" style="width:70px"></div><label>ความสว่าง<input id="brightness" type="range" min="0" max="100"></label><div id="colorPreview" class="panel">ตัวอย่างสีที่เลือก</div><button id="saveColors" class="primary">บันทึกสีหน้าจอ</button><p class="hint">สีและการตั้งค่าจดจำในเครื่อง แม้ปิดเครื่องแล้วเปิดใหม่</p></section></div>`;
 $('[name=volume]').oninput=e=>$('#volumeValue').textContent=e.target.value+'%';$('#soundSettings').onsubmit=attempt(e=>saveSettings({volume:+$('[name=volume]').value,volumeStep:+$('[name=volumeStep]').value,...Object.fromEntries(['autoNext','shuffle','voice'].map(k=>[k,+$(`[name=${k}]`).checked]))}));
 const colors=structuredClone(s.colors||[[160,3,98],[155,9,94],[205,30,22],[160,30,79],[205,17,49],[165,19,88]]),canvas=$('#wheel'),ctx=canvas.getContext('2d'),img=ctx.createImageData(150,150);
 function rgb([h,s,v]){s/=100;v/=100;const f=n=>{const k=(n+h/60)%6;return Math.round(255*(v-v*s*Math.max(0,Math.min(k,4-k,1))));};return[f(5),f(3),f(1)];}
 for(let y=0;y<150;y++)for(let x=0;x<150;x++){const dx=x-75,dy=y-75,r=Math.hypot(dx,dy),i=(y*150+x)*4;if(r<=75){img.data.set([...rgb([(Math.atan2(dy,dx)*180/Math.PI+360)%360,r/75*100,100]),255],i);}}ctx.putImageData(img,0,0);
 const show=()=>{const c=colors[+$('#colorRole').value],hex='#'+rgb(c).map(n=>n.toString(16).padStart(2,'0')).join('');$('#brightness').value=c[2];$('#colorHex').value=hex;$('#colorPreview').style.background=hex;$('#colorPreview').style.color=c[2]>60?'#243b31':'white';};
 const choose=e=>{const r=canvas.getBoundingClientRect(),x=(e.clientX-r.left)/r.width*150-75,y=(e.clientY-r.top)/r.height*150-75,c=colors[+$('#colorRole').value];c[0]=Math.round((Math.atan2(y,x)*180/Math.PI+360)%360)%360;c[1]=Math.round(Math.min(100,Math.hypot(x,y)/75*100));show();};canvas.onpointerdown=e=>{canvas.setPointerCapture(e.pointerId);choose(e);canvas.onpointermove=choose;};canvas.onpointerup=()=>canvas.onpointermove=null;
 $('#colorRole').onchange=show;$('#brightness').oninput=e=>{colors[+$('#colorRole').value][2]=+e.target.value;show();};$('#colorHex').oninput=e=>{const a=e.target.value.match(/\w\w/g).map(x=>parseInt(x,16)/255),max=Math.max(...a),min=Math.min(...a),d=max-min;let h=0;if(d)h=max===a[0]?((a[1]-a[2])/d+6)%6:max===a[1]?(a[2]-a[0])/d+2:(a[0]-a[1])/d+4;colors[+$('#colorRole').value]=[Math.round(h*60)%360,Math.round(max?d/max*100:0),Math.round(max*100)];show();};$('#saveColors').onclick=attempt(()=>saveSettings({colors}));show();}
function ai(){
 const a=status.ai||{},live=(a.provider||'gemini-live')==='gemini-live';
 $('#content').innerHTML=`<form id="aiForm" class="panel"><h2>บริการสนทนา</h2>
 ${toggle('enabled','เปิด AI conversation',a.enabled)}
 <label>Provider<select name="provider" id="aiProvider"><option value="gemini-live" ${live?'selected':''}>Gemini Live (ไมค์ → เสียงตอบกลับ)</option><option value="backend" ${!live?'selected':''}>HTTP backend เดิม</option></select></label>
 <label>API key<input name="apiKey" type="password" maxlength="159" placeholder="${a.apiKeySet?'ตั้งค่าแล้ว · เว้นว่างเพื่อคงค่าเดิม':'ใส่ API key'}"></label>
 <div id="geminiLiveFields">
   <div class="two"><label>Gemini Live model<input name="model" value="${escape(a.model||'gemini-3.1-flash-live-preview')}" maxlength="63" placeholder="gemini-3.1-flash-live-preview"></label><label>Voice<input name="voice" value="${escape(a.voice||'Kore')}" maxlength="31" placeholder="Kore"></label></div>
   <label>System instruction<textarea name="systemInstruction" maxlength="511" rows="5" placeholder="บุคลิกและวิธีตอบของ AI Pet">${escape(a.systemInstruction||'You are ToFan, a friendly desk companion. Respond naturally and briefly. If the user speaks Thai, reply in Thai.')}</textarea></label>
   ${toggle('liveBargeIn','อนุญาตให้พูดแทรกขณะ Gemini กำลังตอบ',a.liveBargeIn)}
   <p class="hint">เมื่ออยู่หน้า AI Pet จะเชื่อม Gemini Live แบบเสียงเท่านั้น: ไมค์ 16 kHz → Gemini และเสียงตอบกลับ 24 kHz → ลำโพงของ ToFan โดยตรง</p>
 </div>
 <div id="legacyAiFields">
   <label>Pipeline URL<input name="pipelineUrl" type="url" value="${escape(a.pipelineUrl||'')}" maxlength="191" placeholder="https://.../voice"></label>
   <div class="two"><label>ชื่อผู้ใช้บริการ<input name="user" maxlength="63" placeholder="เว้นว่างเพื่อคงค่าเดิม"></label><label>รหัสผ่านบริการ<input name="password" type="password" maxlength="63" placeholder="เว้นว่างเพื่อคงค่าเดิม"></label></div>
 </div>
 ${toggle('allowInsecureTLS','อนุญาต TLS ที่ไม่ตรวจใบรับรอง (ใช้เฉพาะระบบทดสอบ)',a.allowInsecureTLS)}
 <p class="hint" id="aiState">สถานะ: ${escape(a.state||'idle')}${a.lastError?` · ${escape(a.lastError)}`:''}</p>
 <button class="primary">บันทึก</button></form>`;
 const switchMode=()=>{const isLive=$('#aiProvider').value==='gemini-live';$('#geminiLiveFields').hidden=!isLive;$('#legacyAiFields').hidden=isLive;};
 $('#aiProvider').onchange=switchMode;switchMode();
 $('#aiForm').onsubmit=attempt(async e=>{
   const values=Object.fromEntries(new FormData(e.target));
   for(const k of ['apiKey','user','password'])if(!values[k])delete values[k];
   for(const k of ['enabled','allowInsecureTLS','liveBargeIn'])values[k]=$(`[name=${k}]`).checked;
   if(values.provider==='gemini-live'){
     delete values.pipelineUrl;delete values.user;delete values.password;
   }else{
     delete values.voice;delete values.systemInstruction;delete values.liveBargeIn;
   }
   await api('ai',{values:JSON.stringify(values)});notice('ส่งการตั้งค่า AI แล้ว');
 });
}
function account(){$('#content').innerHTML=`<form class="panel" id="accountForm"><h2>ข้อมูลบัญชี</h2><p>เปลี่ยนชื่อบัญชี หรือเปลี่ยนรหัสผ่านได้ที่นี่</p><label>ชื่อบัญชี<input name="username" value="${escape(status.username||'')}" maxlength="31" required autocomplete="username"></label><label>รหัสผ่านปัจจุบัน<input name="current" type="password" required autocomplete="current-password"></label><label>รหัสผ่านใหม่ (ไม่บังคับ)<input name="password" type="password" minlength="8" maxlength="128" autocomplete="new-password" placeholder="เว้นว่างเพื่อใช้รหัสผ่านเดิม"></label><button class="primary">บันทึกข้อมูลบัญชี</button><p class="hint">หลังบันทึกให้เข้าสู่ระบบอีกครั้งด้วยข้อมูลใหม่</p></form><button id="accountLogout">ออกจากระบบ</button>`;$('#accountForm').onsubmit=attempt(async e=>{await api('account',Object.fromEntries(new FormData(e.target)));signout();notice('บันทึกบัญชีแล้ว กรุณาเข้าสู่ระบบด้วยข้อมูลใหม่');});$('#accountLogout').onclick=$('#logout').onclick;}
function upload(path,file,name){if(xhr)return Promise.reject(Error('มีการอัปโหลดอยู่ กรุณารอหรือยกเลิกก่อน'));return new Promise((resolve,reject)=>{xhr=new XMLHttpRequest();xhr.open('POST','/api/'+path);xhr.setRequestHeader('Authorization','Bearer '+token);xhr.setRequestHeader('X-ToFan-Client','portal');xhr.setRequestHeader('X-File-Size',file.size);xhr.timeout=0;xhr.upload.onprogress=e=>{if($('#transferProgress'))$('#transferProgress').value=e.lengthComputable?e.loaded/e.total*100:0;};const finish=()=>{xhr=null;if($('#cancelUpload'))$('#cancelUpload').hidden=true;};xhr.onload=()=>{const code=xhr.status;let data;try{data=JSON.parse(xhr.responseText);}catch{data={error:'อุปกรณ์ตอบกลับไม่สมบูรณ์'};}finish();code>=200&&code<300?resolve(data):reject(Error(data.error));};xhr.onerror=()=>{finish();reject(Error('การเชื่อมต่อขาด ตรวจสอบอุปกรณ์แล้วลองใหม่'));};xhr.ontimeout=xhr.onerror;xhr.onabort=()=>{finish();reject(Error('ยกเลิกการอัปโหลดแล้ว'));};const form=new FormData();form.append('file',file,name);xhr.send(form);if($('#cancelUpload')){$('#cancelUpload').hidden=false;$('#cancelUpload').onclick=()=>xhr?.abort();}});}

const sourceVideoRe=/\.(mp4|webm|mov|m4v|avi)$/i;
const rawMjpegRe=/\.(mjpeg|mjpg)$/i;
function waitForVideo(video,event,timeout=12000){return new Promise((resolve,reject)=>{let timer;const done=()=>{clearTimeout(timer);video.removeEventListener(event,done);video.removeEventListener('error',fail);resolve();};const fail=()=>{clearTimeout(timer);video.removeEventListener(event,done);video.removeEventListener('error',fail);reject(Error('เบราว์เซอร์ถอดรหัสวิดีโอนี้ไม่ได้ ลอง MP4 H.264 หรือ WebM'));};video.addEventListener(event,done,{once:true});video.addEventListener('error',fail,{once:true});timer=setTimeout(()=>{video.removeEventListener(event,done);video.removeEventListener('error',fail);reject(Error('อ่านวิดีโอนานเกินไป'));},timeout);});}
async function seekVideo(video,time){if(Math.abs(video.currentTime-time)<.002)return;const ready=waitForVideo(video,'seeked');video.currentTime=time;await ready;}
function canvasJpeg(canvas,quality){return new Promise((resolve,reject)=>canvas.toBlob(blob=>blob?resolve(blob):reject(Error('สร้าง JPEG frame ไม่สำเร็จ')),'image/jpeg',quality));}
async function firstMjpegFrame(file){let amount=Math.min(file.size,MiB);for(;;){const data=new Uint8Array(await file.slice(0,amount).arrayBuffer());try{const first=mjpegFrames(data).next();if(!first.done){const [start,end]=first.value;return new Blob([data.subarray(start,end)],{type:'image/jpeg'});}}catch(error){if(amount>=file.size)throw error;}if(amount>=file.size)throw Error('MJPEG ไม่มีเฟรมภาพ');amount=Math.min(file.size,Math.max(amount+1,amount*2));}}
async function prepareVideo(){
 if(videoProcessing)throw Error('กำลังแปลงวิดีโออยู่ กรุณารอหรือยกเลิกก่อน');
 if(!original||!sourceVideoRe.test(original.name))throw Error('เลือกไฟล์วิดีโอก่อน');
 const sourceFile=original;
 const video=$('#videoPreview');if(!video)throw Error('ไม่พบตัวอย่างวิดีโอ');
 if(!Number.isFinite(video.duration)||video.duration<=0)throw Error('อ่านระยะเวลาวิดีโอไม่ได้');
 const longVideo=video.duration>120;
 const crop=options();
 const w=Math.max(16,Math.min(640,crop.w||status.width||320));
 const h=Math.max(16,Math.min(480,crop.h||status.height||240));
 $('#outW').value=w;$('#outH').value=h;
 const fps=12,quality=Math.max(.35,Math.min(.9,+$('#videoQuality').value||.68));
 const frameCount=Math.max(1,Math.ceil(video.duration*fps));
 const canvas=document.createElement('canvas');canvas.width=w;canvas.height=h;const ctx=canvas.getContext('2d',{alpha:false});
 const parts=[];let total=0,firstFrame=null,largeOutput=false;videoProcessing=true;videoProcessCancelled=false;
 $('#processVideo').disabled=true;$('#cancelVideoProcess').hidden=false;$('#uploadPrepared').disabled=true;$('#downloadPrepared').disabled=true;
 notice(longVideo?`คำเตือน: วิดีโอ ${videoTime(video.duration)} ยาวกว่า 2 นาที จึงอาจใช้ RAM/เวลามาก แต่ระบบจะบีบอัดต่อโดยไม่จำกัดระยะเวลา · ${w}x${h} @ ${fps} FPS`:`กำลังแปลงวิดีโอเป็น MJPEG ${w}x${h} @ ${fps} FPS · ไม่มีเสียง`);
 try{
  if(video.readyState<2)await waitForVideo(video,'loadeddata');
  for(let i=0;i<frameCount;i++){
   if(videoProcessCancelled||original!==sourceFile||!video.isConnected)throw Error('ยกเลิกการแปลงวิดีโอแล้ว');
   const time=Math.min(i/fps,Math.max(0,video.duration-.001));await seekVideo(video,time);
   if(videoProcessCancelled||original!==sourceFile||!video.isConnected)throw Error('ยกเลิกการแปลงวิดีโอแล้ว');
   ctx.fillStyle=crop.background||'#000';ctx.fillRect(0,0,w,h);
   const r=rect(video.videoWidth,video.videoHeight,w,h,crop.mode,crop.zoom,crop.panX,crop.panY);
   ctx.drawImage(video,r.x,r.y,r.width,r.height);
   const jpeg=await canvasJpeg(canvas,quality);if(!firstFrame)firstFrame=jpeg;parts.push(jpeg);total+=jpeg.size;
   if(!largeOutput&&total>softUploadBytes){largeOutput=true;notice('MJPEG เกิน 256 MB แล้ว แต่ระบบจะประมวลผลต่อและยังอัปโหลดได้ หาก SD มีพื้นที่เพียงพอ');}
   if($('#transferProgress'))$('#transferProgress').value=(i+1)/frameCount*100;
   if((i&7)===7)await new Promise(r=>setTimeout(r,0));
  }
  if(videoProcessCancelled||original!==sourceFile||!video.isConnected)throw Error('ยกเลิกการแปลงวิดีโอแล้ว');
  processed=new Blob(parts,{type:'video/x-motion-jpeg'});
  const name=sourceFile.name.replace(/\.[^.]+$/,'')+'.mjpeg';$('#uploadName').value=name;
  $('#sizeInfo').textContent=`${bytes(original.size)} → ${bytes(processed.size)} · ${w}×${h} · ${fps} FPS · audio removed`;
  $('#uploadPrepared').disabled=false;$('#downloadPrepared').disabled=false;$('#uploadPrepared').textContent='อัปโหลด MJPEG ที่เตรียมแล้ว';
  if(firstFrame){const url=URL.createObjectURL(firstFrame),img=new Image();img.src=url;img.className='preview';img.alt='เฟรมแรกของ MJPEG';img.onload=()=>URL.revokeObjectURL(url);$('#prepared').replaceChildren(img);}
  notice(largeOutput?'แปลงวิดีโอเรียบร้อย · ไฟล์ใหญ่กว่า 256 MB แต่ยังอัปโหลดได้หาก SD มีพื้นที่เพียงพอ':'แปลงวิดีโอเป็น MJPEG เรียบร้อย พร้อมอัปโหลดไป ToFan');return processed;
 }finally{videoProcessing=false;if($('#processVideo'))$('#processVideo').disabled=false;if($('#cancelVideoProcess'))$('#cancelVideoProcess').hidden=true;}
}

function files(){$('#content').innerHTML=`<div class="two"><section class="panel"><span class="badge">LOCAL MEDIA STUDIO</span><h2>พร้อมสำหรับจอเล็ก</h2><label class="drop">เลือกภาพ เสียง วิดีโอ หรือไฟล์ทั่วไป<input id="mediaInput" type="file"></label><div id="editor" hidden><canvas id="cropPreview" class="preview" width="320" height="240"></canvas><p class="hint">ลากภาพเพื่อจัดตำแหน่ง · GIF แสดงเฟรมแรกในพื้นที่ครอป</p><div class="two"><label>กว้าง (px)<input id="outW" type="number" min="1" max="4096" value="${status.width||320}"></label><label>สูง (px)<input id="outH" type="number" min="1" max="4096" value="${status.height||240}"></label></div><div class="row"><button id="screenSize">พอดีจอ</button><button id="resetCrop">รีเซ็ตตำแหน่ง</button></div><label>จัดวาง<select id="cropMode"><option value="cover">ครอปให้เต็มจอ</option><option value="contain">เห็นทั้งภาพ พร้อมขอบ</option></select></label><label>ซูม<input id="zoom" type="range" min="1" max="4" step=".01" value="1"></label><div class="two"><label>สีขอบ<input id="background" type="color" value="#edf2ee"></label><label>ชนิดไฟล์<select id="format"><option value="mjpeg" disabled>MJPEG</option><option value="jpeg" selected>JPEG</option><option value="png">PNG</option><option value="gif">GIF · คงภาพเคลื่อนไหว</option></select></label></div><label>คุณภาพ / จำนวนสี GIF<input id="quality" type="range" min=".1" max="1" step=".01" value=".85"></label><label>ขนาดเป้าหมาย (KB, ไม่บังคับ)<input id="target" type="number" min="1" placeholder="เช่น 100"></label><p class="hint">JPEG ปรับคุณภาพให้ใกล้ขนาดเป้าหมาย · PNG บีบอัดแบบไม่สูญเสีย · GIF ปรับจำนวนสี</p><button id="process" class="primary">ครอปและบีบอัดในเครื่องนี้</button><button id="cancelProcess" hidden>ยกเลิก</button></div><div id="videoEditor" hidden><video id="videoPreview" class="preview" controls muted playsinline></video><p class="hint">ใช้กรอบ Crop ด้านบนเพื่อกำหนดขนาด ตำแหน่ง Cover/Contain และ Zoom · ทุกเฟรมจะ Crop ตำแหน่งเดียวกันก่อนบีบอัดเป็น Raw MJPEG 12 FPS</p><label>คุณภาพ JPEG ต่อเฟรม<input id="videoQuality" type="range" min=".35" max=".9" step=".01" value=".68"></label><p class="hint">เสียงจะถูกถอดออก · ไม่มี hard limit ของระยะเวลาหรือขนาดไฟล์ หากคลิปยาว/ใหญ่มากระบบจะแจ้งเตือนแต่ยังบีบอัดต่อ</p><button id="processVideo" class="primary">ครอปและบีบอัดเป็น MJPEG</button><button id="cancelVideoProcess" hidden>ยกเลิกการแปลง</button></div><div id="prepared"></div><div id="uploadOptions" hidden><label>ชื่อไฟล์<input id="uploadName" maxlength="120"></label><p id="sizeInfo" class="hint"></p><button id="uploadPrepared" class="primary">อัปโหลดไฟล์ที่เตรียมแล้ว</button><button id="uploadOriginal">อัปโหลดต้นฉบับ (ไม่บีบอัด)</button><button id="downloadPrepared">ดาวน์โหลดไฟล์ที่เตรียม</button></div><progress id="transferProgress" max="100" value="0"></progress><button id="cancelUpload" hidden>ยกเลิกอัปโหลด</button><p class="hint">เสียงที่เล่นบน ToFan รองรับ MP3 / WAV / AAC / M4A / FLAC · ไฟล์ชนิดอื่นจะเก็บไว้ในโฟลเดอร์ Files</p><hr><form id="audioUrlImport"><h3>นำเข้าไฟล์เสียงจากลิงก์โดยตรง</h3><label>Direct audio URL<input name="url" id="audioUrl" type="url" required maxlength="1023" placeholder="https://example.com/audio/song.mp3"></label><label>ชื่อไฟล์บน SD<input name="name" id="audioUrlName" maxlength="120" required placeholder="song.mp3"></label><button class="primary">ดาวน์โหลดลง ToFan</button><p class="hint">รองรับลิงก์ตรงที่ตอบเป็นไฟล์ MP3/WAV/AAC/M4A/FLAC ผ่าน HTTP(S) โดยตรง หน้า/สตรีม YouTube ไม่รองรับ ให้ใช้อัปโหลดไฟล์เสียงที่คุณมีสิทธิ์ใช้แทน</p><p id="audioImportState" class="hint"></p></form><p class="hint">การบีบอัดภาพ/วิดีโอทำในเบราว์เซอร์ และส่งตรงเข้า ToFan เท่านั้น</p></section><section class="panel"><div class="toolbar"><h2>ไฟล์บน SD</h2><button id="refreshFiles">↻ โหลดใหม่</button></div><label>โฟลเดอร์<select id="directory"><option value="Pictures">รูปภาพ</option><option value="Musics">เพลง</option><option value="Videos">วิดีโอ</option><option value="Files">ไฟล์ทั่วไป</option></select></label><div id="fileList" class="empty">กำลังโหลด…</div><p class="hint">จอ TFT เล่น Raw MJPEG (.mjpeg/.mjpg) จาก Videos · Music player อ่าน MP3/WAV/AAC/M4A/FLAC จาก Musics</p></section></div>`;
 cropUI=cropControls({editor:$('#editor'),screen:()=>[status.width||320,status.height||240],read:options,source:()=>previewImage,change:pan=>{if(pan){panX=Math.max(-1,Math.min(1,pan.panX));panY=Math.max(-1,Math.min(1,pan.panY));}invalidate();drawCrop();}});
 $('#mediaInput').onchange=attempt(async e=>{invalidate();notice('');original=e.target.files[0];processed=null;panX=panY=0;if(!original)return;const isImage=/\.(jpe?g|png|gif)$/i.test(original.name),isVideo=sourceVideoRe.test(original.name),isRaw=rawMjpegRe.test(original.name),isAudio=playableAudio(original.name);$('#uploadOptions').hidden=false;$('#editor').hidden=!(isImage||isRaw||isVideo);$('#format').disabled=isRaw||isVideo;$('#target').disabled=isRaw||isVideo;$('#zoom').value=1;$('#outW').min=isVideo?16:1;$('#outH').min=isVideo?16:1;$('#outW').max=isVideo?640:4096;$('#outH').max=isVideo?480:4096;previewImage?.close?.();previewImage=null;$('#videoEditor').hidden=!isVideo;$('#process').hidden=isVideo;$('#prepared').replaceChildren();$('#uploadOriginal').hidden=false;$('#uploadPrepared').disabled=true;$('#downloadPrepared').disabled=true;$('#uploadPrepared').textContent=isVideo?'แปลงและอัปโหลด MJPEG':'อัปโหลดไฟล์ที่เตรียมแล้ว';$('#uploadName').value=isVideo?original.name.replace(/\.[^.]+$/,'')+'.mjpeg':original.name;const free=storageFree();$('#sizeInfo').textContent=`ต้นฉบับ ${bytes(original.size)}${free===null?'':` · SD ว่าง ${bytes(free)}`}`;if(previewURL)URL.revokeObjectURL(previewURL);previewURL=URL.createObjectURL(original);
  if(isImage||isRaw){const selected=original;let frame=original;if(isRaw)frame=await firstMjpegFrame(original);const bitmap=await createImageBitmap(frame);if(original!==selected){bitmap.close();return;}previewImage=bitmap;$('#format').value=isRaw?'mjpeg':/\.gif$/i.test(original.name)?'gif':'jpeg';$('#format option[value=gif]').disabled=!/\.gif$/i.test(original.name);drawCrop();}
  else{previewImage=null;if(isVideo){const v=$('#videoPreview');v.src=previewURL;v.load();if(v.readyState<1)await waitForVideo(v,'loadedmetadata');previewImage=v;$('#format').value='mjpeg';$('#background').value='#000000';$('#outW').value=Math.min(640,status.width||320);$('#outH').value=Math.min(480,status.height||240);$('#uploadPrepared').disabled=false;const refresh=()=>{if(previewImage===v&&!videoProcessing)drawCrop();};v.onloadeddata=refresh;v.onseeked=refresh;v.ontimeupdate=refresh;if(v.readyState>=2)refresh();notice(`พร้อมครอป ${videoTime(v.duration)} และบีบอัดเป็น MJPEG 12 FPS · ลากกรอบ/ซูมด้านบนเพื่อเลือกตำแหน่ง`);}else if(isAudio){const audio=document.createElement('audio');audio.src=previewURL;audio.controls=true;audio.preload='metadata';audio.className='preview-audio';$('#prepared').replaceChildren(audio);notice('ไฟล์เสียงพร้อมอัปโหลดไปโฟลเดอร์ Musics โดยไม่แปลงไฟล์');}else if(isRaw){processed=original;$('#uploadPrepared').disabled=false;$('#downloadPrepared').disabled=false;$('#uploadPrepared').textContent='อัปโหลด MJPEG';notice('ไฟล์นี้เป็น MJPEG อยู่แล้ว ไม่ต้องแปลงซ้ำ');}else{notice('ไฟล์ชนิดนี้จะถูกเก็บในโฟลเดอร์ Files โดยไม่แปลงไฟล์');}}
  if(original.size>softUploadBytes)notice(`ไฟล์ ${bytes(original.size)} ใหญ่กว่า 256 MB แต่ยังอัปโหลดได้ ระบบจะส่งเป็นช่วง ๆ ไปยัง SD`);else if(original.size>8*MiB&&!isVideo)notice('ไฟล์มีขนาดใหญ่ อาจใช้เวลานาน แนะนำรักษาการเชื่อมต่อ WiFi จนเสร็จ');});
 for(const id of ['outW','outH','cropMode','zoom','background'])$('#'+id).oninput=()=>{invalidate();drawCrop();};for(const id of ['format','quality','target'])$('#'+id).oninput=invalidate;
 $('#videoQuality').oninput=()=>{if(!sourceVideoRe.test(original?.name||''))return;processed=null;videoProcessCancelled=true;$('#uploadPrepared').disabled=false;$('#downloadPrepared').disabled=true;$('#uploadPrepared').textContent='แปลงและอัปโหลด MJPEG';};
 $('#screenSize').onclick=()=>{cropUI.select.value='screen';$('#outW').value=status.width||320;$('#outH').value=status.height||240;invalidate();drawCrop();};$('#resetCrop').onclick=()=>{panX=panY=0;$('#zoom').value=1;invalidate();drawCrop();};
 const cv=$('#cropPreview');cv.onpointerdown=e=>{cv.setPointerCapture(e.pointerId);const x=e.clientX,y=e.clientY,px=panX,py=panY;cv.onpointermove=e=>{panX=Math.max(-1,Math.min(1,px+(e.clientX-x)/cv.clientWidth*2));panY=Math.max(-1,Math.min(1,py+(e.clientY-y)/cv.clientHeight*2));invalidate();drawCrop();};};cv.onpointerup=cv.onpointercancel=cv.onlostpointercapture=()=>cv.onpointermove=null;
 $('#process').onclick=attempt(async()=>{if(!original)return;if(!window.Worker||!window.OffscreenCanvas)throw Error('เบราว์เซอร์นี้ไม่รองรับการประมวลผลภาพ ใช้ Chrome/Edge รุ่นใหม่ หรืออัปโหลดต้นฉบับ');if(worker)worker.terminate();worker=new Worker('/image-worker.js',{type:'module'});$('#process').disabled=true;$('#cancelProcess').hidden=false;const finish=()=>{worker?.terminate();worker=null;if($('#process')){$('#process').disabled=false;$('#cancelProcess').hidden=true;}};worker.onerror=e=>{notice('ประมวลผลไม่สำเร็จ: '+e.message,true);finish();};worker.onmessage=({data})=>{if(data.progress){$('#transferProgress').value=data.progress;return;}finish();if(data.error){notice(data.error,true);return;}processed=data.blob;const raw=rawMjpegRe.test(original.name),ext=raw?'mjpeg':processed.type.split('/')[1];$('#uploadName').value=original.name.replace(/\.[^.]+$/,'')+'.'+ext;$('#sizeInfo').textContent=`${bytes(original.size)} → ${bytes(processed.size)}`;$('#uploadPrepared').disabled=false;$('#downloadPrepared').disabled=false;if(previewURL)URL.revokeObjectURL(previewURL);previewURL=URL.createObjectURL(data.preview||processed);const img=new Image();img.src=previewURL;img.className='preview';img.alt='ภาพหลังประมวลผล';$('#prepared').replaceChildren(img);notice(data.warning||'เตรียมภาพเรียบร้อย ยังไม่ได้อัปโหลด');};worker.postMessage({file:original,options:options()});$('#cancelProcess').onclick=()=>{finish();notice('ยกเลิกการประมวลผลแล้ว');};});
 $('#processVideo').onclick=attempt(()=>prepareVideo());$('#cancelVideoProcess').onclick=()=>{videoProcessCancelled=true;notice('กำลังหยุดการแปลงวิดีโอ...');};
 $('#uploadPrepared').onclick=attempt(async()=>{if(sourceVideoRe.test(original?.name||'')&&!processed)await prepareVideo();await sendMedia(processed,$('#uploadName').value);});$('#uploadOriginal').onclick=attempt(()=>sendMedia(original,original.name));$('#downloadPrepared').onclick=()=>download(processed,$('#uploadName').value);$('#refreshFiles').onclick=attempt(loadFiles);$('#directory').onchange=attempt(loadFiles);$('#audioUrl').oninput=e=>{const suggested=directAudioNameFromUrl(e.target.value);if(suggested&&!$('#audioUrlName').dataset.edited)$('#audioUrlName').value=suggested;};$('#audioUrlName').oninput=e=>e.target.dataset.edited=e.target.value?'1':'';$('#audioUrlImport').onsubmit=attempt(async e=>{const data=Object.fromEntries(new FormData(e.target));if(blockedYouTubeUrl(data.url))throw Error('ลิงก์ YouTube ไม่ใช่ direct audio URL ที่ระบบรองรับ กรุณาอัปโหลดไฟล์เสียงของคุณหรือใช้ลิงก์ไฟล์เสียงโดยตรง');if(!playableAudio(data.name))throw Error('ชื่อไฟล์ต้องลงท้าย .mp3, .wav, .aac, .m4a หรือ .flac');await api('audio/import',data);lastAudioImportState='queued';updateAudioImportStatus();notice('เริ่มดาวน์โหลดไฟล์เสียงลง SD แล้ว เพลงที่กำลังเล่นจะทำงานต่อโดยใช้ PSRAM buffer');});updateAudioImportStatus();loadFiles().catch(e=>notice(e.message,true));}
function updateAudioImportStatus(){const el=$('#audioImportState');if(!el)return;const state=status.audioImportState||lastAudioImportState||'idle',done=Number(status.audioImportDone)||0,total=Number(status.audioImportTotal)||0;const progress=total?` ${bytes(done)} / ${bytes(total)}`:done?` ${bytes(done)}`:'';const labels={idle:'พร้อมนำเข้า',queued:'เข้าคิวดาวน์โหลด',connecting:'กำลังเชื่อมต่อ',downloading:'กำลังดาวน์โหลด',done:'ดาวน์โหลดเสร็จแล้ว',error:'ดาวน์โหลดไม่สำเร็จ'};el.textContent=(status.audioImportError||labels[state]||state)+progress;if(state==='done'&&lastAudioImportState!=='done'&&$('#directory')){$('#directory').value='Musics';loadFiles().catch(()=>{});}lastAudioImportState=state;}
function videoTime(seconds){if(!Number.isFinite(seconds))return 'วิดีโอ';const m=Math.floor(seconds/60),s=Math.round(seconds%60).toString().padStart(2,'0');return `${m}:${s}`;}
function options(){return{w:+$('#outW').value,h:+$('#outH').value,mode:$('#cropMode').value,zoom:+$('#zoom').value,panX,panY,background:$('#background').value,format:$('#format').value,quality:+$('#quality').value,target:+$('#target').value*1024};}
function invalidate(){processed=null;$('#prepared')?.replaceChildren();if($('#transferProgress'))$('#transferProgress').value=0;videoProcessCancelled=true;if($('#uploadPrepared')){const isVideo=sourceVideoRe.test(original?.name||'');$('#uploadPrepared').disabled=!isVideo;$('#downloadPrepared').disabled=true;if(isVideo)$('#uploadPrepared').textContent='แปลงและอัปโหลด MJPEG';}if(worker){worker.terminate();worker=null;$('#process').disabled=false;$('#cancelProcess').hidden=true;}}
function drawCrop(){if(!previewImage)return;const o=options();if(!dimensions(o.w,o.h))return;const sw=previewImage.videoWidth||previewImage.naturalWidth||previewImage.width,sh=previewImage.videoHeight||previewImage.naturalHeight||previewImage.height;if(!sw||!sh)return;const cv=$('#cropPreview');cv.width=o.w;cv.height=o.h;const ctx=cv.getContext('2d'),r=rect(sw,sh,o.w,o.h,o.mode,o.zoom,o.panX,o.panY);ctx.fillStyle=o.background;ctx.fillRect(0,0,o.w,o.h);ctx.drawImage(previewImage,r.x,r.y,r.width,r.height);cropUI?.render();}
function download(blob,name){if(!blob)return;const url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
async function sendMedia(file,name){if(!file)throw Error('เลือกและเตรียมไฟล์ก่อน');const free=storageFree();if(free!==null&&file.size+uploadReserveBytes>free)throw Error(`พื้นที่ SD ไม่พอ: ไฟล์ ${bytes(file.size)} แต่เหลือ ${bytes(free)} (ระบบสำรองพื้นที่ 64 KB)`);if(file.size>softUploadBytes)notice(`คำเตือน: ไฟล์ ${bytes(file.size)} ใหญ่กว่า 256 MB แต่จะอัปโหลดต่ออัตโนมัติ${free===null?'':` · SD ว่าง ${bytes(free)}`}`);else if(file.size>8*MiB)notice(`คำเตือน: ไฟล์ ${bytes(file.size)} อาจใช้เวลานาน แต่จะอัปโหลดต่ออัตโนมัติ`);const dir=mediaDirectory(name);await upload(`upload?${new URLSearchParams({dir,name})}`,file,name);notice('บันทึกไฟล์เรียบร้อย');if($('#directory')){$('#directory').value=dir;await loadFiles();}}
async function loadFiles(){const dir=$('#directory').value,result=await api('files?dir='+dir);if(page!=='files')return;$('#fileList').className='';$('#fileList').innerHTML=result.files.map((f,i)=>`<div class="file"><div class="file-name">${escape(f.name)}<small>${bytes(f.size)}</small></div><div class="row">${dir==='Files'?'':`<button data-i="${i}" data-op="preview">ดู</button>`}<button data-i="${i}" data-op="download">ดาวน์โหลด</button><button data-i="${i}" data-op="rename">เปลี่ยนชื่อ</button><button data-i="${i}" data-op="delete">ลบ</button></div></div>`).join('')||'<p class="empty">ยังไม่มีไฟล์ในโฟลเดอร์นี้</p>';if(result.truncated)notice('แสดง 250 ไฟล์แรกในโฟลเดอร์');$$('[data-op]').forEach(btn=>btn.onclick=attempt(async()=>{const f=result.files[+btn.dataset.i],op=btn.dataset.op,name=f.name.split('/').pop();if(op==='delete'){if(!await ask(`ลบ ${name} ถาวรจาก SD?`))return;await api('file',{dir,name,action:'delete'});await loadFiles();}else if(op==='rename'){const newName=await requestName('ชื่อไฟล์ใหม่ (พร้อมนามสกุล)',name);if(!newName)return;await api('file',{dir,name,newName,action:'rename'});await loadFiles();}else{if(f.size>16*1048576&&!await ask(`ดาวน์โหลด ${bytes(f.size)} เข้าเบราว์เซอร์เพื่อ${op==='preview'?'ดูตัวอย่าง':'บันทึก'}?`))return;const res=await fetch('/api/file?'+new URLSearchParams({dir,name}),{headers:{Authorization:'Bearer '+token}});if(!res.ok)throw Error('ดาวน์โหลดไฟล์ไม่สำเร็จ');let blob=await res.blob();if(op==='download')download(blob,name);else{const mime=dir==='Pictures'?(/\.gif$/i.test(name)?'image/gif':/\.png$/i.test(name)?'image/png':'image/jpeg'):dir==='Musics'?(/\.wav$/i.test(name)?'audio/wav':/\.aac$/i.test(name)?'audio/aac':/\.m4a$/i.test(name)?'audio/mp4':/\.flac$/i.test(name)?'audio/flac':'audio/mpeg'):'video/'+(/\.webm$/i.test(name)?'webm':'mp4');blob=new Blob([blob],{type:mime});const url=URL.createObjectURL(blob),dialog=document.createElement('dialog'),media=document.createElement(dir==='Pictures'?'img':dir==='Musics'?'audio':'video');media.src=url;media.controls=true;media.className='preview-dialog';const close=document.createElement('button');close.textContent='ปิด';close.onclick=()=>dialog.close();dialog.append(media,document.createElement('br'),close);document.body.append(dialog);dialog.onclose=()=>{URL.revokeObjectURL(url);dialog.remove();};dialog.showModal();}}}));}
function ota(){$('#content').innerHTML=`<section class="panel"><span class="badge">FIRMWARE</span><h2>พร้อมสำหรับสิ่งใหม่</h2><p>รุ่นปัจจุบัน ${escape(status.firmware||'—')}</p><p class="hint">ไฟล์ application firmware.bin สำหรับ ESP32-S3 · ไม่ใช่ไฟล์ merged flash หรือ bootloader · สูงสุด ${bytes(status.slotSize)}</p><p class="warning">หยุดเพลงและการอัดเสียงก่อนอัปเดต รักษาไฟเลี้ยงจนเครื่องเริ่มใหม่ ข้อมูลบน SD และการตั้งค่าจะไม่ถูกฟอร์แมต</p><div class="two"><form id="otaFile"><h3>จากไฟล์ในเครื่อง</h3><label>ไฟล์เฟิร์มแวร์<input name="file" type="file" accept=".bin" required></label><button class="primary">ตรวจสอบและอัปเดต</button></form><form id="otaUrl"><h3>จากลิงก์</h3><label>ลิงก์ไฟล์ .bin<input name="url" type="url" required placeholder="https://your-server/firmware.bin" maxlength="1023"></label><p class="hint">ลิงก์ดาวน์โหลดตรงพร้อม Content-Length ไม่มี redirect · HTTPS ตรวจใบรับรอง · HTTP ไม่มีการเข้ารหัส</p><button>ดาวน์โหลดและอัปเดต</button></form></div><progress id="transferProgress" max="100" value="0"></progress><button id="cancelUpload" hidden>ยกเลิกการส่งไฟล์</button><p id="otaState" role="status">ยังไม่มีการอัปเดต</p></section>`;$('#otaFile').onsubmit=attempt(async e=>{const file=e.target.file.files[0];if(file.size>status.slotSize||file.size<24)throw Error('ขนาดเฟิร์มแวร์ไม่ถูกต้อง');const h=new Uint8Array(await file.slice(0,24).arrayBuffer());if(h[0]!==233||h[12]!==9||h[13]!==0)throw Error('ไม่ใช่ application image ของ ESP32-S3');if(!await ask(`อัปเดต ToFan ด้วย ${file.name} (${bytes(file.size)})? เครื่องจะเริ่มใหม่เมื่อสำเร็จ`))return;await upload('ota/file',file,file.name);notice('ตรวจสอบเฟิร์มแวร์สำเร็จ เครื่องกำลังเริ่มใหม่');});$('#otaUrl').onsubmit=attempt(async e=>{const url=e.target.url.value;if(!await ask(`ดาวน์โหลดและติดตั้งเฟิร์มแวร์จาก\n${url}\nเครื่องจะเริ่มใหม่เมื่อสำเร็จ`))return;await api('ota/url',{url});notice('อุปกรณ์เริ่มดาวน์โหลดเฟิร์มแวร์แล้ว');});}
setInterval(async()=>{if(!token||polling)return;polling=true;try{const prev=status.settingsRevision;status=await api('status');recordStatus(status);loadLayout();applyWebTheme();$('#connection').textContent=status.connected?'● เชื่อมต่อแล้ว':'● Access Point';if(status.settingsResult&&prev!==status.settingsRevision)notice(status.settingsResult,/failed|Invalid|Cannot|busy|Exit AI Pet/i.test(status.settingsResult));if(page==='dashboard')$$('[data-value]').forEach(el=>el.innerHTML=widgetValue(el.dataset.value));if(page==='wifi')updateWifi();if(page==='files')updateAudioImportStatus();if(page==='ai'&&$('#aiState'))$('#aiState').textContent=`สถานะ: ${status.ai?.state||'idle'}${status.ai?.lastError?' · '+status.ai.lastError:''}`;if(page==='ota'){const ota=await api('ota');if($('#otaState')){$('#otaState').textContent=ota.error||({idle:'พร้อมอัปเดต',connecting:'กำลังเชื่อมต่อ',uploading:'กำลังรับไฟล์',downloading:'กำลังดาวน์โหลด',rebooting:'อัปเดตสำเร็จ กำลังเริ่มใหม่',error:'อัปเดตไม่สำเร็จ'}[ota.state]||ota.state);if(ota.total&&!xhr)$('#transferProgress').value=ota.done/ota.total*100;}}}catch(e){$('#connection').textContent='○ ขาดการเชื่อมต่อ';}finally{polling=false;}},2000);
boot().catch(e=>notice(e.message,true));

document.body.append($('#notice'));

mobileMenu();

startIcons();
