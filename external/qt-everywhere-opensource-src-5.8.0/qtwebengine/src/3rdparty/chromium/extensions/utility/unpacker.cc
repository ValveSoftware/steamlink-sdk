// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/utility/unpacker.h"

#include <stddef.h>

#include <set>
#include <tuple>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/rtl.h"
#include "base/json/json_file_value_serializer.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "content/public/child/image_decoder_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_utility_messages.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ipc/ipc_message_utils.h"
#include "net/base/file_stream.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

namespace {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

// A limit to stop us passing dangerously large canvases to the browser.
const int kMaxImageCanvas = 4096 * 4096;

SkBitmap DecodeImage(const base::FilePath& path) {
  // Read the file from disk.
  std::string file_contents;
  if (!base::PathExists(path) ||
      !base::ReadFileToString(path, &file_contents)) {
    return SkBitmap();
  }

  // Decode the image using WebKit's image decoder.
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(file_contents.data());
  SkBitmap bitmap =
      content::DecodeImage(data, gfx::Size(), file_contents.length());
  if (bitmap.computeSize64() > kMaxImageCanvas)
    return SkBitmap();
  return bitmap;
}

bool PathContainsParentDirectory(const base::FilePath& path) {
  const base::FilePath::StringType kSeparators(base::FilePath::kSeparators);
  const base::FilePath::StringType kParentDirectory(
      base::FilePath::kParentDirectory);
  const size_t npos = base::FilePath::StringType::npos;
  const base::FilePath::StringType& value = path.value();

  for (size_t i = 0; i < value.length();) {
    i = value.find(kParentDirectory, i);
    if (i != npos) {
      if ((i == 0 || kSeparators.find(value[i - 1]) == npos) &&
          (i + 1 < value.length() || kSeparators.find(value[i + 1]) == npos)) {
        return true;
      }
      ++i;
    }
  }

  return false;
}

bool WritePickle(const IPC::Message& pickle, const base::FilePath& dest_path) {
  int size = base::checked_cast<int>(pickle.size());
  const char* data = static_cast<const char*>(pickle.data());
  int bytes_written = base::WriteFile(dest_path, data, size);
  return (bytes_written == size);
}

}  // namespace

struct Unpacker::InternalData {
  DecodedImages decoded_images;
};

Unpacker::Unpacker(const base::FilePath& working_dir,
                   const base::FilePath& extension_dir,
                   const std::string& extension_id,
                   Manifest::Location location,
                   int creation_flags)
    : working_dir_(working_dir),
      extension_dir_(extension_dir),
      extension_id_(extension_id),
      location_(location),
      creation_flags_(creation_flags) {
  internal_data_.reset(new InternalData());
}

Unpacker::~Unpacker() {
}

std::unique_ptr<base::DictionaryValue> Unpacker::ReadManifest() {
  base::FilePath manifest_path = extension_dir_.Append(kManifestFilename);
  if (!base::PathExists(manifest_path)) {
    SetError(errors::kInvalidManifest);
    return NULL;
  }

  JSONFileValueDeserializer deserializer(manifest_path);
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(NULL, &error);
  if (!root.get()) {
    SetError(error);
    return NULL;
  }

  if (!root->IsType(base::Value::TYPE_DICTIONARY)) {
    SetError(errors::kInvalidManifest);
    return NULL;
  }

  return base::DictionaryValue::From(std::move(root));
}

bool Unpacker::ReadAllMessageCatalogs(const std::string& default_locale) {
  base::FilePath locales_path = extension_dir_.Append(kLocaleFolder);

  // Not all folders under _locales have to be valid locales.
  base::FileEnumerator locales(locales_path, false,
                               base::FileEnumerator::DIRECTORIES);

  std::set<std::string> all_locales;
  extension_l10n_util::GetAllLocales(&all_locales);
  base::FilePath locale_path;
  while (!(locale_path = locales.Next()).empty()) {
    if (extension_l10n_util::ShouldSkipValidation(locales_path, locale_path,
                                                  all_locales))
      continue;

    base::FilePath messages_path = locale_path.Append(kMessagesFilename);

    if (!ReadMessageCatalog(messages_path))
      return false;
  }

  return true;
}

bool Unpacker::Run() {
  // Parse the manifest.
  parsed_manifest_ = ReadManifest();
  if (!parsed_manifest_.get())
    return false;  // Error was already reported.

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(extension_dir_, location_, *parsed_manifest_,
                        creation_flags_, extension_id_, &error));
  if (!extension.get()) {
    SetError(error);
    return false;
  }

  std::vector<InstallWarning> warnings;
  if (!file_util::ValidateExtension(extension.get(), &error, &warnings)) {
    SetError(error);
    return false;
  }
  extension->AddInstallWarnings(warnings);

  // Decode any images that the browser needs to display.
  std::set<base::FilePath> image_paths =
      ExtensionsClient::Get()->GetBrowserImagePaths(extension.get());
  for (const base::FilePath& path : image_paths) {
    if (!AddDecodedImage(path))
      return false;  // Error was already reported.
  }

  // Parse all message catalogs (if any).
  parsed_catalogs_.reset(new base::DictionaryValue);
  if (!LocaleInfo::GetDefaultLocale(extension.get()).empty()) {
    if (!ReadAllMessageCatalogs(LocaleInfo::GetDefaultLocale(extension.get())))
      return false;  // Error was already reported.
  }

  return DumpImagesToFile() && DumpMessageCatalogsToFile();
}

bool Unpacker::DumpImagesToFile() {
  IPC::Message pickle;  // We use a Message so we can use WriteParam.
  IPC::WriteParam(&pickle, internal_data_->decoded_images);

  base::FilePath path = working_dir_.AppendASCII(kDecodedImagesFilename);
  if (!WritePickle(pickle, path)) {
    SetError("Could not write image data to disk.");
    return false;
  }

  return true;
}

bool Unpacker::DumpMessageCatalogsToFile() {
  IPC::Message pickle;
  IPC::WriteParam(&pickle, *parsed_catalogs_.get());

  base::FilePath path =
      working_dir_.AppendASCII(kDecodedMessageCatalogsFilename);
  if (!WritePickle(pickle, path)) {
    SetError("Could not write message catalogs to disk.");
    return false;
  }

  return true;
}

bool Unpacker::AddDecodedImage(const base::FilePath& path) {
  // Make sure it's not referencing a file outside the extension's subdir.
  if (path.IsAbsolute() || PathContainsParentDirectory(path)) {
    SetUTF16Error(l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PACKAGE_IMAGE_PATH_ERROR,
        base::i18n::GetDisplayStringInLTRDirectionality(
            path.LossyDisplayName())));
    return false;
  }

  SkBitmap image_bitmap = DecodeImage(extension_dir_.Append(path));
  if (image_bitmap.isNull()) {
    SetUTF16Error(l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PACKAGE_IMAGE_ERROR,
        base::i18n::GetDisplayStringInLTRDirectionality(
            path.BaseName().LossyDisplayName())));
    return false;
  }

  internal_data_->decoded_images.push_back(std::make_tuple(image_bitmap, path));
  return true;
}

bool Unpacker::ReadMessageCatalog(const base::FilePath& message_path) {
  std::string error;
  JSONFileValueDeserializer deserializer(message_path);
  std::unique_ptr<base::DictionaryValue> root =
      base::DictionaryValue::From(deserializer.Deserialize(NULL, &error));
  if (!root.get()) {
    base::string16 messages_file = message_path.LossyDisplayName();
    if (error.empty()) {
      // If file is missing, Deserialize will fail with empty error.
      SetError(base::StringPrintf("%s %s", errors::kLocalesMessagesFileMissing,
                                  base::UTF16ToUTF8(messages_file).c_str()));
    } else {
      SetError(base::StringPrintf(
          "%s: %s", base::UTF16ToUTF8(messages_file).c_str(), error.c_str()));
    }
    return false;
  }

  base::FilePath relative_path;
  // message_path was created from temp_install_dir. This should never fail.
  if (!extension_dir_.AppendRelativePath(message_path, &relative_path)) {
    NOTREACHED();
    return false;
  }

  std::string dir_name = relative_path.DirName().MaybeAsASCII();
  if (dir_name.empty()) {
    NOTREACHED();
    return false;
  }
  parsed_catalogs_->Set(dir_name, root.release());

  return true;
}

void Unpacker::SetError(const std::string& error) {
  SetUTF16Error(base::UTF8ToUTF16(error));
}

void Unpacker::SetUTF16Error(const base::string16& error) {
  error_message_ = error;
}

}  // namespace extensions
