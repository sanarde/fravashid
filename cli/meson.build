executable(
  'fravashi',
  'src/main.c',
  c_args: ['-DFRAVASHID_LOCALEDIR="' + get_option('prefix') / get_option('localedir') + '"'],
  dependencies: [dependency('dbus-1'), dependency('libcfgd')],
  install: true,
)
