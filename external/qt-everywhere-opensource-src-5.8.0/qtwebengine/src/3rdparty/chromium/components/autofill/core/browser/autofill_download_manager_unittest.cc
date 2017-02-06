// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_download_manager.h"

#include <stddef.h>

#include <list>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/common/form_data.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

namespace {

// Call |fetcher->OnURLFetchComplete()| as the URLFetcher would when
// a response is received.  Params allow caller to set fake status.
void FakeOnURLFetchComplete(net::TestURLFetcher* fetcher,
                            int response_code,
                            const std::string& response_body) {
  fetcher->set_url(GURL());
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(response_code);
  fetcher->SetResponseString(response_body);

  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

}  // namespace

// This tests AutofillDownloadManager. AutofillDownloadManagerTest implements
// AutofillDownloadManager::Observer and creates an instance of
// AutofillDownloadManager. Then it records responses to different initiated
// requests, which are verified later. To mock network requests
// TestURLFetcherFactory is used, which creates URLFetchers that do not
// go over the wire, but allow calling back HTTP responses directly.
// The responses in test are out of order and verify: successful query request,
// successful upload request, failed upload request.
class AutofillDownloadManagerTest : public AutofillDownloadManager::Observer,
                                    public testing::Test {
 public:
  AutofillDownloadManagerTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        download_manager_(&driver_, this) {
    driver_.SetURLRequestContext(request_context_.get());
  }

  void LimitCache(size_t cache_size) {
    download_manager_.set_max_form_cache_size(cache_size);
  }

  // AutofillDownloadManager::Observer implementation.
  void OnLoadedServerPredictions(
      std::string response_xml,
      const std::vector<std::string>& form_signatures) override {
    ResponseData response;
    response.response = std::move(response_xml);
    response.type_of_response = QUERY_SUCCESSFULL;
    responses_.push_back(response);
  }

  void OnUploadedPossibleFieldTypes() override {
    ResponseData response;
    response.type_of_response = UPLOAD_SUCCESSFULL;
    responses_.push_back(response);
  }

  void OnServerRequestError(const std::string& form_signature,
                            AutofillDownloadManager::RequestType request_type,
                            int http_error) override {
    ResponseData response;
    response.signature = form_signature;
    response.error = http_error;
    response.type_of_response =
        request_type == AutofillDownloadManager::REQUEST_QUERY ?
            REQUEST_QUERY_FAILED : REQUEST_UPLOAD_FAILED;
    responses_.push_back(response);
  }

  enum ResponseType {
    QUERY_SUCCESSFULL,
    UPLOAD_SUCCESSFULL,
    REQUEST_QUERY_FAILED,
    REQUEST_UPLOAD_FAILED,
  };

  struct ResponseData {
    ResponseType type_of_response;
    int error;
    std::string signature;
    std::string response;

    ResponseData() : type_of_response(REQUEST_QUERY_FAILED), error(0) {}
  };

  base::MessageLoop message_loop_;
  std::list<ResponseData> responses_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  TestAutofillDriver driver_;
  AutofillDownloadManager download_manager_;
};

TEST_F(AutofillDownloadManagerTest, QueryAndUploadTest) {
  // Create and register factory.
  net::TestURLFetcherFactory factory;

  FormData form;

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("email2");
  field.name = ASCIIToUTF16("email2");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  FormStructure* form_structure = new FormStructure(form);
  ScopedVector<FormStructure> form_structures;
  form_structures.push_back(form_structure);

  form.fields.clear();

  field.label = ASCIIToUTF16("address");
  field.name = ASCIIToUTF16("address");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("address2");
  field.name = ASCIIToUTF16("address2");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("city");
  field.name = ASCIIToUTF16("city");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure = new FormStructure(form);
  form_structures.push_back(form_structure);

  form.fields.clear();

  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure = new FormStructure(form);
  form_structures.push_back(form_structure);

  // Request with id 0.
  base::HistogramTester histogram;
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures.get()));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);

  // Request with id 1.
  EXPECT_TRUE(download_manager_.StartUploadRequest(
      *(form_structures[0]), true, ServerFieldTypeSet(), std::string(), true));
  // Request with id 2.
  EXPECT_TRUE(download_manager_.StartUploadRequest(
      *(form_structures[1]), false, ServerFieldTypeSet(), std::string(), true));
  // Request with id 3. Upload request with a non-empty additional password form
  // signature.
  EXPECT_TRUE(download_manager_.StartUploadRequest(
      *(form_structures[2]), false, ServerFieldTypeSet(), "42", true));

  const char* responses[] = {
      "<autofillqueryresponse>"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"3\" />"
      "<field autofilltype=\"5\" />"
      "<field autofilltype=\"9\" />"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"30\" />"
      "<field autofilltype=\"31\" />"
      "<field autofilltype=\"33\" />"
      "</autofillqueryresponse>",
      "", "<html></html>",
  };

  // Return them out of sequence.

  // Request 1: Successful upload.
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(fetcher);
  FakeOnURLFetchComplete(fetcher, net::HTTP_OK, std::string(responses[1]));

  // Request 2: Unsuccessful upload.
  fetcher = factory.GetFetcherByID(2);
  ASSERT_TRUE(fetcher);
  FakeOnURLFetchComplete(fetcher, net::HTTP_NOT_FOUND,
                         std::string(responses[2]));
  // Request 0: Successful query.
  fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  FakeOnURLFetchComplete(fetcher, net::HTTP_OK, std::string(responses[0]));
  EXPECT_EQ(3U, responses_.size());

  // Check Request 1.
  EXPECT_EQ(AutofillDownloadManagerTest::UPLOAD_SUCCESSFULL,
            responses_.front().type_of_response);
  EXPECT_EQ(0, responses_.front().error);
  EXPECT_EQ(std::string(), responses_.front().signature);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), responses_.front().response);
  responses_.pop_front();

  // Check Request 2.
  EXPECT_EQ(AutofillDownloadManagerTest::REQUEST_UPLOAD_FAILED,
            responses_.front().type_of_response);
  EXPECT_EQ(net::HTTP_NOT_FOUND, responses_.front().error);
  EXPECT_EQ(form_structures[1]->FormSignature(),
            responses_.front().signature);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), responses_.front().response);
  responses_.pop_front();

  // Check Request 0.
  EXPECT_EQ(responses_.front().type_of_response,
            AutofillDownloadManagerTest::QUERY_SUCCESSFULL);
  EXPECT_EQ(0, responses_.front().error);
  EXPECT_EQ(std::string(), responses_.front().signature);
  EXPECT_EQ(responses[0], responses_.front().response);
  responses_.pop_front();

  // Modify form structures to miss the cache.
  field.label = ASCIIToUTF16("Address line 2");
  field.name = ASCIIToUTF16("address2");
  field.form_control_type = "text";
  form.fields.push_back(field);
  form_structure = new FormStructure(form);
  form_structures.push_back(form_structure);

  // Request with id 4, not successful.
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures.get()));
  fetcher = factory.GetFetcherByID(4);
  ASSERT_TRUE(fetcher);
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 2);
  FakeOnURLFetchComplete(fetcher, net::HTTP_INTERNAL_SERVER_ERROR,
                         std::string(responses[0]));

  // Check Request 4.
  EXPECT_EQ(AutofillDownloadManagerTest::REQUEST_QUERY_FAILED,
            responses_.front().type_of_response);
  EXPECT_EQ(net::HTTP_INTERNAL_SERVER_ERROR, responses_.front().error);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), responses_.front().response);
  responses_.pop_front();
}

TEST_F(AutofillDownloadManagerTest, BackoffLogic_Query) {
  // Create and register factory.
  net::TestURLFetcherFactory factory;

  FormData form;
  FormFieldData field;
  field.label = ASCIIToUTF16("address");
  field.name = ASCIIToUTF16("address");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("address2");
  field.name = ASCIIToUTF16("address2");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("city");
  field.name = ASCIIToUTF16("city");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  FormStructure* form_structure = new FormStructure(form);
  ScopedVector<FormStructure> form_structures;
  form_structures.push_back(form_structure);

  // Request with id 0.
  base::HistogramTester histogram;
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures.get()));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  // Request error incurs a retry after 1 second.
  FakeOnURLFetchComplete(fetcher, net::HTTP_NOT_FOUND, "<html></html>");
  EXPECT_EQ(1U, responses_.size());
  EXPECT_LT(download_manager_.fetcher_backoff_.GetTimeUntilRelease(),
            base::TimeDelta::FromMilliseconds(1100));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      base::TimeDelta::FromMilliseconds(1100));
  base::RunLoop().Run();

  // Get the retried request.
  fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(fetcher);

  // Next error incurs a retry after 2 seconds.
  FakeOnURLFetchComplete(fetcher, net::HTTP_INTERNAL_SERVER_ERROR, "");
  EXPECT_EQ(2U, responses_.size());
  EXPECT_LT(download_manager_.fetcher_backoff_.GetTimeUntilRelease(),
            base::TimeDelta::FromMilliseconds(2100));
}

TEST_F(AutofillDownloadManagerTest, BackoffLogic_Upload) {
  // Create and register factory.
  net::TestURLFetcherFactory factory;

  FormData form;
  FormFieldData field;
  field.label = ASCIIToUTF16("address");
  field.name = ASCIIToUTF16("address");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("address2");
  field.name = ASCIIToUTF16("address2");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("city");
  field.name = ASCIIToUTF16("city");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  std::unique_ptr<FormStructure> form_structure(new FormStructure(form));

  // Request with id 0.
  EXPECT_TRUE(download_manager_.StartUploadRequest(
      *form_structure, true, ServerFieldTypeSet(), std::string(), true));

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  // Error incurs a retry after 1 second.
  FakeOnURLFetchComplete(fetcher, net::HTTP_NOT_FOUND, "<html></html>");
  EXPECT_EQ(1U, responses_.size());
  EXPECT_LT(download_manager_.fetcher_backoff_.GetTimeUntilRelease(),
            base::TimeDelta::FromMilliseconds(1100));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      base::TimeDelta::FromMilliseconds(1100));
  base::RunLoop().Run();

  // Check that it was a failure.
  EXPECT_EQ(AutofillDownloadManagerTest::REQUEST_UPLOAD_FAILED,
            responses_.front().type_of_response);
  EXPECT_EQ(net::HTTP_NOT_FOUND, responses_.front().error);
  EXPECT_EQ(form_structure->FormSignature(), responses_.front().signature);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), responses_.front().response);
  responses_.pop_front();

  // Get the retried request, and make it successful.
  fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(fetcher);
  FakeOnURLFetchComplete(fetcher, net::HTTP_OK, "");

  // Check success of response.
  EXPECT_EQ(AutofillDownloadManagerTest::UPLOAD_SUCCESSFULL,
            responses_.front().type_of_response);
  EXPECT_EQ(0, responses_.front().error);
  EXPECT_EQ(std::string(), responses_.front().signature);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), responses_.front().response);
  responses_.pop_front();
}

TEST_F(AutofillDownloadManagerTest, QueryTooManyFieldsTest) {
  // Create and register factory.
  net::TestURLFetcherFactory factory;

  // Create a query that contains too many fields for the server.
  std::vector<FormData> forms(21);
  ScopedVector<FormStructure> form_structures;
  for (auto& form : forms) {
    for (size_t i = 0; i < 5; ++i) {
      FormFieldData field;
      field.label = base::IntToString16(i);
      field.name = base::IntToString16(i);
      field.form_control_type = "text";
      form.fields.push_back(field);
    }
    FormStructure* form_structure = new FormStructure(form);
    form_structures.push_back(form_structure);
  }

  // Check whether the query is aborted.
  EXPECT_FALSE(download_manager_.StartQueryRequest(form_structures.get()));
}

TEST_F(AutofillDownloadManagerTest, QueryNotTooManyFieldsTest) {
  // Create and register factory.
  net::TestURLFetcherFactory factory;

  // Create a query that contains a lot of fields, but not too many for the
  // server.
  std::vector<FormData> forms(25);
  ScopedVector<FormStructure> form_structures;
  for (auto& form : forms) {
    for (size_t i = 0; i < 4; ++i) {
      FormFieldData field;
      field.label = base::IntToString16(i);
      field.name = base::IntToString16(i);
      field.form_control_type = "text";
      form.fields.push_back(field);
    }
    FormStructure* form_structure = new FormStructure(form);
    form_structures.push_back(form_structure);
  }

  // Check that the query is not aborted.
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures.get()));
}

TEST_F(AutofillDownloadManagerTest, CacheQueryTest) {
  // Create and register factory.
  net::TestURLFetcherFactory factory;

  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  form.fields.push_back(field);

  FormStructure* form_structure = new FormStructure(form);
  ScopedVector<FormStructure> form_structures0;
  form_structures0.push_back(form_structure);

  // Add a slightly different form, which should result in a different request.
  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);
  form_structure = new FormStructure(form);
  ScopedVector<FormStructure> form_structures1;
  form_structures1.push_back(form_structure);

  // Add another slightly different form, which should also result in a
  // different request.
  field.label = ASCIIToUTF16("email2");
  field.name = ASCIIToUTF16("email2");
  form.fields.push_back(field);
  form_structure = new FormStructure(form);
  ScopedVector<FormStructure> form_structures2;
  form_structures2.push_back(form_structure);

  // Limit cache to two forms.
  LimitCache(2);

  const char *responses[] = {
    "<autofillqueryresponse>"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"3\" />"
      "<field autofilltype=\"5\" />"
    "</autofillqueryresponse>",
    "<autofillqueryresponse>"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"3\" />"
      "<field autofilltype=\"5\" />"
      "<field autofilltype=\"9\" />"
    "</autofillqueryresponse>",
    "<autofillqueryresponse>"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"3\" />"
      "<field autofilltype=\"5\" />"
      "<field autofilltype=\"9\" />"
      "<field autofilltype=\"0\" />"
    "</autofillqueryresponse>",
  };

  base::HistogramTester histogram;
  // Request with id 0.
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures0.get()));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 1);

  // No responses yet
  EXPECT_EQ(0U, responses_.size());

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  FakeOnURLFetchComplete(fetcher, 200, std::string(responses[0]));
  ASSERT_EQ(1U, responses_.size());
  EXPECT_EQ(responses[0], responses_.front().response);

  responses_.clear();

  // No actual request - should be a cache hit.
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures0.get()));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 2);
  // Data is available immediately from cache - no over-the-wire trip.
  ASSERT_EQ(1U, responses_.size());
  EXPECT_EQ(responses[0], responses_.front().response);
  responses_.clear();

  // Request with id 1.
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures1.get()));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 3);
  // No responses yet
  EXPECT_EQ(0U, responses_.size());

  fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(fetcher);
  FakeOnURLFetchComplete(fetcher, 200, std::string(responses[1]));
  ASSERT_EQ(1U, responses_.size());
  EXPECT_EQ(responses[1], responses_.front().response);

  responses_.clear();

  // Request with id 2.
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures2.get()));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 4);

  fetcher = factory.GetFetcherByID(2);
  ASSERT_TRUE(fetcher);
  FakeOnURLFetchComplete(fetcher, 200, std::string(responses[2]));
  ASSERT_EQ(1U, responses_.size());
  EXPECT_EQ(responses[2], responses_.front().response);

  responses_.clear();

  // No actual requests - should be a cache hit.
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures1.get()));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 5);

  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures2.get()));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 6);

  ASSERT_EQ(2U, responses_.size());
  EXPECT_EQ(responses[1], responses_.front().response);
  EXPECT_EQ(responses[2], responses_.back().response);
  responses_.clear();

  // The first structure should've expired.
  // Request with id 3.
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures0.get()));
  histogram.ExpectUniqueSample("Autofill.ServerQueryResponse",
                               AutofillMetrics::QUERY_SENT, 7);
  // No responses yet
  EXPECT_EQ(0U, responses_.size());

  fetcher = factory.GetFetcherByID(3);
  ASSERT_TRUE(fetcher);
  FakeOnURLFetchComplete(fetcher, 200, std::string(responses[0]));
  ASSERT_EQ(1U, responses_.size());
  EXPECT_EQ(responses[0], responses_.front().response);
}

}  // namespace autofill
