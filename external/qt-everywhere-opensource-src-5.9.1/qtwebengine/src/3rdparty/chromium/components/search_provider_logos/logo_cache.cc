// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/logo_cache.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace {

// The cached logo metadata is persisted as JSON using these keys.
const char kSourceUrlKey[] = "url";
const char kExpirationTimeKey[] = "expiration_time";
const char kCanShowAfterExpirationKey[] = "can_show_after_expiration";
const char kFingerprintKey[] = "fingerprint";
const char kOnClickURLKey[] = "on_click_url";
const char kAltTextKey[] = "alt_text";
const char kMimeTypeKey[] = "mime_type";
const char kNumBytesKey[] = "num_bytes";
const char kAnimatedUrlKey[] = "animated_url";

bool GetTimeValue(const base::DictionaryValue& dict,
                  const std::string& key,
                  base::Time* time) {
  std::string str;
  int64_t internal_time_value;
  if (dict.GetString(key, &str) &&
      base::StringToInt64(str, &internal_time_value)) {
    *time = base::Time::FromInternalValue(internal_time_value);
    return true;
  }
  return false;
}

void SetTimeValue(base::DictionaryValue& dict,
                  const std::string& key,
                  const base::Time& time) {
  int64_t internal_time_value = time.ToInternalValue();
  dict.SetString(key, base::Int64ToString(internal_time_value));
}

}  // namespace

namespace search_provider_logos {

LogoCache::LogoCache(const base::FilePath& cache_directory)
    : cache_directory_(cache_directory),
      metadata_is_valid_(false) {
  // The LogoCache can be constructed on any thread, as long as it's used
  // on a single thread after construction.
  thread_checker_.DetachFromThread();
}

LogoCache::~LogoCache() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void LogoCache::UpdateCachedLogoMetadata(const LogoMetadata& metadata) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(metadata_);
  DCHECK_EQ(metadata_->fingerprint, metadata.fingerprint);

  UpdateMetadata(base::MakeUnique<LogoMetadata>(metadata));
  WriteMetadata();
}

const LogoMetadata* LogoCache::GetCachedLogoMetadata() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ReadMetadataIfNeeded();
  return metadata_.get();
}

void LogoCache::SetCachedLogo(const EncodedLogo* logo) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<LogoMetadata> metadata;
  if (logo) {
    metadata.reset(new LogoMetadata(logo->metadata));
    logo_num_bytes_ = static_cast<int>(logo->encoded_image->size());
  }
  UpdateMetadata(std::move(metadata));
  WriteLogo(logo ? logo->encoded_image : NULL);
}

std::unique_ptr<EncodedLogo> LogoCache::GetCachedLogo() {
  DCHECK(thread_checker_.CalledOnValidThread());

  ReadMetadataIfNeeded();
  if (!metadata_)
    return nullptr;

  scoped_refptr<base::RefCountedString> encoded_image =
      new base::RefCountedString();
  if (!base::ReadFileToString(GetLogoPath(), &encoded_image->data())) {
    UpdateMetadata(nullptr);
    return nullptr;
  }

  if (encoded_image->size() != static_cast<size_t>(logo_num_bytes_)) {
    // Delete corrupt metadata and logo.
    DeleteLogoAndMetadata();
    UpdateMetadata(nullptr);
    return nullptr;
  }

  std::unique_ptr<EncodedLogo> logo(new EncodedLogo());
  logo->encoded_image = encoded_image;
  logo->metadata = *metadata_;
  return logo;
}

// static
std::unique_ptr<LogoMetadata> LogoCache::LogoMetadataFromString(
    const std::string& str,
    int* logo_num_bytes) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(str);
  base::DictionaryValue* dict;
  if (!value || !value->GetAsDictionary(&dict))
    return nullptr;

  std::unique_ptr<LogoMetadata> metadata(new LogoMetadata());
  if (!dict->GetString(kSourceUrlKey, &metadata->source_url) ||
      !dict->GetString(kFingerprintKey, &metadata->fingerprint) ||
      !dict->GetString(kOnClickURLKey, &metadata->on_click_url) ||
      !dict->GetString(kAltTextKey, &metadata->alt_text) ||
      !dict->GetString(kAnimatedUrlKey, &metadata->animated_url) ||
      !dict->GetString(kMimeTypeKey, &metadata->mime_type) ||
      !dict->GetBoolean(kCanShowAfterExpirationKey,
                        &metadata->can_show_after_expiration) ||
      !dict->GetInteger(kNumBytesKey, logo_num_bytes) ||
      !GetTimeValue(*dict, kExpirationTimeKey, &metadata->expiration_time)) {
    return nullptr;
  }

  return metadata;
}

// static
void LogoCache::LogoMetadataToString(const LogoMetadata& metadata,
                                     int num_bytes,
                                     std::string* str) {
  base::DictionaryValue dict;
  dict.SetString(kSourceUrlKey, metadata.source_url);
  dict.SetString(kFingerprintKey, metadata.fingerprint);
  dict.SetString(kOnClickURLKey, metadata.on_click_url);
  dict.SetString(kAltTextKey, metadata.alt_text);
  dict.SetString(kAnimatedUrlKey, metadata.animated_url);
  dict.SetString(kMimeTypeKey, metadata.mime_type);
  dict.SetBoolean(kCanShowAfterExpirationKey,
                  metadata.can_show_after_expiration);
  dict.SetInteger(kNumBytesKey, num_bytes);
  SetTimeValue(dict, kExpirationTimeKey, metadata.expiration_time);
  base::JSONWriter::Write(dict, str);
}

base::FilePath LogoCache::GetLogoPath() {
  return cache_directory_.Append(FILE_PATH_LITERAL("logo"));
}

base::FilePath LogoCache::GetMetadataPath() {
  return cache_directory_.Append(FILE_PATH_LITERAL("metadata"));
}

void LogoCache::UpdateMetadata(std::unique_ptr<LogoMetadata> metadata) {
  metadata_ = std::move(metadata);
  metadata_is_valid_ = true;
}

void LogoCache::ReadMetadataIfNeeded() {
  if (metadata_is_valid_)
    return;

  std::unique_ptr<LogoMetadata> metadata;
  base::FilePath metadata_path = GetMetadataPath();
  std::string str;
  if (base::ReadFileToString(metadata_path, &str)) {
    metadata = LogoMetadataFromString(str, &logo_num_bytes_);
    if (!metadata) {
      // Delete corrupt metadata and logo.
      DeleteLogoAndMetadata();
    }
  }

  UpdateMetadata(std::move(metadata));
}

void LogoCache::WriteMetadata() {
  if (!EnsureCacheDirectoryExists())
    return;

  std::string str;
  LogoMetadataToString(*metadata_, logo_num_bytes_, &str);
  base::WriteFile(GetMetadataPath(), str.data(), static_cast<int>(str.size()));
}

void LogoCache::WriteLogo(scoped_refptr<base::RefCountedMemory> encoded_image) {
  if (!EnsureCacheDirectoryExists())
    return;

  if (!metadata_ || !encoded_image.get()) {
    DeleteLogoAndMetadata();
    return;
  }

  // To minimize the chances of ending up in an undetectably broken state:
  // First, delete the metadata file, then update the logo file, then update the
  // metadata file.
  base::FilePath logo_path = GetLogoPath();
  base::FilePath metadata_path = GetMetadataPath();

  if (!base::DeleteFile(metadata_path, false))
    return;

  if (base::WriteFile(
          logo_path,
          encoded_image->front_as<char>(),
          static_cast<int>(encoded_image->size())) == -1) {
    base::DeleteFile(logo_path, false);
    return;
  }

  WriteMetadata();
}

void LogoCache::DeleteLogoAndMetadata() {
  base::DeleteFile(GetLogoPath(), false);
  base::DeleteFile(GetMetadataPath(), false);
}

bool LogoCache::EnsureCacheDirectoryExists() {
  if (base::DirectoryExists(cache_directory_))
    return true;
  return base::CreateDirectory(cache_directory_);
}

}  // namespace search_provider_logos
