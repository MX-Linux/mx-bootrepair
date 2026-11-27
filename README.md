# mx-bootrepair

Simple GUI for re-installing/repairing GRUB in MX Linux

[![build result](https://build.opensuse.org/projects/home:mx-packaging/packages/mx-bootrepair/badge.svg?type=default)](https://software.opensuse.org//download.html?project=home%3Amx-packaging&package=mx-bootrepair)

[![Packaging status](https://repology.org/badge/vertical-allrepos/mx-bootrepair.svg)](https://repology.org/project/mx-bootrepair/versions)

![image](https://github.com/MX-Linux/mx-bootrepair/assets/418436/8cdd3c79-4c76-4f00-a78f-5ebdc90aa553)

## CLI Mode

- Force CLI: run `mx-boot-repair -c` or `mx-boot-repair --cli`.
- Automatic fallback: when no windowing environment is detected (no `DISPLAY` and no `WAYLAND_DISPLAY`), the program starts in CLI mode automatically.
- The CLI offers the same repair actions as the GUI via interactive prompts. For non‑interactive usage, see the `mx-boot-repair-cli` helper (flags like `--action`, `--target`, `--root`, etc.).
- Logs are written to `/tmp/mx-boot-repair.log` in both GUI and CLI modes.

## AppImage

Download the x86-64 AppImage from a GitHub release’s **Assets**, make it executable with `chmod +x`, and launch it as your desktop user. The image includes both repair helpers. The selected repair uses GRUB, filesystem, encryption and initramfs tools from the host or the target installation. Administrative actions reopen the AppImage with the host’s elevation tool and require working FUSE. Run it from a location that the administrator can access.

To build locally on Debian 13 (trixie), install the build dependencies and run the packaging script:

```sh
sudo apt-get install \
    ca-certificates git build-essential cmake ninja-build dpkg-dev curl file \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-svg-plugins qt6-translations-l10n \
    qt6-wayland
./make-appimage.sh
```

The script writes `build-appimage/mx-boot-repair-<version>-x86_64.AppImage` using the version in `debian/changelog`. GitHub Actions builds the same image and attaches it to each non-Arch release.
