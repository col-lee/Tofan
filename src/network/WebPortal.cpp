#include "WebPortal.hpp"
#include "WebPolicy.hpp"
#include "PortalAssets.hpp"
#include "Network.hpp"
#include "../core/UserSettings.hpp"
#include "../core/GlobalState.hpp"
#include "../audio/SoundManager.hpp"
#include "../display/DisplayManager.hpp"
#include "../ai/AIConversation.hpp"
#include <esp_ota_ops.h>
#include <esp_http_client.h>
// Arduino shadows the SDK header with an identically named header.
// Use the ESP-IDF built-in certificate bundle, not Arduino's unset custom bundle.
extern "C" esp_err_t esp_crt_bundle_attach(void* conf);
#include <mbedtls/sha256.h>
#include <atomic>
#include <new>
#include <memory>
#include <algorithm>
#include <time.h>

namespace {
SemaphoreHandle_t portalMutex = nullptr;
QueueHandle_t commands = nullptr;
struct Command { enum Kind { Settings, Wifi, AI } kind; char body[1536]; };
String snapshot = "{}", account, salt, passwordHash, session;
uint32_t sessionAt = 0, lastLogin = 0;
std::atomic<bool> transfer{false}, updating{false};
std::atomic<uint32_t> rebootAt{0};
String otaState = "idle", otaError;
size_t otaDone = 0, otaTotal = 0;
String settingsResult;
uint32_t settingsRevision=0;
struct Guard { SemaphoreHandle_t m; bool held; Guard(SemaphoreHandle_t m):m(m),held(m && xSemaphoreTake(m,pdMS_TO_TICKS(1500))==pdTRUE){} ~Guard(){if(held)xSemaphoreGive(m);} };
void reply(AsyncWebServerRequest* r,int code,const String& message,bool ok=false) {
    JsonDocument d; d["ok"]=ok; d[ok?"message":"error"]=message; String out; serializeJson(d,out);
    auto* response=r->beginResponse(code,"application/json",out); response->addHeader("Cache-Control","no-store"); r->send(response);
}
String param(AsyncWebServerRequest* r,const char* name,bool post=true) { return r->hasParam(name,post)?r->getParam(name,post)->value():String(); }
String randomHex() { char b[49]; for(int i=0;i<6;i++) snprintf(b+i*8,9,"%08lx",static_cast<unsigned long>(esp_random())); return b; }
String hashPassword(const String& pass,const String& s) {
    String input=s+":"+pass; uint8_t digest[32]; mbedtls_sha256_ret(reinterpret_cast<const uint8_t*>(input.c_str()),input.length(),digest,0);
    // Iterated salted hash; no plaintext credential is retained in NVS.
    for(int i=0;i<10000;i++) mbedtls_sha256_ret(digest,32,digest,0);
    char hex[65]; for(int i=0;i<32;i++) snprintf(hex+i*2,3,"%02x",digest[i]); return hex;
}
bool saveAccount(const String& user,const String& newSalt,const String& newHash) {
    // One NVS value prevents a power interruption from mixing old and new fields.
    JsonDocument d;d["user"]=user;d["salt"]=newSalt;d["hash"]=newHash;String record;serializeJson(d,record);
    Preferences p;if(!p.begin("portal",false))return false;bool ok=p.putString("account",record)==record.length();p.end();return ok;
}
bool authorized(AsyncWebServerRequest* r,bool mutation=false,bool send=true) {
    bool ok=false; { Guard g(portalMutex); if(g.held) ok=session.length() && millis()-sessionAt<86400000u && r->hasHeader("Authorization") && r->header("Authorization")=="Bearer "+session; }
    if(mutation) ok=ok && r->header("X-ToFan-Client")=="portal";
    if(!ok && send) reply(r,401,"Please sign in again"); return ok;
}
void otaStatus(const char* state,const String& error="",size_t done=0,size_t total=0) { Guard g(portalMutex); if(g.held){otaState=state;otaError=error;otaDone=done;otaTotal=total;} }
size_t slotSize() { const auto* p=esp_ota_get_next_update_partition(nullptr); return p?p->size:0; }
bool deviceBusy() { return networkSettingsBusy() || DISM.currentState==UI_STATE::APP_DISPLAY || app::runtime.isRecordingMode || app::runtime.aiPetProcessing || isPlayingAudio || hasPausedAudio; }
String pathFor(AsyncWebServerRequest* r,bool post) {
    String dir=param(r,"dir",post), name=param(r,"name",post);
    if(!portal::directory(dir.c_str()) || !portal::filename(name.c_str())) return "";
    return "/main/"+dir+"/"+name;
}
bool mediaExtension(const String& name,const String& dir) {
    String s=name.substring(name.lastIndexOf('.')+1); s.toLowerCase();
    if(dir=="Pictures") return s=="jpg"||s=="jpeg"||s=="png"||s=="gif";
    if(dir=="Musics") return s=="mp3"||s=="wav"||s=="aac"||s=="m4a"||s=="flac";
    return dir=="Videos" && (s=="mp4"||s=="webm"||s=="mov"||s=="m4v"||s=="avi"||s=="mjpeg"||s=="mjpg");
}
bool enqueue(AsyncWebServerRequest* r,Command::Kind kind,const String& body) {
    if(body.length()>=sizeof(Command::body)){reply(r,413,"Settings too large");return false;}
    Command c{}; c.kind=kind; body.toCharArray(c.body,sizeof(c.body));
    if(updating || rebootAt.load() || xQueueSend(commands,&c,0)!=pdTRUE){reply(r,409,"Device busy; try again");return false;}
    reply(r,202,"Settings queued",true); return true;
}

struct Upload {
    File file; String temp,path,error; size_t expected=0,received=0; int code=400;
    bool ota=false,owned=false,started=false,finished=false,committed=false;
    uint8_t header[24]{}; size_t headerBytes=0;
};
void dispose(Upload* u) {
    if(!u)return;
    if(u->ota && u->started && !u->committed) Update.abort();
    if(u->temp.length()){Guard g(sdSemaphore);if(g.held){if(u->file)u->file.close();SD.remove(u->temp);}}
    if(u->owned){transfer=false;if(u->ota)updating=false;} delete u;
}
void uploadChunk(AsyncWebServerRequest* r,String filename,size_t index,uint8_t* data,size_t len,bool final,bool ota) {
    auto* u=static_cast<Upload*>(r->_tempObject);
    if(!u) {
        u=new(std::nothrow) Upload; if(!u)return; r->_tempObject=u; u->ota=ota;
        r->onDisconnect([r](){auto* p=static_cast<Upload*>(r->_tempObject);r->_tempObject=nullptr;dispose(p);});
        if(!authorized(r,true,false)){u->code=401;u->error="Sign in required";return;}
        r->client()->setRxTimeout(30);
        String length=r->header("X-File-Size"); char* end=nullptr; unsigned long n=strtoul(length.c_str(),&end,10);
        if(!length.length() || !end || *end || n==0){u->error="Missing valid X-File-Size";return;} u->expected=n;
        if(ota && (!portal::firmwareSize(n,slotSize())||!filename.endsWith(".bin"))){u->error="Invalid firmware size or extension";return;}
        if(!ota && n>portal::MaxUpload){u->code=413;u->error="File exceeds 256 MiB";return;}
        if(rebootAt.load() || (ota && deviceBusy())){u->code=409;u->error="Stop playback and recording before updating";return;}
        bool free=false; if(!transfer.compare_exchange_strong(free,true)){u->code=409;u->error="Another transfer is active";return;} u->owned=true;
        if(ota){updating=true;otaStatus("uploading","",0,n);}
        else {
            const String dir=param(r,"dir",false); u->path=pathFor(r,false);
            if(!u->path.length() || filename!=param(r,"name",false) || !mediaExtension(filename,dir)){u->error="Unsupported filename or directory";return;}
            Guard g(sdSemaphore);
            if(!g.held||!isConnectSDcard){u->code=503;u->error="SD unavailable";return;}
            if(SD.exists(u->path)){u->code=409;u->error="File already exists; rename the upload";return;}
            if(SD.totalBytes()-SD.usedBytes()<n+65536){u->code=507;u->error="Not enough SD space";return;}
            u->temp="/main/.upload-"+randomHex()+".part"; u->file=SD.open(u->temp,FILE_WRITE);
            if(!u->file){u->code=500;u->error="Cannot create temporary file";return;}
        }
    }
    if(u->error.length())return;
    if(!portal::uploadChunkValid(u->expected,u->received,index,len,u->finished)){u->error="Unexpected upload length or multiple files";return;}
    if(ota) {
        size_t used=0;
        while(u->headerBytes<sizeof(u->header) && used<len)u->header[u->headerBytes++]=data[used++];
        if(!u->started && u->headerBytes==sizeof(u->header)) {
            if(!portal::imageHeader(u->header,sizeof(u->header))){u->error="Not an ESP32-S3 application image";return;}
            if(!Update.begin(u->expected,U_FLASH)){u->error=Update.errorString();return;} u->started=true;
            if(Update.write(u->header,sizeof(u->header))!=sizeof(u->header)){u->error=Update.errorString();return;}
        }
        if(u->started && used<len && Update.write(data+used,len-used)!=len-used){u->error=Update.errorString();return;}
    } else { Guard g(sdSemaphore); if(!g.held||u->file.write(data,len)!=len){u->error="SD write failed";u->code=500;return;} }
    u->received+=len; if(ota)otaStatus("uploading","",u->received,u->expected);
    if(final) { if(u->received!=u->expected){u->error="Incomplete upload";return;} u->finished=true; }
}
void uploadComplete(AsyncWebServerRequest* r) {
    auto* u=static_cast<Upload*>(r->_tempObject); if(!u){reply(r,400,"No file received");return;}
    if(u->error.isEmpty() && !u->finished)u->error="Incomplete upload";
    if(u->error.isEmpty()) {
        if(u->ota) {
            if(!u->started || !Update.end(false)){u->error=Update.errorString();u->finished=false;}
            else {u->committed=true;otaStatus("rebooting","",u->received,u->expected);rebootAt=millis()+2500;}
        } else {Guard g(sdSemaphore);if(!g.held)u->error="SD busy";else{u->file.flush();u->file.close();if(SD.exists(u->path)||!SD.rename(u->temp,u->path))u->error="Cannot commit file";}}
    }
    if(u->error.length()){if(u->ota){u->finished=false;otaStatus("error",u->error);}reply(r,u->code,u->error);}
    else reply(r,200,u->ota?"Firmware verified; restarting":"File saved",true);
    r->_tempObject=nullptr;dispose(u);
}

void urlUpdate(void* arg) {
    String url=*static_cast<String*>(arg); delete static_cast<String*>(arg);
    bool begun=false,success=false; String error;
    otaStatus("connecting");
    if(url.startsWith("https://") && time(nullptr)<1700000000){
        configTime(0,0,"pool.ntp.org","time.nist.gov");
        for(int i=0;i<100 && time(nullptr)<1700000000;i++)vTaskDelay(pdMS_TO_TICKS(100));
        if(time(nullptr)<1700000000)error="Clock unavailable for TLS verification";
    }
    esp_http_client_config_t config{};config.url=url.c_str();config.timeout_ms=15000;config.buffer_size=2048;
    config.crt_bundle_attach=esp_crt_bundle_attach;config.disable_auto_redirect=true;
    auto client=error.isEmpty()?esp_http_client_init(&config):nullptr;
    if(!client && error.isEmpty())error="Cannot create HTTP client";
    size_t total=0,done=0; uint8_t bytes[2048];
    if(client) {
        if(esp_http_client_open(client,0)!=ESP_OK)error="Connection failed (check URL, WiFi and TLS certificate)";
        else {
            int64_t length=esp_http_client_fetch_headers(client);int status=esp_http_client_get_status_code(client);
            if(status!=200)error="URL must return firmware directly with HTTP 200; redirects are not followed";
            else if(length<24 || !portal::firmwareSize(static_cast<size_t>(length),slotSize()))error="Missing Content-Length or firmware exceeds OTA partition";
            else total=length;
        }
        while(error.isEmpty() && done<total) {
            int n=esp_http_client_read(client,reinterpret_cast<char*>(bytes),std::min(sizeof(bytes),total-done));
            if(n<=0){error="Download interrupted";break;}
            if(!begun) {
                // TCP reads may return fewer than 24 bytes.
                while(n<24){int extra=esp_http_client_read(client,reinterpret_cast<char*>(bytes+n),24-n);if(extra<=0)break;n+=extra;}
                if(!portal::imageHeader(bytes,n)){error="Not an ESP32-S3 application image";break;}
                if(!Update.begin(total,U_FLASH)){error=Update.errorString();break;} begun=true;
            }
            if(Update.write(bytes,n)!=static_cast<size_t>(n)){error=Update.errorString();break;}
            done+=n;otaStatus("downloading","",done,total);vTaskDelay(1);
        }
        esp_http_client_close(client);esp_http_client_cleanup(client);
    }
    if(error.isEmpty() && begun && done==total){success=Update.end(false);if(!success)error=Update.errorString();}
    if(success){otaStatus("rebooting","",done,total);rebootAt=millis()+2500;}
    else{if(begun)Update.abort();otaStatus("error",error.length()?error:"Empty firmware");}
    transfer=false;updating=false;vTaskDelete(nullptr);
}
}

void registerWebPortal(AsyncWebServer& server) {
    portalMutex=xSemaphoreCreateMutex();commands=xQueueCreate(3,sizeof(Command));
    if(!portalMutex||!commands){Serial.println("Portal initialization failed");return;}
    Preferences p;p.begin("portal",true);String record=p.getString("account","");p.end();JsonDocument saved;
    if(!deserializeJson(saved,record)){account=saved["user"]|"";salt=saved["salt"]|"";passwordHash=saved["hash"]|"";}
    if(account.isEmpty()) {
        // Preserve access for an existing installation while migrating away from plaintext.
        if(p.begin("UsernameConfig",true)){String user=p.getString("username",""),pass=p.getString("password","");p.end();
            if(user.length()&&pass.length()){String s=randomHex(),h=hashPassword(pass,s);if(saveAccount(user,s,h)){account=user;salt=s;passwordHash=h;}}}
    }
    for(const auto& asset:portalAssets){const auto* a=&asset;server.on(a->path,AsyncWebRequestMethod::HTTP_GET,[a](AsyncWebServerRequest* r){
        auto* response=r->beginResponse(200,a->mime,a->data,a->size);response->addHeader("Content-Encoding","gzip");
        response->addHeader("Cache-Control","no-cache");response->addHeader("X-Content-Type-Options","nosniff");
        response->addHeader("Content-Security-Policy","default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' blob: data:; media-src 'self' blob:; worker-src 'self' blob:; connect-src 'self'; object-src 'none'; frame-ancestors 'none'");r->send(response);
    });}
    server.on("/wifiManager",AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){r->redirect("/#wifi");});
    server.on("/index.html",AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){r->redirect("/");});
    for(const char* path:{"/controlPanel.html","/loginPage.html","/WEB_Source/index.html"})server.on(path,AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){r->redirect("/");});
    for(const char* path:{"/uploadFile.html","/pageUploadFile/"})server.on(path,AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){r->redirect("/#files");});
    server.on("/WiFiManger/wifiManager.html",AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){r->redirect("/#wifi");});
    server.on("/api/session",AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){Guard g(portalMutex);r->send(200,"application/json",account.isEmpty()?"{\"setup\":true}":"{\"setup\":false}");});
    server.on("/api/session",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){
        if(r->header("X-ToFan-Client")!="portal"){reply(r,403,"Invalid client");return;}
        if(millis()-lastLogin<1500){reply(r,429,"Please wait before trying again");return;}lastLogin=millis();
        String user=param(r,"username"),pass=param(r,"password");
        if(user.length()<1||user.length()>31||pass.length()<(account.isEmpty()?8:1)||pass.length()>128){reply(r,400,"Username 1–31 characters; new password 8–128 characters");return;}
        Guard g(portalMutex);if(!g.held){reply(r,503,"Busy");return;}
        if(account.isEmpty()){
            if(r->client()->localIP()!=WiFi.softAPIP()){reply(r,403,"Connect to the device WiFi AP for first setup");return;}
            String newSalt=randomHex(),newHash=hashPassword(pass,newSalt);
            bool saved=saveAccount(user,newSalt,newHash);
            if(!saved){reply(r,500,"Cannot save account");return;}account=user;salt=newSalt;passwordHash=newHash;
        }else if(user!=account || hashPassword(pass,salt)!=passwordHash){reply(r,401,"Incorrect username or password");return;}
        session=randomHex();sessionAt=millis();JsonDocument d;d["token"]=session;String out;serializeJson(d,out);r->send(200,"application/json",out);
    });
    server.on("/api/logout",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(!authorized(r,true))return;{Guard g(portalMutex);session="";}reply(r,200,"Signed out",true);});
    server.on("/api/account",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(!authorized(r,true))return;
        String old=param(r,"current"),pass=param(r,"password");Guard g(portalMutex);
        if(!g.held||hashPassword(old,salt)!=passwordHash){reply(r,403,"Incorrect current password");return;}
        if(pass.length()<8||pass.length()>128){reply(r,400,"Password must be 8–128 characters");return;}
        String s=randomHex(),h=hashPassword(pass,s);bool ok=saveAccount(account,s,h);
        if(!ok){reply(r,500,"Cannot save password");return;}salt=s;passwordHash=h;session="";reply(r,200,"Password changed; sign in again",true);
    });
    server.on("/api/status",AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){if(!authorized(r))return;String out;{Guard g(portalMutex);out=snapshot;}auto* response=r->beginResponse(200,"application/json",out);response->addHeader("Cache-Control","no-store");r->send(response);});
    server.on("/api/settings",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(!authorized(r,true))return;enqueue(r,Command::Settings,param(r,"values"));});
    server.on("/api/wifi",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(!authorized(r,true))return;
        String ssid=param(r,"ssid"),pass=param(r,"password");if(ssid.isEmpty()||ssid.length()>32||pass.length()>63||(pass.length()&&pass.length()<8)){reply(r,400,"SSID 1–32 bytes; password empty or 8–63 bytes");return;}
        JsonDocument d;d["ssid"]=ssid;d["password"]=pass;String out;serializeJson(d,out);enqueue(r,Command::Wifi,out);
    });
    server.on("/api/ai",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(authorized(r,true))enqueue(r,Command::AI,param(r,"values"));});
    server.on("/api/files",AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){if(!authorized(r))return;String dir=param(r,"dir",false);
        if(!portal::directory(dir.c_str())){reply(r,400,"Invalid directory");return;}
        JsonDocument d;auto files=d["files"].to<JsonArray>();{Guard g(sdSemaphore);if(!g.held||!isConnectSDcard){reply(r,503,"SD unavailable");return;}
        File root=SD.open("/main/"+dir);if(!root){reply(r,404,"Directory not found");return;}File file=root.openNextFile();int count=0;
        while(file){if(!file.isDirectory()){auto item=files.add<JsonObject>();item["name"]=file.name();item["size"]=file.size();if(++count>=250){d["truncated"]=true;break;}}file.close();file=root.openNextFile();}file.close();root.close();}
        String out;serializeJson(d,out);r->send(200,"application/json",out);
    });
    server.on("/api/file",AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){if(!authorized(r))return;String path=pathFor(r,false);
        if(path.isEmpty()){reply(r,400,"Invalid path");return;}
        // Async response owns an open File; all reads share the SD mutex with playback.
        bool free=false;if(!transfer.compare_exchange_strong(free,true)){reply(r,409,"Another transfer is active");return;}
        File opened;
        {Guard g(sdSemaphore);if(!g.held||!isConnectSDcard){transfer=false;reply(r,503,"SD unavailable");return;}
            opened=SD.open(path,FILE_READ);if(!opened){transfer=false;reply(r,404,"File not found");return;}}
        auto f=std::shared_ptr<File>(new File(opened),[](File* file){Guard g(sdSemaphore);file->close();delete file;transfer=false;});
        auto* response=r->beginResponse("application/octet-stream",f->size(),[f](uint8_t* buffer,size_t maxLen,size_t index)->size_t {Guard guard(sdSemaphore);if(!guard.held)return RESPONSE_TRY_AGAIN;f->seek(index);return f->read(buffer,maxLen);});
        response->addHeader("Cache-Control","no-store");r->send(response);
    });
    server.on("/api/file",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(!authorized(r,true))return;String path=pathFor(r,true),action=param(r,"action");
        if(path.isEmpty()){reply(r,400,"Invalid path");return;}if(transfer||deviceBusy()){reply(r,409,"Stop playback/recording and wait for transfers");return;}
        Guard g(sdSemaphore);if(!g.held||!isConnectSDcard){reply(r,503,"SD unavailable");return;}
        bool ok=false;if(action=="delete")ok=SD.remove(path);else if(action=="rename") {String name=param(r,"newName"),dir=param(r,"dir");String target="/main/"+dir+"/"+name;
            if(!portal::filename(name.c_str())||!mediaExtension(name,dir)){reply(r,400,"Invalid new name");return;}if(SD.exists(target)){reply(r,409,"Name already exists");return;}ok=SD.rename(path,target);}
        reply(r,ok?200:400,ok?"File updated":"File operation failed",ok);
    });
    server.on("/api/upload",AsyncWebRequestMethod::HTTP_POST,uploadComplete,[](AsyncWebServerRequest* r,String name,size_t i,uint8_t* data,size_t len,bool last){uploadChunk(r,name,i,data,len,last,false);});
    server.on("/api/ota/file",AsyncWebRequestMethod::HTTP_POST,uploadComplete,[](AsyncWebServerRequest* r,String name,size_t i,uint8_t* data,size_t len,bool last){uploadChunk(r,name,i,data,len,last,true);});
    server.on("/api/ota",AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){if(!authorized(r))return;JsonDocument d;{Guard g(portalMutex);d["state"]=otaState;d["error"]=otaError;d["done"]=otaDone;d["total"]=otaTotal;}String out;serializeJson(d,out);r->send(200,"application/json",out);});
    server.on("/api/ota/url",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(!authorized(r,true))return;String url=param(r,"url");
        if(url.length()>1023||url.indexOf('@')>=0||url.indexOf('\r')>=0||url.indexOf('\n')>=0||(!url.startsWith("https://")&&!url.startsWith("http://"))){reply(r,400,"Use a direct HTTP(S) firmware URL without credentials");return;}
        if(rebootAt.load()||WiFi.status()!=WL_CONNECTED||deviceBusy()){reply(r,409,"Connect WiFi and stop playback/recording first");return;}
        bool free=false;if(!transfer.compare_exchange_strong(free,true)){reply(r,409,"Another transfer is active");return;}updating=true;
        auto* job=new(std::nothrow) String(url);if(!job||xTaskCreate(urlUpdate,"portalOTA",8192,job,2,nullptr)!=pdPASS){delete job;transfer=false;updating=false;reply(r,503,"Not enough memory");return;}
        reply(r,202,"Firmware download started",true);
    });
    server.onNotFound([](AsyncWebServerRequest* r){reply(r,404,"Not found");});
}

void serviceWebPortal() {
    if(!commands||!portalMutex)return;
    static bool wasUpdating=false;bool active=webFirmwareUpdating();
    if(active!=wasUpdating){setVoiceAssistantEnabled(active?false:userSettings.values.voice);wasUpdating=active;}
    uint32_t restart=rebootAt.load();if(restart && static_cast<int32_t>(millis()-restart)>=0)ESP.restart();
    Command c{};
    if(!updating && xQueueReceive(commands,&c,0)==pdTRUE) {
        JsonDocument d;String result="Settings saved";
        if(deserializeJson(d,c.body))result="Invalid settings JSON";
        else if(c.kind==Command::Settings) {
            auto v=userSettings.values;
            // Reject out-of-range values before narrowing to uint8_t.
            bool valid=true;const char* keys[]={"volume","volumeStep","voice","autoNext","shuffle","wifi","admin"};
            uint8_t* fields[]={&v.volume,&v.volumeStep,&v.voice,&v.autoNext,&v.shuffle,&v.wifi,&v.admin};
            for(int i=0;i<7;i++)if(!d[keys[i]].isNull()){int n=d[keys[i]].as<int>();if(!d[keys[i]].is<int>()||n<0||n>100)valid=false;else *fields[i]=n;}
            if(d["colors"].is<JsonArray>()){auto colors=d["colors"].as<JsonArray>();if(colors.size()!=preferences::ColorCount)valid=false;else for(int i=0;i<preferences::ColorCount;i++){int h=colors[i][0]|-1,s=colors[i][1]|-1,b=colors[i][2]|-1;if(h<0||h>=360||s<0||s>100||b<0||b>100)valid=false;else v.colors[i]={static_cast<uint16_t>(h),static_cast<uint8_t>(s),static_cast<uint8_t>(b)};}}
            if(!valid||!preferences::valid(v))result="Invalid settings values";
            else {auto previous=userSettings.values;userSettings.values=v;if(!userSettings.save()){userSettings.values=previous;result="Settings save failed";}else {setOutputVolume(v.volume);setVoiceAssistantEnabled(v.voice);DISM.applyTheme();if(v.wifi!=previous.wifi||v.admin!=previous.admin)requestNetworkSettings(v.wifi,v.admin);}}
        } else if(c.kind==Command::Wifi) {
            Preferences p;if(!p.begin("WiFiConfig",false))result="Cannot save WiFi";
            else{String ssid=d["ssid"]|"",pass=d["password"]|"";bool ok=p.putString("ssid",ssid)==ssid.length() && p.putString("password",pass)==pass.length();p.end();if(ok){userSettings.values.wifi=1;userSettings.values.admin=1;if(!userSettings.save())result="WiFi saved, but startup settings save failed";requestWiFiReconnect();}else result="Cannot save WiFi";}
        } else {if(app::runtime.aiPetProcessing)result="AI busy; retry later";else if(!aiConversation.saveConfig(d))result="Cannot save AI configuration";}
        {Guard g(portalMutex);if(g.held){settingsResult=result;++settingsRevision;}}
    }
    static uint32_t last=0;if(millis()-last<1000)return;last=millis();
    JsonDocument d;d["device"]=String(static_cast<uint32_t>(ESP.getEfuseMac()),HEX);d["uptime"]=millis()/1000;d["heap"]=ESP.getFreeHeap();d["psram"]=ESP.getFreePsram();d["firmware"]=__DATE__ " " __TIME__;d["slotSize"]=slotSize();d["width"]=tft.width();d["height"]=tft.height();
    d["connected"]=WiFi.status()==WL_CONNECTED;d["ssid"]=WiFi.SSID();d["ip"]=WiFi.localIP().toString();d["apIP"]=WiFi.softAPIP().toString();d["rssi"]=WiFi.RSSI();d["networkBusy"]=networkSettingsBusy();
    d["sd"]=isConnectSDcard;{Guard g(sdSemaphore);if(g.held && isConnectSDcard){d["storageTotal"]=SD.totalBytes();d["storageUsed"]=SD.usedBytes();}}
    d["playing"]=isPlayingAudio;d["title"]=currentSongTitle;d["current"]=currentAudioTime;d["duration"]=totalAudioDuration;d["recording"]=app::runtime.isRecording;d["busy"]=transfer.load();
    auto s=d["settings"].to<JsonObject>();const auto& v=userSettings.values;s["volume"]=v.volume;s["volumeStep"]=v.volumeStep;s["voice"]=v.voice;s["autoNext"]=v.autoNext;s["shuffle"]=v.shuffle;s["wifi"]=v.wifi;s["admin"]=v.admin;
    auto colors=s["colors"].to<JsonArray>();for(auto color:v.colors){auto row=colors.add<JsonArray>();row.add(color.hue);row.add(color.saturation);row.add(color.value);}
    JsonDocument ai;deserializeJson(ai,aiConversation.getConfigJson(false));d["ai"]=ai;
    String out;{Guard g(portalMutex);d["settingsResult"]=settingsResult;d["settingsRevision"]=settingsRevision;serializeJson(d,out);if(g.held)snapshot=out;}
}

bool webFirmwareUpdating() { return updating.load() || rebootAt.load()!=0; }
