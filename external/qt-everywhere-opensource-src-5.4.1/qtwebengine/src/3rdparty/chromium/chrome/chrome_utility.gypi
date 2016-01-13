# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'utility',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
        '../components/components_strings.gyp:components_strings',
        '../components/components.gyp:url_fixer',
        '../content/content.gyp:content_common',
        '../content/content.gyp:content_utility',
        '../media/media.gyp:media',
        '../skia/skia.gyp:skia',
        '../third_party/libexif/libexif.gyp:libexif',
        '../third_party/libxml/libxml.gyp:libxml',
        '<(DEPTH)/chrome/chrome_resources.gyp:chrome_resources',
        '<(DEPTH)/chrome/chrome_resources.gyp:chrome_strings',
        'common',
        'common/extensions/api/api.gyp:chrome_api',
      ],
      'include_dirs': [
        '..',
        '<(grit_out_dir)',
      ],
      'export_dependent_settings': [
        'common/extensions/api/api.gyp:chrome_api',
      ],
      'sources': [
        'utility/chrome_content_utility_client.cc',
        'utility/chrome_content_utility_client.h',
        'utility/chrome_content_utility_ipc_whitelist.cc',
        'utility/chrome_content_utility_ipc_whitelist.h',
        'utility/cloud_print/bitmap_image.cc',
        'utility/cloud_print/bitmap_image.h',
        'utility/cloud_print/pwg_encoder.cc',
        'utility/cloud_print/pwg_encoder.h',
        'utility/extensions/unpacker.cc',
        'utility/extensions/unpacker.h',
        'utility/image_writer/disk_unmounter_mac.cc',
        'utility/image_writer/disk_unmounter_mac.h',
        'utility/image_writer/error_messages.cc',
        'utility/image_writer/error_messages.h',
        'utility/image_writer/image_writer.cc',
        'utility/image_writer/image_writer.h',
        'utility/image_writer/image_writer_handler.cc',
        'utility/image_writer/image_writer_handler.h',
        'utility/image_writer/image_writer_mac.cc',
        'utility/image_writer/image_writer_win.cc',
        'utility/importer/bookmark_html_reader.cc',
        'utility/importer/bookmark_html_reader.h',
        'utility/importer/bookmarks_file_importer.cc',
        'utility/importer/bookmarks_file_importer.h',
        'utility/importer/external_process_importer_bridge.cc',
        'utility/importer/external_process_importer_bridge.h',
        'utility/importer/favicon_reencode.cc',
        'utility/importer/favicon_reencode.h',
        'utility/importer/firefox_importer.cc',
        'utility/importer/firefox_importer.h',
        'utility/importer/ie_importer_win.cc',
        'utility/importer/ie_importer_win.h',
        'utility/importer/importer.cc',
        'utility/importer/importer.h',
        'utility/importer/importer_creator.cc',
        'utility/importer/importer_creator.h',
        'utility/importer/nss_decryptor.cc',
        'utility/importer/nss_decryptor.h',
        'utility/importer/nss_decryptor_mac.h',
        'utility/importer/nss_decryptor_mac.mm',
        'utility/importer/nss_decryptor_win.cc',
        'utility/importer/nss_decryptor_win.h',
        'utility/importer/safari_importer.h',
        'utility/importer/safari_importer.mm',
        'utility/local_discovery/service_discovery_message_handler.cc',
        'utility/local_discovery/service_discovery_message_handler.h',
        'utility/media_galleries/image_metadata_extractor.cc',
        'utility/media_galleries/image_metadata_extractor.h',
        'utility/media_galleries/ipc_data_source.cc',
        'utility/media_galleries/ipc_data_source.h',
        'utility/media_galleries/itunes_pref_parser_win.cc',
        'utility/media_galleries/itunes_pref_parser_win.h',
        'utility/media_galleries/media_metadata_parser.cc',
        'utility/media_galleries/media_metadata_parser.h',
        'utility/printing_handler.cc',
        'utility/printing_handler.h',
        'utility/profile_import_handler.cc',
        'utility/profile_import_handler.h',
        'utility/utility_message_handler.h',
        'utility/web_resource_unpacker.cc',
        'utility/web_resource_unpacker.h',
      ],
      'conditions': [
        ['OS=="win" or OS=="mac"', {
          'dependencies': [
            '../components/components.gyp:wifi_component',
          ],
          'sources': [
            'utility/media_galleries/iapps_xml_utils.cc',
            'utility/media_galleries/iapps_xml_utils.h',
            'utility/media_galleries/itunes_library_parser.cc',
            'utility/media_galleries/itunes_library_parser.h',
            'utility/media_galleries/picasa_album_table_reader.cc',
            'utility/media_galleries/picasa_album_table_reader.h',
            'utility/media_galleries/picasa_albums_indexer.cc',
            'utility/media_galleries/picasa_albums_indexer.h',
            'utility/media_galleries/pmp_column_reader.cc',
            'utility/media_galleries/pmp_column_reader.h',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'utility/media_galleries/iphoto_library_parser.cc',
            'utility/media_galleries/iphoto_library_parser.h',
          ],
        }],
        ['use_openssl==1', {
          'sources!': [
            'utility/importer/nss_decryptor.cc',
          ]
        }],
        ['OS!="win" and OS!="mac" and use_openssl==0', {
          'dependencies': [
            '../crypto/crypto.gyp:crypto',
          ],
          'sources': [
            'utility/importer/nss_decryptor_system_nss.cc',
            'utility/importer/nss_decryptor_system_nss.h',
          ],
        }],
        ['OS=="android"', {
          'dependencies!': [
            '../third_party/libexif/libexif.gyp:libexif',
          ],
          'sources/': [
            ['exclude', '^utility/importer/'],
            ['exclude', '^utility/media_galleries/'],
            ['exclude', '^utility/profile_import_handler\.cc'],
          ],
        }],
        ['OS!="win" and OS!="mac"', {
          'sources': [
            'utility/image_writer/image_writer_stub.cc',
          ]
        }],
        ['enable_printing!=1', {
          'sources!': [
            'utility/printing_handler.cc',
            'utility/printing_handler.h',
          ]
        }],
        ['enable_mdns==0', {
          'sources!': [
            'utility/local_discovery/service_discovery_message_handler.cc',
            'utility/local_discovery/service_discovery_message_handler.h',
          ]
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
}
