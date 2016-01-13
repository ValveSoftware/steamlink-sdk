// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/speech/audio_buffer.h"
#include "content/browser/speech/google_one_shot_remote_engine.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class GoogleOneShotRemoteEngineTest : public SpeechRecognitionEngineDelegate,
                                      public testing::Test {
 public:
  GoogleOneShotRemoteEngineTest()
      : error_(SPEECH_RECOGNITION_ERROR_NONE) {}

  // Creates a speech recognition request and invokes its URL fetcher delegate
  // with the given test data.
  void CreateAndTestRequest(bool success, const std::string& http_response);

  // SpeechRecognitionRequestDelegate methods.
  virtual void OnSpeechRecognitionEngineResults(
      const SpeechRecognitionResults& results) OVERRIDE {
    results_ = results;
  }

  virtual void OnSpeechRecognitionEngineError(
      const SpeechRecognitionError& error) OVERRIDE {
    error_ = error.code;
  }

  // Accessor for the only result item.
  const SpeechRecognitionResult& result() const {
    DCHECK_EQ(results_.size(), 1U);
    return results_[0];
  }

 protected:
  base::MessageLoop message_loop_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  SpeechRecognitionErrorCode error_;
  SpeechRecognitionResults results_;
};

void GoogleOneShotRemoteEngineTest::CreateAndTestRequest(
    bool success, const std::string& http_response) {
  GoogleOneShotRemoteEngine client(NULL);
  unsigned char dummy_audio_buffer_data[2] = {'\0', '\0'};
  scoped_refptr<AudioChunk> dummy_audio_chunk(
      new AudioChunk(&dummy_audio_buffer_data[0],
                     sizeof(dummy_audio_buffer_data),
                     2 /* bytes per sample */));
  client.set_delegate(this);
  client.StartRecognition();
  client.TakeAudioChunk(*dummy_audio_chunk.get());
  client.AudioChunksEnded();
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  fetcher->set_url(fetcher->GetOriginalURL());
  net::URLRequestStatus status;
  status.set_status(success ? net::URLRequestStatus::SUCCESS :
                              net::URLRequestStatus::FAILED);
  fetcher->set_status(status);
  fetcher->set_response_code(success ? 200 : 500);
  fetcher->SetResponseString(http_response);

  fetcher->delegate()->OnURLFetchComplete(fetcher);
  // Parsed response will be available in result().
}

TEST_F(GoogleOneShotRemoteEngineTest, BasicTest) {
  // Normal success case with one result.
  CreateAndTestRequest(true,
      "{\"status\":0,\"hypotheses\":"
      "[{\"utterance\":\"123456\",\"confidence\":0.9}]}");
  EXPECT_EQ(error_, SPEECH_RECOGNITION_ERROR_NONE);
  EXPECT_EQ(1U, result().hypotheses.size());
  EXPECT_EQ(base::ASCIIToUTF16("123456"), result().hypotheses[0].utterance);
  EXPECT_EQ(0.9, result().hypotheses[0].confidence);

  // Normal success case with multiple results.
  CreateAndTestRequest(true,
      "{\"status\":0,\"hypotheses\":["
      "{\"utterance\":\"hello\",\"confidence\":0.9},"
      "{\"utterance\":\"123456\",\"confidence\":0.5}]}");
  EXPECT_EQ(error_, SPEECH_RECOGNITION_ERROR_NONE);
  EXPECT_EQ(2u, result().hypotheses.size());
  EXPECT_EQ(base::ASCIIToUTF16("hello"), result().hypotheses[0].utterance);
  EXPECT_EQ(0.9, result().hypotheses[0].confidence);
  EXPECT_EQ(base::ASCIIToUTF16("123456"), result().hypotheses[1].utterance);
  EXPECT_EQ(0.5, result().hypotheses[1].confidence);

  // Zero results.
  CreateAndTestRequest(true, "{\"status\":0,\"hypotheses\":[]}");
  EXPECT_EQ(error_, SPEECH_RECOGNITION_ERROR_NONE);
  EXPECT_EQ(0U, result().hypotheses.size());

  // Http failure case.
  CreateAndTestRequest(false, std::string());
  EXPECT_EQ(error_, SPEECH_RECOGNITION_ERROR_NETWORK);
  EXPECT_EQ(0U, result().hypotheses.size());

  // Invalid status case.
  CreateAndTestRequest(true, "{\"status\":\"invalid\",\"hypotheses\":[]}");
  EXPECT_EQ(error_, SPEECH_RECOGNITION_ERROR_NETWORK);
  EXPECT_EQ(0U, result().hypotheses.size());

  // Server-side error case.
  CreateAndTestRequest(true, "{\"status\":1,\"hypotheses\":[]}");
  EXPECT_EQ(error_, SPEECH_RECOGNITION_ERROR_NETWORK);
  EXPECT_EQ(0U, result().hypotheses.size());

  // Malformed JSON case.
  CreateAndTestRequest(true, "{\"status\":0,\"hypotheses\":"
      "[{\"unknownkey\":\"hello\"}]}");
  EXPECT_EQ(error_, SPEECH_RECOGNITION_ERROR_NETWORK);
  EXPECT_EQ(0U, result().hypotheses.size());
}

}  // namespace content
