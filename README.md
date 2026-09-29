# OTEditor

OTEditor is a desktop Tibia object editor built with **Qt 6, QML, and C++17**. Its workspace has an information and preview sidebar, an object browser, an object editor, a sprite browser, and a log panel. Panel widths are adjustable.

## Getting started

For the Windows release ZIP, extract the archive and run `OTEditor.exe` from the extracted `OTEditor` folder. Keep the entire folder together; the executable alone does not include the required Qt files. For a local build, run `dist/OTEditor.exe`.

1. Choose **File → Open** (`Ctrl+O`) and select a folder containing DAT and SPR files.
2. When available, the editor also loads `items.otb`, `items.xml`, and OTFI from the project folder or their usual nearby locations.
3. Select the DAT format version, such as 772, 860, or 1098. OTFI settings take precedence for extended sprites, transparency, frame durations, and frame groups. A custom client's protocol version may differ from its DAT format version.
4. Select an object to inspect it. Double-click an object to open its properties, or double-click a sprite to assign it to an object slot.
5. Press `Ctrl+S` to compile the current project. `Ctrl+Shift+S` compiles the complete project into a new folder, including DAT, SPR, OTFI, and any loaded `items.otb` and `items.xml` files.

## Features

- One project session for DAT, editable SPR, OTFI, `items.otb`, and `items.xml`, backed by the local `otformats` library.
- Creation of a missing `items.otb` from the Attributes tab or Tools menu. The file is saved to the selected server folder, or to the client folder if no server folder is selected. **Create Missing OTB Items** adds remaining entries from DAT in batches and shows progress.
- Virtualized browsers for items, outfits, effects, missiles, and sprites, with list and grid views, ID filtering, and an option to hide objects without assigned sprites.
- Multi-selection in the object browser: Ctrl selects individual objects, and Shift selects a range. Batch export writes a separate file for each selected ID without overwriting existing files.
- Layer-aware previews with nearest-neighbor scaling, transparency checkerboard, pixel grid, and frame and pattern selection.
- Editing of item flags and dimensions, animation frames, layers, patterns, and sprite assignments.
- Creating, duplicating, and removing objects in all four DAT categories while preserving later IDs.
- Undo/redo for attribute edits, sprite assignments, and clearing (up to 100 operations). Creating or duplicating an item starts a new undo history.
- **Compile** and **Compile As** save the full project. Existing OTFI settings are preserved; projects without OTFI receive a compatible metadata file.
- SPR editing: replace a sprite with a PNG of the configured sprite size, add sprites, or clear a slot without shifting IDs. The writer supports RLE, standard and extended IDs, RGB, and alpha.
- Drag a sprite from the sprite browser onto a texture tile to assign it to that exact frame, pattern, layer, and position. Drop an image from the file manager onto the texture to import a single sprite or a complete sprite sheet. Use **Save** in the inspector to accept dropped texture changes or **Reset** to restore the object; compiling the project accepts pending drops. Matching sheets replace the selected animation group; a combined outfit sheet can replace all groups. For objects with one frame, layer, and pattern, the editor detects width and height from the image's tile dimensions. Images are limited to 16 million pixels and sheets to 4,096 sprites.
- **Tools → Slicer** opens a sprite-sheet workspace with rulers, a checkerboard, rotation, mirroring, grid selection, and zoom. **Crop** collects tiles in column order, converts magenta to transparency, and skips empty sprites by default; **Import** adds the tiles to the open client's SPR.
- Atomic DAT, SPR, OTB, XML, and OTFI writes through `QSaveFile`. Unchanged sprite blocks are copied without recompression.
- **Export** saves object sheets as PNG, BMP, JPG, or ObjectBuilder OBD v3 (32×32 sprites). Outfit directions occupy columns; Idle and Walking frames continue in successive rows. Export supports a custom name and destination, multiple selected objects, and a transparent PNG background. Object context menus also import OBD v3 objects into the selected slot.
- **File → Export All** writes objects, animation sheets, or sprites with progress. **File → Merge** appends DAT objects and sprites from a compatible client project. Export preferences are saved between sessions; **New Window** opens an independent editor.
- **Tools → Save items.otb / items.xml** writes either server file separately; the Copy commands export a snapshot without changing the active project path or clearing pending edits. OTB tools also edit IDs and version data, reload selected entries, and compare another OTB file.
- A warning before closing with unsaved changes, an activity log, and keyboard shortcuts.
- Object context menus in both list and grid views: Replace, Export, Import OBD, Edit, Duplicate, Bulk Edit, Bulk Replace, Compare, copy/paste object, copy/paste patterns and graphics, copy/paste DAT properties, copy/paste OTB attributes, Remove, and copy Client or Server ID. Pasting preserves the target ID. Clipboards are independent and reset when opening another project.
- A failed attempt to open another client leaves the current project intact.

## Object editor

The central **Object Editor** has **Texture**, **Properties**, and **Attributes** tabs. Double-clicking an object opens Properties. Texture provides zoom, animation frames, Film Roll, grid and crop guides, and object structure. Animated items and effects also show the selected frame's minimum and maximum duration and the animation's total duration; edits support undo/redo.

The preview arranges Pattern X variants side by side and Pattern Y/Z variants in subsequent rows, up to 256 visible patterns. Outfit, effect, and missile dimensions, layers, patterns, and frame counts can be changed in Texture. These changes update the DAT model immediately, support undo/redo, and are saved during Compile.

Properties groups DAT flags and values. Attributes exposes supported OTB fields, including names synchronized with `items.xml`. Other `items.xml` fields do not yet have editing controls. Use Compile to save project files.

**Edit Pixels** paints individual sprite pixels. Outfit bone offsets can be edited on clients that support them. The separate **Useable** flag remains disabled. Action names other than None are currently shown as numbers. Available fields depend on the DAT version. Market names must fit the Latin-1 encoding used by the format.

## Current limitations

OTEditor is not yet a complete replacement for [ObjectBuilder](https://github.com/punkice3407/ObjectBuilder). OBD support currently covers v3 and 32×32 sprites; older OBD versions are not supported. Remaining gaps include editing every OTB/XML field and general client-version conversion.

OTB and XML participate in loading and compilation, and OTB names are synchronized with `items.xml`. The slicer imports sprites from a selected sheet area. Preview playback uses frame durations from DAT. Real-client round trips have been checked with local 7.72 and 10.98 assets; other supported versions need broader validation.

## Building on Windows

Requirements: Qt 6.5+ (Core, Gui, Qml, Quick, QuickControls2, Test), liblzma, CMake 3.24+, Ninja, and a compiler compatible with your Qt installation. The script's defaults target a local Qt 6.10.2 / MinGW 13.1 installation.

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

ObjectBuilder was used as a functional reference; its code was not copied. Client graphics are not included in the distribution.
