#ifndef ARDUINO
#include "../src/core/UserSettings.hpp"
#include "support/Preferences.h"
#include "../src/audio/PlaybackEvents.hpp"
#include <cassert>
#include <cstring>
#include <set>

static void volume_mapping() {
    assert(preferences::hardwareVolume(0)==0);
    assert(preferences::hardwareVolume(50)==11);
    assert(preferences::hardwareVolume(100)==21);
    assert(preferences::hardwareVolume(-10)==0);
    assert(preferences::hardwareVolume(110)==21);
    int previous=0; std::set<int> levels;
    for(int p=0;p<=100;++p) { int level=preferences::hardwareVolume(p);assert(level>=previous);previous=level;levels.insert(level); }
    assert(levels.size()==22);
}
static void volume_steps() {
    for(int step=2;step<=5;++step) {
        assert(preferences::adjustVolume(50,1,step)==50+step);
        assert(preferences::adjustVolume(50,-1,step)==50-step);
        assert(preferences::adjustVolume(99,1,step)==100);
        assert(preferences::adjustVolume(1,-1,step)==0);
    }
}
static void sequencing() {
    assert(preferences::nextTrack(3,4,false,0)==0);
    assert(preferences::nextTrack(-1,4,false,0)==0);
    assert(preferences::nextTrack(99,4,false,0)==0);
    assert(preferences::nextTrack(0,0,true,4)==-1);
    assert(preferences::nextTrack(0,1,true,4)==0);
}
static void shuffle() {
    for(int current=0;current<9;++current) {
        std::set<int> found;
        for(unsigned r=0;r<100;++r) {
            int next=preferences::nextTrack(current,9,true,r);
            assert(next>=0 && next<9 && next!=current);found.insert(next);
        }
        assert(found.size()==8);
    }
}
static void time_label() {
    char text[40];preferences::playbackTime(text,sizeof(text),83,245);
    assert(std::strcmp(text,"01:23 / 04:05")==0);
    preferences::playbackTime(text,sizeof(text),0,0);
    assert(std::strcmp(text,"00:00 / 00:00")==0);
}
static void colors() {
    assert(preferences::rgb({0,100,100})==0xff0000);
    assert(preferences::rgb({120,100,100})==0x00ff00);
    assert(preferences::rgb({240,100,100})==0x0000ff);
    assert(preferences::rgb({280,0,100})==0xffffff);
    assert(preferences::rgb({280,80,0})==0);
    assert(preferences::rgb565(0xff0000)==0xf800);
}
static void persistence_roundtrip() {
    Preferences::storage().clear();UserSettings first;first.begin();
    assert(preferences::valid(first.values) && !first.values.voice);
    first.values.volume=75;first.values.volumeStep=3;
    first.values.wifi=1;first.values.admin=1;first.values.voice=1;
    first.values.autoNext=0;first.values.shuffle=1;
    for(auto& color:first.values.colors) color={235,22,91};
    assert(first.save());
    UserSettings rebooted;rebooted.begin();
    assert(std::memcmp(&first.values,&rebooted.values,sizeof(first.values))==0);
}
static void corruption() {
    auto& data=Preferences::storage()["tofan-ui/settings"];data[8]^=0x11;
    UserSettings rebooted;rebooted.begin();
    assert(rebooted.values.volume==50 && rebooted.values.voice==0);
}
static void invalid_values() {
    preferences::Values values;values.volume=101;assert(!preferences::valid(values));
    values=preferences::Values{};values.volumeStep=1;assert(!preferences::valid(values));
    values=preferences::Values{};values.colors[0].hue=360;assert(!preferences::valid(values));
    values=preferences::Values{};values.voice=2;assert(!preferences::valid(values));
    preferences::Record record;record.checksum=preferences::checksum(record.values);record.version=99;
    assert(!preferences::decode(record,values));
}
static void failed_save() {
    UserSettings settings;settings.values.volume=65;assert(settings.save());
    Preferences::failWrite()=true;settings.values.volume=95;assert(!settings.save() && settings.saveFailed);
    Preferences::failWrite()=false;UserSettings rebooted;rebooted.begin();assert(rebooted.values.volume==65);
    assert(settings.save() && !settings.saveFailed);
}
static void completion_events() {
    PlaybackEvents events;
    assert(events.takeCompleted()==0);
    assert(events.beginCommand());
    events.complete(true);
    auto eof=events.takeCompleted();assert(eof!=0 && events.takeCompleted()==0);
    assert(events.beginCommand(eof)); // A natural EOF can advance exactly once.
    assert(!events.beginCommand(eof));
    events.markStarted(3);assert(events.takeStarted()==3 && events.takeStarted()==-1);
    events.complete(false);assert(events.takeCompleted()==0); // Online/AI audio never advances the library.
    events.complete(true);eof=events.takeCompleted();
    events.beginCommand(); // User picks another track before queued autoplay is processed.
    assert(!events.beginCommand(eof));
}
static void voice_gate() {
    assert(shouldRunRecognition(true,true,false));
    assert(!shouldRunRecognition(false,true,false));
    assert(!shouldRunRecognition(true,false,false));
    assert(!shouldRunRecognition(true,true,true));
    assert(!shouldRunRecognition(false,true,true));
}
int main() {
    volume_mapping();volume_steps();sequencing();shuffle();time_label();colors();
    persistence_roundtrip();corruption();invalid_values();failed_save();completion_events();voice_gate();
    std::puts("PASS: 12 settings, persistence, volume, shuffle, theme, EOF and voice test groups");
}
#endif
