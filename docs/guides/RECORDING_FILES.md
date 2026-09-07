# Saved recordings

In Voice Recording, press **REC** to start and **STOP** to save. Each new
session creates its own WAV file under `/main/Musics`, for example:

```text
voice_record_000001.wav
voice_record_000002.wav
voice_record_000003.wav
```

The screen shows the current/last recording filename. Existing recordings,
including the old `voice_record.wav`, remain intact. File existence is checked
under the SD mutex before creation, including after reboot. Numbering may skip
occupied names or failed creation attempts; deleted numbers can be reused after
reboot. AI Pet continues using its separate temporary input file.

Host regression tests (from the project root with GCC):

```powershell
g++ -std=c++11 -Wall -Wextra -pedantic test/test_recording_native.cpp -o .pio/recording-tests.exe
& .pio/recording-tests.exe
```

Six scenarios cover consecutive recordings, reboot, occupied names, insufficient
path buffer space, and number exhaustion with/without an existing final file.
Firmware build log: `docs/build/recording-build.log`.

On hardware, record two clips, restart the device, record a third, and verify
that all clips and any pre-existing recording still play independently in Music.
