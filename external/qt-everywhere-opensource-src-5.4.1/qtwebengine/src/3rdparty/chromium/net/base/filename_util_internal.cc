// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/filename_util.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/escape.h"
#include "net/base/filename_util_internal.h"
#include "net/base/mime_util.h"
#include "net/base/net_string_util.h"
#include "net/http/http_content_disposition.h"
#include "url/gurl.h"

namespace net {

void SanitizeGeneratedFileName(base::FilePath::StringType* filename,
                               bool replace_trailing) {
  const base::FilePath::CharType kReplace[] = FILE_PATH_LITERAL("-");
  if (filename->empty())
    return;
  if (replace_trailing) {
    // Handle CreateFile() stripping trailing dots and spaces on filenames
    // http://support.microsoft.com/kb/115827
    size_t length = filename->size();
    size_t pos = filename->find_last_not_of(FILE_PATH_LITERAL(" ."));
    filename->resize((pos == std::string::npos) ? 0 : (pos + 1));
    base::TrimWhitespace(*filename, base::TRIM_TRAILING, filename);
    if (filename->empty())
      return;
    size_t trimmed = length - filename->size();
    if (trimmed)
      filename->insert(filename->end(), trimmed, kReplace[0]);
  }
  base::TrimString(*filename, FILE_PATH_LITERAL("."), filename);
  if (filename->empty())
    return;
  // Replace any path information by changing path separators.
  ReplaceSubstringsAfterOffset(filename, 0, FILE_PATH_LITERAL("/"), kReplace);
  ReplaceSubstringsAfterOffset(filename, 0, FILE_PATH_LITERAL("\\"), kReplace);
}

// Returns the filename determined from the last component of the path portion
// of the URL.  Returns an empty string if the URL doesn't have a path or is
// invalid. If the generated filename is not reliable,
// |should_overwrite_extension| will be set to true, in which case a better
// extension should be determined based on the content type.
std::string GetFileNameFromURL(const GURL& url,
                               const std::string& referrer_charset,
                               bool* should_overwrite_extension) {
  // about: and data: URLs don't have file names, but esp. data: URLs may
  // contain parts that look like ones (i.e., contain a slash).  Therefore we
  // don't attempt to divine a file name out of them.
  if (!url.is_valid() || url.SchemeIs("about") || url.SchemeIs("data"))
    return std::string();

  const std::string unescaped_url_filename = UnescapeURLComponent(
      url.ExtractFileName(),
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);

  // The URL's path should be escaped UTF-8, but may not be.
  std::string decoded_filename = unescaped_url_filename;
  if (!base::IsStringUTF8(decoded_filename)) {
    // TODO(jshin): this is probably not robust enough. To be sure, we need
    // encoding detection.
    base::string16 utf16_output;
    if (!referrer_charset.empty() &&
        net::ConvertToUTF16(
            unescaped_url_filename, referrer_charset.c_str(), &utf16_output)) {
      decoded_filename = base::UTF16ToUTF8(utf16_output);
    } else {
      decoded_filename =
          base::WideToUTF8(base::SysNativeMBToWide(unescaped_url_filename));
    }
  }
  // If the URL contains a (possibly empty) query, assume it is a generator, and
  // allow the determined extension to be overwritten.
  *should_overwrite_extension = !decoded_filename.empty() && url.has_query();

  return decoded_filename;
}

// Returns whether the specified extension is automatically integrated into the
// windows shell.
bool IsShellIntegratedExtension(const base::FilePath::StringType& extension) {
  base::FilePath::StringType extension_lower = StringToLowerASCII(extension);

  // http://msdn.microsoft.com/en-us/library/ms811694.aspx
  // Right-clicking on shortcuts can be magical.
  if ((extension_lower == FILE_PATH_LITERAL("local")) ||
      (extension_lower == FILE_PATH_LITERAL("lnk")))
    return true;

  // http://www.juniper.net/security/auto/vulnerabilities/vuln2612.html
  // Files become magical if they end in a CLSID, so block such extensions.
  if (!extension_lower.empty() &&
      (extension_lower[0] == FILE_PATH_LITERAL('{')) &&
      (extension_lower[extension_lower.length() - 1] == FILE_PATH_LITERAL('}')))
    return true;
  return false;
}

// Returns whether the specified file name is a reserved name on windows.
// This includes names like "com2.zip" (which correspond to devices) and
// desktop.ini and thumbs.db which have special meaning to the windows shell.
bool IsReservedName(const base::FilePath::StringType& filename) {
  // This list is taken from the MSDN article "Naming a file"
  // http://msdn2.microsoft.com/en-us/library/aa365247(VS.85).aspx
  // I also added clock$ because GetSaveFileName seems to consider it as a
  // reserved name too.
  static const char* const known_devices[] = {
      "con",  "prn",  "aux",  "nul",  "com1", "com2", "com3",  "com4",
      "com5", "com6", "com7", "com8", "com9", "lpt1", "lpt2",  "lpt3",
      "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9", "clock$"};
#if defined(OS_WIN)
  std::string filename_lower = StringToLowerASCII(base::WideToUTF8(filename));
#elif defined(OS_POSIX)
  std::string filename_lower = StringToLowerASCII(filename);
#endif

  for (size_t i = 0; i < arraysize(known_devices); ++i) {
    // Exact match.
    if (filename_lower == known_devices[i])
      return true;
    // Starts with "DEVICE.".
    if (filename_lower.find(std::string(known_devices[i]) + ".") == 0)
      return true;
  }

  static const char* const magic_names[] = {
    // These file names are used by the "Customize folder" feature of the shell.
    "desktop.ini",
    "thumbs.db",
  };

  for (size_t i = 0; i < arraysize(magic_names); ++i) {
    if (filename_lower == magic_names[i])
      return true;
  }

  return false;
}

// Examines the current extension in |file_name| and modifies it if necessary in
// order to ensure the filename is safe.  If |file_name| doesn't contain an
// extension or if |ignore_extension| is true, then a new extension will be
// constructed based on the |mime_type|.
//
// We're addressing two things here:
//
// 1) Usability.  If there is no reliable file extension, we want to guess a
//    reasonable file extension based on the content type.
//
// 2) Shell integration.  Some file extensions automatically integrate with the
//    shell.  We block these extensions to prevent a malicious web site from
//    integrating with the user's shell.
void EnsureSafeExtension(const std::string& mime_type,
                         bool ignore_extension,
                         base::FilePath* file_name) {
  // See if our file name already contains an extension.
  base::FilePath::StringType extension = file_name->Extension();
  if (!extension.empty())
    extension.erase(extension.begin());  // Erase preceding '.'.

  if ((ignore_extension || extension.empty()) && !mime_type.empty()) {
    base::FilePath::StringType preferred_mime_extension;
    std::vector<base::FilePath::StringType> all_mime_extensions;
    net::GetPreferredExtensionForMimeType(mime_type, &preferred_mime_extension);
    net::GetExtensionsForMimeType(mime_type, &all_mime_extensions);
    // If the existing extension is in the list of valid extensions for the
    // given type, use it. This avoids doing things like pointlessly renaming
    // "foo.jpg" to "foo.jpeg".
    if (std::find(all_mime_extensions.begin(),
                  all_mime_extensions.end(),
                  extension) != all_mime_extensions.end()) {
      // leave |extension| alone
    } else if (!preferred_mime_extension.empty()) {
      extension = preferred_mime_extension;
    }
  }

#if defined(OS_WIN)
  static const base::FilePath::CharType default_extension[] =
      FILE_PATH_LITERAL("download");

  // Rename shell-integrated extensions.
  // TODO(asanka): Consider stripping out the bad extension and replacing it
  // with the preferred extension for the MIME type if one is available.
  if (IsShellIntegratedExtension(extension))
    extension.assign(default_extension);
#endif

  *file_name = file_name->ReplaceExtension(extension);
}

bool FilePathToString16(const base::FilePath& path, base::string16* converted) {
#if defined(OS_WIN)
  return base::WideToUTF16(
      path.value().c_str(), path.value().size(), converted);
#elif defined(OS_POSIX)
  std::string component8 = path.AsUTF8Unsafe();
  return !component8.empty() &&
         base::UTF8ToUTF16(component8.c_str(), component8.size(), converted);
#endif
}

base::string16 GetSuggestedFilenameImpl(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& referrer_charset,
    const std::string& suggested_name,
    const std::string& mime_type,
    const std::string& default_name,
    ReplaceIllegalCharactersCallback replace_illegal_characters_callback) {
  // TODO: this function to be updated to match the httpbis recommendations.
  // Talk to abarth for the latest news.

  // We don't translate this fallback string, "download". If localization is
  // needed, the caller should provide localized fallback in |default_name|.
  static const base::FilePath::CharType kFinalFallbackName[] =
      FILE_PATH_LITERAL("download");
  std::string filename;  // In UTF-8
  bool overwrite_extension = false;

  // Try to extract a filename from content-disposition first.
  if (!content_disposition.empty()) {
    HttpContentDisposition header(content_disposition, referrer_charset);
    filename = header.filename();
  }

  // Then try to use the suggested name.
  if (filename.empty() && !suggested_name.empty())
    filename = suggested_name;

  // Now try extracting the filename from the URL.  GetFileNameFromURL() only
  // looks at the last component of the URL and doesn't return the hostname as a
  // failover.
  if (filename.empty())
    filename = GetFileNameFromURL(url, referrer_charset, &overwrite_extension);

  // Finally try the URL hostname, but only if there's no default specified in
  // |default_name|.  Some schemes (e.g.: file:, about:, data:) do not have a
  // host name.
  if (filename.empty() && default_name.empty() && url.is_valid() &&
      !url.host().empty()) {
    // TODO(jungshik) : Decode a 'punycoded' IDN hostname. (bug 1264451)
    filename = url.host();
  }

  bool replace_trailing = false;
  base::FilePath::StringType result_str, default_name_str;
#if defined(OS_WIN)
  replace_trailing = true;
  result_str = base::UTF8ToUTF16(filename);
  default_name_str = base::UTF8ToUTF16(default_name);
#else
  result_str = filename;
  default_name_str = default_name;
#endif
  SanitizeGeneratedFileName(&result_str, replace_trailing);
  if (result_str.find_last_not_of(FILE_PATH_LITERAL("-_")) ==
      base::FilePath::StringType::npos) {
    result_str = !default_name_str.empty()
                     ? default_name_str
                     : base::FilePath::StringType(kFinalFallbackName);
    overwrite_extension = false;
  }
  replace_illegal_characters_callback.Run(&result_str, '-');
  base::FilePath result(result_str);
  GenerateSafeFileName(mime_type, overwrite_extension, &result);

  base::string16 result16;
  if (!FilePathToString16(result, &result16)) {
    result = base::FilePath(default_name_str);
    if (!FilePathToString16(result, &result16)) {
      result = base::FilePath(kFinalFallbackName);
      FilePathToString16(result, &result16);
    }
  }
  return result16;
}

base::FilePath GenerateFileNameImpl(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& referrer_charset,
    const std::string& suggested_name,
    const std::string& mime_type,
    const std::string& default_file_name,
    ReplaceIllegalCharactersCallback replace_illegal_characters_callback) {
  base::string16 file_name =
      GetSuggestedFilenameImpl(url,
                               content_disposition,
                               referrer_charset,
                               suggested_name,
                               mime_type,
                               default_file_name,
                               replace_illegal_characters_callback);

#if defined(OS_WIN)
  base::FilePath generated_name(file_name);
#else
  base::FilePath generated_name(
      base::SysWideToNativeMB(base::UTF16ToWide(file_name)));
#endif

  DCHECK(!generated_name.empty());

  return generated_name;
}

}  // namespace net
