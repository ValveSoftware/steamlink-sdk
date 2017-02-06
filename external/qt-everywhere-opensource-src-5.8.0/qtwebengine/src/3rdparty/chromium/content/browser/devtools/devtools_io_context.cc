// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_io_context.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/third_party/icu/icu_utf.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace devtools {

namespace {
unsigned s_last_stream_handle = 0;
}

using Stream = DevToolsIOContext::Stream;

Stream::Stream()
    : base::RefCountedDeleteOnMessageLoop<Stream>(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)),
      handle_(base::UintToString(++s_last_stream_handle)),
      had_errors_(false),
      last_read_pos_(0) {}

Stream::~Stream() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
}

bool Stream::InitOnFileThreadIfNeeded() {
  if (had_errors_)
    return false;
  if (file_.IsValid())
    return true;
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path)) {
    LOG(ERROR) << "Failed to create temporary file";
    had_errors_ = true;
    return false;
  }
  const unsigned flags = base::File::FLAG_OPEN_TRUNCATED |
                         base::File::FLAG_WRITE | base::File::FLAG_READ |
                         base::File::FLAG_DELETE_ON_CLOSE;
  file_.Initialize(temp_path, flags);
  if (!file_.IsValid()) {
    LOG(ERROR) << "Failed to open temporary file: " << temp_path.value()
        << ", " << base::File::ErrorToString(file_.error_details());
    had_errors_ = true;
    DeleteFile(temp_path, false);
    return false;
  }
  return true;
}

void Stream::Read(off_t position, size_t max_size, ReadCallback callback) {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&Stream::ReadOnFileThread, this, position, max_size,
                 callback));
}

void Stream::Append(const scoped_refptr<base::RefCountedString>& data) {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&Stream::AppendOnFileThread, this, data));
}

void Stream::ReadOnFileThread(off_t position, size_t max_size,
                              ReadCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  Status status = StatusFailure;
  scoped_refptr<base::RefCountedString> data;

  if (file_.IsValid()) {
    std::string buffer;
    buffer.resize(max_size);
    if (position < 0)
      position = last_read_pos_;
    int size_got = file_.ReadNoBestEffort(position, &*buffer.begin(), max_size);
    if (size_got < 0) {
      LOG(ERROR) << "Failed to read temporary file";
      had_errors_ = true;
      file_.Close();
    } else {
      // Provided client has requested sufficient large block, make their
      // life easier by not truncating in the middle of a UTF-8 character.
      if (size_got > 6 && !CBU8_IS_SINGLE(buffer[size_got - 1])) {
        base::TruncateUTF8ToByteSize(buffer, size_got, &buffer);
        size_got = buffer.size();
      } else {
        buffer.resize(size_got);
      }
      data = base::RefCountedString::TakeString(&buffer);
      status = size_got ? StatusSuccess : StatusEOF;
      last_read_pos_ = position + size_got;
    }
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, data, status));
}

void Stream::AppendOnFileThread(
    const scoped_refptr<base::RefCountedString>& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (!InitOnFileThreadIfNeeded())
    return;
  const std::string& buffer = data->data();
  int size_written = file_.WriteAtCurrentPos(&*buffer.begin(), buffer.size());
  if (size_written != static_cast<int>(buffer.size())) {
    LOG(ERROR) << "Failed to write temporary file";
    had_errors_ = true;
    file_.Close();
  }
}

DevToolsIOContext::DevToolsIOContext() {}

DevToolsIOContext::~DevToolsIOContext() {}

scoped_refptr<Stream> DevToolsIOContext::CreateTempFileBackedStream() {
  scoped_refptr<Stream> result = new Stream();
  bool inserted =
      streams_.insert(std::make_pair(result->handle(), result)).second;
  DCHECK(inserted);
  return result;
}

scoped_refptr<Stream>
    DevToolsIOContext::GetByHandle(const std::string& handle) {
  StreamsMap::const_iterator it = streams_.find(handle);
  return it == streams_.end() ? scoped_refptr<Stream>() : it->second;
}

bool DevToolsIOContext::Close(const std::string& handle) {
  return streams_.erase(handle) == 1;
}

void DevToolsIOContext::DiscardAllStreams() {
  return streams_.clear();
}

}  // namespace devtools
}  // namespace content
