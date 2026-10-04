# OTEditor

OTEditor is a desktop Tibia object editor built with **Qt 6, QML, and C++20**. Its workspace has an information and preview sidebar, an object browser, an object editor, a sprite browser, and a log panel. Panel widths are adjustable.

## Getting started

For the Windows release ZIP, extract the archive and run `OTEditor.exe` from the extracted `OTEditor` folder. Keep the entire folder together; the executable alone does not include the required Qt files. For a local build, run `dist/OTEditor.exe`.

1. Choose **File → Open** (`Ctrl+O`) and select a folder containing DAT and SPR files.
2. The **Server Files Folder** is optional. Select the folder containing `items.otb` and `items.xml` to load server data. Leave it empty to open only client files and hide server attributes. Server files are read directly from the selected folder; parent folders and subfolders are not searched. OTFI is read from the client folder.
3. Select the DAT format version, such as 772, 860, or 1098. OTFI settings take precedence for extended sprites, transparency, frame durations, and frame groups. A custom client's protocol version may differ from its DAT format version.
4. Select an object to inspect it. Double-click an object to open its properties, or double-click a sprite to assign it to an object slot.
5. Press `Ctrl+S` to compile the current project. `Ctrl+Shift+S` opens **Compile As**, where you choose the output folder, asset name, DAT version, Extended, Transparency, Improved animations, Frame Groups, and whether to export loaded server files. It writes a separate DAT/SPR/OTFI project and leaves the open project unchanged.

## Features

- One project session for DAT, editable SPR, OTFI, `items.otb`, and `items.xml`, backed by the local `otformats` library.
- Creation of a missing `items.otb` from the Attributes tab or Tools menu. The file is saved to the selected server folder, or to the client folder if no server folder is selected. **Create Missing OTB Items** adds remaining entries from DAT in batches and shows progress.
- Virtualized browsers for items, outfits, effects, missiles, and sprites, with list and grid views, ID filtering, and an option to hide objects without assigned sprites.
- Multi-selection in the object browser: Ctrl selects individual objects, and Shift selects a range. Batch export writes a separate file for each selected ID without overwriting existing files.
- Layer-aware previews with nearest-neighbor scaling, transparency checkerboard, pixel grid, and frame and pattern selection.
- Editing of item flags and dimensions, animation frames, layers, patterns, and sprite assignments.
- Creating, duplicating, and removing objects in all four DAT categories while preserving later IDs.
- Undo/redo for attribute edits, sprite assignments, and clearing (up to 100 operations). Creating or duplicating an item starts a new undo history.
- **Compile** saves the open project. **Compile As** converts it to a separate named DAT/SPR/OTFI set using the selected format options, with optional `items.otb` and `items.xml` export. Existing OTFI settings are preserved when compiling the current project; projects without OTFI receive a compatible metadata file.
- SPR editing: replace a sprite with a PNG of the configured sprite size, add sprites, or clear a slot without shifting IDs. The writer supports RLE, standard and extended IDs, RGB, and alpha.
- Drag a sprite from the sprite browser onto a texture tile to assign it to that exact frame, pattern, layer, and position. Drop an image from the file manager onto the texture to import a single sprite or a complete sprite sheet. Use **Save** in the inspector to accept dropped texture changes or **Reset** to restore the object; compiling the project accepts pending drops. Matching sheets replace the selected animation group; a combined outfit sheet can replace all groups. For objects with one frame, layer, and pattern, the editor detects width and height from the image's tile dimensions. Images are limited to 16 million pixels and sheets to 4,096 sprites.
- **Tools → Slicer** opens a sprite-sheet workspace with rulers, a checkerboard, rotation, mirroring, grid selection, and zoom. **Crop** collects tiles in column order, converts magenta to transparency, and skips empty sprites by default; **Import** adds the tiles to the open client's SPR.
- Atomic DAT, SPR, OTB, XML, and OTFI writes through `QSaveFile`. Unchanged sprite blocks are copied without recompression.
- **Export** saves object sheets as PNG, BMP, JPG, or ObjectBuilder OBD v3 (32×32 sprites). Outfit directions occupy columns; Idle and Walking frames continue in successive rows. Export supports a custom name and destination, multiple selected objects, and a transparent PNG background. Object context menus import OBD v1, v2, and v3 objects into the selected slot.
- **File → Export All** writes objects, animation sheets, or sprites with progress. **File → Merge** appends DAT objects and sprites from a compatible client project. Export preferences are saved between sessions; **New Window** opens an independent editor.
- **File → Convert Project** writes a separate project in a selected DAT version from 7.72 through 13.10. It keeps object IDs, remaps used sprites, adapts unsupported flags and frame groups, and copies loaded OTB/XML server data without changing the open project.
- **Compile** saves loaded server files together with client files. OTB tools also edit IDs and version data, reload selected entries, and compare another OTB file.
- A warning before closing with unsaved changes, an activity log, and keyboard shortcuts.
- Object context menus in both list and grid views: Replace, Export, Import OBD, Edit, Duplicate, Bulk Edit, Bulk Replace, Compare, copy/paste object, copy/paste patterns and graphics, copy/paste DAT properties, copy/paste OTB attributes, Remove, and copy Client or Server ID. Pasting preserves the target ID. Clipboards are independent and reset when opening another project.
- A failed attempt to open another client leaves the current project intact.

## Object editor

**Tools → AI Sprite Generator** connects to AI Sprite Studio without requiring an open client. Enter your API key, a prompt of 1–2000 characters, and choose **Creature Sheet** (`qwen21-midhem-256`, 12 credits, 256×192 transparent PNG) or **Flat 2D Item** (`tibia-style-items-1`, 4 credits, 32×32 transparent PNG). Custom model IDs remain available. The documented defaults are ready to use: POST `https://aispritestudio.com/api/v1/generations`, GET `/api/v1/generations/{jobId}`, and `Authorization: Bearer` on submissions, status checks, and downloads. **API Settings** retains advanced endpoint and authentication overrides. Browser sign-in cookies do not authenticate API calls.

The generator previews transparent PNGs with nearest-neighbor scaling. **Save PNG** opens the native save dialog; **Open in Slicer** passes the result to the existing slicing workspace. The model chooses the output dimensions. It does not automatically create an object or assign directions and frames. Adding sliced sprites to SPR requires an open client.

Generation uses the provider's credits. Every new submission gets a unique `Idempotency-Key`. A POST is never automatically retried; after an uncertain submission, **Retry Submission** resends the original prompt and model with the same key to recover the original job without another charge. **Cancel** stops local requests and polling; the provider may still complete and charge for the job. **Check Result** resumes an existing job without submitting another generation. Status is checked every three seconds, with a ten-minute local deadline; accepted jobs remain on the provider and can be checked again. Rate-limit responses respect `Retry-After`; status checks and downloads retry after that delay without resubmitting a generation. Downloads are limited to 32 MB and PNGs to 16 million pixels. API keys stay in memory and are excluded from saved settings; credentials are sent only to the configured API origin. Generated preview files live in a temporary session directory. Save downloaded PNGs: deleting an image from the provider's Media removes its API result. The integration has been verified with a local API fixture; a paid live generation has not been run.

The central **Object Editor** has **Texture**, **Properties**, and **Attributes** tabs. Double-clicking an object opens Properties. Texture provides zoom, animation frames, Film Roll, grid and crop guides, and object structure. Animated items and effects also show the selected frame's minimum and maximum duration and the animation's total duration; edits support undo/redo.

The preview arranges Pattern X variants side by side and Pattern Y/Z variants in subsequent rows, up to 256 visible patterns. Outfit, effect, and missile dimensions, layers, patterns, and frame counts can be changed in Texture. These changes update the DAT model immediately, support undo/redo, and are saved during Compile.

Properties groups DAT flags and values. Attributes places OTB flags beside item values, Name, Type, and a named Stack Order selector. Multi Use and Force Use are separate flags. Identity and additional flags are available in an expandable section; names are synchronized with `items.xml`. Use Compile to save project files.
The Tools menu also opens a generic `items.xml` key/value attribute editor for the selected server item. It preserves unrelated XML entries and can override individual IDs from a ranged entry. Removing an attribute from a ranged entry is blocked because that could silently change the whole range.

**Edit Pixels** paints individual sprite pixels. Outfit bone offsets can be edited on clients that support them. The separate **Useable** flag is available for clients that support it. Action names other than None are currently shown as numbers. Available fields depend on the DAT version. Market names must fit the Latin-1 encoding used by the format.

## Current limitations

OTEditor is not yet a complete replacement for [ObjectBuilder](https://github.com/punkice3407/ObjectBuilder). OBD import covers v1–v3 with 32×32 sprites; export writes v3. The XML editor covers standard key/value attributes; complex nested XML nodes remain preserved but are not editable through its controls. Version conversion adapts DAT/SPR and copies OTB/XML; it does not rewrite arbitrary server XML semantics or recreate unsupported client features.

OTB and XML participate in loading and compilation, and OTB names are synchronized with `items.xml`. The slicer imports sprites from a selected sheet area. Preview playback uses frame durations from DAT. Real-client round trips have been checked with local 7.72 and 10.98 assets, version detection with 7.72, 8.00, and 10.98, and conversion from a real 7.72 client to 10.98. Other supported versions need broader validation.

## Building on Windows

Requirements: Qt 6.5+ (Core, Gui, Qml, Quick, QuickControls2, Network, Widgets, Test), liblzma, CMake 3.24+, Ninja, and a C++20 compiler compatible with your Qt installation. The script's defaults target a local Qt 6.10.2 / MinGW 13.1 installation.

```powershell
./scripts/build.ps1 -Deploy
```

If `dist/OTEditor.exe` is running, the script stages the new build as `dist/OTEditor.next.exe` and replaces the main executable after the app closes. It does not close the editor, so current changes can be saved first.

Set `QT_ROOT` to use another Qt installation. The script also contains local MinGW and Ninja paths. To build manually with another toolchain:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.10.2/mingw_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests generate small DAT and SPR fixtures in a temporary directory. They cover parsing, undo/redo, complete compilation, preservation of unchanged SPR blocks, RGB/RGBA RLE writing, standard and extended counters, reloading, additional flags and animation data, malformed categories, PNG export, duplication, and sprite validation.

The application and its local `otformats` library use C++20. The project builds with MinGW GCC 13.1, and both test suites pass.

Diagnostic launch options:

```powershell
./dist/OTEditor.exe --folder "C:/path/to/client" --version 1098
./dist/OTEditor.exe --folder "C:/path/to/client" --version 1098 --screenshot "C:/path/to/preview.png"
```

## Project layout and provenance

- `libs/otformats`: a copy of the library from ModernItemEditor with local fixes. The source project was not modified.
- `src/projectmodel.*`: the shared DAT/SPR/OTFI/OTB/XML session and Compile/Compile As.
- `src/editorbackend.cpp` and `src/editorbackend.h`: object model, editing, history, and image provider. `src/editorbackend_slicer.cpp` and `src/editorbackend_export.cpp` contain sprite slicing and image export.
- `qml/Main.qml`: window state, menus, and connections between panels and dialogs. Named `*Panel.qml` and `*Dialog.qml` files contain workspace panels and tools. Shared controls are in `AssetToggle.qml`, `Tool.qml`, `Panel.qml`, and `Checker.qml`.
- `tests/`: format and backend regression tests.

ObjectBuilder was used as a functional reference; its code was not copied. Its official client-signature catalog is included as `assets/ObjectBuilder-versions.xml` under the accompanying `assets/ObjectBuilder-LICENSE.txt` (MIT). Client graphics are not included in the distribution.
