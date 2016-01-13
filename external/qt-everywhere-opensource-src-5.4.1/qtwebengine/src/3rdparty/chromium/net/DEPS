include_rules = [
  "+crypto",
  "+gin/public",
  "+jni",
  "+third_party/apple_apsl",
  "+third_party/libevent",
  "+third_party/nss",
  "+third_party/zlib",
  "+sdch/open-vcdiff",
  "+v8",

  # Most of net should not depend on icu, to keep size down when built as a
  # library.
  "-base/i18n",
  "-third_party/icu",
]

specific_include_rules = {
  # Within net, only used by file: requests.
  "directory_lister(\.cc|_unittest\.cc)": [
    "+base/i18n",
  ],

  # Within net, only used by file: requests.
  "filename_util_icu\.cc": [
    "+base/i18n/file_util_icu.h",
  ],

  # Functions largely not used by the rest of net.
  "net_util_icu\.cc": [
    "+base/i18n",
    "+third_party/icu",
  ],

  # Consolidated string functions that depend on icu.
  "net_string_util_icu\.cc": [
    "+base/i18n/i18n_constants.h",
    "+base/i18n/icu_string_conversions.h",
    "+third_party/icu/source/common/unicode/ucnv.h"
  ],

  "websocket_channel\.h": [
    "+base/i18n",
  ],

  "ftp_util\.cc": [
    "+base/i18n",
    "+third_party/icu",
  ],
  "ftp_directory_listing_parser\.cc": [
    "+base/i18n",
  ],
}

skip_child_includes = [
  "third_party",
  "tools/flip_server",
]
