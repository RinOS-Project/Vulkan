project('RinVulkan', 'c', version: '1.0.0', meson_version: '>=1.3.0',
        default_options: ['warning_level=3'])

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

if host_machine.system() == 'linux'
  rinvulkan_icd = shared_library(
    'rin-vulkan-icd', rinvulkan_sources,
    include_directories: rinvulkan_inc,
    dependencies: ringpu_dep,
    link_args: ['-Wl,-Bsymbolic'],
    install: true,
  )

  fs = import('fs')
  icd_install_dir = join_paths(get_option('datadir'), 'vulkan', 'icd.d')
  icd_library_relative_dir = fs.relative_to(
    get_option('libdir'), icd_install_dir)
  icd_configuration = configuration_data()
  icd_configuration.set('RIN_VULKAN_ICD_LIBRARY',
                        'librin-vulkan-icd.so')
  icd_configuration.set('RIN_VULKAN_ICD_LIBRARY_RELATIVE_DIR',
                        icd_library_relative_dir)
  configure_file(
    input: 'icd.d/rin-vulkan.json.in',
    output: 'rin-vulkan.json',
    configuration: icd_configuration,
    install_dir: icd_install_dir,
  )

  icd_build_configuration = configuration_data()
  icd_build_configuration.set('RIN_VULKAN_ICD_LIBRARY',
                              'librin-vulkan-icd.so')
  icd_build_configuration.set('RIN_VULKAN_ICD_LIBRARY_RELATIVE_DIR', '.')
  configure_file(
    input: 'icd.d/rin-vulkan.json.in',
    output: 'rin-vulkan-build.json',
    configuration: icd_build_configuration,
  )

  python3 = find_program('python3')
  test('rinvulkan-icd-manifest-template', python3,
       args: files('scripts/check_manifest.py'))
  test('rinvulkan-icd-build-manifest', python3,
       args: [files('scripts/check_manifest.py'),
              join_paths(meson.current_build_dir(), 'rin-vulkan-build.json'),
              rinvulkan_icd.full_path()],
       depends: [rinvulkan_icd])
endif

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

rinvulkan_wsi_display_test = executable(
  'rinvulkan-wsi-display-test',
  'tests/rinvulkan_wsi_display_test.c',
  dependencies: rinvulkan_dep,
)
test('rinvulkan-wsi-display', rinvulkan_wsi_display_test)

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
