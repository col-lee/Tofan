#include "../src/core/UserSettings.hpp"
#include "support/Preferences.h"
#include <cassert>
#include <cstring>
#include <set>
struct LegacyValues {
    preferences::HSV colors[preferences::ColorCount];
    uint8_t volume,volumeStep,reserved,wifi,admin,voice,autoNext,shuffle;
};
int main(){
    static_assert(sizeof(LegacyValues)==sizeof(preferences::Values),"Preserve saved setting layout");
    static_assert(offsetof(LegacyValues,reserved)==offsetof(preferences::Values,petPersonality),"Preserve reserved-byte offset");
    Preferences::storage().clear();
    // A version-1 record with the old reserved byte still loads without resetting preferences.
    preferences::Record record;record.values.volume=85;record.values.wifi=1;record.values.petPersonality=0;
    record.checksum=preferences::checksum(record.values);
    Preferences p;p.begin("tofan-ui",false);p.putBytes("settings",&record,sizeof(record));p.end();
    UserSettings original;original.begin();assert(original.values.volume==85&&original.values.wifi==1&&original.values.petPersonality==0);
    std::set<uint32_t> backgrounds;
    for(unsigned i=0;i<pet::personalityCount;++i){
        original.values.petPersonality=i;assert(original.save());
        UserSettings rebooted;rebooted.begin();assert(rebooted.values.petPersonality==i&&rebooted.values.volume==85);
        backgrounds.insert(pet::appearance(i).background);
    }
    assert(backgrounds.size()==pet::personalityCount);
    original.values.petPersonality=255;assert(!original.save());
    UserSettings valid;valid.begin();assert(valid.values.petPersonality==pet::personalityCount-1);
    assert(pet::appearance(255).background==pet::appearance(0).background);
    Preferences::failWrite()=true;valid.values.petPersonality=2;assert(!valid.save());Preferences::failWrite()=false;
    UserSettings retained;retained.begin();assert(retained.values.petPersonality==pet::personalityCount-1);
    auto custom=retained.customPet;custom.base=7;custom.background=0x123456;custom.eyeWidth=125;
    std::strcpy(custom.voice,"Fenrir");std::strcpy(custom.prompt,"My saved custom persona");
    assert(retained.saveCustomPet(custom));UserSettings customBoot;customBoot.begin();
    assert(customBoot.customPet.base==7&&customBoot.customPet.background==0x123456&&customBoot.customPet.eyeWidth==125);
    assert(std::strcmp(customBoot.customPet.prompt,"My saved custom persona")==0);
    custom.base=8;assert(!retained.saveCustomPet(custom));custom.base=7;
    custom.eyeWidth=255;assert(!retained.saveCustomPet(custom));custom.eyeWidth=125;
    custom.background=0x1000000;assert(!retained.saveCustomPet(custom));custom.background=0x112233;
    Preferences::failWrite()=true;assert(!retained.saveCustomPet(custom));Preferences::failWrite()=false;
    assert(retained.customPet.background==0x123456);
    auto& customBytes=Preferences::storage()["tofan-ui/petCustom"];customBytes[10]^=0x11;
    UserSettings corruptedCustom;corruptedCustom.begin();assert(corruptedCustom.customPet.background==pet::Custom{}.background);
    assert(corruptedCustom.values.petPersonality==8); // Custom corruption does not reset ordinary settings.
    std::puts("PASS: nine pet profiles, legacy settings compatibility, persistence, invalid IDs and failed writes");
}
