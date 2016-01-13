// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_reliable_client_stream.h"

#include "base/callback_helpers.h"
#include "net/base/net_errors.h"
#include "net/quic/quic_session.h"
#include "net/quic/quic_write_blocked_list.h"

namespace net {

QuicReliableClientStream::QuicReliableClientStream(QuicStreamId id,
                                                   QuicSession* session,
                                                   const BoundNetLog& net_log)
    : QuicDataStream(id, session),
      net_log_(net_log),
      delegate_(NULL) {
}

QuicReliableClientStream::~QuicReliableClientStream() {
  if (delegate_)
    delegate_->OnClose(connection_error());
}

uint32 QuicReliableClientStream::ProcessData(const char* data,
                                             uint32 data_len) {
  // TODO(rch): buffer data if we don't have a delegate.
  if (!delegate_)
    return ERR_ABORTED;

  int rv = delegate_->OnDataReceived(data, data_len);
  if (rv != OK) {
    DLOG(ERROR) << "Delegate refused data, rv: " << rv;
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
    return 0;
  }
  return data_len;
}

void QuicReliableClientStream::OnFinRead() {
  if (delegate_) {
    delegate_->OnClose(connection_error());
    delegate_ = NULL;
  }
  ReliableQuicStream::OnFinRead();
}

void QuicReliableClientStream::OnCanWrite() {
  ReliableQuicStream::OnCanWrite();

  if (!HasBufferedData() && !callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(OK);
  }
}

QuicPriority QuicReliableClientStream::EffectivePriority() const {
  if (delegate_ && delegate_->HasSendHeadersComplete()) {
    return QuicDataStream::EffectivePriority();
  }
  return QuicWriteBlockedList::kHighestPriority;
}

int QuicReliableClientStream::WriteStreamData(
    base::StringPiece data,
    bool fin,
    const CompletionCallback& callback) {
  // We should not have data buffered.
  DCHECK(!HasBufferedData());
  // Writes the data, or buffers it.
  WriteOrBufferData(data, fin, NULL);
  if (!HasBufferedData()) {
    return OK;
  }

  callback_ = callback;
  return ERR_IO_PENDING;
}

void QuicReliableClientStream::SetDelegate(
    QuicReliableClientStream::Delegate* delegate) {
  DCHECK((!delegate_ && delegate) || (delegate_ && !delegate));
  delegate_ = delegate;
}

void QuicReliableClientStream::OnError(int error) {
  if (delegate_) {
    QuicReliableClientStream::Delegate* delegate = delegate_;
    delegate_ = NULL;
    delegate->OnError(error);
  }
}

bool QuicReliableClientStream::CanWrite(const CompletionCallback& callback) {
  bool can_write =  session()->connection()->CanWrite(HAS_RETRANSMITTABLE_DATA);
  if (!can_write) {
    session()->MarkWriteBlocked(id(), EffectivePriority());
    DCHECK(callback_.is_null());
    callback_ = callback;
  }
  return can_write;
}

}  // namespace net
