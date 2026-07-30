# CONTENT-1A Runtime Hotfix

Fixes the first macOS runtime findings from CONTENT-1A / VM-SETTINGS-1A.

- Moves the SHA-256 streaming buffer from a 1 MiB automatic array to a 64 KiB
  heap allocation. The original implementation overflowed the NSURLSession
  worker thread stack on Apple silicon.
- Commits active key-field editing and dismisses the GPU settings window after
  Apply.
- Reasserts the Metal view as first responder after sheets/window transitions.
- Consumes movement key-up events after removing them from the pressed-key set.

This hotfix does not add textured Metal rendering. Content acquisition and
cataloging remain CONTENT-1A; original Shapes texture sampling is TEX-1A.
