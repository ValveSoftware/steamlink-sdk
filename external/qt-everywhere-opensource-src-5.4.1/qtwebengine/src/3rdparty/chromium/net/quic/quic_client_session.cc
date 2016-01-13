// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_session.h"

#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/crypto/quic_server_info.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_default_packet_writer.h"
#include "net/quic/quic_server_id.h"
#include "net/quic/quic_stream_factory.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/udp/datagram_client_socket.h"

namespace net {

namespace {

// The length of time to wait for a 0-RTT handshake to complete
// before allowing the requests to possibly proceed over TCP.
const int k0RttHandshakeTimeoutMs = 300;

// Histograms for tracking down the crashes from http://crbug.com/354669
// Note: these values must be kept in sync with the corresponding values in:
// tools/metrics/histograms/histograms.xml
enum Location {
  DESTRUCTOR = 0,
  ADD_OBSERVER = 1,
  TRY_CREATE_STREAM = 2,
  CREATE_OUTGOING_RELIABLE_STREAM = 3,
  NOTIFY_FACTORY_OF_SESSION_CLOSED_LATER = 4,
  NOTIFY_FACTORY_OF_SESSION_CLOSED = 5,
  NUM_LOCATIONS = 6,
};

void RecordUnexpectedOpenStreams(Location location) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.UnexpectedOpenStreams", location,
                            NUM_LOCATIONS);
}

void RecordUnexpectedObservers(Location location) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.UnexpectedObservers", location,
                            NUM_LOCATIONS);
}

void RecordUnexpectedNotGoingAway(Location location) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.UnexpectedNotGoingAway", location,
                            NUM_LOCATIONS);
}

// Histogram for recording the different reasons that a QUIC session is unable
// to complete the handshake.
enum HandshakeFailureReason {
  HANDSHAKE_FAILURE_UNKNOWN = 0,
  HANDSHAKE_FAILURE_BLACK_HOLE = 1,
  HANDSHAKE_FAILURE_PUBLIC_RESET = 2,
  NUM_HANDSHAKE_FAILURE_REASONS = 3,
};

void RecordHandshakeFailureReason(HandshakeFailureReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "Net.QuicSession.ConnectionClose.HandshakeNotConfirmed.Reason",
      reason, NUM_HANDSHAKE_FAILURE_REASONS);
}

// Note: these values must be kept in sync with the corresponding values in:
// tools/metrics/histograms/histograms.xml
enum HandshakeState {
  STATE_STARTED = 0,
  STATE_ENCRYPTION_ESTABLISHED = 1,
  STATE_HANDSHAKE_CONFIRMED = 2,
  STATE_FAILED = 3,
  NUM_HANDSHAKE_STATES = 4
};

void RecordHandshakeState(HandshakeState state) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicHandshakeState", state,
                            NUM_HANDSHAKE_STATES);
}

}  // namespace

QuicClientSession::StreamRequest::StreamRequest() : stream_(NULL) {}

QuicClientSession::StreamRequest::~StreamRequest() {
  CancelRequest();
}

int QuicClientSession::StreamRequest::StartRequest(
    const base::WeakPtr<QuicClientSession>& session,
    QuicReliableClientStream** stream,
    const CompletionCallback& callback) {
  session_ = session;
  stream_ = stream;
  int rv = session_->TryCreateStream(this, stream_);
  if (rv == ERR_IO_PENDING) {
    callback_ = callback;
  }

  return rv;
}

void QuicClientSession::StreamRequest::CancelRequest() {
  if (session_)
    session_->CancelRequest(this);
  session_.reset();
  callback_.Reset();
}

void QuicClientSession::StreamRequest::OnRequestCompleteSuccess(
    QuicReliableClientStream* stream) {
  session_.reset();
  *stream_ = stream;
  ResetAndReturn(&callback_).Run(OK);
}

void QuicClientSession::StreamRequest::OnRequestCompleteFailure(int rv) {
  session_.reset();
  ResetAndReturn(&callback_).Run(rv);
}

QuicClientSession::QuicClientSession(
    QuicConnection* connection,
    scoped_ptr<DatagramClientSocket> socket,
    scoped_ptr<QuicDefaultPacketWriter> writer,
    QuicStreamFactory* stream_factory,
    QuicCryptoClientStreamFactory* crypto_client_stream_factory,
    scoped_ptr<QuicServerInfo> server_info,
    const QuicServerId& server_id,
    const QuicConfig& config,
    QuicCryptoClientConfig* crypto_config,
    base::TaskRunner* task_runner,
    NetLog* net_log)
    : QuicClientSessionBase(connection,
                            config),
      require_confirmation_(false),
      stream_factory_(stream_factory),
      socket_(socket.Pass()),
      writer_(writer.Pass()),
      read_buffer_(new IOBufferWithSize(kMaxPacketSize)),
      server_info_(server_info.Pass()),
      read_pending_(false),
      num_total_streams_(0),
      task_runner_(task_runner),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_QUIC_SESSION)),
      logger_(net_log_),
      num_packets_read_(0),
      going_away_(false),
      weak_factory_(this) {
  crypto_stream_.reset(
      crypto_client_stream_factory ?
          crypto_client_stream_factory->CreateQuicCryptoClientStream(
              server_id, this, crypto_config) :
          new QuicCryptoClientStream(server_id, this,
                                     new ProofVerifyContextChromium(net_log_),
                                     crypto_config));

  connection->set_debug_visitor(&logger_);
  // TODO(rch): pass in full host port proxy pair
  net_log_.BeginEvent(
      NetLog::TYPE_QUIC_SESSION,
      NetLog::StringCallback("host", &server_id.host()));
}

QuicClientSession::~QuicClientSession() {
  if (!streams()->empty())
    RecordUnexpectedOpenStreams(DESTRUCTOR);
  if (!observers_.empty())
    RecordUnexpectedObservers(DESTRUCTOR);
  if (!going_away_)
    RecordUnexpectedNotGoingAway(DESTRUCTOR);

  while (!streams()->empty() ||
         !observers_.empty() ||
         !stream_requests_.empty()) {
    // The session must be closed before it is destroyed.
    DCHECK(streams()->empty());
    CloseAllStreams(ERR_UNEXPECTED);
    DCHECK(observers_.empty());
    CloseAllObservers(ERR_UNEXPECTED);

    connection()->set_debug_visitor(NULL);
    net_log_.EndEvent(NetLog::TYPE_QUIC_SESSION);

    while (!stream_requests_.empty()) {
      StreamRequest* request = stream_requests_.front();
      stream_requests_.pop_front();
      request->OnRequestCompleteFailure(ERR_ABORTED);
    }
  }

  if (connection()->connected()) {
    // Ensure that the connection is closed by the time the session is
    // destroyed.
    connection()->CloseConnection(QUIC_INTERNAL_ERROR, false);
  }

  if (IsEncryptionEstablished())
    RecordHandshakeState(STATE_ENCRYPTION_ESTABLISHED);
  if (IsCryptoHandshakeConfirmed())
    RecordHandshakeState(STATE_HANDSHAKE_CONFIRMED);
  else
    RecordHandshakeState(STATE_FAILED);

  UMA_HISTOGRAM_COUNTS("Net.QuicSession.NumTotalStreams", num_total_streams_);
  UMA_HISTOGRAM_COUNTS("Net.QuicNumSentClientHellos",
                       crypto_stream_->num_sent_client_hellos());
  if (!IsCryptoHandshakeConfirmed())
    return;

  // Sending one client_hello means we had zero handshake-round-trips.
  int round_trip_handshakes = crypto_stream_->num_sent_client_hellos() - 1;

  // Don't bother with these histogram during tests, which mock out
  // num_sent_client_hellos().
  if (round_trip_handshakes < 0 || !stream_factory_)
    return;

  bool port_selected = stream_factory_->enable_port_selection();
  SSLInfo ssl_info;
  if (!GetSSLInfo(&ssl_info) || !ssl_info.cert) {
    if (port_selected) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ConnectSelectPortForHTTP",
                                  round_trip_handshakes, 0, 3, 4);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ConnectRandomPortForHTTP",
                                  round_trip_handshakes, 0, 3, 4);
    }
  } else {
    if (port_selected) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ConnectSelectPortForHTTPS",
                                  round_trip_handshakes, 0, 3, 4);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ConnectRandomPortForHTTPS",
                                  round_trip_handshakes, 0, 3, 4);
    }
  }
  const QuicConnectionStats stats = connection()->GetStats();
  if (stats.max_sequence_reordering == 0)
    return;
  const uint64 kMaxReordering = 100;
  uint64 reordering = kMaxReordering;
  if (stats.min_rtt_us > 0 ) {
    reordering =
        GG_UINT64_C(100) * stats.max_time_reordering_us / stats.min_rtt_us;
  }
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.MaxReorderingTime",
                                reordering, 0, kMaxReordering, 50);
  if (stats.min_rtt_us > 100 * 1000) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.MaxReorderingTimeLongRtt",
                                reordering, 0, kMaxReordering, 50);
  }
  UMA_HISTOGRAM_COUNTS("Net.QuicSession.MaxReordering",
                       stats.max_sequence_reordering);
}

void QuicClientSession::OnStreamFrames(
    const std::vector<QuicStreamFrame>& frames) {
  // Record total number of stream frames.
  UMA_HISTOGRAM_COUNTS("Net.QuicNumStreamFramesInPacket", frames.size());

  // Record number of frames per stream in packet.
  typedef std::map<QuicStreamId, size_t> FrameCounter;
  FrameCounter frames_per_stream;
  for (size_t i = 0; i < frames.size(); ++i) {
    frames_per_stream[frames[i].stream_id]++;
  }
  for (FrameCounter::const_iterator it = frames_per_stream.begin();
       it != frames_per_stream.end(); ++it) {
    UMA_HISTOGRAM_COUNTS("Net.QuicNumStreamFramesPerStreamInPacket",
                         it->second);
  }

  return QuicSession::OnStreamFrames(frames);
}

void QuicClientSession::AddObserver(Observer* observer) {
  if (going_away_) {
    RecordUnexpectedObservers(ADD_OBSERVER);
    observer->OnSessionClosed(ERR_UNEXPECTED);
    return;
  }

  DCHECK(!ContainsKey(observers_, observer));
  observers_.insert(observer);
}

void QuicClientSession::RemoveObserver(Observer* observer) {
  DCHECK(ContainsKey(observers_, observer));
  observers_.erase(observer);
}

int QuicClientSession::TryCreateStream(StreamRequest* request,
                                       QuicReliableClientStream** stream) {
  if (!crypto_stream_->encryption_established()) {
    DLOG(DFATAL) << "Encryption not established.";
    return ERR_CONNECTION_CLOSED;
  }

  if (goaway_received()) {
    DVLOG(1) << "Going away.";
    return ERR_CONNECTION_CLOSED;
  }

  if (!connection()->connected()) {
    DVLOG(1) << "Already closed.";
    return ERR_CONNECTION_CLOSED;
  }

  if (going_away_) {
    RecordUnexpectedOpenStreams(TRY_CREATE_STREAM);
    return ERR_CONNECTION_CLOSED;
  }

  if (GetNumOpenStreams() < get_max_open_streams()) {
    *stream = CreateOutgoingReliableStreamImpl();
    return OK;
  }

  stream_requests_.push_back(request);
  return ERR_IO_PENDING;
}

void QuicClientSession::CancelRequest(StreamRequest* request) {
  // Remove |request| from the queue while preserving the order of the
  // other elements.
  StreamRequestQueue::iterator it =
      std::find(stream_requests_.begin(), stream_requests_.end(), request);
  if (it != stream_requests_.end()) {
    it = stream_requests_.erase(it);
  }
}

QuicReliableClientStream* QuicClientSession::CreateOutgoingDataStream() {
  if (!crypto_stream_->encryption_established()) {
    DVLOG(1) << "Encryption not active so no outgoing stream created.";
    return NULL;
  }
  if (GetNumOpenStreams() >= get_max_open_streams()) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
             << "Already " << GetNumOpenStreams() << " open.";
    return NULL;
  }
  if (goaway_received()) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
             << "Already received goaway.";
    return NULL;
  }
  if (going_away_) {
    RecordUnexpectedOpenStreams(CREATE_OUTGOING_RELIABLE_STREAM);
    return NULL;
  }
  return CreateOutgoingReliableStreamImpl();
}

QuicReliableClientStream*
QuicClientSession::CreateOutgoingReliableStreamImpl() {
  DCHECK(connection()->connected());
  QuicReliableClientStream* stream =
      new QuicReliableClientStream(GetNextStreamId(), this, net_log_);
  ActivateStream(stream);
  ++num_total_streams_;
  UMA_HISTOGRAM_COUNTS("Net.QuicSession.NumOpenStreams", GetNumOpenStreams());
  return stream;
}

QuicCryptoClientStream* QuicClientSession::GetCryptoStream() {
  return crypto_stream_.get();
};

// TODO(rtenneti): Add unittests for GetSSLInfo which exercise the various ways
// we learn about SSL info (sync vs async vs cached).
bool QuicClientSession::GetSSLInfo(SSLInfo* ssl_info) const {
  ssl_info->Reset();
  if (!cert_verify_result_) {
    return false;
  }

  ssl_info->cert_status = cert_verify_result_->cert_status;
  ssl_info->cert = cert_verify_result_->verified_cert;

  // TODO(wtc): Define QUIC "cipher suites".
  // Report the TLS cipher suite that most closely resembles the crypto
  // parameters of the QUIC connection.
  QuicTag aead = crypto_stream_->crypto_negotiated_params().aead;
  int cipher_suite;
  int security_bits;
  switch (aead) {
    case kAESG:
      cipher_suite = 0xc02f;  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
      security_bits = 128;
      break;
    case kCC12:
      cipher_suite = 0xcc13;  // TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256
      security_bits = 256;
      break;
    default:
      NOTREACHED();
      return false;
  }
  int ssl_connection_status = 0;
  ssl_connection_status |=
      (cipher_suite & SSL_CONNECTION_CIPHERSUITE_MASK) <<
       SSL_CONNECTION_CIPHERSUITE_SHIFT;
  ssl_connection_status |=
      (SSL_CONNECTION_VERSION_QUIC & SSL_CONNECTION_VERSION_MASK) <<
       SSL_CONNECTION_VERSION_SHIFT;

  ssl_info->public_key_hashes = cert_verify_result_->public_key_hashes;
  ssl_info->is_issued_by_known_root =
      cert_verify_result_->is_issued_by_known_root;

  ssl_info->connection_status = ssl_connection_status;
  ssl_info->client_cert_sent = false;
  ssl_info->channel_id_sent = false;
  ssl_info->security_bits = security_bits;
  ssl_info->handshake_type = SSLInfo::HANDSHAKE_FULL;
  ssl_info->pinning_failure_log = pinning_failure_log_;
  return true;
}

int QuicClientSession::CryptoConnect(bool require_confirmation,
                                     const CompletionCallback& callback) {
  require_confirmation_ = require_confirmation;
  handshake_start_ = base::TimeTicks::Now();
  RecordHandshakeState(STATE_STARTED);
  if (!crypto_stream_->CryptoConnect()) {
    // TODO(wtc): change crypto_stream_.CryptoConnect() to return a
    // QuicErrorCode and map it to a net error code.
    return ERR_CONNECTION_FAILED;
  }

  if (IsCryptoHandshakeConfirmed())
    return OK;

  // Unless we require handshake confirmation, activate the session if
  // we have established initial encryption.
  if (!require_confirmation_ && IsEncryptionEstablished()) {
    // To mitigate the effects of hanging 0-RTT connections, set up a timer to
    // cancel any requests, if the handshake takes too long.
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&QuicClientSession::OnConnectTimeout,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(k0RttHandshakeTimeoutMs));
    return OK;

  }

  callback_ = callback;
  return ERR_IO_PENDING;
}

int QuicClientSession::ResumeCryptoConnect(const CompletionCallback& callback) {

  if (IsCryptoHandshakeConfirmed())
    return OK;

  if (!connection()->connected())
    return ERR_QUIC_HANDSHAKE_FAILED;

  callback_ = callback;
  return ERR_IO_PENDING;
}

int QuicClientSession::GetNumSentClientHellos() const {
  return crypto_stream_->num_sent_client_hellos();
}

bool QuicClientSession::CanPool(const std::string& hostname) const {
  // TODO(rch): When QUIC supports channel ID or client certificates, this
  // logic will need to be revised.
  DCHECK(connection()->connected());
  SSLInfo ssl_info;
  if (!GetSSLInfo(&ssl_info) || !ssl_info.cert) {
    // We can always pool with insecure QUIC sessions.
    return true;
  }

  // Disable pooling for secure sessions.
  // TODO(rch): re-enable this.
  return false;

#if 0
  bool unused = false;
  // Only pool secure QUIC sessions if the cert matches the new hostname.
  return ssl_info.cert->VerifyNameMatch(hostname, &unused);
#endif
}

QuicDataStream* QuicClientSession::CreateIncomingDataStream(
    QuicStreamId id) {
  DLOG(ERROR) << "Server push not supported";
  return NULL;
}

void QuicClientSession::CloseStream(QuicStreamId stream_id) {
  ReliableQuicStream* stream = GetStream(stream_id);
  if (stream) {
    logger_.UpdateReceivedFrameCounts(
        stream_id, stream->num_frames_received(),
        stream->num_duplicate_frames_received());
  }
  QuicSession::CloseStream(stream_id);
  OnClosedStream();
}

void QuicClientSession::SendRstStream(QuicStreamId id,
                                      QuicRstStreamErrorCode error,
                                      QuicStreamOffset bytes_written) {
  QuicSession::SendRstStream(id, error, bytes_written);
  OnClosedStream();
}

void QuicClientSession::OnClosedStream() {
  if (GetNumOpenStreams() < get_max_open_streams() &&
      !stream_requests_.empty() &&
      crypto_stream_->encryption_established() &&
      !goaway_received() &&
      !going_away_ &&
      connection()->connected()) {
    StreamRequest* request = stream_requests_.front();
    stream_requests_.pop_front();
    request->OnRequestCompleteSuccess(CreateOutgoingReliableStreamImpl());
  }

  if (GetNumOpenStreams() == 0) {
    stream_factory_->OnIdleSession(this);
  }
}

void QuicClientSession::OnCryptoHandshakeEvent(CryptoHandshakeEvent event) {
  if (!callback_.is_null() &&
      (!require_confirmation_ || event == HANDSHAKE_CONFIRMED)) {
    // TODO(rtenneti): Currently for all CryptoHandshakeEvent events, callback_
    // could be called because there are no error events in CryptoHandshakeEvent
    // enum. If error events are added to CryptoHandshakeEvent, then the
    // following code needs to changed.
    base::ResetAndReturn(&callback_).Run(OK);
  }
  if (event == HANDSHAKE_CONFIRMED) {
    UMA_HISTOGRAM_TIMES("Net.QuicSession.HandshakeConfirmedTime",
                        base::TimeTicks::Now() - handshake_start_);
    ObserverSet::iterator it = observers_.begin();
    while (it != observers_.end()) {
      Observer* observer = *it;
      ++it;
      observer->OnCryptoHandshakeConfirmed();
    }
  }
  QuicSession::OnCryptoHandshakeEvent(event);
}

void QuicClientSession::OnCryptoHandshakeMessageSent(
    const CryptoHandshakeMessage& message) {
  logger_.OnCryptoHandshakeMessageSent(message);
}

void QuicClientSession::OnCryptoHandshakeMessageReceived(
    const CryptoHandshakeMessage& message) {
  logger_.OnCryptoHandshakeMessageReceived(message);
}

void QuicClientSession::OnConnectionClosed(QuicErrorCode error,
                                           bool from_peer) {
  DCHECK(!connection()->connected());
  logger_.OnConnectionClosed(error, from_peer);
  if (from_peer) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "Net.QuicSession.ConnectionCloseErrorCodeServer", error);
  } else {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "Net.QuicSession.ConnectionCloseErrorCodeClient", error);
  }

  if (error == QUIC_CONNECTION_TIMED_OUT) {
    UMA_HISTOGRAM_COUNTS(
        "Net.QuicSession.ConnectionClose.NumOpenStreams.TimedOut",
        GetNumOpenStreams());
    if (!IsCryptoHandshakeConfirmed()) {
      UMA_HISTOGRAM_COUNTS(
          "Net.QuicSession.ConnectionClose.NumOpenStreams.HandshakeTimedOut",
          GetNumOpenStreams());
      UMA_HISTOGRAM_COUNTS(
          "Net.QuicSession.ConnectionClose.NumTotalStreams.HandshakeTimedOut",
          num_total_streams_);
    }
  }

  if (!IsCryptoHandshakeConfirmed()) {
    if (error == QUIC_PUBLIC_RESET) {
      RecordHandshakeFailureReason(HANDSHAKE_FAILURE_PUBLIC_RESET);
    } else if (connection()->GetStats().packets_received == 0) {
      RecordHandshakeFailureReason(HANDSHAKE_FAILURE_BLACK_HOLE);
      UMA_HISTOGRAM_SPARSE_SLOWLY(
          "Net.QuicSession.ConnectionClose.HandshakeFailureBlackHole.QuicError",
          error);
    } else {
      RecordHandshakeFailureReason(HANDSHAKE_FAILURE_UNKNOWN);
      UMA_HISTOGRAM_SPARSE_SLOWLY(
          "Net.QuicSession.ConnectionClose.HandshakeFailureUnknown.QuicError",
          error);
    }
  }

  UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.QuicVersion",
                              connection()->version());
  NotifyFactoryOfSessionGoingAway();
  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(ERR_QUIC_PROTOCOL_ERROR);
  }
  socket_->Close();
  QuicSession::OnConnectionClosed(error, from_peer);
  DCHECK(streams()->empty());
  CloseAllStreams(ERR_UNEXPECTED);
  CloseAllObservers(ERR_UNEXPECTED);
  NotifyFactoryOfSessionClosedLater();
}

void QuicClientSession::OnSuccessfulVersionNegotiation(
    const QuicVersion& version) {
  logger_.OnSuccessfulVersionNegotiation(version);
  QuicSession::OnSuccessfulVersionNegotiation(version);
}

void QuicClientSession::OnProofValid(
    const QuicCryptoClientConfig::CachedState& cached) {
  DCHECK(cached.proof_valid());

  if (!server_info_ || !server_info_->IsReadyToPersist()) {
    return;
  }

  QuicServerInfo::State* state = server_info_->mutable_state();

  state->server_config = cached.server_config();
  state->source_address_token = cached.source_address_token();
  state->server_config_sig = cached.signature();
  state->certs = cached.certs();

  server_info_->Persist();
}

void QuicClientSession::OnProofVerifyDetailsAvailable(
    const ProofVerifyDetails& verify_details) {
  const ProofVerifyDetailsChromium* verify_details_chromium =
      reinterpret_cast<const ProofVerifyDetailsChromium*>(&verify_details);
  CertVerifyResult* result_copy = new CertVerifyResult;
  result_copy->CopyFrom(verify_details_chromium->cert_verify_result);
  cert_verify_result_.reset(result_copy);
  pinning_failure_log_ = verify_details_chromium->pinning_failure_log;
}

void QuicClientSession::StartReading() {
  if (read_pending_) {
    return;
  }
  read_pending_ = true;
  int rv = socket_->Read(read_buffer_.get(),
                         read_buffer_->size(),
                         base::Bind(&QuicClientSession::OnReadComplete,
                                    weak_factory_.GetWeakPtr()));
  if (rv == ERR_IO_PENDING) {
    num_packets_read_ = 0;
    return;
  }

  if (++num_packets_read_ > 32) {
    num_packets_read_ = 0;
    // Data was read, process it.
    // Schedule the work through the message loop to 1) prevent infinite
    // recursion and 2) avoid blocking the thread for too long.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&QuicClientSession::OnReadComplete,
                   weak_factory_.GetWeakPtr(), rv));
  } else {
    OnReadComplete(rv);
  }
}

void QuicClientSession::CloseSessionOnError(int error) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.CloseSessionOnError", -error);
  CloseSessionOnErrorInner(error, QUIC_INTERNAL_ERROR);
  NotifyFactoryOfSessionClosed();
}

void QuicClientSession::CloseSessionOnErrorInner(int net_error,
                                                 QuicErrorCode quic_error) {
  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(net_error);
  }
  CloseAllStreams(net_error);
  CloseAllObservers(net_error);
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_CLOSE_ON_ERROR,
      NetLog::IntegerCallback("net_error", net_error));

  if (connection()->connected())
    connection()->CloseConnection(quic_error, false);
  DCHECK(!connection()->connected());
}

void QuicClientSession::CloseAllStreams(int net_error) {
  while (!streams()->empty()) {
    ReliableQuicStream* stream = streams()->begin()->second;
    QuicStreamId id = stream->id();
    static_cast<QuicReliableClientStream*>(stream)->OnError(net_error);
    CloseStream(id);
  }
}

void QuicClientSession::CloseAllObservers(int net_error) {
  while (!observers_.empty()) {
    Observer* observer = *observers_.begin();
    observers_.erase(observer);
    observer->OnSessionClosed(net_error);
  }
}

base::Value* QuicClientSession::GetInfoAsValue(
    const std::set<HostPortPair>& aliases) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  // TODO(rch): remove "host_port_pair" when Chrome 34 is stable.
  dict->SetString("host_port_pair", aliases.begin()->ToString());
  dict->SetString("version", QuicVersionToString(connection()->version()));
  dict->SetInteger("open_streams", GetNumOpenStreams());
  base::ListValue* stream_list = new base::ListValue();
  for (base::hash_map<QuicStreamId, QuicDataStream*>::const_iterator it
           = streams()->begin();
       it != streams()->end();
       ++it) {
    stream_list->Append(new base::StringValue(
        base::Uint64ToString(it->second->id())));
  }
  dict->Set("active_streams", stream_list);

  dict->SetInteger("total_streams", num_total_streams_);
  dict->SetString("peer_address", peer_address().ToString());
  dict->SetString("connection_id", base::Uint64ToString(connection_id()));
  dict->SetBoolean("connected", connection()->connected());
  const QuicConnectionStats& stats = connection()->GetStats();
  dict->SetInteger("packets_sent", stats.packets_sent);
  dict->SetInteger("packets_received", stats.packets_received);
  dict->SetInteger("packets_lost", stats.packets_lost);
  SSLInfo ssl_info;
  dict->SetBoolean("secure", GetSSLInfo(&ssl_info) && ssl_info.cert);

  base::ListValue* alias_list = new base::ListValue();
  for (std::set<HostPortPair>::const_iterator it = aliases.begin();
       it != aliases.end(); it++) {
    alias_list->Append(new base::StringValue(it->ToString()));
  }
  dict->Set("aliases", alias_list);

  return dict;
}

base::WeakPtr<QuicClientSession> QuicClientSession::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void QuicClientSession::OnReadComplete(int result) {
  read_pending_ = false;
  if (result == 0)
    result = ERR_CONNECTION_CLOSED;

  if (result < 0) {
    DVLOG(1) << "Closing session on read error: " << result;
    UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.ReadError", -result);
    NotifyFactoryOfSessionGoingAway();
    CloseSessionOnErrorInner(result, QUIC_PACKET_READ_ERROR);
    NotifyFactoryOfSessionClosedLater();
    return;
  }

  QuicEncryptedPacket packet(read_buffer_->data(), result);
  IPEndPoint local_address;
  IPEndPoint peer_address;
  socket_->GetLocalAddress(&local_address);
  socket_->GetPeerAddress(&peer_address);
  // ProcessUdpPacket might result in |this| being deleted, so we
  // use a weak pointer to be safe.
  connection()->ProcessUdpPacket(local_address, peer_address, packet);
  if (!connection()->connected()) {
    NotifyFactoryOfSessionClosedLater();
    return;
  }
  StartReading();
}

void QuicClientSession::NotifyFactoryOfSessionGoingAway() {
  going_away_ = true;
  if (stream_factory_)
    stream_factory_->OnSessionGoingAway(this);
}

void QuicClientSession::NotifyFactoryOfSessionClosedLater() {
  if (!streams()->empty())
    RecordUnexpectedOpenStreams(NOTIFY_FACTORY_OF_SESSION_CLOSED_LATER);

  if (!going_away_)
    RecordUnexpectedNotGoingAway(NOTIFY_FACTORY_OF_SESSION_CLOSED_LATER);

  going_away_ = true;
  DCHECK_EQ(0u, GetNumOpenStreams());
  DCHECK(!connection()->connected());
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&QuicClientSession::NotifyFactoryOfSessionClosed,
                 weak_factory_.GetWeakPtr()));
}

void QuicClientSession::NotifyFactoryOfSessionClosed() {
  if (!streams()->empty())
    RecordUnexpectedOpenStreams(NOTIFY_FACTORY_OF_SESSION_CLOSED);

  if (!going_away_)
    RecordUnexpectedNotGoingAway(NOTIFY_FACTORY_OF_SESSION_CLOSED);

  going_away_ = true;
  DCHECK_EQ(0u, GetNumOpenStreams());
  // Will delete |this|.
  if (stream_factory_)
    stream_factory_->OnSessionClosed(this);
}

void QuicClientSession::OnConnectTimeout() {
  DCHECK(callback_.is_null());
  DCHECK(IsEncryptionEstablished());

  if (IsCryptoHandshakeConfirmed())
    return;

  // TODO(rch): re-enable this code once beta is cut.
  //  if (stream_factory_)
  //    stream_factory_->OnSessionConnectTimeout(this);
  //  CloseAllStreams(ERR_QUIC_HANDSHAKE_FAILED);
  //  DCHECK_EQ(0u, GetNumOpenStreams());
}

}  // namespace net
