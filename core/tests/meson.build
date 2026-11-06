check_inc = include_directories('.')

foreach name : ['layout', 'media', 'playlist']
  test_exe = executable(
    'test_' + name,
    'test_' + name + '.c',
    include_directories: [fravashid_core_inc, check_inc],
    link_with: fravashid_core,
  )
  test('core-' + name, test_exe)
endforeach
