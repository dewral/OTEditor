# OTB compatibility test report

Date: 2026-10-04

## Results

- 37 real OTB files; 765403 item records checked.
- Major OTB versions 1, 2, and 3.
- 44 checks passed; no failures or skips.
- Original files were read only. Outputs were written into temporary directories.

## Coverage

All exposed item properties were compared before and after save/reload. Decoded node headers (group and all flag bits), root metadata, and opaque attribute payloads including sprite hashes were compared separately. Existing tests also exercised server attribute editing, OTB creation, copies retaining active paths, OTB tools, and XML attribute editing. The writer may normalize known attributes and the file identifier; binary-identical output is not required.

This verifies OTEditor round-trip preservation on the tested corpus. It does not certify that every saved file loads in every TFS/RME/Object Builder version; those external application tests were not run.

## Corpus

| Source | Items | OTB major/minor/build |
| --- | ---: | --- |
| Maps/SoeRPG/RMe/data/986/items.otb | 20568 | 3/49/51 |
| Maps/SoeRPG/RMe/data/970/items.otb | 18455 | 3/43/45 |
| Maps/SoeRPG/RMe/data/960/items.otb | 18437 | 3/40/42 |
| Maps/SoeRPG/RMe/data/954/items.otb | 17333 | 3/39/41 |
| Maps/SoeRPG/RMe/data/946/items.otb | 15985 | 3/35/37 |
| Maps/SoeRPG/RMe/data/920/items.otb | 14220 | 3/29/32 |
| Maps/SoeRPG/RMe/data/910/items.otb | 13882 | 3/28/31 |
| Maps/SoeRPG/RMe/data/870/items.otb | 13232 | 3/23/24 |
| Maps/SoeRPG/RMe/data/1286/items.otb | 35687 | 3/62/62 |
| Maps/SoeRPG/RMe/data/860/items.otb | 12561 | 3/20/20 |
| Maps/SoeRPG/RMe/data/1285/items.otb | 35625 | 3/61/62 |
| Maps/SoeRPG/RMe/data/854/items.otb | 11295 | 3/17/17 |
| Maps/SoeRPG/RMe/data/1281/items.otb | 35152 | 3/60/62 |
| Maps/SoeRPG/RMe/data/850/items.otb | 10444 | 3/15/13 |
| Maps/SoeRPG/RMe/data/740/items.otb | 3035 | 1/1/1 |
| Maps/SoeRPG/RMe/data/800/items.otb | 7376 | 2/7/2 |
| Maps/SoeRPG/RMe/data/1290/items.otb | 37292 | 3/64/62 |
| Maps/SoeRPG/RMe/data/1287/items.otb | 35737 | 3/63/62 |
| Maps/SoeRPG/RMe/data/810/items.otb | 8175 | 2/8/2 |
| Maps/SoeRPG/RMe/data/760.old/items.otb | 5006 | 1/3/17 |
| Maps/SoeRPG/RMe/data/1310/items.otb | 38242 | 3/65/62 |
| Maps/SoeRPG/RMe/data/840/items.otb | 9918 | 3/12/7 |
| Maps/SoeRPG/RMe/data/1020/items.otb | 21633 | 3/51/56 |
| Maps/SoeRPG/RMe/data/820/items.otb | 8974 | 3/10/5 |
| Maps/SoeRPG/RMe/data/1031/items.otb | 22571 | 3/54/59 |
| Maps/SoeRPG/RMe/data/1271/items.otb | 35018 | 3/59/62 |
| Maps/SoeRPG/RMe/data/1021/items.otb | 21633 | 3/52/57 |
| Maps/SoeRPG/RMe/data/1010/items.otb | 21453 | 3/50/55 |
| Maps/SoeRPG/RMe/data/10100/items.otb | 27768 | 3/58/63 |
| Maps/SoeRPG/RMe/data/1035/items.otb | 22603 | 3/55/60 |
| Maps/SoeRPG/RMe/data/1098/items.otb | 26282 | 3/57/62 |
| Maps/SoeRPG/RMe/data/1041/items.otb | 22603 | 3/55/60 |
| Maps/SoeRPG/RMe/data/1077/items.otb | 24233 | 3/56/61 |
| Maps/SoeRPG/RMe/data/1030/items.otb | 22571 | 3/53/58 |
| Midhem-SPR/items.otb | 13501 | 1/57/81 |
| TibiaGameEngine/data/items/items.otb | 51772 | 3/64/62 |
| TeriaEngine/data/items/items.otb | 5131 | 3/3/49 |

## Reproduction

Set `OTE_TEST_OTB_FILES` to semicolon-separated absolute OTB paths and run `editor_tests realOtbCompatibility`. The test skips when no corpus is configured. On restricted Windows environments, use a writable temporary directory and `QT_QPA_PLATFORM=offscreen`.
