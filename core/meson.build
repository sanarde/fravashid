fravashid_core_inc = include_directories('src')

fravashid_core = static_library(
  'fravashid-core',
  'src/layout.c',
  'src/media.c',
  'src/paint.c',
  'src/playlist.c',
  'src/spec.c',
  include_directories: fravashid_core_inc,
  gnu_symbol_visibility: 'hidden',
)

subdir('tests')
