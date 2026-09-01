# Project Structure Improvement Plan

## Current issues

จากโครงสร้างปัจจุบันมีปัญหา 5 ประเด็นหลัก:

1. ชื่อโฟลเดอร์ `extendstion` เป็น typo ของ `extension` / `extensions`
2. ไฟล์ `src/main.cpp` ทำหน้าที่รวมการควบคุมทุกอย่างมากเกินไป เช่น UI, networking, audio, hardware, AI, queue, event loop
3. `src/extendstion/GlobalVar.hpp` มีการ include หลายโมดูลและเก็บ state แบบ global มากเกินไป ทำให้เกิด coupling ระหว่างโมดูล และ circular dependency ได้ง่าย
4. มีการใช้ global singleton แบบ `nm`, `file_card`, `hwManager`, `aiConversation` ในหลายไฟล์ ทำให้ความสัมพันธ์ซับซ้อนและยากทดสอบ
5. Include guards และ header structure ยังไม่สม่ำเสมอ เช่น `DisplayManager.hpp` และ `Network.hpp` มีการ define guard แบบผิดรูปแบบ ทำให้โครงสร้างยากต่อการรักษา

---

## Recommended target structure

```text
src/
  app/
    main.cpp
    AppCoordinator.hpp
    AppCoordinator.cpp

  core/
    config.hpp
    enums.hpp
    global_defines.hpp
    GlobalState.hpp
    logger.hpp

  platform/
    hardware/
      HardwareManager.hpp
      HardwareManager.cpp
      IOManager.hpp
      IOManager.cpp
      RGBLed.hpp
      RGBLed.cpp
    storage/
      FileManager.hpp
      FileManager.cpp
    network/
      Network.hpp
      Network.cpp
      AIConversation.hpp
      AIConversation.cpp

  services/
    audio/
      SoundManager.hpp
      SoundManager.cpp
    ui/
      DisplayManager.hpp
      DisplayManager.cpp
      event.hpp

  utils/
    StringUtils.hpp
    TimeUtils.hpp
    ErrorCodes.hpp
```

---

## Design rules to follow

### 1. `main.cpp` should be thin
`main.cpp` ควรทำหน้าที่แค่:
- initialize modules
- create task / scheduler
- start services
- loop event dispatcher

ไม่ควรมี logic การจัดการ UI, audio, network, AI เรียงรวมกันในไฟล์เดียวแบบปัจจุบัน

### 2. Use one source of truth for shared state
ควรย้าย global variables ไปไว้ใน `GlobalState.hpp` หรือ `AppState.hpp` แบบนี้:

```cpp
struct AppState {
    bool isRecordingMode;
    bool isRecording;
    bool aiPetListening;
    volatile bool aiPetProcessing;
};

extern AppState appState;
```

- หลีกเลี่ยงการ `#include` ทุกไฟล์ใน `GlobalVar.hpp`
- `GlobalVar.hpp` ควรแค่ประกาศ type / constant / extern object
- ห้ามให้ header เรียก dependency หลายชั้นเข้าด้วยกัน

### 3. Split responsibilities by domain
- `HardwareManager`: manage device status and health
- `IOManager`: GPIO, button, LED
- `SoundManager`: recorder / player / volume controls
- `DisplayManager`: rendering UI and screen states
- `NetworkManager`: wifi + web server + admin mode
- `AIConversation`: API request / config / audio upload
- `FileManager`: file operations on SD card

### 4. Reduce circular include
ปัจจุบันหลายไฟล์ include กันแบบไขว้:
- `DisplayManager.cpp` includes `FileManager.hpp` and `SoundManager.hpp`
- `HardwareManager.cpp` includes `DisplayManager.hpp` and `Network.hpp`
- `Network.cpp` includes `AIConversation.hpp` and `FileManager.hpp`

แนวทางที่ควรทำคือ:
- ใช้ forward declaration เมื่อจำเป็น
- headers ควร include เฉพาะสิ่งที่ต้องใช้จริง
- ย้าย shared enums/structs ไปไว้ใน `core/enums.hpp` หรือ `core/global_defines.hpp`

---

## Suggested migration plan

### Step 1: Rename folder
Rename:

```text
src/extendstion -> src/extensions
```

แม้จะไม่ใช่การ refactor หลัก แต่ช่วยลดความสับสนและแก้ typo โดยตรง

### Step 2: Extract app orchestration
สร้างไฟล์ใหม่:

```text
src/app/AppCoordinator.hpp
src/app/AppCoordinator.cpp
```

หน้าที่:
- initialize all modules
- manage screen flow
- handle input actions
- orchestrate background tasks

### Step 3: Move shared definitions out of `GlobalVar.hpp`
ให้ `GlobalVar.hpp` เหลือแต่:
- extern object declarations
- enum definitions
- shared constants
- common pin definitions

### Step 4: Split large UI logic
ถ้าต้องการให้โปรเจ็กต์ดูดีจริง ๆ ควรแยก `DisplayManager.cpp` ออกเป็น:
- `HomeScreen.cpp`
- `MusicScreen.cpp`
- `SettingsScreen.cpp`
- `PetScreen.cpp`
- `DebugScreen.cpp`

แต่ถ้าอยากเริ่มแบบไม่ยุ่งมาก ให้เริ่มจากการแยก function group ภายใน file ก่อน

### Step 5: Standardize naming
อนุรักษ์แนวทางต่อไป:
- class names: PascalCase
- methods: camelCase
- files: PascalCase or snake_case ที่สอดคล้องกัน
- avoid abbreviations unless common and consistent

---

## Recommended immediate cleanup without risky rewrite

ถ้าจะปรับแบบไม่กระทบ build มากเกินไป ให้ทำรายการต่อไปนี้ก่อน:

1. Rename `extendstion` to `extensions`
2. Move shared constants out of `main.cpp`
3. Create `AppCoordinator` and let `main.cpp` only bootstrap
4. Remove all broad `#include` from `GlobalVar.hpp`
5. Fix inconsistent include guards in `Network.hpp`
6. Keep singletons but centralize them in `GlobalState.hpp`

---

## Bottom line

โครงสร้างปัจจุบันยังใช้งานได้ แต่ยัง “ทำงานได้” มากกว่าดีตามหลัก architecture เพราะมันมีปัญหาเรื่อง
- coupling สูง
- responsibility overlap
- global state มาก
- naming/typo
- single-file orchestration

ถ้าจะปรับให้ดีจริง ๆ ควรเริ่มจาก 3 เรื่องนี้ก่อน:
1. แยก `main.cpp` ออกเป็น app coordinator
2. แก้ `extendstion` typo และโครงสร้างโฟลเดอร์
3. ลด dependency จาก `GlobalVar.hpp`

---

## Suggested next action

ผมแนะนำให้เริ่มด้วยการ refactor แบบ incremental:

```text
main.cpp -> AppCoordinator
GlobalVar.hpp -> GlobalState.hpp + config.hpp
extendstion -> extensions
```

การย้ายแบบนี้ไม่จำเป็นต้องทำพร้อมกันทั้งหมดใน 1 commit แต่ควรทำทีละช่วง เพื่อให้ build เสถียรและตรวจสอบง่าย
