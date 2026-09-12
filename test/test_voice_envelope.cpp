#include "../src/audio/VoiceEnvelope.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main() {
    VoiceEnvelope meter;
    int16_t quiet[]={0,0,0,0}, speech[]={4096,-4096,4096,-4096};
    int16_t extreme[]={-32768,32767};
    meter.push(quiet,4,100);assert(meter.level(100)==0);
    meter.push(speech,4,200);assert(std::abs(meter.level(200)-.125f)<.0001f);
    assert(std::abs(meter.level(310)-.0625f)<.0001f);
    assert(meter.level(360)==0); // no mouth held open after playback stalls
    meter.push(speech,4,400,50);assert(std::abs(meter.level(400)-.0625f)<.0001f);
    meter.push(speech,4,410,0);assert(meter.level(410)==0);
    meter.push(extreme,2,500);assert(meter.level(500)>.99f && meter.level(500)<=1);
    meter.reset();assert(meter.level(510)==0);
    meter.push(speech,4,0xfffffff0u);assert(meter.level(20)>.12f);
    assert(meter.level(200)==0);
    std::puts("PASS: frame energy, signed extrema, volume/mute, stale decay, interruption reset and clock wrap");
}
