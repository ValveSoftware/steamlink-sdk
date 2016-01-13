// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/test_util.h"

#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "google_apis/drive/gdata_wapi_requests.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace google_apis {
namespace test_util {

bool RemovePrefix(const std::string& input,
                  const std::string& prefix,
                  std::string* output) {
  if (!StartsWithASCII(input, prefix, true /* case sensitive */))
    return false;

  *output = input.substr(prefix.size());
  return true;
}

base::FilePath GetTestFilePath(const std::string& relative_path) {
  base::FilePath path;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &path))
    return base::FilePath();
  path = path.AppendASCII("chrome")
             .AppendASCII("test")
             .AppendASCII("data")
             .Append(base::FilePath::FromUTF8Unsafe(relative_path));
  return path;
}

GURL GetBaseUrlForTesting(int port) {
  return GURL(base::StringPrintf("http://127.0.0.1:%d/", port));
}

void RunAndQuit(base::RunLoop* run_loop, const base::Closure& closure) {
  closure.Run();
  run_loop->Quit();
}

bool WriteStringToFile(const base::FilePath& file_path,
                       const std::string& content) {
  int result = base::WriteFile(
      file_path, content.data(), static_cast<int>(content.size()));
  return content.size() == static_cast<size_t>(result);
}

bool CreateFileOfSpecifiedSize(const base::FilePath& temp_dir,
                               size_t size,
                               base::FilePath* path,
                               std::string* data) {
  if (!base::CreateTemporaryFileInDir(temp_dir, path))
    return false;

  if (size == 0) {
    // Note: RandBytesAsString doesn't support generating an empty string.
    data->clear();
    return true;
  }

  *data = base::RandBytesAsString(size);
  return WriteStringToFile(*path, *data);
}

scoped_ptr<base::Value> LoadJSONFile(const std::string& relative_path) {
  base::FilePath path = GetTestFilePath(relative_path);

  std::string error;
  JSONFileValueSerializer serializer(path);
  scoped_ptr<base::Value> value(serializer.Deserialize(NULL, &error));
  LOG_IF(WARNING, !value.get()) << "Failed to parse " << path.value()
                                << ": " << error;
  return value.Pass();
}

// Returns a HttpResponse created from the given file path.
scoped_ptr<net::test_server::BasicHttpResponse> CreateHttpResponseFromFile(
    const base::FilePath& file_path) {
  std::string content;
  if (!base::ReadFileToString(file_path, &content))
    return scoped_ptr<net::test_server::BasicHttpResponse>();

  std::string content_type = "text/plain";
  if (EndsWith(file_path.AsUTF8Unsafe(), ".json", true /* case sensitive */))
    content_type = "application/json";

  scoped_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(content);
  http_response->set_content_type(content_type);
  return http_response.Pass();
}

scoped_ptr<net::test_server::HttpResponse> HandleDownloadFileRequest(
    const GURL& base_url,
    net::test_server::HttpRequest* out_request,
    const net::test_server::HttpRequest& request) {
  *out_request = request;

  GURL absolute_url = base_url.Resolve(request.relative_url);
  std::string remaining_path;
  if (!RemovePrefix(absolute_url.path(), "/files/", &remaining_path))
    return scoped_ptr<net::test_server::HttpResponse>();
  return CreateHttpResponseFromFile(
      GetTestFilePath(remaining_path)).PassAs<net::test_server::HttpResponse>();
}

bool ParseContentRangeHeader(const std::string& value,
                             int64* start_position,
                             int64* end_position,
                             int64* length) {
  DCHECK(start_position);
  DCHECK(end_position);
  DCHECK(length);

  std::string remaining;
  if (!RemovePrefix(value, "bytes ", &remaining))
    return false;

  std::vector<std::string> parts;
  base::SplitString(remaining, '/', &parts);
  if (parts.size() != 2U)
    return false;

  const std::string range = parts[0];
  if (!base::StringToInt64(parts[1], length))
    return false;

  parts.clear();
  base::SplitString(range, '-', &parts);
  if (parts.size() != 2U)
    return false;

  return (base::StringToInt64(parts[0], start_position) &&
          base::StringToInt64(parts[1], end_position));
}

void AppendProgressCallbackResult(std::vector<ProgressInfo>* progress_values,
                                  int64 progress,
                                  int64 total) {
  progress_values->push_back(ProgressInfo(progress, total));
}

TestGetContentCallback::TestGetContentCallback()
    : callback_(base::Bind(&TestGetContentCallback::OnGetContent,
                           base::Unretained(this))) {
}

TestGetContentCallback::~TestGetContentCallback() {
}

std::string TestGetContentCallback::GetConcatenatedData() const {
  std::string result;
  for (size_t i = 0; i < data_.size(); ++i) {
    result += *data_[i];
  }
  return result;
}

void TestGetContentCallback::OnGetContent(google_apis::GDataErrorCode error,
                                          scoped_ptr<std::string> data) {
  data_.push_back(data.release());
}

}  // namespace test_util
}  // namespace google_apis
