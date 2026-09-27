# OTEditor

Desktopowy edytor obiektów Tibii: **Qt 6 / QML + C++17**. Układ oparty na dostarczonej referencji ObjectBuilder: Info i Preview po lewej, katalog obiektów, obszar edycji, sprite'y po prawej oraz log na dole. Panele mają regulowaną szerokość.

## Uruchomienie

Po zbudowaniu paczki uruchom `dist/OTEditor.exe`. Nie wymaga instalacji Qt na komputerze docelowym; przenoś cały katalog `dist`, nie sam plik EXE.

1. File → Open client folder (`Ctrl+O`).
2. Wybierz katalog zawierający DAT i SPR. Edytor automatycznie dołączy `items.otb`, `items.xml` i OTFI, jeżeli są dostępne w katalogu projektu lub jego standardowym sąsiedztwie.
3. Ustaw wersję układu DAT (np. 772, 860, 1098). OTFI ma pierwszeństwo przy ustalaniu extended sprites, alpha, frame durations i frame groups. Wersja protokołu custom klienta może różnić się od wersji jego DAT.
4. Wybierz obiekt. Dwuklik otwiera atrybuty. Dwuklik sprite'a otwiera przypisanie do slotu obiektu.
5. `Ctrl+S` kompiluje bieżący projekt. `Ctrl+Shift+S` kompiluje kompletny projekt do nowego katalogu: DAT, SPR, OTFI oraz załadowane `items.otb` i `items.xml`.

## Zaimplementowane

- Wspólna sesja projektu dla DAT, edytowalnego SPR, OTFI, `items.otb` i `items.xml` przez lokalną bibliotekę `otformats`.
- Jeśli klient nie ma `items.otb`, można utworzyć nowy plik z zakładki Attributes lub menu Tools. Plik trafia do wybranego katalogu serwerowego, a gdy go nie wskazano — do katalogu klienta. Polecenie Create Missing OTB Items dodaje pozostałe wpisy na podstawie DAT partiami i pokazuje postęp.
- Wirtualizowane katalogi przedmiotów, outfitów, efektów, pocisków i sprite'ów; lista/siatka, filtr ID, ukrywanie obiektów bez przypisanych sprite'ów.
- W katalogu obiektów Ctrl zaznacza pojedyncze pozycje, a Shift zaznacza zakres. Eksport obiektów i arkuszy animacji dla wielu zaznaczonych pozycji zapisuje osobny PNG na ID do wybranego folderu, nie nadpisując istniejących plików.
- Podgląd z warstwami, skalowanie nearest-neighbor, szachownica przezroczystości, siatka pikseli, wybór klatki i wzorca.
- Edycja flag i wymiarów przedmiotów, liczby klatek, warstw, wzorców oraz przypisań sprite'ów.
- Tworzenie i duplikowanie przedmiotów; czyszczenie z zachowaniem ID, bez przesuwania identyfikatorów innych obiektów.
- Undo/redo zmian atrybutów, przypisań i czyszczenia (100 operacji). Tworzenie/duplikowanie rozpoczyna nową historię.
- Compile i Compile As zapisujące pełny zestaw plików projektu. Oryginalny OTFI jest zachowywany wraz z dodatkowymi ustawieniami; dla projektów bez OTFI tworzony jest zgodny plik opisowy.
- Edycja SPR: zamiana sprite'a obrazem PNG 32×32, dodawanie nowych sprite'ów i czyszczenie slotu bez przesuwania identyfikatorów. Writer obsługuje RLE, standardowe i extended ID, RGB i kanał alpha.
- Tools → Slicer otwiera dialog z linijkami, szachownicą i listą sprite'ów. Pozwala obracać i odbijać obraz, wybierać obszar siatką oraz powiększać podgląd. Crop zbiera kafelki w kolejności kolumnowej, zamienia magentę na przezroczystość i domyślnie pomija puste sprite'y; Import dodaje zebrane kafelki do SPR otwartego klienta.
- Atomowy zapis DAT, SPR, OTB, XML i OTFI przez QSaveFile. Niezmienione bloki sprite'ów są kopiowane bez rekompresji.
- Okno Export zapisuje arkusz obiektu do PNG, BMP lub JPG. Dla outfitów umieszcza kierunki w kolumnach, a klatki Idle i Walking w kolejnych wierszach bez przerwy między grupami. Można nazwać plik, wybrać folder, wyeksportować wiele zaznaczonych obiektów i w PNG wybrać przezroczyste tło. OBD jest widoczne jako niedostępne, dopóki eksport nie będzie zgodny z ObjectBuilderem.
- Ochrona przed zamknięciem niezapisanych zmian, log, skróty klawiaturowe.
- Menu pod prawym przyciskiem na obiekcie w siatce i liście: Replace (z innego ID w projekcie), Export, Edit, Duplicate, kopiowanie/wklejanie obiektu, wzorców/grafiki, właściwości DAT i atrybutów OTB, Remove oraz kopiowanie Client ID i Server ID z OTB. Wklejanie zachowuje docelowe ID. Schowki są niezależne i resetowane przy otwarciu projektu; Patterns obejmuje strukturę, sprite'y i animację, Properties — flagi i ich wartości bez zmiany grafiki. Bulk Edit i Compare pozostają nieaktywne.
- Nieudane otwarcie innego klienta nie usuwa bieżącego projektu.

## Inspekcja obiektu

Wybranie obiektu wyświetla w środkowym panelu Object Editor zakładki Texture, Properties i Attributes. Dwuklik przełącza panel na Properties. Texture zawiera zoom, klatki animacji (Film Roll), siatkę, granice crop oraz strukturę obiektu. Dla animowanych itemów i efektów pokazuje też minimum i maksimum czasu wybranej klatki oraz sumę czasów animacji; zmiany obsługują undo/redo. Podgląd układa wzorce Pattern X obok siebie, a Pattern Y/Z w kolejnych rzędach (do 256 widocznych wzorców). Wymiary, warstwy, wzorce i klatki outfitów, efektów i pocisków można zmieniać w panelu Texture; zmiany trafiają od razu do modelu DAT, obsługują undo/redo i zapisują się przy Compile. Properties grupuje właściwości i flagi DAT. Attributes pozwala edytować obsługiwane pola OTB, w tym nazwę synchronizowaną z items.xml. Pozostałe pola items.xml nie mają jeszcze formularza. Zapis plików projektu odbywa się przez Compile.

Edit Pixels, Has Bones i osobna flaga Useable pozostają nieaktywne. Nazwy akcji poza None są obecnie prezentowane jako numery. Zakres dostępności pól zależy od wersji DAT. Nazwy Market muszą mieścić się w kodowaniu Latin-1 używanym przez ten format.
## Zakres kolejnych etapów

To nadal nie jest pełny zamiennik [ObjectBuildera](https://github.com/punkice3407/ObjectBuilder). Pozostały: przypisywanie sprite'ów outfitom/efektom/pociskom i edycja ich flag, obsługa Has Bones, rysowanie pikseli bezpośrednio w aplikacji, import/eksport obiektów OBD, operacje masowe, formularze edycji pól OTB/XML i konwersja wersji klienta. OTB oraz XML są już częścią sesji i kompilacji, a nazwy OTB są synchronizowane z `items.xml`. Slicer importuje sprite'y z wybranego obszaru arkusza. Przeglądarka outfitów pokazuje pierwszą grupę animacji. Odtwarzanie podglądu ma stały interwał 150 ms; zapis zachowuje istniejące czasy klatek, a po zmianie liczby klatek inicjalizuje je na 100 ms. Nie deklarujemy sprawdzenia wszystkich wersji 7.10–13.10.

## Budowanie w Windows

Wymagane: Qt 6.5+ (Core, Gui, Qml, Quick, QuickControls2, Test), CMake 3.24+, Ninja i kompilator zgodny z Qt. Skrypt jest skonfigurowany dla dostępnego lokalnie Qt 6.10.2 / MinGW 13.1.

```powershell
./scripts/build.ps1 -Deploy
```

Jeśli `dist/OTEditor.exe` jest uruchomiony, skrypt umieszcza nową wersję w `dist/OTEditor.next.exe` i podmienia główny plik automatycznie po zamknięciu aplikacji. Nie zamyka uruchomionego edytora, więc przed wyjściem można zapisać bieżące zmiany.

`QT_ROOT` pozwala zmienić katalog Qt. Ścieżki MinGW i Ninja są w skrypcie. Ręczne budowanie innym toolchainem:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.10.2/mingw_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Testy generują własne miniaturowe DAT/SPR w katalogu tymczasowym. Sprawdzają odczyt, undo/redo, pełną kompilację, surowe zachowanie niezmienionych bloków SPR, zapis RLE w RGB i RGBA, standardowe i extended liczniki, ponowny odczyt, dodatkowe flagi i animację, uszkodzone kategorie, eksport PNG, duplikowanie i walidację sprite'ów.

Parametry diagnostyczne:

```powershell
./dist/OTEditor.exe --folder "C:/path/to/client" --version 1098
./dist/OTEditor.exe --folder "C:/path/to/client" --version 1098 --screenshot "C:/path/to/preview.png"
```

## Struktura i pochodzenie

- `libs/otformats`: kopia biblioteki z projektu ModernItemEditor, z poprawkami lokalnymi. Projekt źródłowy nie został zmieniony.
- `src/projectmodel.*`: wspólna sesja DAT/SPR/OTFI/OTB/XML oraz Compile/Compile As.
- `src/editorbackend.cpp` i `src/editorbackend.h`: model obiektów, edycja, historia i dostawca obrazów. `src/editorbackend_slicer.cpp` oraz `src/editorbackend_export.cpp` zawierają odpowiednio cięcie sprite'ów i eksport obrazów.
- `qml/Main.qml`: stan okna, menu, połączenia między panelami i oknami dialogowymi. `*Panel.qml` zawierają panele przestrzeni roboczej, a nazwane `*Dialog.qml` — okna poszczególnych narzędzi. Wspólne kontrolki są w `AssetToggle.qml`, `Tool.qml`, `Panel.qml` i `Checker.qml`.
- `tests/`: regresje formatów i backendu.

Repozytorium ObjectBuilder służyło jako referencja funkcjonalna; jego kod nie został skopiowany. Dane graficzne klienta nie są dołączone do dystrybucji.
