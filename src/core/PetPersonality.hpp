#pragma once
#include <cstdint>
namespace pet {
// IDs are persisted: append new personalities rather than reordering these.
struct Appearance {
    uint32_t background, face, accent, cheeks;
    float eyeWidth, eyeHeight, blush;
    int radius;
};
constexpr Appearance appearances[]={
    {0x0e111e,0xfff4e6,0x6fe8c9,0xff89ae,1.f,1.f,1.f,14}, // Friendly
    {0x241622,0xffe8d5,0xffaa87,0xff86ac,1.08f,.86f,1.2f,18}, // Playful
    {0x191b30,0xe3ddff,0xbeb0ef,0xb6a3db,.95f,.72f,.65f,18}, // Calm
    {0x101f2b,0xdaf3ff,0x8ed5ef,0xa0cae1,.88f,1.08f,.65f,10}, // Curious
    {0x262014,0xffecc3,0xf4d179,0xe8b58f,1.12f,.8f,.65f,7}, // Confident
    {0x281c2a,0xffe1ed,0xeeb3d5,0xf184b1,.85f,.85f,1.45f,17}, // Shy
    {0x1d2415,0xe8f7c1,0xc9ed7b,0xe7b28b,1.08f,.85f,.8f,10}, // Cheeky
    {0x290e14,0xff684c,0xffb04f,0x9c3343,1.2f,.65f,.25f,4}, // Savage
    {0x102523,0xd9fff1,0x80e2bd,0xe69ab8,1.f,1.f,1.f,14}, // Custom
};
constexpr unsigned personalityCount=sizeof(appearances)/sizeof(appearances[0]);
inline const Appearance& appearance(unsigned id){return appearances[id<personalityCount?id:0];}
struct Custom {
    uint32_t background=0x102523,face=0xd9fff1,accent=0x80e2bd,cheeks=0xe69ab8;
    uint8_t eyeWidth=100,eyeHeight=100,blush=100,radius=14,base=0;
    char voice[32]="Kore";
    char prompt[512]="You are ToFan, my custom desk companion. Be helpful and concise. Reply in the user's language.";
};
inline bool valid(const Custom& c){
    if(c.background>0xffffff||c.face>0xffffff||c.accent>0xffffff||c.cheeks>0xffffff)return false;
    if(c.eyeWidth<60||c.eyeWidth>130||c.eyeHeight<30||c.eyeHeight>130||c.blush>150||c.radius>24||c.base>7)return false;
    bool voiceEnd=false,promptEnd=false;
    for(char ch:c.voice)if(ch==0)voiceEnd=true;
    for(char ch:c.prompt)if(ch==0)promptEnd=true;
    return voiceEnd&&promptEnd&&c.voice[0]&&c.prompt[0];
}
inline Appearance appearance(const Custom& c){return {c.background,c.face,c.accent,c.cheeks,c.eyeWidth/100.f,c.eyeHeight/100.f,c.blush/100.f,c.radius};}
struct CustomRecord {uint32_t version=1;Custom value;uint32_t checksum=0;};
inline uint32_t checksum(const Custom& c){uint32_t hash=2166136261u;const auto* bytes=reinterpret_cast<const uint8_t*>(&c);for(unsigned i=0;i<sizeof(c);++i)hash=(hash^bytes[i])*16777619u;return hash;}
}
