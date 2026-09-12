#include "UserSettings.hpp"
#include <Preferences.h>
UserSettings userSettings;
void UserSettings::begin() {
    Preferences storage;
    if (!storage.begin("tofan-ui", false)) { saveFailed=true; return; }
    preferences::Record record;
    if (storage.getBytesLength("settings") == sizeof(record) &&
        storage.getBytes("settings", &record, sizeof(record)) == sizeof(record)) {
        preferences::decode(record, values);
    }
    pet::CustomRecord custom;
    if(storage.getBytesLength("petCustom")==sizeof(custom)&&storage.getBytes("petCustom",&custom,sizeof(custom))==sizeof(custom)
       &&custom.version==1&&pet::valid(custom.value)&&custom.checksum==pet::checksum(custom.value))customPet=custom.value;
    storage.end();
}
bool UserSettings::saveCustomPet(const pet::Custom& value){
    if(!pet::valid(value))return false;
    pet::CustomRecord record;record.value=value;record.checksum=pet::checksum(value);
    Preferences storage;if(!storage.begin("tofan-ui",false))return false;
    bool ok=storage.putBytes("petCustom",&record,sizeof(record))==sizeof(record);storage.end();
    if(ok)customPet=value;return ok;
}
bool UserSettings::save() {
    if (!preferences::valid(values)) { saveFailed=true; return false; }
    preferences::Record record;
    record.values=values;
    record.checksum=preferences::checksum(values);
    Preferences storage;
    if (!storage.begin("tofan-ui", false)) { saveFailed=true; return false; }
    saveFailed=storage.putBytes("settings", &record, sizeof(record)) != sizeof(record);
    storage.end();
    return !saveFailed;
}
