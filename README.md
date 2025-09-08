# mx-bootrepair

Simple GUI for re-installing/repairing GRUB in MX Linux

[![latest packaged version(s)](https://repology.org/badge/latest-versions/mx-bootrepair.svg)](https://repology.org/project/mx-bootrepair/versions)
[![build result](https://build.opensuse.org/projects/home:mx-packaging/packages/mx-bootrepair/badge.svg?type=default)](https://software.opensuse.org//download.html?project=home%3Amx-packaging&package=mx-bootrepair)

![image](https://github.com/MX-Linux/mx-bootrepair/assets/418436/8cdd3c79-4c76-4f00-a78f-5ebdc90aa553)

## CLI Mode

- Force CLI: run `mx-boot-repair -c` or `mx-boot-repair --cli`.
- Automatic fallback: when no windowing environment is detected (no `DISPLAY` and no `WAYLAND_DISPLAY`), the program starts in CLI mode automatically.
- The CLI offers the same repair actions as the GUI via interactive prompts. For non‑interactive usage, see the `mx-boot-repair-cli` helper (flags like `--action`, `--target`, `--root`, etc.).
- Logs are written to `/tmp/mx-boot-repair.log` in both GUI and CLI modes.
