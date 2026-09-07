# Music and Display changes

Music now always opens the SD player, with an **Online** control available even
when SD tracks exist. Turn the encoder to select a control, then press it.
Online radio uses the existing three configured stations; previous/next changes
station, play starts its URL, and **SD card** returns to the local player.
If Wi-Fi is disconnected, playback offers to connect using existing settings.
This is direct internet radio streaming, not a Spotify or YouTube integration.

Both players share a light minimal layout, green selection accents, geometric
transport icons, source switch, and explicit playback/network status. Redraws are
limited to five per second. Tracks opens the local file list.

## Bugs addressed

- First Display entry configured the encoder from an empty list before loading
  files. Lists now load first and all encoder limits use `count - 1`, safely
  handling zero files. Selection is validated before indexing paths.
- JPEG rendering used the UI sprite after it had been deleted. JPEG now renders
  directly to the panel with SD/display synchronization.
- Returning from media guessed completion using a 50 ms delay. The UI now waits
  asynchronously for an atomic completion flag from the display task before
  rebuilding the list, preventing a late clear from erasing it.
- FreeRTOS byte-copy queues contained Arduino Strings with heap pointers.
  Commands now own their path bytes; compile-time assertions verify they are
  trivially copyable. Paths over 1023 bytes are rejected. Task stacks have extra
  room for the owned payloads.
- Initial Play sent an empty path without a paused track. It now selects the
  first SD track or an explicit station URL. Playback state uses the connection
  result and stops reporting playback after the decoder stops.
- Previous-track selection tolerates a library that has shrunk. Corrupt JPEG
  marker scans stop at EOF instead of repeatedly reading a missing marker.

## Automated validation

Run the independent host tests from the project root (GCC required):

```powershell
g++ -std=c++11 -Wall -Wextra -pedantic test/test_media_native.cpp -o .pio/media-tests.exe
& .pio/media-tests.exe
pio run -e esp32-s3-devkitc-1-n16r8v
```

Twelve host tests cover empty, single and multi-item bounds; station wrapping;
display/audio queue payload lifetime after byte copying and sender destruction;
path capacity rejection; UTF-8 path preservation; JPEG dimensions with metadata;
and truncated/invalid JPEG markers. These exercise production
headers. The file is excluded on Arduino so it does not conflict with the
existing Unity test runner. Build output: `docs/build/music-display-build.log`.

## Hardware checks still required

1. Cold boot with at least eight pictures; enter Display and immediately scroll
   through all items, including wraparound. Repeat with zero and one picture.
2. Open a JPEG on the first click, return, and repeat with GIF and uppercase file
   extensions. Repeat rapid open/back while local audio is playing.
3. Open missing/corrupt media, then return and choose a valid file.
4. Enter Music with and without SD tracks. Test initial play, pause/resume,
   previous/next, Tracks, volume overlay, and both source-switch controls.
5. Play each online station with no SD card. Disconnect Wi-Fi, reconnect/retry,
   and verify audible playback and metadata. External stream availability and
   actual display/encoder behavior cannot be confirmed by host unit tests.
