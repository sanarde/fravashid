# Contributing

- Build and run the tests before sending a change: `meson setup build && meson test -C build`.
- For changes in C code also run the tests with sanitizers: `meson setup build-san -Db_sanitize=address,undefined && meson test -C build-san`.
- Settings, application logic, backends, the video player, the D-Bus service and the command line tool have no automated tests. Check changes to them by hand on Wayland (a compositor with `wlr-layer-shell`) and on X11.
- Keep each change limited to one area.
- Report bugs with the version, the steps to reproduce, and the observed output.

Source layout:

- `core`: scaling modes, painting, playlists and value parsing, without any display or bus code.
- `daemon`: `fravashid`, the Wayland and X11 backends, video, settings and the D-Bus service.
- `cli`: the `fravashi` tool.
- `data`: cfgd schema, D-Bus service file and autostart entry.
- `protocols`: Wayland protocol XML that the system does not provide.
- `po`: message catalogs of the command line tool.
