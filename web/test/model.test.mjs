import test from 'node:test';import assert from 'node:assert/strict';import {layout,rect,dimensions,snapWidth,snapHeight} from '../model.mjs';
test('layout restores bounds, drops unknown and duplicate widgets',()=>{assert.deepEqual(layout([{id:'music',w:99,h:0},{id:'music'},{id:'bad'},{id:'volume',w:-2,h:999}]),[{id:'music',w:12,h:220},{id:'volume',w:3,h:620}]);assert.deepEqual(layout([]),[]);assert.equal(layout(null).length,6);});
test('cover crops centrally and pan never exposes background',()=>{assert.deepEqual(rect(640,240,320,240),{x:-160,y:0,width:640,height:240});assert.equal(rect(640,240,320,240,'cover',1,1).x,0);assert.equal(rect(640,240,320,240,'cover',1,-10).x,-320);});
test('contain preserves entire image and borders',()=>{assert.deepEqual(rect(640,240,320,240,'contain'),{x:0,y:60,width:320,height:120});});
test('reject invalid and memory-heavy dimensions',()=>{assert.equal(dimensions(320,240),true);for(const d of [[0,10],[1.2,10],[4097,1],[4096,4096]])assert.equal(dimensions(...d),false);assert.throws(()=>rect(0,1,320,240));});

test('snap quantizes and migrates arbitrary sizes consistently',()=>{assert.equal(snapWidth(4),3);assert.equal(snapWidth(5),6);assert.equal(snapHeight(259),220);assert.equal(snapHeight(261),300);assert.equal(snapHeight(9999),620);assert.deepEqual(layout([{id:'music',w:7,h:301}]),[{id:'music',w:6,h:300}]);});
