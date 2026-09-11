export const widgets = {network:'เครือข่าย',storage:'พื้นที่จัดเก็บ',music:'กำลังเล่น',volume:'ระดับเสียง',voice:'Voice assistant',system:'ระบบ'};
export const snapWidth=n=>Math.max(3,Math.min(12,Math.round((Number(n)||6)/3)*3));
export const snapHeight=n=>220+Math.max(0,Math.min(5,Math.round(((Number(n)||220)-220)/80)))*80;
export const defaults = () => Object.keys(widgets).map(id=>({id,w:6,h:220}));
export function layout(value){
 if(!Array.isArray(value))return defaults();const seen=new Set();
 return value.filter(x=>x&&widgets[x.id]&&!seen.has(x.id)&&seen.add(x.id)).map(x=>({id:x.id,w:snapWidth(x.w),h:snapHeight(x.h)}));
}
export function rect(sw,sh,w,h,mode='cover',zoom=1,panX=0,panY=0){
 if(![sw,sh,w,h,zoom].every(n=>Number.isFinite(n)&&n>0))throw Error('ขนาดภาพไม่ถูกต้อง');
 const scale=(mode==='contain'?Math.min(w/sw,h/sh):Math.max(w/sw,h/sh))*zoom;
 const width=sw*scale,height=sh*scale;
 return {x:(w-width)/2+Math.max(0,(width-w)/2)*Math.max(-1,Math.min(1,panX)),y:(h-height)/2+Math.max(0,(height-h)/2)*Math.max(-1,Math.min(1,panY)),width,height};
}
export function dimensions(w,h){return Number.isInteger(w)&&Number.isInteger(h)&&w>0&&h>0&&w<=4096&&h<=4096&&w*h<=4194304;}
export const bytes=n=>n>=1073741824?`${(n/1073741824).toFixed(1)} GB`:n>=1048576?`${(n/1048576).toFixed(1)} MB`:n>=1024?`${(n/1024).toFixed(1)} KB`:`${n||0} B`;
export const time=n=>`${Math.floor((n||0)/60).toString().padStart(2,'0')}:${((n||0)%60).toString().padStart(2,'0')}`;

export const playableAudio=name=>/\.(mp3|wav|aac|m4a|flac)$/i.test(String(name||''));
export function mediaDirectory(name){
 const value=String(name||'');
 if(/\.(jpe?g|png|gif)$/i.test(value))return 'Pictures';
 if(playableAudio(value))return 'Musics';
 if(/\.(mp4|webm|mov|m4v|avi|mjpeg|mjpg)$/i.test(value))return 'Videos';
 return 'Files';
}
export function directAudioNameFromUrl(value){
 try{const u=new URL(value);const part=decodeURIComponent(u.pathname.split('/').pop()||'').trim();return playableAudio(part)?part:'';}catch{return '';}
}
export function blockedYouTubeUrl(value){
 try{const host=new URL(value).hostname.toLowerCase();return host==='youtu.be'||host.endsWith('.youtu.be')||host==='youtube.com'||host.endsWith('.youtube.com')||host.endsWith('.youtube-nocookie.com')||host.endsWith('.googlevideo.com');}catch{return false;}
}
