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
    storage.end();
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
