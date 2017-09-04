// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_url_fetcher.h"

#include <memory>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace cloud_print {

namespace {

const char kSamplePrivetURL[] =
    "http://10.0.0.8:7676/privet/register?action=start";
const char kSamplePrivetToken[] = "MyToken";
const char kEmptyPrivetToken[] = "\"\"";

const char kSampleParsableJSON[] = "{ \"hello\" : 2 }";
const char kSampleUnparsableJSON[] = "{ \"hello\" : }";
const char kSampleJSONWithError[] = "{ \"error\" : \"unittest_example\" }";

class MockPrivetURLFetcherDelegate : public PrivetURLFetcher::Delegate {
 public:
  MockPrivetURLFetcherDelegate() : raw_mode_(false) {
  }

  ~MockPrivetURLFetcherDelegate() override {
  }

  void OnError(PrivetURLFetcher* fetcher,
               PrivetURLFetcher::ErrorType error) override {
    OnErrorInternal(error);
  }

  MOCK_METHOD1(OnErrorInternal, void(PrivetURLFetcher::ErrorType error));

  void OnParsedJson(PrivetURLFetcher* fetcher,
                    const base::DictionaryValue& value,
                    bool has_error) override {
    saved_value_.reset(value.DeepCopy());
    OnParsedJsonInternal(has_error);
  }

  MOCK_METHOD1(OnParsedJsonInternal, void(bool has_error));

  virtual void OnNeedPrivetToken(
      PrivetURLFetcher* fetcher,
      const PrivetURLFetcher::TokenCallback& callback) {
  }

  bool OnRawData(PrivetURLFetcher* fetcher,
                 bool response_is_file,
                 const std::string& data,
                 const base::FilePath& response_file) {
    if (!raw_mode_) return false;

    if (response_is_file) {
      EXPECT_TRUE(response_file != base::FilePath());
      OnFileInternal();
    } else {
      OnRawDataInternal(data);
    }

    return true;
  }

  MOCK_METHOD1(OnRawDataInternal, void(const std::string& data));

  MOCK_METHOD0(OnFileInternal, void());

  const base::DictionaryValue* saved_value() { return saved_value_.get(); }

  void SetRawMode(bool raw_mode) {
    raw_mode_ = raw_mode;
  }

 private:
  std::unique_ptr<base::DictionaryValue> saved_value_;
  bool raw_mode_;
};

class PrivetURLFetcherTest : public ::testing::Test {
 public:
  PrivetURLFetcherTest() {
    request_context_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());
    privet_urlfetcher_.reset(new PrivetURLFetcher(
        GURL(kSamplePrivetURL),
        net::URLFetcher::POST,
        request_context_.get(),
        &delegate_));

    PrivetURLFetcher::SetTokenForHost(GURL(kSamplePrivetURL).GetOrigin().spec(),
                                      kSamplePrivetToken);
  }
  virtual ~PrivetURLFetcherTest() {
  }

  void RunFor(base::TimeDelta time_period) {
    base::CancelableCallback<void()> callback(base::Bind(
        &PrivetURLFetcherTest::Stop, base::Unretained(this)));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, callback.callback(), time_period);

    base::RunLoop().Run();
    callback.Cancel();
  }

  void Stop() { base::MessageLoop::current()->QuitWhenIdle(); }

 protected:
  base::MessageLoop loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::TestURLFetcherFactory fetcher_factory_;
  std::unique_ptr<PrivetURLFetcher> privet_urlfetcher_;
  StrictMock<MockPrivetURLFetcherDelegate> delegate_;
};

TEST_F(PrivetURLFetcherTest, FetchSuccess) {
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->SetResponseString(kSampleParsableJSON);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(delegate_, OnParsedJsonInternal(false));
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  const base::DictionaryValue* value = delegate_.saved_value();
  int hello_value;
  ASSERT_TRUE(value != NULL);
  ASSERT_TRUE(value->GetInteger("hello", &hello_value));
  EXPECT_EQ(2, hello_value);
}

TEST_F(PrivetURLFetcherTest, HTTP503Retry) {
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->SetResponseString(kSampleParsableJSON);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(503);

  fetcher->delegate()->OnURLFetchComplete(fetcher);

  RunFor(base::TimeDelta::FromSeconds(7));
  fetcher = fetcher_factory_.GetFetcherByID(0);

  ASSERT_TRUE(fetcher != NULL);
  fetcher->SetResponseString(kSampleParsableJSON);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(delegate_, OnParsedJsonInternal(false));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(PrivetURLFetcherTest, ResponseCodeError) {
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->SetResponseString(kSampleParsableJSON);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(404);

  EXPECT_CALL(delegate_,
              OnErrorInternal(PrivetURLFetcher::RESPONSE_CODE_ERROR));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(PrivetURLFetcherTest, JsonParseError) {
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->SetResponseString(kSampleUnparsableJSON);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(delegate_,
              OnErrorInternal(PrivetURLFetcher::JSON_PARSE_ERROR));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(PrivetURLFetcherTest, Header) {
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  net::HttpRequestHeaders headers;
  fetcher->GetExtraRequestHeaders(&headers);

  std::string header_token;
  ASSERT_TRUE(headers.GetHeader("X-Privet-Token", &header_token));
  EXPECT_EQ(kSamplePrivetToken, header_token);
}

TEST_F(PrivetURLFetcherTest, Header2) {
  PrivetURLFetcher::SetTokenForHost(GURL(kSamplePrivetURL).GetOrigin().spec(),
                                    "");

  privet_urlfetcher_->SendEmptyPrivetToken();
  privet_urlfetcher_->Start();

  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  net::HttpRequestHeaders headers;
  fetcher->GetExtraRequestHeaders(&headers);

  std::string header_token;
  ASSERT_TRUE(headers.GetHeader("X-Privet-Token", &header_token));
  EXPECT_EQ(kEmptyPrivetToken, header_token);
}

TEST_F(PrivetURLFetcherTest, AlwaysSendEmpty) {
  PrivetURLFetcher::SetTokenForHost(GURL(kSamplePrivetURL).GetOrigin().spec(),
                                    "SampleToken");

  privet_urlfetcher_->SendEmptyPrivetToken();
  privet_urlfetcher_->Start();

  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  net::HttpRequestHeaders headers;
  fetcher->GetExtraRequestHeaders(&headers);

  std::string header_token;
  ASSERT_TRUE(headers.GetHeader("X-Privet-Token", &header_token));
  EXPECT_EQ(kEmptyPrivetToken, header_token);
}

TEST_F(PrivetURLFetcherTest, FetchHasError) {
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->SetResponseString(kSampleJSONWithError);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(delegate_, OnParsedJsonInternal(true));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(PrivetURLFetcherTest, FetcherRawData) {
  delegate_.SetRawMode(true);
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->SetResponseString(kSampleJSONWithError);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(delegate_, OnRawDataInternal(kSampleJSONWithError));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(PrivetURLFetcherTest, RangeRequest) {
  delegate_.SetRawMode(true);
  privet_urlfetcher_->SetByteRange(200, 300);
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  net::HttpRequestHeaders headers;
  fetcher->GetExtraRequestHeaders(&headers);

  std::string header_range;
  ASSERT_TRUE(headers.GetHeader("Range", &header_range));
  EXPECT_EQ("bytes=200-300", header_range);
}

TEST_F(PrivetURLFetcherTest, FetcherToFile) {
  delegate_.SetRawMode(true);
  privet_urlfetcher_->SaveResponseToFile();
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->SetResponseFilePath(
      base::FilePath(FILE_PATH_LITERAL("sample/file")));
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(delegate_, OnFileInternal());
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

}  // namespace

}  // namespace cloud_print
