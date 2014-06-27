# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'display',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'DISPLAY_IMPLEMENTATION',
      ],
      'sources': [
        'chromeos/display_mode.cc',
        'chromeos/display_mode.h',
        'chromeos/display_snapshot.cc',
        'chromeos/display_snapshot.h',
        'chromeos/native_display_delegate.h',
        'chromeos/native_display_observer.h',
        'chromeos/output_configurator.cc',
        'chromeos/output_configurator.h',
        'chromeos/ozone/native_display_delegate_ozone.cc',
        'chromeos/ozone/native_display_delegate_ozone.h',
        'chromeos/ozone/touchscreen_delegate_ozone.cc',
        'chromeos/ozone/touchscreen_delegate_ozone.h',
        'chromeos/x11/display_mode_x11.cc',
        'chromeos/x11/display_mode_x11.h',
        'chromeos/x11/display_snapshot_x11.cc',
        'chromeos/x11/display_snapshot_x11.h',
        'chromeos/x11/display_util_x11.cc',
        'chromeos/x11/display_util_x11.h',
        'chromeos/x11/native_display_delegate_x11.cc',
        'chromeos/x11/native_display_delegate_x11.h',
        'chromeos/x11/native_display_event_dispatcher_x11.cc',
        'chromeos/x11/native_display_event_dispatcher_x11.h',
        'chromeos/x11/touchscreen_delegate_x11.cc',
        'chromeos/x11/touchscreen_delegate_x11.h',
        'display_constants.h',
        'display_export.h',
        'edid_parser.cc',
        'edid_parser.h',
        'x11/edid_parser_x11.cc',
        'x11/edid_parser_x11.h',
      ],
      'conditions': [
        ['use_x11 == 1', {
          'dependencies': [
            '../../build/linux/system.gyp:x11',
            '../../build/linux/system.gyp:xext',
            '../../build/linux/system.gyp:xi',
            '../../build/linux/system.gyp:xrandr',
          ],
        }],
      ],
    },
    {
      'target_name': 'display_test_util',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
        'display',
      ],
      'defines': [
        'DISPLAY_IMPLEMENTATION',
      ],
      'sources': [
        'chromeos/test/test_display_snapshot.cc',
        'chromeos/test/test_display_snapshot.h',
      ],
    },
  ],
}
