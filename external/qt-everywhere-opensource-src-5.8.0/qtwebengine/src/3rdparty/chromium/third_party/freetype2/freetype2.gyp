# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'variables': {
    'ft2_dir': 'src',
 },
 'targets': [
    {
      # We use a hard-coded shared library version in product_extension
      # in order to match the version used on Ubuntu Precise.
      # We also disable bzip2 and the ftpatent code to match the version
      # that shipped on Ubuntu Lucid.
      'target_name': 'freetype2',
      'type': 'shared_library',
      'product_name': 'freetype',
      'product_extension': 'so.6',
      'toolsets': ['target'],
      'sources': [
        '<(ft2_dir)/src/base/ftsystem.c',
        '<(ft2_dir)/src/base/ftinit.c',
        '<(ft2_dir)/src/base/ftdebug.c',

        '<(ft2_dir)/src/base/ftbase.c',

        '<(ft2_dir)/src/base/ftbbox.c',
        '<(ft2_dir)/src/base/ftglyph.c',

        '<(ft2_dir)/src/base/ftbdf.c',
        '<(ft2_dir)/src/base/ftbitmap.c',
        '<(ft2_dir)/src/base/ftcid.c',
        '<(ft2_dir)/src/base/ftfstype.c',
        '<(ft2_dir)/src/base/ftgasp.c',
        '<(ft2_dir)/src/base/ftgxval.c',
        '<(ft2_dir)/src/base/ftlcdfil.c',
        '<(ft2_dir)/src/base/ftmm.c',
        '<(ft2_dir)/src/base/ftpfr.c',
        '<(ft2_dir)/src/base/ftstroke.c',
        '<(ft2_dir)/src/base/ftsynth.c',
        '<(ft2_dir)/src/base/fttype1.c',
        '<(ft2_dir)/src/base/ftwinfnt.c',
        '<(ft2_dir)/src/base/ftxf86.c',

        '<(ft2_dir)/src/bdf/bdf.c',
        '<(ft2_dir)/src/cff/cff.c',
        '<(ft2_dir)/src/cid/type1cid.c',
        '<(ft2_dir)/src/pcf/pcf.c',
        '<(ft2_dir)/src/pfr/pfr.c',
        '<(ft2_dir)/src/sfnt/sfnt.c',
        '<(ft2_dir)/src/truetype/truetype.c',
        '<(ft2_dir)/src/type1/type1.c',
        '<(ft2_dir)/src/type42/type42.c',
        '<(ft2_dir)/src/winfonts/winfnt.c',

        '<(ft2_dir)/src/psaux/psaux.c',
        '<(ft2_dir)/src/psnames/psnames.c',
        '<(ft2_dir)/src/pshinter/pshinter.c',

        '<(ft2_dir)/src/raster/raster.c',
        '<(ft2_dir)/src/smooth/smooth.c',

        '<(ft2_dir)/src/autofit/autofit.c',
        '<(ft2_dir)/src/gzip/ftgzip.c',
        '<(ft2_dir)/src/lzw/ftlzw.c',
      ],
      'defines': [
        'FT_CONFIG_OPTION_SYSTEM_ZLIB',
        'FT2_BUILD_LIBRARY',
        'FT_CONFIG_CONFIG_H=<ftconfig.h>',  # See comments in README.chromium.
        'FT_CONFIG_MODULES_H=<ftmodule.h>',  # See comments in README.chromium.
      ],
      'include_dirs': [
        'include',
        '<(ft2_dir)/include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
          '<(ft2_dir)/include',
        ],
      },
      'link_settings': {
        'libraries': [
          '-lz',
        ],
      },
    },
  ], # targets
}
