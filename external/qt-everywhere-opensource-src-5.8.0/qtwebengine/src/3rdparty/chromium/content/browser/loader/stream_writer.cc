// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/stream_writer.h"

#include "base/guid.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_registry.h"
#include "content/public/browser/resource_controller.h"
#include "net/base/io_buffer.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

StreamWriter::StreamWriter() : controller_(nullptr), immediate_mode_(false) {
}

StreamWriter::~StreamWriter() {
  if (stream_.get())
    Finalize();
}

void StreamWriter::InitializeStream(StreamRegistry* registry,
                                    const GURL& origin) {
  DCHECK(!stream_.get());

  // TODO(tyoshino): Find a way to share this with the blob URL creation in
  // WebKit.
  GURL url(std::string(url::kBlobScheme) + ":" + origin.spec() +
           base::GenerateGUID());
  stream_ = new Stream(registry, this, url);
}

void StreamWriter::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                              int* buf_size,
                              int min_size) {
  static const int kReadBufSize = 32768;

  DCHECK(buf);
  DCHECK(buf_size);
  DCHECK_LE(min_size, kReadBufSize);
  if (!read_buffer_.get())
    read_buffer_ = new net::IOBuffer(kReadBufSize);
  *buf = read_buffer_.get();
  *buf_size = kReadBufSize;
}

void StreamWriter::OnReadCompleted(int bytes_read, bool* defer) {
  if (!bytes_read)
    return;

  // We have more data to read.
  DCHECK(read_buffer_.get());

  // Release the ownership of the buffer, and store a reference
  // to it. A new one will be allocated in OnWillRead().
  scoped_refptr<net::IOBuffer> buffer;
  read_buffer_.swap(buffer);
  stream_->AddData(buffer, bytes_read);

  if (immediate_mode_)
    stream_->Flush();

  if (!stream_->can_add_data())
    *defer = true;
}

void StreamWriter::Finalize() {
  DCHECK(stream_.get());
  stream_->Finalize();
  stream_->RemoveWriteObserver(this);
  stream_ = nullptr;
}

void StreamWriter::OnSpaceAvailable(Stream* stream) {
  controller_->Resume();
}

void StreamWriter::OnClose(Stream* stream) {
  controller_->Cancel();
}

}  // namespace content
