// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/test_data_util.h"

#include <stdint.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "media/base/decoder_buffer.h"

namespace media {

const base::FilePath::CharType kTestDataPath[] =
    FILE_PATH_LITERAL("media/test/data");

base::FilePath GetTestDataFilePath(const std::string& name) {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));
  return file_path.Append(GetTestDataPath()).AppendASCII(name);
}

base::FilePath GetTestDataPath() {
  return base::FilePath(kTestDataPath);
}

std::string GetURLQueryString(const base::StringPairs& query_params) {
  std::string query = "";
  base::StringPairs::const_iterator itr = query_params.begin();
  for (; itr != query_params.end(); ++itr) {
    if (itr != query_params.begin())
      query.append("&");
    query.append(itr->first + "=" + itr->second);
  }
  return query;
}

scoped_refptr<DecoderBuffer> ReadTestDataFile(const std::string& name) {
  base::FilePath file_path = GetTestDataFilePath(name);

  int64_t tmp = 0;
  CHECK(base::GetFileSize(file_path, &tmp))
      << "Failed to get file size for '" << name << "'";

  int file_size = base::checked_cast<int>(tmp);

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(file_size));
  CHECK_EQ(file_size,
           base::ReadFile(
               file_path, reinterpret_cast<char*>(buffer->writable_data()),
               file_size)) << "Failed to read '" << name << "'";

  return buffer;
}

}  // namespace media
