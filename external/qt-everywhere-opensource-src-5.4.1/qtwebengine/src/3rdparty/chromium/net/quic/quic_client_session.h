// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A client specific QuicSession subclass.  This class owns the underlying
// QuicConnection and QuicConnectionHelper objects.  The connection stores
// a non-owning pointer to the helper so this session needs to ensure that
// the helper outlives the connection.

#ifndef NET_QUIC_QUIC_CLIENT_SESSION_H_
#define NET_QUIC_QUIC_CLIENT_SESSION_H_

#include <string>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_callback.h"
#include "net/proxy/proxy_server.h"
#include "net/quic/quic_client_session_base.h"
#include "net/quic/quic_connection_logger.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_reliable_client_stream.h"

namespace net {

class CertVerifyResult;
class DatagramClientSocket;
class QuicConnectionHelper;
class QuicCryptoClientStreamFactory;
class QuicDefaultPacketWriter;
class QuicServerId;
class QuicServerInfo;
class QuicStreamFactory;
class SSLInfo;

namespace test {
class QuicClientSessionPeer;
}  // namespace test

class NET_EXPORT_PRIVATE QuicClientSession : public QuicClientSessionBase {
 public:
  // An interface for observing events on a session.
  class NET_EXPORT_PRIVATE Observer {
   public:
    virtual ~Observer() {}
    virtual void OnCryptoHandshakeConfirmed() = 0;
    virtual void OnSessionClosed(int error) = 0;
  };

  // A helper class used to manage a request to create a stream.
  class NET_EXPORT_PRIVATE StreamRequest {
   public:
    StreamRequest();
    ~StreamRequest();

    // Starts a request to create a stream.  If OK is returned, then
    // |stream| will be updated with the newly created stream.  If
    // ERR_IO_PENDING is returned, then when the request is eventuallly
    // complete |callback| will be called.
    int StartRequest(const base::WeakPtr<QuicClientSession>& session,
                     QuicReliableClientStream** stream,
                     const CompletionCallback& callback);

    // Cancels any pending stream creation request. May be called
    // repeatedly.
    void CancelRequest();

   private:
    friend class QuicClientSession;

    // Called by |session_| for an asynchronous request when the stream
    // request has finished successfully.
    void OnRequestCompleteSuccess(QuicReliableClientStream* stream);

    // Called by |session_| for an asynchronous request when the stream
    // request has finished with an error. Also called with ERR_ABORTED
    // if |session_| is destroyed while the stream request is still pending.
    void OnRequestCompleteFailure(int rv);

    base::WeakPtr<QuicClientSession> session_;
    CompletionCallback callback_;
    QuicReliableClientStream** stream_;

    DISALLOW_COPY_AND_ASSIGN(StreamRequest);
  };

  // Constructs a new session which will own |connection| and |helper|, but
  // not |stream_factory|, which must outlive this session.
  // TODO(rch): decouple the factory from the session via a Delegate interface.
  QuicClientSession(QuicConnection* connection,
                    scoped_ptr<DatagramClientSocket> socket,
                    scoped_ptr<QuicDefaultPacketWriter> writer,
                    QuicStreamFactory* stream_factory,
                    QuicCryptoClientStreamFactory* crypto_client_stream_factory,
                    scoped_ptr<QuicServerInfo> server_info,
                    const QuicServerId& server_id,
                    const QuicConfig& config,
                    QuicCryptoClientConfig* crypto_config,
                    base::TaskRunner* task_runner,
                    NetLog* net_log);
  virtual ~QuicClientSession();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Attempts to create a new stream.  If the stream can be
  // created immediately, returns OK.  If the open stream limit
  // has been reached, returns ERR_IO_PENDING, and |request|
  // will be added to the stream requets queue and will
  // be completed asynchronously.
  // TODO(rch): remove |stream| from this and use setter on |request|
  // and fix in spdy too.
  int TryCreateStream(StreamRequest* request,
                      QuicReliableClientStream** stream);

  // Cancels the pending stream creation request.
  void CancelRequest(StreamRequest* request);

  // QuicSession methods:
  virtual void OnStreamFrames(
      const std::vector<QuicStreamFrame>& frames) OVERRIDE;
  virtual QuicReliableClientStream* CreateOutgoingDataStream() OVERRIDE;
  virtual QuicCryptoClientStream* GetCryptoStream() OVERRIDE;
  virtual void CloseStream(QuicStreamId stream_id) OVERRIDE;
  virtual void SendRstStream(QuicStreamId id,
                             QuicRstStreamErrorCode error,
                             QuicStreamOffset bytes_written) OVERRIDE;
  virtual void OnCryptoHandshakeEvent(CryptoHandshakeEvent event) OVERRIDE;
  virtual void OnCryptoHandshakeMessageSent(
      const CryptoHandshakeMessage& message) OVERRIDE;
  virtual void OnCryptoHandshakeMessageReceived(
      const CryptoHandshakeMessage& message) OVERRIDE;
  virtual bool GetSSLInfo(SSLInfo* ssl_info) const OVERRIDE;

  // QuicClientSessionBase methods:
  virtual void OnProofValid(
      const QuicCryptoClientConfig::CachedState& cached) OVERRIDE;
  virtual void OnProofVerifyDetailsAvailable(
      const ProofVerifyDetails& verify_details) OVERRIDE;

  // QuicConnectionVisitorInterface methods:
  virtual void OnConnectionClosed(QuicErrorCode error, bool from_peer) OVERRIDE;
  virtual void OnSuccessfulVersionNegotiation(
      const QuicVersion& version) OVERRIDE;

  // Performs a crypto handshake with the server.
  int CryptoConnect(bool require_confirmation,
                    const CompletionCallback& callback);

  // Resumes a crypto handshake with the server after a timeout.
  int ResumeCryptoConnect(const CompletionCallback& callback);

  // Causes the QuicConnectionHelper to start reading from the socket
  // and passing the data along to the QuicConnection.
  void StartReading();

  // Close the session because of |error| and notifies the factory
  // that this session has been closed, which will delete the session.
  void CloseSessionOnError(int error);

  base::Value* GetInfoAsValue(const std::set<HostPortPair>& aliases);

  const BoundNetLog& net_log() const { return net_log_; }

  base::WeakPtr<QuicClientSession> GetWeakPtr();

  // Returns the number of client hello messages that have been sent on the
  // crypto stream. If the handshake has completed then this is one greater
  // than the number of round-trips needed for the handshake.
  int GetNumSentClientHellos() const;

  // Returns true if |hostname| may be pooled onto this session.  If this
  // is a secure QUIC session, then |hostname| must match the certificate
  // presented during the handshake.
  bool CanPool(const std::string& hostname) const;

 protected:
  // QuicSession methods:
  virtual QuicDataStream* CreateIncomingDataStream(QuicStreamId id) OVERRIDE;

 private:
  friend class test::QuicClientSessionPeer;

  typedef std::set<Observer*> ObserverSet;
  typedef std::list<StreamRequest*> StreamRequestQueue;

  QuicReliableClientStream* CreateOutgoingReliableStreamImpl();
  // A completion callback invoked when a read completes.
  void OnReadComplete(int result);

  void OnClosedStream();

  // A Session may be closed via any of three methods:
  // OnConnectionClosed - called by the connection when the connection has been
  //     closed, perhaps due to a timeout or a protocol error.
  // CloseSessionOnError - called from the owner of the session,
  //     the QuicStreamFactory, when there is an error.
  // OnReadComplete - when there is a read error.
  // This method closes all stream and performs any necessary cleanup.
  void CloseSessionOnErrorInner(int net_error, QuicErrorCode quic_error);

  void CloseAllStreams(int net_error);
  void CloseAllObservers(int net_error);

  // Notifies the factory that this session is going away and no more streams
  // should be created from it.  This needs to be called before closing any
  // streams, because closing a stream may cause a new stream to be created.
  void NotifyFactoryOfSessionGoingAway();

  // Posts a task to notify the factory that this session has been closed.
  void NotifyFactoryOfSessionClosedLater();

  // Notifies the factory that this session has been closed which will
  // delete |this|.
  void NotifyFactoryOfSessionClosed();

  void OnConnectTimeout();

  bool require_confirmation_;
  scoped_ptr<QuicCryptoClientStream> crypto_stream_;
  QuicStreamFactory* stream_factory_;
  scoped_ptr<DatagramClientSocket> socket_;
  scoped_ptr<QuicDefaultPacketWriter> writer_;
  scoped_refptr<IOBufferWithSize> read_buffer_;
  scoped_ptr<QuicServerInfo> server_info_;
  scoped_ptr<CertVerifyResult> cert_verify_result_;
  std::string pinning_failure_log_;
  ObserverSet observers_;
  StreamRequestQueue stream_requests_;
  bool read_pending_;
  CompletionCallback callback_;
  size_t num_total_streams_;
  base::TaskRunner* task_runner_;
  BoundNetLog net_log_;
  base::TimeTicks handshake_start_;  // Time the handshake was started.
  QuicConnectionLogger logger_;
  // Number of packets read in the current read loop.
  size_t num_packets_read_;
  // True when the session is going away, and streams may no longer be created
  // on this session. Existing stream will continue to be processed.
  bool going_away_;
  base::WeakPtrFactory<QuicClientSession> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuicClientSession);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CLIENT_SESSION_H_
