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

## Reproduction

Set `OTE_TEST_OTB_FILES` to semicolon-separated absolute OTB paths and run `editor_tests realOtbCompatibility`. The test skips when no corpus is configured. On restricted Windows environments, use a writable temporary directory and `QT_QPA_PLATFORM=offscreen`.
