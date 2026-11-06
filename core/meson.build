fravashid_core_inc = include_directories('src')

fravashid_core = static_library(
  'fravashid-core',
  'src/layout.c',
  'src/media.c',
  'src/playlist.c',
  include_directories: fravashid_core_inc,
  gnu_symbol_visibility: 'hidden',
)

subdir('tests')
