# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'google_toolbox_for_mac',
      'type': '<(component)',
      'include_dirs': [
        '.',
        'src',
        'src/AppKit',
        'src/DebugUtils',
        'src/Foundation',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
          'src',
          'src/AppKit',
          'src/DebugUtils',
          'src/Foundation',
        ],
      },
      'sources': [
        'src/AppKit/GTMCarbonEvent.h',
        'src/AppKit/GTMCarbonEvent.m',
        'src/AppKit/GTMFadeTruncatingTextFieldCell.h',
        'src/AppKit/GTMFadeTruncatingTextFieldCell.m',
        'src/AppKit/GTMIBArray.h',
        'src/AppKit/GTMIBArray.m',
        'src/AppKit/GTMKeyValueAnimation.h',
        'src/AppKit/GTMKeyValueAnimation.m',
        'src/AppKit/GTMNSAnimation+Duration.h',
        'src/AppKit/GTMNSAnimation+Duration.m',
        'src/AppKit/GTMNSBezierPath+CGPath.h',
        'src/AppKit/GTMNSBezierPath+CGPath.m',
        'src/AppKit/GTMNSBezierPath+RoundRect.h',
        'src/AppKit/GTMNSBezierPath+RoundRect.m',
        'src/AppKit/GTMNSColor+Luminance.h',
        'src/AppKit/GTMNSColor+Luminance.m',
        'src/AppKit/GTMUILocalizer.h',
        'src/AppKit/GTMUILocalizer.m',
        'src/AppKit/GTMUILocalizerAndLayoutTweaker.h',
        'src/AppKit/GTMUILocalizerAndLayoutTweaker.m',
        'src/DebugUtils/GTMDebugSelectorValidation.h',
        'src/DebugUtils/GTMMethodCheck.h',
        'src/DebugUtils/GTMMethodCheck.m',
        'src/DebugUtils/GTMTypeCasting.h',
        'src/Foundation/GTMLightweightProxy.h',
        'src/Foundation/GTMLightweightProxy.m',
        'src/Foundation/GTMLogger.h',
        'src/Foundation/GTMLogger.m',
        'src/Foundation/GTMNSDictionary+URLArguments.h',
        'src/Foundation/GTMNSDictionary+URLArguments.m',
        'src/Foundation/GTMNSString+URLArguments.h',
        'src/Foundation/GTMNSString+URLArguments.m',
        'src/Foundation/GTMServiceManagement.c',
        'src/Foundation/GTMServiceManagement.h',
        'src/Foundation/GTMStringEncoding.h',
        'src/Foundation/GTMStringEncoding.m',
        'src/GTMDefines.h',
        'src/iPhone/GTMFadeTruncatingLabel.h',
        'src/iPhone/GTMFadeTruncatingLabel.m',
        'src/iPhone/GTMRoundedRectPath.h',
        'src/iPhone/GTMRoundedRectPath.m',
        'src/iPhone/GTMUIImage+Resize.h',
        'src/iPhone/GTMUIImage+Resize.m',
        'src/iPhone/GTMUILocalizer.h',
        'src/iPhone/GTMUILocalizer.m',
      ],
      'conditions': [
        ['component=="shared_library"',
          {
            # GTM is third-party code, so we don't want to add _EXPORT
            # annotations to it, so build it without -fvisibility=hidden
            # (else the interface class symbols will be hidden in a 64bit
            # build). Only do this in a component build, so that the shipping
            # chrome binary doesn't end up with unnecessarily exported
            # symbols.
            'xcode_settings': {
              'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
            },
          }
        ],
        ['OS!="ios"', {
          'sources/': [
            ['exclude', '^src/iPhone/'],
            ['exclude', '^src/DebugUtils/GTMMethodCheck\\.m$'],
            ['exclude', '^src/Foundation/GTMLogger\\.m$'],
            ['exclude', '^src/Foundation/GTMNSDictionary\\+URLArguments\\.m$'],
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AddressBook.framework',
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
              '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
            ],
          },
        }, {  # OS=="ios"
          'sources/': [
            # Exclude everything except what's needed for iOS.
            ['exclude', '\\.(c|m)$'],
            ['include', '^src/DebugUtils/GTMMethodCheck\\.m$'],
            ['include', '^src/Foundation/GTMLightweightProxy\\.m$'],
            ['include', '^src/Foundation/GTMLogger\\.m$'],
            ['include', '^src/Foundation/GTMNSDictionary\\+URLArguments\\.m$'],
            ['include', '^src/Foundation/GTMNSString\\+URLArguments\\.m$'],
            ['include', '^src/Foundation/GTMStringEncoding\\.m$'],
            ['include', '^src/iPhone/'],
          ],
          # TODO(crbug.com/569158): Suppresses warnings that are treated as
          # errors when minimum iOS version support is increased to iOS 9. This
          # should be removed once all deprecation violations have been fixed.
          'xcode_settings': {
            'WARNING_CFLAGS': ['-Wno-deprecated-declarations'],
          },
        }],
      ],
    },
  ],
}
