# Patches for bochs on LXsystem

## `0001-Use-pkg-config-for-SDL2.patch`

Use pkg-config for SDL2

Modifies configure to use `pkg_config` to detect SDL2 installed on
LXsystem.

Without this patch, configure uses `sdl2_config` and will detect SDL2
installed on the host system.

