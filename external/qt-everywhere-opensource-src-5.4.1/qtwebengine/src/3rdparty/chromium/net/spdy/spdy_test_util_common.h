// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_TEST_UTIL_COMMON_H_
#define NET_SPDY_SPDY_TEST_UTIL_COMMON_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "net/base/completion_callback.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_state.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_protocol.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "testing/gtest/include/gtest/gtest.h"

class GURL;

namespace net {

class BoundNetLog;
class SpdySession;
class SpdySessionKey;
class SpdySessionPool;
class SpdyStream;
class SpdyStreamRequest;

// Default upload data used by both, mock objects and framer when creating
// data frames.
const char kDefaultURL[] = "http://www.google.com";
const char kUploadData[] = "hello!";
const int kUploadDataSize = arraysize(kUploadData)-1;

// SpdyNextProtos returns a vector of next protocols for negotiating
// SPDY.
NextProtoVector SpdyNextProtos();

// Chop a frame into an array of MockWrites.
// |data| is the frame to chop.
// |length| is the length of the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopWriteFrame(const char* data, int length, int num_chunks);

// Chop a SpdyFrame into an array of MockWrites.
// |frame| is the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopWriteFrame(const SpdyFrame& frame, int num_chunks);

// Chop a frame into an array of MockReads.
// |data| is the frame to chop.
// |length| is the length of the frame to chop.
// |num_chunks| is the number of chunks to create.
MockRead* ChopReadFrame(const char* data, int length, int num_chunks);

// Chop a SpdyFrame into an array of MockReads.
// |frame| is the frame to chop.
// |num_chunks| is the number of chunks to create.
MockRead* ChopReadFrame(const SpdyFrame& frame, int num_chunks);

// Adds headers and values to a map.
// |extra_headers| is an array of { name, value } pairs, arranged as strings
// where the even entries are the header names, and the odd entries are the
// header values.
// |headers| gets filled in from |extra_headers|.
void AppendToHeaderBlock(const char* const extra_headers[],
                         int extra_header_count,
                         SpdyHeaderBlock* headers);

// Create an async MockWrite from the given SpdyFrame.
MockWrite CreateMockWrite(const SpdyFrame& req);

// Create an async MockWrite from the given SpdyFrame and sequence number.
MockWrite CreateMockWrite(const SpdyFrame& req, int seq);

MockWrite CreateMockWrite(const SpdyFrame& req, int seq, IoMode mode);

// Create a MockRead from the given SpdyFrame.
MockRead CreateMockRead(const SpdyFrame& resp);

// Create a MockRead from the given SpdyFrame and sequence number.
MockRead CreateMockRead(const SpdyFrame& resp, int seq);

MockRead CreateMockRead(const SpdyFrame& resp, int seq, IoMode mode);

// Combines the given SpdyFrames into the given char array and returns
// the total length.
int CombineFrames(const SpdyFrame** frames, int num_frames,
                  char* buff, int buff_len);

// Returns the SpdyPriority embedded in the given frame.  Returns true
// and fills in |priority| on success.
bool GetSpdyPriority(SpdyMajorVersion version,
                     const SpdyFrame& frame,
                     SpdyPriority* priority);

// Tries to create a stream in |session| synchronously. Returns NULL
// on failure.
base::WeakPtr<SpdyStream> CreateStreamSynchronously(
    SpdyStreamType type,
    const base::WeakPtr<SpdySession>& session,
    const GURL& url,
    RequestPriority priority,
    const BoundNetLog& net_log);

// Helper class used by some tests to release a stream as soon as it's
// created.
class StreamReleaserCallback : public TestCompletionCallbackBase {
 public:
  StreamReleaserCallback();

  virtual ~StreamReleaserCallback();

  // Returns a callback that releases |request|'s stream.
  CompletionCallback MakeCallback(SpdyStreamRequest* request);

 private:
  void OnComplete(SpdyStreamRequest* request, int result);
};

const size_t kSpdyCredentialSlotUnused = 0;

// This struct holds information used to construct spdy control and data frames.
struct SpdyHeaderInfo {
  SpdyFrameType kind;
  SpdyStreamId id;
  SpdyStreamId assoc_id;
  SpdyPriority priority;
  size_t credential_slot;  // SPDY3 only
  SpdyControlFlags control_flags;
  bool compressed;
  SpdyRstStreamStatus status;
  const char* data;
  uint32 data_length;
  SpdyDataFlags data_flags;
};

// An ECSignatureCreator that returns deterministic signatures.
class MockECSignatureCreator : public crypto::ECSignatureCreator {
 public:
  explicit MockECSignatureCreator(crypto::ECPrivateKey* key);

  // crypto::ECSignatureCreator
  virtual bool Sign(const uint8* data,
                    int data_len,
                    std::vector<uint8>* signature) OVERRIDE;
  virtual bool DecodeSignature(const std::vector<uint8>& signature,
                               std::vector<uint8>* out_raw_sig) OVERRIDE;

 private:
  crypto::ECPrivateKey* key_;

  DISALLOW_COPY_AND_ASSIGN(MockECSignatureCreator);
};

// An ECSignatureCreatorFactory creates MockECSignatureCreator.
class MockECSignatureCreatorFactory : public crypto::ECSignatureCreatorFactory {
 public:
  MockECSignatureCreatorFactory();
  virtual ~MockECSignatureCreatorFactory();

  // crypto::ECSignatureCreatorFactory
  virtual crypto::ECSignatureCreator* Create(
      crypto::ECPrivateKey* key) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockECSignatureCreatorFactory);
};

// Helper to manage the lifetimes of the dependencies for a
// HttpNetworkTransaction.
struct SpdySessionDependencies {
  // Default set of dependencies -- "null" proxy service.
  explicit SpdySessionDependencies(NextProto protocol);

  // Custom proxy service dependency.
  SpdySessionDependencies(NextProto protocol, ProxyService* proxy_service);

  ~SpdySessionDependencies();

  static HttpNetworkSession* SpdyCreateSession(
      SpdySessionDependencies* session_deps);
  static HttpNetworkSession* SpdyCreateSessionDeterministic(
      SpdySessionDependencies* session_deps);
  static HttpNetworkSession::Params CreateSessionParams(
      SpdySessionDependencies* session_deps);

  // NOTE: host_resolver must be ordered before http_auth_handler_factory.
  scoped_ptr<MockHostResolverBase> host_resolver;
  scoped_ptr<CertVerifier> cert_verifier;
  scoped_ptr<TransportSecurityState> transport_security_state;
  scoped_ptr<ProxyService> proxy_service;
  scoped_refptr<SSLConfigService> ssl_config_service;
  scoped_ptr<MockClientSocketFactory> socket_factory;
  scoped_ptr<DeterministicMockClientSocketFactory> deterministic_socket_factory;
  scoped_ptr<HttpAuthHandlerFactory> http_auth_handler_factory;
  HttpServerPropertiesImpl http_server_properties;
  bool enable_ip_pooling;
  bool enable_compression;
  bool enable_ping;
  bool enable_user_alternate_protocol_ports;
  NextProto protocol;
  size_t stream_initial_recv_window_size;
  SpdySession::TimeFunc time_func;
  NextProtoVector next_protos;
  std::string trusted_spdy_proxy;
  bool force_spdy_over_ssl;
  bool force_spdy_always;
  bool use_alternate_protocols;
  bool enable_websocket_over_spdy;
  NetLog* net_log;
};

class SpdyURLRequestContext : public URLRequestContext {
 public:
  SpdyURLRequestContext(NextProto protocol,
                        bool force_spdy_over_ssl,
                        bool force_spdy_always);
  virtual ~SpdyURLRequestContext();

  MockClientSocketFactory& socket_factory() { return socket_factory_; }

 private:
  MockClientSocketFactory socket_factory_;
  net::URLRequestContextStorage storage_;
};

// Equivalent to pool->GetIfExists(spdy_session_key, BoundNetLog()) != NULL.
bool HasSpdySession(SpdySessionPool* pool, const SpdySessionKey& key);

// Creates a SPDY session for the given key and puts it in the SPDY
// session pool in |http_session|. A SPDY session for |key| must not
// already exist.
base::WeakPtr<SpdySession> CreateInsecureSpdySession(
    const scoped_refptr<HttpNetworkSession>& http_session,
    const SpdySessionKey& key,
    const BoundNetLog& net_log);

// Tries to create a SPDY session for the given key but expects the
// attempt to fail with the given error. A SPDY session for |key| must
// not already exist. The session will be created but close in the
// next event loop iteration.
base::WeakPtr<SpdySession> TryCreateInsecureSpdySessionExpectingFailure(
    const scoped_refptr<HttpNetworkSession>& http_session,
    const SpdySessionKey& key,
    Error expected_error,
    const BoundNetLog& net_log);

// Like CreateInsecureSpdySession(), but uses TLS.
base::WeakPtr<SpdySession> CreateSecureSpdySession(
    const scoped_refptr<HttpNetworkSession>& http_session,
    const SpdySessionKey& key,
    const BoundNetLog& net_log);

// Creates an insecure SPDY session for the given key and puts it in
// |pool|. The returned session will neither receive nor send any
// data. A SPDY session for |key| must not already exist.
base::WeakPtr<SpdySession> CreateFakeSpdySession(SpdySessionPool* pool,
                                                 const SpdySessionKey& key);

// Tries to create an insecure SPDY session for the given key but
// expects the attempt to fail with the given error. The session will
// neither receive nor send any data. A SPDY session for |key| must
// not already exist. The session will be created but close in the
// next event loop iteration.
base::WeakPtr<SpdySession> TryCreateFakeSpdySessionExpectingFailure(
    SpdySessionPool* pool,
    const SpdySessionKey& key,
    Error expected_error);

class SpdySessionPoolPeer {
 public:
  explicit SpdySessionPoolPeer(SpdySessionPool* pool);

  void RemoveAliases(const SpdySessionKey& key);
  void DisableDomainAuthenticationVerification();
  void SetEnableSendingInitialData(bool enabled);

 private:
  SpdySessionPool* const pool_;

  DISALLOW_COPY_AND_ASSIGN(SpdySessionPoolPeer);
};

class SpdyTestUtil {
 public:
  explicit SpdyTestUtil(NextProto protocol);

  // Add the appropriate headers to put |url| into |block|.
  void AddUrlToHeaderBlock(base::StringPiece url,
                           SpdyHeaderBlock* headers) const;

  scoped_ptr<SpdyHeaderBlock> ConstructGetHeaderBlock(
      base::StringPiece url) const;
  scoped_ptr<SpdyHeaderBlock> ConstructGetHeaderBlockForProxy(
      base::StringPiece url) const;
  scoped_ptr<SpdyHeaderBlock> ConstructHeadHeaderBlock(
      base::StringPiece url,
      int64 content_length) const;
  scoped_ptr<SpdyHeaderBlock> ConstructPostHeaderBlock(
      base::StringPiece url,
      int64 content_length) const;
  scoped_ptr<SpdyHeaderBlock> ConstructPutHeaderBlock(
      base::StringPiece url,
      int64 content_length) const;

  // Construct a SPDY frame.  If it is a SYN_STREAM or SYN_REPLY frame (as
  // specified in header_info.kind), the provided headers are included in the
  // frame.
  SpdyFrame* ConstructSpdyFrame(
      const SpdyHeaderInfo& header_info,
      scoped_ptr<SpdyHeaderBlock> headers) const;

  // Construct a SPDY frame.  If it is a SYN_STREAM or SYN_REPLY frame (as
  // specified in header_info.kind), the headers provided in extra_headers and
  // (if non-NULL) tail_headers are concatenated and included in the frame.
  // (extra_headers must always be non-NULL.)
  SpdyFrame* ConstructSpdyFrame(const SpdyHeaderInfo& header_info,
                                const char* const extra_headers[],
                                int extra_header_count,
                                const char* const tail_headers[],
                                int tail_header_count) const;

  // Construct a generic SpdyControlFrame.
  SpdyFrame* ConstructSpdyControlFrame(
      scoped_ptr<SpdyHeaderBlock> headers,
      bool compressed,
      SpdyStreamId stream_id,
      RequestPriority request_priority,
      SpdyFrameType type,
      SpdyControlFlags flags,
      SpdyStreamId associated_stream_id) const;

  // Construct a generic SpdyControlFrame.
  //
  // Warning: extra_header_count is the number of header-value pairs in
  // extra_headers (so half the number of elements), but tail_headers_size is
  // the actual number of elements (both keys and values) in tail_headers.
  // TODO(ttuttle): Fix this inconsistency.
  SpdyFrame* ConstructSpdyControlFrame(
      const char* const extra_headers[],
      int extra_header_count,
      bool compressed,
      SpdyStreamId stream_id,
      RequestPriority request_priority,
      SpdyFrameType type,
      SpdyControlFlags flags,
      const char* const* tail_headers,
      int tail_headers_size,
      SpdyStreamId associated_stream_id) const;

  // Construct an expected SPDY reply string from the given headers.
  std::string ConstructSpdyReplyString(const SpdyHeaderBlock& headers) const;

  // Construct an expected SPDY SETTINGS frame.
  // |settings| are the settings to set.
  // Returns the constructed frame.  The caller takes ownership of the frame.
  SpdyFrame* ConstructSpdySettings(const SettingsMap& settings) const;

  // Constructs an expected SPDY SETTINGS acknowledgement frame, if the protocol
  // version is SPDY4 or higher, or an empty placeholder frame otherwise.
  SpdyFrame* ConstructSpdySettingsAck() const;

  // Construct a SPDY PING frame.
  // Returns the constructed frame.  The caller takes ownership of the frame.
  SpdyFrame* ConstructSpdyPing(uint32 ping_id, bool is_ack) const;

  // Construct a SPDY GOAWAY frame with last_good_stream_id = 0.
  // Returns the constructed frame.  The caller takes ownership of the frame.
  SpdyFrame* ConstructSpdyGoAway() const;

  // Construct a SPDY GOAWAY frame with the specified last_good_stream_id.
  // Returns the constructed frame.  The caller takes ownership of the frame.
  SpdyFrame* ConstructSpdyGoAway(SpdyStreamId last_good_stream_id) const;

  // Construct a SPDY GOAWAY frame with the specified last_good_stream_id,
  // status, and description. Returns the constructed frame. The caller takes
  // ownership of the frame.
  SpdyFrame* ConstructSpdyGoAway(SpdyStreamId last_good_stream_id,
                                 SpdyGoAwayStatus status,
                                 const std::string& desc) const;

  // Construct a SPDY WINDOW_UPDATE frame.
  // Returns the constructed frame.  The caller takes ownership of the frame.
  SpdyFrame* ConstructSpdyWindowUpdate(
      SpdyStreamId stream_id,
      uint32 delta_window_size) const;

  // Construct a SPDY RST_STREAM frame.
  // Returns the constructed frame.  The caller takes ownership of the frame.
  SpdyFrame* ConstructSpdyRstStream(SpdyStreamId stream_id,
                                    SpdyRstStreamStatus status) const;

  // Constructs a standard SPDY GET SYN frame, optionally compressed
  // for |url|.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.
  // Returns a SpdyFrame.
  SpdyFrame* ConstructSpdyGet(const char* const url,
                              bool compressed,
                              SpdyStreamId stream_id,
                              RequestPriority request_priority) const;

  SpdyFrame* ConstructSpdyGetForProxy(const char* const url,
                                      bool compressed,
                                      SpdyStreamId stream_id,
                                      RequestPriority request_priority) const;

  // Constructs a standard SPDY GET SYN frame, optionally compressed.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.  If |direct| is false, the
  // the full url will be used instead of simply the path.
  // Returns a SpdyFrame.
  SpdyFrame* ConstructSpdyGet(const char* const extra_headers[],
                              int extra_header_count,
                              bool compressed,
                              int stream_id,
                              RequestPriority request_priority,
                              bool direct) const;

  // Constructs a standard SPDY SYN_STREAM frame for a CONNECT request.
  SpdyFrame* ConstructSpdyConnect(const char* const extra_headers[],
                                  int extra_header_count,
                                  int stream_id,
                                  RequestPriority priority) const;

  // Constructs a standard SPDY push SYN frame.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.
  // Returns a SpdyFrame.
  SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                               int extra_header_count,
                               int stream_id,
                               int associated_stream_id,
                               const char* url);
  SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                               int extra_header_count,
                               int stream_id,
                               int associated_stream_id,
                               const char* url,
                               const char* status,
                               const char* location);

  SpdyFrame* ConstructInitialSpdyPushFrame(scoped_ptr<SpdyHeaderBlock> headers,
                                           int stream_id,
                                           int associated_stream_id);

  SpdyFrame* ConstructSpdyPushHeaders(int stream_id,
                                      const char* const extra_headers[],
                                      int extra_header_count);

  // Constructs a standard SPDY SYN_REPLY frame to match the SPDY GET.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.
  // Returns a SpdyFrame.
  SpdyFrame* ConstructSpdyGetSynReply(const char* const extra_headers[],
                                      int extra_header_count,
                                      int stream_id);

  // Constructs a standard SPDY SYN_REPLY frame to match the SPDY GET.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.
  // Returns a SpdyFrame.
  SpdyFrame* ConstructSpdyGetSynReplyRedirect(int stream_id);

  // Constructs a standard SPDY SYN_REPLY frame with an Internal Server
  // Error status code.
  // Returns a SpdyFrame.
  SpdyFrame* ConstructSpdySynReplyError(int stream_id);

  // Constructs a standard SPDY SYN_REPLY frame with the specified status code.
  // Returns a SpdyFrame.
  SpdyFrame* ConstructSpdySynReplyError(const char* const status,
                                        const char* const* const extra_headers,
                                        int extra_header_count,
                                        int stream_id);

  // Constructs a standard SPDY POST SYN frame.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.
  // Returns a SpdyFrame.
  SpdyFrame* ConstructSpdyPost(const char* url,
                               SpdyStreamId stream_id,
                               int64 content_length,
                               RequestPriority priority,
                               const char* const extra_headers[],
                               int extra_header_count);

  // Constructs a chunked transfer SPDY POST SYN frame.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.
  // Returns a SpdyFrame.
  SpdyFrame* ConstructChunkedSpdyPost(const char* const extra_headers[],
                                      int extra_header_count);

  // Constructs a standard SPDY SYN_REPLY frame to match the SPDY POST.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.
  // Returns a SpdyFrame.
  SpdyFrame* ConstructSpdyPostSynReply(const char* const extra_headers[],
                                       int extra_header_count);

  // Constructs a single SPDY data frame with the contents "hello!"
  SpdyFrame* ConstructSpdyBodyFrame(int stream_id,
                                    bool fin);

  // Constructs a single SPDY data frame with the given content.
  SpdyFrame* ConstructSpdyBodyFrame(int stream_id, const char* data,
                                    uint32 len, bool fin);

  // Wraps |frame| in the payload of a data frame in stream |stream_id|.
  SpdyFrame* ConstructWrappedSpdyFrame(const scoped_ptr<SpdyFrame>& frame,
                                       int stream_id);

  const SpdyHeaderInfo MakeSpdyHeader(SpdyFrameType type);

  // For versions below SPDY4, adds the version HTTP/1.1 header.
  void MaybeAddVersionHeader(SpdyFrameWithNameValueBlockIR* frame_ir) const;

  // Maps |priority| to SPDY version priority, and sets it on |frame_ir|.
  void SetPriority(RequestPriority priority, SpdySynStreamIR* frame_ir) const;

  NextProto protocol() const { return protocol_; }
  SpdyMajorVersion spdy_version() const { return spdy_version_; }
  bool is_spdy2() const { return protocol_ < kProtoSPDY3; }
  bool include_version_header() const { return protocol_ < kProtoSPDY4; }
  scoped_ptr<SpdyFramer> CreateFramer(bool compressed) const;

  const char* GetMethodKey() const;
  const char* GetStatusKey() const;
  const char* GetHostKey() const;
  const char* GetSchemeKey() const;
  const char* GetVersionKey() const;
  const char* GetPathKey() const;

 private:
  // |content_length| may be NULL, in which case the content-length
  // header will be omitted.
  scoped_ptr<SpdyHeaderBlock> ConstructHeaderBlock(
      base::StringPiece method,
      base::StringPiece url,
      int64* content_length) const;

  const NextProto protocol_;
  const SpdyMajorVersion spdy_version_;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_TEST_UTIL_COMMON_H_
