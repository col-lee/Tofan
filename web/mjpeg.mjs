// Raw concatenated JPEG frames. Segment lengths protect APP/EXIF data from
// being mistaken for image boundaries; entropy escapes and restart markers stay intact.
export function* mjpegFrames(bytes) {
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
