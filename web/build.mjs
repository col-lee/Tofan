import {build} from 'esbuild';
import {mkdir,readFile,writeFile,copyFile} from 'node:fs/promises';
import {gzipSync} from 'node:zlib';
await mkdir('dist',{recursive:true});
const licenses=await Promise.all(['gifenc/LICENSE.md','gifuct-js/LICENSE','js-binary-schema-parser/LICENSE'].map(async path=>`${path}\n${await readFile('node_modules/'+path,'utf8')}`));
await build({entryPoints:{app:'app.mjs','image-worker':'image-worker.mjs'},bundle:true,minify:true,outdir:'dist',format:'esm',target:'es2020',legalComments:'eof',banner:{js:'/*!\n'+licenses.join('\n\n')+'\n*/'}});
for(const file of ['index.html','style.css'])await copyFile(file,`dist/${file}`);
let out='#pragma once\n#include <Arduino.h>\nstruct PortalAsset { const char* path; const char* mime; const uint8_t* data; size_t size; };\n';
const assets=[['index.html','/','text/html; charset=utf-8'],['style.css','/style.css','text/css'],['app.js','/app.js','text/javascript'],['image-worker.js','/image-worker.js','text/javascript']];
for(let i=0;i<assets.length;i++){const bytes=gzipSync(await readFile(`dist/${assets[i][0]}`),{level:9});out+=`static const uint8_t portalAsset${i}[] PROGMEM = {${[...bytes].join(',')}};\n`;}
out+='static const PortalAsset portalAssets[] = {\n'+assets.map(([f,p,m],i)=>`{"${p}","${m}",portalAsset${i},sizeof(portalAsset${i})}`).join(',\n')+'\n};\n';
await writeFile('../src/network/PortalAssets.hpp',out);console.log('Built local portal + embedded gzip assets');
