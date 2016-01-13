// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_net_log_parameters.h"

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace content {

namespace {

static const char* download_type_names[] = {
  "NEW_DOWNLOAD",
  "HISTORY_IMPORT",
  "SAVE_PAGE_AS"
};
static const char* download_danger_names[] = {
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

COMPILE_ASSERT(ARRAYSIZE_UNSAFE(download_type_names) == SRC_SAVE_PAGE_AS + 1,
               download_type_enum_has_changed);
COMPILE_ASSERT(ARRAYSIZE_UNSAFE(download_danger_names) ==
                  DOWNLOAD_DANGER_TYPE_MAX,
               download_danger_enum_has_changed);

}  // namespace

base::Value* ItemActivatedNetLogCallback(
    const DownloadItem* download_item,
    DownloadType download_type,
    const std::string* file_name,
    net::NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("type", download_type_names[download_type]);
  dict->SetString("id", base::Int64ToString(download_item->GetId()));
  dict->SetString("original_url", download_item->GetOriginalUrl().spec());
  dict->SetString("final_url", download_item->GetURL().spec());
  dict->SetString("file_name", *file_name);
  dict->SetString("danger_type",
                  download_danger_names[download_item->GetDangerType()]);
  dict->SetString("start_offset",
                  base::Int64ToString(download_item->GetReceivedBytes()));
  dict->SetBoolean("has_user_gesture", download_item->HasUserGesture());

  return dict;
}

base::Value* ItemCheckedNetLogCallback(
    DownloadDangerType danger_type,
    net::NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("danger_type", download_danger_names[danger_type]);

  return dict;
}

base::Value* ItemRenamedNetLogCallback(const base::FilePath* old_filename,
                                       const base::FilePath* new_filename,
                                       net::NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("old_filename", old_filename->AsUTF8Unsafe());
  dict->SetString("new_filename", new_filename->AsUTF8Unsafe());

  return dict;
}

base::Value* ItemInterruptedNetLogCallback(DownloadInterruptReason reason,
                                           int64 bytes_so_far,
                                           const std::string* hash_state,
                                           net::NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("interrupt_reason", DownloadInterruptReasonToString(reason));
  dict->SetString("bytes_so_far", base::Int64ToString(bytes_so_far));
  dict->SetString("hash_state",
                  base::HexEncode(hash_state->data(), hash_state->size()));

  return dict;
}

base::Value* ItemResumingNetLogCallback(bool user_initiated,
                                        DownloadInterruptReason reason,
                                        int64 bytes_so_far,
                                        const std::string* hash_state,
                                        net::NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("user_initiated", user_initiated ? "true" : "false");
  dict->SetString("interrupt_reason", DownloadInterruptReasonToString(reason));
  dict->SetString("bytes_so_far", base::Int64ToString(bytes_so_far));
  dict->SetString("hash_state",
                  base::HexEncode(hash_state->data(), hash_state->size()));

  return dict;
}

base::Value* ItemCompletingNetLogCallback(int64 bytes_so_far,
                                          const std::string* final_hash,
                                          net::NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("bytes_so_far", base::Int64ToString(bytes_so_far));
  dict->SetString("final_hash",
                  base::HexEncode(final_hash->data(), final_hash->size()));

  return dict;
}

base::Value* ItemFinishedNetLogCallback(bool auto_opened,
                                        net::NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("auto_opened", auto_opened ? "yes" : "no");

  return dict;
}

base::Value* ItemCanceledNetLogCallback(int64 bytes_so_far,
                                        const std::string* hash_state,
                                        net::NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("bytes_so_far", base::Int64ToString(bytes_so_far));
  dict->SetString("hash_state",
                  base::HexEncode(hash_state->data(), hash_state->size()));

  return dict;
}

base::Value* FileOpenedNetLogCallback(const base::FilePath* file_name,
                                      int64 start_offset,
                                      net::NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("file_name", file_name->AsUTF8Unsafe());
  dict->SetString("start_offset", base::Int64ToString(start_offset));

  return dict;
}

base::Value* FileStreamDrainedNetLogCallback(size_t stream_size,
                                             size_t num_buffers,
                                             net::NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetInteger("stream_size", static_cast<int>(stream_size));
  dict->SetInteger("num_buffers", static_cast<int>(num_buffers));

  return dict;
}

base::Value* FileRenamedNetLogCallback(const base::FilePath* old_filename,
                                       const base::FilePath* new_filename,
                                       net::NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("old_filename", old_filename->AsUTF8Unsafe());
  dict->SetString("new_filename", new_filename->AsUTF8Unsafe());

  return dict;
}

base::Value* FileErrorNetLogCallback(const char* operation,
                                     net::Error net_error,
                                     net::NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("operation", operation);
  dict->SetInteger("net_error", net_error);

  return dict;
}

base::Value* FileInterruptedNetLogCallback(const char* operation,
                                           int os_error,
                                           DownloadInterruptReason reason,
                                           net::NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("operation", operation);
  if (os_error != 0)
    dict->SetInteger("os_error", os_error);
  dict->SetString("interrupt_reason", DownloadInterruptReasonToString(reason));

  return dict;
}

}  // namespace content
