// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/dump_cache/simple_cache_dumper.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "net/base/cache_type.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/tools/dump_cache/cache_dumper.h"

namespace net {

SimpleCacheDumper::SimpleCacheDumper(base::FilePath input_path,
                                     base::FilePath output_path)
    : state_(STATE_NONE),
      input_path_(input_path),
      output_path_(output_path),
      writer_(new DiskDumper(output_path)),
      cache_thread_(new base::Thread("CacheThead")),
      iter_(NULL),
      src_entry_(NULL),
      dst_entry_(NULL),
      io_callback_(base::Bind(&SimpleCacheDumper::OnIOComplete,
                              base::Unretained(this))),
      rv_(0) {
}

SimpleCacheDumper::~SimpleCacheDumper() {
}

int SimpleCacheDumper::Run() {
  base::MessageLoopForIO main_message_loop;

  LOG(INFO) << "Reading cache from: " << input_path_.value();
  LOG(INFO) << "Writing cache to: " << output_path_.value();

  if (!cache_thread_->StartWithOptions(
          base::Thread::Options(base::MessageLoop::TYPE_IO, 0))) {
    LOG(ERROR) << "Unable to start thread";
    return ERR_UNEXPECTED;
  }
  state_ = STATE_CREATE_CACHE;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING) {
    main_message_loop.Run();
    return rv_;
  }
  return rv;
}

int SimpleCacheDumper::DoLoop(int rv) {
  do {
    State state = state_;
    state_ = STATE_NONE;
    switch (state) {
      case STATE_CREATE_CACHE:
        CHECK_EQ(OK, rv);
        rv = DoCreateCache();
        break;
      case STATE_CREATE_CACHE_COMPLETE:
        rv = DoCreateCacheComplete(rv);
        break;
      case STATE_OPEN_ENTRY:
        CHECK_EQ(OK, rv);
        rv = DoOpenEntry();
        break;
      case STATE_OPEN_ENTRY_COMPLETE:
        rv = DoOpenEntryComplete(rv);
        break;
      case STATE_CREATE_ENTRY:
        CHECK_EQ(OK, rv);
        rv = DoCreateEntry();
        break;
      case STATE_CREATE_ENTRY_COMPLETE:
        rv = DoCreateEntryComplete(rv);
        break;
      case STATE_READ_HEADERS:
        CHECK_EQ(OK, rv);
        rv = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        rv = DoReadHeadersComplete(rv);
        break;
      case STATE_WRITE_HEADERS:
        CHECK_EQ(OK, rv);
        rv = DoWriteHeaders();
        break;
      case STATE_WRITE_HEADERS_COMPLETE:
        rv = DoWriteHeadersComplete(rv);
        break;
      case STATE_READ_BODY:
        CHECK_EQ(OK, rv);
        rv = DoReadBody();
        break;
      case STATE_READ_BODY_COMPLETE:
        rv = DoReadBodyComplete(rv);
        break;
      case STATE_WRITE_BODY:
        CHECK_EQ(OK, rv);
        rv = DoWriteBody();
        break;
      case STATE_WRITE_BODY_COMPLETE:
        rv = DoWriteBodyComplete(rv);
        break;
      default:
        NOTREACHED() << "state_: " << state_;
        break;
    }
  } while (state_ != STATE_NONE && rv != ERR_IO_PENDING);
  return rv;
}

int SimpleCacheDumper::DoCreateCache() {
  DCHECK(!cache_);
  state_ = STATE_CREATE_CACHE_COMPLETE;
  return disk_cache::CreateCacheBackend(
      DISK_CACHE,
      CACHE_BACKEND_DEFAULT,
      input_path_,
      0,
      false,
      cache_thread_->message_loop_proxy().get(),
      NULL,
      &cache_,
      io_callback_);
}

int SimpleCacheDumper::DoCreateCacheComplete(int rv) {
  if (rv < 0)
    return rv;

  reinterpret_cast<disk_cache::BackendImpl*>(cache_.get())->SetUpgradeMode();
  reinterpret_cast<disk_cache::BackendImpl*>(cache_.get())->SetFlags(
      disk_cache::kNoRandom);

  state_ = STATE_OPEN_ENTRY;
  return OK;
}

int SimpleCacheDumper::DoOpenEntry() {
  DCHECK(!dst_entry_);
  DCHECK(!src_entry_);
  state_ = STATE_OPEN_ENTRY_COMPLETE;
  return cache_->OpenNextEntry(&iter_, &src_entry_, io_callback_);
}

int SimpleCacheDumper::DoOpenEntryComplete(int rv) {
  // ERR_FAILED indicates iteration finished.
  if (rv == ERR_FAILED) {
    cache_->EndEnumeration(&iter_);
    return OK;
  }

  if (rv < 0)
    return rv;

  state_ = STATE_CREATE_ENTRY;
  return OK;
}

int SimpleCacheDumper::DoCreateEntry() {
  DCHECK(!dst_entry_);
  state_ = STATE_CREATE_ENTRY_COMPLETE;

  return writer_->CreateEntry(src_entry_->GetKey(), &dst_entry_,
                              io_callback_);
}

int SimpleCacheDumper::DoCreateEntryComplete(int rv) {
  if (rv < 0)
    return rv;

  state_ = STATE_READ_HEADERS;
  return OK;
}

int SimpleCacheDumper::DoReadHeaders() {
  state_ = STATE_READ_HEADERS_COMPLETE;
  int32 size = src_entry_->GetDataSize(0);
  buf_ = new IOBufferWithSize(size);
  return src_entry_->ReadData(0, 0, buf_.get(), size, io_callback_);
}

int SimpleCacheDumper::DoReadHeadersComplete(int rv) {
  if (rv < 0)
    return rv;

  state_ = STATE_WRITE_HEADERS;
  return OK;
}

int SimpleCacheDumper::DoWriteHeaders() {
  int rv = writer_->WriteEntry(
      dst_entry_, 0, 0, buf_.get(), buf_->size(), io_callback_);
  if (rv == 0)
    return ERR_FAILED;

  state_ = STATE_WRITE_HEADERS_COMPLETE;
  return OK;
}

int SimpleCacheDumper::DoWriteHeadersComplete(int rv) {
  if (rv < 0)
    return rv;

  state_ = STATE_READ_BODY;
  return OK;
}

int SimpleCacheDumper::DoReadBody() {
  state_ = STATE_READ_BODY_COMPLETE;
  int32 size = src_entry_->GetDataSize(1);
  // If the body is empty, we can neither read nor write it, so
  // just move to the next.
  if (size <= 0) {
    state_ = STATE_WRITE_BODY_COMPLETE;
    return OK;
  }
  buf_ = new IOBufferWithSize(size);
  return src_entry_->ReadData(1, 0, buf_.get(), size, io_callback_);
}

int SimpleCacheDumper::DoReadBodyComplete(int rv) {
  if (rv < 0)
    return rv;

  state_ = STATE_WRITE_BODY;
  return OK;
}

int SimpleCacheDumper::DoWriteBody() {
  int rv = writer_->WriteEntry(
      dst_entry_, 1, 0, buf_.get(), buf_->size(), io_callback_);
  if (rv == 0)
    return ERR_FAILED;

  state_ = STATE_WRITE_BODY_COMPLETE;
  return OK;
}

int SimpleCacheDumper::DoWriteBodyComplete(int rv) {
  if (rv < 0)
    return rv;

  src_entry_->Close();
  writer_->CloseEntry(dst_entry_, base::Time::Now(), base::Time::Now());
  src_entry_ = NULL;
  dst_entry_ = NULL;

  state_ = STATE_OPEN_ENTRY;
  return OK;
}

void SimpleCacheDumper::OnIOComplete(int rv) {
  rv = DoLoop(rv);

  if (rv != ERR_IO_PENDING) {
    rv_ = rv;
    cache_.reset();
    base::MessageLoop::current()->Quit();
  }
}

}  // namespace net
