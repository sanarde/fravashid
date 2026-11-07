# Sanar Fravashid

> [!NOTE]
> **Repository Mirrors**
>
> - **git.nuros.org** ([sanarde/fravashid](https://git.nuros.org/sanarde/fravashid)): primary, self-hosted Forgejo instance, accounts restricted to the core team.
> - **GitHub** ([sanarde/fravashid](https://github.com/sanarde/fravashid)): mirror for external contributors. Issues and Pull Requests opened here are welcome and are reviewed and processed by the core team.

## About

Wallpaper daemon for SanarDE that shows images and videos on Wayland and X11 and reads its settings from cfgd.

## Dependencies

- C17 compiler
- Meson (>= 1.0) and Ninja
- libdbus-1
- libcfgd, the client library of [cfgd](https://git.nuros.org/sanarde/cfgd)
- wayland-client, wayland-protocols and wayland-scanner
- libxcb with the RandR extension
- libpng and libjpeg
- libmpv
- gettext tools (xgettext, msgmerge, msgfmt)

## Build

```sh
meson setup build
meson compile -C build
meson test -C build
meson install -C build
```

The install step puts `fravashid` and `fravashi` in `bin`, the cfgd schema in `share/cfgd/schemas`, the D-Bus service file in `share/dbus-1/services` and the autostart entry in `<sysconfdir>/xdg/autostart`.

## Running

`fravashid` selects Wayland when `WAYLAND_DISPLAY` is set and X11 otherwise. Use `fravashid -b wayland` or `fravashid -b x11` to choose. Only one instance can run per session.

On Wayland the compositor must support `wlr-layer-shell`. GNOME Shell does not. Each output gets a surface on the background layer. On X11 the image is drawn to the root window and published as `_XROOTPMAP_ID`, one region per RandR monitor. Videos are played without sound and in a loop.

## Settings

Settings are keys of the cfgd schema `org.nuros.sanar.fravashi`, path `/org/nuros/sanar/fravashi/`. Changes apply without a restart. Without cfgd the defaults are used.

- `wallpaper`: image (PNG, JPEG), video (MP4, MKV, WebM) or a directory of them. Empty shows the background color only.
- `mode`: `fill`, `fit`, `stretch`, `center` or `tile`. Default `fill`.
- `color`: background color as `#rrggbb`. Default `#000000`.
- `interval`: seconds between two files of a directory. Zero keeps the first file.
- `shuffle`: play the files of a directory in random order.
- `outputs`: list of `NAME=PATH` entries that replace `wallpaper` on single outputs. Names are the ones of the compositor or of RandR, for example `DP-1`.

```sh
cfgd set /org/nuros/sanar/fravashi/mode fit
cfgd set /org/nuros/sanar/fravashi/interval 600
```

## Command line

```
fravashi get
fravashi next
fravashi prev
fravashi set [-o OUTPUT] PATH
```

`get` prints the current file of every output. `next` and `prev` move every directory one file. `set` writes `wallpaper`, or the entry of `OUTPUT` in `outputs`, through cfgd.

## D-Bus interface

Session bus, name `org.nuros.sanar.Fravashi`, object `/org/nuros/sanar/fravashi`, interface `org.nuros.sanar.Fravashi`.

- `Next()`
- `Previous()`
- `GetCurrent() -> a{ss}`: output name to current file, empty when none.
- Signal `Changed(s output, s path)`.

## Seurat

The first output reports its image to [Seurat](https://git.nuros.org/sanarde/seurat) with `SetWallpaper(s path)` so that system colors follow the wallpaper. A video or a missing file is reported as an empty path. The path is sent again when `seuratd` appears on the bus later than `fravashid`.

## Links

- [CONTRIBUTING](CONTRIBUTING.md)

## License

GPL-3.0-only. See [LICENSE](LICENSE). The file `protocols/wlr-layer-shell-unstable-v1.xml` is taken from [wlr-protocols](https://gitlab.freedesktop.org/wlroots/wlr-protocols) and keeps its own license text.
