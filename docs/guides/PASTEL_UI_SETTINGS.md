# Pastel UI and saved settings

Home uses six labeled tiles. Turn the encoder to select a tile and press to open.
Back returns one level. Hold Back for one second to open volume; press, go back,
or wait two seconds to return to the original screen. Media viewing and the color
editor use Back for their own return/cancel actions.

## Settings

| Category | Controls |
| --- | --- |
| Display | Background, cards/panels, text, accent, secondary text, selection; reset palette |
| Network | Wi-Fi and Admin access point toggle switches |
| Music & Sound | Auto-next and shuffle toggle switches, volume step (2–5), volume |
| Voice Assistant | On-device voice recognition toggle switch |

Turn through the Display list to reach all six colors and Reset palette. Select
a color to open its wheel. Turn to change hue, press for saturation, press for
brightness, then press again to save. The wheel and interface preview the result.
Back cancels the entire color edit. Brightness here adjusts the color value;
the board has no configured backlight control pin.

Wi-Fi applies in the network task so navigation stays responsive while connecting.
The screen reports connection status separately from the requested on/off setting.
Wi-Fi and Admin mode can run together. Wi-Fi credentials continue using the existing
Admin setup and its separate NVS storage. After enabling Wi-Fi from the online
player, press Play once the connection has completed.

## Music and volume

- Local music shows elapsed / total time, for example `01:23 / 04:05`.
- Auto-next advances when a local track reaches EOF, including WAV and MP3. It
  wraps to the beginning at the end of the list. It is enabled by default.
- Shuffle changes next-track selection and avoids the current song when another
  song exists. Enable Auto-next as well for continuous shuffled playback.
- A manual song change invalidates any pending completion request from the old
  song. Online radio and AI response audio do not advance the local playlist.
- Volume displays 0–100; the audio task maps it to the library's 0–21 scale.
  Default movement is 5 points per detent; Music & Sound offers 2, 3, 4 or 5.

## Voice and persistence

Voice Assistant is off by default. While off, the continuous classifier does not
run and capture stops feeding its inference buffers. The microphone remains
available for Recorder and AI Pet. Recording temporarily gates recognition even
when the switch is enabled; the classifier resets before recognition resumes.
Boot opens Home rather than immediately entering an AI Pet recording cycle.

The six colors, Wi-Fi/Admin preferences, auto-next, shuffle, voice switch, volume
and volume step are saved together in the `tofan-ui` NVS namespace. A version and
checksum guard stored data; missing or invalid data falls back to defaults.
Switches and confirmed color changes save immediately. Volume saves after 500 ms
without adjustment and when leaving the volume screen. Wait for that save before
cutting power. Failed writes show a retry message in Settings.

## Validation

From the project root, with GCC available:

```powershell
g++ -std=c++11 -Wall -Wextra -pedantic -I test/support test/test_settings_native.cpp src/core/UserSettings.cpp -o .pio/settings-tests.exe
& .pio/settings-tests.exe
python test/test_ui_flow.py
g++ -std=c++11 .pio/ui-flow.cpp -o .pio/ui-flow-tests.exe
& .pio/ui-flow-tests.exe
```

The settings suite covers 12 groups: every audio volume level and detent bounds,
playlist boundaries, non-repeating next-song selection, time labels, HSV/RGB565,
actual settings save/load through an in-memory NVS adapter, corruption, validation,
write failure recovery, EOF invalidation and recognition gating. The controller
harness executes production navigation and renderer code with hardware adapters.
Existing media and recording regression suites are retained.

`test/render_ui_preview.py` and `test/render_ui_preview.ps1` generate previews by
executing production drawing methods. See `docs/build/ui-preview/contact-sheet.png`.
These previews use desktop font approximations, not photos of the board. On-device
checks still needed: encoder feel, actual fonts, audible volume transitions,
MP3/WAV EOF playback, Wi-Fi/AP coexistence and persistence across a power cycle.

Final firmware build output: `docs/build/pastel-settings-build.log`.
