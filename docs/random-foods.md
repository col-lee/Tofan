# Random foods and Gemini voices

Open **Random foods** on Home page 2. Turn the encoder to choose a category (or all categories), press to pick, and use Back to return home. With multiple choices, the previous result is excluded from the next draw.

Food results use larger, centered text on the device (normally 2×, fitting long names over multiple lines) and larger responsive text on the web. A draw cycles through the selected category for 16 frames with intervals growing from 40 to 250 ms, then reveals the device's final choice after 2.32 seconds. Intermediate names never change the stored random result. Device animation uses elapsed time without blocking input; changing category cancels the preview. The web disables duplicate draws and edits until the result arrives, stops detached views, and skips animation for reduced-motion preferences or a single item.

The web portal's **Random foods** page adds, edits, moves, and deletes food/drink items and categories. Catalog changes are saved in device NVS, without an SD card or external service. Limits: 12 categories, 60 items, 6,000 serialized UTF-8 bytes total. Names allow 60 bytes per category and 96 bytes per item; the web editor conservatively limits these to 20/32 characters. A category containing items must be emptied before deletion. Failed writes restore the previous in-memory catalog. Random draws do not write flash. The last draw/category is session state, not persisted settings.

AI settings now use a styled voice selector with all 30 voices in Google's [voice options](https://ai.google.dev/gemini-api/docs/speech-generation#voice-options), checked 2026-09-12. Google's [Live capabilities](https://ai.google.dev/gemini-api/docs/live-api/capabilities) link native audio voices to this catalog. Existing custom/saved names are preserved as an extra option. Save the AI settings to apply a different voice.

Thai food names on the TFT use U8g2's `etl16thai_t` font, embedded locally. Original font copyright: “Public domain font. Share and enjoy.” Source: https://github.com/olikraus/u8g2/blob/master/tools/font/build/single_font_files/u8g2_font_etl16thai_t.c

Also restored the missing account/media/upload page functions from commit `e3ba6d7`; their absence in the current source caused `files is not defined` during login rendering.

Validation: production FoodStore host tests use in-memory Preferences/mutex adapters; browser tests cover menu editing, filtering, random results, escaped names, modal cancellation, mobile sizing and the voice selector. Device rendering and physical NVS still require an on-board check; firmware has not been flashed automatically.
