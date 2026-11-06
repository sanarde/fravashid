fs = import('fs')

dbus_dep = dependency('dbus-1')
cfgd_dep = dependency('libcfgd')
wayland_dep = dependency('wayland-client')
wayland_protocols_dep = dependency('wayland-protocols')
xcb_dep = dependency('xcb')
xcb_randr_dep = dependency('xcb-randr')
png_dep = dependency('libpng')
jpeg_dep = dependency('libjpeg')
mpv_dep = dependency('mpv')

wayland_scanner = find_program('wayland-scanner')
xdg_shell_xml = wayland_protocols_dep.get_variable('pkgdatadir') / 'stable/xdg-shell/xdg-shell.xml'
protocol_xml = [
  xdg_shell_xml,
  meson.project_source_root() / 'protocols/wlr-layer-shell-unstable-v1.xml',
]

protocol_sources = []
foreach xml : protocol_xml
  name = fs.stem(xml)
  protocol_sources += custom_target(
    name + '-code',
    input: xml,
    output: name + '-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
  protocol_sources += custom_target(
    name + '-header',
    input: xml,
    output: name + '-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
endforeach

fravashid_deps = [
  dbus_dep,
  cfgd_dep,
  wayland_dep,
  xcb_dep,
  xcb_randr_dep,
  png_dep,
  jpeg_dep,
  mpv_dep,
]

fravashid_src_inc = include_directories('src')

fravashid_daemon = static_library(
  'fravashid-daemon',
  'src/app.c',
  'src/bus.c',
  'src/image.c',
  'src/settings.c',
  'src/video.c',
  'src/wayland.c',
  'src/x11.c',
  protocol_sources,
  include_directories: [fravashid_src_inc, fravashid_core_inc],
  link_with: fravashid_core,
  dependencies: fravashid_deps,
)

executable(
  'fravashid',
  'src/main.c',
  include_directories: [fravashid_src_inc, fravashid_core_inc],
  link_with: [fravashid_daemon, fravashid_core],
  dependencies: fravashid_deps,
  install: true,
)

subdir('tests')
