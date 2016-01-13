// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of buzz::AsyncSocket that uses Chrome sockets.

#ifndef JINGLE_GLUE_CHROME_ASYNC_SOCKET_H_
#define JINGLE_GLUE_CHROME_ASYNC_SOCKET_H_

#if !defined(FEATURE_ENABLE_SSL)
#error ChromeAsyncSocket expects FEATURE_ENABLE_SSL to be defined
#endif

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "third_party/libjingle/source/talk/xmpp/asyncsocket.h"

namespace net {
class IOBufferWithSize;
class StreamSocket;
}  // namespace net

namespace jingle_glue {

class ResolvingClientSocketFactory;

class ChromeAsyncSocket : public buzz::AsyncSocket {
 public:
  // Takes ownership of |resolving_client_socket_factory|.
  ChromeAsyncSocket(
      ResolvingClientSocketFactory* resolving_client_socket_factory,
      size_t read_buf_size,
      size_t write_buf_size);

  // Does not raise any signals.
  virtual ~ChromeAsyncSocket();

  // buzz::AsyncSocket implementation.

  // The current state (see buzz::AsyncSocket::State; all but
  // STATE_CLOSING is used).
  virtual State state() OVERRIDE;

  // The last generated error.  Errors are generated when the main
  // functions below return false or when SignalClosed is raised due
  // to an asynchronous error.
  virtual Error error() OVERRIDE;

  // GetError() (which is of type net::Error) != net::OK only when
  // error() == ERROR_WINSOCK.
  virtual int GetError() OVERRIDE;

  // Tries to connect to the given address.
  //
  // If state() is not STATE_CLOSED, sets error to ERROR_WRONGSTATE
  // and returns false.
  //
  // If |address| has an empty hostname or a zero port, sets error to
  // ERROR_DNS and returns false.  (We don't use the IP address even
  // if it's present, as DNS resolution is done by
  // |resolving_client_socket_factory_|.  But it's perfectly fine if
  // the hostname is a stringified IP address.)
  //
  // Otherwise, starts the connection process and returns true.
  // SignalConnected will be raised when the connection is successful;
  // otherwise, SignalClosed will be raised with a net error set.
  virtual bool Connect(const talk_base::SocketAddress& address) OVERRIDE;

  // Tries to read at most |len| bytes into |data|.
  //
  // If state() is not STATE_TLS_CONNECTING, STATE_OPEN, or
  // STATE_TLS_OPEN, sets error to ERROR_WRONGSTATE and returns false.
  //
  // Otherwise, fills in |len_read| with the number of bytes read and
  // returns true.  If this is called when state() is
  // STATE_TLS_CONNECTING, reads 0 bytes.  (We have to handle this
  // case because StartTls() is called during a slot connected to
  // SignalRead after parsing the final non-TLS reply from the server
  // [see XmppClient::Private::OnSocketRead()].)
  virtual bool Read(char* data, size_t len, size_t* len_read) OVERRIDE;

  // Queues up |len| bytes of |data| for writing.
  //
  // If state() is not STATE_TLS_CONNECTING, STATE_OPEN, or
  // STATE_TLS_OPEN, sets error to ERROR_WRONGSTATE and returns false.
  //
  // If the given data is too big for the internal write buffer, sets
  // error to ERROR_WINSOCK/net::ERR_INSUFFICIENT_RESOURCES and
  // returns false.
  //
  // Otherwise, queues up the data and returns true.  If this is
  // called when state() == STATE_TLS_CONNECTING, the data is will be
  // sent only after the TLS connection succeeds.  (See StartTls()
  // below for why this happens.)
  //
  // Note that there's no guarantee that the data will actually be
  // sent; however, it is guaranteed that the any data sent will be
  // sent in FIFO order.
  virtual bool Write(const char* data, size_t len) OVERRIDE;

  // If the socket is not already closed, closes the socket and raises
  // SignalClosed.  Always returns true.
  virtual bool Close() OVERRIDE;

  // Tries to change to a TLS connection with the given domain name.
  //
  // If state() is not STATE_OPEN or there are pending reads or
  // writes, sets error to ERROR_WRONGSTATE and returns false.  (In
  // practice, this means that StartTls() can only be called from a
  // slot connected to SignalRead.)
  //
  // Otherwise, starts the TLS connection process and returns true.
  // SignalSSLConnected will be raised when the connection is
  // successful; otherwise, SignalClosed will be raised with a net
  // error set.
  virtual bool StartTls(const std::string& domain_name) OVERRIDE;

  // Signal behavior:
  //
  // SignalConnected: raised whenever the connect initiated by a call
  // to Connect() is complete.
  //
  // SignalSSLConnected: raised whenever the connect initiated by a
  // call to StartTls() is complete.  Not actually used by
  // XmppClient. (It just assumes that if SignalRead is raised after a
  // call to StartTls(), the connection has been successfully
  // upgraded.)
  //
  // SignalClosed: raised whenever the socket is closed, either due to
  // an asynchronous error, the other side closing the connection, or
  // when Close() is called.
  //
  // SignalRead: raised whenever the next call to Read() will succeed
  // with a non-zero |len_read| (assuming nothing else happens in the
  // meantime).
  //
  // SignalError: not used.

 private:
  enum AsyncIOState {
    // An I/O op is not in progress.
    IDLE,
    // A function has been posted to do the I/O.
    POSTED,
    // An async I/O operation is pending.
    PENDING,
  };

  bool IsOpen() const;

  // Error functions.
  void DoNonNetError(Error error);
  void DoNetError(net::Error net_error);
  void DoNetErrorFromStatus(int status);

  // Connection functions.
  void ProcessConnectDone(int status);

  // Read loop functions.
  void PostDoRead();
  void DoRead();
  void ProcessReadDone(int status);

  // Write loop functions.
  void PostDoWrite();
  void DoWrite();
  void ProcessWriteDone(int status);

  // SSL/TLS connection functions.
  void ProcessSSLConnectDone(int status);

  // Close functions.
  void DoClose();

  scoped_ptr<ResolvingClientSocketFactory> resolving_client_socket_factory_;

  // buzz::AsyncSocket state.
  buzz::AsyncSocket::State state_;
  buzz::AsyncSocket::Error error_;
  net::Error net_error_;

  // NULL iff state() == STATE_CLOSED.
  scoped_ptr<net::StreamSocket> transport_socket_;

  // State for the read loop.  |read_start_| <= |read_end_| <=
  // |read_buf_->size()|.  There's a read in flight (i.e.,
  // |read_state_| != IDLE) iff |read_end_| == 0.
  AsyncIOState read_state_;
  scoped_refptr<net::IOBufferWithSize> read_buf_;
  size_t read_start_, read_end_;

  // State for the write loop.  |write_end_| <= |write_buf_->size()|.
  // There's a write in flight (i.e., |write_state_| != IDLE) iff
  // |write_end_| > 0.
  AsyncIOState write_state_;
  scoped_refptr<net::IOBufferWithSize> write_buf_;
  size_t write_end_;

  base::WeakPtrFactory<ChromeAsyncSocket> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAsyncSocket);
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_CHROME_ASYNC_SOCKET_H_
