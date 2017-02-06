// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_net_log_parameters.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace content {

namespace {

static const char* const download_type_names[] = {
  "NEW_DOWNLOAD",
  "HISTORY_IMPORT",
  "SAVE_PAGE_AS"
};
static const char* const download_danger_names[] = {
  "NOT_DANGEROUS",
  "DANGEROUS_FILE",
  "DANGEROUS_URL",
  "DANGEROUS_CONTENT",
  "MAYBE_DANGEROUS_CONTENT",
  "UNCOMMON_CONTENT",
  "USER_VALIDATED",
  "DANGEROUS_HOST",
  "POTENTIALLY_UNWANTED"
};

static_assert(arraysize(download_type_names) == SRC_SAVE_PAGE_AS + 1,
              "download type enum has changed");
static_assert(arraysize(download_danger_names) == DOWNLOAD_DANGER_TYPE_MAX,
              "download danger enum has changed");

}  // namespace

std::unique_ptr<base::Value> ItemActivatedNetLogCallback(
    const DownloadItem* download_item,
    DownloadType download_type,
    const std::string* file_name,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString("type", download_type_names[download_type]);
  dict->SetString("id", base::UintToString(download_item->GetId()));
  dict->SetString("original_url", download_item->GetOriginalUrl().spec());
  dict->SetString("final_url", download_item->GetURL().spec());
  dict->SetString("file_name", *file_name);
  dict->SetString("danger_type",
                  download_danger_names[download_item->GetDangerType()]);
  dict->SetString("start_offset",
                  base::Int64ToString(download_item->GetReceivedBytes()));
  dict->SetBoolean("has_user_gesture", download_item->HasUserGesture());

  return std::move(dict);
}

std::unique_ptr<base::Value> ItemCheckedNetLogCallback(
    DownloadDangerType danger_type,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString("danger_type", download_danger_names[danger_type]);

  return std::move(dict);
}

std::unique_ptr<base::Value> ItemRenamedNetLogCallback(
    const base::FilePath* old_filename,
    const base::FilePath* new_filename,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString("old_filename", old_filename->AsUTF8Unsafe());
  dict->SetString("new_filename", new_filename->AsUTF8Unsafe());

  return std::move(dict);
}

std::unique_ptr<base::Value> ItemInterruptedNetLogCallback(
    DownloadInterruptReason reason,
    int64_t bytes_so_far,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString("interrupt_reason", DownloadInterruptReasonToString(reason));
  dict->SetString("bytes_so_far", base::Int64ToString(bytes_so_far));

  return std::move(dict);
}

std::unique_ptr<base::Value> ItemResumingNetLogCallback(
    bool user_initiated,
    DownloadInterruptReason reason,
    int64_t bytes_so_far,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString("user_initiated", user_initiated ? "true" : "false");
  dict->SetString("interrupt_reason", DownloadInterruptReasonToString(reason));
  dict->SetString("bytes_so_far", base::Int64ToString(bytes_so_far));

  return std::move(dict);
}

std::unique_ptr<base::Value> ItemCompletingNetLogCallback(
    int64_t bytes_so_far,
    const std::string* final_hash,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString("bytes_so_far", base::Int64ToString(bytes_so_far));
  dict->SetString("final_hash",
                  base::HexEncode(final_hash->data(), final_hash->size()));

  return std::move(dict);
}

std::unique_ptr<base::Value> ItemFinishedNetLogCallback(
    bool auto_opened,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString("auto_opened", auto_opened ? "yes" : "no");

  return std::move(dict);
}

std::unique_ptr<base::Value> ItemCanceledNetLogCallback(
    int64_t bytes_so_far,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString("bytes_so_far", base::Int64ToString(bytes_so_far));

  return std::move(dict);
}

std::unique_ptr<base::Value> FileOpenedNetLogCallback(
    const base::FilePath* file_name,
    int64_t start_offset,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString("file_name", file_name->AsUTF8Unsafe());
  dict->SetString("start_offset", base::Int64ToString(start_offset));

  return std::move(dict);
}

std::unique_ptr<base::Value> FileStreamDrainedNetLogCallback(
    size_t stream_size,
    size_t num_buffers,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetInteger("stream_size", static_cast<int>(stream_size));
  dict->SetInteger("num_buffers", static_cast<int>(num_buffers));

  return std::move(dict);
}

std::unique_ptr<base::Value> FileRenamedNetLogCallback(
    const base::FilePath* old_filename,
    const base::FilePath* new_filename,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString("old_filename", old_filename->AsUTF8Unsafe());
  dict->SetString("new_filename", new_filename->AsUTF8Unsafe());

  return std::move(dict);
}

std::unique_ptr<base::Value> FileErrorNetLogCallback(
    const char* operation,
    net::Error net_error,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString("operation", operation);
  dict->SetInteger("net_error", net_error);

  return std::move(dict);
}

std::unique_ptr<base::Value> FileInterruptedNetLogCallback(
    const char* operation,
    int os_error,
    DownloadInterruptReason reason,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  dict->SetString("operation", operation);
  if (os_error != 0)
    dict->SetInteger("os_error", os_error);
  dict->SetString("interrupt_reason", DownloadInterruptReasonToString(reason));

  return std::move(dict);
}

}  // namespace content
