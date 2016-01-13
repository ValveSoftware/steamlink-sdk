# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'mozilla',
      'type': '<(component)',
      'sources': [
        'ComplexTextInputPanel.h',
        'ComplexTextInputPanel.mm',
        'MozillaExport.h',
        'NSPasteboard+Utils.h',
        'NSPasteboard+Utils.mm',
        'NSScreen+Utils.h',
        'NSScreen+Utils.m',
        'NSString+Utils.h',
        'NSString+Utils.mm',
        'NSURL+Utils.h',
        'NSURL+Utils.m',
        'NSWorkspace+Utils.h',
        'NSWorkspace+Utils.m',
      ],
      'defines': [
        'MOZILLA_IMPLEMENTATION',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
        ],
      },
      'dependencies': [
        '../../url/url.gyp:url_lib',
      ],
      'conditions': [
        ['component=="shared_library"',
          {
            # Needed to link to Obj-C static libraries.
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          }
        ],
      ],
    },
  ],
}
