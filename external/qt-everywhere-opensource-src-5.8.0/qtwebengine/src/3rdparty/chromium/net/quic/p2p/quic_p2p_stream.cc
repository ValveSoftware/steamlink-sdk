// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/p2p/quic_p2p_stream.h"

#include "base/callback_helpers.h"
#include "net/base/net_errors.h"
#include "net/quic/quic_session.h"
#include "net/quic/quic_write_blocked_list.h"

namespace net {

QuicP2PStream::QuicP2PStream(QuicStreamId id, QuicSession* session)
    : ReliableQuicStream(id, session) {}

QuicP2PStream::~QuicP2PStream() {}

void QuicP2PStream::OnDataAvailable() {
  DCHECK(delegate_);

  struct iovec iov;
  while (true) {
    if (sequencer()->GetReadableRegions(&iov, 1) != 1) {
      // No more data to read.
      break;
    }
    delegate_->OnDataReceived(reinterpret_cast<const char*>(iov.iov_base),
                              iov.iov_len);
    sequencer()->MarkConsumed(iov.iov_len);
  }
}

void QuicP2PStream::OnClose() {
  ReliableQuicStream::OnClose();

  if (delegate_) {
    Delegate* delegate = delegate_;
    delegate_ = nullptr;
    delegate->OnClose(connection_error());
  }
}

void QuicP2PStream::OnCanWrite() {
  ReliableQuicStream::OnCanWrite();

  if (!HasBufferedData() && !write_callback_.is_null()) {
    base::ResetAndReturn(&write_callback_).Run(last_write_size_);
  }
}

void QuicP2PStream::WriteHeader(base::StringPiece data) {
  WriteOrBufferData(data, false, nullptr);
}

int QuicP2PStream::Write(base::StringPiece data,
                         const CompletionCallback& callback) {
  DCHECK(write_callback_.is_null());

  // Writes the data, or buffers it.
  WriteOrBufferData(data, false, nullptr);
  if (!HasBufferedData()) {
    return data.size();
  }

  write_callback_ = callback;
  last_write_size_ = data.size();
  return ERR_IO_PENDING;
}

void QuicP2PStream::SetDelegate(QuicP2PStream::Delegate* delegate) {
  DCHECK(!(delegate_ && delegate));
  delegate_ = delegate;
}

}  // namespace net
