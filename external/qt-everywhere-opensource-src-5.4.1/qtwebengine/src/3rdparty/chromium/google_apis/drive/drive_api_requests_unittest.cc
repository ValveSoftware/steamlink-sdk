// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/drive_api_requests.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "google_apis/drive/dummy_auth_service.h"
#include "google_apis/drive/request_sender.h"
#include "google_apis/drive/test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestETag[] = "test_etag";
const char kTestUserAgent[] = "test-user-agent";

const char kTestChildrenResponse[] =
    "{\n"
    "\"kind\": \"drive#childReference\",\n"
    "\"id\": \"resource_id\",\n"
    "\"selfLink\": \"self_link\",\n"
    "\"childLink\": \"child_link\",\n"
    "}\n";

const char kTestPermissionResponse[] =
    "{\n"
    "\"kind\": \"drive#permission\",\n"
    "\"id\": \"resource_id\",\n"
    "\"selfLink\": \"self_link\",\n"
    "}\n";

const char kTestUploadExistingFilePath[] = "/upload/existingfile/path";
const char kTestUploadNewFilePath[] = "/upload/newfile/path";
const char kTestDownloadPathPrefix[] = "/download/";

// Used as a GetContentCallback.
void AppendContent(std::string* out,
                   GDataErrorCode error,
                   scoped_ptr<std::string> content) {
  EXPECT_EQ(HTTP_SUCCESS, error);
  out->append(*content);
}

}  // namespace

class DriveApiRequestsTest : public testing::Test {
 public:
  DriveApiRequestsTest() {
  }

  virtual void SetUp() OVERRIDE {
    request_context_getter_ = new net::TestURLRequestContextGetter(
        message_loop_.message_loop_proxy());

    request_sender_.reset(new RequestSender(new DummyAuthService,
                                            request_context_getter_.get(),
                                            message_loop_.message_loop_proxy(),
                                            kTestUserAgent));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(test_server_.InitializeAndWaitUntilReady());
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleChildrenDeleteRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleDataFileRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleDeleteRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandlePreconditionFailedRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleResumeUploadRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleInitiateUploadRequest,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleContentResponse,
                   base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::Bind(&DriveApiRequestsTest::HandleDownloadRequest,
                   base::Unretained(this)));

    GURL test_base_url = test_util::GetBaseUrlForTesting(test_server_.port());
    url_generator_.reset(new DriveApiUrlGenerator(
        test_base_url, test_base_url.Resolve(kTestDownloadPathPrefix)));

    // Reset the server's expected behavior just in case.
    ResetExpectedResponse();
    received_bytes_ = 0;
    content_length_ = 0;
  }

  base::MessageLoopForIO message_loop_;  // Test server needs IO thread.
  net::test_server::EmbeddedTestServer test_server_;
  scoped_ptr<RequestSender> request_sender_;
  scoped_ptr<DriveApiUrlGenerator> url_generator_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  base::ScopedTempDir temp_dir_;

  // This is a path to the file which contains expected response from
  // the server. See also HandleDataFileRequest below.
  base::FilePath expected_data_file_path_;

  // This is a path string in the expected response header from the server
  // for initiating file uploading.
  std::string expected_upload_path_;

  // This is a path to the file which contains expected response for
  // PRECONDITION_FAILED response.
  base::FilePath expected_precondition_failed_file_path_;

  // These are content and its type in the expected response from the server.
  // See also HandleContentResponse below.
  std::string expected_content_type_;
  std::string expected_content_;

  // The incoming HTTP request is saved so tests can verify the request
  // parameters like HTTP method (ex. some requests should use DELETE
  // instead of GET).
  net::test_server::HttpRequest http_request_;

 private:
  void ResetExpectedResponse() {
    expected_data_file_path_.clear();
    expected_upload_path_.clear();
    expected_content_type_.clear();
    expected_content_.clear();
  }

  // For "Children: delete" request, the server will return "204 No Content"
  // response meaning "success".
  scoped_ptr<net::test_server::HttpResponse> HandleChildrenDeleteRequest(
      const net::test_server::HttpRequest& request) {
    if (request.method != net::test_server::METHOD_DELETE ||
        request.relative_url.find("/children/") == string::npos) {
      // The request is not the "Children: delete" request. Delegate the
      // processing to the next handler.
      return scoped_ptr<net::test_server::HttpResponse>();
    }

    http_request_ = request;

    // Return the response with just "204 No Content" status code.
    scoped_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_NO_CONTENT);
    return http_response.PassAs<net::test_server::HttpResponse>();
  }

  // Reads the data file of |expected_data_file_path_| and returns its content
  // for the request.
  // To use this method, it is necessary to set |expected_data_file_path_|
  // to the appropriate file path before sending the request to the server.
  scoped_ptr<net::test_server::HttpResponse> HandleDataFileRequest(
      const net::test_server::HttpRequest& request) {
    if (expected_data_file_path_.empty()) {
      // The file is not specified. Delegate the processing to the next
      // handler.
      return scoped_ptr<net::test_server::HttpResponse>();
    }

    http_request_ = request;

    // Return the response from the data file.
    return test_util::CreateHttpResponseFromFile(
        expected_data_file_path_).PassAs<net::test_server::HttpResponse>();
  }

  // Deletes the resource and returns no content with HTTP_NO_CONTENT status
  // code.
  scoped_ptr<net::test_server::HttpResponse> HandleDeleteRequest(
      const net::test_server::HttpRequest& request) {
    if (request.method != net::test_server::METHOD_DELETE ||
        request.relative_url.find("/files/") == string::npos) {
      // The file is not file deletion request. Delegate the processing to the
      // next handler.
      return scoped_ptr<net::test_server::HttpResponse>();
    }

    http_request_ = request;

    scoped_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_NO_CONTENT);

    return response.PassAs<net::test_server::HttpResponse>();
  }

  // Returns PRECONDITION_FAILED response for ETag mismatching with error JSON
  // content specified by |expected_precondition_failed_file_path_|.
  // To use this method, it is necessary to set the variable to the appropriate
  // file path before sending the request to the server.
  scoped_ptr<net::test_server::HttpResponse> HandlePreconditionFailedRequest(
      const net::test_server::HttpRequest& request) {
    if (expected_precondition_failed_file_path_.empty()) {
      // The file is not specified. Delegate the process to the next handler.
      return scoped_ptr<net::test_server::HttpResponse>();
    }

    http_request_ = request;

    scoped_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_PRECONDITION_FAILED);

    std::string content;
    if (base::ReadFileToString(expected_precondition_failed_file_path_,
                               &content)) {
      response->set_content(content);
      response->set_content_type("application/json");
    }

    return response.PassAs<net::test_server::HttpResponse>();
  }

  // Returns the response based on set expected upload url.
  // The response contains the url in its "Location: " header. Also, it doesn't
  // have any content.
  // To use this method, it is necessary to set |expected_upload_path_|
  // to the string representation of the url to be returned.
  scoped_ptr<net::test_server::HttpResponse> HandleInitiateUploadRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == expected_upload_path_ ||
        expected_upload_path_.empty()) {
      // The request is for resume uploading or the expected upload url is not
      // set. Delegate the processing to the next handler.
      return scoped_ptr<net::test_server::HttpResponse>();
    }

    http_request_ = request;

    scoped_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);

    // Check if the X-Upload-Content-Length is present. If yes, store the
    // length of the file.
    std::map<std::string, std::string>::const_iterator found =
        request.headers.find("X-Upload-Content-Length");
    if (found == request.headers.end() ||
        !base::StringToInt64(found->second, &content_length_)) {
      return scoped_ptr<net::test_server::HttpResponse>();
    }
    received_bytes_ = 0;

    response->set_code(net::HTTP_OK);
    response->AddCustomHeader(
        "Location",
        test_server_.base_url().Resolve(expected_upload_path_).spec());
    return response.PassAs<net::test_server::HttpResponse>();
  }

  scoped_ptr<net::test_server::HttpResponse> HandleResumeUploadRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != expected_upload_path_) {
      // The request path is different from the expected path for uploading.
      // Delegate the processing to the next handler.
      return scoped_ptr<net::test_server::HttpResponse>();
    }

    http_request_ = request;

    if (!request.content.empty()) {
      std::map<std::string, std::string>::const_iterator iter =
          request.headers.find("Content-Range");
      if (iter == request.headers.end()) {
        // The range must be set.
        return scoped_ptr<net::test_server::HttpResponse>();
      }

      int64 length = 0;
      int64 start_position = 0;
      int64 end_position = 0;
      if (!test_util::ParseContentRangeHeader(
              iter->second, &start_position, &end_position, &length)) {
        // Invalid "Content-Range" value.
        return scoped_ptr<net::test_server::HttpResponse>();
      }

      EXPECT_EQ(start_position, received_bytes_);
      EXPECT_EQ(length, content_length_);

      // end_position is inclusive, but so +1 to change the range to byte size.
      received_bytes_ = end_position + 1;
    }

    if (received_bytes_ < content_length_) {
      scoped_ptr<net::test_server::BasicHttpResponse> response(
          new net::test_server::BasicHttpResponse);
      // Set RESUME INCOMPLETE (308) status code.
      response->set_code(static_cast<net::HttpStatusCode>(308));

      // Add Range header to the response, based on the values of
      // Content-Range header in the request.
      // The header is annotated only when at least one byte is received.
      if (received_bytes_ > 0) {
        response->AddCustomHeader(
            "Range", "bytes=0-" + base::Int64ToString(received_bytes_ - 1));
      }

      return response.PassAs<net::test_server::HttpResponse>();
    }

    // All bytes are received. Return the "success" response with the file's
    // (dummy) metadata.
    scoped_ptr<net::test_server::BasicHttpResponse> response =
        test_util::CreateHttpResponseFromFile(
            test_util::GetTestFilePath("drive/file_entry.json"));

    // The response code is CREATED if it is new file uploading.
    if (http_request_.relative_url == kTestUploadNewFilePath) {
      response->set_code(net::HTTP_CREATED);
    }

    return response.PassAs<net::test_server::HttpResponse>();
  }

  // Returns the response based on set expected content and its type.
  // To use this method, both |expected_content_type_| and |expected_content_|
  // must be set in advance.
  scoped_ptr<net::test_server::HttpResponse> HandleContentResponse(
      const net::test_server::HttpRequest& request) {
    if (expected_content_type_.empty() || expected_content_.empty()) {
      // Expected content is not set. Delegate the processing to the next
      // handler.
      return scoped_ptr<net::test_server::HttpResponse>();
    }

    http_request_ = request;

    scoped_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_OK);
    response->set_content_type(expected_content_type_);
    response->set_content(expected_content_);
    return response.PassAs<net::test_server::HttpResponse>();
  }

  // Handles a request for downloading a file.
  scoped_ptr<net::test_server::HttpResponse> HandleDownloadRequest(
      const net::test_server::HttpRequest& request) {
    http_request_ = request;

    const GURL absolute_url = test_server_.GetURL(request.relative_url);
    std::string id;
    if (!test_util::RemovePrefix(absolute_url.path(),
                                 kTestDownloadPathPrefix,
                                 &id)) {
      return scoped_ptr<net::test_server::HttpResponse>();
    }

    // For testing, returns a text with |id| repeated 3 times.
    scoped_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_OK);
    response->set_content(id + id + id);
    response->set_content_type("text/plain");
    return response.PassAs<net::test_server::HttpResponse>();
  }

  // These are for the current upload file status.
  int64 received_bytes_;
  int64 content_length_;
};

TEST_F(DriveApiRequestsTest, DriveApiDataRequest_Fields) {
  // Make sure that "fields" query param is supported by using its subclass,
  // AboutGetRequest.

  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "drive/about.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AboutResource> about_resource;

  {
    base::RunLoop run_loop;
    drive::AboutGetRequest* request = new drive::AboutGetRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&error, &about_resource)));
    request->set_fields(
        "kind,quotaBytesTotal,quotaBytesUsed,largestChangeId,rootFolderId");
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/drive/v2/about?"
            "fields=kind%2CquotaBytesTotal%2CquotaBytesUsed%2C"
            "largestChangeId%2CrootFolderId",
            http_request_.relative_url);

  scoped_ptr<AboutResource> expected(
      AboutResource::CreateFrom(
          *test_util::LoadJSONFile("drive/about.json")));
  ASSERT_TRUE(about_resource.get());
  EXPECT_EQ(expected->largest_change_id(), about_resource->largest_change_id());
  EXPECT_EQ(expected->quota_bytes_total(), about_resource->quota_bytes_total());
  EXPECT_EQ(expected->quota_bytes_used(), about_resource->quota_bytes_used());
  EXPECT_EQ(expected->root_folder_id(), about_resource->root_folder_id());
}

TEST_F(DriveApiRequestsTest, FilesInsertRequest) {
  const base::Time::Exploded kModifiedDate = {2012, 7, 0, 19, 15, 59, 13, 123};
  const base::Time::Exploded kLastViewedByMeDate =
      {2013, 7, 0, 19, 15, 59, 13, 123};

  // Set an expected data file containing the directory's entry data.
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/directory_entry.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<FileResource> file_resource;

  // Create "new directory" in the root directory.
  {
    base::RunLoop run_loop;
    drive::FilesInsertRequest* request = new drive::FilesInsertRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&error, &file_resource)));
    request->set_last_viewed_by_me_date(
        base::Time::FromUTCExploded(kLastViewedByMeDate));
    request->set_mime_type("application/vnd.google-apps.folder");
    request->set_modified_date(base::Time::FromUTCExploded(kModifiedDate));
    request->add_parent("root");
    request->set_title("new directory");
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files", http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"lastViewedByMeDate\":\"2013-07-19T15:59:13.123Z\","
            "\"mimeType\":\"application/vnd.google-apps.folder\","
            "\"modifiedDate\":\"2012-07-19T15:59:13.123Z\","
            "\"parents\":[{\"id\":\"root\"}],"
            "\"title\":\"new directory\"}",
            http_request_.content);

  scoped_ptr<FileResource> expected(
      FileResource::CreateFrom(
          *test_util::LoadJSONFile("drive/directory_entry.json")));

  // Sanity check.
  ASSERT_TRUE(file_resource.get());

  EXPECT_EQ(expected->file_id(), file_resource->file_id());
  EXPECT_EQ(expected->title(), file_resource->title());
  EXPECT_EQ(expected->mime_type(), file_resource->mime_type());
  EXPECT_EQ(expected->parents().size(), file_resource->parents().size());
}

TEST_F(DriveApiRequestsTest, FilesPatchRequest) {
  const base::Time::Exploded kModifiedDate = {2012, 7, 0, 19, 15, 59, 13, 123};
  const base::Time::Exploded kLastViewedByMeDate =
      {2013, 7, 0, 19, 15, 59, 13, 123};

  // Set an expected data file containing valid result.
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/file_entry.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<FileResource> file_resource;

  {
    base::RunLoop run_loop;
    drive::FilesPatchRequest* request = new drive::FilesPatchRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&error, &file_resource)));
    request->set_file_id("resource_id");
    request->set_set_modified_date(true);
    request->set_update_viewed_date(false);

    request->set_title("new title");
    request->set_modified_date(base::Time::FromUTCExploded(kModifiedDate));
    request->set_last_viewed_by_me_date(
        base::Time::FromUTCExploded(kLastViewedByMeDate));
    request->add_parent("parent_resource_id");

    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_PATCH, http_request_.method);
  EXPECT_EQ("/drive/v2/files/resource_id"
            "?setModifiedDate=true&updateViewedDate=false",
            http_request_.relative_url);

  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"lastViewedByMeDate\":\"2013-07-19T15:59:13.123Z\","
            "\"modifiedDate\":\"2012-07-19T15:59:13.123Z\","
            "\"parents\":[{\"id\":\"parent_resource_id\"}],"
            "\"title\":\"new title\"}",
            http_request_.content);
  EXPECT_TRUE(file_resource);
}

TEST_F(DriveApiRequestsTest, AboutGetRequest_ValidJson) {
  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "drive/about.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AboutResource> about_resource;

  {
    base::RunLoop run_loop;
    drive::AboutGetRequest* request = new drive::AboutGetRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&error, &about_resource)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/drive/v2/about", http_request_.relative_url);

  scoped_ptr<AboutResource> expected(
      AboutResource::CreateFrom(
          *test_util::LoadJSONFile("drive/about.json")));
  ASSERT_TRUE(about_resource.get());
  EXPECT_EQ(expected->largest_change_id(), about_resource->largest_change_id());
  EXPECT_EQ(expected->quota_bytes_total(), about_resource->quota_bytes_total());
  EXPECT_EQ(expected->quota_bytes_used(), about_resource->quota_bytes_used());
  EXPECT_EQ(expected->root_folder_id(), about_resource->root_folder_id());
}

TEST_F(DriveApiRequestsTest, AboutGetRequest_InvalidJson) {
  // Set an expected data file containing invalid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "gdata/testfile.txt");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AboutResource> about_resource;

  {
    base::RunLoop run_loop;
    drive::AboutGetRequest* request = new drive::AboutGetRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&error, &about_resource)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  // "parse error" should be returned, and the about resource should be NULL.
  EXPECT_EQ(GDATA_PARSE_ERROR, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/drive/v2/about", http_request_.relative_url);
  EXPECT_FALSE(about_resource);
}

TEST_F(DriveApiRequestsTest, AppsListRequest) {
  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "drive/applist.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AppList> app_list;

  {
    base::RunLoop run_loop;
    drive::AppsListRequest* request = new drive::AppsListRequest(
        request_sender_.get(),
        *url_generator_,
        false,  // use_internal_endpoint
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&error, &app_list)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/drive/v2/apps", http_request_.relative_url);
  EXPECT_TRUE(app_list);
}

TEST_F(DriveApiRequestsTest, ChangesListRequest) {
  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "drive/changelist.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ChangeList> result;

  {
    base::RunLoop run_loop;
    drive::ChangesListRequest* request = new drive::ChangesListRequest(
        request_sender_.get(), *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&error, &result)));
    request->set_include_deleted(true);
    request->set_start_change_id(100);
    request->set_max_results(500);
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/drive/v2/changes?maxResults=500&startChangeId=100",
            http_request_.relative_url);
  EXPECT_TRUE(result);
}

TEST_F(DriveApiRequestsTest, ChangesListNextPageRequest) {
  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "drive/changelist.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ChangeList> result;

  {
    base::RunLoop run_loop;
    drive::ChangesListNextPageRequest* request =
        new drive::ChangesListNextPageRequest(
            request_sender_.get(),
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &result)));
    request->set_next_link(test_server_.GetURL("/continue/get/change/list"));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/continue/get/change/list", http_request_.relative_url);
  EXPECT_TRUE(result);
}

TEST_F(DriveApiRequestsTest, FilesCopyRequest) {
  const base::Time::Exploded kModifiedDate = {2012, 7, 0, 19, 15, 59, 13, 123};

  // Set an expected data file containing the dummy file entry data.
  // It'd be returned if we copy a file.
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/file_entry.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<FileResource> file_resource;

  // Copy the file to a new file named "new title".
  {
    base::RunLoop run_loop;
    drive::FilesCopyRequest* request = new drive::FilesCopyRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&error, &file_resource)));
    request->set_file_id("resource_id");
    request->set_modified_date(base::Time::FromUTCExploded(kModifiedDate));
    request->add_parent("parent_resource_id");
    request->set_title("new title");
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files/resource_id/copy", http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(
      "{\"modifiedDate\":\"2012-07-19T15:59:13.123Z\","
      "\"parents\":[{\"id\":\"parent_resource_id\"}],\"title\":\"new title\"}",
      http_request_.content);
  EXPECT_TRUE(file_resource);
}

TEST_F(DriveApiRequestsTest, FilesCopyRequest_EmptyParentResourceId) {
  // Set an expected data file containing the dummy file entry data.
  // It'd be returned if we copy a file.
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/file_entry.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<FileResource> file_resource;

  // Copy the file to a new file named "new title".
  {
    base::RunLoop run_loop;
    drive::FilesCopyRequest* request = new drive::FilesCopyRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&error, &file_resource)));
    request->set_file_id("resource_id");
    request->set_title("new title");
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files/resource_id/copy", http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"title\":\"new title\"}", http_request_.content);
  EXPECT_TRUE(file_resource);
}

TEST_F(DriveApiRequestsTest, FilesListRequest) {
  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "drive/filelist.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<FileList> result;

  {
    base::RunLoop run_loop;
    drive::FilesListRequest* request = new drive::FilesListRequest(
        request_sender_.get(), *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&error, &result)));
    request->set_max_results(50);
    request->set_q("\"abcde\" in parents");
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/drive/v2/files?maxResults=50&q=%22abcde%22+in+parents",
            http_request_.relative_url);
  EXPECT_TRUE(result);
}

TEST_F(DriveApiRequestsTest, FilesListNextPageRequest) {
  // Set an expected data file containing valid result.
  expected_data_file_path_ = test_util::GetTestFilePath(
      "drive/filelist.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<FileList> result;

  {
    base::RunLoop run_loop;
    drive::FilesListNextPageRequest* request =
        new drive::FilesListNextPageRequest(
            request_sender_.get(),
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &result)));
    request->set_next_link(test_server_.GetURL("/continue/get/file/list"));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/continue/get/file/list", http_request_.relative_url);
  EXPECT_TRUE(result);
}

TEST_F(DriveApiRequestsTest, FilesDeleteRequest) {
  GDataErrorCode error = GDATA_OTHER_ERROR;

  // Delete a resource with the given resource id.
  {
    base::RunLoop run_loop;
    drive::FilesDeleteRequest* request = new drive::FilesDeleteRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop, test_util::CreateCopyResultCallback(&error)));
    request->set_file_id("resource_id");
    request->set_etag(kTestETag);
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_NO_CONTENT, error);
  EXPECT_EQ(net::test_server::METHOD_DELETE, http_request_.method);
  EXPECT_EQ(kTestETag, http_request_.headers["If-Match"]);
  EXPECT_EQ("/drive/v2/files/resource_id", http_request_.relative_url);
  EXPECT_FALSE(http_request_.has_content);
}

TEST_F(DriveApiRequestsTest, FilesTrashRequest) {
  // Set data for the expected result. Directory entry should be returned
  // if the trashing entry is a directory, so using it here should be fine.
  expected_data_file_path_ =
      test_util::GetTestFilePath("drive/directory_entry.json");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<FileResource> file_resource;

  // Trash a resource with the given resource id.
  {
    base::RunLoop run_loop;
    drive::FilesTrashRequest* request = new drive::FilesTrashRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&error, &file_resource)));
    request->set_file_id("resource_id");
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files/resource_id/trash", http_request_.relative_url);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(http_request_.content.empty());
}

TEST_F(DriveApiRequestsTest, ChildrenInsertRequest) {
  // Set an expected data file containing the children entry.
  expected_content_type_ = "application/json";
  expected_content_ = kTestChildrenResponse;

  GDataErrorCode error = GDATA_OTHER_ERROR;

  // Add a resource with "resource_id" to a directory with
  // "parent_resource_id".
  {
    base::RunLoop run_loop;
    drive::ChildrenInsertRequest* request = new drive::ChildrenInsertRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&error)));
    request->set_folder_id("parent_resource_id");
    request->set_id("resource_id");
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files/parent_resource_id/children",
            http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"id\":\"resource_id\"}", http_request_.content);
}

TEST_F(DriveApiRequestsTest, ChildrenDeleteRequest) {
  GDataErrorCode error = GDATA_OTHER_ERROR;

  // Remove a resource with "resource_id" from a directory with
  // "parent_resource_id".
  {
    base::RunLoop run_loop;
    drive::ChildrenDeleteRequest* request = new drive::ChildrenDeleteRequest(
        request_sender_.get(),
        *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&error)));
    request->set_child_id("resource_id");
    request->set_folder_id("parent_resource_id");
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_NO_CONTENT, error);
  EXPECT_EQ(net::test_server::METHOD_DELETE, http_request_.method);
  EXPECT_EQ("/drive/v2/files/parent_resource_id/children/resource_id",
            http_request_.relative_url);
  EXPECT_FALSE(http_request_.has_content);
}

TEST_F(DriveApiRequestsTest, UploadNewFileRequest) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadNewFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');
  const base::FilePath kTestFilePath =
      temp_dir_.path().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kTestContent));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with
  // "parent_resource_id".
  {
    base::RunLoop run_loop;
    drive::InitiateUploadNewFileRequest* request =
        new drive::InitiateUploadNewFileRequest(
            request_sender_.get(),
            *url_generator_,
            kTestContentType,
            kTestContent.size(),
            "parent_resource_id",  // The resource id of the parent directory.
            "new file title",  // The title of the file being uploaded.
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadNewFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);

  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/upload/drive/v2/files?uploadType=resumable",
            http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"parents\":[{"
            "\"id\":\"parent_resource_id\","
            "\"kind\":\"drive#fileLink\""
            "}],"
            "\"title\":\"new file title\"}",
            http_request_.content);

  // Upload the content to the upload URL.
  UploadRangeResponse response;
  scoped_ptr<FileResource> new_entry;

  {
    base::RunLoop run_loop;
    drive::ResumeUploadRequest* resume_request =
        new drive::ResumeUploadRequest(
            request_sender_.get(),
            upload_url,
            0,  // start_position
            kTestContent.size(),  // end_position (exclusive)
            kTestContent.size(),  // content_length,
            kTestContentType,
            kTestFilePath,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&response, &new_entry)),
            ProgressCallback());
    request_sender_->StartRequestWithRetry(resume_request);
    run_loop.Run();
  }

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should be added.
  EXPECT_EQ("bytes 0-" +
            base::Int64ToString(kTestContent.size() - 1) + "/" +
            base::Int64ToString(kTestContent.size()),
            http_request_.headers["Content-Range"]);
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kTestContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_CREATED, response.code);  // Because it's a new file
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

TEST_F(DriveApiRequestsTest, UploadNewEmptyFileRequest) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadNewFilePath;

  const char kTestContentType[] = "text/plain";
  const char kTestContent[] = "";
  const base::FilePath kTestFilePath =
      temp_dir_.path().AppendASCII("empty_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kTestContent));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    drive::InitiateUploadNewFileRequest* request =
        new drive::InitiateUploadNewFileRequest(
            request_sender_.get(),
            *url_generator_,
            kTestContentType,
            0,
            "parent_resource_id",  // The resource id of the parent directory.
            "new file title",  // The title of the file being uploaded.
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadNewFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ("0", http_request_.headers["X-Upload-Content-Length"]);

  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/upload/drive/v2/files?uploadType=resumable",
            http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"parents\":[{"
            "\"id\":\"parent_resource_id\","
            "\"kind\":\"drive#fileLink\""
            "}],"
            "\"title\":\"new file title\"}",
            http_request_.content);

  // Upload the content to the upload URL.
  UploadRangeResponse response;
  scoped_ptr<FileResource> new_entry;

  {
    base::RunLoop run_loop;
    drive::ResumeUploadRequest* resume_request =
        new drive::ResumeUploadRequest(
            request_sender_.get(),
            upload_url,
            0,  // start_position
            0,  // end_position (exclusive)
            0,  // content_length,
            kTestContentType,
            kTestFilePath,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&response, &new_entry)),
            ProgressCallback());
    request_sender_->StartRequestWithRetry(resume_request);
    run_loop.Run();
  }

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should NOT be added.
  EXPECT_EQ(0U, http_request_.headers.count("Content-Range"));
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kTestContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_CREATED, response.code);  // Because it's a new file
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

TEST_F(DriveApiRequestsTest, UploadNewLargeFileRequest) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadNewFilePath;

  const char kTestContentType[] = "text/plain";
  const size_t kNumChunkBytes = 10;  // Num bytes in a chunk.
  const std::string kTestContent(100, 'a');
  const base::FilePath kTestFilePath =
      temp_dir_.path().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kTestContent));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    drive::InitiateUploadNewFileRequest* request =
        new drive::InitiateUploadNewFileRequest(
            request_sender_.get(),
            *url_generator_,
            kTestContentType,
            kTestContent.size(),
            "parent_resource_id",  // The resource id of the parent directory.
            "new file title",  // The title of the file being uploaded.
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadNewFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);

  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/upload/drive/v2/files?uploadType=resumable",
            http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"parents\":[{"
            "\"id\":\"parent_resource_id\","
            "\"kind\":\"drive#fileLink\""
            "}],"
            "\"title\":\"new file title\"}",
            http_request_.content);

  // Before sending any data, check the current status.
  // This is an edge case test for GetUploadStatusRequest.
  {
    UploadRangeResponse response;
    scoped_ptr<FileResource> new_entry;

    // Check the response by GetUploadStatusRequest.
    {
      base::RunLoop run_loop;
      drive::GetUploadStatusRequest* get_upload_status_request =
          new drive::GetUploadStatusRequest(
              request_sender_.get(),
              upload_url,
              kTestContent.size(),
              test_util::CreateQuitCallback(
                  &run_loop,
                  test_util::CreateCopyResultCallback(&response, &new_entry)));
      request_sender_->StartRequestWithRetry(get_upload_status_request);
      run_loop.Run();
    }

    // METHOD_PUT should be used to upload data.
    EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
    // Request should go to the upload URL.
    EXPECT_EQ(upload_url.path(), http_request_.relative_url);
    // Content-Range header should be added.
    EXPECT_EQ("bytes */" + base::Int64ToString(kTestContent.size()),
              http_request_.headers["Content-Range"]);
    EXPECT_TRUE(http_request_.has_content);
    EXPECT_TRUE(http_request_.content.empty());

    // Check the response.
    EXPECT_EQ(HTTP_RESUME_INCOMPLETE, response.code);
    EXPECT_EQ(0, response.start_position_received);
    EXPECT_EQ(0, response.end_position_received);
  }

  // Upload the content to the upload URL.
  for (size_t start_position = 0; start_position < kTestContent.size();
       start_position += kNumChunkBytes) {
    const std::string payload = kTestContent.substr(
        start_position,
        std::min(kNumChunkBytes, kTestContent.size() - start_position));
    const size_t end_position = start_position + payload.size();

    UploadRangeResponse response;
    scoped_ptr<FileResource> new_entry;

    {
      base::RunLoop run_loop;
      drive::ResumeUploadRequest* resume_request =
          new drive::ResumeUploadRequest(
              request_sender_.get(),
              upload_url,
              start_position,
              end_position,
              kTestContent.size(),  // content_length,
              kTestContentType,
              kTestFilePath,
              test_util::CreateQuitCallback(
                  &run_loop,
                  test_util::CreateCopyResultCallback(&response, &new_entry)),
              ProgressCallback());
      request_sender_->StartRequestWithRetry(resume_request);
      run_loop.Run();
    }

    // METHOD_PUT should be used to upload data.
    EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
    // Request should go to the upload URL.
    EXPECT_EQ(upload_url.path(), http_request_.relative_url);
    // Content-Range header should be added.
    EXPECT_EQ("bytes " +
              base::Int64ToString(start_position) + "-" +
              base::Int64ToString(end_position - 1) + "/" +
              base::Int64ToString(kTestContent.size()),
              http_request_.headers["Content-Range"]);
    // The upload content should be set in the HTTP request.
    EXPECT_TRUE(http_request_.has_content);
    EXPECT_EQ(payload, http_request_.content);

    if (end_position == kTestContent.size()) {
      // Check the response.
      EXPECT_EQ(HTTP_CREATED, response.code);  // Because it's a new file
      // The start and end positions should be set to -1, if an upload is
      // complete.
      EXPECT_EQ(-1, response.start_position_received);
      EXPECT_EQ(-1, response.end_position_received);
      break;
    }

    // Check the response.
    EXPECT_EQ(HTTP_RESUME_INCOMPLETE, response.code);
    EXPECT_EQ(0, response.start_position_received);
    EXPECT_EQ(static_cast<int64>(end_position), response.end_position_received);

    // Check the response by GetUploadStatusRequest.
    {
      base::RunLoop run_loop;
      drive::GetUploadStatusRequest* get_upload_status_request =
          new drive::GetUploadStatusRequest(
              request_sender_.get(),
              upload_url,
              kTestContent.size(),
              test_util::CreateQuitCallback(
                  &run_loop,
                  test_util::CreateCopyResultCallback(&response, &new_entry)));
      request_sender_->StartRequestWithRetry(get_upload_status_request);
      run_loop.Run();
    }

    // METHOD_PUT should be used to upload data.
    EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
    // Request should go to the upload URL.
    EXPECT_EQ(upload_url.path(), http_request_.relative_url);
    // Content-Range header should be added.
    EXPECT_EQ("bytes */" + base::Int64ToString(kTestContent.size()),
              http_request_.headers["Content-Range"]);
    EXPECT_TRUE(http_request_.has_content);
    EXPECT_TRUE(http_request_.content.empty());

    // Check the response.
    EXPECT_EQ(HTTP_RESUME_INCOMPLETE, response.code);
    EXPECT_EQ(0, response.start_position_received);
    EXPECT_EQ(static_cast<int64>(end_position),
              response.end_position_received);
  }
}

TEST_F(DriveApiRequestsTest, UploadNewFileWithMetadataRequest) {
  const base::Time::Exploded kModifiedDate = {2012, 7, 0, 19, 15, 59, 13, 123};
  const base::Time::Exploded kLastViewedByMeDate =
      {2013, 7, 0, 19, 15, 59, 13, 123};

  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadNewFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    drive::InitiateUploadNewFileRequest* request =
        new drive::InitiateUploadNewFileRequest(
            request_sender_.get(),
            *url_generator_,
            kTestContentType,
            kTestContent.size(),
            "parent_resource_id",  // The resource id of the parent directory.
            "new file title",  // The title of the file being uploaded.
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request->set_modified_date(base::Time::FromUTCExploded(kModifiedDate));
    request->set_last_viewed_by_me_date(
        base::Time::FromUTCExploded(kLastViewedByMeDate));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadNewFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);

  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/upload/drive/v2/files?uploadType=resumable&setModifiedDate=true",
            http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"lastViewedByMeDate\":\"2013-07-19T15:59:13.123Z\","
            "\"modifiedDate\":\"2012-07-19T15:59:13.123Z\","
            "\"parents\":[{\"id\":\"parent_resource_id\","
            "\"kind\":\"drive#fileLink\"}],"
            "\"title\":\"new file title\"}",
            http_request_.content);
}

TEST_F(DriveApiRequestsTest, UploadExistingFileRequest) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadExistingFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');
  const base::FilePath kTestFilePath =
      temp_dir_.path().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kTestContent));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    drive::InitiateUploadExistingFileRequest* request =
        new drive::InitiateUploadExistingFileRequest(
            request_sender_.get(),
            *url_generator_,
            kTestContentType,
            kTestContent.size(),
            "resource_id",  // The resource id of the file to be overwritten.
            std::string(),  // No etag.
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadExistingFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  EXPECT_EQ("*", http_request_.headers["If-Match"]);

  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("/upload/drive/v2/files/resource_id?uploadType=resumable",
            http_request_.relative_url);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(http_request_.content.empty());

  // Upload the content to the upload URL.
  UploadRangeResponse response;
  scoped_ptr<FileResource> new_entry;

  {
    base::RunLoop run_loop;
    drive::ResumeUploadRequest* resume_request =
        new drive::ResumeUploadRequest(
            request_sender_.get(),
            upload_url,
            0,  // start_position
            kTestContent.size(),  // end_position (exclusive)
            kTestContent.size(),  // content_length,
            kTestContentType,
            kTestFilePath,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&response, &new_entry)),
            ProgressCallback());
    request_sender_->StartRequestWithRetry(resume_request);
    run_loop.Run();
  }

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should be added.
  EXPECT_EQ("bytes 0-" +
            base::Int64ToString(kTestContent.size() - 1) + "/" +
            base::Int64ToString(kTestContent.size()),
            http_request_.headers["Content-Range"]);
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kTestContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_SUCCESS, response.code);  // Because it's an existing file
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

TEST_F(DriveApiRequestsTest, UploadExistingFileRequestWithETag) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadExistingFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');
  const base::FilePath kTestFilePath =
      temp_dir_.path().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kTestContent));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    drive::InitiateUploadExistingFileRequest* request =
        new drive::InitiateUploadExistingFileRequest(
            request_sender_.get(),
            *url_generator_,
            kTestContentType,
            kTestContent.size(),
            "resource_id",  // The resource id of the file to be overwritten.
            kTestETag,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadExistingFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  EXPECT_EQ(kTestETag, http_request_.headers["If-Match"]);

  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("/upload/drive/v2/files/resource_id?uploadType=resumable",
            http_request_.relative_url);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(http_request_.content.empty());

  // Upload the content to the upload URL.
  UploadRangeResponse response;
  scoped_ptr<FileResource> new_entry;

  {
    base::RunLoop run_loop;
    drive::ResumeUploadRequest* resume_request =
        new drive::ResumeUploadRequest(
            request_sender_.get(),
            upload_url,
            0,  // start_position
            kTestContent.size(),  // end_position (exclusive)
            kTestContent.size(),  // content_length,
            kTestContentType,
            kTestFilePath,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&response, &new_entry)),
            ProgressCallback());
    request_sender_->StartRequestWithRetry(resume_request);
    run_loop.Run();
  }

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should be added.
  EXPECT_EQ("bytes 0-" +
            base::Int64ToString(kTestContent.size() - 1) + "/" +
            base::Int64ToString(kTestContent.size()),
            http_request_.headers["Content-Range"]);
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kTestContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_SUCCESS, response.code);  // Because it's an existing file
  // The start and end positions should be set to -1, if an upload is complete.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);
}

TEST_F(DriveApiRequestsTest, UploadExistingFileRequestWithETagConflicting) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadExistingFilePath;

  // If it turned out that the etag is conflicting, PRECONDITION_FAILED should
  // be returned.
  expected_precondition_failed_file_path_ =
      test_util::GetTestFilePath("drive/error.json");

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    drive::InitiateUploadExistingFileRequest* request =
        new drive::InitiateUploadExistingFileRequest(
            request_sender_.get(),
            *url_generator_,
            kTestContentType,
            kTestContent.size(),
            "resource_id",  // The resource id of the file to be overwritten.
            "Conflicting-etag",
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_PRECONDITION, error);
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  EXPECT_EQ("Conflicting-etag", http_request_.headers["If-Match"]);

  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("/upload/drive/v2/files/resource_id?uploadType=resumable",
            http_request_.relative_url);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(http_request_.content.empty());
}

TEST_F(DriveApiRequestsTest,
       UploadExistingFileRequestWithETagConflictOnResumeUpload) {
  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadExistingFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');
  const base::FilePath kTestFilePath =
      temp_dir_.path().AppendASCII("upload_file.txt");
  ASSERT_TRUE(test_util::WriteStringToFile(kTestFilePath, kTestContent));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    drive::InitiateUploadExistingFileRequest* request =
        new drive::InitiateUploadExistingFileRequest(
            request_sender_.get(),
            *url_generator_,
            kTestContentType,
            kTestContent.size(),
            "resource_id",  // The resource id of the file to be overwritten.
            kTestETag,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadExistingFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  EXPECT_EQ(kTestETag, http_request_.headers["If-Match"]);

  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("/upload/drive/v2/files/resource_id?uploadType=resumable",
            http_request_.relative_url);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(http_request_.content.empty());

  // Set PRECONDITION_FAILED to the server. This is the emulation of the
  // confliction during uploading.
  expected_precondition_failed_file_path_ =
      test_util::GetTestFilePath("drive/error.json");

  // Upload the content to the upload URL.
  UploadRangeResponse response;
  scoped_ptr<FileResource> new_entry;

  {
    base::RunLoop run_loop;
    drive::ResumeUploadRequest* resume_request =
        new drive::ResumeUploadRequest(
            request_sender_.get(),
            upload_url,
            0,  // start_position
            kTestContent.size(),  // end_position (exclusive)
            kTestContent.size(),  // content_length,
            kTestContentType,
            kTestFilePath,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&response, &new_entry)),
            ProgressCallback());
    request_sender_->StartRequestWithRetry(resume_request);
    run_loop.Run();
  }

  // METHOD_PUT should be used to upload data.
  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  // Request should go to the upload URL.
  EXPECT_EQ(upload_url.path(), http_request_.relative_url);
  // Content-Range header should be added.
  EXPECT_EQ("bytes 0-" +
            base::Int64ToString(kTestContent.size() - 1) + "/" +
            base::Int64ToString(kTestContent.size()),
            http_request_.headers["Content-Range"]);
  // The upload content should be set in the HTTP request.
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ(kTestContent, http_request_.content);

  // Check the response.
  EXPECT_EQ(HTTP_PRECONDITION, response.code);
  // The start and end positions should be set to -1 for error.
  EXPECT_EQ(-1, response.start_position_received);
  EXPECT_EQ(-1, response.end_position_received);

  // New entry should be NULL.
  EXPECT_FALSE(new_entry.get());
}

TEST_F(DriveApiRequestsTest, UploadExistingFileWithMetadataRequest) {
  const base::Time::Exploded kModifiedDate = {2012, 7, 0, 19, 15, 59, 13, 123};
  const base::Time::Exploded kLastViewedByMeDate =
      {2013, 7, 0, 19, 15, 59, 13, 123};

  // Set an expected url for uploading.
  expected_upload_path_ = kTestUploadExistingFilePath;

  const char kTestContentType[] = "text/plain";
  const std::string kTestContent(100, 'a');

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_url;

  // Initiate uploading a new file to the directory with "parent_resource_id".
  {
    base::RunLoop run_loop;
    drive::InitiateUploadExistingFileRequest* request =
        new drive::InitiateUploadExistingFileRequest(
            request_sender_.get(),
            *url_generator_,
            kTestContentType,
            kTestContent.size(),
            "resource_id",  // The resource id of the file to be overwritten.
            kTestETag,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error, &upload_url)));
    request->set_parent_resource_id("new_parent_resource_id");
    request->set_title("new file title");
    request->set_modified_date(base::Time::FromUTCExploded(kModifiedDate));
    request->set_last_viewed_by_me_date(
        base::Time::FromUTCExploded(kLastViewedByMeDate));

    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(kTestUploadExistingFilePath, upload_url.path());
  EXPECT_EQ(kTestContentType, http_request_.headers["X-Upload-Content-Type"]);
  EXPECT_EQ(base::Int64ToString(kTestContent.size()),
            http_request_.headers["X-Upload-Content-Length"]);
  EXPECT_EQ(kTestETag, http_request_.headers["If-Match"]);

  EXPECT_EQ(net::test_server::METHOD_PUT, http_request_.method);
  EXPECT_EQ("/upload/drive/v2/files/resource_id?"
            "uploadType=resumable&setModifiedDate=true",
            http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_EQ("{\"lastViewedByMeDate\":\"2013-07-19T15:59:13.123Z\","
            "\"modifiedDate\":\"2012-07-19T15:59:13.123Z\","
            "\"parents\":[{\"id\":\"new_parent_resource_id\","
            "\"kind\":\"drive#fileLink\"}],"
            "\"title\":\"new file title\"}",
            http_request_.content);
}

TEST_F(DriveApiRequestsTest, DownloadFileRequest) {
  const base::FilePath kDownloadedFilePath =
      temp_dir_.path().AppendASCII("cache_file");
  const std::string kTestId("dummyId");

  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  base::FilePath temp_file;
  {
    base::RunLoop run_loop;
    drive::DownloadFileRequest* request = new drive::DownloadFileRequest(
        request_sender_.get(),
        *url_generator_,
        kTestId,
        kDownloadedFilePath,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &temp_file)),
        GetContentCallback(),
        ProgressCallback());
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  std::string contents;
  base::ReadFileToString(temp_file, &contents);
  base::DeleteFile(temp_file, false);

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ(kTestDownloadPathPrefix + kTestId, http_request_.relative_url);
  EXPECT_EQ(kDownloadedFilePath, temp_file);

  const std::string expected_contents = kTestId + kTestId + kTestId;
  EXPECT_EQ(expected_contents, contents);
}

TEST_F(DriveApiRequestsTest, DownloadFileRequest_GetContentCallback) {
  const base::FilePath kDownloadedFilePath =
      temp_dir_.path().AppendASCII("cache_file");
  const std::string kTestId("dummyId");

  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  base::FilePath temp_file;
  std::string contents;
  {
    base::RunLoop run_loop;
    drive::DownloadFileRequest* request = new drive::DownloadFileRequest(
        request_sender_.get(),
        *url_generator_,
        kTestId,
        kDownloadedFilePath,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &temp_file)),
        base::Bind(&AppendContent, &contents),
        ProgressCallback());
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  base::DeleteFile(temp_file, false);

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ(kTestDownloadPathPrefix + kTestId, http_request_.relative_url);
  EXPECT_EQ(kDownloadedFilePath, temp_file);

  const std::string expected_contents = kTestId + kTestId + kTestId;
  EXPECT_EQ(expected_contents, contents);
}

TEST_F(DriveApiRequestsTest, PermissionsInsertRequest) {
  expected_content_type_ = "application/json";
  expected_content_ = kTestPermissionResponse;

  GDataErrorCode error = GDATA_OTHER_ERROR;

  // Add comment permission to the user "user@example.com".
  {
    base::RunLoop run_loop;
    drive::PermissionsInsertRequest* request =
        new drive::PermissionsInsertRequest(
            request_sender_.get(),
            *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error)));
    request->set_id("resource_id");
    request->set_role(drive::PERMISSION_ROLE_COMMENTER);
    request->set_type(drive::PERMISSION_TYPE_USER);
    request->set_value("user@example.com");
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files/resource_id/permissions",
            http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  scoped_ptr<base::Value> expected(base::JSONReader::Read(
      "{\"additionalRoles\":[\"commenter\"], \"role\":\"reader\", "
      "\"type\":\"user\",\"value\":\"user@example.com\"}"));
  ASSERT_TRUE(expected);

  scoped_ptr<base::Value> result(base::JSONReader::Read(http_request_.content));
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(base::Value::Equals(expected.get(), result.get()));

  // Add "can edit" permission to users in "example.com".
  error = GDATA_OTHER_ERROR;
  {
    base::RunLoop run_loop;
    drive::PermissionsInsertRequest* request =
        new drive::PermissionsInsertRequest(
            request_sender_.get(),
            *url_generator_,
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&error)));
    request->set_id("resource_id2");
    request->set_role(drive::PERMISSION_ROLE_WRITER);
    request->set_type(drive::PERMISSION_TYPE_DOMAIN);
    request->set_value("example.com");
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_POST, http_request_.method);
  EXPECT_EQ("/drive/v2/files/resource_id2/permissions",
            http_request_.relative_url);
  EXPECT_EQ("application/json", http_request_.headers["Content-Type"]);

  expected.reset(base::JSONReader::Read(
      "{\"role\":\"writer\", \"type\":\"domain\",\"value\":\"example.com\"}"));
  ASSERT_TRUE(expected);

  result.reset(base::JSONReader::Read(http_request_.content));
  EXPECT_TRUE(http_request_.has_content);
  EXPECT_TRUE(base::Value::Equals(expected.get(), result.get()));
}

}  // namespace google_apis
