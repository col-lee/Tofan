#include "../core/MemoryPolicy.hpp"
#include "WebPortal.hpp"
#include "EspNowManager.hpp"
#include "WebPolicy.hpp"
#include "PortalAssets.hpp"
#include "Network.hpp"
#include "../core/UserSettings.hpp"
#include "../core/GlobalState.hpp"
#include "../audio/SoundManager.hpp"
#include "../display/DisplayManager.hpp"
#include "../ai/AIConversation.hpp"
#include "../ai/ChatHistory.hpp"
#include "../app/AppCoordinator.hpp"
#include "../food/FoodStore.hpp"
#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <esp_heap_caps.h>
// Arduino shadows the SDK header with an identically named header.
// Use the ESP-IDF built-in certificate bundle, not Arduino's unset custom bundle.
extern "C" esp_err_t esp_crt_bundle_attach(void* conf);
#include <mbedtls/sha256.h>
#include <atomic>
#include <new>
#include <memory>
#include <algorithm>
#include <cstdlib>
#include <cerrno>
#include <limits>
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
String audioImportState = "idle", audioImportError;
size_t audioImportDone = 0, audioImportTotal = 0;
String settingsResult;
uint32_t settingsRevision=0;
struct Guard { SemaphoreHandle_t m; bool held; Guard(SemaphoreHandle_t m):m(m),held(m && xSemaphoreTake(m,pdMS_TO_TICKS(1500))==pdTRUE){} ~Guard(){if(held)xSemaphoreGive(m);} };
void reply(AsyncWebServerRequest* r,int code,const String& message,bool ok=false) {
    JsonDocument d(memory::jsonAllocator()); d["ok"]=ok; d[ok?"message":"error"]=message; String out; serializeJson(d,out);
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
    JsonDocument d(memory::jsonAllocator());d["user"]=user;d["salt"]=newSalt;d["hash"]=newHash;String record;serializeJson(d,record);
    Preferences p;if(!p.begin("portal",false))return false;bool ok=p.putString("account",record)==record.length();p.end();return ok;
}
bool authorized(AsyncWebServerRequest* r,bool mutation=false,bool send=true) {
    bool ok=false; { Guard g(portalMutex); if(g.held) ok=session.length() && millis()-sessionAt<86400000u && r->hasHeader("Authorization") && r->header("Authorization")=="Bearer "+session; }
    if(mutation) ok=ok && r->header("X-ToFan-Client")=="portal";
    if(!ok && send) reply(r,401,"Please sign in again"); return ok;
}
void otaStatus(const char* state,const String& error="",size_t done=0,size_t total=0) { Guard g(portalMutex); if(g.held){otaState=state;otaError=error;otaDone=done;otaTotal=total;} }
void audioImportStatus(const char* state,const String& error="",size_t done=0,size_t total=0) { Guard g(portalMutex); if(g.held){audioImportState=state;audioImportError=error;audioImportDone=done;audioImportTotal=total;} }
size_t slotSize() { const auto* p=esp_ota_get_next_update_partition(nullptr); return p?p->size:0; }
bool deviceBusy() { return networkSettingsBusy() || DISM.currentState==UI_STATE::APP_DISPLAY || app::runtime.isRecordingMode || app::runtime.aiPetProcessing || isPlayingAudio || hasPausedAudio; }
String pathFor(AsyncWebServerRequest* r,bool post) {
    String dir=param(r,"dir",post), name=param(r,"name",post);
    if(!portal::directory(dir.c_str()) || !portal::filename(name.c_str())) return "";
    return "/main/"+dir+"/"+name;
}
bool mediaExtension(const String& name,const String& dir) {
    return portal::mediaFilename(name.c_str(),dir.c_str());
}
bool directHttpUrl(const String& url) {
    return url.length() <= 1023 && url.indexOf('@') < 0 && url.indexOf('\r') < 0 && url.indexOf('\n') < 0 &&
           (url.startsWith("https://") || url.startsWith("http://"));
}
bool blockedYouTubeUrl(const String& url) {
    String lower=url; lower.toLowerCase();
    return lower.indexOf("youtube.com") >= 0 || lower.indexOf("youtu.be") >= 0 ||
           lower.indexOf("youtube-nocookie.com") >= 0 || lower.indexOf("googlevideo.com") >= 0;
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
bool writeUploadChunkCooperative(File& file,const uint8_t* data,size_t length) {
    constexpr size_t sliceBytes = 4 * 1024;
    size_t offset = 0;
    while (offset < length) {
        const size_t slice = std::min(sliceBytes, length - offset);
        if (xSemaphoreTake(sdSemaphore, pdMS_TO_TICKS(100)) != pdTRUE) return false;
        const size_t written = file.write(data + offset, slice);
        xSemaphoreGive(sdSemaphore);
        if (written != slice) return false;
        offset += written;

        // Never yield while owning sdSemaphore. This keeps MJPEG/audio prefetch
        // responsive even during large browser uploads.
        taskYIELD();
    }
    return true;
}

void uploadChunk(AsyncWebServerRequest* r,String filename,size_t index,uint8_t* data,size_t len,bool final,bool ota) {
    auto* u=static_cast<Upload*>(r->_tempObject);
    if(!u) {
        u=new(std::nothrow) Upload; if(!u)return; r->_tempObject=u; u->ota=ota;
        r->onDisconnect([r](){auto* p=static_cast<Upload*>(r->_tempObject);r->_tempObject=nullptr;dispose(p);});
        if(!authorized(r,true,false)){u->code=401;u->error="Sign in required";return;}
        r->client()->setRxTimeout(30);
        String length=r->header("X-File-Size"); char* end=nullptr; errno=0; unsigned long long n64=strtoull(length.c_str(),&end,10);
        if(!length.length() || !end || *end || errno==ERANGE || n64==0 || n64>static_cast<unsigned long long>(std::numeric_limits<size_t>::max())){u->code=413;u->error="Invalid file size for this device";return;}
        const size_t n=static_cast<size_t>(n64);u->expected=n;
        if(ota && (!portal::firmwareSize(n,slotSize())||!filename.endsWith(".bin"))){u->error="Invalid firmware size or extension";return;}
        if(rebootAt.load() || (ota && deviceBusy())){u->code=409;u->error="Stop playback and recording before updating";return;}
        bool free=false; if(!transfer.compare_exchange_strong(free,true)){u->code=409;u->error="Another transfer is active";return;} u->owned=true;
        if(ota){updating=true;otaStatus("uploading","",0,n);}
        else {
            const String dir=param(r,"dir",false); u->path=pathFor(r,false);
            if(!u->path.length() || filename!=param(r,"name",false) || !mediaExtension(filename,dir)){u->error="Unsupported filename or directory";return;}
            Guard g(sdSemaphore);
            if(!g.held||!isConnectSDcard){u->code=503;u->error="SD unavailable";return;}
            if(SD.exists(u->path)){u->code=409;u->error="File already exists; rename the upload";return;}
            const uint64_t total=SD.totalBytes(),used=SD.usedBytes();
            if(!portal::uploadFits(static_cast<uint64_t>(n),total,used)){u->code=507;u->error="Not enough SD space";return;}
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
    } else {
        if(!writeUploadChunkCooperative(u->file,data,len)){u->error="SD write failed";u->code=500;return;}
    }
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

struct AudioImportJob { String url; String name; };

bool writeImportChunk(File& file,const uint8_t* data,size_t length,String& error) {
    constexpr size_t sliceBytes = 4 * 1024;
    size_t offset=0;
    while(offset<length) {
        const size_t slice=std::min(sliceBytes,length-offset);
        {
            Guard g(sdSemaphore);
            if(!g.held){error="SD busy during download";return false;}
            if(file.write(data+offset,slice)!=slice){error="SD write failed";return false;}
        }
        offset+=slice;
        // Audio task has a higher priority; yield only after releasing SD so its
        // PSRAM read-ahead buffer can be topped up during a long import.
        vTaskDelay(1);
    }
    return true;
}

void audioImportTask(void* arg) {
    std::unique_ptr<AudioImportJob> job(static_cast<AudioImportJob*>(arg));
    const String path="/main/Musics/"+job->name;
    const String temp="/main/.audio-"+randomHex()+".part";
    String error;
    size_t done=0,total=0;
    File file;
    esp_http_client_handle_t client=nullptr;
    uint8_t* buffer=nullptr;
    size_t bufferSize=32*1024;
    audioImportStatus("connecting");

    if(job->url.startsWith("https://") && time(nullptr)<1700000000){
        configTime(0,0,"pool.ntp.org","time.nist.gov");
        for(int i=0;i<100 && time(nullptr)<1700000000;i++)vTaskDelay(pdMS_TO_TICKS(100));
        if(time(nullptr)<1700000000)error="Clock unavailable for TLS verification";
    }

    {
        Guard g(sdSemaphore);
        if(!g.held || !isConnectSDcard) error="SD unavailable";
        else if(SD.exists(path)) error="File already exists; choose another name";
        else {
            file=SD.open(temp,FILE_WRITE);
            if(!file) error="Cannot create temporary audio file";
        }
    }

    esp_http_client_config_t config{};
    config.url=job->url.c_str();config.timeout_ms=15000;config.buffer_size=4096;
    config.crt_bundle_attach=esp_crt_bundle_attach;config.disable_auto_redirect=true;
    if(error.isEmpty()) client=esp_http_client_init(&config);
    if(!client && error.isEmpty()) error="Cannot create HTTP client";

    if(client && error.isEmpty()) {
        if(esp_http_client_open(client,0)!=ESP_OK) error="Connection failed (check URL, WiFi and TLS certificate)";
        else {
            const int64_t length=esp_http_client_fetch_headers(client);
            const int status=esp_http_client_get_status_code(client);
            if(status!=200) error="Audio URL must return the file directly with HTTP 200; redirects are not followed";
            else if(length>static_cast<int64_t>(std::numeric_limits<size_t>::max())) error="Audio file is too large for this device";
            else if(length>0) total=static_cast<size_t>(length);
        }
    }

    if(error.isEmpty() && total) {
        Guard g(sdSemaphore);
        if(!g.held || !portal::uploadFits(total,SD.totalBytes(),SD.usedBytes())) error="Not enough SD space";
    }

    if(error.isEmpty()) {
        buffer=static_cast<uint8_t*>(heap_caps_malloc(bufferSize,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(!buffer) {
            bufferSize=4096;
            buffer=static_cast<uint8_t*>(heap_caps_malloc(bufferSize,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
        }
        if(!buffer) error="Not enough memory for download buffer";
    }

    while(error.isEmpty()) {
        const int n=esp_http_client_read(client,reinterpret_cast<char*>(buffer),bufferSize);
        if(n<0){error="Audio download interrupted";break;}
        if(n==0)break;
        if(total && done+static_cast<size_t>(n)>total){error="Server sent more data than Content-Length";break;}
        if(!total) {
            Guard g(sdSemaphore);
            if(!g.held || !portal::uploadFits(static_cast<uint64_t>(n),SD.totalBytes(),SD.usedBytes())) {error="Not enough SD space";break;}
        }
        if(!writeImportChunk(file,buffer,static_cast<size_t>(n),error))break;
        done+=static_cast<size_t>(n);
        audioImportStatus("downloading","",done,total);
    }

    if(error.isEmpty() && total && done!=total) error="Audio download ended before Content-Length";
    if(client){esp_http_client_close(client);esp_http_client_cleanup(client);}
    if(buffer)heap_caps_free(buffer);

    bool committed=false;
    {
        Guard g(sdSemaphore);
        if(g.held) {
            if(file){file.flush();file.close();}
            if(error.isEmpty() && !SD.exists(path) && SD.rename(temp,path)) committed=true;
            else {SD.remove(temp);if(error.isEmpty())error="Cannot save downloaded audio";}
        } else if(error.isEmpty()) error="SD busy while finishing download";
    }

    if(committed) audioImportStatus("done","",done,total?total:done);
    else audioImportStatus("error",error.length()?error:"Audio import failed",done,total);
    transfer=false;
    vTaskDelete(nullptr);
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
    Preferences p;p.begin("portal",true);String record=p.getString("account","");p.end();JsonDocument saved(memory::jsonAllocator());
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
        session=randomHex();sessionAt=millis();JsonDocument d(memory::jsonAllocator());d["token"]=session;String out;serializeJson(d,out);r->send(200,"application/json",out);
    });
    server.on("/api/logout",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(!authorized(r,true))return;{Guard g(portalMutex);session="";}reply(r,200,"Signed out",true);});
    server.on("/api/account",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(!authorized(r,true))return;
        String old=param(r,"current"),pass=param(r,"password"),user=param(r,"username");Guard g(portalMutex);
        if(!g.held||hashPassword(old,salt)!=passwordHash){reply(r,403,"Incorrect current password");return;}
        if(!r->hasParam("username",true))user=account;user.trim();
        if(!portal::accountName(user.c_str())){reply(r,400,"Username must be 1–31 bytes without control characters");return;}
        if(pass.length()&&(pass.length()<8||pass.length()>128)){reply(r,400,"New password must be 8–128 characters");return;}
        String s=pass.length()?randomHex():salt,h=pass.length()?hashPassword(pass,s):passwordHash;
        if(!saveAccount(user,s,h)){reply(r,500,"Cannot save account");return;}
        account=user;salt=s;passwordHash=h;session="";reply(r,200,"Account updated; sign in with your new details",true);
    });
    server.on("/api/status",AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){if(!authorized(r))return;String out;{Guard g(portalMutex);out=snapshot;}auto* response=r->beginResponse(200,"application/json",out);response->addHeader("Cache-Control","no-store");r->send(response);});
    server.on("/api/settings",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(!authorized(r,true))return;enqueue(r,Command::Settings,param(r,"values"));});
    server.on("/api/wifi",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(!authorized(r,true))return;
        String ssid=param(r,"ssid"),pass=param(r,"password");if(ssid.isEmpty()||ssid.length()>32||pass.length()>63||(pass.length()&&pass.length()<8)){reply(r,400,"SSID 1–32 bytes; password empty or 8–63 bytes");return;}
        JsonDocument d(memory::jsonAllocator());d["ssid"]=ssid;d["password"]=pass;String out;serializeJson(d,out);enqueue(r,Command::Wifi,out);
    });
    server.on("/api/ai",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(authorized(r,true))enqueue(r,Command::AI,param(r,"values"));});
    server.on("/api/espnow",AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){if(!authorized(r))return;auto* response=r->beginResponse(200,"application/json",espnow::status());response->addHeader("Cache-Control","no-store");r->send(response);});
    server.on("/api/espnow",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(!authorized(r,true))return;if(webFirmwareUpdating()){reply(r,409,"Firmware update in progress");return;}if(!espnow::submitConfig(param(r,"values"))){reply(r,409,"Configuration too large or queue full");return;}reply(r,202,"ESP-NOW settings queued",true);});
    server.on("/api/espnow/send",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(!authorized(r,true))return;if(webFirmwareUpdating()){reply(r,409,"Firmware update in progress");return;}uint8_t mac[6],payload[espnow::MaxPayload];String hex=param(r,"hex");if(!espnow::mac(param(r,"mac").c_str(),mac)||hex.length()%2||hex.length()>espnow::MaxPayload*2){reply(r,400,"Invalid MAC or payload (max 200 bytes)");return;}for(size_t i=0;i<hex.length();i+=2){int a=espnow::hex(hex[i]),b=espnow::hex(hex[i+1]);if(a<0||b<0){reply(r,400,"Payload must be hexadecimal");return;}payload[i/2]=(a<<4)|b;}if(!espnow::sendPacket(mac,payload,hex.length()/2)){reply(r,409,"Send queue full");return;}reply(r,202,"Packet queued; check TX/RX status",true);});
    server.on("/api/history",AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){
        if(!authorized(r))return;String before=param(r,"before",false);
        for(size_t i=0;i<before.length();++i)if(before[i]<'0'||before[i]>'9'){reply(r,400,"Invalid history cursor");return;}
        if(before.length()>5){reply(r,400,"Invalid history cursor");return;}
        auto* response=r->beginResponse(200,"application/json",chatHistory.page(before.toInt(),param(r,"summary",false)!="1"));response->addHeader("Cache-Control","no-store");r->send(response);
    });
    server.on("/api/history/reset",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){
        if(!authorized(r,true))return;if(updating.load()||app::runtime.aiPetProcessing){reply(r,409,"Device busy; try again after the current request");return;}
        if(param(r,"confirm")!="reset"){reply(r,400,"Reset confirmation required");return;}
        if(!chatHistory.requestReset()){reply(r,409,"History reset already in progress");return;}
        reply(r,202,"History reset requested",true);
    });
    server.on("/api/foods",AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){
        if(!authorized(r))return;auto* response=r->beginResponse(200,"application/json",foodStore.snapshot());response->addHeader("Cache-Control","no-store");r->send(response);
    });
    server.on("/api/foods",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){
        if(!authorized(r,true))return;if(updating.load()){reply(r,409,"Firmware update in progress");return;}
        String body=param(r,"values");JsonDocument d(memory::jsonAllocator());
        if(body.length()>512||deserializeJson(d,body)||!d.is<JsonObject>()){reply(r,400,"Invalid food request");return;}
        String error=foodStore.change(d);if(error.length()){reply(r,400,error);return;}
        r->send(200,"application/json",foodStore.snapshot());
    });
    server.on("/api/files",AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){if(!authorized(r))return;String dir=param(r,"dir",false);
        if(!portal::directory(dir.c_str())){reply(r,400,"Invalid directory");return;}
        JsonDocument d(memory::jsonAllocator());auto files=d["files"].to<JsonArray>();{Guard g(sdSemaphore);if(!g.held||!isConnectSDcard){reply(r,503,"SD unavailable");return;}
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
        auto* response=r->beginResponse("application/octet-stream",f->size(),[f](uint8_t* buffer,size_t maxLen,size_t index)->size_t {Guard guard(sdSemaphore);if(!guard.held)return RESPONSE_TRY_AGAIN;f->seek(index);return f->read(buffer,std::min(maxLen,size_t(4096)));});
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
    server.on("/api/audio/import",AsyncWebRequestMethod::HTTP_POST,[](AsyncWebServerRequest* r){if(!authorized(r,true))return;
        String url=param(r,"url"),name=param(r,"name");name.trim();
        if(!directHttpUrl(url)){reply(r,400,"Use a direct HTTP(S) audio file URL without credentials");return;}
        if(blockedYouTubeUrl(url)){reply(r,400,"YouTube page/stream URLs are not supported. Use a direct audio file URL you are allowed to download, or upload your audio file.");return;}
        if(!portal::audioFilename(name.c_str())||!portal::filename(name.c_str())){reply(r,400,"Audio name must end in .mp3, .wav, .aac, .m4a or .flac");return;}
        if(rebootAt.load()||updating||WiFi.status()!=WL_CONNECTED||app::runtime.isRecordingMode||app::runtime.aiPetProcessing||networkSettingsBusy()){reply(r,409,"Connect internet WiFi and stop recording/AI processing first");return;}
        bool free=false;if(!transfer.compare_exchange_strong(free,true)){reply(r,409,"Another transfer is active");return;}
        auto* job=new(std::nothrow) AudioImportJob{url,name};
        audioImportStatus("queued","",0,0);
        if(!job||xTaskCreatePinnedToCore(audioImportTask,"audioImport",8192,job,2,nullptr,0)!=pdPASS){delete job;transfer=false;audioImportStatus("error","Not enough memory to start audio import");reply(r,503,"Not enough memory to start audio import");return;}
        reply(r,202,"Audio download started",true);
    });
    server.on("/api/ota/file",AsyncWebRequestMethod::HTTP_POST,uploadComplete,[](AsyncWebServerRequest* r,String name,size_t i,uint8_t* data,size_t len,bool last){uploadChunk(r,name,i,data,len,last,true);});
    server.on("/api/ota",AsyncWebRequestMethod::HTTP_GET,[](AsyncWebServerRequest* r){if(!authorized(r))return;JsonDocument d(memory::jsonAllocator());{Guard g(portalMutex);d["state"]=otaState;d["error"]=otaError;d["done"]=otaDone;d["total"]=otaTotal;}String out;serializeJson(d,out);r->send(200,"application/json",out);});
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
    if(chatHistory.resetting()){
        if(aiConversation.hasLiveWorker()){
            if(aiConversation.isLiveSessionActive())appCoordinator.stopAiPetListening();
        }else chatHistory.reset();
    }
    static bool wasUpdating=false;bool active=webFirmwareUpdating();
    if(active!=wasUpdating){setVoiceAssistantEnabled(active?false:userSettings.values.voice);wasUpdating=active;}
    uint32_t restart=rebootAt.load();if(restart && static_cast<int32_t>(millis()-restart)>=0)ESP.restart();
    Command c{};
    if(!updating && xQueueReceive(commands,&c,0)==pdTRUE) {
        JsonDocument d(memory::jsonAllocator());String result="Settings saved";
        if(deserializeJson(d,c.body))result="Invalid settings JSON";
        else if(c.kind==Command::Settings && !d["customPet"].isNull()) {
            auto value=userSettings.customPet;JsonVariant json=d["customPet"];bool valid=d.size()==1&&json.is<JsonObject>();
            const char* colorKeys[]={"background","face","accent","cheeks"};
            uint32_t* colors[]={&value.background,&value.face,&value.accent,&value.cheeks};
            for(int i=0;i<4;++i)if(!json[colorKeys[i]].isNull()){if(!json[colorKeys[i]].is<uint32_t>()||json[colorKeys[i]].as<uint32_t>()>0xffffff)valid=false;else *colors[i]=json[colorKeys[i]].as<uint32_t>();}
            const char* keys[]={"eyeWidth","eyeHeight","blush","radius","base"};
            uint8_t* fields[]={&value.eyeWidth,&value.eyeHeight,&value.blush,&value.radius,&value.base};
            for(int i=0;i<5;++i)if(!json[keys[i]].isNull()){int n=json[keys[i]]|-1;if(!json[keys[i]].is<int>()||n<0||n>255)valid=false;else *fields[i]=n;}
            if(!json["voice"].isNull()){const char* v=json["voice"].as<const char*>();if(!v||!strlen(v)||strlen(v)>=sizeof(value.voice))valid=false;else strlcpy(value.voice,v,sizeof(value.voice));}
            if(!json["prompt"].isNull()){const char* v=json["prompt"].as<const char*>();if(!v||!strlen(v)||strlen(v)>=sizeof(value.prompt))valid=false;else strlcpy(value.prompt,v,sizeof(value.prompt));}
            if(!valid||!pet::valid(value))result="Invalid custom pet values (prompt max 511 UTF-8 bytes)";
            else if(!userSettings.saveCustomPet(value))result="Custom pet save failed";
        }
        else if(c.kind==Command::Settings) {
            auto v=userSettings.values;
            // Reject out-of-range values before narrowing to uint8_t.
            bool valid=true;const char* keys[]={"volume","volumeStep","voice","autoNext","shuffle","wifi","admin","petPersonality"};
            uint8_t* fields[]={&v.volume,&v.volumeStep,&v.voice,&v.autoNext,&v.shuffle,&v.wifi,&v.admin,&v.petPersonality};
            for(int i=0;i<8;i++)if(!d[keys[i]].isNull()){int n=d[keys[i]].as<int>();if(!d[keys[i]].is<int>()||n<0||n>100)valid=false;else *fields[i]=n;}
            if(d["colors"].is<JsonArray>()){auto colors=d["colors"].as<JsonArray>();if(colors.size()!=preferences::ColorCount)valid=false;else for(int i=0;i<preferences::ColorCount;i++){int h=colors[i][0]|-1,s=colors[i][1]|-1,b=colors[i][2]|-1;if(h<0||h>=360||s<0||s>100||b<0||b>100)valid=false;else v.colors[i]={static_cast<uint16_t>(h),static_cast<uint8_t>(s),static_cast<uint8_t>(b)};}}
            if(!valid||!preferences::valid(v))result="Invalid settings values";
            else {auto previous=userSettings.values;userSettings.values=v;if(!userSettings.save()){userSettings.values=previous;result="Settings save failed";}else {setOutputVolume(v.volume);setVoiceAssistantEnabled(v.voice);DISM.applyTheme();if(v.wifi!=previous.wifi||v.admin!=previous.admin)requestNetworkSettings(v.wifi,v.admin);}}
        } else if(c.kind==Command::Wifi) {
            Preferences p;if(!p.begin("WiFiConfig",false))result="Cannot save WiFi";
            else{String ssid=d["ssid"]|"",pass=d["password"]|"";bool ok=p.putString("ssid",ssid)==ssid.length() && p.putString("password",pass)==pass.length();p.end();if(ok){userSettings.values.wifi=1;userSettings.values.admin=1;if(!userSettings.save())result="WiFi saved, but startup settings save failed";requestWiFiReconnect();}else result="Cannot save WiFi";}
        } else {
            if(app::runtime.aiPetProcessing || aiConversation.isLiveSessionActive()) result="Exit AI Pet before changing AI settings";
            else if(!aiConversation.saveConfig(d)) result="Cannot save AI configuration";
        }
        {Guard g(portalMutex);if(g.held){settingsResult=result;++settingsRevision;}}
    }
    static uint32_t last=0;if(millis()-last<1000)return;last=millis();
    JsonDocument d(memory::jsonAllocator());d["device"]=String(static_cast<uint32_t>(ESP.getEfuseMac()),HEX);d["uptime"]=millis()/1000;d["heap"]=ESP.getFreeHeap();d["psram"]=ESP.getFreePsram();d["heapMin"]=ESP.getMinFreeHeap();d["heapLargest"]=ESP.getMaxAllocHeap();d["tlsPsram"]=memory::tlsUsesPsram();d["firmware"]=__DATE__ " " __TIME__;d["slotSize"]=slotSize();d["width"]=tft.width();d["height"]=tft.height();
    d["connected"]=WiFi.status()==WL_CONNECTED;d["ssid"]=WiFi.SSID();d["ip"]=WiFi.localIP().toString();d["apIP"]=WiFi.softAPIP().toString();d["rssi"]=WiFi.RSSI();d["networkBusy"]=networkSettingsBusy();
    // FAT free-space queries can be slow. Never block the music reader for a
    // dashboard refresh; serve cached capacity while audio/recording is active.
    static uint64_t storageTotal=0,storageUsed=0;static uint32_t storageChecked=0;
    if(!isConnectSDcard){storageTotal=storageUsed=0;storageChecked=0;}
    else if(!isPlayingAudio&&!app::runtime.isRecordingMode&&(!storageChecked||millis()-storageChecked>30000)){
        Guard g(sdSemaphore);if(g.held){storageTotal=SD.totalBytes();storageUsed=SD.usedBytes();storageChecked=millis();}
    }
    d["sd"]=isConnectSDcard;if(storageChecked){d["storageTotal"]=storageTotal;d["storageUsed"]=storageUsed;}d["storageCached"]=true;d["storageKnown"]=storageChecked!=0;
    d["musicSdWaits"]=getMusicSdWaits();d["musicMaxServiceGapMs"]=getMusicMaxServiceGapMs();
    d["playing"]=isPlayingAudio;d["title"]=getCurrentSongTitle();d["current"]=currentAudioTime;d["duration"]=totalAudioDuration;d["recording"]=app::runtime.isRecording;d["busy"]=transfer.load();
    {Guard g(portalMutex);if(g.held){d["audioImportState"]=audioImportState;d["audioImportError"]=audioImportError;d["audioImportDone"]=audioImportDone;d["audioImportTotal"]=audioImportTotal;}}
    auto custom=d["customPet"].to<JsonObject>();const auto& cp=userSettings.customPet;
    custom["background"]=cp.background;custom["face"]=cp.face;custom["accent"]=cp.accent;custom["cheeks"]=cp.cheeks;
    custom["eyeWidth"]=cp.eyeWidth;custom["eyeHeight"]=cp.eyeHeight;custom["blush"]=cp.blush;custom["radius"]=cp.radius;custom["base"]=cp.base;custom["voice"]=cp.voice;custom["prompt"]=cp.prompt;
    auto s=d["settings"].to<JsonObject>();const auto& v=userSettings.values;s["petPersonality"]=v.petPersonality;s["volume"]=v.volume;s["volumeStep"]=v.volumeStep;s["voice"]=v.voice;s["autoNext"]=v.autoNext;s["shuffle"]=v.shuffle;s["wifi"]=v.wifi;s["admin"]=v.admin;
    auto colors=s["colors"].to<JsonArray>();for(auto color:v.colors){auto row=colors.add<JsonArray>();row.add(color.hue);row.add(color.saturation);row.add(color.value);}
    JsonDocument ai(memory::jsonAllocator());deserializeJson(ai,aiConversation.getConfigJson(false));d["ai"]=ai;
    String out;{Guard g(portalMutex);d["username"]=account;d["settingsResult"]=settingsResult;d["settingsRevision"]=settingsRevision;serializeJson(d,out);if(g.held)snapshot=out;}
}

bool webFirmwareUpdating() { return updating.load() || rebootAt.load()!=0; }
