project('RinVulkan', 'c', version: '1.0.0', default_options: ['warning_level=3'])

ringpu_dep = dependency('ringpu', required: true)
rinvulkan_inc = include_directories('include')
rinvulkan_sources = files(
  'src/command_runtime.c',
  'src/descriptor_runtime.c',
  'src/graphics.c',
  'src/graphics_runtime.c',
  'src/icd.c',
  'src/profile.c',
  'src/runtime.c',
  'src/software_platform.c',
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
