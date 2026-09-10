#include "../src/network/WebPolicy.hpp"
#include <cassert>
#include <initializer_list>
#include <cstdio>
int main(){
 assert(portal::accountName("new-owner"));assert(portal::accountName("ชื่อใหม่"));
 for(auto s:{""," leading","trailing ","bad\nname","12345678901234567890123456789012"})assert(!portal::accountName(s));
 assert(portal::filename("holiday.JPG"));assert(portal::filename("ภาพ.png"));
 for(auto s:{"","../a.jpg","a/b.jpg","a\\b.jpg",".hidden","a:","a\n.jpg","a.jpg ","a."})assert(!portal::filename(s));
 assert(portal::directory("Pictures"));assert(!portal::directory("Pictures/../"));assert(!portal::directory("/main"));
 uint8_t h[24]{};h[0]=0xe9;h[1]=2;h[12]=9;assert(portal::imageHeader(h,24));assert(!portal::imageHeader(h,23));h[12]=0;assert(!portal::imageHeader(h,24));h[12]=9;h[0]=0;assert(!portal::imageHeader(h,24));
 assert(portal::firmwareSize(24,500));assert(!portal::firmwareSize(23,500));assert(!portal::firmwareSize(501,500));
 assert(portal::uploadChunkValid(100,0,0,10,false));assert(portal::uploadChunkValid(100,10,10,90,false));
 assert(!portal::uploadChunkValid(100,10,0,10,false)); // duplicate / second multipart file
 assert(!portal::uploadChunkValid(100,10,20,10,false)); // gap
 assert(!portal::uploadChunkValid(100,90,90,11,false)); // overflow
 assert(!portal::uploadChunkValid(100,101,101,1,false)); // no unsigned underflow
 assert(!portal::uploadChunkValid(100,100,100,0,true)); // already finalized
 puts("Web path and firmware policy checks passed");
}
