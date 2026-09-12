# AI Pet personalities

In the web portal, open **AI conversation → บุคลิกและหน้าตา**. Selecting a card queues a persisted appearance setting and updates the device's AI Pet renderer once the setting is applied. The nine options are Friendly, Playful, Calm, Curious, Confident, Shy, Cheeky, Savage and Custom. Appearance changes are allowed during a Live session; mouth movement, listening indicators, temporary moods and encoder interactions remain active.

Each card has a matching Gemini Live conversation preset. **ใช้ preset Gemini Live** fills the provider, voice and system instruction in the form. It does not save the conversation automatically. Review or edit the text and press the conversation form's Save button. Exit AI Pet before saving conversation settings while a session is active, as required by the existing session lifecycle. Selecting an appearance alone never overwrites a custom prompt or voice.

| Profile | Voice | Appearance |
| --- | --- | --- |
| Friendly | Sulafat | Cream eyes, mint accent |
| Playful | Puck | Peach palette, wider eyes, sparkles |
| Calm | Achernar | Lavender, relaxed rounded eyes |
| Curious | Sadaltager | Sky blue, tall eyes, spectacles |
| Confident | Kore | Gold, angular eyes, crown |
| Shy | Vindemiatrix | Pink, smaller eyes, prominent cheeks |
| Cheeky | Fenrir | Lime palette, one narrowed eye, sideways grin |
| Savage | Fenrir | Red/orange eyes, slanted brows, pointed horns and a sneer |
| Custom | User choice | Editable colors, eye sizes, cheek size, corner radius and base expression |

`preferences::Values.petPersonality` uses the old zero-valued reserved byte. The version-1 binary record size, offsets and checksum algorithm are unchanged; existing records map to Friendly and retain other preferences. IDs outside 0–8 are rejected. Saves use the existing NVS write/rollback path. AI presets use the existing persisted voice/system-instruction configuration.

Validation covers old setting layout compatibility, all nine persisted IDs, invalid IDs and write failures; the built portal's nine selectors and presets, custom prompt preservation, reload and mobile fit; and production-rendered mouth movement and appearance previews. Preview images use a desktop drawing adapter, not a physical TFT capture. No board flash or live Gemini conversation was performed automatically.

## Custom editor

Select Custom, choose a base expression (any of the eight built-in profiles), adjust the four colors and shape sliders, select a voice and write a prompt. The preview follows the inputs. **Save Custom** stores the design and its own voice/prompt in device NVS; changing to another personality does not overwrite it. **Apply preset** copies the currently edited custom voice/prompt into AI conversation; save that form to activate it for Gemini. Appearance changes can be saved while talking; conversation settings retain the existing requirement to exit AI Pet first.

The custom record has its own version and checksum under `tofan-ui/petCustom`, independent of the original UI record. Invalid fields, oversized UTF-8 prompts (over 511 bytes) and failed writes are rejected. Corrupt custom records fall back to defaults while other settings are retained.
