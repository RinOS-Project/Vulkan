vulkan_kernel64_profile = static_library(
  'vulkan-profile-kernel64',
  files('../../src/profile.c'),
  include_directories: include_directories('../../include'),
  c_args: kernel64_flags + kernel64_project_warning_flags,
  pic: false,
  install: false,
  build_by_default: false,
)
