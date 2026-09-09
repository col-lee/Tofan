# Web portal validation

All checks below passed on the development PC. Browser data is mocked; no device was flashed.

| Check | Result |
| --- | --- |
| Native web policy | Safe filenames/directories, firmware header/chip/size, chunk ordering/duplicates/overflow/finalization passed |
| Native media regression | 12 navigation, queue ownership and JPEG parser cases passed |
| Native recording regression | 6 unique recording filename scenarios passed |
| Native settings regression | 12 settings/persistence/volume/shuffle/theme/EOF/voice groups passed |
| Actual input controller host harness | Navigation, color save/cancel, toggles, volume restore, first image entry and repeated recording passed |
| Web model unit tests | 5 layout/crop/size-limit/snap groups passed |
| Chrome browser integration with mock API | Login, add/remove widgets, persistence after reload, keyboard resizing, settings, WiFi form, JPEG/PNG processing, animated GIF frame count/dimensions, invalid image dimensions, explicit upload, OTA page and mobile overflow checks passed |
| Browser network inspection | No external requests; image processing did not upload a file until the explicit upload action |
| Modal and control regression | Rename confirmation, delete confirmation/cancel, OTA cancel without POST, custom selector mouse/keyboard input, themed file picker, keyboard/pointer snap, equal card heights, status meters and real-sample RAM graph passed; no native JavaScript dialogs fired |

Firmware build details are in `web-portal-build.log`. Screenshots are in `web-preview/`.

Pending hardware validation: SD write failures/disconnection and file commit, first PNG display, real WiFi reconnect and NVS restore, HTTPS OTA with the server's certificate and NTP, wrong/truncated firmware, interrupted update, reboot and power-loss behavior. Host and mock browser checks do not establish hardware behavior.
