project('RinVulkan', 'c', version: '1.0.0', default_options: ['warning_level=3'])

ringpu_dep = dependency('ringpu', required: true)
rinvulkan_inc = include_directories('include')
rinvulkan_sources = files(
  'src/buffer_ownership.c',
  'src/command_runtime.c',
  'src/descriptor_runtime.c',
  'src/graphics.c',
  'src/graphics_runtime.c',
  'src/icd.c',
  'src/profile.c',
  'src/runtime.c',
  'src/platform/time.c',
  'src/software_platform.c',
  'src/sync2_scope.c',
  'src/wsi.c',
)
rinvulkan = library('rinvulkan', rinvulkan_sources,
  include_directories: rinvulkan_inc,
  dependencies: ringpu_dep,
  install: true,
)
rinvulkan_dep = declare_dependency(
  link_with: rinvulkan,
  include_directories: rinvulkan_inc,
  dependencies: ringpu_dep,
)

rinvulkan_abi_test = executable(
  'rinvulkan-abi-test',
  'tests/rinvulkan_abi_test.c',
  dependencies: rinvulkan_dep,
)
test('rinvulkan-abi', rinvulkan_abi_test)

rinvulkan_graphics_runtime_test = executable(
  'rinvulkan-graphics-runtime-test',
  'tests/rinvulkan_graphics_runtime_test.c',
  dependencies: rinvulkan_dep,
)
test('rinvulkan-graphics-runtime', rinvulkan_graphics_runtime_test)

rinvulkan_buffer_ownership_test = executable(
  'rinvulkan-buffer-ownership-test',
  'tests/rinvulkan_buffer_ownership_test.c',
  'src/buffer_ownership.c',
  include_directories: include_directories('include', 'src'),
)
test('rinvulkan-buffer-ownership', rinvulkan_buffer_ownership_test)

rinvulkan_sync2_scope_test = executable(
  'rinvulkan-sync2-scope-test',
  'tests/rinvulkan_sync2_scope_test.c',
  'src/sync2_scope.c',
  include_directories: include_directories('include', 'src'),
)
test('rinvulkan-sync2-scope', rinvulkan_sync2_scope_test)

rinvulkan_platform_time_test = executable(
  'rinvulkan-platform-time-test',
  'tests/rinvulkan_platform_time_test.c',
  'src/platform/time.c',
  include_directories: include_directories('src'),
)
test('rinvulkan-platform-time', rinvulkan_platform_time_test)
