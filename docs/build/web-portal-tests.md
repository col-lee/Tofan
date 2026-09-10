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
| Content / mobile / account | Long song and unbroken network names remain inside resized cards without overflow; menu reaches all seven pages and fits 320×568; username form sends the new name with an optional blank new password and returns to sign-in |
| MJPEG parser | Consecutive frames, EOI bytes inside metadata, truncated frames, size limits, unsupported progressive JPEG, bounded prefix scanning and real browser-converted MJPEG checked |

Firmware build details are in `web-portal-build.log`. Screenshots are in `web-preview/`.

Crop editor regression: screen/320×240/custom presets, pointer drag, keyboard adjustment and invalidation passed in Chrome. Two real JPEG frames were cropped to 64×64 and decoded again: both retained the selected green right-hand region and the original frame count, with no implicit upload. Existing GIF animation/frame-count and image conversion checks passed. Native JavaScript unit tests also cover MJPEG metadata marker escapes and truncated/malformed frames. Screenshot: `web-preview/crop-mjpeg.png`.

Font Awesome: all configured icon names and CSS font URLs resolve to local assets. Chrome loaded the solid WOFF2 font, rendered icons in all seven navigation links, and passed the existing desktop/mobile, modal, selector, file picker and resize regression checks with no external requests. Updated screenshots show the actual rendered icons.

Pending hardware validation: SD write failures/disconnection and file commit, first PNG display, real WiFi reconnect and NVS restore, HTTPS OTA with the server's certificate and NTP, wrong/truncated firmware, interrupted update, reboot and power-loss behavior. Host and mock browser checks do not establish hardware behavior.
