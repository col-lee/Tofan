import {test} from 'node:test';
import assert from 'node:assert/strict';
import {mjpegFrames} from '../mjpeg.mjs';
test('MJPEG skips metadata EOI and entropy escapes, preserving all frames',()=>{
 const frame=[255,216,255,225,0,6,255,217,1,2,255,218,0,2,1,255,0,2,255,208,3,255,217];
 assert.deepEqual([...mjpegFrames(Uint8Array.from([...frame,...frame]))],[[0,frame.length],[frame.length,frame.length*2]]);
});
test('MJPEG rejects empty, truncated and malformed input',()=>{
 for(const raw of [[],[1,2],[255,216],[255,216,255,225,0,9,1],[255,216,255,218,0,2,1]])
  assert.throws(()=>[...mjpegFrames(Uint8Array.from(raw))]);
});

test('MJPEG has no arbitrary 2-minute / 1440-frame parser limit',()=>{
 const frame=Uint8Array.from([255,216,255,217]);
 const bytes=new Uint8Array(frame.length*1501);for(let i=0;i<1501;i++)bytes.set(frame,i*frame.length);
 assert.equal([...mjpegFrames(bytes)].length,1501);
});
