import {parseGIF,decompressFrame} from 'gifuct-js';
import {GIFEncoder,quantize,applyPalette} from 'gifenc';
import {rect,dimensions} from './model.mjs';
import {mjpegFrames} from './mjpeg.mjs';
self.onmessage=async({data:{file,options:o}})=>{try{
 if(!dimensions(o.w,o.h))throw Error('ขนาดสูงสุด 4096 px และไม่เกิน 4 ล้านพิกเซล');
 const warnings=[];const warn=text=>{if(text&&!warnings.includes(text))warnings.push(text);};
 const canvas=new OffscreenCanvas(o.w,o.h),ctx=canvas.getContext('2d');
 const draw=(source,w,h)=>{ctx.fillStyle=o.background;ctx.fillRect(0,0,o.w,o.h);const r=rect(w,h,o.w,o.h,o.mode,o.zoom,o.panX,o.panY);ctx.drawImage(source,r.x,r.y,r.width,r.height);};
 let blob,preview;
 if(/\.(mjpeg|mjpg)$/i.test(file.name)){
  if(o.w>640||o.h>480)warn('คำเตือน: MJPEG ใหญ่กว่า 640×480 อาจเล่นบน ToFan ไม่ลื่นหรือเล่นไม่ได้ แต่ระบบจะบีบอัดต่อ');
  if(file.size>256*1048576)warn('คำเตือน: MJPEG ต้นฉบับใหญ่กว่า 256 MB ระบบจะบีบอัดต่อโดยไม่จำกัดขนาด');
  const bytes=new Uint8Array(await file.arrayBuffer()),parts=[];let total=0;
  for(const [start,end] of mjpegFrames(bytes)){
   const bitmap=await createImageBitmap(new Blob([bytes.subarray(start,end)],{type:'image/jpeg'}));
   if(bitmap.width*bitmap.height>40000000)warn('คำเตือน: เฟรมต้นฉบับเกิน 40 ล้านพิกเซล อาจใช้ RAM/เวลาเพิ่ม แต่ระบบจะประมวลผลต่อ');
   draw(bitmap,bitmap.width,bitmap.height);bitmap.close();
   const frame=await canvas.convertToBlob({type:'image/jpeg',quality:o.quality});
   if(frame.size>512*1024)warn('คำเตือน: มีเฟรม MJPEG ใหญ่กว่า 512 KB อาจเล่นบน ToFan ไม่ได้ในบางเฟรม แต่ระบบจะบีบอัดและอัปโหลดต่อได้');
   parts.push(frame);total+=frame.size;
   if(total>256*1048576)warn('คำเตือน: ไฟล์ผลลัพธ์ MJPEG ใหญ่กว่า 256 MB แต่ระบบจะบีบอัดต่อและยังอัปโหลดได้หาก SD มีพื้นที่');
   self.postMessage({progress:Math.round(end/bytes.length*100)});
  }
  blob=new Blob(parts,{type:'video/x-motion-jpeg'});
  preview=parts[0];
 }else if(/\.gif$/i.test(file.name)&&o.format==='gif'){
  if(file.size>16*1048576)warn('คำเตือน: GIF ใหญ่กว่า 16 MB อาจใช้ RAM และเวลามาก แต่ระบบจะบีบอัดต่อ');
  const gif=parseGIF(await file.arrayBuffer()),frames=gif.frames.filter(f=>f.image);
  const w=gif.lsd.width,h=gif.lsd.height;
  const loop=gif.frames.find(f=>f.application && /NETSCAPE|ANIMEXTS/.test(f.application.id||''))?.application.blocks;
  const repeat=loop?.length>=3 ? loop[1]|(loop[2]<<8) : -1;
  if(!frames.length)throw Error('GIF ไม่มีเฟรมภาพ');
  if(w*h>4194304||frames.length>300||frames.reduce((n,f)=>n+f.image.descriptor.width*f.image.descriptor.height,0)>24000000)warn('คำเตือน: GIF มีความละเอียด/จำนวนเฟรมสูง อาจใช้ RAM และเวลามาก แต่ระบบจะประมวลผลต่อ');
  const source=new OffscreenCanvas(w,h),sc=source.getContext('2d',{willReadFrequently:true}),enc=GIFEncoder();
  let previous=null,backup=null;
  for(let i=0;i<frames.length;i++){
   if(previous?.disposalType===2)sc.clearRect(previous.dims.left,previous.dims.top,previous.dims.width,previous.dims.height);
   if(previous?.disposalType===3&&backup)sc.putImageData(backup,0,0);
   const f=decompressFrame(frames[i],gif.gct,true);backup=f.disposalType===3?sc.getImageData(0,0,w,h):null;
   const patch=new OffscreenCanvas(f.dims.width,f.dims.height);patch.getContext('2d').putImageData(new ImageData(f.patch,f.dims.width,f.dims.height),0,0);sc.drawImage(patch,f.dims.left,f.dims.top);
   draw(source,w,h);const rgba=ctx.getImageData(0,0,o.w,o.h).data,palette=quantize(rgba,Math.max(16,Math.round(o.quality*256)));
   enc.writeFrame(applyPalette(rgba,palette),o.w,o.h,{palette,delay:f.delay||100,repeat,dispose:1});previous=f;
   self.postMessage({progress:Math.round((i+1)/frames.length*100)});
  }
  enc.finish();blob=new Blob([enc.bytes()],{type:'image/gif'});
 }else{
  const bitmap=await createImageBitmap(file);if(bitmap.width*bitmap.height>40000000)warn('คำเตือน: ภาพต้นฉบับเกิน 40 ล้านพิกเซล อาจใช้ RAM และเวลามาก แต่ระบบจะบีบอัดต่อ');draw(bitmap,bitmap.width,bitmap.height);bitmap.close();
  const type=o.format==='png'?'image/png':'image/jpeg';blob=await canvas.convertToBlob({type,quality:o.quality});
  if(o.target&&type==='image/jpeg'&&blob.size>o.target){let low=.1,high=o.quality;let smallest=await canvas.convertToBlob({type,quality:low});for(let i=0;i<7;i++){const q=(low+high)/2,b=await canvas.convertToBlob({type,quality:q});if(b.size<=o.target){smallest=b;low=q;}else high=q;}blob=smallest;}
 }
 if(o.target&&blob.size>o.target)warn('ไฟล์ยังเกินขนาดเป้าหมายที่ตั้งไว้ แต่ระบบไม่หยุดงานและยังอัปโหลดได้ตามปกติ');
 self.postMessage({blob,preview,warning:warnings.join(' · ')});
}catch(e){self.postMessage({error:e.message});}};
