// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_stats.h"

#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_util.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "net/http/http_content_disposition.h"

namespace content {

namespace {

// All possible error codes from the network module. Note that the error codes
// are all positive (since histograms expect positive sample values).
const int kAllInterruptReasonCodes[] = {
#define INTERRUPT_REASON(label, value) (value),
#include "content/public/browser/download_interrupt_reason_values.h"
#undef INTERRUPT_REASON
};

// These values are based on net::HttpContentDisposition::ParseResult values.
// Values other than HEADER_PRESENT and IS_VALID are only measured if |IS_VALID|
// is true.
enum ContentDispositionCountTypes {
  // Count of downloads which had a Content-Disposition headers. The total
  // number of downloads is measured by UNTHROTTLED_COUNT.
  CONTENT_DISPOSITION_HEADER_PRESENT = 0,

  // Either 'filename' or 'filename*' attributes were valid and
  // yielded a non-empty filename.
  CONTENT_DISPOSITION_IS_VALID,

  // The following enum values correspond to
  // net::HttpContentDisposition::ParseResult.
  CONTENT_DISPOSITION_HAS_DISPOSITION_TYPE,
  CONTENT_DISPOSITION_HAS_UNKNOWN_TYPE,

  CONTENT_DISPOSITION_HAS_NAME,  // Obsolete; kept for UMA compatiblity.

  CONTENT_DISPOSITION_HAS_FILENAME,
  CONTENT_DISPOSITION_HAS_EXT_FILENAME,
  CONTENT_DISPOSITION_HAS_NON_ASCII_STRINGS,
  CONTENT_DISPOSITION_HAS_PERCENT_ENCODED_STRINGS,
  CONTENT_DISPOSITION_HAS_RFC2047_ENCODED_STRINGS,

  CONTENT_DISPOSITION_HAS_NAME_ONLY,  // Obsolete; kept for UMA compatiblity.

  CONTENT_DISPOSITION_LAST_ENTRY
};

void RecordContentDispositionCount(ContentDispositionCountTypes type,
                                   bool record) {
  if (!record)
    return;
  UMA_HISTOGRAM_ENUMERATION(
      "Download.ContentDisposition", type, CONTENT_DISPOSITION_LAST_ENTRY);
}

void RecordContentDispositionCountFlag(
    ContentDispositionCountTypes type,
    int flags_to_test,
    net::HttpContentDisposition::ParseResultFlags flag) {
  RecordContentDispositionCount(type, (flags_to_test & flag) == flag);
}

// Do not insert, delete, or reorder; this is being histogrammed. Append only.
// All of the download_file_types.asciipb entries should be in this list.
// TODO(asanka): Replace this enum with calls to FileTypePolicies and move the
// UMA metrics for dangerous/malicious downloads to //chrome/browser/download.
const base::FilePath::CharType* kDangerousFileTypes[] = {
  FILE_PATH_LITERAL(".ad"),
  FILE_PATH_LITERAL(".ade"),
  FILE_PATH_LITERAL(".adp"),
  FILE_PATH_LITERAL(".ah"),
  FILE_PATH_LITERAL(".apk"),
  FILE_PATH_LITERAL(".app"),
  FILE_PATH_LITERAL(".application"),
  FILE_PATH_LITERAL(".asp"),
  FILE_PATH_LITERAL(".asx"),
  FILE_PATH_LITERAL(".bas"),
  FILE_PATH_LITERAL(".bash"),
  FILE_PATH_LITERAL(".bat"),
  FILE_PATH_LITERAL(".cfg"),
  FILE_PATH_LITERAL(".chi"),
  FILE_PATH_LITERAL(".chm"),
  FILE_PATH_LITERAL(".class"),
  FILE_PATH_LITERAL(".cmd"),
  FILE_PATH_LITERAL(".com"),
  FILE_PATH_LITERAL(".command"),
  FILE_PATH_LITERAL(".crt"),
  FILE_PATH_LITERAL(".crx"),
  FILE_PATH_LITERAL(".csh"),
  FILE_PATH_LITERAL(".deb"),
  FILE_PATH_LITERAL(".dex"),
  FILE_PATH_LITERAL(".dll"),
  FILE_PATH_LITERAL(".drv"),
  FILE_PATH_LITERAL(".exe"),
  FILE_PATH_LITERAL(".fxp"),
  FILE_PATH_LITERAL(".grp"),
  FILE_PATH_LITERAL(".hlp"),
  FILE_PATH_LITERAL(".hta"),
  FILE_PATH_LITERAL(".htm"),
  FILE_PATH_LITERAL(".html"),
  FILE_PATH_LITERAL(".htt"),
  FILE_PATH_LITERAL(".inf"),
  FILE_PATH_LITERAL(".ini"),
  FILE_PATH_LITERAL(".ins"),
  FILE_PATH_LITERAL(".isp"),
  FILE_PATH_LITERAL(".jar"),
  FILE_PATH_LITERAL(".jnlp"),
  FILE_PATH_LITERAL(".user.js"),
  FILE_PATH_LITERAL(".js"),
  FILE_PATH_LITERAL(".jse"),
  FILE_PATH_LITERAL(".ksh"),
  FILE_PATH_LITERAL(".lnk"),
  FILE_PATH_LITERAL(".local"),
  FILE_PATH_LITERAL(".mad"),
  FILE_PATH_LITERAL(".maf"),
  FILE_PATH_LITERAL(".mag"),
  FILE_PATH_LITERAL(".mam"),
  FILE_PATH_LITERAL(".manifest"),
  FILE_PATH_LITERAL(".maq"),
  FILE_PATH_LITERAL(".mar"),
  FILE_PATH_LITERAL(".mas"),
  FILE_PATH_LITERAL(".mat"),
  FILE_PATH_LITERAL(".mau"),
  FILE_PATH_LITERAL(".mav"),
  FILE_PATH_LITERAL(".maw"),
  FILE_PATH_LITERAL(".mda"),
  FILE_PATH_LITERAL(".mdb"),
  FILE_PATH_LITERAL(".mde"),
  FILE_PATH_LITERAL(".mdt"),
  FILE_PATH_LITERAL(".mdw"),
  FILE_PATH_LITERAL(".mdz"),
  FILE_PATH_LITERAL(".mht"),
  FILE_PATH_LITERAL(".mhtml"),
  FILE_PATH_LITERAL(".mmc"),
  FILE_PATH_LITERAL(".mof"),
  FILE_PATH_LITERAL(".msc"),
  FILE_PATH_LITERAL(".msh"),
  FILE_PATH_LITERAL(".mshxml"),
  FILE_PATH_LITERAL(".msi"),
  FILE_PATH_LITERAL(".msp"),
  FILE_PATH_LITERAL(".mst"),
  FILE_PATH_LITERAL(".ocx"),
  FILE_PATH_LITERAL(".ops"),
  FILE_PATH_LITERAL(".pcd"),
  FILE_PATH_LITERAL(".pif"),
  FILE_PATH_LITERAL(".pkg"),
  FILE_PATH_LITERAL(".pl"),
  FILE_PATH_LITERAL(".plg"),
  FILE_PATH_LITERAL(".prf"),
  FILE_PATH_LITERAL(".prg"),
  FILE_PATH_LITERAL(".pst"),
  FILE_PATH_LITERAL(".py"),
  FILE_PATH_LITERAL(".pyc"),
  FILE_PATH_LITERAL(".pyw"),
  FILE_PATH_LITERAL(".rb"),
  FILE_PATH_LITERAL(".reg"),
  FILE_PATH_LITERAL(".rpm"),
  FILE_PATH_LITERAL(".scf"),
  FILE_PATH_LITERAL(".scr"),
  FILE_PATH_LITERAL(".sct"),
  FILE_PATH_LITERAL(".sh"),
  FILE_PATH_LITERAL(".shar"),
  FILE_PATH_LITERAL(".shb"),
  FILE_PATH_LITERAL(".shs"),
  FILE_PATH_LITERAL(".shtm"),
  FILE_PATH_LITERAL(".shtml"),
  FILE_PATH_LITERAL(".spl"),
  FILE_PATH_LITERAL(".svg"),
  FILE_PATH_LITERAL(".swf"),
  FILE_PATH_LITERAL(".sys"),
  FILE_PATH_LITERAL(".tcsh"),
  FILE_PATH_LITERAL(".url"),
  FILE_PATH_LITERAL(".vb"),
  FILE_PATH_LITERAL(".vbe"),
  FILE_PATH_LITERAL(".vbs"),
  FILE_PATH_LITERAL(".vsd"),
  FILE_PATH_LITERAL(".vsmacros"),
  FILE_PATH_LITERAL(".vss"),
  FILE_PATH_LITERAL(".vst"),
  FILE_PATH_LITERAL(".vsw"),
  FILE_PATH_LITERAL(".ws"),
  FILE_PATH_LITERAL(".wsc"),
  FILE_PATH_LITERAL(".wsf"),
  FILE_PATH_LITERAL(".wsh"),
  FILE_PATH_LITERAL(".xbap"),
  FILE_PATH_LITERAL(".xht"),
  FILE_PATH_LITERAL(".xhtm"),
  FILE_PATH_LITERAL(".xhtml"),
  FILE_PATH_LITERAL(".xml"),
  FILE_PATH_LITERAL(".xsl"),
  FILE_PATH_LITERAL(".xslt"),
  FILE_PATH_LITERAL(".website"),
  FILE_PATH_LITERAL(".msh1"),
  FILE_PATH_LITERAL(".msh2"),
  FILE_PATH_LITERAL(".msh1xml"),
  FILE_PATH_LITERAL(".msh2xml"),
  FILE_PATH_LITERAL(".ps1"),
  FILE_PATH_LITERAL(".ps1xml"),
  FILE_PATH_LITERAL(".ps2"),
  FILE_PATH_LITERAL(".ps2xml"),
  FILE_PATH_LITERAL(".psc1"),
  FILE_PATH_LITERAL(".psc2"),
  FILE_PATH_LITERAL(".xnk"),
  FILE_PATH_LITERAL(".appref-ms"),
  FILE_PATH_LITERAL(".gadget"),
  FILE_PATH_LITERAL(".efi"),
  FILE_PATH_LITERAL(".fon"),
  FILE_PATH_LITERAL(".partial"),
  FILE_PATH_LITERAL(".svg"),
  FILE_PATH_LITERAL(".xml"),
  FILE_PATH_LITERAL(".xrm_ms"),
  FILE_PATH_LITERAL(".xsl"),
  FILE_PATH_LITERAL(".action"),
  FILE_PATH_LITERAL(".bin"),
  FILE_PATH_LITERAL(".inx"),
  FILE_PATH_LITERAL(".ipa"),
  FILE_PATH_LITERAL(".isu"),
  FILE_PATH_LITERAL(".job"),
  FILE_PATH_LITERAL(".out"),
  FILE_PATH_LITERAL(".pad"),
  FILE_PATH_LITERAL(".paf"),
  FILE_PATH_LITERAL(".rgs"),
  FILE_PATH_LITERAL(".u3p"),
  FILE_PATH_LITERAL(".vbscript"),
  FILE_PATH_LITERAL(".workflow"),
  FILE_PATH_LITERAL(".001"),
  FILE_PATH_LITERAL(".7z"),
  FILE_PATH_LITERAL(".ace"),
  FILE_PATH_LITERAL(".arc"),
  FILE_PATH_LITERAL(".arj"),
  FILE_PATH_LITERAL(".b64"),
  FILE_PATH_LITERAL(".balz"),
  FILE_PATH_LITERAL(".bhx"),
  FILE_PATH_LITERAL(".bz"),
  FILE_PATH_LITERAL(".bz2"),
  FILE_PATH_LITERAL(".bzip2"),
  FILE_PATH_LITERAL(".cab"),
  FILE_PATH_LITERAL(".cpio"),
  FILE_PATH_LITERAL(".fat"),
  FILE_PATH_LITERAL(".gz"),
  FILE_PATH_LITERAL(".gzip"),
  FILE_PATH_LITERAL(".hfs"),
  FILE_PATH_LITERAL(".hqx"),
  FILE_PATH_LITERAL(".iso"),
  FILE_PATH_LITERAL(".lha"),
  FILE_PATH_LITERAL(".lpaq1"),
  FILE_PATH_LITERAL(".lpaq5"),
  FILE_PATH_LITERAL(".lpaq8"),
  FILE_PATH_LITERAL(".lzh"),
  FILE_PATH_LITERAL(".lzma"),
  FILE_PATH_LITERAL(".mim"),
  FILE_PATH_LITERAL(".ntfs"),
  FILE_PATH_LITERAL(".paq8f"),
  FILE_PATH_LITERAL(".paq8jd"),
  FILE_PATH_LITERAL(".paq8l"),
  FILE_PATH_LITERAL(".paq8o"),
  FILE_PATH_LITERAL(".pea"),
  FILE_PATH_LITERAL(".quad"),
  FILE_PATH_LITERAL(".r00"),
  FILE_PATH_LITERAL(".r01"),
  FILE_PATH_LITERAL(".r02"),
  FILE_PATH_LITERAL(".r03"),
  FILE_PATH_LITERAL(".r04"),
  FILE_PATH_LITERAL(".r05"),
  FILE_PATH_LITERAL(".r06"),
  FILE_PATH_LITERAL(".r07"),
  FILE_PATH_LITERAL(".r08"),
  FILE_PATH_LITERAL(".r09"),
  FILE_PATH_LITERAL(".r10"),
  FILE_PATH_LITERAL(".r11"),
  FILE_PATH_LITERAL(".r12"),
  FILE_PATH_LITERAL(".r13"),
  FILE_PATH_LITERAL(".r14"),
  FILE_PATH_LITERAL(".r15"),
  FILE_PATH_LITERAL(".r16"),
  FILE_PATH_LITERAL(".r17"),
  FILE_PATH_LITERAL(".r18"),
  FILE_PATH_LITERAL(".r19"),
  FILE_PATH_LITERAL(".r20"),
  FILE_PATH_LITERAL(".r21"),
  FILE_PATH_LITERAL(".r22"),
  FILE_PATH_LITERAL(".r23"),
  FILE_PATH_LITERAL(".r24"),
  FILE_PATH_LITERAL(".r25"),
  FILE_PATH_LITERAL(".r26"),
  FILE_PATH_LITERAL(".r27"),
  FILE_PATH_LITERAL(".r28"),
  FILE_PATH_LITERAL(".r29"),
  FILE_PATH_LITERAL(".rar"),
  FILE_PATH_LITERAL(".squashfs"),
  FILE_PATH_LITERAL(".swm"),
  FILE_PATH_LITERAL(".tar"),
  FILE_PATH_LITERAL(".taz"),
  FILE_PATH_LITERAL(".tbz"),
  FILE_PATH_LITERAL(".tbz2"),
  FILE_PATH_LITERAL(".tgz"),
  FILE_PATH_LITERAL(".tpz"),
  FILE_PATH_LITERAL(".txz"),
  FILE_PATH_LITERAL(".tz"),
  FILE_PATH_LITERAL(".udf"),
  FILE_PATH_LITERAL(".uu"),
  FILE_PATH_LITERAL(".uue"),
  FILE_PATH_LITERAL(".vhd"),
  FILE_PATH_LITERAL(".vmdk"),
  FILE_PATH_LITERAL(".wim"),
  FILE_PATH_LITERAL(".wrc"),
  FILE_PATH_LITERAL(".xar"),
  FILE_PATH_LITERAL(".xxe"),
  FILE_PATH_LITERAL(".xz"),
  FILE_PATH_LITERAL(".z"),
  FILE_PATH_LITERAL(".zip"),
  FILE_PATH_LITERAL(".zipx"),
  FILE_PATH_LITERAL(".zpaq"),
  FILE_PATH_LITERAL(".cdr"),
  FILE_PATH_LITERAL(".dart"),
  FILE_PATH_LITERAL(".dc42"),
  FILE_PATH_LITERAL(".diskcopy42"),
  FILE_PATH_LITERAL(".dmg"),
  FILE_PATH_LITERAL(".dmgpart"),
  FILE_PATH_LITERAL(".dvdr"),
  FILE_PATH_LITERAL(".img"),
  FILE_PATH_LITERAL(".imgpart"),
  FILE_PATH_LITERAL(".ndif"),
  FILE_PATH_LITERAL(".smi"),
  FILE_PATH_LITERAL(".sparsebundle"),
  FILE_PATH_LITERAL(".sparseimage"),
  FILE_PATH_LITERAL(".toast"),
  FILE_PATH_LITERAL(".udif"),
};

// Maps extensions to their matching UMA histogram int value.
int GetDangerousFileType(const base::FilePath& file_path) {
  for (size_t i = 0; i < arraysize(kDangerousFileTypes); ++i) {
    if (file_path.MatchesExtension(kDangerousFileTypes[i]))
      return i + 1;
  }
  return 0;  // Unknown extension.
}

} // namespace

void RecordDownloadCount(DownloadCountTypes type) {
  UMA_HISTOGRAM_ENUMERATION(
      "Download.Counts", type, DOWNLOAD_COUNT_TYPES_LAST_ENTRY);
}

void RecordDownloadSource(DownloadSource source) {
  UMA_HISTOGRAM_ENUMERATION(
      "Download.Sources", source, DOWNLOAD_SOURCE_LAST_ENTRY);
}

void RecordDownloadCompleted(const base::TimeTicks& start,
                             int64_t download_len) {
  RecordDownloadCount(COMPLETED_COUNT);
  UMA_HISTOGRAM_LONG_TIMES("Download.Time", (base::TimeTicks::Now() - start));
  int64_t max = 1024 * 1024 * 1024;  // One Terabyte.
  download_len /= 1024;  // In Kilobytes
  UMA_HISTOGRAM_CUSTOM_COUNTS("Download.DownloadSize",
                              download_len,
                              1,
                              max,
                              256);
}

void RecordDownloadInterrupted(DownloadInterruptReason reason,
                               int64_t received,
                               int64_t total) {
  RecordDownloadCount(INTERRUPTED_COUNT);
  UMA_HISTOGRAM_CUSTOM_ENUMERATION(
      "Download.InterruptedReason",
      reason,
      base::CustomHistogram::ArrayToCustomRanges(
          kAllInterruptReasonCodes, arraysize(kAllInterruptReasonCodes)));

  // The maximum should be 2^kBuckets, to have the logarithmic bucket
  // boundaries fall on powers of 2.
  static const int kBuckets = 30;
  static const int64_t kMaxKb = 1 << kBuckets;  // One Terabyte, in Kilobytes.
  int64_t delta_bytes = total - received;
  bool unknown_size = total <= 0;
  int64_t received_kb = received / 1024;
  int64_t total_kb = total / 1024;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Download.InterruptedReceivedSizeK",
                              received_kb,
                              1,
                              kMaxKb,
                              kBuckets);
  if (!unknown_size) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Download.InterruptedTotalSizeK",
                                total_kb,
                                1,
                                kMaxKb,
                                kBuckets);
    if (delta_bytes == 0) {
      RecordDownloadCount(INTERRUPTED_AT_END_COUNT);
      UMA_HISTOGRAM_CUSTOM_ENUMERATION(
          "Download.InterruptedAtEndReason",
          reason,
          base::CustomHistogram::ArrayToCustomRanges(
              kAllInterruptReasonCodes,
              arraysize(kAllInterruptReasonCodes)));
    } else if (delta_bytes > 0) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Download.InterruptedOverrunBytes",
                                  delta_bytes,
                                  1,
                                  kMaxKb,
                                  kBuckets);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Download.InterruptedUnderrunBytes",
                                  -delta_bytes,
                                  1,
                                  kMaxKb,
                                  kBuckets);
    }
  }

  UMA_HISTOGRAM_BOOLEAN("Download.InterruptedUnknownSize", unknown_size);
}

void RecordMaliciousDownloadClassified(DownloadDangerType danger_type) {
  UMA_HISTOGRAM_ENUMERATION("Download.MaliciousDownloadClassified",
                            danger_type,
                            DOWNLOAD_DANGER_TYPE_MAX);
}

void RecordDangerousDownloadAccept(DownloadDangerType danger_type,
                                   const base::FilePath& file_path) {
  UMA_HISTOGRAM_ENUMERATION("Download.DangerousDownloadValidated",
                            danger_type,
                            DOWNLOAD_DANGER_TYPE_MAX);
  if (danger_type == DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "Download.DangerousFile.DangerousDownloadValidated",
        GetDangerousFileType(file_path));
  }
}

void RecordDangerousDownloadDiscard(DownloadDiscardReason reason,
                                    DownloadDangerType danger_type,
                                    const base::FilePath& file_path) {
  switch (reason) {
    case DOWNLOAD_DISCARD_DUE_TO_USER_ACTION:
      UMA_HISTOGRAM_ENUMERATION(
          "Download.UserDiscard", danger_type, DOWNLOAD_DANGER_TYPE_MAX);
      if (danger_type == DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE) {
        UMA_HISTOGRAM_SPARSE_SLOWLY("Download.DangerousFile.UserDiscard",
                                    GetDangerousFileType(file_path));
      }
      break;
    case DOWNLOAD_DISCARD_DUE_TO_SHUTDOWN:
      UMA_HISTOGRAM_ENUMERATION(
          "Download.Discard", danger_type, DOWNLOAD_DANGER_TYPE_MAX);
      if (danger_type == DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE) {
        UMA_HISTOGRAM_SPARSE_SLOWLY("Download.DangerousFile.Discard",
                                    GetDangerousFileType(file_path));
      }
      break;
    default:
      NOTREACHED();
  }
}

void RecordDownloadWriteSize(size_t data_len) {
  int max = 1024 * 1024;  // One Megabyte.
  UMA_HISTOGRAM_CUSTOM_COUNTS("Download.WriteSize", data_len, 1, max, 256);
}

void RecordDownloadWriteLoopCount(int count) {
  UMA_HISTOGRAM_ENUMERATION("Download.WriteLoopCount", count, 20);
}

void RecordAcceptsRanges(const std::string& accepts_ranges,
                         int64_t download_len,
                         bool has_strong_validator) {
  int64_t max = 1024 * 1024 * 1024;  // One Terabyte.
  download_len /= 1024;  // In Kilobytes
  static const int kBuckets = 50;

  if (base::LowerCaseEqualsASCII(accepts_ranges, "none")) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Download.AcceptRangesNone.KBytes",
                                download_len,
                                1,
                                max,
                                kBuckets);
  } else if (base::LowerCaseEqualsASCII(accepts_ranges, "bytes")) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Download.AcceptRangesBytes.KBytes",
                                download_len,
                                1,
                                max,
                                kBuckets);
    if (has_strong_validator)
      RecordDownloadCount(STRONG_VALIDATOR_AND_ACCEPTS_RANGES);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Download.AcceptRangesMissingOrInvalid.KBytes",
                                download_len,
                                1,
                                max,
                                kBuckets);
  }
}

namespace {

enum DownloadContent {
  DOWNLOAD_CONTENT_UNRECOGNIZED = 0,
  DOWNLOAD_CONTENT_TEXT = 1,
  DOWNLOAD_CONTENT_IMAGE = 2,
  DOWNLOAD_CONTENT_AUDIO = 3,
  DOWNLOAD_CONTENT_VIDEO = 4,
  DOWNLOAD_CONTENT_OCTET_STREAM = 5,
  DOWNLOAD_CONTENT_PDF = 6,
  DOWNLOAD_CONTENT_DOC = 7,
  DOWNLOAD_CONTENT_XLS = 8,
  DOWNLOAD_CONTENT_PPT = 9,
  DOWNLOAD_CONTENT_ARCHIVE = 10,
  DOWNLOAD_CONTENT_EXE = 11,
  DOWNLOAD_CONTENT_DMG = 12,
  DOWNLOAD_CONTENT_CRX = 13,
  DOWNLOAD_CONTENT_MAX = 14,
};

struct MimeTypeToDownloadContent {
  const char* mime_type;
  DownloadContent download_content;
};

static MimeTypeToDownloadContent kMapMimeTypeToDownloadContent[] = {
  {"application/octet-stream", DOWNLOAD_CONTENT_OCTET_STREAM},
  {"binary/octet-stream", DOWNLOAD_CONTENT_OCTET_STREAM},
  {"application/pdf", DOWNLOAD_CONTENT_PDF},
  {"application/msword", DOWNLOAD_CONTENT_DOC},
  {"application/vnd.ms-excel", DOWNLOAD_CONTENT_XLS},
  {"application/vns.ms-powerpoint", DOWNLOAD_CONTENT_PPT},
  {"application/zip", DOWNLOAD_CONTENT_ARCHIVE},
  {"application/x-gzip", DOWNLOAD_CONTENT_ARCHIVE},
  {"application/x-rar-compressed", DOWNLOAD_CONTENT_ARCHIVE},
  {"application/x-tar", DOWNLOAD_CONTENT_ARCHIVE},
  {"application/x-bzip", DOWNLOAD_CONTENT_ARCHIVE},
  {"application/x-exe", DOWNLOAD_CONTENT_EXE},
  {"application/x-apple-diskimage", DOWNLOAD_CONTENT_DMG},
  {"application/x-chrome-extension", DOWNLOAD_CONTENT_CRX},
};

enum DownloadImage {
  DOWNLOAD_IMAGE_UNRECOGNIZED = 0,
  DOWNLOAD_IMAGE_GIF = 1,
  DOWNLOAD_IMAGE_JPEG = 2,
  DOWNLOAD_IMAGE_PNG = 3,
  DOWNLOAD_IMAGE_TIFF = 4,
  DOWNLOAD_IMAGE_ICON = 5,
  DOWNLOAD_IMAGE_WEBP = 6,
  DOWNLOAD_IMAGE_MAX = 7,
};

struct MimeTypeToDownloadImage {
  const char* mime_type;
  DownloadImage download_image;
};

static MimeTypeToDownloadImage kMapMimeTypeToDownloadImage[] = {
  {"image/gif", DOWNLOAD_IMAGE_GIF},
  {"image/jpeg", DOWNLOAD_IMAGE_JPEG},
  {"image/png", DOWNLOAD_IMAGE_PNG},
  {"image/tiff", DOWNLOAD_IMAGE_TIFF},
  {"image/vnd.microsoft.icon", DOWNLOAD_IMAGE_ICON},
  {"image/webp", DOWNLOAD_IMAGE_WEBP},
};

void RecordDownloadImageType(const std::string& mime_type_string) {
  DownloadImage download_image = DOWNLOAD_IMAGE_UNRECOGNIZED;

  // Look up exact matches.
  for (size_t i = 0; i < arraysize(kMapMimeTypeToDownloadImage); ++i) {
    const MimeTypeToDownloadImage& entry = kMapMimeTypeToDownloadImage[i];
    if (mime_type_string == entry.mime_type) {
      download_image = entry.download_image;
      break;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Download.ContentImageType",
                            download_image,
                            DOWNLOAD_IMAGE_MAX);
}

}  // namespace

void RecordDownloadMimeType(const std::string& mime_type_string) {
  DownloadContent download_content = DOWNLOAD_CONTENT_UNRECOGNIZED;

  // Look up exact matches.
  for (size_t i = 0; i < arraysize(kMapMimeTypeToDownloadContent); ++i) {
    const MimeTypeToDownloadContent& entry = kMapMimeTypeToDownloadContent[i];
    if (mime_type_string == entry.mime_type) {
      download_content = entry.download_content;
      break;
    }
  }

  // Do partial matches.
  if (download_content == DOWNLOAD_CONTENT_UNRECOGNIZED) {
    if (base::StartsWith(mime_type_string, "text/",
                         base::CompareCase::SENSITIVE)) {
      download_content = DOWNLOAD_CONTENT_TEXT;
    } else if (base::StartsWith(mime_type_string, "image/",
                                base::CompareCase::SENSITIVE)) {
      download_content = DOWNLOAD_CONTENT_IMAGE;
      RecordDownloadImageType(mime_type_string);
    } else if (base::StartsWith(mime_type_string, "audio/",
                                base::CompareCase::SENSITIVE)) {
      download_content = DOWNLOAD_CONTENT_AUDIO;
    } else if (base::StartsWith(mime_type_string, "video/",
                                base::CompareCase::SENSITIVE)) {
      download_content = DOWNLOAD_CONTENT_VIDEO;
    }
  }

  // Record the value.
  UMA_HISTOGRAM_ENUMERATION("Download.ContentType",
                            download_content,
                            DOWNLOAD_CONTENT_MAX);
}

void RecordDownloadContentDisposition(
    const std::string& content_disposition_string) {
  if (content_disposition_string.empty())
    return;
  net::HttpContentDisposition content_disposition(content_disposition_string,
                                                  std::string());
  int result = content_disposition.parse_result_flags();

  bool is_valid = !content_disposition.filename().empty();
  RecordContentDispositionCount(CONTENT_DISPOSITION_HEADER_PRESENT, true);
  RecordContentDispositionCount(CONTENT_DISPOSITION_IS_VALID, is_valid);
  if (!is_valid)
    return;

  RecordContentDispositionCountFlag(
      CONTENT_DISPOSITION_HAS_DISPOSITION_TYPE, result,
      net::HttpContentDisposition::HAS_DISPOSITION_TYPE);
  RecordContentDispositionCountFlag(
      CONTENT_DISPOSITION_HAS_UNKNOWN_TYPE, result,
      net::HttpContentDisposition::HAS_UNKNOWN_DISPOSITION_TYPE);
  RecordContentDispositionCountFlag(
      CONTENT_DISPOSITION_HAS_FILENAME, result,
      net::HttpContentDisposition::HAS_FILENAME);
  RecordContentDispositionCountFlag(
      CONTENT_DISPOSITION_HAS_EXT_FILENAME, result,
      net::HttpContentDisposition::HAS_EXT_FILENAME);
  RecordContentDispositionCountFlag(
      CONTENT_DISPOSITION_HAS_NON_ASCII_STRINGS, result,
      net::HttpContentDisposition::HAS_NON_ASCII_STRINGS);
  RecordContentDispositionCountFlag(
      CONTENT_DISPOSITION_HAS_PERCENT_ENCODED_STRINGS, result,
      net::HttpContentDisposition::HAS_PERCENT_ENCODED_STRINGS);
  RecordContentDispositionCountFlag(
      CONTENT_DISPOSITION_HAS_RFC2047_ENCODED_STRINGS, result,
      net::HttpContentDisposition::HAS_RFC2047_ENCODED_STRINGS);
}

void RecordFileThreadReceiveBuffers(size_t num_buffers) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Download.FileThreadReceiveBuffers", num_buffers, 1,
      100, 100);
}

void RecordBandwidth(double actual_bandwidth, double potential_bandwidth) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Download.ActualBandwidth", actual_bandwidth, 1, 1000000000, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Download.PotentialBandwidth", potential_bandwidth, 1, 1000000000, 50);
  UMA_HISTOGRAM_PERCENTAGE(
      "Download.BandwidthUsed",
      (int) ((actual_bandwidth * 100)/ potential_bandwidth));
}

void RecordOpen(const base::Time& end, bool first) {
  if (!end.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Download.OpenTime", (base::Time::Now() - end));
    if (first) {
      UMA_HISTOGRAM_LONG_TIMES("Download.FirstOpenTime",
                              (base::Time::Now() - end));
    }
  }
}

void RecordClearAllSize(int size) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Download.ClearAllSize",
                              size,
                              0/*min*/,
                              (1 << 10)/*max*/,
                              32/*num_buckets*/);
}

void RecordOpensOutstanding(int size) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Download.OpensOutstanding",
                              size,
                              0/*min*/,
                              (1 << 10)/*max*/,
                              64/*num_buckets*/);
}

void RecordContiguousWriteTime(base::TimeDelta time_blocked) {
  UMA_HISTOGRAM_TIMES("Download.FileThreadBlockedTime", time_blocked);
}

// Record what percentage of the time we have the network flow controlled.
void RecordNetworkBlockage(base::TimeDelta resource_handler_lifetime,
                           base::TimeDelta resource_handler_blocked_time) {
  int percentage = 0;
  // Avoid division by zero errors.
  if (!resource_handler_blocked_time.is_zero()) {
    percentage =
        resource_handler_blocked_time * 100 / resource_handler_lifetime;
  }

  UMA_HISTOGRAM_COUNTS_100("Download.ResourceHandlerBlockedPercentage",
                           percentage);
}

void RecordFileBandwidth(size_t length,
                         base::TimeDelta disk_write_time,
                         base::TimeDelta elapsed_time) {
  size_t elapsed_time_ms = elapsed_time.InMilliseconds();
  if (0u == elapsed_time_ms)
    elapsed_time_ms = 1;
  size_t disk_write_time_ms = disk_write_time.InMilliseconds();
  if (0u == disk_write_time_ms)
    disk_write_time_ms = 1;

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Download.BandwidthOverallBytesPerSecond",
      (1000 * length / elapsed_time_ms), 1, 50000000, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Download.BandwidthDiskBytesPerSecond",
      (1000 * length / disk_write_time_ms), 1, 50000000, 50);
  UMA_HISTOGRAM_COUNTS_100("Download.DiskBandwidthUsedPercentage",
                           disk_write_time_ms * 100 / elapsed_time_ms);
}

void RecordDownloadFileRenameResultAfterRetry(
    base::TimeDelta time_since_first_failure,
    DownloadInterruptReason interrupt_reason) {
  if (interrupt_reason == DOWNLOAD_INTERRUPT_REASON_NONE) {
    UMA_HISTOGRAM_TIMES("Download.TimeToRenameSuccessAfterInitialFailure",
                        time_since_first_failure);
  } else {
    UMA_HISTOGRAM_TIMES("Download.TimeToRenameFailureAfterInitialFailure",
                        time_since_first_failure);
  }
}

void RecordSavePackageEvent(SavePackageEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Download.SavePackage",
                            event,
                            SAVE_PACKAGE_LAST_ENTRY);
}

void RecordOriginStateOnResumption(bool is_partial,
                                   int state) {
  if (is_partial)
    UMA_HISTOGRAM_ENUMERATION("Download.OriginStateOnPartialResumption", state,
                              ORIGIN_STATE_ON_RESUMPTION_MAX);
  else
    UMA_HISTOGRAM_ENUMERATION("Download.OriginStateOnFullResumption", state,
                              ORIGIN_STATE_ON_RESUMPTION_MAX);
}

}  // namespace content
