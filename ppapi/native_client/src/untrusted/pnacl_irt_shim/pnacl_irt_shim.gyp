# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../../native_client/build/untrusted.gypi',
  ],
  'targets': [
    {
      # The full library, which PNaCl uses for offline .pexe -> .nexe.
      'target_name': 'pnacl_irt_shim_aot',
      'type': 'none',
      'variables': {
        'nlib_target': 'libpnacl_irt_shim.a',
        'out_pnacl_newlib_arm': '>(tc_lib_dir_pnacl_translate)/lib-arm/>(nlib_target)',
        'out_pnacl_newlib_x86_32': '>(tc_lib_dir_pnacl_translate)/lib-x86-32/>(nlib_target)',
        'out_pnacl_newlib_x86_64': '>(tc_lib_dir_pnacl_translate)/lib-x86-64/>(nlib_target)',
        'out_pnacl_newlib_mips': '>(tc_lib_dir_pnacl_translate)/lib-mips32/>(nlib_target)',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
        'pnacl_native_biased': 1,
        'enable_x86_32': 1,
        'enable_x86_64': 1,
        'enable_arm': 1,
        'enable_mips': 1,
        'sources': [
          'irt_shim_ppapi.c',
          'pnacl_shim.c',
          'shim_entry.c',
          'shim_ppapi.c',
        ],
        'extra_args': [
          '--strip-debug',
        ],
        # Indicate that shim should not depend on unstable IRT hook interface.
        'compile_flags': [
          '-DPNACL_SHIM_AOT',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    # Smaller shim library for PNaCl in-browser translation.
    # Uses an unstable IRT hook interface to get the shim from the IRT itself.
    # If we ever change that hook interface or change the in-IRT shim's ABI,
    # we would need to clear the translation cache to match the new IRT.
    {
      'target_name': 'pnacl_irt_shim_browser',
      'type': 'none',
      'variables': {
        # Same name as pnacl_irt_shim_aot, so that we don't need to change
        # the linker commandlines, but output to the "for_browser" directory,
        # and have the pnacl_support_extension copy from that directory.
        'nlib_target': 'libpnacl_irt_shim.a',
        'out_pnacl_newlib_arm': '>(tc_lib_dir_pnacl_translate)/lib-arm/for_browser/>(nlib_target)',
        'out_pnacl_newlib_x86_32': '>(tc_lib_dir_pnacl_translate)/lib-x86-32/for_browser/>(nlib_target)',
        'out_pnacl_newlib_x86_64': '>(tc_lib_dir_pnacl_translate)/lib-x86-64/for_browser/>(nlib_target)',
        'out_pnacl_newlib_mips': '>(tc_lib_dir_pnacl_translate)/lib-mips32/for_browser/>(nlib_target)',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
        'pnacl_native_biased': 1,
        'enable_x86_32': 1,
        'enable_x86_64': 1,
        'enable_arm': 1,
        'enable_mips': 1,
        'sources': [
          'shim_entry.c',
          'shim_ppapi.c',
        ],
        'extra_args': [
          '--strip-debug',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      # Second half of shim library for PNaCl in-browser translation.
      # This half goes into the IRT and is returned by the unstable
      # IRT hook interface.
      'target_name': 'pnacl_irt_shim_for_irt',
      'type': 'none',
      'variables': {
        'nlib_target': 'libpnacl_irt_shim_for_irt.a',
        'build_glibc': 0,
        'build_newlib': 0,
        # Unlike the above, build this the way the IRT is built so that the
        # output library directories match the IRT linking search paths.
        'build_irt': 1,
        'sources': [
          'irt_shim_ppapi.c',
          'pnacl_shim.c',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
  ],
}
