// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_SERVER_SOCKET_OPENSSL_H_
#define NET_SOCKET_SSL_SERVER_SOCKET_OPENSSL_H_

#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_log.h"
#include "net/socket/ssl_server_socket.h"
#include "net/ssl/ssl_config_service.h"

// Avoid including misc OpenSSL headers, i.e.:
// <openssl/bio.h>
typedef struct bio_st BIO;
// <openssl/ssl.h>
typedef struct ssl_st SSL;

namespace net {

class SSLInfo;

class SSLServerSocketOpenSSL : public SSLServerSocket {
 public:
  // See comments on CreateSSLServerSocket for details of how these
  // parameters are used.
  SSLServerSocketOpenSSL(scoped_ptr<StreamSocket> socket,
                         scoped_refptr<X509Certificate> certificate,
                         crypto::RSAPrivateKey* key,
                         const SSLConfig& ssl_config);
  virtual ~SSLServerSocketOpenSSL();

  // SSLServerSocket interface.
  virtual int Handshake(const CompletionCallback& callback) OVERRIDE;

  // SSLSocket interface.
  virtual int ExportKeyingMaterial(const base::StringPiece& label,
                                   bool has_context,
                                   const base::StringPiece& context,
                                   unsigned char* out,
                                   unsigned int outlen) OVERRIDE;
  virtual int GetTLSUniqueChannelBinding(std::string* out) OVERRIDE;

  // Socket interface (via StreamSocket).
  virtual int Read(IOBuffer* buf, int buf_len,
                   const CompletionCallback& callback) OVERRIDE;
  virtual int Write(IOBuffer* buf, int buf_len,
                    const CompletionCallback& callback) OVERRIDE;
  virtual int SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual int SetSendBufferSize(int32 size) OVERRIDE;

  // StreamSocket implementation.
  virtual int Connect(const CompletionCallback& callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE;
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE;
  virtual const BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual bool WasNpnNegotiated() const OVERRIDE;
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE;
  virtual bool GetSSLInfo(SSLInfo* ssl_info) OVERRIDE;

 private:
  enum State {
    STATE_NONE,
    STATE_HANDSHAKE,
  };

  void OnSendComplete(int result);
  void OnRecvComplete(int result);
  void OnHandshakeIOComplete(int result);

  int BufferSend();
  void BufferSendComplete(int result);
  void TransportWriteComplete(int result);
  int BufferRecv();
  void BufferRecvComplete(int result);
  int TransportReadComplete(int result);
  bool DoTransportIO();
  int DoPayloadRead();
  int DoPayloadWrite();

  int DoHandshakeLoop(int last_io_result);
  int DoReadLoop(int result);
  int DoWriteLoop(int result);
  int DoHandshake();
  void DoHandshakeCallback(int result);
  void DoReadCallback(int result);
  void DoWriteCallback(int result);

  int Init();

  // Members used to send and receive buffer.
  bool transport_send_busy_;
  bool transport_recv_busy_;
  bool transport_recv_eof_;

  scoped_refptr<DrainableIOBuffer> send_buffer_;
  scoped_refptr<IOBuffer> recv_buffer_;

  BoundNetLog net_log_;

  CompletionCallback user_handshake_callback_;
  CompletionCallback user_read_callback_;
  CompletionCallback user_write_callback_;

  // Used by Read function.
  scoped_refptr<IOBuffer> user_read_buf_;
  int user_read_buf_len_;

  // Used by Write function.
  scoped_refptr<IOBuffer> user_write_buf_;
  int user_write_buf_len_;

  // Used by TransportWriteComplete() and TransportReadComplete() to signify an
  // error writing to the transport socket. A value of OK indicates no error.
  int transport_write_error_;

  // OpenSSL stuff
  SSL* ssl_;
  BIO* transport_bio_;

  // StreamSocket for sending and receiving data.
  scoped_ptr<StreamSocket> transport_socket_;

  // Options for the SSL socket.
  SSLConfig ssl_config_;

  // Certificate for the server.
  scoped_refptr<X509Certificate> cert_;

  // Private key used by the server.
  scoped_ptr<crypto::RSAPrivateKey> key_;

  State next_handshake_state_;
  bool completed_handshake_;

  DISALLOW_COPY_AND_ASSIGN(SSLServerSocketOpenSSL);
};

}  // namespace net

#endif  // NET_SOCKET_SSL_SERVER_SOCKET_OPENSSL_H_
