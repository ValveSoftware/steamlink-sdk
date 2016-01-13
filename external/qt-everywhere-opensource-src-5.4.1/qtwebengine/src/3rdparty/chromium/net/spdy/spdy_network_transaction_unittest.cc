// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/test/test_file_util.h"
#include "net/base/auth.h"
#include "net/base/net_log_unittest.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "net/http/http_network_session_peer.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_test_util.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/next_proto.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/spdy/spdy_test_utils.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace net {

namespace {

using testing::Each;
using testing::Eq;

const char kRequestUrl[] = "http://www.google.com/";

enum SpdyNetworkTransactionTestSSLType {
  SPDYNPN,
  SPDYNOSSL,
  SPDYSSL,
};

struct SpdyNetworkTransactionTestParams {
  SpdyNetworkTransactionTestParams()
      : protocol(kProtoSPDY3),
        ssl_type(SPDYNPN) {}

  SpdyNetworkTransactionTestParams(
      NextProto protocol,
      SpdyNetworkTransactionTestSSLType ssl_type)
      : protocol(protocol),
        ssl_type(ssl_type) {}

  NextProto protocol;
  SpdyNetworkTransactionTestSSLType ssl_type;
};

void UpdateSpdySessionDependencies(
    SpdyNetworkTransactionTestParams test_params,
    SpdySessionDependencies* session_deps) {
  switch (test_params.ssl_type) {
    case SPDYNPN:
      session_deps->http_server_properties.SetAlternateProtocol(
          HostPortPair("www.google.com", 80), 443,
          AlternateProtocolFromNextProto(test_params.protocol));
      session_deps->use_alternate_protocols = true;
      session_deps->next_protos = SpdyNextProtos();
      break;
    case SPDYNOSSL:
      session_deps->force_spdy_over_ssl = false;
      session_deps->force_spdy_always = true;
      break;
    case SPDYSSL:
      session_deps->force_spdy_over_ssl = true;
      session_deps->force_spdy_always = true;
      break;
    default:
      NOTREACHED();
  }
}

SpdySessionDependencies* CreateSpdySessionDependencies(
    SpdyNetworkTransactionTestParams test_params) {
  SpdySessionDependencies* session_deps =
      new SpdySessionDependencies(test_params.protocol);
  UpdateSpdySessionDependencies(test_params, session_deps);
  return session_deps;
}

SpdySessionDependencies* CreateSpdySessionDependencies(
    SpdyNetworkTransactionTestParams test_params,
    ProxyService* proxy_service) {
  SpdySessionDependencies* session_deps =
      new SpdySessionDependencies(test_params.protocol, proxy_service);
  UpdateSpdySessionDependencies(test_params, session_deps);
  return session_deps;
}

}  // namespace

class SpdyNetworkTransactionTest
    : public ::testing::TestWithParam<SpdyNetworkTransactionTestParams> {
 protected:
  SpdyNetworkTransactionTest() : spdy_util_(GetParam().protocol) {
    LOG(INFO) << __FUNCTION__;
  }

  virtual ~SpdyNetworkTransactionTest() {
    LOG(INFO) << __FUNCTION__;
    // UploadDataStream posts deletion tasks back to the message loop on
    // destruction.
    upload_data_stream_.reset();
    base::RunLoop().RunUntilIdle();
    LOG(INFO) << __FUNCTION__;
  }

  virtual void SetUp() {
    LOG(INFO) << __FUNCTION__;
    google_get_request_initialized_ = false;
    google_post_request_initialized_ = false;
    google_chunked_post_request_initialized_ = false;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    LOG(INFO) << __FUNCTION__;
  }

  struct TransactionHelperResult {
    int rv;
    std::string status_line;
    std::string response_data;
    HttpResponseInfo response_info;
  };

  // A helper class that handles all the initial npn/ssl setup.
  class NormalSpdyTransactionHelper {
   public:
    NormalSpdyTransactionHelper(const HttpRequestInfo& request,
                                RequestPriority priority,
                                const BoundNetLog& log,
                                SpdyNetworkTransactionTestParams test_params,
                                SpdySessionDependencies* session_deps)
        : request_(request),
          priority_(priority),
          session_deps_(session_deps == NULL ?
                        CreateSpdySessionDependencies(test_params) :
                        session_deps),
          session_(SpdySessionDependencies::SpdyCreateSession(
                       session_deps_.get())),
          log_(log),
          test_params_(test_params),
          deterministic_(false),
          spdy_enabled_(true) {
      switch (test_params_.ssl_type) {
        case SPDYNOSSL:
        case SPDYSSL:
          port_ = 80;
          break;
        case SPDYNPN:
          port_ = 443;
          break;
        default:
          NOTREACHED();
      }
    }

    ~NormalSpdyTransactionHelper() {
      // Any test which doesn't close the socket by sending it an EOF will
      // have a valid session left open, which leaks the entire session pool.
      // This is just fine - in fact, some of our tests intentionally do this
      // so that we can check consistency of the SpdySessionPool as the test
      // finishes.  If we had put an EOF on the socket, the SpdySession would
      // have closed and we wouldn't be able to check the consistency.

      // Forcefully close existing sessions here.
      session()->spdy_session_pool()->CloseAllSessions();
    }

    void SetDeterministic() {
      session_ = SpdySessionDependencies::SpdyCreateSessionDeterministic(
          session_deps_.get());
      deterministic_ = true;
    }

    void SetSpdyDisabled() {
      spdy_enabled_ = false;
      port_ = 80;
    }

    void RunPreTestSetup() {
      LOG(INFO) << __FUNCTION__;
      if (!session_deps_.get())
        session_deps_.reset(CreateSpdySessionDependencies(test_params_));
      if (!session_.get()) {
        session_ = SpdySessionDependencies::SpdyCreateSession(
            session_deps_.get());
      }

      // We're now ready to use SSL-npn SPDY.
      trans_.reset(new HttpNetworkTransaction(priority_, session_.get()));
      LOG(INFO) << __FUNCTION__;
    }

    // Start the transaction, read some data, finish.
    void RunDefaultTest() {
      LOG(INFO) << __FUNCTION__;
      if (!StartDefaultTest())
        return;
      FinishDefaultTest();
      LOG(INFO) << __FUNCTION__;
    }

    bool StartDefaultTest() {
      output_.rv = trans_->Start(&request_, callback_.callback(), log_);

      // We expect an IO Pending or some sort of error.
      EXPECT_LT(output_.rv, 0);
      return output_.rv == ERR_IO_PENDING;
    }

    void FinishDefaultTest() {
      output_.rv = callback_.WaitForResult();
      if (output_.rv != OK) {
        session_->spdy_session_pool()->CloseCurrentSessions(net::ERR_ABORTED);
        return;
      }

      // Verify responses.
      const HttpResponseInfo* response = trans_->GetResponseInfo();
      ASSERT_TRUE(response != NULL);
      ASSERT_TRUE(response->headers.get() != NULL);
      EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
      EXPECT_EQ(spdy_enabled_, response->was_fetched_via_spdy);
      if (HttpStreamFactory::spdy_enabled()) {
        EXPECT_EQ(
            HttpResponseInfo::ConnectionInfoFromNextProto(
                test_params_.protocol),
            response->connection_info);
      } else {
        EXPECT_EQ(HttpResponseInfo::CONNECTION_INFO_HTTP1,
                  response->connection_info);
      }
      if (test_params_.ssl_type == SPDYNPN && spdy_enabled_) {
        EXPECT_TRUE(response->was_npn_negotiated);
      } else {
        EXPECT_TRUE(!response->was_npn_negotiated);
      }
      // If SPDY is not enabled, a HTTP request should not be diverted
      // over a SSL session.
      if (!spdy_enabled_) {
        EXPECT_EQ(request_.url.SchemeIs("https"),
                  response->was_npn_negotiated);
      }
      EXPECT_EQ("127.0.0.1", response->socket_address.host());
      EXPECT_EQ(port_, response->socket_address.port());
      output_.status_line = response->headers->GetStatusLine();
      output_.response_info = *response;  // Make a copy so we can verify.
      output_.rv = ReadTransaction(trans_.get(), &output_.response_data);
    }

    // Most tests will want to call this function. In particular, the MockReads
    // should end with an empty read, and that read needs to be processed to
    // ensure proper deletion of the spdy_session_pool.
    void VerifyDataConsumed() {
      for (DataVector::iterator it = data_vector_.begin();
          it != data_vector_.end(); ++it) {
        EXPECT_TRUE((*it)->at_read_eof()) << "Read count: "
                                          << (*it)->read_count()
                                          << " Read index: "
                                          << (*it)->read_index();
        EXPECT_TRUE((*it)->at_write_eof()) << "Write count: "
                                           << (*it)->write_count()
                                           << " Write index: "
                                           << (*it)->write_index();
      }
    }

    // Occasionally a test will expect to error out before certain reads are
    // processed. In that case we want to explicitly ensure that the reads were
    // not processed.
    void VerifyDataNotConsumed() {
      for (DataVector::iterator it = data_vector_.begin();
          it != data_vector_.end(); ++it) {
        EXPECT_TRUE(!(*it)->at_read_eof()) << "Read count: "
                                           << (*it)->read_count()
                                           << " Read index: "
                                           << (*it)->read_index();
        EXPECT_TRUE(!(*it)->at_write_eof()) << "Write count: "
                                            << (*it)->write_count()
                                            << " Write index: "
                                            << (*it)->write_index();
      }
    }

    void RunToCompletion(StaticSocketDataProvider* data) {
      RunPreTestSetup();
      AddData(data);
      RunDefaultTest();
      VerifyDataConsumed();
    }

    void RunToCompletionWithSSLData(
        StaticSocketDataProvider* data,
        scoped_ptr<SSLSocketDataProvider> ssl_provider) {
      RunPreTestSetup();
      AddDataWithSSLSocketDataProvider(data, ssl_provider.Pass());
      RunDefaultTest();
      VerifyDataConsumed();
    }

    void AddData(StaticSocketDataProvider* data) {
      scoped_ptr<SSLSocketDataProvider> ssl_provider(
          new SSLSocketDataProvider(ASYNC, OK));
      AddDataWithSSLSocketDataProvider(data, ssl_provider.Pass());
    }

    void AddDataWithSSLSocketDataProvider(
        StaticSocketDataProvider* data,
        scoped_ptr<SSLSocketDataProvider> ssl_provider) {
      DCHECK(!deterministic_);
      data_vector_.push_back(data);
      if (test_params_.ssl_type == SPDYNPN)
        ssl_provider->SetNextProto(test_params_.protocol);

      if (test_params_.ssl_type == SPDYNPN ||
          test_params_.ssl_type == SPDYSSL) {
        session_deps_->socket_factory->AddSSLSocketDataProvider(
            ssl_provider.get());
      }
      ssl_vector_.push_back(ssl_provider.release());

      session_deps_->socket_factory->AddSocketDataProvider(data);
      if (test_params_.ssl_type == SPDYNPN) {
        MockConnect never_finishing_connect(SYNCHRONOUS, ERR_IO_PENDING);
        StaticSocketDataProvider* hanging_non_alternate_protocol_socket =
            new StaticSocketDataProvider(NULL, 0, NULL, 0);
        hanging_non_alternate_protocol_socket->set_connect_data(
            never_finishing_connect);
        session_deps_->socket_factory->AddSocketDataProvider(
            hanging_non_alternate_protocol_socket);
        alternate_vector_.push_back(hanging_non_alternate_protocol_socket);
      }
    }

    void AddDeterministicData(DeterministicSocketData* data) {
      DCHECK(deterministic_);
      data_vector_.push_back(data);
      SSLSocketDataProvider* ssl_provider =
          new SSLSocketDataProvider(ASYNC, OK);
      if (test_params_.ssl_type == SPDYNPN)
        ssl_provider->SetNextProto(test_params_.protocol);

      ssl_vector_.push_back(ssl_provider);
      if (test_params_.ssl_type == SPDYNPN ||
          test_params_.ssl_type == SPDYSSL) {
        session_deps_->deterministic_socket_factory->
            AddSSLSocketDataProvider(ssl_provider);
      }
      session_deps_->deterministic_socket_factory->AddSocketDataProvider(data);
      if (test_params_.ssl_type == SPDYNPN) {
        MockConnect never_finishing_connect(SYNCHRONOUS, ERR_IO_PENDING);
        DeterministicSocketData* hanging_non_alternate_protocol_socket =
            new DeterministicSocketData(NULL, 0, NULL, 0);
        hanging_non_alternate_protocol_socket->set_connect_data(
            never_finishing_connect);
        session_deps_->deterministic_socket_factory->AddSocketDataProvider(
            hanging_non_alternate_protocol_socket);
        alternate_deterministic_vector_.push_back(
            hanging_non_alternate_protocol_socket);
      }
    }

    void SetSession(const scoped_refptr<HttpNetworkSession>& session) {
      session_ = session;
    }
    HttpNetworkTransaction* trans() { return trans_.get(); }
    void ResetTrans() { trans_.reset(); }
    TransactionHelperResult& output() { return output_; }
    const HttpRequestInfo& request() const { return request_; }
    const scoped_refptr<HttpNetworkSession>& session() const {
      return session_;
    }
    scoped_ptr<SpdySessionDependencies>& session_deps() {
      return session_deps_;
    }
    int port() const { return port_; }
    SpdyNetworkTransactionTestParams test_params() const {
      return test_params_;
    }

   private:
    typedef std::vector<StaticSocketDataProvider*> DataVector;
    typedef ScopedVector<SSLSocketDataProvider> SSLVector;
    typedef ScopedVector<StaticSocketDataProvider> AlternateVector;
    typedef ScopedVector<DeterministicSocketData> AlternateDeterministicVector;
    HttpRequestInfo request_;
    RequestPriority priority_;
    scoped_ptr<SpdySessionDependencies> session_deps_;
    scoped_refptr<HttpNetworkSession> session_;
    TransactionHelperResult output_;
    scoped_ptr<StaticSocketDataProvider> first_transaction_;
    SSLVector ssl_vector_;
    TestCompletionCallback callback_;
    scoped_ptr<HttpNetworkTransaction> trans_;
    scoped_ptr<HttpNetworkTransaction> trans_http_;
    DataVector data_vector_;
    AlternateVector alternate_vector_;
    AlternateDeterministicVector alternate_deterministic_vector_;
    const BoundNetLog& log_;
    SpdyNetworkTransactionTestParams test_params_;
    int port_;
    bool deterministic_;
    bool spdy_enabled_;
  };

  void ConnectStatusHelperWithExpectedStatus(const MockRead& status,
                                             int expected_status);

  void ConnectStatusHelper(const MockRead& status);

  const HttpRequestInfo& CreateGetPushRequest() {
    google_get_push_request_.method = "GET";
    google_get_push_request_.url = GURL("http://www.google.com/foo.dat");
    google_get_push_request_.load_flags = 0;
    return google_get_push_request_;
  }

  const HttpRequestInfo& CreateGetRequest() {
    if (!google_get_request_initialized_) {
      google_get_request_.method = "GET";
      google_get_request_.url = GURL(kDefaultURL);
      google_get_request_.load_flags = 0;
      google_get_request_initialized_ = true;
    }
    return google_get_request_;
  }

  const HttpRequestInfo& CreateGetRequestWithUserAgent() {
    if (!google_get_request_initialized_) {
      google_get_request_.method = "GET";
      google_get_request_.url = GURL(kDefaultURL);
      google_get_request_.load_flags = 0;
      google_get_request_.extra_headers.SetHeader("User-Agent", "Chrome");
      google_get_request_initialized_ = true;
    }
    return google_get_request_;
  }

  const HttpRequestInfo& CreatePostRequest() {
    if (!google_post_request_initialized_) {
      ScopedVector<UploadElementReader> element_readers;
      element_readers.push_back(
          new UploadBytesElementReader(kUploadData, kUploadDataSize));
      upload_data_stream_.reset(
          new UploadDataStream(element_readers.Pass(), 0));

      google_post_request_.method = "POST";
      google_post_request_.url = GURL(kDefaultURL);
      google_post_request_.upload_data_stream = upload_data_stream_.get();
      google_post_request_initialized_ = true;
    }
    return google_post_request_;
  }

  const HttpRequestInfo& CreateFilePostRequest() {
    if (!google_post_request_initialized_) {
      base::FilePath file_path;
      CHECK(base::CreateTemporaryFileInDir(temp_dir_.path(), &file_path));
      CHECK_EQ(static_cast<int>(kUploadDataSize),
               base::WriteFile(file_path, kUploadData, kUploadDataSize));

      ScopedVector<UploadElementReader> element_readers;
      element_readers.push_back(
          new UploadFileElementReader(base::MessageLoopProxy::current().get(),
                                      file_path,
                                      0,
                                      kUploadDataSize,
                                      base::Time()));
      upload_data_stream_.reset(
          new UploadDataStream(element_readers.Pass(), 0));

      google_post_request_.method = "POST";
      google_post_request_.url = GURL(kDefaultURL);
      google_post_request_.upload_data_stream = upload_data_stream_.get();
      google_post_request_initialized_ = true;
    }
    return google_post_request_;
  }

  const HttpRequestInfo& CreateUnreadableFilePostRequest() {
    if (google_post_request_initialized_)
      return google_post_request_;

    base::FilePath file_path;
    CHECK(base::CreateTemporaryFileInDir(temp_dir_.path(), &file_path));
    CHECK_EQ(static_cast<int>(kUploadDataSize),
             base::WriteFile(file_path, kUploadData, kUploadDataSize));
    CHECK(file_util::MakeFileUnreadable(file_path));

    ScopedVector<UploadElementReader> element_readers;
    element_readers.push_back(
        new UploadFileElementReader(base::MessageLoopProxy::current().get(),
                                    file_path,
                                    0,
                                    kUploadDataSize,
                                    base::Time()));
    upload_data_stream_.reset(
        new UploadDataStream(element_readers.Pass(), 0));

    google_post_request_.method = "POST";
    google_post_request_.url = GURL(kDefaultURL);
    google_post_request_.upload_data_stream = upload_data_stream_.get();
    google_post_request_initialized_ = true;
    return google_post_request_;
  }

  const HttpRequestInfo& CreateComplexPostRequest() {
    if (!google_post_request_initialized_) {
      const int kFileRangeOffset = 1;
      const int kFileRangeLength = 3;
      CHECK_LT(kFileRangeOffset + kFileRangeLength, kUploadDataSize);

      base::FilePath file_path;
      CHECK(base::CreateTemporaryFileInDir(temp_dir_.path(), &file_path));
      CHECK_EQ(static_cast<int>(kUploadDataSize),
               base::WriteFile(file_path, kUploadData, kUploadDataSize));

      ScopedVector<UploadElementReader> element_readers;
      element_readers.push_back(
          new UploadBytesElementReader(kUploadData, kFileRangeOffset));
      element_readers.push_back(
          new UploadFileElementReader(base::MessageLoopProxy::current().get(),
                                      file_path,
                                      kFileRangeOffset,
                                      kFileRangeLength,
                                      base::Time()));
      element_readers.push_back(new UploadBytesElementReader(
          kUploadData + kFileRangeOffset + kFileRangeLength,
          kUploadDataSize - (kFileRangeOffset + kFileRangeLength)));
      upload_data_stream_.reset(
          new UploadDataStream(element_readers.Pass(), 0));

      google_post_request_.method = "POST";
      google_post_request_.url = GURL(kDefaultURL);
      google_post_request_.upload_data_stream = upload_data_stream_.get();
      google_post_request_initialized_ = true;
    }
    return google_post_request_;
  }

  const HttpRequestInfo& CreateChunkedPostRequest() {
    if (!google_chunked_post_request_initialized_) {
      upload_data_stream_.reset(
          new UploadDataStream(UploadDataStream::CHUNKED, 0));
      google_chunked_post_request_.method = "POST";
      google_chunked_post_request_.url = GURL(kDefaultURL);
      google_chunked_post_request_.upload_data_stream =
          upload_data_stream_.get();
      google_chunked_post_request_initialized_ = true;
    }
    return google_chunked_post_request_;
  }

  // Read the result of a particular transaction, knowing that we've got
  // multiple transactions in the read pipeline; so as we read, we may have
  // to skip over data destined for other transactions while we consume
  // the data for |trans|.
  int ReadResult(HttpNetworkTransaction* trans,
                 StaticSocketDataProvider* data,
                 std::string* result) {
    const int kSize = 3000;

    int bytes_read = 0;
    scoped_refptr<net::IOBufferWithSize> buf(new net::IOBufferWithSize(kSize));
    TestCompletionCallback callback;
    while (true) {
      int rv = trans->Read(buf.get(), kSize, callback.callback());
      if (rv == ERR_IO_PENDING) {
        // Multiple transactions may be in the data set.  Keep pulling off
        // reads until we complete our callback.
        while (!callback.have_result()) {
          data->CompleteRead();
          base::RunLoop().RunUntilIdle();
        }
        rv = callback.WaitForResult();
      } else if (rv <= 0) {
        break;
      }
      result->append(buf->data(), rv);
      bytes_read += rv;
    }
    return bytes_read;
  }

  void VerifyStreamsClosed(const NormalSpdyTransactionHelper& helper) {
    // This lengthy block is reaching into the pool to dig out the active
    // session.  Once we have the session, we verify that the streams are
    // all closed and not leaked at this point.
    const GURL& url = helper.request().url;
    int port = helper.test_params().ssl_type == SPDYNPN ? 443 : 80;
    HostPortPair host_port_pair(url.host(), port);
    SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                       PRIVACY_MODE_DISABLED);
    BoundNetLog log;
    const scoped_refptr<HttpNetworkSession>& session = helper.session();
    base::WeakPtr<SpdySession> spdy_session =
        session->spdy_session_pool()->FindAvailableSession(key, log);
    ASSERT_TRUE(spdy_session != NULL);
    EXPECT_EQ(0u, spdy_session->num_active_streams());
    EXPECT_EQ(0u, spdy_session->num_unclaimed_pushed_streams());
  }

  void RunServerPushTest(OrderedSocketData* data,
                         HttpResponseInfo* response,
                         HttpResponseInfo* push_response,
                         const std::string& expected) {
    NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                       BoundNetLog(), GetParam(), NULL);
    helper.RunPreTestSetup();
    helper.AddData(data);

    HttpNetworkTransaction* trans = helper.trans();

    // Start the transaction with basic parameters.
    TestCompletionCallback callback;
    int rv = trans->Start(
        &CreateGetRequest(), callback.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    rv = callback.WaitForResult();

    // Request the pushed path.
    scoped_ptr<HttpNetworkTransaction> trans2(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
    rv = trans2->Start(
        &CreateGetPushRequest(), callback.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    base::RunLoop().RunUntilIdle();

    // The data for the pushed path may be coming in more than 1 frame. Compile
    // the results into a single string.

    // Read the server push body.
    std::string result2;
    ReadResult(trans2.get(), data, &result2);
    // Read the response body.
    std::string result;
    ReadResult(trans, data, &result);

    // Verify that we consumed all test data.
    EXPECT_TRUE(data->at_read_eof());
    EXPECT_TRUE(data->at_write_eof());

    // Verify that the received push data is same as the expected push data.
    EXPECT_EQ(result2.compare(expected), 0) << "Received data: "
                                            << result2
                                            << "||||| Expected data: "
                                            << expected;

    // Verify the SYN_REPLY.
    // Copy the response info, because trans goes away.
    *response = *trans->GetResponseInfo();
    *push_response = *trans2->GetResponseInfo();

    VerifyStreamsClosed(helper);
  }

  static void DeleteSessionCallback(NormalSpdyTransactionHelper* helper,
                                    int result) {
    helper->ResetTrans();
  }

  static void StartTransactionCallback(
      const scoped_refptr<HttpNetworkSession>& session,
      int result) {
    scoped_ptr<HttpNetworkTransaction> trans(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
    TestCompletionCallback callback;
    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;
    int rv = trans->Start(&request, callback.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    callback.WaitForResult();
  }

  SpdyTestUtil spdy_util_;

 private:
  scoped_ptr<UploadDataStream> upload_data_stream_;
  bool google_get_request_initialized_;
  bool google_post_request_initialized_;
  bool google_chunked_post_request_initialized_;
  HttpRequestInfo google_get_request_;
  HttpRequestInfo google_post_request_;
  HttpRequestInfo google_chunked_post_request_;
  HttpRequestInfo google_get_push_request_;
  base::ScopedTempDir temp_dir_;
};

//-----------------------------------------------------------------------------
// All tests are run with three different connection types: SPDY after NPN
// negotiation, SPDY without SSL, and SPDY with SSL.
//
// TODO(akalin): Use ::testing::Combine() when we are able to use
// <tr1/tuple>.
INSTANTIATE_TEST_CASE_P(
    Spdy,
    SpdyNetworkTransactionTest,
    ::testing::Values(
        SpdyNetworkTransactionTestParams(kProtoDeprecatedSPDY2, SPDYNOSSL),
        SpdyNetworkTransactionTestParams(kProtoDeprecatedSPDY2, SPDYSSL),
        SpdyNetworkTransactionTestParams(kProtoDeprecatedSPDY2, SPDYNPN),
        SpdyNetworkTransactionTestParams(kProtoSPDY3, SPDYNOSSL),
        SpdyNetworkTransactionTestParams(kProtoSPDY3, SPDYSSL),
        SpdyNetworkTransactionTestParams(kProtoSPDY3, SPDYNPN),
        SpdyNetworkTransactionTestParams(kProtoSPDY31, SPDYNOSSL),
        SpdyNetworkTransactionTestParams(kProtoSPDY31, SPDYSSL),
        SpdyNetworkTransactionTestParams(kProtoSPDY31, SPDYNPN),
        SpdyNetworkTransactionTestParams(kProtoSPDY4, SPDYNOSSL),
        SpdyNetworkTransactionTestParams(kProtoSPDY4, SPDYSSL),
        SpdyNetworkTransactionTestParams(kProtoSPDY4, SPDYNPN)));

// Verify HttpNetworkTransaction constructor.
TEST_P(SpdyNetworkTransactionTest, Constructor) {
  LOG(INFO) << __FUNCTION__;
  scoped_ptr<SpdySessionDependencies> session_deps(
      CreateSpdySessionDependencies(GetParam()));
  LOG(INFO) << __FUNCTION__;
  scoped_refptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(session_deps.get()));
  LOG(INFO) << __FUNCTION__;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session.get()));
  LOG(INFO) << __FUNCTION__;
}

TEST_P(SpdyNetworkTransactionTest, Get) {
  LOG(INFO) << __FUNCTION__;
  // Construct the request.
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

TEST_P(SpdyNetworkTransactionTest, GetAtEachPriority) {
  for (RequestPriority p = MINIMUM_PRIORITY; p <= MAXIMUM_PRIORITY;
       p = RequestPriority(p + 1)) {
    // Construct the request.
    scoped_ptr<SpdyFrame> req(
        spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, p, true));
    MockWrite writes[] = { CreateMockWrite(*req) };

    SpdyPriority spdy_prio = 0;
    EXPECT_TRUE(GetSpdyPriority(spdy_util_.spdy_version(), *req, &spdy_prio));
    // this repeats the RequestPriority-->SpdyPriority mapping from
    // SpdyFramer::ConvertRequestPriorityToSpdyPriority to make
    // sure it's being done right.
    if (spdy_util_.spdy_version() < SPDY3) {
      switch(p) {
        case HIGHEST:
          EXPECT_EQ(0, spdy_prio);
          break;
        case MEDIUM:
          EXPECT_EQ(1, spdy_prio);
          break;
        case LOW:
        case LOWEST:
          EXPECT_EQ(2, spdy_prio);
          break;
        case IDLE:
          EXPECT_EQ(3, spdy_prio);
          break;
        default:
          FAIL();
      }
    } else {
      switch(p) {
        case HIGHEST:
          EXPECT_EQ(0, spdy_prio);
          break;
        case MEDIUM:
          EXPECT_EQ(1, spdy_prio);
          break;
        case LOW:
          EXPECT_EQ(2, spdy_prio);
          break;
        case LOWEST:
          EXPECT_EQ(3, spdy_prio);
          break;
        case IDLE:
          EXPECT_EQ(4, spdy_prio);
          break;
        default:
          FAIL();
      }
    }

    scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
    scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
    MockRead reads[] = {
      CreateMockRead(*resp),
      CreateMockRead(*body),
      MockRead(ASYNC, 0, 0)  // EOF
    };

    DelayedSocketData data(1, reads, arraysize(reads),
                              writes, arraysize(writes));
    HttpRequestInfo http_req = CreateGetRequest();

    NormalSpdyTransactionHelper helper(http_req, p, BoundNetLog(),
                                       GetParam(), NULL);
    helper.RunToCompletion(&data);
    TransactionHelperResult out = helper.output();
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!", out.response_data);
  }
}

// Start three gets simultaniously; making sure that multiplexed
// streams work properly.

// This can't use the TransactionHelper method, since it only
// handles a single transaction, and finishes them as soon
// as it launches them.

// TODO(gavinp): create a working generalized TransactionHelper that
// can allow multiple streams in flight.

TEST_P(SpdyNetworkTransactionTest, ThreeGets) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> fbody(spdy_util_.ConstructSpdyBodyFrame(1, true));

  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, LOWEST, true));
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(3, false));
  scoped_ptr<SpdyFrame> fbody2(spdy_util_.ConstructSpdyBodyFrame(3, true));

  scoped_ptr<SpdyFrame> req3(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 5, LOWEST, true));
  scoped_ptr<SpdyFrame> resp3(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 5));
  scoped_ptr<SpdyFrame> body3(spdy_util_.ConstructSpdyBodyFrame(5, false));
  scoped_ptr<SpdyFrame> fbody3(spdy_util_.ConstructSpdyBodyFrame(5, true));

  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*req2),
    CreateMockWrite(*req3),
  };
  MockRead reads[] = {
    CreateMockRead(*resp, 1),
    CreateMockRead(*body),
    CreateMockRead(*resp2, 4),
    CreateMockRead(*body2),
    CreateMockRead(*resp3, 7),
    CreateMockRead(*body3),

    CreateMockRead(*fbody),
    CreateMockRead(*fbody2),
    CreateMockRead(*fbody3),

    MockRead(ASYNC, 0, 0),  // EOF
  };
  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  OrderedSocketData data_placeholder(NULL, 0, NULL, 0);

  BoundNetLog log;
  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  // We require placeholder data because three get requests are sent out, so
  // there needs to be three sets of SSL connection data.
  helper.AddData(&data_placeholder);
  helper.AddData(&data_placeholder);
  scoped_ptr<HttpNetworkTransaction> trans1(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
  scoped_ptr<HttpNetworkTransaction> trans3(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;
  TestCompletionCallback callback3;

  HttpRequestInfo httpreq1 = CreateGetRequest();
  HttpRequestInfo httpreq2 = CreateGetRequest();
  HttpRequestInfo httpreq3 = CreateGetRequest();

  out.rv = trans1->Start(&httpreq1, callback1.callback(), log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);
  out.rv = trans2->Start(&httpreq2, callback2.callback(), log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);
  out.rv = trans3->Start(&httpreq3, callback3.callback(), log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);

  out.rv = callback1.WaitForResult();
  ASSERT_EQ(OK, out.rv);
  out.rv = callback3.WaitForResult();
  ASSERT_EQ(OK, out.rv);

  const HttpResponseInfo* response1 = trans1->GetResponseInfo();
  EXPECT_TRUE(response1->headers.get() != NULL);
  EXPECT_TRUE(response1->was_fetched_via_spdy);
  out.status_line = response1->headers->GetStatusLine();
  out.response_info = *response1;

  trans2->GetResponseInfo();

  out.rv = ReadTransaction(trans1.get(), &out.response_data);
  helper.VerifyDataConsumed();
  EXPECT_EQ(OK, out.rv);

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);
}

TEST_P(SpdyNetworkTransactionTest, TwoGetsLateBinding) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> fbody(spdy_util_.ConstructSpdyBodyFrame(1, true));

  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, LOWEST, true));
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(3, false));
  scoped_ptr<SpdyFrame> fbody2(spdy_util_.ConstructSpdyBodyFrame(3, true));

  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*req2),
  };
  MockRead reads[] = {
    CreateMockRead(*resp, 1),
    CreateMockRead(*body),
    CreateMockRead(*resp2, 4),
    CreateMockRead(*body2),
    CreateMockRead(*fbody),
    CreateMockRead(*fbody2),
    MockRead(ASYNC, 0, 0),  // EOF
  };
  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));

  MockConnect never_finishing_connect(SYNCHRONOUS, ERR_IO_PENDING);

  OrderedSocketData data_placeholder(NULL, 0, NULL, 0);
  data_placeholder.set_connect_data(never_finishing_connect);

  BoundNetLog log;
  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  // We require placeholder data because two get requests are sent out, so
  // there needs to be two sets of SSL connection data.
  helper.AddData(&data_placeholder);
  scoped_ptr<HttpNetworkTransaction> trans1(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;

  HttpRequestInfo httpreq1 = CreateGetRequest();
  HttpRequestInfo httpreq2 = CreateGetRequest();

  out.rv = trans1->Start(&httpreq1, callback1.callback(), log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);
  out.rv = trans2->Start(&httpreq2, callback2.callback(), log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);

  out.rv = callback1.WaitForResult();
  ASSERT_EQ(OK, out.rv);
  out.rv = callback2.WaitForResult();
  ASSERT_EQ(OK, out.rv);

  const HttpResponseInfo* response1 = trans1->GetResponseInfo();
  EXPECT_TRUE(response1->headers.get() != NULL);
  EXPECT_TRUE(response1->was_fetched_via_spdy);
  out.status_line = response1->headers->GetStatusLine();
  out.response_info = *response1;
  out.rv = ReadTransaction(trans1.get(), &out.response_data);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  const HttpResponseInfo* response2 = trans2->GetResponseInfo();
  EXPECT_TRUE(response2->headers.get() != NULL);
  EXPECT_TRUE(response2->was_fetched_via_spdy);
  out.status_line = response2->headers->GetStatusLine();
  out.response_info = *response2;
  out.rv = ReadTransaction(trans2.get(), &out.response_data);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, TwoGetsLateBindingFromPreconnect) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> fbody(spdy_util_.ConstructSpdyBodyFrame(1, true));

  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, LOWEST, true));
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(3, false));
  scoped_ptr<SpdyFrame> fbody2(spdy_util_.ConstructSpdyBodyFrame(3, true));

  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*req2),
  };
  MockRead reads[] = {
    CreateMockRead(*resp, 1),
    CreateMockRead(*body),
    CreateMockRead(*resp2, 4),
    CreateMockRead(*body2),
    CreateMockRead(*fbody),
    CreateMockRead(*fbody2),
    MockRead(ASYNC, 0, 0),  // EOF
  };
  OrderedSocketData preconnect_data(reads, arraysize(reads),
                                    writes, arraysize(writes));

  MockConnect never_finishing_connect(ASYNC, ERR_IO_PENDING);

  OrderedSocketData data_placeholder(NULL, 0, NULL, 0);
  data_placeholder.set_connect_data(never_finishing_connect);

  BoundNetLog log;
  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&preconnect_data);
  // We require placeholder data because 3 connections are attempted (first is
  // the preconnect, 2nd and 3rd are the never finished connections.
  helper.AddData(&data_placeholder);
  helper.AddData(&data_placeholder);

  scoped_ptr<HttpNetworkTransaction> trans1(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;

  HttpRequestInfo httpreq = CreateGetRequest();

  // Preconnect the first.
  SSLConfig preconnect_ssl_config;
  helper.session()->ssl_config_service()->GetSSLConfig(&preconnect_ssl_config);
  HttpStreamFactory* http_stream_factory =
      helper.session()->http_stream_factory();
  helper.session()->GetNextProtos(&preconnect_ssl_config.next_protos);

  http_stream_factory->PreconnectStreams(
      1, httpreq, DEFAULT_PRIORITY,
      preconnect_ssl_config, preconnect_ssl_config);

  out.rv = trans1->Start(&httpreq, callback1.callback(), log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);
  out.rv = trans2->Start(&httpreq, callback2.callback(), log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);

  out.rv = callback1.WaitForResult();
  ASSERT_EQ(OK, out.rv);
  out.rv = callback2.WaitForResult();
  ASSERT_EQ(OK, out.rv);

  const HttpResponseInfo* response1 = trans1->GetResponseInfo();
  EXPECT_TRUE(response1->headers.get() != NULL);
  EXPECT_TRUE(response1->was_fetched_via_spdy);
  out.status_line = response1->headers->GetStatusLine();
  out.response_info = *response1;
  out.rv = ReadTransaction(trans1.get(), &out.response_data);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  const HttpResponseInfo* response2 = trans2->GetResponseInfo();
  EXPECT_TRUE(response2->headers.get() != NULL);
  EXPECT_TRUE(response2->was_fetched_via_spdy);
  out.status_line = response2->headers->GetStatusLine();
  out.response_info = *response2;
  out.rv = ReadTransaction(trans2.get(), &out.response_data);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  helper.VerifyDataConsumed();
}

// Similar to ThreeGets above, however this test adds a SETTINGS
// frame.  The SETTINGS frame is read during the IO loop waiting on
// the first transaction completion, and sets a maximum concurrent
// stream limit of 1.  This means that our IO loop exists after the
// second transaction completes, so we can assert on read_index().
TEST_P(SpdyNetworkTransactionTest, ThreeGetsWithMaxConcurrent) {
  // Construct the request.
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> fbody(spdy_util_.ConstructSpdyBodyFrame(1, true));

  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, LOWEST, true));
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(3, false));
  scoped_ptr<SpdyFrame> fbody2(spdy_util_.ConstructSpdyBodyFrame(3, true));

  scoped_ptr<SpdyFrame> req3(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 5, LOWEST, true));
  scoped_ptr<SpdyFrame> resp3(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 5));
  scoped_ptr<SpdyFrame> body3(spdy_util_.ConstructSpdyBodyFrame(5, false));
  scoped_ptr<SpdyFrame> fbody3(spdy_util_.ConstructSpdyBodyFrame(5, true));

  SettingsMap settings;
  const uint32 max_concurrent_streams = 1;
  settings[SETTINGS_MAX_CONCURRENT_STREAMS] =
      SettingsFlagsAndValue(SETTINGS_FLAG_NONE, max_concurrent_streams);
  scoped_ptr<SpdyFrame> settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  scoped_ptr<SpdyFrame> settings_ack(spdy_util_.ConstructSpdySettingsAck());

  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*settings_ack, 2),
    CreateMockWrite(*req2),
    CreateMockWrite(*req3),
  };

  MockRead reads[] = {
    CreateMockRead(*settings_frame, 1),
    CreateMockRead(*resp),
    CreateMockRead(*body),
    CreateMockRead(*fbody),
    CreateMockRead(*resp2, 8),
    CreateMockRead(*body2),
    CreateMockRead(*fbody2),
    CreateMockRead(*resp3, 13),
    CreateMockRead(*body3),
    CreateMockRead(*fbody3),

    MockRead(ASYNC, 0, 0),  // EOF
  };

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  OrderedSocketData data_placeholder(NULL, 0, NULL, 0);

  BoundNetLog log;
  TransactionHelperResult out;
  {
    NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                       BoundNetLog(), GetParam(), NULL);
    helper.RunPreTestSetup();
    helper.AddData(&data);
    // We require placeholder data because three get requests are sent out, so
    // there needs to be three sets of SSL connection data.
    helper.AddData(&data_placeholder);
    helper.AddData(&data_placeholder);
    scoped_ptr<HttpNetworkTransaction> trans1(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
    scoped_ptr<HttpNetworkTransaction> trans2(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
    scoped_ptr<HttpNetworkTransaction> trans3(
        new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));

    TestCompletionCallback callback1;
    TestCompletionCallback callback2;
    TestCompletionCallback callback3;

    HttpRequestInfo httpreq1 = CreateGetRequest();
    HttpRequestInfo httpreq2 = CreateGetRequest();
    HttpRequestInfo httpreq3 = CreateGetRequest();

    out.rv = trans1->Start(&httpreq1, callback1.callback(), log);
    ASSERT_EQ(out.rv, ERR_IO_PENDING);
    // Run transaction 1 through quickly to force a read of our SETTINGS
    // frame.
    out.rv = callback1.WaitForResult();
    ASSERT_EQ(OK, out.rv);

    out.rv = trans2->Start(&httpreq2, callback2.callback(), log);
    ASSERT_EQ(out.rv, ERR_IO_PENDING);
    out.rv = trans3->Start(&httpreq3, callback3.callback(), log);
    ASSERT_EQ(out.rv, ERR_IO_PENDING);
    out.rv = callback2.WaitForResult();
    ASSERT_EQ(OK, out.rv);
    EXPECT_EQ(7U, data.read_index());  // i.e. the third trans was queued

    out.rv = callback3.WaitForResult();
    ASSERT_EQ(OK, out.rv);

    const HttpResponseInfo* response1 = trans1->GetResponseInfo();
    ASSERT_TRUE(response1 != NULL);
    EXPECT_TRUE(response1->headers.get() != NULL);
    EXPECT_TRUE(response1->was_fetched_via_spdy);
    out.status_line = response1->headers->GetStatusLine();
    out.response_info = *response1;
    out.rv = ReadTransaction(trans1.get(), &out.response_data);
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!hello!", out.response_data);

    const HttpResponseInfo* response2 = trans2->GetResponseInfo();
    out.status_line = response2->headers->GetStatusLine();
    out.response_info = *response2;
    out.rv = ReadTransaction(trans2.get(), &out.response_data);
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!hello!", out.response_data);

    const HttpResponseInfo* response3 = trans3->GetResponseInfo();
    out.status_line = response3->headers->GetStatusLine();
    out.response_info = *response3;
    out.rv = ReadTransaction(trans3.get(), &out.response_data);
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!hello!", out.response_data);

    helper.VerifyDataConsumed();
  }
  EXPECT_EQ(OK, out.rv);
}

// Similar to ThreeGetsWithMaxConcurrent above, however this test adds
// a fourth transaction.  The third and fourth transactions have
// different data ("hello!" vs "hello!hello!") and because of the
// user specified priority, we expect to see them inverted in
// the response from the server.
TEST_P(SpdyNetworkTransactionTest, FourGetsWithMaxConcurrentPriority) {
  // Construct the request.
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> fbody(spdy_util_.ConstructSpdyBodyFrame(1, true));

  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, LOWEST, true));
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(3, false));
  scoped_ptr<SpdyFrame> fbody2(spdy_util_.ConstructSpdyBodyFrame(3, true));

  scoped_ptr<SpdyFrame> req4(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 5, HIGHEST, true));
  scoped_ptr<SpdyFrame> resp4(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 5));
  scoped_ptr<SpdyFrame> fbody4(spdy_util_.ConstructSpdyBodyFrame(5, true));

  scoped_ptr<SpdyFrame> req3(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 7, LOWEST, true));
  scoped_ptr<SpdyFrame> resp3(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 7));
  scoped_ptr<SpdyFrame> body3(spdy_util_.ConstructSpdyBodyFrame(7, false));
  scoped_ptr<SpdyFrame> fbody3(spdy_util_.ConstructSpdyBodyFrame(7, true));

  SettingsMap settings;
  const uint32 max_concurrent_streams = 1;
  settings[SETTINGS_MAX_CONCURRENT_STREAMS] =
      SettingsFlagsAndValue(SETTINGS_FLAG_NONE, max_concurrent_streams);
  scoped_ptr<SpdyFrame> settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  scoped_ptr<SpdyFrame> settings_ack(spdy_util_.ConstructSpdySettingsAck());

  MockWrite writes[] = { CreateMockWrite(*req),
    CreateMockWrite(*settings_ack, 2),
    CreateMockWrite(*req2),
    CreateMockWrite(*req4),
    CreateMockWrite(*req3),
  };
  MockRead reads[] = {
    CreateMockRead(*settings_frame, 1),
    CreateMockRead(*resp),
    CreateMockRead(*body),
    CreateMockRead(*fbody),
    CreateMockRead(*resp2, 8),
    CreateMockRead(*body2),
    CreateMockRead(*fbody2),
    CreateMockRead(*resp4, 14),
    CreateMockRead(*fbody4),
    CreateMockRead(*resp3, 17),
    CreateMockRead(*body3),
    CreateMockRead(*fbody3),

    MockRead(ASYNC, 0, 0),  // EOF
  };

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  OrderedSocketData data_placeholder(NULL, 0, NULL, 0);

  BoundNetLog log;
  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  // We require placeholder data because four get requests are sent out, so
  // there needs to be four sets of SSL connection data.
  helper.AddData(&data_placeholder);
  helper.AddData(&data_placeholder);
  helper.AddData(&data_placeholder);
  scoped_ptr<HttpNetworkTransaction> trans1(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
  scoped_ptr<HttpNetworkTransaction> trans3(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
  scoped_ptr<HttpNetworkTransaction> trans4(
      new HttpNetworkTransaction(HIGHEST, helper.session().get()));

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;
  TestCompletionCallback callback3;
  TestCompletionCallback callback4;

  HttpRequestInfo httpreq1 = CreateGetRequest();
  HttpRequestInfo httpreq2 = CreateGetRequest();
  HttpRequestInfo httpreq3 = CreateGetRequest();
  HttpRequestInfo httpreq4 = CreateGetRequest();

  out.rv = trans1->Start(&httpreq1, callback1.callback(), log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);
  // Run transaction 1 through quickly to force a read of our SETTINGS frame.
  out.rv = callback1.WaitForResult();
  ASSERT_EQ(OK, out.rv);

  out.rv = trans2->Start(&httpreq2, callback2.callback(), log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);
  out.rv = trans3->Start(&httpreq3, callback3.callback(), log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);
  out.rv = trans4->Start(&httpreq4, callback4.callback(), log);
  ASSERT_EQ(ERR_IO_PENDING, out.rv);

  out.rv = callback2.WaitForResult();
  ASSERT_EQ(OK, out.rv);
  EXPECT_EQ(data.read_index(), 7U);  // i.e. the third & fourth trans queued

  out.rv = callback3.WaitForResult();
  ASSERT_EQ(OK, out.rv);

  const HttpResponseInfo* response1 = trans1->GetResponseInfo();
  EXPECT_TRUE(response1->headers.get() != NULL);
  EXPECT_TRUE(response1->was_fetched_via_spdy);
  out.status_line = response1->headers->GetStatusLine();
  out.response_info = *response1;
  out.rv = ReadTransaction(trans1.get(), &out.response_data);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  const HttpResponseInfo* response2 = trans2->GetResponseInfo();
  out.status_line = response2->headers->GetStatusLine();
  out.response_info = *response2;
  out.rv = ReadTransaction(trans2.get(), &out.response_data);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  // notice: response3 gets two hellos, response4 gets one
  // hello, so we know dequeuing priority was respected.
  const HttpResponseInfo* response3 = trans3->GetResponseInfo();
  out.status_line = response3->headers->GetStatusLine();
  out.response_info = *response3;
  out.rv = ReadTransaction(trans3.get(), &out.response_data);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  out.rv = callback4.WaitForResult();
  EXPECT_EQ(OK, out.rv);
  const HttpResponseInfo* response4 = trans4->GetResponseInfo();
  out.status_line = response4->headers->GetStatusLine();
  out.response_info = *response4;
  out.rv = ReadTransaction(trans4.get(), &out.response_data);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
  helper.VerifyDataConsumed();
  EXPECT_EQ(OK, out.rv);
}

// Similar to ThreeGetsMaxConcurrrent above, however, this test
// deletes a session in the middle of the transaction to insure
// that we properly remove pendingcreatestream objects from
// the spdy_session
TEST_P(SpdyNetworkTransactionTest, ThreeGetsWithMaxConcurrentDelete) {
  // Construct the request.
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> fbody(spdy_util_.ConstructSpdyBodyFrame(1, true));

  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, LOWEST, true));
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(3, false));
  scoped_ptr<SpdyFrame> fbody2(spdy_util_.ConstructSpdyBodyFrame(3, true));

  SettingsMap settings;
  const uint32 max_concurrent_streams = 1;
  settings[SETTINGS_MAX_CONCURRENT_STREAMS] =
      SettingsFlagsAndValue(SETTINGS_FLAG_NONE, max_concurrent_streams);
  scoped_ptr<SpdyFrame> settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  scoped_ptr<SpdyFrame> settings_ack(spdy_util_.ConstructSpdySettingsAck());

  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*settings_ack, 2),
    CreateMockWrite(*req2),
  };
  MockRead reads[] = {
    CreateMockRead(*settings_frame, 1),
    CreateMockRead(*resp),
    CreateMockRead(*body),
    CreateMockRead(*fbody),
    CreateMockRead(*resp2, 8),
    CreateMockRead(*body2),
    CreateMockRead(*fbody2),
    MockRead(ASYNC, 0, 0),  // EOF
  };

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  OrderedSocketData data_placeholder(NULL, 0, NULL, 0);

  BoundNetLog log;
  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  // We require placeholder data because three get requests are sent out, so
  // there needs to be three sets of SSL connection data.
  helper.AddData(&data_placeholder);
  helper.AddData(&data_placeholder);
  scoped_ptr<HttpNetworkTransaction> trans1(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
  scoped_ptr<HttpNetworkTransaction> trans3(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;
  TestCompletionCallback callback3;

  HttpRequestInfo httpreq1 = CreateGetRequest();
  HttpRequestInfo httpreq2 = CreateGetRequest();
  HttpRequestInfo httpreq3 = CreateGetRequest();

  out.rv = trans1->Start(&httpreq1, callback1.callback(), log);
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  // Run transaction 1 through quickly to force a read of our SETTINGS frame.
  out.rv = callback1.WaitForResult();
  ASSERT_EQ(OK, out.rv);

  out.rv = trans2->Start(&httpreq2, callback2.callback(), log);
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = trans3->Start(&httpreq3, callback3.callback(), log);
  delete trans3.release();
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = callback2.WaitForResult();
  ASSERT_EQ(OK, out.rv);

  EXPECT_EQ(8U, data.read_index());

  const HttpResponseInfo* response1 = trans1->GetResponseInfo();
  ASSERT_TRUE(response1 != NULL);
  EXPECT_TRUE(response1->headers.get() != NULL);
  EXPECT_TRUE(response1->was_fetched_via_spdy);
  out.status_line = response1->headers->GetStatusLine();
  out.response_info = *response1;
  out.rv = ReadTransaction(trans1.get(), &out.response_data);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);

  const HttpResponseInfo* response2 = trans2->GetResponseInfo();
  ASSERT_TRUE(response2 != NULL);
  out.status_line = response2->headers->GetStatusLine();
  out.response_info = *response2;
  out.rv = ReadTransaction(trans2.get(), &out.response_data);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!hello!", out.response_data);
  helper.VerifyDataConsumed();
  EXPECT_EQ(OK, out.rv);
}

namespace {

// The KillerCallback will delete the transaction on error as part of the
// callback.
class KillerCallback : public TestCompletionCallbackBase {
 public:
  explicit KillerCallback(HttpNetworkTransaction* transaction)
      : transaction_(transaction),
        callback_(base::Bind(&KillerCallback::OnComplete,
                             base::Unretained(this))) {
  }

  virtual ~KillerCallback() {}

  const CompletionCallback& callback() const { return callback_; }

 private:
  void OnComplete(int result) {
    if (result < 0)
      delete transaction_;

    SetResult(result);
  }

  HttpNetworkTransaction* transaction_;
  CompletionCallback callback_;
};

}  // namespace

// Similar to ThreeGetsMaxConcurrrentDelete above, however, this test
// closes the socket while we have a pending transaction waiting for
// a pending stream creation.  http://crbug.com/52901
TEST_P(SpdyNetworkTransactionTest, ThreeGetsWithMaxConcurrentSocketClose) {
  // Construct the request.
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> fin_body(spdy_util_.ConstructSpdyBodyFrame(1, true));

  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, LOWEST, true));
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));

  SettingsMap settings;
  const uint32 max_concurrent_streams = 1;
  settings[SETTINGS_MAX_CONCURRENT_STREAMS] =
      SettingsFlagsAndValue(SETTINGS_FLAG_NONE, max_concurrent_streams);
  scoped_ptr<SpdyFrame> settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  scoped_ptr<SpdyFrame> settings_ack(spdy_util_.ConstructSpdySettingsAck());

  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*settings_ack, 2),
    CreateMockWrite(*req2),
  };
  MockRead reads[] = {
    CreateMockRead(*settings_frame, 1),
    CreateMockRead(*resp),
    CreateMockRead(*body),
    CreateMockRead(*fin_body),
    CreateMockRead(*resp2, 8),
    MockRead(ASYNC, ERR_CONNECTION_RESET, 0),  // Abort!
  };

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  OrderedSocketData data_placeholder(NULL, 0, NULL, 0);

  BoundNetLog log;
  TransactionHelperResult out;
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  // We require placeholder data because three get requests are sent out, so
  // there needs to be three sets of SSL connection data.
  helper.AddData(&data_placeholder);
  helper.AddData(&data_placeholder);
  HttpNetworkTransaction trans1(DEFAULT_PRIORITY, helper.session().get());
  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, helper.session().get());
  HttpNetworkTransaction* trans3(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;
  KillerCallback callback3(trans3);

  HttpRequestInfo httpreq1 = CreateGetRequest();
  HttpRequestInfo httpreq2 = CreateGetRequest();
  HttpRequestInfo httpreq3 = CreateGetRequest();

  out.rv = trans1.Start(&httpreq1, callback1.callback(), log);
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  // Run transaction 1 through quickly to force a read of our SETTINGS frame.
  out.rv = callback1.WaitForResult();
  ASSERT_EQ(OK, out.rv);

  out.rv = trans2.Start(&httpreq2, callback2.callback(), log);
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = trans3->Start(&httpreq3, callback3.callback(), log);
  ASSERT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = callback3.WaitForResult();
  ASSERT_EQ(ERR_ABORTED, out.rv);

  EXPECT_EQ(6U, data.read_index());

  const HttpResponseInfo* response1 = trans1.GetResponseInfo();
  ASSERT_TRUE(response1 != NULL);
  EXPECT_TRUE(response1->headers.get() != NULL);
  EXPECT_TRUE(response1->was_fetched_via_spdy);
  out.status_line = response1->headers->GetStatusLine();
  out.response_info = *response1;
  out.rv = ReadTransaction(&trans1, &out.response_data);
  EXPECT_EQ(OK, out.rv);

  const HttpResponseInfo* response2 = trans2.GetResponseInfo();
  ASSERT_TRUE(response2 != NULL);
  out.status_line = response2->headers->GetStatusLine();
  out.response_info = *response2;
  out.rv = ReadTransaction(&trans2, &out.response_data);
  EXPECT_EQ(ERR_CONNECTION_RESET, out.rv);

  helper.VerifyDataConsumed();
}

// Test that a simple PUT request works.
TEST_P(SpdyNetworkTransactionTest, Put) {
  // Setup the request
  HttpRequestInfo request;
  request.method = "PUT";
  request.url = GURL("http://www.google.com/");

  const SpdyHeaderInfo kSynStartHeader = {
    SYN_STREAM,             // Kind = Syn
    1,                      // Stream ID
    0,                      // Associated stream ID
    ConvertRequestPriorityToSpdyPriority(
        LOWEST, spdy_util_.spdy_version()),
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_FIN,       // Control Flags
    false,                  // Compressed
    RST_STREAM_INVALID,     // Status
    NULL,                   // Data
    0,                      // Length
    DATA_FLAG_NONE          // Data Flags
  };
  scoped_ptr<SpdyHeaderBlock> put_headers(
      spdy_util_.ConstructPutHeaderBlock("http://www.google.com", 0));
  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyFrame(
      kSynStartHeader, put_headers.Pass()));
  MockWrite writes[] = {
    CreateMockWrite(*req),
  };

  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  const SpdyHeaderInfo kSynReplyHeader = {
    SYN_REPLY,              // Kind = SynReply
    1,                      // Stream ID
    0,                      // Associated stream ID
    ConvertRequestPriorityToSpdyPriority(
        LOWEST, spdy_util_.spdy_version()),
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_NONE,      // Control Flags
    false,                  // Compressed
    RST_STREAM_INVALID,     // Status
    NULL,                   // Data
    0,                      // Length
    DATA_FLAG_NONE          // Data Flags
  };
  scoped_ptr<SpdyHeaderBlock> reply_headers(new SpdyHeaderBlock());
  (*reply_headers)[spdy_util_.GetStatusKey()] = "200";
  (*reply_headers)[spdy_util_.GetVersionKey()] = "HTTP/1.1";
  (*reply_headers)["content-length"] = "1234";
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyFrame(
      kSynReplyHeader, reply_headers.Pass()));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(request, DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
}

// Test that a simple HEAD request works.
TEST_P(SpdyNetworkTransactionTest, Head) {
  // Setup the request
  HttpRequestInfo request;
  request.method = "HEAD";
  request.url = GURL("http://www.google.com/");

  const SpdyHeaderInfo kSynStartHeader = {
    SYN_STREAM,             // Kind = Syn
    1,                      // Stream ID
    0,                      // Associated stream ID
    ConvertRequestPriorityToSpdyPriority(
        LOWEST, spdy_util_.spdy_version()),
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_FIN,       // Control Flags
    false,                  // Compressed
    RST_STREAM_INVALID,     // Status
    NULL,                   // Data
    0,                      // Length
    DATA_FLAG_NONE          // Data Flags
  };
  scoped_ptr<SpdyHeaderBlock> head_headers(
      spdy_util_.ConstructHeadHeaderBlock("http://www.google.com", 0));
  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyFrame(
      kSynStartHeader, head_headers.Pass()));
  MockWrite writes[] = {
    CreateMockWrite(*req),
  };

  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  const SpdyHeaderInfo kSynReplyHeader = {
    SYN_REPLY,              // Kind = SynReply
    1,                      // Stream ID
    0,                      // Associated stream ID
    ConvertRequestPriorityToSpdyPriority(
        LOWEST, spdy_util_.spdy_version()),
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_NONE,      // Control Flags
    false,                  // Compressed
    RST_STREAM_INVALID,     // Status
    NULL,                   // Data
    0,                      // Length
    DATA_FLAG_NONE          // Data Flags
  };
  scoped_ptr<SpdyHeaderBlock> reply_headers(new SpdyHeaderBlock());
  (*reply_headers)[spdy_util_.GetStatusKey()] = "200";
  (*reply_headers)[spdy_util_.GetVersionKey()] = "HTTP/1.1";
  (*reply_headers)["content-length"] = "1234";
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyFrame(
      kSynReplyHeader,
      reply_headers.Pass()));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(request, DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
}

// Test that a simple POST works.
TEST_P(SpdyNetworkTransactionTest, Post) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kRequestUrl, 1, kUploadDataSize, LOWEST, NULL, 0));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*body),  // POST upload frame
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(2, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreatePostRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a POST with a file works.
TEST_P(SpdyNetworkTransactionTest, FilePost) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kRequestUrl, 1, kUploadDataSize, LOWEST, NULL, 0));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*body),  // POST upload frame
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(2, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateFilePostRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a POST with a unreadable file fails.
TEST_P(SpdyNetworkTransactionTest, UnreadableFilePost) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, 0)  // EOF
  };
  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads), writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateUnreadableFilePostRequest(),
                                     DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  helper.RunDefaultTest();

  base::RunLoop().RunUntilIdle();
  helper.VerifyDataNotConsumed();
  EXPECT_EQ(ERR_ACCESS_DENIED, helper.output().rv);
}

// Test that a complex POST works.
TEST_P(SpdyNetworkTransactionTest, ComplexPost) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kRequestUrl, 1, kUploadDataSize, LOWEST, NULL, 0));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*body),  // POST upload frame
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(2, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateComplexPostRequest(),
                                     DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a chunked POST works.
TEST_P(SpdyNetworkTransactionTest, ChunkedPost) {
  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructChunkedSpdyPost(NULL, 0));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*body),
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(2, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateChunkedPostRequest(),
                                     DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);

  // These chunks get merged into a single frame when being sent.
  const int kFirstChunkSize = kUploadDataSize/2;
  helper.request().upload_data_stream->AppendChunk(
      kUploadData, kFirstChunkSize, false);
  helper.request().upload_data_stream->AppendChunk(
      kUploadData + kFirstChunkSize, kUploadDataSize - kFirstChunkSize, true);

  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ(kUploadData, out.response_data);
}

// Test that a chunked POST works with chunks appended after transaction starts.
TEST_P(SpdyNetworkTransactionTest, DelayedChunkedPost) {
  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructChunkedSpdyPost(NULL, 0));
  scoped_ptr<SpdyFrame> chunk1(spdy_util_.ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> chunk2(spdy_util_.ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> chunk3(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*chunk1),
    CreateMockWrite(*chunk2),
    CreateMockWrite(*chunk3),
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*chunk1),
    CreateMockRead(*chunk2),
    CreateMockRead(*chunk3),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(4, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateChunkedPostRequest(),
                                     DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);

  helper.request().upload_data_stream->AppendChunk(
      kUploadData, kUploadDataSize, false);

  helper.RunPreTestSetup();
  helper.AddData(&data);
  ASSERT_TRUE(helper.StartDefaultTest());

  base::RunLoop().RunUntilIdle();
  helper.request().upload_data_stream->AppendChunk(
      kUploadData, kUploadDataSize, false);
  base::RunLoop().RunUntilIdle();
  helper.request().upload_data_stream->AppendChunk(
      kUploadData, kUploadDataSize, true);

  helper.FinishDefaultTest();
  helper.VerifyDataConsumed();

  std::string expected_response;
  expected_response += kUploadData;
  expected_response += kUploadData;
  expected_response += kUploadData;

  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ(expected_response, out.response_data);
}

// Test that a POST without any post data works.
TEST_P(SpdyNetworkTransactionTest, NullPost) {
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);
  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL(kRequestUrl);
  // Create an empty UploadData.
  request.upload_data_stream = NULL;

  // When request.upload_data_stream is NULL for post, content-length is
  // expected to be 0.
  SpdySynStreamIR syn_ir(1);
  syn_ir.set_name_value_block(
      *spdy_util_.ConstructPostHeaderBlock(kRequestUrl, 0));
  syn_ir.set_fin(true);  // No body.
  syn_ir.set_priority(ConvertRequestPriorityToSpdyPriority(
      LOWEST, spdy_util_.spdy_version()));
  scoped_ptr<SpdyFrame> req(framer.SerializeFrame(syn_ir));

  MockWrite writes[] = {
    CreateMockWrite(*req),
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(request, DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a simple POST works.
TEST_P(SpdyNetworkTransactionTest, EmptyPost) {
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);
  // Create an empty UploadDataStream.
  ScopedVector<UploadElementReader> element_readers;
  UploadDataStream stream(element_readers.Pass(), 0);

  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL(kRequestUrl);
  request.upload_data_stream = &stream;

  const uint64 kContentLength = 0;

  SpdySynStreamIR syn_ir(1);
  syn_ir.set_name_value_block(
      *spdy_util_.ConstructPostHeaderBlock(kRequestUrl, kContentLength));
  syn_ir.set_fin(true);  // No body.
  syn_ir.set_priority(ConvertRequestPriorityToSpdyPriority(
      LOWEST, spdy_util_.spdy_version()));
  scoped_ptr<SpdyFrame> req(framer.SerializeFrame(syn_ir));

  MockWrite writes[] = {
    CreateMockWrite(*req),
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads), writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(request, DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// While we're doing a post, the server sends the reply before upload completes.
TEST_P(SpdyNetworkTransactionTest, ResponseBeforePostCompletes) {
  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructChunkedSpdyPost(NULL, 0));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req, 0),
    CreateMockWrite(*body, 3),
  };
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp, 1),
    CreateMockRead(*body, 2),
    MockRead(ASYNC, 0, 4)  // EOF
  };

  // Write the request headers, and read the complete response
  // while still waiting for chunked request data.
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateChunkedPostRequest(),
                                     DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.SetDeterministic();
  helper.RunPreTestSetup();
  helper.AddDeterministicData(&data);

  ASSERT_TRUE(helper.StartDefaultTest());

  // Process the request headers, SYN_REPLY, and response body.
  // The request body is still in flight.
  data.RunFor(3);

  const HttpResponseInfo* response = helper.trans()->GetResponseInfo();
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  // Finish sending the request body.
  helper.request().upload_data_stream->AppendChunk(
      kUploadData, kUploadDataSize, true);
  data.RunFor(2);

  std::string response_body;
  EXPECT_EQ(OK, ReadTransaction(helper.trans(), &response_body));
  EXPECT_EQ(kUploadData, response_body);
  helper.VerifyDataConsumed();
}

// The client upon cancellation tries to send a RST_STREAM frame. The mock
// socket causes the TCP write to return zero. This test checks that the client
// tries to queue up the RST_STREAM frame again.
TEST_P(SpdyNetworkTransactionTest, SocketWriteReturnsZero) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 0, SYNCHRONOUS),
    MockWrite(SYNCHRONOUS, 0, 0, 2),
    CreateMockWrite(*rst.get(), 3, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp.get(), 1, ASYNC),
    MockRead(ASYNC, 0, 0, 4)  // EOF
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.SetDeterministic();
  helper.RunPreTestSetup();
  helper.AddDeterministicData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  data.SetStop(2);
  data.Run();
  helper.ResetTrans();
  data.SetStop(20);
  data.Run();

  helper.VerifyDataConsumed();
}

// Test that the transaction doesn't crash when we don't have a reply.
TEST_P(SpdyNetworkTransactionTest, ResponseWithoutSynReply) {
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads), NULL, 0);
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
}

// Test that the transaction doesn't crash when we get two replies on the same
// stream ID. See http://crbug.com/45639.
TEST_P(SpdyNetworkTransactionTest, ResponseWithTwoSynReplies) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_PROTOCOL_ERROR));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*rst),
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  std::string response_data;
  rv = ReadTransaction(trans, &response_data);
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, rv);

  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, ResetReplyWithTransferEncoding) {
  // Construct the request.
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_PROTOCOL_ERROR));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*rst),
  };

  const char* const headers[] = {
    "transfer-encoding", "chunked"
  };
  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(headers, 1, 1));
  scoped_ptr<SpdyFrame> body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);

  helper.session()->spdy_session_pool()->CloseAllSessions();
  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, ResetPushWithTransferEncoding) {
  // Construct the request.
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_PROTOCOL_ERROR));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*rst),
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  const char* const headers[] = {
    "transfer-encoding", "chunked"
  };
  scoped_ptr<SpdyFrame> push(
      spdy_util_.ConstructSpdyPush(headers, arraysize(headers) / 2,
                                   2, 1, "http://www.google.com/1"));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*push),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  helper.session()->spdy_session_pool()->CloseAllSessions();
  helper.VerifyDataConsumed();
}

TEST_P(SpdyNetworkTransactionTest, CancelledTransaction) {
  // Construct the request.
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = {
    CreateMockWrite(*req),
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp),
    // This following read isn't used by the test, except during the
    // RunUntilIdle() call at the end since the SpdySession survives the
    // HttpNetworkTransaction and still tries to continue Read()'ing.  Any
    // MockRead will do here.
    MockRead(ASYNC, 0, 0)  // EOF
  };

  StaticSocketDataProvider data(reads, arraysize(reads),
                                writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  helper.ResetTrans();  // Cancel the transaction.

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  base::RunLoop().RunUntilIdle();
  helper.VerifyDataNotConsumed();
}

// Verify that the client sends a Rst Frame upon cancelling the stream.
TEST_P(SpdyNetworkTransactionTest, CancelledTransactionSendRst) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite writes[] = {
    CreateMockWrite(*req, 0, SYNCHRONOUS),
    CreateMockWrite(*rst, 2, SYNCHRONOUS),
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp, 1, ASYNC),
    MockRead(ASYNC, 0, 0, 3)  // EOF
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(),
                                     GetParam(), NULL);
  helper.SetDeterministic();
  helper.RunPreTestSetup();
  helper.AddDeterministicData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;

  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  data.SetStop(2);
  data.Run();
  helper.ResetTrans();
  data.SetStop(20);
  data.Run();

  helper.VerifyDataConsumed();
}

// Verify that the client can correctly deal with the user callback attempting
// to start another transaction on a session that is closing down. See
// http://crbug.com/47455
TEST_P(SpdyNetworkTransactionTest, StartTransactionOnReadCallback) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = { CreateMockWrite(*req) };
  MockWrite writes2[] = { CreateMockWrite(*req) };

  // The indicated length of this frame is longer than its actual length. When
  // the session receives an empty frame after this one, it shuts down the
  // session, and calls the read callback with the incomplete data.
  const uint8 kGetBodyFrame2[] = {
    0x00, 0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x07,
    'h', 'e', 'l', 'l', 'o', '!',
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    MockRead(ASYNC, ERR_IO_PENDING, 3),  // Force a pause
    MockRead(ASYNC, reinterpret_cast<const char*>(kGetBodyFrame2),
             arraysize(kGetBodyFrame2), 4),
    MockRead(ASYNC, ERR_IO_PENDING, 5),  // Force a pause
    MockRead(ASYNC, 0, 0, 6),  // EOF
  };
  MockRead reads2[] = {
    CreateMockRead(*resp, 2),
    MockRead(ASYNC, 0, 0, 3),  // EOF
  };

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  DelayedSocketData data2(1, reads2, arraysize(reads2),
                          writes2, arraysize(writes2));

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  helper.AddData(&data2);
  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();

  const int kSize = 3000;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSize));
  rv = trans->Read(
      buf.get(),
      kSize,
      base::Bind(&SpdyNetworkTransactionTest::StartTransactionCallback,
                 helper.session()));
  // This forces an err_IO_pending, which sets the callback.
  data.CompleteRead();
  // This finishes the read.
  data.CompleteRead();
  helper.VerifyDataConsumed();
}

// Verify that the client can correctly deal with the user callback deleting the
// transaction. Failures will usually be valgrind errors. See
// http://crbug.com/46925
TEST_P(SpdyNetworkTransactionTest, DeleteSessionOnReadCallback) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp.get(), 2),
    MockRead(ASYNC, ERR_IO_PENDING, 3),  // Force a pause
    CreateMockRead(*body.get(), 4),
    MockRead(ASYNC, 0, 0, 5),  // EOF
  };

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();

  // Setup a user callback which will delete the session, and clear out the
  // memory holding the stream object. Note that the callback deletes trans.
  const int kSize = 3000;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSize));
  rv = trans->Read(
      buf.get(),
      kSize,
      base::Bind(&SpdyNetworkTransactionTest::DeleteSessionCallback,
                 base::Unretained(&helper)));
  ASSERT_EQ(ERR_IO_PENDING, rv);
  data.CompleteRead();

  // Finish running rest of tasks.
  base::RunLoop().RunUntilIdle();
  helper.VerifyDataConsumed();
}

// Send a spdy request to www.google.com that gets redirected to www.foo.com.
TEST_P(SpdyNetworkTransactionTest, RedirectGetRequest) {
  const SpdyHeaderInfo kSynStartHeader = spdy_util_.MakeSpdyHeader(SYN_STREAM);
  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock("http://www.google.com/"));
  (*headers)["user-agent"] = "";
  (*headers)["accept-encoding"] = "gzip,deflate";
  scoped_ptr<SpdyHeaderBlock> headers2(
      spdy_util_.ConstructGetHeaderBlock("http://www.foo.com/index.php"));
  (*headers2)["user-agent"] = "";
  (*headers2)["accept-encoding"] = "gzip,deflate";

  // Setup writes/reads to www.google.com
  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyFrame(
      kSynStartHeader, headers.Pass()));
  scoped_ptr<SpdyFrame> req2(spdy_util_.ConstructSpdyFrame(
      kSynStartHeader, headers2.Pass()));
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReplyRedirect(1));
  MockWrite writes[] = {
    CreateMockWrite(*req, 1),
  };
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    MockRead(ASYNC, 0, 0, 3)  // EOF
  };

  // Setup writes/reads to www.foo.com
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes2[] = {
    CreateMockWrite(*req2, 1),
  };
  MockRead reads2[] = {
    CreateMockRead(*resp2, 2),
    CreateMockRead(*body2, 3),
    MockRead(ASYNC, 0, 0, 4)  // EOF
  };
  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  OrderedSocketData data2(reads2, arraysize(reads2),
                          writes2, arraysize(writes2));

  // TODO(erikchen): Make test support SPDYSSL, SPDYNPN
  TestDelegate d;
  {
    SpdyURLRequestContext spdy_url_request_context(
        GetParam().protocol,
        false  /* force_spdy_over_ssl*/,
        true  /* force_spdy_always */);
    net::URLRequest r(GURL("http://www.google.com/"),
                      DEFAULT_PRIORITY,
                      &d,
                      &spdy_url_request_context);
    spdy_url_request_context.socket_factory().
        AddSocketDataProvider(&data);
    spdy_url_request_context.socket_factory().
        AddSocketDataProvider(&data2);

    d.set_quit_on_redirect(true);
    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(1, d.received_redirect_count());

    r.FollowDeferredRedirect();
    base::RunLoop().Run();
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, r.status().status());
    std::string contents("hello!");
    EXPECT_EQ(contents, d.data_received());
  }
  EXPECT_TRUE(data.at_read_eof());
  EXPECT_TRUE(data.at_write_eof());
  EXPECT_TRUE(data2.at_read_eof());
  EXPECT_TRUE(data2.at_write_eof());
}

// Send a spdy request to www.google.com. Get a pushed stream that redirects to
// www.foo.com.
TEST_P(SpdyNetworkTransactionTest, RedirectServerPush) {
  const SpdyHeaderInfo kSynStartHeader = spdy_util_.MakeSpdyHeader(SYN_STREAM);

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock("http://www.google.com/"));
  (*headers)["user-agent"] = "";
  (*headers)["accept-encoding"] = "gzip,deflate";

  // Setup writes/reads to www.google.com
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyFrame(kSynStartHeader, headers.Pass()));
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> rep(
      spdy_util_.ConstructSpdyPush(NULL,
                        0,
                        2,
                        1,
                        "http://www.google.com/foo.dat",
                        "301 Moved Permanently",
                        "http://www.foo.com/index.php"));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_CANCEL));
  MockWrite writes[] = {
    CreateMockWrite(*req, 1),
    CreateMockWrite(*rst, 6),
  };
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    CreateMockRead(*rep, 3),
    CreateMockRead(*body, 4),
    MockRead(ASYNC, ERR_IO_PENDING, 5),  // Force a pause
    MockRead(ASYNC, 0, 0, 7)  // EOF
  };

  // Setup writes/reads to www.foo.com
  scoped_ptr<SpdyHeaderBlock> headers2(
      spdy_util_.ConstructGetHeaderBlock("http://www.foo.com/index.php"));
  (*headers2)["user-agent"] = "";
  (*headers2)["accept-encoding"] = "gzip,deflate";
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyFrame(kSynStartHeader, headers2.Pass()));
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes2[] = {
    CreateMockWrite(*req2, 1),
  };
  MockRead reads2[] = {
    CreateMockRead(*resp2, 2),
    CreateMockRead(*body2, 3),
    MockRead(ASYNC, 0, 0, 5)  // EOF
  };
  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  OrderedSocketData data2(reads2, arraysize(reads2),
                          writes2, arraysize(writes2));

  // TODO(erikchen): Make test support SPDYSSL, SPDYNPN
  TestDelegate d;
  TestDelegate d2;
  SpdyURLRequestContext spdy_url_request_context(
      GetParam().protocol,
      false  /* force_spdy_over_ssl*/,
      true  /* force_spdy_always */);
  {
    net::URLRequest r(GURL("http://www.google.com/"),
                      DEFAULT_PRIORITY,
                      &d,
                      &spdy_url_request_context);
    spdy_url_request_context.socket_factory().
        AddSocketDataProvider(&data);

    r.Start();
    base::RunLoop().Run();

    EXPECT_EQ(0, d.received_redirect_count());
    std::string contents("hello!");
    EXPECT_EQ(contents, d.data_received());

    net::URLRequest r2(GURL("http://www.google.com/foo.dat"),
                       DEFAULT_PRIORITY,
                       &d2,
                       &spdy_url_request_context);
    spdy_url_request_context.socket_factory().
        AddSocketDataProvider(&data2);

    d2.set_quit_on_redirect(true);
    r2.Start();
    base::RunLoop().Run();
    EXPECT_EQ(1, d2.received_redirect_count());

    r2.FollowDeferredRedirect();
    base::RunLoop().Run();
    EXPECT_EQ(1, d2.response_started_count());
    EXPECT_FALSE(d2.received_data_before_response());
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, r2.status().status());
    std::string contents2("hello!");
    EXPECT_EQ(contents2, d2.data_received());
  }
  data.CompleteRead();
  data2.CompleteRead();
  EXPECT_TRUE(data.at_read_eof());
  EXPECT_TRUE(data.at_write_eof());
  EXPECT_TRUE(data2.at_read_eof());
  EXPECT_TRUE(data2.at_write_eof());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushSingleDataFrame) {
  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
  };

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame>
      stream2_syn(spdy_util_.ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "http://www.google.com/foo.dat"));
  const char kPushedData[] = "pushed";
  scoped_ptr<SpdyFrame> stream2_body(
      spdy_util_.ConstructSpdyBodyFrame(
          2, kPushedData, strlen(kPushedData), true));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream1_body, 4, SYNCHRONOUS),
    CreateMockRead(*stream2_body, 5),
    MockRead(ASYNC, ERR_IO_PENDING, 6),  // Force a pause
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed");
  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  RunServerPushTest(&data,
                    &response,
                    &response2,
                    expected_push_result);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushBeforeSynReply) {
  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
  };

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame>
      stream2_syn(spdy_util_.ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "http://www.google.com/foo.dat"));
  const char kPushedData[] = "pushed";
  scoped_ptr<SpdyFrame> stream2_body(
      spdy_util_.ConstructSpdyBodyFrame(
          2, kPushedData, strlen(kPushedData), true));
  MockRead reads[] = {
    CreateMockRead(*stream2_syn, 2),
    CreateMockRead(*stream1_reply, 3),
    CreateMockRead(*stream1_body, 4, SYNCHRONOUS),
    CreateMockRead(*stream2_body, 5),
    MockRead(ASYNC, ERR_IO_PENDING, 6),  // Force a pause
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed");
  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  RunServerPushTest(&data,
                    &response,
                    &response2,
                    expected_push_result);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushSingleDataFrame2) {
  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = { CreateMockWrite(*stream1_syn, 1), };

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame>
      stream2_syn(spdy_util_.ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "http://www.google.com/foo.dat"));
  const char kPushedData[] = "pushed";
  scoped_ptr<SpdyFrame> stream2_body(
      spdy_util_.ConstructSpdyBodyFrame(
          2, kPushedData, strlen(kPushedData), true));
  scoped_ptr<SpdyFrame>
      stream1_body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream2_body, 4),
    CreateMockRead(*stream1_body, 5, SYNCHRONOUS),
    MockRead(ASYNC, ERR_IO_PENDING, 6),  // Force a pause
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed");
  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  RunServerPushTest(&data,
                    &response,
                    &response2,
                    expected_push_result);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushServerAborted) {
  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
  };

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame>
      stream2_syn(spdy_util_.ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "http://www.google.com/foo.dat"));
  scoped_ptr<SpdyFrame> stream2_rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_PROTOCOL_ERROR));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream2_rst, 4),
    CreateMockRead(*stream1_body, 5, SYNCHRONOUS),
    MockRead(ASYNC, ERR_IO_PENDING, 6),  // Force a pause
  };

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);

  helper.RunPreTestSetup();
  helper.AddData(&data);

  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  // Verify that we consumed all test data.
  EXPECT_TRUE(data.at_read_eof()) << "Read count: "
                                   << data.read_count()
                                   << " Read index: "
                                   << data.read_index();
  EXPECT_TRUE(data.at_write_eof()) << "Write count: "
                                    << data.write_count()
                                    << " Write index: "
                                    << data.write_index();

  // Verify the SYN_REPLY.
  HttpResponseInfo response = *trans->GetResponseInfo();
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());
}

// Verify that we don't leak streams and that we properly send a reset
// if the server pushes the same stream twice.
TEST_P(SpdyNetworkTransactionTest, ServerPushDuplicate) {
  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> stream3_rst(
      spdy_util_.ConstructSpdyRstStream(4, RST_STREAM_PROTOCOL_ERROR));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
    CreateMockWrite(*stream3_rst, 5),
  };

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame>
      stream2_syn(spdy_util_.ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "http://www.google.com/foo.dat"));
  const char kPushedData[] = "pushed";
  scoped_ptr<SpdyFrame> stream2_body(
      spdy_util_.ConstructSpdyBodyFrame(
          2, kPushedData, strlen(kPushedData), true));
  scoped_ptr<SpdyFrame>
      stream3_syn(spdy_util_.ConstructSpdyPush(NULL,
                                    0,
                                    4,
                                    1,
                                    "http://www.google.com/foo.dat"));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream3_syn, 4),
    CreateMockRead(*stream1_body, 6, SYNCHRONOUS),
    CreateMockRead(*stream2_body, 7),
    MockRead(ASYNC, ERR_IO_PENDING, 8),  // Force a pause
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed");
  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  RunServerPushTest(&data,
                    &response,
                    &response2,
                    expected_push_result);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushMultipleDataFrame) {
  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
  };

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame>
      stream2_syn(spdy_util_.ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "http://www.google.com/foo.dat"));
  static const char kPushedData[] = "pushed my darling hello my baby";
  scoped_ptr<SpdyFrame> stream2_body_base(
      spdy_util_.ConstructSpdyBodyFrame(
          2, kPushedData, strlen(kPushedData), true));
  const size_t kChunkSize = strlen(kPushedData) / 4;
  scoped_ptr<SpdyFrame> stream2_body1(
      new SpdyFrame(stream2_body_base->data(), kChunkSize, false));
  scoped_ptr<SpdyFrame> stream2_body2(
      new SpdyFrame(stream2_body_base->data() + kChunkSize, kChunkSize, false));
  scoped_ptr<SpdyFrame> stream2_body3(
      new SpdyFrame(stream2_body_base->data() + 2 * kChunkSize,
                    kChunkSize, false));
  scoped_ptr<SpdyFrame> stream2_body4(
      new SpdyFrame(stream2_body_base->data() + 3 * kChunkSize,
                    stream2_body_base->size() - 3 * kChunkSize, false));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream2_body1, 4),
    CreateMockRead(*stream2_body2, 5),
    CreateMockRead(*stream2_body3, 6),
    CreateMockRead(*stream2_body4, 7),
    CreateMockRead(*stream1_body, 8, SYNCHRONOUS),
    MockRead(ASYNC, ERR_IO_PENDING, 9),  // Force a pause
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed my darling hello my baby");
  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  RunServerPushTest(&data, &response, &response2, kPushedData);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushMultipleDataFrameInterrupted) {
  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
  };

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame>
      stream2_syn(spdy_util_.ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    1,
                                    "http://www.google.com/foo.dat"));
  static const char kPushedData[] = "pushed my darling hello my baby";
  scoped_ptr<SpdyFrame> stream2_body_base(
      spdy_util_.ConstructSpdyBodyFrame(
          2, kPushedData, strlen(kPushedData), true));
  const size_t kChunkSize = strlen(kPushedData) / 4;
  scoped_ptr<SpdyFrame> stream2_body1(
      new SpdyFrame(stream2_body_base->data(), kChunkSize, false));
  scoped_ptr<SpdyFrame> stream2_body2(
      new SpdyFrame(stream2_body_base->data() + kChunkSize, kChunkSize, false));
  scoped_ptr<SpdyFrame> stream2_body3(
      new SpdyFrame(stream2_body_base->data() + 2 * kChunkSize,
                    kChunkSize, false));
  scoped_ptr<SpdyFrame> stream2_body4(
      new SpdyFrame(stream2_body_base->data() + 3 * kChunkSize,
                    stream2_body_base->size() - 3 * kChunkSize, false));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream2_body1, 4),
    CreateMockRead(*stream2_body2, 5),
    MockRead(ASYNC, ERR_IO_PENDING, 6),  // Force a pause
    CreateMockRead(*stream2_body3, 7),
    CreateMockRead(*stream2_body4, 8),
    CreateMockRead(*stream1_body.get(), 9, SYNCHRONOUS),
    MockRead(ASYNC, ERR_IO_PENDING, 10)  // Force a pause.
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  RunServerPushTest(&data, &response, &response2, kPushedData);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushInvalidAssociatedStreamID0) {
  if (spdy_util_.spdy_version() == SPDY4) {
    // PUSH_PROMISE with stream id 0 is connection-level error.
    // TODO(baranovich): Test session going away.
    return;
  }

  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> stream2_rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_REFUSED_STREAM));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
    CreateMockWrite(*stream2_rst, 4),
  };

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame>
      stream2_syn(spdy_util_.ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    0,
                                    "http://www.google.com/foo.dat"));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream1_body, 4),
    MockRead(ASYNC, ERR_IO_PENDING, 5)  // Force a pause
  };

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);

  helper.RunPreTestSetup();
  helper.AddData(&data);

  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  // Verify that we consumed all test data.
  EXPECT_TRUE(data.at_read_eof()) << "Read count: "
                                   << data.read_count()
                                   << " Read index: "
                                   << data.read_index();
  EXPECT_TRUE(data.at_write_eof()) << "Write count: "
                                    << data.write_count()
                                    << " Write index: "
                                    << data.write_index();

  // Verify the SYN_REPLY.
  HttpResponseInfo response = *trans->GetResponseInfo();
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushInvalidAssociatedStreamID9) {
  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> stream2_rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_INVALID_STREAM));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
    CreateMockWrite(*stream2_rst, 4),
  };

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame>
      stream2_syn(spdy_util_.ConstructSpdyPush(NULL,
                                    0,
                                    2,
                                    9,
                                    "http://www.google.com/foo.dat"));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream1_body, 4),
    MockRead(ASYNC, ERR_IO_PENDING, 5),  // Force a pause
  };

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);

  helper.RunPreTestSetup();
  helper.AddData(&data);

  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  // Verify that we consumed all test data.
  EXPECT_TRUE(data.at_read_eof()) << "Read count: "
                                   << data.read_count()
                                   << " Read index: "
                                   << data.read_index();
  EXPECT_TRUE(data.at_write_eof()) << "Write count: "
                                    << data.write_count()
                                    << " Write index: "
                                    << data.write_index();

  // Verify the SYN_REPLY.
  HttpResponseInfo response = *trans->GetResponseInfo();
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushNoURL) {
  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> stream2_rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_PROTOCOL_ERROR));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
    CreateMockWrite(*stream2_rst, 4),
  };

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyHeaderBlock> incomplete_headers(new SpdyHeaderBlock());
  (*incomplete_headers)["hello"] = "bye";
  (*incomplete_headers)[spdy_util_.GetStatusKey()] = "200 OK";
  (*incomplete_headers)[spdy_util_.GetVersionKey()] = "HTTP/1.1";
  scoped_ptr<SpdyFrame> stream2_syn(spdy_util_.ConstructInitialSpdyPushFrame(
      incomplete_headers.Pass(), 2, 1));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream1_body, 4),
    MockRead(ASYNC, ERR_IO_PENDING, 5)  // Force a pause
  };

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);

  helper.RunPreTestSetup();
  helper.AddData(&data);

  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  // Verify that we consumed all test data.
  EXPECT_TRUE(data.at_read_eof()) << "Read count: "
                                   << data.read_count()
                                   << " Read index: "
                                   << data.read_index();
  EXPECT_TRUE(data.at_write_eof()) << "Write count: "
                                    << data.write_count()
                                    << " Write index: "
                                    << data.write_index();

  // Verify the SYN_REPLY.
  HttpResponseInfo response = *trans->GetResponseInfo();
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());
}

// Verify that various SynReply headers parse correctly through the
// HTTP layer.
TEST_P(SpdyNetworkTransactionTest, SynReplyHeaders) {
  struct SynReplyHeadersTests {
    int num_headers;
    const char* extra_headers[5];
    SpdyHeaderBlock expected_headers;
  } test_cases[] = {
    // This uses a multi-valued cookie header.
    { 2,
      { "cookie", "val1",
        "cookie", "val2",  // will get appended separated by NULL
        NULL
      },
    },
    // This is the minimalist set of headers.
    { 0,
      { NULL },
    },
    // Headers with a comma separated list.
    { 1,
      { "cookie", "val1,val2",
        NULL
      },
    }
  };

  test_cases[0].expected_headers["cookie"] = "val1";
  test_cases[0].expected_headers["cookie"] += '\0';
  test_cases[0].expected_headers["cookie"] += "val2";
  test_cases[0].expected_headers["hello"] = "bye";
  test_cases[0].expected_headers["status"] = "200";

  test_cases[1].expected_headers["hello"] = "bye";
  test_cases[1].expected_headers["status"] = "200";

  test_cases[2].expected_headers["cookie"] = "val1,val2";
  test_cases[2].expected_headers["hello"] = "bye";
  test_cases[2].expected_headers["status"] = "200";

  if (spdy_util_.spdy_version() < SPDY4) {
    // SPDY4/HTTP2 eliminates use of the :version header.
    test_cases[0].expected_headers["version"] = "HTTP/1.1";
    test_cases[1].expected_headers["version"] = "HTTP/1.1";
    test_cases[2].expected_headers["version"] = "HTTP/1.1";
  }

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    scoped_ptr<SpdyFrame> req(
        spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
    MockWrite writes[] = { CreateMockWrite(*req) };

    scoped_ptr<SpdyFrame> resp(
        spdy_util_.ConstructSpdyGetSynReply(test_cases[i].extra_headers,
                                 test_cases[i].num_headers,
                                 1));
    scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
    MockRead reads[] = {
      CreateMockRead(*resp),
      CreateMockRead(*body),
      MockRead(ASYNC, 0, 0)  // EOF
    };

    DelayedSocketData data(1, reads, arraysize(reads),
                           writes, arraysize(writes));
    NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                       BoundNetLog(), GetParam(), NULL);
    helper.RunToCompletion(&data);
    TransactionHelperResult out = helper.output();

    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!", out.response_data);

    scoped_refptr<HttpResponseHeaders> headers = out.response_info.headers;
    EXPECT_TRUE(headers.get() != NULL);
    void* iter = NULL;
    std::string name, value;
    SpdyHeaderBlock header_block;
    while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
      if (header_block[name].empty()) {
        header_block[name] = value;
      } else {
        header_block[name] += '\0';
        header_block[name] += value;
      }
    }
    EXPECT_EQ(test_cases[i].expected_headers, header_block);
  }
}

// Verify that various SynReply headers parse vary fields correctly
// through the HTTP layer, and the response matches the request.
TEST_P(SpdyNetworkTransactionTest, SynReplyHeadersVary) {
  static const SpdyHeaderInfo syn_reply_info = {
    SYN_REPLY,                              // Syn Reply
    1,                                      // Stream ID
    0,                                      // Associated Stream ID
    ConvertRequestPriorityToSpdyPriority(
        LOWEST, spdy_util_.spdy_version()),
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_NONE,                      // Control Flags
    false,                                  // Compressed
    RST_STREAM_INVALID,                     // Status
    NULL,                                   // Data
    0,                                      // Data Length
    DATA_FLAG_NONE                          // Data Flags
  };
  // Modify the following data to change/add test cases:
  struct SynReplyTests {
    const SpdyHeaderInfo* syn_reply;
    bool vary_matches;
    int num_headers[2];
    const char* extra_headers[2][16];
  } test_cases[] = {
    // Test the case of a multi-valued cookie.  When the value is delimited
    // with NUL characters, it needs to be unfolded into multiple headers.
    {
      &syn_reply_info,
      true,
      { 1, 4 },
      { { "cookie",   "val1,val2",
          NULL
        },
        { "vary",     "cookie",
          spdy_util_.GetStatusKey(), "200",
          spdy_util_.GetPathKey(),      "/index.php",
          spdy_util_.GetVersionKey(), "HTTP/1.1",
          NULL
        }
      }
    }, {    // Multiple vary fields.
      &syn_reply_info,
      true,
      { 2, 5 },
      { { "friend",   "barney",
          "enemy",    "snaggletooth",
          NULL
        },
        { "vary",     "friend",
          "vary",     "enemy",
          spdy_util_.GetStatusKey(), "200",
          spdy_util_.GetPathKey(),      "/index.php",
          spdy_util_.GetVersionKey(), "HTTP/1.1",
          NULL
        }
      }
    }, {    // Test a '*' vary field.
      &syn_reply_info,
      false,
      { 1, 4 },
      { { "cookie",   "val1,val2",
          NULL
        },
        { "vary",     "*",
          spdy_util_.GetStatusKey(), "200",
          spdy_util_.GetPathKey(),      "/index.php",
          spdy_util_.GetVersionKey(), "HTTP/1.1",
          NULL
        }
      }
    }, {    // Multiple comma-separated vary fields.
      &syn_reply_info,
      true,
      { 2, 4 },
      { { "friend",   "barney",
          "enemy",    "snaggletooth",
          NULL
        },
        { "vary",     "friend,enemy",
          spdy_util_.GetStatusKey(), "200",
          spdy_util_.GetPathKey(),      "/index.php",
          spdy_util_.GetVersionKey(), "HTTP/1.1",
          NULL
        }
      }
    }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    // Construct the request.
    scoped_ptr<SpdyFrame> frame_req(
        spdy_util_.ConstructSpdyGet(test_cases[i].extra_headers[0],
                                    test_cases[i].num_headers[0],
                                    false, 1, LOWEST, true));

    MockWrite writes[] = {
      CreateMockWrite(*frame_req),
    };

    // Construct the reply.
    scoped_ptr<SpdyFrame> frame_reply(
      spdy_util_.ConstructSpdyFrame(*test_cases[i].syn_reply,
                                    test_cases[i].extra_headers[1],
                                    test_cases[i].num_headers[1],
                                    NULL,
                                    0));

    scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
    MockRead reads[] = {
      CreateMockRead(*frame_reply),
      CreateMockRead(*body),
      MockRead(ASYNC, 0, 0)  // EOF
    };

    // Attach the headers to the request.
    int header_count = test_cases[i].num_headers[0];

    HttpRequestInfo request = CreateGetRequest();
    for (int ct = 0; ct < header_count; ct++) {
      const char* header_key = test_cases[i].extra_headers[0][ct * 2];
      const char* header_value = test_cases[i].extra_headers[0][ct * 2 + 1];
      request.extra_headers.SetHeader(header_key, header_value);
    }

    DelayedSocketData data(1, reads, arraysize(reads),
                           writes, arraysize(writes));
    NormalSpdyTransactionHelper helper(request, DEFAULT_PRIORITY,
                                       BoundNetLog(), GetParam(), NULL);
    helper.RunToCompletion(&data);
    TransactionHelperResult out = helper.output();

    EXPECT_EQ(OK, out.rv) << i;
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line) << i;
    EXPECT_EQ("hello!", out.response_data) << i;

    // Test the response information.
    EXPECT_TRUE(out.response_info.response_time >
                out.response_info.request_time) << i;
    base::TimeDelta test_delay = out.response_info.response_time -
                                 out.response_info.request_time;
    base::TimeDelta min_expected_delay;
    min_expected_delay.FromMilliseconds(10);
    EXPECT_GT(test_delay.InMillisecondsF(),
              min_expected_delay.InMillisecondsF()) << i;
    EXPECT_EQ(out.response_info.vary_data.is_valid(),
              test_cases[i].vary_matches) << i;

    // Check the headers.
    scoped_refptr<HttpResponseHeaders> headers = out.response_info.headers;
    ASSERT_TRUE(headers.get() != NULL) << i;
    void* iter = NULL;
    std::string name, value, lines;
    while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
      lines.append(name);
      lines.append(": ");
      lines.append(value);
      lines.append("\n");
    }

    // Construct the expected header reply string.
    SpdyHeaderBlock reply_headers;
    AppendToHeaderBlock(test_cases[i].extra_headers[1],
                        test_cases[i].num_headers[1],
                        &reply_headers);
    std::string expected_reply =
        spdy_util_.ConstructSpdyReplyString(reply_headers);
    EXPECT_EQ(expected_reply, lines) << i;
  }
}

// Verify that we don't crash on invalid SynReply responses.
TEST_P(SpdyNetworkTransactionTest, InvalidSynReply) {
  const SpdyHeaderInfo kSynStartHeader = {
    SYN_REPLY,              // Kind = SynReply
    1,                      // Stream ID
    0,                      // Associated stream ID
    ConvertRequestPriorityToSpdyPriority(
        LOWEST, spdy_util_.spdy_version()),
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_NONE,      // Control Flags
    false,                  // Compressed
    RST_STREAM_INVALID,     // Status
    NULL,                   // Data
    0,                      // Length
    DATA_FLAG_NONE          // Data Flags
  };

  struct InvalidSynReplyTests {
    int num_headers;
    const char* headers[10];
  } test_cases[] = {
    // SYN_REPLY missing status header
    { 4,
      { "cookie", "val1",
        "cookie", "val2",
        spdy_util_.GetPathKey(), "/index.php",
        spdy_util_.GetVersionKey(), "HTTP/1.1",
        NULL
      },
    },
    // SYN_REPLY missing version header
    { 2,
      { "status", "200",
        spdy_util_.GetPathKey(), "/index.php",
        NULL
      },
    },
    // SYN_REPLY with no headers
    { 0, { NULL }, },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    scoped_ptr<SpdyFrame> req(
        spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
    scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_PROTOCOL_ERROR));
    MockWrite writes[] = {
      CreateMockWrite(*req),
      CreateMockWrite(*rst),
    };

    scoped_ptr<SpdyFrame> resp(
       spdy_util_.ConstructSpdyFrame(kSynStartHeader,
                                     NULL, 0,
                                     test_cases[i].headers,
                                     test_cases[i].num_headers));
    scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
    MockRead reads[] = {
      CreateMockRead(*resp),
      MockRead(ASYNC, 0, 0)  // EOF
    };

    DelayedSocketData data(1, reads, arraysize(reads),
                           writes, arraysize(writes));
    NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                       BoundNetLog(), GetParam(), NULL);
    helper.RunToCompletion(&data);
    TransactionHelperResult out = helper.output();
    EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
  }
}

// Verify that we don't crash on some corrupt frames.
// TODO(jgraettinger): SPDY4 and up treats a header decompression failure as a
// connection error. I'd like to backport this behavior to SPDY3 as well.
TEST_P(SpdyNetworkTransactionTest, CorruptFrameSessionError) {
  if (spdy_util_.spdy_version() >= SPDY4) {
    return;
  }
  // This is the length field that's too short.
  scoped_ptr<SpdyFrame> syn_reply_wrong_length(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);
  size_t right_size =
      (spdy_util_.spdy_version() < SPDY4) ?
      syn_reply_wrong_length->size() - framer.GetControlFrameHeaderSize() :
      syn_reply_wrong_length->size();
  size_t wrong_size = right_size - 4;
  test::SetFrameLength(syn_reply_wrong_length.get(),
                       wrong_size,
                       spdy_util_.spdy_version());

  struct SynReplyTests {
    const SpdyFrame* syn_reply;
  } test_cases[] = {
    { syn_reply_wrong_length.get(), },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    scoped_ptr<SpdyFrame> req(
        spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
    scoped_ptr<SpdyFrame> rst(
        spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_PROTOCOL_ERROR));
    MockWrite writes[] = {
      CreateMockWrite(*req),
      CreateMockWrite(*rst),
    };

    scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
    MockRead reads[] = {
      MockRead(ASYNC, test_cases[i].syn_reply->data(), wrong_size),
      CreateMockRead(*body),
      MockRead(ASYNC, 0, 0)  // EOF
    };

    DelayedSocketData data(1, reads, arraysize(reads),
                           writes, arraysize(writes));
    NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                       BoundNetLog(), GetParam(), NULL);
    helper.RunToCompletion(&data);
    TransactionHelperResult out = helper.output();
    EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
  }
}

// SPDY4 treats a header decompression failure as a connection-level error.
TEST_P(SpdyNetworkTransactionTest, CorruptFrameSessionErrorSpdy4) {
  if (spdy_util_.spdy_version() < SPDY4) {
    return;
  }
  // This is the length field that's too short.
  scoped_ptr<SpdyFrame> syn_reply_wrong_length(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);
  size_t right_size =
      syn_reply_wrong_length->size() - framer.GetControlFrameHeaderSize();
  size_t wrong_size = right_size - 4;
  test::SetFrameLength(syn_reply_wrong_length.get(),
                       wrong_size,
                       spdy_util_.spdy_version());

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> goaway(spdy_util_.ConstructSpdyGoAway(
      0, GOAWAY_COMPRESSION_ERROR, "Framer error: 5 (DECOMPRESS_FAILURE)."));
  MockWrite writes[] = {CreateMockWrite(*req), CreateMockWrite(*goaway)};

  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    MockRead(ASYNC, syn_reply_wrong_length->data(),
             syn_reply_wrong_length->size() - 4),
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SPDY_COMPRESSION_ERROR, out.rv);
}

TEST_P(SpdyNetworkTransactionTest, GoAwayOnDecompressionFailure) {
  if (GetParam().protocol < kProtoSPDY4) {
    // Decompression failures are a stream error in SPDY3 and above.
    return;
  }
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> goaway(spdy_util_.ConstructSpdyGoAway(
      0, GOAWAY_COMPRESSION_ERROR, "Framer error: 5 (DECOMPRESS_FAILURE)."));
  MockWrite writes[] = {CreateMockWrite(*req), CreateMockWrite(*goaway)};

  // Read HEADERS with corrupted payload.
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  memset(resp->data() + 12, 0xff, resp->size() - 12);
  MockRead reads[] = {CreateMockRead(*resp)};

  DelayedSocketData data(1, reads, arraysize(reads), writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(
      CreateGetRequest(), DEFAULT_PRIORITY, BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SPDY_COMPRESSION_ERROR, out.rv);
}

TEST_P(SpdyNetworkTransactionTest, GoAwayOnFrameSizeError) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> goaway(spdy_util_.ConstructSpdyGoAway(
      0, GOAWAY_PROTOCOL_ERROR, "Framer error: 1 (INVALID_CONTROL_FRAME)."));
  MockWrite writes[] = {CreateMockWrite(*req), CreateMockWrite(*goaway)};

  // Read WINDOW_UPDATE with incorrectly-sized payload.
  // TODO(jgraettinger): SpdyFramer signals this as an INVALID_CONTROL_FRAME,
  // which is mapped to a protocol error, and not a frame size error.
  scoped_ptr<SpdyFrame> bad_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, 1));
  test::SetFrameLength(bad_window_update.get(),
                       bad_window_update->size() - 1,
                       spdy_util_.spdy_version());
  MockRead reads[] = {CreateMockRead(*bad_window_update)};

  DelayedSocketData data(1, reads, arraysize(reads), writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(
      CreateGetRequest(), DEFAULT_PRIORITY, BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
}

// Test that we shutdown correctly on write errors.
TEST_P(SpdyNetworkTransactionTest, WriteError) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = {
      // We'll write 10 bytes successfully
      MockWrite(ASYNC, req->data(), 10, 0),
      // Followed by ERROR!
      MockWrite(ASYNC, ERR_FAILED, 1),
      // Session drains and attempts to write a GOAWAY: Another ERROR!
      MockWrite(ASYNC, ERR_FAILED, 2),
  };

  MockRead reads[] = {
      MockRead(ASYNC, 0, 3)  // EOF
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.SetDeterministic();
  helper.RunPreTestSetup();
  helper.AddDeterministicData(&data);
  EXPECT_TRUE(helper.StartDefaultTest());
  data.RunFor(2);
  helper.FinishDefaultTest();
  EXPECT_TRUE(data.at_write_eof());
  EXPECT_TRUE(!data.at_read_eof());
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_FAILED, out.rv);
}

// Test that partial writes work.
TEST_P(SpdyNetworkTransactionTest, PartialWrite) {
  // Chop the SYN_STREAM frame into 5 chunks.
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  const int kChunks = 5;
  scoped_ptr<MockWrite[]> writes(ChopWriteFrame(*req.get(), kChunks));

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(kChunks, reads, arraysize(reads),
                         writes.get(), kChunks);
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// In this test, we enable compression, but get a uncompressed SynReply from
// the server.  Verify that teardown is all clean.
TEST_P(SpdyNetworkTransactionTest, DecompressFailureOnSynReply) {
  if (spdy_util_.spdy_version() >= SPDY4) {
    // HPACK doesn't use deflate compression.
    return;
  }
  scoped_ptr<SpdyFrame> compressed(
      spdy_util_.ConstructSpdyGet(NULL, 0, true, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> goaway(spdy_util_.ConstructSpdyGoAway(
      0, GOAWAY_COMPRESSION_ERROR, "Framer error: 5 (DECOMPRESS_FAILURE)."));
  MockWrite writes[] = {CreateMockWrite(*compressed), CreateMockWrite(*goaway)};

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));
  SpdySessionDependencies* session_deps =
      CreateSpdySessionDependencies(GetParam());
  session_deps->enable_compression = true;
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), session_deps);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SPDY_COMPRESSION_ERROR, out.rv);
  data.Reset();
}

// Test that the NetLog contains good data for a simple GET request.
TEST_P(SpdyNetworkTransactionTest, NetLog) {
  static const char* const kExtraHeaders[] = {
    "user-agent",   "Chrome",
  };
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(kExtraHeaders, 1, false, 1, LOWEST, true));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  CapturingBoundNetLog log;

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequestWithUserAgent(),
                                     DEFAULT_PRIORITY,
                                     log.bound(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // Check that the NetLog was filled reasonably.
  // This test is intentionally non-specific about the exact ordering of the
  // log; instead we just check to make sure that certain events exist, and that
  // they are in the right order.
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);

  EXPECT_LT(0u, entries.size());
  int pos = 0;
  pos = net::ExpectLogContainsSomewhere(entries, 0,
      net::NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST,
      net::NetLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(entries, pos + 1,
      net::NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST,
      net::NetLog::PHASE_END);
  pos = net::ExpectLogContainsSomewhere(entries, pos + 1,
      net::NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS,
      net::NetLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(entries, pos + 1,
      net::NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS,
      net::NetLog::PHASE_END);
  pos = net::ExpectLogContainsSomewhere(entries, pos + 1,
      net::NetLog::TYPE_HTTP_TRANSACTION_READ_BODY,
      net::NetLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(entries, pos + 1,
      net::NetLog::TYPE_HTTP_TRANSACTION_READ_BODY,
      net::NetLog::PHASE_END);

  // Check that we logged all the headers correctly
  pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_SPDY_SESSION_SYN_STREAM,
      net::NetLog::PHASE_NONE);

  base::ListValue* header_list;
  ASSERT_TRUE(entries[pos].params.get());
  ASSERT_TRUE(entries[pos].params->GetList("headers", &header_list));

  std::vector<std::string> expected;
  expected.push_back(std::string(spdy_util_.GetHostKey()) + ": www.google.com");
  expected.push_back(std::string(spdy_util_.GetPathKey()) + ": /");
  expected.push_back(std::string(spdy_util_.GetSchemeKey()) + ": http");
  expected.push_back(std::string(spdy_util_.GetMethodKey()) + ": GET");
  expected.push_back("user-agent: Chrome");
  if (spdy_util_.spdy_version() < SPDY4) {
    // SPDY4/HTTP2 eliminates use of the :version header.
    expected.push_back(std::string(spdy_util_.GetVersionKey()) + ": HTTP/1.1");
  }
  EXPECT_EQ(expected.size(), header_list->GetSize());
  for (std::vector<std::string>::const_iterator it = expected.begin();
       it != expected.end();
       ++it) {
    base::StringValue header(*it);
    EXPECT_NE(header_list->end(), header_list->Find(header)) <<
        "Header not found: " << *it;
  }
}

// Since we buffer the IO from the stream to the renderer, this test verifies
// that when we read out the maximum amount of data (e.g. we received 50 bytes
// on the network, but issued a Read for only 5 of those bytes) that the data
// flow still works correctly.
TEST_P(SpdyNetworkTransactionTest, BufferFull) {
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = { CreateMockWrite(*req) };

  // 2 data frames in a single read.
  scoped_ptr<SpdyFrame> data_frame_1(
      framer.CreateDataFrame(1, "goodby", 6, DATA_FLAG_NONE));
  scoped_ptr<SpdyFrame> data_frame_2(
      framer.CreateDataFrame(1, "e worl", 6, DATA_FLAG_NONE));
  const SpdyFrame* data_frames[2] = {
    data_frame_1.get(),
    data_frame_2.get(),
  };
  char combined_data_frames[100];
  int combined_data_frames_len =
      CombineFrames(data_frames, arraysize(data_frames),
                    combined_data_frames, arraysize(combined_data_frames));
  scoped_ptr<SpdyFrame> last_frame(
      framer.CreateDataFrame(1, "d", 1, DATA_FLAG_FIN));

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp),
    MockRead(ASYNC, ERR_IO_PENDING),  // Force a pause
    MockRead(ASYNC, combined_data_frames, combined_data_frames_len),
    MockRead(ASYNC, ERR_IO_PENDING),  // Force a pause
    CreateMockRead(*last_frame),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));

  TestCompletionCallback callback;

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();
  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TransactionHelperResult out = helper.output();
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 3;
    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSmallReadSize));
    rv = trans->Read(buf.get(), kSmallReadSize, read_callback.callback());
    if (rv == net::ERR_IO_PENDING) {
      data.CompleteRead();
      rv = read_callback.WaitForResult();
    }
    if (rv > 0) {
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      NOTREACHED();
    }
  } while (rv > 0);

  out.response_data.swap(content);

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  base::RunLoop().RunUntilIdle();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("goodbye world", out.response_data);
}

// Verify that basic buffering works; when multiple data frames arrive
// at the same time, ensure that we don't notify a read completion for
// each data frame individually.
TEST_P(SpdyNetworkTransactionTest, Buffering) {
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = { CreateMockWrite(*req) };

  // 4 data frames in a single read.
  scoped_ptr<SpdyFrame> data_frame(
      framer.CreateDataFrame(1, "message", 7, DATA_FLAG_NONE));
  scoped_ptr<SpdyFrame> data_frame_fin(
      framer.CreateDataFrame(1, "message", 7, DATA_FLAG_FIN));
  const SpdyFrame* data_frames[4] = {
    data_frame.get(),
    data_frame.get(),
    data_frame.get(),
    data_frame_fin.get()
  };
  char combined_data_frames[100];
  int combined_data_frames_len =
      CombineFrames(data_frames, arraysize(data_frames),
                    combined_data_frames, arraysize(combined_data_frames));

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp),
    MockRead(ASYNC, ERR_IO_PENDING),  // Force a pause
    MockRead(ASYNC, combined_data_frames, combined_data_frames_len),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TransactionHelperResult out = helper.output();
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  int reads_completed = 0;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 14;
    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSmallReadSize));
    rv = trans->Read(buf.get(), kSmallReadSize, read_callback.callback());
    if (rv == net::ERR_IO_PENDING) {
      data.CompleteRead();
      rv = read_callback.WaitForResult();
    }
    if (rv > 0) {
      EXPECT_EQ(kSmallReadSize, rv);
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      FAIL() << "Unexpected read error: " << rv;
    }
    reads_completed++;
  } while (rv > 0);

  EXPECT_EQ(3, reads_completed);  // Reads are: 14 bytes, 14 bytes, 0 bytes.

  out.response_data.swap(content);

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  base::RunLoop().RunUntilIdle();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("messagemessagemessagemessage", out.response_data);
}

// Verify the case where we buffer data but read it after it has been buffered.
TEST_P(SpdyNetworkTransactionTest, BufferedAll) {
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = { CreateMockWrite(*req) };

  // 5 data frames in a single read.
  SpdySynReplyIR reply_ir(1);
  reply_ir.SetHeader(spdy_util_.GetStatusKey(), "200");
  reply_ir.SetHeader(spdy_util_.GetVersionKey(), "HTTP/1.1");

  scoped_ptr<SpdyFrame> syn_reply(framer.SerializeFrame(reply_ir));
  scoped_ptr<SpdyFrame> data_frame(
      framer.CreateDataFrame(1, "message", 7, DATA_FLAG_NONE));
  scoped_ptr<SpdyFrame> data_frame_fin(
      framer.CreateDataFrame(1, "message", 7, DATA_FLAG_FIN));
  const SpdyFrame* frames[5] = {
    syn_reply.get(),
    data_frame.get(),
    data_frame.get(),
    data_frame.get(),
    data_frame_fin.get()
  };
  char combined_frames[200];
  int combined_frames_len =
      CombineFrames(frames, arraysize(frames),
                    combined_frames, arraysize(combined_frames));

  MockRead reads[] = {
    MockRead(ASYNC, combined_frames, combined_frames_len),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TransactionHelperResult out = helper.output();
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  int reads_completed = 0;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 14;
    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSmallReadSize));
    rv = trans->Read(buf.get(), kSmallReadSize, read_callback.callback());
    if (rv > 0) {
      EXPECT_EQ(kSmallReadSize, rv);
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      FAIL() << "Unexpected read error: " << rv;
    }
    reads_completed++;
  } while (rv > 0);

  EXPECT_EQ(3, reads_completed);

  out.response_data.swap(content);

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  base::RunLoop().RunUntilIdle();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("messagemessagemessagemessage", out.response_data);
}

// Verify the case where we buffer data and close the connection.
TEST_P(SpdyNetworkTransactionTest, BufferedClosed) {
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = { CreateMockWrite(*req) };

  // All data frames in a single read.
  // NOTE: We don't FIN the stream.
  scoped_ptr<SpdyFrame> data_frame(
      framer.CreateDataFrame(1, "message", 7, DATA_FLAG_NONE));
  const SpdyFrame* data_frames[4] = {
    data_frame.get(),
    data_frame.get(),
    data_frame.get(),
    data_frame.get()
  };
  char combined_data_frames[100];
  int combined_data_frames_len =
      CombineFrames(data_frames, arraysize(data_frames),
                    combined_data_frames, arraysize(combined_data_frames));
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp),
    MockRead(ASYNC, ERR_IO_PENDING),  // Force a wait
    MockRead(ASYNC, combined_data_frames, combined_data_frames_len),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;

  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TransactionHelperResult out = helper.output();
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  int reads_completed = 0;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 14;
    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSmallReadSize));
    rv = trans->Read(buf.get(), kSmallReadSize, read_callback.callback());
    if (rv == net::ERR_IO_PENDING) {
      data.CompleteRead();
      rv = read_callback.WaitForResult();
    }
    if (rv > 0) {
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      // This test intentionally closes the connection, and will get an error.
      EXPECT_EQ(ERR_CONNECTION_CLOSED, rv);
      break;
    }
    reads_completed++;
  } while (rv > 0);

  EXPECT_EQ(0, reads_completed);

  out.response_data.swap(content);

  // Flush the MessageLoop while the SpdySessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  base::RunLoop().RunUntilIdle();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();
}

// Verify the case where we buffer data and cancel the transaction.
TEST_P(SpdyNetworkTransactionTest, BufferedCancelled) {
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite writes[] = {CreateMockWrite(*req), CreateMockWrite(*rst)};

  // NOTE: We don't FIN the stream.
  scoped_ptr<SpdyFrame> data_frame(
      framer.CreateDataFrame(1, "message", 7, DATA_FLAG_NONE));

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp),
    MockRead(ASYNC, ERR_IO_PENDING),  // Force a wait
    CreateMockRead(*data_frame),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();
  TestCompletionCallback callback;

  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TransactionHelperResult out = helper.output();
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  const int kReadSize = 256;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kReadSize));
  rv = trans->Read(buf.get(), kReadSize, read_callback.callback());
  ASSERT_EQ(net::ERR_IO_PENDING, rv) << "Unexpected read: " << rv;

  // Complete the read now, which causes buffering to start.
  data.CompleteRead();
  // Destroy the transaction, causing the stream to get cancelled
  // and orphaning the buffered IO task.
  helper.ResetTrans();

  // Flush the MessageLoop; this will cause the buffered IO task
  // to run for the final time.
  base::RunLoop().RunUntilIdle();

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();
}

// Test that if the server requests persistence of settings, that we save
// the settings in the HttpServerProperties.
TEST_P(SpdyNetworkTransactionTest, SettingsSaved) {
  if (spdy_util_.spdy_version() >= SPDY4) {
    // SPDY4 doesn't support flags on individual settings, and
    // has no concept of settings persistence.
    return;
  }
  static const SpdyHeaderInfo kSynReplyInfo = {
    SYN_REPLY,                              // Syn Reply
    1,                                      // Stream ID
    0,                                      // Associated Stream ID
    ConvertRequestPriorityToSpdyPriority(
        LOWEST, spdy_util_.spdy_version()),
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_NONE,                      // Control Flags
    false,                                  // Compressed
    RST_STREAM_INVALID,                     // Status
    NULL,                                   // Data
    0,                                      // Data Length
    DATA_FLAG_NONE                          // Data Flags
  };

  BoundNetLog net_log;
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     net_log, GetParam(), NULL);
  helper.RunPreTestSetup();

  // Verify that no settings exist initially.
  HostPortPair host_port_pair("www.google.com", helper.port());
  SpdySessionPool* spdy_session_pool = helper.session()->spdy_session_pool();
  EXPECT_TRUE(spdy_session_pool->http_server_properties()->GetSpdySettings(
      host_port_pair).empty());

  // Construct the request.
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = { CreateMockWrite(*req) };

  // Construct the reply.
  scoped_ptr<SpdyHeaderBlock> reply_headers(new SpdyHeaderBlock());
  (*reply_headers)[spdy_util_.GetStatusKey()] = "200";
  (*reply_headers)[spdy_util_.GetVersionKey()] = "HTTP/1.1";
  scoped_ptr<SpdyFrame> reply(
    spdy_util_.ConstructSpdyFrame(kSynReplyInfo, reply_headers.Pass()));

  const SpdySettingsIds kSampleId1 = SETTINGS_UPLOAD_BANDWIDTH;
  unsigned int kSampleValue1 = 0x0a0a0a0a;
  const SpdySettingsIds kSampleId2 = SETTINGS_DOWNLOAD_BANDWIDTH;
  unsigned int kSampleValue2 = 0x0b0b0b0b;
  const SpdySettingsIds kSampleId3 = SETTINGS_ROUND_TRIP_TIME;
  unsigned int kSampleValue3 = 0x0c0c0c0c;
  scoped_ptr<SpdyFrame> settings_frame;
  {
    // Construct the SETTINGS frame.
    SettingsMap settings;
    // First add a persisted setting.
    settings[kSampleId1] =
        SettingsFlagsAndValue(SETTINGS_FLAG_PLEASE_PERSIST, kSampleValue1);
    // Next add a non-persisted setting.
    settings[kSampleId2] =
        SettingsFlagsAndValue(SETTINGS_FLAG_NONE, kSampleValue2);
    // Next add another persisted setting.
    settings[kSampleId3] =
        SettingsFlagsAndValue(SETTINGS_FLAG_PLEASE_PERSIST, kSampleValue3);
    settings_frame.reset(spdy_util_.ConstructSpdySettings(settings));
  }

  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*reply),
    CreateMockRead(*body),
    CreateMockRead(*settings_frame),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));
  helper.AddData(&data);
  helper.RunDefaultTest();
  helper.VerifyDataConsumed();
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  {
    // Verify we had two persisted settings.
    const SettingsMap& settings_map =
        spdy_session_pool->http_server_properties()->GetSpdySettings(
            host_port_pair);
    ASSERT_EQ(2u, settings_map.size());

    // Verify the first persisted setting.
    SettingsMap::const_iterator it1 = settings_map.find(kSampleId1);
    EXPECT_TRUE(it1 != settings_map.end());
    SettingsFlagsAndValue flags_and_value1 = it1->second;
    EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1.first);
    EXPECT_EQ(kSampleValue1, flags_and_value1.second);

    // Verify the second persisted setting.
    SettingsMap::const_iterator it3 = settings_map.find(kSampleId3);
    EXPECT_TRUE(it3 != settings_map.end());
    SettingsFlagsAndValue flags_and_value3 = it3->second;
    EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value3.first);
    EXPECT_EQ(kSampleValue3, flags_and_value3.second);
  }
}

// Test that when there are settings saved that they are sent back to the
// server upon session establishment.
TEST_P(SpdyNetworkTransactionTest, SettingsPlayback) {
  // TODO(jgraettinger): Remove settings persistence mechanisms altogether.
  static const SpdyHeaderInfo kSynReplyInfo = {
    SYN_REPLY,                              // Syn Reply
    1,                                      // Stream ID
    0,                                      // Associated Stream ID
    ConvertRequestPriorityToSpdyPriority(
        LOWEST, spdy_util_.spdy_version()),
    kSpdyCredentialSlotUnused,
    CONTROL_FLAG_NONE,                      // Control Flags
    false,                                  // Compressed
    RST_STREAM_INVALID,                     // Status
    NULL,                                   // Data
    0,                                      // Data Length
    DATA_FLAG_NONE                          // Data Flags
  };

  BoundNetLog net_log;
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     net_log, GetParam(), NULL);
  helper.RunPreTestSetup();

  SpdySessionPool* spdy_session_pool = helper.session()->spdy_session_pool();

  SpdySessionPoolPeer pool_peer(spdy_session_pool);
  pool_peer.SetEnableSendingInitialData(true);

  // Verify that no settings exist initially.
  HostPortPair host_port_pair("www.google.com", helper.port());
  EXPECT_TRUE(spdy_session_pool->http_server_properties()->GetSpdySettings(
      host_port_pair).empty());

  const SpdySettingsIds kSampleId1 = SETTINGS_MAX_CONCURRENT_STREAMS;
  unsigned int kSampleValue1 = 0x0a0a0a0a;
  const SpdySettingsIds kSampleId2 = SETTINGS_INITIAL_WINDOW_SIZE;
  unsigned int kSampleValue2 = 0x0c0c0c0c;

  // First add a persisted setting.
  spdy_session_pool->http_server_properties()->SetSpdySetting(
      host_port_pair,
      kSampleId1,
      SETTINGS_FLAG_PLEASE_PERSIST,
      kSampleValue1);

  // Next add another persisted setting.
  spdy_session_pool->http_server_properties()->SetSpdySetting(
      host_port_pair,
      kSampleId2,
      SETTINGS_FLAG_PLEASE_PERSIST,
      kSampleValue2);

  EXPECT_EQ(2u, spdy_session_pool->http_server_properties()->GetSpdySettings(
      host_port_pair).size());

  // Construct the initial SETTINGS frame.
  SettingsMap initial_settings;
  initial_settings[SETTINGS_MAX_CONCURRENT_STREAMS] =
      SettingsFlagsAndValue(SETTINGS_FLAG_NONE, kMaxConcurrentPushedStreams);
  scoped_ptr<SpdyFrame> initial_settings_frame(
      spdy_util_.ConstructSpdySettings(initial_settings));

  // Construct the initial window update.
  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));

  // Construct the persisted SETTINGS frame.
  const SettingsMap& settings =
      spdy_session_pool->http_server_properties()->GetSpdySettings(
          host_port_pair);
  scoped_ptr<SpdyFrame> settings_frame(
      spdy_util_.ConstructSpdySettings(settings));

  // Construct the request.
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));

  std::vector<MockWrite> writes;
  if (GetParam().protocol == kProtoSPDY4) {
    writes.push_back(
        MockWrite(ASYNC,
                  kHttp2ConnectionHeaderPrefix,
                  kHttp2ConnectionHeaderPrefixSize));
  }
  writes.push_back(CreateMockWrite(*initial_settings_frame));
  if (GetParam().protocol >= kProtoSPDY31) {
    writes.push_back(CreateMockWrite(*initial_window_update));
  };
  writes.push_back(CreateMockWrite(*settings_frame));
  writes.push_back(CreateMockWrite(*req));

  // Construct the reply.
  scoped_ptr<SpdyHeaderBlock> reply_headers(new SpdyHeaderBlock());
  (*reply_headers)[spdy_util_.GetStatusKey()] = "200";
  (*reply_headers)[spdy_util_.GetVersionKey()] = "HTTP/1.1";
  scoped_ptr<SpdyFrame> reply(
    spdy_util_.ConstructSpdyFrame(kSynReplyInfo, reply_headers.Pass()));

  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*reply),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(2, reads, arraysize(reads),
                         vector_as_array(&writes), writes.size());
  helper.AddData(&data);
  helper.RunDefaultTest();
  helper.VerifyDataConsumed();
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  {
    // Verify we had two persisted settings.
    const SettingsMap& settings_map =
        spdy_session_pool->http_server_properties()->GetSpdySettings(
            host_port_pair);
    ASSERT_EQ(2u, settings_map.size());

    // Verify the first persisted setting.
    SettingsMap::const_iterator it1 = settings_map.find(kSampleId1);
    EXPECT_TRUE(it1 != settings_map.end());
    SettingsFlagsAndValue flags_and_value1 = it1->second;
    EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1.first);
    EXPECT_EQ(kSampleValue1, flags_and_value1.second);

    // Verify the second persisted setting.
    SettingsMap::const_iterator it2 = settings_map.find(kSampleId2);
    EXPECT_TRUE(it2 != settings_map.end());
    SettingsFlagsAndValue flags_and_value2 = it2->second;
    EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value2.first);
    EXPECT_EQ(kSampleValue2, flags_and_value2.second);
  }
}

TEST_P(SpdyNetworkTransactionTest, GoAwayWithActiveStream) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<SpdyFrame> go_away(spdy_util_.ConstructSpdyGoAway());
  MockRead reads[] = {
    CreateMockRead(*go_away),
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.AddData(&data);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_ABORTED, out.rv);
}

TEST_P(SpdyNetworkTransactionTest, CloseWithActiveStream) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = { CreateMockWrite(*req) };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp),
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));
  BoundNetLog log;
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     log, GetParam(), NULL);
  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  TransactionHelperResult out;
  out.rv = trans->Start(&CreateGetRequest(), callback.callback(), log);

  EXPECT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.rv = ReadTransaction(trans, &out.response_data);
  EXPECT_EQ(ERR_CONNECTION_CLOSED, out.rv);

  // Verify that we consumed all test data.
  helper.VerifyDataConsumed();
}

// Test to make sure we can correctly connect through a proxy.
TEST_P(SpdyNetworkTransactionTest, ProxyConnect) {
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.session_deps().reset(CreateSpdySessionDependencies(
      GetParam(),
      ProxyService::CreateFixedFromPacResult("PROXY myproxy:70")));
  helper.SetSession(make_scoped_refptr(
      SpdySessionDependencies::SpdyCreateSession(helper.session_deps().get())));
  helper.RunPreTestSetup();
  HttpNetworkTransaction* trans = helper.trans();

  const char kConnect443[] = {"CONNECT www.google.com:443 HTTP/1.1\r\n"
                           "Host: www.google.com\r\n"
                           "Proxy-Connection: keep-alive\r\n\r\n"};
  const char kConnect80[] = {"CONNECT www.google.com:80 HTTP/1.1\r\n"
                           "Host: www.google.com\r\n"
                           "Proxy-Connection: keep-alive\r\n\r\n"};
  const char kHTTP200[] = {"HTTP/1.1 200 OK\r\n\r\n"};
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));

  MockWrite writes_SPDYNPN[] = {
    MockWrite(SYNCHRONOUS, kConnect443, arraysize(kConnect443) - 1, 0),
    CreateMockWrite(*req, 2),
  };
  MockRead reads_SPDYNPN[] = {
    MockRead(SYNCHRONOUS, kHTTP200, arraysize(kHTTP200) - 1, 1),
    CreateMockRead(*resp, 3),
    CreateMockRead(*body.get(), 4),
    MockRead(ASYNC, 0, 0, 5),
  };

  MockWrite writes_SPDYSSL[] = {
    MockWrite(SYNCHRONOUS, kConnect80, arraysize(kConnect80) - 1, 0),
    CreateMockWrite(*req, 2),
  };
  MockRead reads_SPDYSSL[] = {
    MockRead(SYNCHRONOUS, kHTTP200, arraysize(kHTTP200) - 1, 1),
    CreateMockRead(*resp, 3),
    CreateMockRead(*body.get(), 4),
    MockRead(ASYNC, 0, 0, 5),
  };

  MockWrite writes_SPDYNOSSL[] = {
    CreateMockWrite(*req, 0),
  };

  MockRead reads_SPDYNOSSL[] = {
    CreateMockRead(*resp, 1),
    CreateMockRead(*body.get(), 2),
    MockRead(ASYNC, 0, 0, 3),
  };

  scoped_ptr<OrderedSocketData> data;
  switch(GetParam().ssl_type) {
    case SPDYNOSSL:
      data.reset(new OrderedSocketData(reads_SPDYNOSSL,
                                       arraysize(reads_SPDYNOSSL),
                                       writes_SPDYNOSSL,
                                       arraysize(writes_SPDYNOSSL)));
      break;
    case SPDYSSL:
      data.reset(new OrderedSocketData(reads_SPDYSSL,
                                       arraysize(reads_SPDYSSL),
                                       writes_SPDYSSL,
                                       arraysize(writes_SPDYSSL)));
      break;
    case SPDYNPN:
      data.reset(new OrderedSocketData(reads_SPDYNPN,
                                       arraysize(reads_SPDYNPN),
                                       writes_SPDYNPN,
                                       arraysize(writes_SPDYNPN)));
      break;
    default:
      NOTREACHED();
  }

  helper.AddData(data.get());
  TestCompletionCallback callback;

  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(0, rv);

  // Verify the SYN_REPLY.
  HttpResponseInfo response = *trans->GetResponseInfo();
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans, &response_data));
  EXPECT_EQ("hello!", response_data);
  helper.VerifyDataConsumed();
}

// Test to make sure we can correctly connect through a proxy to www.google.com,
// if there already exists a direct spdy connection to www.google.com. See
// http://crbug.com/49874
TEST_P(SpdyNetworkTransactionTest, DirectConnectProxyReconnect) {
  // When setting up the first transaction, we store the SpdySessionPool so that
  // we can use the same pool in the second transaction.
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);

  // Use a proxy service which returns a proxy fallback list from DIRECT to
  // myproxy:70. For this test there will be no fallback, so it is equivalent
  // to simply DIRECT. The reason for appending the second proxy is to verify
  // that the session pool key used does is just "DIRECT".
  helper.session_deps().reset(CreateSpdySessionDependencies(
      GetParam(),
      ProxyService::CreateFixedFromPacResult("DIRECT; PROXY myproxy:70")));
  helper.SetSession(make_scoped_refptr(
      SpdySessionDependencies::SpdyCreateSession(helper.session_deps().get())));

  SpdySessionPool* spdy_session_pool = helper.session()->spdy_session_pool();
  helper.RunPreTestSetup();

  // Construct and send a simple GET request.
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = {
    CreateMockWrite(*req, 1),
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    CreateMockRead(*body, 3),
    MockRead(ASYNC, ERR_IO_PENDING, 4),  // Force a pause
    MockRead(ASYNC, 0, 5)  // EOF
  };
  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  TransactionHelperResult out;
  out.rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());

  EXPECT_EQ(out.rv, ERR_IO_PENDING);
  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers.get() != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.rv = ReadTransaction(trans, &out.response_data);
  EXPECT_EQ(OK, out.rv);
  out.status_line = response->headers->GetStatusLine();
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // Check that the SpdySession is still in the SpdySessionPool.
  HostPortPair host_port_pair("www.google.com", helper.port());
  SpdySessionKey session_pool_key_direct(
      host_port_pair, ProxyServer::Direct(), PRIVACY_MODE_DISABLED);
  EXPECT_TRUE(HasSpdySession(spdy_session_pool, session_pool_key_direct));
  SpdySessionKey session_pool_key_proxy(
      host_port_pair,
      ProxyServer::FromURI("www.foo.com", ProxyServer::SCHEME_HTTP),
      PRIVACY_MODE_DISABLED);
  EXPECT_FALSE(HasSpdySession(spdy_session_pool, session_pool_key_proxy));

  // Set up data for the proxy connection.
  const char kConnect443[] = {"CONNECT www.google.com:443 HTTP/1.1\r\n"
                           "Host: www.google.com\r\n"
                           "Proxy-Connection: keep-alive\r\n\r\n"};
  const char kConnect80[] = {"CONNECT www.google.com:80 HTTP/1.1\r\n"
                           "Host: www.google.com\r\n"
                           "Proxy-Connection: keep-alive\r\n\r\n"};
  const char kHTTP200[] = {"HTTP/1.1 200 OK\r\n\r\n"};
  scoped_ptr<SpdyFrame> req2(spdy_util_.ConstructSpdyGet(
      "http://www.google.com/foo.dat", false, 1, LOWEST));
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(1, true));

  MockWrite writes_SPDYNPN[] = {
    MockWrite(SYNCHRONOUS, kConnect443, arraysize(kConnect443) - 1, 0),
    CreateMockWrite(*req2, 2),
  };
  MockRead reads_SPDYNPN[] = {
    MockRead(SYNCHRONOUS, kHTTP200, arraysize(kHTTP200) - 1, 1),
    CreateMockRead(*resp2, 3),
    CreateMockRead(*body2, 4),
    MockRead(ASYNC, 0, 5)  // EOF
  };

  MockWrite writes_SPDYNOSSL[] = {
    CreateMockWrite(*req2, 0),
  };
  MockRead reads_SPDYNOSSL[] = {
    CreateMockRead(*resp2, 1),
    CreateMockRead(*body2, 2),
    MockRead(ASYNC, 0, 3)  // EOF
  };

  MockWrite writes_SPDYSSL[] = {
    MockWrite(SYNCHRONOUS, kConnect80, arraysize(kConnect80) - 1, 0),
    CreateMockWrite(*req2, 2),
  };
  MockRead reads_SPDYSSL[] = {
    MockRead(SYNCHRONOUS, kHTTP200, arraysize(kHTTP200) - 1, 1),
    CreateMockRead(*resp2, 3),
    CreateMockRead(*body2, 4),
    MockRead(ASYNC, 0, 0, 5),
  };

  scoped_ptr<OrderedSocketData> data_proxy;
  switch(GetParam().ssl_type) {
    case SPDYNPN:
      data_proxy.reset(new OrderedSocketData(reads_SPDYNPN,
                                             arraysize(reads_SPDYNPN),
                                             writes_SPDYNPN,
                                             arraysize(writes_SPDYNPN)));
      break;
    case SPDYNOSSL:
      data_proxy.reset(new OrderedSocketData(reads_SPDYNOSSL,
                                             arraysize(reads_SPDYNOSSL),
                                             writes_SPDYNOSSL,
                                             arraysize(writes_SPDYNOSSL)));
      break;
    case SPDYSSL:
      data_proxy.reset(new OrderedSocketData(reads_SPDYSSL,
                                             arraysize(reads_SPDYSSL),
                                             writes_SPDYSSL,
                                             arraysize(writes_SPDYSSL)));
      break;
    default:
      NOTREACHED();
  }

  // Create another request to www.google.com, but this time through a proxy.
  HttpRequestInfo request_proxy;
  request_proxy.method = "GET";
  request_proxy.url = GURL("http://www.google.com/foo.dat");
  request_proxy.load_flags = 0;
  scoped_ptr<SpdySessionDependencies> ssd_proxy(
      CreateSpdySessionDependencies(GetParam()));
  // Ensure that this transaction uses the same SpdySessionPool.
  scoped_refptr<HttpNetworkSession> session_proxy(
      SpdySessionDependencies::SpdyCreateSession(ssd_proxy.get()));
  NormalSpdyTransactionHelper helper_proxy(request_proxy, DEFAULT_PRIORITY,
                                           BoundNetLog(), GetParam(), NULL);
  HttpNetworkSessionPeer session_peer(session_proxy);
  scoped_ptr<net::ProxyService> proxy_service(
          ProxyService::CreateFixedFromPacResult("PROXY myproxy:70"));
  session_peer.SetProxyService(proxy_service.get());
  helper_proxy.session_deps().swap(ssd_proxy);
  helper_proxy.SetSession(session_proxy);
  helper_proxy.RunPreTestSetup();
  helper_proxy.AddData(data_proxy.get());

  HttpNetworkTransaction* trans_proxy = helper_proxy.trans();
  TestCompletionCallback callback_proxy;
  int rv = trans_proxy->Start(
      &request_proxy, callback_proxy.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback_proxy.WaitForResult();
  EXPECT_EQ(0, rv);

  HttpResponseInfo response_proxy = *trans_proxy->GetResponseInfo();
  EXPECT_TRUE(response_proxy.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response_proxy.headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans_proxy, &response_data));
  EXPECT_EQ("hello!", response_data);

  data.CompleteRead();
  helper_proxy.VerifyDataConsumed();
}

// When we get a TCP-level RST, we need to retry a HttpNetworkTransaction
// on a new connection, if the connection was previously known to be good.
// This can happen when a server reboots without saying goodbye, or when
// we're behind a NAT that masked the RST.
TEST_P(SpdyNetworkTransactionTest, VerifyRetryOnConnectionReset) {
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, ERR_IO_PENDING),
    MockRead(ASYNC, ERR_CONNECTION_RESET),
  };

  MockRead reads2[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  // This test has a couple of variants.
  enum {
    // Induce the RST while waiting for our transaction to send.
    VARIANT_RST_DURING_SEND_COMPLETION,
    // Induce the RST while waiting for our transaction to read.
    // In this case, the send completed - everything copied into the SNDBUF.
    VARIANT_RST_DURING_READ_COMPLETION
  };

  for (int variant = VARIANT_RST_DURING_SEND_COMPLETION;
       variant <= VARIANT_RST_DURING_READ_COMPLETION;
       ++variant) {
    DelayedSocketData data1(1, reads, arraysize(reads), NULL, 0);

    DelayedSocketData data2(1, reads2, arraysize(reads2), NULL, 0);

    NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                       BoundNetLog(), GetParam(), NULL);
    helper.AddData(&data1);
    helper.AddData(&data2);
    helper.RunPreTestSetup();

    for (int i = 0; i < 2; ++i) {
      scoped_ptr<HttpNetworkTransaction> trans(
          new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));

      TestCompletionCallback callback;
      int rv = trans->Start(
          &helper.request(), callback.callback(), BoundNetLog());
      EXPECT_EQ(ERR_IO_PENDING, rv);
      // On the second transaction, we trigger the RST.
      if (i == 1) {
        if (variant == VARIANT_RST_DURING_READ_COMPLETION) {
          // Writes to the socket complete asynchronously on SPDY by running
          // through the message loop.  Complete the write here.
          base::RunLoop().RunUntilIdle();
        }

        // Now schedule the ERR_CONNECTION_RESET.
        EXPECT_EQ(3u, data1.read_index());
        data1.CompleteRead();
        EXPECT_EQ(4u, data1.read_index());
      }
      rv = callback.WaitForResult();
      EXPECT_EQ(OK, rv);

      const HttpResponseInfo* response = trans->GetResponseInfo();
      ASSERT_TRUE(response != NULL);
      EXPECT_TRUE(response->headers.get() != NULL);
      EXPECT_TRUE(response->was_fetched_via_spdy);
      std::string response_data;
      rv = ReadTransaction(trans.get(), &response_data);
      EXPECT_EQ(OK, rv);
      EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
      EXPECT_EQ("hello!", response_data);
    }

    helper.VerifyDataConsumed();
  }
}

// Test that turning SPDY on and off works properly.
TEST_P(SpdyNetworkTransactionTest, SpdyOnOffToggle) {
  HttpStreamFactory::set_spdy_enabled(true);
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite spdy_writes[] = { CreateMockWrite(*req) };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, spdy_reads, arraysize(spdy_reads),
                         spdy_writes, arraysize(spdy_writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  net::HttpStreamFactory::set_spdy_enabled(false);
  MockRead http_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello from http"),
    MockRead(SYNCHRONOUS, OK),
  };
  DelayedSocketData data2(1, http_reads, arraysize(http_reads), NULL, 0);
  NormalSpdyTransactionHelper helper2(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper2.SetSpdyDisabled();
  helper2.RunToCompletion(&data2);
  TransactionHelperResult out2 = helper2.output();
  EXPECT_EQ(OK, out2.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out2.status_line);
  EXPECT_EQ("hello from http", out2.response_data);

  net::HttpStreamFactory::set_spdy_enabled(true);
}

// Tests that Basic authentication works over SPDY
TEST_P(SpdyNetworkTransactionTest, SpdyBasicAuth) {
  net::HttpStreamFactory::set_spdy_enabled(true);

  // The first request will be a bare GET, the second request will be a
  // GET with an Authorization header.
  scoped_ptr<SpdyFrame> req_get(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  const char* const kExtraAuthorizationHeaders[] = {
    "authorization", "Basic Zm9vOmJhcg=="
  };
  scoped_ptr<SpdyFrame> req_get_authorization(
      spdy_util_.ConstructSpdyGet(kExtraAuthorizationHeaders,
                                  arraysize(kExtraAuthorizationHeaders) / 2,
                                  false, 3, LOWEST, true));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*req_get, 1),
    CreateMockWrite(*req_get_authorization, 4),
  };

  // The first response is a 401 authentication challenge, and the second
  // response will be a 200 response since the second request includes a valid
  // Authorization header.
  const char* const kExtraAuthenticationHeaders[] = {
    "www-authenticate",
    "Basic realm=\"MyRealm\""
  };
  scoped_ptr<SpdyFrame> resp_authentication(
      spdy_util_.ConstructSpdySynReplyError(
          "401 Authentication Required",
          kExtraAuthenticationHeaders,
          arraysize(kExtraAuthenticationHeaders) / 2,
          1));
  scoped_ptr<SpdyFrame> body_authentication(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> resp_data(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body_data(spdy_util_.ConstructSpdyBodyFrame(3, true));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp_authentication, 2),
    CreateMockRead(*body_authentication, 3),
    CreateMockRead(*resp_data, 5),
    CreateMockRead(*body_data, 6),
    MockRead(ASYNC, 0, 7),
  };

  OrderedSocketData data(spdy_reads, arraysize(spdy_reads),
                         spdy_writes, arraysize(spdy_writes));
  HttpRequestInfo request(CreateGetRequest());
  BoundNetLog net_log;
  NormalSpdyTransactionHelper helper(request, DEFAULT_PRIORITY,
                                     net_log, GetParam(), NULL);

  helper.RunPreTestSetup();
  helper.AddData(&data);
  HttpNetworkTransaction* trans = helper.trans();
  TestCompletionCallback callback;
  const int rv_start = trans->Start(&request, callback.callback(), net_log);
  EXPECT_EQ(ERR_IO_PENDING, rv_start);
  const int rv_start_complete = callback.WaitForResult();
  EXPECT_EQ(OK, rv_start_complete);

  // Make sure the response has an auth challenge.
  const HttpResponseInfo* const response_start = trans->GetResponseInfo();
  ASSERT_TRUE(response_start != NULL);
  ASSERT_TRUE(response_start->headers.get() != NULL);
  EXPECT_EQ(401, response_start->headers->response_code());
  EXPECT_TRUE(response_start->was_fetched_via_spdy);
  AuthChallengeInfo* auth_challenge = response_start->auth_challenge.get();
  ASSERT_TRUE(auth_challenge != NULL);
  EXPECT_FALSE(auth_challenge->is_proxy);
  EXPECT_EQ("basic", auth_challenge->scheme);
  EXPECT_EQ("MyRealm", auth_challenge->realm);

  // Restart with a username/password.
  AuthCredentials credentials(base::ASCIIToUTF16("foo"),
                              base::ASCIIToUTF16("bar"));
  TestCompletionCallback callback_restart;
  const int rv_restart = trans->RestartWithAuth(
      credentials, callback_restart.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv_restart);
  const int rv_restart_complete = callback_restart.WaitForResult();
  EXPECT_EQ(OK, rv_restart_complete);
  // TODO(cbentzel): This is actually the same response object as before, but
  // data has changed.
  const HttpResponseInfo* const response_restart = trans->GetResponseInfo();
  ASSERT_TRUE(response_restart != NULL);
  ASSERT_TRUE(response_restart->headers.get() != NULL);
  EXPECT_EQ(200, response_restart->headers->response_code());
  EXPECT_TRUE(response_restart->auth_challenge.get() == NULL);
}

TEST_P(SpdyNetworkTransactionTest, ServerPushWithHeaders) {
  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 1),
  };

  scoped_ptr<SpdyHeaderBlock> initial_headers(new SpdyHeaderBlock());
  spdy_util_.AddUrlToHeaderBlock(
      "http://www.google.com/foo.dat", initial_headers.get());
  scoped_ptr<SpdyFrame> stream2_syn(
      spdy_util_.ConstructInitialSpdyPushFrame(initial_headers.Pass(), 2, 1));

  scoped_ptr<SpdyHeaderBlock> late_headers(new SpdyHeaderBlock());
  (*late_headers)["hello"] = "bye";
  (*late_headers)[spdy_util_.GetStatusKey()] = "200";
  (*late_headers)[spdy_util_.GetVersionKey()] = "HTTP/1.1";
  scoped_ptr<SpdyFrame> stream2_headers(
      spdy_util_.ConstructSpdyControlFrame(late_headers.Pass(),
                                           false,
                                           2,
                                           LOWEST,
                                           HEADERS,
                                           CONTROL_FLAG_NONE,
                                           0));

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  const char kPushedData[] = "pushed";
  scoped_ptr<SpdyFrame> stream2_body(
      spdy_util_.ConstructSpdyBodyFrame(
          2, kPushedData, strlen(kPushedData), true));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 2),
    CreateMockRead(*stream2_syn, 3),
    CreateMockRead(*stream2_headers, 4),
    CreateMockRead(*stream1_body, 5, SYNCHRONOUS),
    CreateMockRead(*stream2_body, 5),
    MockRead(ASYNC, ERR_IO_PENDING, 7),  // Force a pause
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed");
  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  RunServerPushTest(&data,
                    &response,
                    &response2,
                    expected_push_result);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushClaimBeforeHeaders) {
  // We push a stream and attempt to claim it before the headers come down.
  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyHeaderBlock> initial_headers(new SpdyHeaderBlock());
  spdy_util_.AddUrlToHeaderBlock(
      "http://www.google.com/foo.dat", initial_headers.get());
  scoped_ptr<SpdyFrame> stream2_syn(
      spdy_util_.ConstructInitialSpdyPushFrame(initial_headers.Pass(), 2, 1));

  scoped_ptr<SpdyHeaderBlock> late_headers(new SpdyHeaderBlock());
  (*late_headers)["hello"] = "bye";
  (*late_headers)[spdy_util_.GetStatusKey()] = "200";
  (*late_headers)[spdy_util_.GetVersionKey()] = "HTTP/1.1";
  scoped_ptr<SpdyFrame> stream2_headers(
      spdy_util_.ConstructSpdyControlFrame(late_headers.Pass(),
                                           false,
                                           2,
                                           LOWEST,
                                           HEADERS,
                                           CONTROL_FLAG_NONE,
                                           0));

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  const char kPushedData[] = "pushed";
  scoped_ptr<SpdyFrame> stream2_body(
      spdy_util_.ConstructSpdyBodyFrame(
          2, kPushedData, strlen(kPushedData), true));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 1),
    CreateMockRead(*stream2_syn, 2),
    CreateMockRead(*stream1_body, 3),
    CreateMockRead(*stream2_headers, 4),
    CreateMockRead(*stream2_body, 5),
    MockRead(ASYNC, 0, 6),  // EOF
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed");
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.SetDeterministic();
  helper.AddDeterministicData(&data);
  helper.RunPreTestSetup();

  HttpNetworkTransaction* trans = helper.trans();

  // Run until we've received the primary SYN_STREAM, the pushed SYN_STREAM,
  // and the body of the primary stream, but before we've received the HEADERS
  // for the pushed stream.
  data.SetStop(3);

  // Start the transaction.
  TestCompletionCallback callback;
  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  data.Run();
  rv = callback.WaitForResult();
  EXPECT_EQ(0, rv);

  // Request the pushed path.  At this point, we've received the push, but the
  // headers are not yet complete.
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
  rv = trans2->Start(
      &CreateGetPushRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  data.RunFor(3);
  base::RunLoop().RunUntilIdle();

  // Read the server push body.
  std::string result2;
  ReadResult(trans2.get(), &data, &result2);
  // Read the response body.
  std::string result;
  ReadResult(trans, &data, &result);

  // Verify that the received push data is same as the expected push data.
  EXPECT_EQ(result2.compare(expected_push_result), 0)
      << "Received data: "
      << result2
      << "||||| Expected data: "
      << expected_push_result;

  // Verify the SYN_REPLY.
  // Copy the response info, because trans goes away.
  response = *trans->GetResponseInfo();
  response2 = *trans2->GetResponseInfo();

  VerifyStreamsClosed(helper);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());

  // Read the final EOF (which will close the session)
  data.RunFor(1);

  // Verify that we consumed all test data.
  EXPECT_TRUE(data.at_read_eof());
  EXPECT_TRUE(data.at_write_eof());
}

// TODO(baranovich): HTTP 2 does not allow multiple HEADERS frames
TEST_P(SpdyNetworkTransactionTest, ServerPushWithTwoHeaderFrames) {
  // We push a stream and attempt to claim it before the headers come down.
  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyHeaderBlock> initial_headers(new SpdyHeaderBlock());
  if (spdy_util_.spdy_version() < SPDY4) {
    // In SPDY4 PUSH_PROMISE headers won't show up in the response headers.
    (*initial_headers)["alpha"] = "beta";
  }
  spdy_util_.AddUrlToHeaderBlock(
      "http://www.google.com/foo.dat", initial_headers.get());
  scoped_ptr<SpdyFrame> stream2_syn(
      spdy_util_.ConstructInitialSpdyPushFrame(initial_headers.Pass(), 2, 1));

  scoped_ptr<SpdyHeaderBlock> middle_headers(new SpdyHeaderBlock());
  (*middle_headers)["hello"] = "bye";
  scoped_ptr<SpdyFrame> stream2_headers1(
      spdy_util_.ConstructSpdyControlFrame(middle_headers.Pass(),
                                           false,
                                           2,
                                           LOWEST,
                                           HEADERS,
                                           CONTROL_FLAG_NONE,
                                           0));

  scoped_ptr<SpdyHeaderBlock> late_headers(new SpdyHeaderBlock());
  (*late_headers)[spdy_util_.GetStatusKey()] = "200";
  if (spdy_util_.spdy_version() < SPDY4) {
    // SPDY4/HTTP2 eliminates use of the :version header.
    (*late_headers)[spdy_util_.GetVersionKey()] = "HTTP/1.1";
  }
  scoped_ptr<SpdyFrame> stream2_headers2(
      spdy_util_.ConstructSpdyControlFrame(late_headers.Pass(),
                                           false,
                                           2,
                                           LOWEST,
                                           HEADERS,
                                           CONTROL_FLAG_NONE,
                                           0));

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  const char kPushedData[] = "pushed";
  scoped_ptr<SpdyFrame> stream2_body(
      spdy_util_.ConstructSpdyBodyFrame(
          2, kPushedData, strlen(kPushedData), true));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 1),
    CreateMockRead(*stream2_syn, 2),
    CreateMockRead(*stream1_body, 3),
    CreateMockRead(*stream2_headers1, 4),
    CreateMockRead(*stream2_headers2, 5),
    CreateMockRead(*stream2_body, 6),
    MockRead(ASYNC, 0, 7),  // EOF
  };

  HttpResponseInfo response;
  HttpResponseInfo response2;
  std::string expected_push_result("pushed");
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.SetDeterministic();
  helper.AddDeterministicData(&data);
  helper.RunPreTestSetup();

  HttpNetworkTransaction* trans = helper.trans();

  // Run until we've received the primary SYN_STREAM, the pushed SYN_STREAM,
  // the first HEADERS frame, and the body of the primary stream, but before
  // we've received the final HEADERS for the pushed stream.
  data.SetStop(4);

  // Start the transaction.
  TestCompletionCallback callback;
  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  data.Run();
  rv = callback.WaitForResult();
  EXPECT_EQ(0, rv);

  // Request the pushed path.  At this point, we've received the push, but the
  // headers are not yet complete.
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
  rv = trans2->Start(
      &CreateGetPushRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  data.RunFor(3);
  base::RunLoop().RunUntilIdle();

  // Read the server push body.
  std::string result2;
  ReadResult(trans2.get(), &data, &result2);
  // Read the response body.
  std::string result;
  ReadResult(trans, &data, &result);

  // Verify that the received push data is same as the expected push data.
  EXPECT_EQ(expected_push_result, result2);

  // Verify the SYN_REPLY.
  // Copy the response info, because trans goes away.
  response = *trans->GetResponseInfo();
  response2 = *trans2->GetResponseInfo();

  VerifyStreamsClosed(helper);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Verify the pushed stream.
  EXPECT_TRUE(response2.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());

  // Verify we got all the headers from all header blocks.
  if (spdy_util_.spdy_version() < SPDY4)
    EXPECT_TRUE(response2.headers->HasHeaderValue("alpha", "beta"));
  EXPECT_TRUE(response2.headers->HasHeaderValue("hello", "bye"));
  EXPECT_TRUE(response2.headers->HasHeaderValue("status", "200"));

  // Read the final EOF (which will close the session)
  data.RunFor(1);

  // Verify that we consumed all test data.
  EXPECT_TRUE(data.at_read_eof());
  EXPECT_TRUE(data.at_write_eof());
}

TEST_P(SpdyNetworkTransactionTest, ServerPushWithNoStatusHeaderFrames) {
  // We push a stream and attempt to claim it before the headers come down.
  scoped_ptr<SpdyFrame> stream1_syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*stream1_syn, 0, SYNCHRONOUS),
  };

  scoped_ptr<SpdyHeaderBlock> initial_headers(new SpdyHeaderBlock());
  spdy_util_.AddUrlToHeaderBlock(
      "http://www.google.com/foo.dat", initial_headers.get());
  scoped_ptr<SpdyFrame> stream2_syn(
      spdy_util_.ConstructInitialSpdyPushFrame(initial_headers.Pass(), 2, 1));

  scoped_ptr<SpdyHeaderBlock> middle_headers(new SpdyHeaderBlock());
  (*middle_headers)["hello"] = "bye";
  scoped_ptr<SpdyFrame> stream2_headers1(
      spdy_util_.ConstructSpdyControlFrame(middle_headers.Pass(),
                                           false,
                                           2,
                                           LOWEST,
                                           HEADERS,
                                           CONTROL_FLAG_NONE,
                                           0));

  scoped_ptr<SpdyFrame>
      stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  const char kPushedData[] = "pushed";
  scoped_ptr<SpdyFrame> stream2_body(
      spdy_util_.ConstructSpdyBodyFrame(
          2, kPushedData, strlen(kPushedData), true));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply, 1),
    CreateMockRead(*stream2_syn, 2),
    CreateMockRead(*stream1_body, 3),
    CreateMockRead(*stream2_headers1, 4),
    CreateMockRead(*stream2_body, 5),
    MockRead(ASYNC, 0, 6),  // EOF
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.SetDeterministic();
  helper.AddDeterministicData(&data);
  helper.RunPreTestSetup();

  HttpNetworkTransaction* trans = helper.trans();

  // Run until we've received the primary SYN_STREAM, the pushed SYN_STREAM,
  // the first HEADERS frame, and the body of the primary stream, but before
  // we've received the final HEADERS for the pushed stream.
  data.SetStop(4);

  // Start the transaction.
  TestCompletionCallback callback;
  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  data.Run();
  rv = callback.WaitForResult();
  EXPECT_EQ(0, rv);

  // Request the pushed path.  At this point, we've received the push, but the
  // headers are not yet complete.
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, helper.session().get()));
  rv = trans2->Start(
      &CreateGetPushRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  data.RunFor(2);
  base::RunLoop().RunUntilIdle();

  // Read the server push body.
  std::string result2;
  ReadResult(trans2.get(), &data, &result2);
  // Read the response body.
  std::string result;
  ReadResult(trans, &data, &result);
  EXPECT_EQ("hello!", result);

  // Verify that we haven't received any push data.
  EXPECT_EQ("", result2);

  // Verify the SYN_REPLY.
  // Copy the response info, because trans goes away.
  HttpResponseInfo response = *trans->GetResponseInfo();
  ASSERT_TRUE(trans2->GetResponseInfo() == NULL);

  VerifyStreamsClosed(helper);

  // Verify the SYN_REPLY.
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());

  // Read the final EOF (which will close the session).
  data.RunFor(1);

  // Verify that we consumed all test data.
  EXPECT_TRUE(data.at_read_eof());
  EXPECT_TRUE(data.at_write_eof());
}

TEST_P(SpdyNetworkTransactionTest, SynReplyWithHeaders) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_PROTOCOL_ERROR));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*rst),
 };

  scoped_ptr<SpdyHeaderBlock> initial_headers(new SpdyHeaderBlock());
  (*initial_headers)[spdy_util_.GetStatusKey()] = "200 OK";
  (*initial_headers)[spdy_util_.GetVersionKey()] = "HTTP/1.1";
  scoped_ptr<SpdyFrame> stream1_reply(
      spdy_util_.ConstructSpdyControlFrame(initial_headers.Pass(),
                                           false,
                                           1,
                                           LOWEST,
                                           SYN_REPLY,
                                           CONTROL_FLAG_NONE,
                                           0));

  scoped_ptr<SpdyHeaderBlock> late_headers(new SpdyHeaderBlock());
  (*late_headers)["hello"] = "bye";
  scoped_ptr<SpdyFrame> stream1_headers(
      spdy_util_.ConstructSpdyControlFrame(late_headers.Pass(),
                                           false,
                                           1,
                                           LOWEST,
                                           HEADERS,
                                           CONTROL_FLAG_NONE,
                                           0));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply),
    CreateMockRead(*stream1_headers),
    CreateMockRead(*stream1_body),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
}

TEST_P(SpdyNetworkTransactionTest, SynReplyWithLateHeaders) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_PROTOCOL_ERROR));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*rst),
  };

  scoped_ptr<SpdyHeaderBlock> initial_headers(new SpdyHeaderBlock());
  (*initial_headers)[spdy_util_.GetStatusKey()] = "200 OK";
  (*initial_headers)[spdy_util_.GetVersionKey()] = "HTTP/1.1";
  scoped_ptr<SpdyFrame> stream1_reply(
      spdy_util_.ConstructSpdyControlFrame(initial_headers.Pass(),
                                           false,
                                           1,
                                           LOWEST,
                                           SYN_REPLY,
                                           CONTROL_FLAG_NONE,
                                           0));

  scoped_ptr<SpdyHeaderBlock> late_headers(new SpdyHeaderBlock());
  (*late_headers)["hello"] = "bye";
  scoped_ptr<SpdyFrame> stream1_headers(
      spdy_util_.ConstructSpdyControlFrame(late_headers.Pass(),
                                           false,
                                           1,
                                           LOWEST,
                                           HEADERS,
                                           CONTROL_FLAG_NONE,
                                           0));
  scoped_ptr<SpdyFrame> stream1_body(
      spdy_util_.ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> stream1_body2(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*stream1_reply),
    CreateMockRead(*stream1_body),
    CreateMockRead(*stream1_headers),
    CreateMockRead(*stream1_body2),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  DelayedSocketData data(1, reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.RunToCompletion(&data);
  TransactionHelperResult out = helper.output();
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
}

TEST_P(SpdyNetworkTransactionTest, ServerPushCrossOriginCorrectness) {
  // In this test we want to verify that we can't accidentally push content
  // which can't be pushed by this content server.
  // This test assumes that:
  //   - if we're requesting http://www.foo.com/barbaz
  //   - the browser has made a connection to "www.foo.com".

  // A list of the URL to fetch, followed by the URL being pushed.
  static const char* const kTestCases[] = {
    "http://www.google.com/foo.html",
    "http://www.google.com:81/foo.js",     // Bad port

    "http://www.google.com/foo.html",
    "https://www.google.com/foo.js",       // Bad protocol

    "http://www.google.com/foo.html",
    "ftp://www.google.com/foo.js",         // Invalid Protocol

    "http://www.google.com/foo.html",
    "http://blat.www.google.com/foo.js",   // Cross subdomain

    "http://www.google.com/foo.html",
    "http://www.foo.com/foo.js",           // Cross domain
  };

  for (size_t index = 0; index < arraysize(kTestCases); index += 2) {
    const char* url_to_fetch = kTestCases[index];
    const char* url_to_push = kTestCases[index + 1];

    scoped_ptr<SpdyFrame> stream1_syn(
        spdy_util_.ConstructSpdyGet(url_to_fetch, false, 1, LOWEST));
    scoped_ptr<SpdyFrame> stream1_body(
        spdy_util_.ConstructSpdyBodyFrame(1, true));
    scoped_ptr<SpdyFrame> push_rst(
        spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_REFUSED_STREAM));
    MockWrite writes[] = {
      CreateMockWrite(*stream1_syn, 1),
      CreateMockWrite(*push_rst, 4),
    };

    scoped_ptr<SpdyFrame>
        stream1_reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
    scoped_ptr<SpdyFrame>
        stream2_syn(spdy_util_.ConstructSpdyPush(NULL,
                                                 0,
                                                 2,
                                                 1,
                                                 url_to_push));
    const char kPushedData[] = "pushed";
    scoped_ptr<SpdyFrame> stream2_body(
        spdy_util_.ConstructSpdyBodyFrame(
            2, kPushedData, strlen(kPushedData), true));
    scoped_ptr<SpdyFrame> rst(
        spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_CANCEL));

    MockRead reads[] = {
      CreateMockRead(*stream1_reply, 2),
      CreateMockRead(*stream2_syn, 3),
      CreateMockRead(*stream1_body, 5, SYNCHRONOUS),
      CreateMockRead(*stream2_body, 6),
      MockRead(ASYNC, ERR_IO_PENDING, 7),  // Force a pause
    };

    HttpResponseInfo response;
    OrderedSocketData data(reads, arraysize(reads),
                           writes, arraysize(writes));

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL(url_to_fetch);
    request.load_flags = 0;

    // Enable cross-origin push. Since we are not using a proxy, this should
    // not actually enable cross-origin SPDY push.
    scoped_ptr<SpdySessionDependencies> session_deps(
        CreateSpdySessionDependencies(GetParam()));
    session_deps->trusted_spdy_proxy = "123.45.67.89:8080";
    NormalSpdyTransactionHelper helper(request, DEFAULT_PRIORITY,
                                       BoundNetLog(), GetParam(),
                                       session_deps.release());
    helper.RunPreTestSetup();
    helper.AddData(&data);

    HttpNetworkTransaction* trans = helper.trans();

    // Start the transaction with basic parameters.
    TestCompletionCallback callback;

    int rv = trans->Start(&request, callback.callback(), BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    rv = callback.WaitForResult();

    // Read the response body.
    std::string result;
    ReadResult(trans, &data, &result);

    // Verify that we consumed all test data.
    EXPECT_TRUE(data.at_read_eof());
    EXPECT_TRUE(data.at_write_eof());

    // Verify the SYN_REPLY.
    // Copy the response info, because trans goes away.
    response = *trans->GetResponseInfo();

    VerifyStreamsClosed(helper);

    // Verify the SYN_REPLY.
    EXPECT_TRUE(response.headers.get() != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());
  }
}

TEST_P(SpdyNetworkTransactionTest, RetryAfterRefused) {
  // Construct the request.
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, LOWEST, true));
  MockWrite writes[] = {
    CreateMockWrite(*req, 1),
    CreateMockWrite(*req2, 3),
  };

  scoped_ptr<SpdyFrame> refused(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_REFUSED_STREAM));
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(3, true));
  MockRead reads[] = {
    CreateMockRead(*refused, 2),
    CreateMockRead(*resp, 4),
    CreateMockRead(*body, 5),
    MockRead(ASYNC, 0, 6)  // EOF
  };

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);

  helper.RunPreTestSetup();
  helper.AddData(&data);

  HttpNetworkTransaction* trans = helper.trans();

  // Start the transaction with basic parameters.
  TestCompletionCallback callback;
  int rv = trans->Start(
      &CreateGetRequest(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  // Verify that we consumed all test data.
  EXPECT_TRUE(data.at_read_eof()) << "Read count: "
                                   << data.read_count()
                                   << " Read index: "
                                   << data.read_index();
  EXPECT_TRUE(data.at_write_eof()) << "Write count: "
                                    << data.write_count()
                                    << " Write index: "
                                    << data.write_index();

  // Verify the SYN_REPLY.
  HttpResponseInfo response = *trans->GetResponseInfo();
  EXPECT_TRUE(response.headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response.headers->GetStatusLine());
}

TEST_P(SpdyNetworkTransactionTest, OutOfOrderSynStream) {
  // This first request will start to establish the SpdySession.
  // Then we will start the second (MEDIUM priority) and then third
  // (HIGHEST priority) request in such a way that the third will actually
  // start before the second, causing the second to be numbered differently
  // than the order they were created.
  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, HIGHEST, true));
  scoped_ptr<SpdyFrame> req3(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 5, MEDIUM, true));
  MockWrite writes[] = {
    CreateMockWrite(*req1, 0),
    CreateMockWrite(*req2, 3),
    CreateMockWrite(*req3, 4),
  };

  scoped_ptr<SpdyFrame> resp1(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body1(spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(3, true));
  scoped_ptr<SpdyFrame> resp3(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 5));
  scoped_ptr<SpdyFrame> body3(spdy_util_.ConstructSpdyBodyFrame(5, true));
  MockRead reads[] = {
    CreateMockRead(*resp1, 1),
    CreateMockRead(*body1, 2),
    CreateMockRead(*resp2, 5),
    CreateMockRead(*body2, 6),
    CreateMockRead(*resp3, 7),
    CreateMockRead(*body3, 8),
    MockRead(ASYNC, 0, 9)  // EOF
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  NormalSpdyTransactionHelper helper(CreateGetRequest(), LOWEST,
                                     BoundNetLog(), GetParam(), NULL);
  helper.SetDeterministic();
  helper.RunPreTestSetup();
  helper.AddDeterministicData(&data);

  // Start the first transaction to set up the SpdySession
  HttpNetworkTransaction* trans = helper.trans();
  TestCompletionCallback callback;
  HttpRequestInfo info1 = CreateGetRequest();
  int rv = trans->Start(&info1, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Run the message loop, but do not allow the write to complete.
  // This leaves the SpdySession with a write pending, which prevents
  // SpdySession from attempting subsequent writes until this write completes.
  base::RunLoop().RunUntilIdle();

  // Now, start both new transactions
  HttpRequestInfo info2 = CreateGetRequest();
  TestCompletionCallback callback2;
  scoped_ptr<HttpNetworkTransaction> trans2(
      new HttpNetworkTransaction(MEDIUM, helper.session().get()));
  rv = trans2->Start(&info2, callback2.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  base::RunLoop().RunUntilIdle();

  HttpRequestInfo info3 = CreateGetRequest();
  TestCompletionCallback callback3;
  scoped_ptr<HttpNetworkTransaction> trans3(
      new HttpNetworkTransaction(HIGHEST, helper.session().get()));
  rv = trans3->Start(&info3, callback3.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  base::RunLoop().RunUntilIdle();

  // We now have two SYN_STREAM frames queued up which will be
  // dequeued only once the first write completes, which we
  // now allow to happen.
  data.RunFor(2);
  EXPECT_EQ(OK, callback.WaitForResult());

  // And now we can allow everything else to run to completion.
  data.SetStop(10);
  data.Run();
  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_EQ(OK, callback3.WaitForResult());

  helper.VerifyDataConsumed();
}

// The tests below are only for SPDY/3 and above.

// Test that sent data frames and received WINDOW_UPDATE frames change
// the send_window_size_ correctly.

// WINDOW_UPDATE is different than most other frames in that it can arrive
// while the client is still sending the request body.  In order to enforce
// this scenario, we feed a couple of dummy frames and give a delay of 0 to
// socket data provider, so that initial read that is done as soon as the
// stream is created, succeeds and schedules another read.  This way reads
// and writes are interleaved; after doing a full frame write, SpdyStream
// will break out of DoLoop and will read and process a WINDOW_UPDATE.
// Once our WINDOW_UPDATE is read, we cannot send SYN_REPLY right away
// since request has not been completely written, therefore we feed
// enough number of WINDOW_UPDATEs to finish the first read and cause a
// write, leading to a complete write of request body; after that we send
// a reply with a body, to cause a graceful shutdown.

// TODO(agayev): develop a socket data provider where both, reads and
// writes are ordered so that writing tests like these are easy and rewrite
// all these tests using it.  Right now we are working around the
// limitations as described above and it's not deterministic, tests may
// fail under specific circumstances.
TEST_P(SpdyNetworkTransactionTest, WindowUpdateReceived) {
  if (GetParam().protocol < kProtoSPDY3)
    return;

  static int kFrameCount = 2;
  scoped_ptr<std::string> content(
      new std::string(kMaxSpdyFrameChunkSize, 'a'));
  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyPost(
      kRequestUrl, 1, kMaxSpdyFrameChunkSize * kFrameCount, LOWEST, NULL, 0));
  scoped_ptr<SpdyFrame> body(
      spdy_util_.ConstructSpdyBodyFrame(
          1, content->c_str(), content->size(), false));
  scoped_ptr<SpdyFrame> body_end(
      spdy_util_.ConstructSpdyBodyFrame(
          1, content->c_str(), content->size(), true));

  MockWrite writes[] = {
    CreateMockWrite(*req, 0),
    CreateMockWrite(*body, 1),
    CreateMockWrite(*body_end, 2),
  };

  static const int32 kDeltaWindowSize = 0xff;
  static const int kDeltaCount = 4;
  scoped_ptr<SpdyFrame> window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, kDeltaWindowSize));
  scoped_ptr<SpdyFrame> window_update_dummy(
      spdy_util_.ConstructSpdyWindowUpdate(2, kDeltaWindowSize));
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*window_update_dummy, 3),
    CreateMockRead(*window_update_dummy, 4),
    CreateMockRead(*window_update_dummy, 5),
    CreateMockRead(*window_update, 6),     // Four updates, therefore window
    CreateMockRead(*window_update, 7),     // size should increase by
    CreateMockRead(*window_update, 8),     // kDeltaWindowSize * 4
    CreateMockRead(*window_update, 9),
    CreateMockRead(*resp, 10),
    CreateMockRead(*body_end, 11),
    MockRead(ASYNC, 0, 0, 12)  // EOF
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));

  ScopedVector<UploadElementReader> element_readers;
  for (int i = 0; i < kFrameCount; ++i) {
    element_readers.push_back(
        new UploadBytesElementReader(content->c_str(), content->size()));
  }
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL(kDefaultURL);
  request.upload_data_stream = &upload_data_stream;

  NormalSpdyTransactionHelper helper(request, DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.SetDeterministic();
  helper.AddDeterministicData(&data);
  helper.RunPreTestSetup();

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), callback.callback(), BoundNetLog());

  EXPECT_EQ(ERR_IO_PENDING, rv);

  data.RunFor(11);

  SpdyHttpStream* stream = static_cast<SpdyHttpStream*>(trans->stream_.get());
  ASSERT_TRUE(stream != NULL);
  ASSERT_TRUE(stream->stream() != NULL);
  EXPECT_EQ(static_cast<int>(kSpdyStreamInitialWindowSize) +
            kDeltaWindowSize * kDeltaCount -
            kMaxSpdyFrameChunkSize * kFrameCount,
            stream->stream()->send_window_size());

  data.RunFor(1);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  helper.VerifyDataConsumed();
}

// Test that received data frames and sent WINDOW_UPDATE frames change
// the recv_window_size_ correctly.
TEST_P(SpdyNetworkTransactionTest, WindowUpdateSent) {
  if (GetParam().protocol < kProtoSPDY3)
    return;

  // Amount of body required to trigger a sent window update.
  const size_t kTargetSize = kSpdyStreamInitialWindowSize / 2 + 1;

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> session_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(0, kTargetSize));
  scoped_ptr<SpdyFrame> window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, kTargetSize));

  std::vector<MockWrite> writes;
  writes.push_back(CreateMockWrite(*req));
  if (GetParam().protocol >= kProtoSPDY31)
    writes.push_back(CreateMockWrite(*session_window_update));
  writes.push_back(CreateMockWrite(*window_update));

  std::vector<MockRead> reads;
  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  reads.push_back(CreateMockRead(*resp));

  ScopedVector<SpdyFrame> body_frames;
  const std::string body_data(4096, 'x');
  for (size_t remaining = kTargetSize; remaining != 0;) {
    size_t frame_size = std::min(remaining, body_data.size());
    body_frames.push_back(spdy_util_.ConstructSpdyBodyFrame(
        1, body_data.data(), frame_size, false));
    reads.push_back(CreateMockRead(*body_frames.back()));
    remaining -= frame_size;
  }
  reads.push_back(MockRead(ASYNC, ERR_IO_PENDING, 0)); // Yield.

  DelayedSocketData data(1, vector_as_array(&reads), reads.size(),
                         vector_as_array(&writes), writes.size());

  NormalSpdyTransactionHelper helper(CreateGetRequest(), DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.AddData(&data);
  helper.RunPreTestSetup();
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), callback.callback(), BoundNetLog());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  SpdyHttpStream* stream =
      static_cast<SpdyHttpStream*>(trans->stream_.get());
  ASSERT_TRUE(stream != NULL);
  ASSERT_TRUE(stream->stream() != NULL);

  // All data has been read, but not consumed. The window reflects this.
  EXPECT_EQ(static_cast<int>(kSpdyStreamInitialWindowSize - kTargetSize),
            stream->stream()->recv_window_size());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers.get() != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);

  // Issue a read which will cause a WINDOW_UPDATE to be sent and window
  // size increased to default.
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kTargetSize));
  EXPECT_EQ(static_cast<int>(kTargetSize),
            trans->Read(buf.get(), kTargetSize, CompletionCallback()));
  EXPECT_EQ(static_cast<int>(kSpdyStreamInitialWindowSize),
            stream->stream()->recv_window_size());
  EXPECT_THAT(base::StringPiece(buf->data(), kTargetSize), Each(Eq('x')));

  // Allow scheduled WINDOW_UPDATE frames to write.
  base::RunLoop().RunUntilIdle();
  helper.VerifyDataConsumed();
}

// Test that WINDOW_UPDATE frame causing overflow is handled correctly.
TEST_P(SpdyNetworkTransactionTest, WindowUpdateOverflow) {
  if (GetParam().protocol < kProtoSPDY3)
    return;

  // Number of full frames we hope to write (but will not, used to
  // set content-length header correctly)
  static int kFrameCount = 3;

  scoped_ptr<std::string> content(
      new std::string(kMaxSpdyFrameChunkSize, 'a'));
  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyPost(
      kRequestUrl, 1, kMaxSpdyFrameChunkSize * kFrameCount, LOWEST, NULL, 0));
  scoped_ptr<SpdyFrame> body(
      spdy_util_.ConstructSpdyBodyFrame(
          1, content->c_str(), content->size(), false));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_FLOW_CONTROL_ERROR));

  // We're not going to write a data frame with FIN, we'll receive a bad
  // WINDOW_UPDATE while sending a request and will send a RST_STREAM frame.
  MockWrite writes[] = {
    CreateMockWrite(*req, 0),
    CreateMockWrite(*body, 2),
    CreateMockWrite(*rst, 3),
  };

  static const int32 kDeltaWindowSize = 0x7fffffff;  // cause an overflow
  scoped_ptr<SpdyFrame> window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, kDeltaWindowSize));
  MockRead reads[] = {
    CreateMockRead(*window_update, 1),
    MockRead(ASYNC, 0, 4)  // EOF
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));

  ScopedVector<UploadElementReader> element_readers;
  for (int i = 0; i < kFrameCount; ++i) {
    element_readers.push_back(
        new UploadBytesElementReader(content->c_str(), content->size()));
  }
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data_stream = &upload_data_stream;

  NormalSpdyTransactionHelper helper(request, DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.SetDeterministic();
  helper.RunPreTestSetup();
  helper.AddDeterministicData(&data);
  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), callback.callback(), BoundNetLog());
  ASSERT_EQ(ERR_IO_PENDING, rv);

  data.RunFor(5);
  ASSERT_TRUE(callback.have_result());
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, callback.WaitForResult());
  helper.VerifyDataConsumed();
}

// Test that after hitting a send window size of 0, the write process
// stalls and upon receiving WINDOW_UPDATE frame write resumes.

// This test constructs a POST request followed by enough data frames
// containing 'a' that would make the window size 0, followed by another
// data frame containing default content (which is "hello!") and this frame
// also contains a FIN flag.  DelayedSocketData is used to enforce all
// writes go through before a read could happen.  However, the last frame
// ("hello!")  is not supposed to go through since by the time its turn
// arrives, window size is 0.  At this point MessageLoop::Run() called via
// callback would block.  Therefore we call MessageLoop::RunUntilIdle()
// which returns after performing all possible writes.  We use DCHECKS to
// ensure that last data frame is still there and stream has stalled.
// After that, next read is artifically enforced, which causes a
// WINDOW_UPDATE to be read and I/O process resumes.
TEST_P(SpdyNetworkTransactionTest, FlowControlStallResume) {
  if (GetParam().protocol < kProtoSPDY3)
    return;

  // Number of frames we need to send to zero out the window size: data
  // frames plus SYN_STREAM plus the last data frame; also we need another
  // data frame that we will send once the WINDOW_UPDATE is received,
  // therefore +3.
  size_t num_writes = kSpdyStreamInitialWindowSize / kMaxSpdyFrameChunkSize + 3;

  // Calculate last frame's size; 0 size data frame is legal.
  size_t last_frame_size =
      kSpdyStreamInitialWindowSize % kMaxSpdyFrameChunkSize;

  // Construct content for a data frame of maximum size.
  std::string content(kMaxSpdyFrameChunkSize, 'a');

  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyPost(
      kRequestUrl, 1, kSpdyStreamInitialWindowSize + kUploadDataSize,
      LOWEST, NULL, 0));

  // Full frames.
  scoped_ptr<SpdyFrame> body1(
      spdy_util_.ConstructSpdyBodyFrame(
          1, content.c_str(), content.size(), false));

  // Last frame to zero out the window size.
  scoped_ptr<SpdyFrame> body2(
      spdy_util_.ConstructSpdyBodyFrame(
          1, content.c_str(), last_frame_size, false));

  // Data frame to be sent once WINDOW_UPDATE frame is received.
  scoped_ptr<SpdyFrame> body3(spdy_util_.ConstructSpdyBodyFrame(1, true));

  // Fill in mock writes.
  scoped_ptr<MockWrite[]> writes(new MockWrite[num_writes]);
  size_t i = 0;
  writes[i] = CreateMockWrite(*req);
  for (i = 1; i < num_writes - 2; i++)
    writes[i] = CreateMockWrite(*body1);
  writes[i++] = CreateMockWrite(*body2);
  writes[i] = CreateMockWrite(*body3);

  // Construct read frame, give enough space to upload the rest of the
  // data.
  scoped_ptr<SpdyFrame> session_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(0, kUploadDataSize));
  scoped_ptr<SpdyFrame> window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, kUploadDataSize));
  scoped_ptr<SpdyFrame> reply(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*session_window_update),
    CreateMockRead(*session_window_update),
    CreateMockRead(*window_update),
    CreateMockRead(*window_update),
    CreateMockRead(*reply),
    CreateMockRead(*body2),
    CreateMockRead(*body3),
    MockRead(ASYNC, 0, 0)  // EOF
  };

  // Skip the session window updates unless we're using SPDY/3.1 and
  // above.
  size_t read_offset = (GetParam().protocol >= kProtoSPDY31) ? 0 : 2;
  size_t num_reads = arraysize(reads) - read_offset;

  // Force all writes to happen before any read, last write will not
  // actually queue a frame, due to window size being 0.
  DelayedSocketData data(num_writes, reads + read_offset, num_reads,
                         writes.get(), num_writes);

  ScopedVector<UploadElementReader> element_readers;
  std::string upload_data_string(kSpdyStreamInitialWindowSize, 'a');
  upload_data_string.append(kUploadData, kUploadDataSize);
  element_readers.push_back(new UploadBytesElementReader(
      upload_data_string.c_str(), upload_data_string.size()));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data_stream = &upload_data_stream;
  NormalSpdyTransactionHelper helper(request, DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.AddData(&data);
  helper.RunPreTestSetup();

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  base::RunLoop().RunUntilIdle();  // Write as much as we can.

  SpdyHttpStream* stream = static_cast<SpdyHttpStream*>(trans->stream_.get());
  ASSERT_TRUE(stream != NULL);
  ASSERT_TRUE(stream->stream() != NULL);
  EXPECT_EQ(0, stream->stream()->send_window_size());
  // All the body data should have been read.
  // TODO(satorux): This is because of the weirdness in reading the request
  // body in OnSendBodyComplete(). See crbug.com/113107.
  EXPECT_TRUE(upload_data_stream.IsEOF());
  // But the body is not yet fully sent (kUploadData is not yet sent)
  // since we're send-stalled.
  EXPECT_TRUE(stream->stream()->send_stalled_by_flow_control());

  data.ForceNextRead();   // Read in WINDOW_UPDATE frame.
  rv = callback.WaitForResult();
  helper.VerifyDataConsumed();
}

// Test we correctly handle the case where the SETTINGS frame results in
// unstalling the send window.
TEST_P(SpdyNetworkTransactionTest, FlowControlStallResumeAfterSettings) {
  if (GetParam().protocol < kProtoSPDY3)
    return;

  // Number of frames we need to send to zero out the window size: data
  // frames plus SYN_STREAM plus the last data frame; also we need another
  // data frame that we will send once the SETTING is received, therefore +3.
  size_t num_writes = kSpdyStreamInitialWindowSize / kMaxSpdyFrameChunkSize + 3;

  // Calculate last frame's size; 0 size data frame is legal.
  size_t last_frame_size =
      kSpdyStreamInitialWindowSize % kMaxSpdyFrameChunkSize;

  // Construct content for a data frame of maximum size.
  std::string content(kMaxSpdyFrameChunkSize, 'a');

  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyPost(
      kRequestUrl, 1, kSpdyStreamInitialWindowSize + kUploadDataSize,
      LOWEST, NULL, 0));

  // Full frames.
  scoped_ptr<SpdyFrame> body1(
      spdy_util_.ConstructSpdyBodyFrame(
          1, content.c_str(), content.size(), false));

  // Last frame to zero out the window size.
  scoped_ptr<SpdyFrame> body2(
      spdy_util_.ConstructSpdyBodyFrame(
          1, content.c_str(), last_frame_size, false));

  // Data frame to be sent once SETTINGS frame is received.
  scoped_ptr<SpdyFrame> body3(spdy_util_.ConstructSpdyBodyFrame(1, true));

  // Fill in mock reads/writes.
  std::vector<MockRead> reads;
  std::vector<MockWrite> writes;
  size_t i = 0;
  writes.push_back(CreateMockWrite(*req, i++));
  while (i < num_writes - 2)
    writes.push_back(CreateMockWrite(*body1, i++));
  writes.push_back(CreateMockWrite(*body2, i++));

  // Construct read frame for SETTINGS that gives enough space to upload the
  // rest of the data.
  SettingsMap settings;
  settings[SETTINGS_INITIAL_WINDOW_SIZE] =
      SettingsFlagsAndValue(
          SETTINGS_FLAG_NONE, kSpdyStreamInitialWindowSize * 2);
  scoped_ptr<SpdyFrame> settings_frame_large(
      spdy_util_.ConstructSpdySettings(settings));

  reads.push_back(CreateMockRead(*settings_frame_large, i++));

  scoped_ptr<SpdyFrame> session_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(0, kUploadDataSize));
  if (GetParam().protocol >= kProtoSPDY31)
    reads.push_back(CreateMockRead(*session_window_update, i++));

  scoped_ptr<SpdyFrame> settings_ack(spdy_util_.ConstructSpdySettingsAck());
  writes.push_back(CreateMockWrite(*settings_ack, i++));

  writes.push_back(CreateMockWrite(*body3, i++));

  scoped_ptr<SpdyFrame> reply(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  reads.push_back(CreateMockRead(*reply, i++));
  reads.push_back(CreateMockRead(*body2, i++));
  reads.push_back(CreateMockRead(*body3, i++));
  reads.push_back(MockRead(ASYNC, 0, i++));  // EOF

  // Force all writes to happen before any read, last write will not
  // actually queue a frame, due to window size being 0.
  DeterministicSocketData data(vector_as_array(&reads), reads.size(),
                               vector_as_array(&writes), writes.size());

  ScopedVector<UploadElementReader> element_readers;
  std::string upload_data_string(kSpdyStreamInitialWindowSize, 'a');
  upload_data_string.append(kUploadData, kUploadDataSize);
  element_readers.push_back(new UploadBytesElementReader(
      upload_data_string.c_str(), upload_data_string.size()));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data_stream = &upload_data_stream;
  NormalSpdyTransactionHelper helper(request, DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.SetDeterministic();
  helper.RunPreTestSetup();
  helper.AddDeterministicData(&data);

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  data.RunFor(num_writes - 1);   // Write as much as we can.

  SpdyHttpStream* stream = static_cast<SpdyHttpStream*>(trans->stream_.get());
  ASSERT_TRUE(stream != NULL);
  ASSERT_TRUE(stream->stream() != NULL);
  EXPECT_EQ(0, stream->stream()->send_window_size());

  // All the body data should have been read.
  // TODO(satorux): This is because of the weirdness in reading the request
  // body in OnSendBodyComplete(). See crbug.com/113107.
  EXPECT_TRUE(upload_data_stream.IsEOF());
  // But the body is not yet fully sent (kUploadData is not yet sent)
  // since we're send-stalled.
  EXPECT_TRUE(stream->stream()->send_stalled_by_flow_control());

  data.RunFor(7);   // Read in SETTINGS frame to unstall.
  rv = callback.WaitForResult();
  helper.VerifyDataConsumed();
  // If stream is NULL, that means it was unstalled and closed.
  EXPECT_TRUE(stream->stream() == NULL);
}

// Test we correctly handle the case where the SETTINGS frame results in a
// negative send window size.
TEST_P(SpdyNetworkTransactionTest, FlowControlNegativeSendWindowSize) {
  if (GetParam().protocol < kProtoSPDY3)
    return;

  // Number of frames we need to send to zero out the window size: data
  // frames plus SYN_STREAM plus the last data frame; also we need another
  // data frame that we will send once the SETTING is received, therefore +3.
  size_t num_writes = kSpdyStreamInitialWindowSize / kMaxSpdyFrameChunkSize + 3;

  // Calculate last frame's size; 0 size data frame is legal.
  size_t last_frame_size =
      kSpdyStreamInitialWindowSize % kMaxSpdyFrameChunkSize;

  // Construct content for a data frame of maximum size.
  std::string content(kMaxSpdyFrameChunkSize, 'a');

  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyPost(
      kRequestUrl, 1, kSpdyStreamInitialWindowSize + kUploadDataSize,
      LOWEST, NULL, 0));

  // Full frames.
  scoped_ptr<SpdyFrame> body1(
      spdy_util_.ConstructSpdyBodyFrame(
          1, content.c_str(), content.size(), false));

  // Last frame to zero out the window size.
  scoped_ptr<SpdyFrame> body2(
      spdy_util_.ConstructSpdyBodyFrame(
          1, content.c_str(), last_frame_size, false));

  // Data frame to be sent once SETTINGS frame is received.
  scoped_ptr<SpdyFrame> body3(spdy_util_.ConstructSpdyBodyFrame(1, true));

  // Fill in mock reads/writes.
  std::vector<MockRead> reads;
  std::vector<MockWrite> writes;
  size_t i = 0;
  writes.push_back(CreateMockWrite(*req, i++));
  while (i < num_writes - 2)
    writes.push_back(CreateMockWrite(*body1, i++));
  writes.push_back(CreateMockWrite(*body2, i++));

  // Construct read frame for SETTINGS that makes the send_window_size
  // negative.
  SettingsMap new_settings;
  new_settings[SETTINGS_INITIAL_WINDOW_SIZE] =
      SettingsFlagsAndValue(
          SETTINGS_FLAG_NONE, kSpdyStreamInitialWindowSize / 2);
  scoped_ptr<SpdyFrame> settings_frame_small(
      spdy_util_.ConstructSpdySettings(new_settings));
  // Construct read frames for WINDOW_UPDATE that makes the send_window_size
  // positive.
  scoped_ptr<SpdyFrame> session_window_update_init_size(
      spdy_util_.ConstructSpdyWindowUpdate(0, kSpdyStreamInitialWindowSize));
  scoped_ptr<SpdyFrame> window_update_init_size(
      spdy_util_.ConstructSpdyWindowUpdate(1, kSpdyStreamInitialWindowSize));

  reads.push_back(CreateMockRead(*settings_frame_small, i++));

  if (GetParam().protocol >= kProtoSPDY3)
    reads.push_back(CreateMockRead(*session_window_update_init_size, i++));

  reads.push_back(CreateMockRead(*window_update_init_size, i++));

  scoped_ptr<SpdyFrame> settings_ack(spdy_util_.ConstructSpdySettingsAck());
  writes.push_back(CreateMockWrite(*settings_ack, i++));

  writes.push_back(CreateMockWrite(*body3, i++));

  scoped_ptr<SpdyFrame> reply(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  reads.push_back(CreateMockRead(*reply, i++));
  reads.push_back(CreateMockRead(*body2, i++));
  reads.push_back(CreateMockRead(*body3, i++));
  reads.push_back(MockRead(ASYNC, 0, i++));  // EOF

  // Force all writes to happen before any read, last write will not
  // actually queue a frame, due to window size being 0.
  DeterministicSocketData data(vector_as_array(&reads), reads.size(),
                               vector_as_array(&writes), writes.size());

  ScopedVector<UploadElementReader> element_readers;
  std::string upload_data_string(kSpdyStreamInitialWindowSize, 'a');
  upload_data_string.append(kUploadData, kUploadDataSize);
  element_readers.push_back(new UploadBytesElementReader(
      upload_data_string.c_str(), upload_data_string.size()));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data_stream = &upload_data_stream;
  NormalSpdyTransactionHelper helper(request, DEFAULT_PRIORITY,
                                     BoundNetLog(), GetParam(), NULL);
  helper.SetDeterministic();
  helper.RunPreTestSetup();
  helper.AddDeterministicData(&data);

  HttpNetworkTransaction* trans = helper.trans();

  TestCompletionCallback callback;
  int rv = trans->Start(&helper.request(), callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  data.RunFor(num_writes - 1);   // Write as much as we can.

  SpdyHttpStream* stream = static_cast<SpdyHttpStream*>(trans->stream_.get());
  ASSERT_TRUE(stream != NULL);
  ASSERT_TRUE(stream->stream() != NULL);
  EXPECT_EQ(0, stream->stream()->send_window_size());

  // All the body data should have been read.
  // TODO(satorux): This is because of the weirdness in reading the request
  // body in OnSendBodyComplete(). See crbug.com/113107.
  EXPECT_TRUE(upload_data_stream.IsEOF());
  // But the body is not yet fully sent (kUploadData is not yet sent)
  // since we're send-stalled.
  EXPECT_TRUE(stream->stream()->send_stalled_by_flow_control());

  // Read in WINDOW_UPDATE or SETTINGS frame.
  data.RunFor((GetParam().protocol >= kProtoSPDY31) ? 9 : 8);
  rv = callback.WaitForResult();
  helper.VerifyDataConsumed();
}

class SpdyNetworkTransactionNoTLSUsageCheckTest
    : public SpdyNetworkTransactionTest {
 protected:
  void RunNoTLSUsageCheckTest(scoped_ptr<SSLSocketDataProvider> ssl_provider) {
    // Construct the request.
    scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyGet(
        "https://www.google.com/", false, 1, LOWEST));
    MockWrite writes[] = {CreateMockWrite(*req)};

    scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
    scoped_ptr<SpdyFrame> body(spdy_util_.ConstructSpdyBodyFrame(1, true));
    MockRead reads[] = {
        CreateMockRead(*resp), CreateMockRead(*body),
        MockRead(ASYNC, 0, 0)  // EOF
    };

    DelayedSocketData data(
        1, reads, arraysize(reads), writes, arraysize(writes));
    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("https://www.google.com/");
    NormalSpdyTransactionHelper helper(
        request, DEFAULT_PRIORITY, BoundNetLog(), GetParam(), NULL);
    helper.RunToCompletionWithSSLData(&data, ssl_provider.Pass());
    TransactionHelperResult out = helper.output();
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!", out.response_data);
  }
};

//-----------------------------------------------------------------------------
// All tests are run with three different connection types: SPDY after NPN
// negotiation, SPDY without SSL, and SPDY with SSL.
//
// TODO(akalin): Use ::testing::Combine() when we are able to use
// <tr1/tuple>.
INSTANTIATE_TEST_CASE_P(
    Spdy,
    SpdyNetworkTransactionNoTLSUsageCheckTest,
    ::testing::Values(SpdyNetworkTransactionTestParams(kProtoDeprecatedSPDY2,
                                                       SPDYNPN),
                      SpdyNetworkTransactionTestParams(kProtoSPDY3, SPDYNPN),
                      SpdyNetworkTransactionTestParams(kProtoSPDY31, SPDYNPN)));

TEST_P(SpdyNetworkTransactionNoTLSUsageCheckTest, TLSVersionTooOld) {
  scoped_ptr<SSLSocketDataProvider> ssl_provider(
      new SSLSocketDataProvider(ASYNC, OK));
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_SSL3,
                                &ssl_provider->connection_status);

  RunNoTLSUsageCheckTest(ssl_provider.Pass());
}

TEST_P(SpdyNetworkTransactionNoTLSUsageCheckTest, TLSCipherSuiteSucky) {
  scoped_ptr<SSLSocketDataProvider> ssl_provider(
      new SSLSocketDataProvider(ASYNC, OK));
  // Set to TLS_RSA_WITH_NULL_MD5
  SSLConnectionStatusSetCipherSuite(0x1, &ssl_provider->connection_status);

  RunNoTLSUsageCheckTest(ssl_provider.Pass());
}

class SpdyNetworkTransactionTLSUsageCheckTest
    : public SpdyNetworkTransactionTest {
 protected:
  void RunTLSUsageCheckTest(scoped_ptr<SSLSocketDataProvider> ssl_provider) {
    scoped_ptr<SpdyFrame> goaway(
        spdy_util_.ConstructSpdyGoAway(0, GOAWAY_INADEQUATE_SECURITY, ""));
    MockWrite writes[] = {CreateMockWrite(*goaway)};

    DelayedSocketData data(1, NULL, 0, writes, arraysize(writes));
    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("https://www.google.com/");
    NormalSpdyTransactionHelper helper(
        request, DEFAULT_PRIORITY, BoundNetLog(), GetParam(), NULL);
    helper.RunToCompletionWithSSLData(&data, ssl_provider.Pass());
    TransactionHelperResult out = helper.output();
    EXPECT_EQ(ERR_SPDY_INADEQUATE_TRANSPORT_SECURITY, out.rv);
  }
};

INSTANTIATE_TEST_CASE_P(
    Spdy,
    SpdyNetworkTransactionTLSUsageCheckTest,
    ::testing::Values(SpdyNetworkTransactionTestParams(kProtoSPDY4, SPDYNPN)));

TEST_P(SpdyNetworkTransactionTLSUsageCheckTest, TLSVersionTooOld) {
  scoped_ptr<SSLSocketDataProvider> ssl_provider(
      new SSLSocketDataProvider(ASYNC, OK));
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_SSL3,
                                &ssl_provider->connection_status);

  RunTLSUsageCheckTest(ssl_provider.Pass());
}

TEST_P(SpdyNetworkTransactionTLSUsageCheckTest, TLSCipherSuiteSucky) {
  scoped_ptr<SSLSocketDataProvider> ssl_provider(
      new SSLSocketDataProvider(ASYNC, OK));
  // Set to TLS_RSA_WITH_NULL_MD5
  SSLConnectionStatusSetCipherSuite(0x1, &ssl_provider->connection_status);

  RunTLSUsageCheckTest(ssl_provider.Pass());
}

}  // namespace net
