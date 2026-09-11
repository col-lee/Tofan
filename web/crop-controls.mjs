import {rect,dimensions} from './model.mjs';
export function cropControls({editor,screen,read,change,source}){
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
