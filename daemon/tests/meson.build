check_inc = include_directories('../../core/tests')
test_data_dir = meson.project_source_root() / 'daemon/tests/data'

test_image = executable(
  'test_image',
  'test_image.c',
  c_args: ['-DTEST_DATA_DIR="' + test_data_dir + '"'],
  include_directories: [fravashid_src_inc, fravashid_core_inc, check_inc],
  link_with: [fravashid_daemon, fravashid_core],
  dependencies: fravashid_deps,
)
test('daemon-image', test_image)
