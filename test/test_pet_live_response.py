"""Run production Live-response predicate and both pet mood update paths."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
ai=(root/'src/ai/AIConversation.cpp').read_text(encoding='utf8')
app=(root/'src/app/AppCoordinator.cpp').read_text(encoding='utf8')
def function(source, signature):
    start=source.index(signature);brace=source.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
predicate=function(ai,'bool AIConversation::isLiveResponseActive() const')
assignment=app[app.index('    DISM.petSpeaking ='):app.index('    DISM.petSpeechLevel =')]
mood=app[app.index('    if (DISM.petSpeaking &&'):app.index('    // User speech reaction')]
listening=function(app,'void AppCoordinator::updateAiPetListening()')
listening=listening[listening.index('        if (aiConversation.isLiveSessionReady()'):listening.index('        return;')]
prefix=r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
uint32_t now=0;bool buffered=false;
bool livePcmHasBufferedAudio(){return buffered;}
class AIConversation{public:
 bool ready=true,provider=true;std::atomic<bool> liveModelTurnActive{false};
 bool isLiveSessionReady()const{return ready;}bool isLiveProvider()const{return provider;}
 bool isLiveResponseActive()const;
} aiConversation;
namespace app {struct {bool aiPetListening=true;} runtime;}
namespace ui {enum class PetMood{Happy,Listening};}
struct {bool petSpeaking=false;ui::PetMood petMood=ui::PetMood::Listening;uint32_t until=0;int listenWrites=0;
 bool isPetMoodHeld(){return until&&int32_t(until-now)>0;}
 void setPetMood(ui::PetMood m,const char*,unsigned ms){petMood=m;until=now+ms;if(m==ui::PetMood::Listening)++listenWrites;}
} DISM;
'''
code=prefix+predicate+'\nvoid listeningStep(){'+listening+'}\nvoid behavior(){'+assignment+mood+'}\n'+r'''
void step(uint32_t t){now=t;listeningStep();behavior();}
int main(){
 step(10);assert(!DISM.petSpeaking&&DISM.petMood==ui::PetMood::Listening);
 aiConversation.liveModelTurnActive=true;buffered=true;step(20);assert(DISM.petSpeaking&&DISM.petMood==ui::PetMood::Happy);
 int writes=DISM.listenWrites;
 // Repeat packet gaps across multiple 500 ms mood-hold expirations.
 for(uint32_t t=30;t<6000;t+=50){buffered=(t%300)<100;step(t);assert(DISM.petSpeaking&&DISM.petMood==ui::PetMood::Happy&&DISM.listenWrites==writes);}
 // Server turnComplete can precede local playback completion.
 aiConversation.liveModelTurnActive=false;buffered=true;step(6500);assert(DISM.petSpeaking&&DISM.listenWrites==writes);
 buffered=false;step(7100);assert(!DISM.petSpeaking&&DISM.petMood==ui::PetMood::Listening);
 aiConversation.liveModelTurnActive=true;buffered=false;step(7200);assert(DISM.petSpeaking);
 // Interrupted response clears active turn/output; disconnect overrides stale flags.
 aiConversation.liveModelTurnActive=false;assert(!aiConversation.isLiveResponseActive());
 aiConversation.liveModelTurnActive=true;buffered=true;aiConversation.ready=false;step(7300);assert(!DISM.petSpeaking);
 aiConversation.ready=true;app::runtime.aiPetListening=false;step(7400);assert(!DISM.petSpeaking);
 app::runtime.aiPetListening=true;aiConversation.provider=false;behavior();assert(!DISM.petSpeaking);
 std::cout<<"PASS: packet gaps, mood expiry, turn completion with queued audio, next turn, interruption, disconnect and provider/exit guards\n";
}
'''
out=root/'.pio/pet-response-test';out.mkdir(exist_ok=True)
(out/'main.cpp').write_text(code,encoding='utf8')
subprocess.run(['g++','-std=c++17',str(out/'main.cpp'),'-o',str(out/'test.exe')],check=True)
subprocess.run([str(out/'test.exe')],check=True)
