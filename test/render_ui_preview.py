"""Run real DisplayManager rendering through a desktop geometry adapter."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
output = root / 'docs/build/ui-preview'
output.mkdir(parents=True, exist_ok=True)
header = (root / 'src/display/DisplayManager.hpp').read_text(encoding='utf-8')
header = re.sub(r'^#include .*$', '', header, flags=re.M)
source = (root / 'src/display/DisplayManager.cpp').read_text(encoding='utf-8')
names = ['applyTheme','present','pageHeader','footer','toggle','drawLoading','drawHomeMenu',
         'drawMusicPlayer','drawMusicSurface','drawPopupNoMusic','drawOnlineMusicPlayer',
         'drawVolumeOverlay','mediaList','drawMusicList','drawImageList','settingsCount',
         'drawSettings','drawColorPicker','drawAIPet','debug','recorde']
functions = []
for name in names:
    start = re.search(r'(?:void|int) DisplayManager::' + name + r'\(', source).start()
    following = re.search(r'\n(?:void|int|bool) DisplayManager::', source[start+1:])
    end = start+1+following.start() if following else len(source)
    functions.append(source[start:end])
main = r'''
DisplayManager::DisplayManager(){} DisplayManager::~DisplayManager(){}
int main(){
    DisplayManager ui;ui.applyTheme();
    ui.drawHomeMenu();spr.save("01-home");
    ui.playlistNames={"Morning light","Slow waves","Quiet afternoon"};
    ui.drawMusicPlayer(currentSongTitle,34,true);spr.save("02-music");
    ui.drawSettings();spr.save("03-settings");
    ui.settingsPage=ui::SettingsPage::Display;ui.drawSettings();spr.save("04-display");
    ui.settingsPage=ui::SettingsPage::Sound;ui.drawSettings();spr.save("05-sound");
    ui.settingsPage=ui::SettingsPage::Voice;ui.drawSettings();spr.save("06-voice");
    ui.colorRole=preferences::Accent;ui.drawColorPicker();spr.save("07-color");
    ui.recorde();spr.save("08-recorder");
    ui.drawVolumeOverlay();spr.save("09-volume");
    ui.drawMusicList();spr.save("10-tracks");
    ui.imageNames={"Beach.jpg","Clouds.jpg","Forest.gif"};ui.drawImageList();spr.save("11-pictures");
    ui.drawAIPet();spr.save("12-pet");
    ui.petSpeaking=true;ui.petSpeechLevel=.8f;
    for(int i=0;i<12;++i){simulatedMillis+=33;ui.drawAIPet();}spr.save("12-pet-speaking");
    ui.petSpeechLevel=0;
    for(int i=0;i<12;++i){simulatedMillis+=33;ui.drawAIPet();}spr.save("12-pet-pause");
    ui.petSpeaking=false;ui.petMood=ui::PetMood::Listening;ui.petVoiceLevel=.8f;
    for(int i=0;i<12;++i){simulatedMillis+=33;ui.drawAIPet();}spr.save("12-pet-listening");
    ui.debug();spr.save("13-devices");
    ui.settingsPage=ui::SettingsPage::Network;ui.drawSettings();spr.save("14-network");
    ui.drawPopupNoMusic();spr.save("15-connect");
    ui.drawOnlineMusicPlayer();spr.save("16-online");
}
'''
(root / '.pio/ui-preview.cpp').write_text('#include "../test/support/UiPreview.hpp"\n' + header + '\n' + '\n'.join(functions) + main, encoding='utf-8')
