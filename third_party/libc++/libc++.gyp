# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libc++',
      'type': 'shared_library',
      'variables': {
        'prune_self_dependency': 1,
        # Don't add this target to the dependencies of targets with type=none.
        'link_dependency': 1,
      },
      'dependencies=': [
        '../libc++abi/libc++abi.gyp:libc++abi',
      ],
      'sources': [
        'trunk/src/algorithm.cpp',
        'trunk/src/bind.cpp',
        'trunk/src/chrono.cpp',
        'trunk/src/condition_variable.cpp',
        'trunk/src/debug.cpp',
        'trunk/src/exception.cpp',
        'trunk/src/future.cpp',
        'trunk/src/hash.cpp',
        'trunk/src/ios.cpp',
        'trunk/src/iostream.cpp',
        'trunk/src/locale.cpp',
        'trunk/src/memory.cpp',
        'trunk/src/mutex.cpp',
        'trunk/src/new.cpp',
        'trunk/src/optional.cpp',
        'trunk/src/random.cpp',
        'trunk/src/regex.cpp',
        'trunk/src/shared_mutex.cpp',
        'trunk/src/stdexcept.cpp',
        'trunk/src/string.cpp',
        'trunk/src/strstream.cpp',
        'trunk/src/system_error.cpp',
        'trunk/src/thread.cpp',
        'trunk/src/typeinfo.cpp',
        'trunk/src/utility.cpp',
        'trunk/src/valarray.cpp',
      ],
      'include_dirs': [
        'trunk/include',
        '../libc++abi/trunk/include',
      ],
      'cflags': [
        '-g', '-Os', '-fPIC',
        '-std=c++11',
        '-fstrict-aliasing',
        '-Wall',
        '-Wextra',
        '-Wshadow',
        '-Wconversion',
        '-Wnewline-eof',
        '-Wpadded',
        '-Wmissing-prototypes',
        '-Wstrict-aliasing=2',
        '-Wstrict-overflow=4',
        '-nostdinc++',
      ],
      'direct_dependent_settings': {
        'target_conditions': [
          ['_type!="none"', {
            'include_dirs': [
              'trunk/include',
            ],
            'cflags_cc': [
              '-nostdinc++',
            ],
            'ldflags': [
              '-stdlib=libc++',
              '-L<(PRODUCT_DIR)/lib/',
            ],
          }],
        ],
      },
      'cflags_cc!': [
        '-fno-rtti',
      ],
      'cflags!': [
        '-fno-exceptions',
        '-fvisibility=hidden',
      ],
      'ldflags': [
        '-nodefaultlibs',
        '<(PRODUCT_DIR)/lib/libc++abi.so',
      ],
      'ldflags!': [
        # This somehow causes a warning from clang about an unused compilation
        # option. Use '-lpthread' instead.
        # TODO(earthdok): find out what's causing the warning.
        '-pthread',
      ],
      'libraries': [
        '-lc',
        '-lgcc_s',
        '-lpthread',
        '-lrt',
      ],
    },
  ]
}
