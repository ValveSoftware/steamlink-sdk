// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_IMPL_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/printing/cloud_print/privet_http.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "ui/gfx/geometry/size.h"

namespace cloud_print {

class PrivetHTTPClient;

class PrivetInfoOperationImpl : public PrivetJSONOperation,
                                public PrivetURLFetcher::Delegate {
 public:
  PrivetInfoOperationImpl(PrivetHTTPClient* privet_client,
                          const PrivetJSONOperation::ResultCallback& callback);
  ~PrivetInfoOperationImpl() override;

  void Start() override;

  PrivetHTTPClient* GetHTTPClient() override;

  void OnError(PrivetURLFetcher* fetcher,
               PrivetURLFetcher::ErrorType error) override;
  void OnParsedJson(PrivetURLFetcher* fetcher,
                    const base::DictionaryValue& value,
                    bool has_error) override;

 private:
  PrivetHTTPClient* privet_client_;
  PrivetJSONOperation::ResultCallback callback_;
  std::unique_ptr<PrivetURLFetcher> url_fetcher_;
};

class PrivetRegisterOperationImpl
    : public PrivetRegisterOperation,
      public PrivetURLFetcher::Delegate,
      public base::SupportsWeakPtr<PrivetRegisterOperationImpl> {
 public:
  PrivetRegisterOperationImpl(PrivetHTTPClient* privet_client,
                              const std::string& user,
                              PrivetRegisterOperation::Delegate* delegate);
  ~PrivetRegisterOperationImpl() override;

  void Start() override;
  void Cancel() override;
  void CompleteRegistration() override;

  void OnError(PrivetURLFetcher* fetcher,
               PrivetURLFetcher::ErrorType error) override;

  void OnParsedJson(PrivetURLFetcher* fetcher,
                    const base::DictionaryValue& value,
                    bool has_error) override;

  void OnNeedPrivetToken(
      PrivetURLFetcher* fetcher,
      const PrivetURLFetcher::TokenCallback& callback) override;

  PrivetHTTPClient* GetHTTPClient() override;

 private:
  class Cancelation : public PrivetURLFetcher::Delegate {
   public:
    Cancelation(PrivetHTTPClient* privet_client, const std::string& user);
    ~Cancelation() override;

    void OnError(PrivetURLFetcher* fetcher,
                 PrivetURLFetcher::ErrorType error) override;

    void OnParsedJson(PrivetURLFetcher* fetcher,
                      const base::DictionaryValue& value,
                      bool has_error) override;

    void Cleanup();

   private:
    std::unique_ptr<PrivetURLFetcher> url_fetcher_;
  };

  // Arguments is JSON value from request.
  typedef base::Callback<void(const base::DictionaryValue&)>
      ResponseHandler;

  void StartInfoOperation();
  void OnPrivetInfoDone(const base::DictionaryValue* value);

  void StartResponse(const base::DictionaryValue& value);
  void GetClaimTokenResponse(const base::DictionaryValue& value);
  void CompleteResponse(const base::DictionaryValue& value);

  void SendRequest(const std::string& action);

  std::string user_;
  std::string current_action_;
  std::unique_ptr<PrivetURLFetcher> url_fetcher_;
  PrivetRegisterOperation::Delegate* delegate_;
  PrivetHTTPClient* privet_client_;
  ResponseHandler next_response_handler_;
  // Required to ensure destroying completed register operations doesn't cause
  // extraneous cancelations.
  bool ongoing_;

  std::unique_ptr<PrivetJSONOperation> info_operation_;
  std::string expected_id_;
};

class PrivetJSONOperationImpl : public PrivetJSONOperation,
                                public PrivetURLFetcher::Delegate {
 public:
  PrivetJSONOperationImpl(PrivetHTTPClient* privet_client,
                          const std::string& path,
                          const std::string& query_params,
                          const PrivetJSONOperation::ResultCallback& callback);
  ~PrivetJSONOperationImpl() override;
  void Start() override;

  PrivetHTTPClient* GetHTTPClient() override;

  void OnError(PrivetURLFetcher* fetcher,
               PrivetURLFetcher::ErrorType error) override;
  void OnParsedJson(PrivetURLFetcher* fetcher,
                    const base::DictionaryValue& value,
                    bool has_error) override;
  void OnNeedPrivetToken(
      PrivetURLFetcher* fetcher,
      const PrivetURLFetcher::TokenCallback& callback) override;

 private:
  PrivetHTTPClient* privet_client_;
  std::string path_;
  std::string query_params_;
  PrivetJSONOperation::ResultCallback callback_;

  std::unique_ptr<PrivetURLFetcher> url_fetcher_;
};

#if defined(ENABLE_PRINT_PREVIEW)
class PrivetLocalPrintOperationImpl
    : public PrivetLocalPrintOperation,
      public PrivetURLFetcher::Delegate {
 public:
  PrivetLocalPrintOperationImpl(PrivetHTTPClient* privet_client,
                                PrivetLocalPrintOperation::Delegate* delegate);

  ~PrivetLocalPrintOperationImpl() override;
  void Start() override;

  void SetData(const scoped_refptr<base::RefCountedBytes>& data) override;

  void SetCapabilities(const std::string& capabilities) override;

  void SetTicket(const std::string& ticket) override;

  void SetUsername(const std::string& user) override;

  void SetJobname(const std::string& jobname) override;

  void SetOffline(bool offline) override;

  void SetPageSize(const gfx::Size& page_size) override;

  void SetPWGRasterConverterForTesting(
      std::unique_ptr<printing::PWGRasterConverter> pwg_raster_converter)
      override;

  PrivetHTTPClient* GetHTTPClient() override;

  void OnError(PrivetURLFetcher* fetcher,
               PrivetURLFetcher::ErrorType error) override;
  void OnParsedJson(PrivetURLFetcher* fetcher,
                    const base::DictionaryValue& value,
                    bool has_error) override;
  void OnNeedPrivetToken(
      PrivetURLFetcher* fetcher,
      const PrivetURLFetcher::TokenCallback& callback) override;

 private:
  typedef base::Callback<void(bool, const base::DictionaryValue* value)>
      ResponseCallback;

  void StartInitialRequest();
  void DoCreatejob();
  void DoSubmitdoc();

  void StartConvertToPWG();
  void StartPrinting();

  void OnPrivetInfoDone(const base::DictionaryValue* value);
  void OnSubmitdocResponse(bool has_error,
                           const base::DictionaryValue* value);
  void OnCreatejobResponse(bool has_error,
                           const base::DictionaryValue* value);
  void OnPWGRasterConverted(bool success, const base::FilePath& pwg_file_path);

  PrivetHTTPClient* privet_client_;
  PrivetLocalPrintOperation::Delegate* delegate_;

  ResponseCallback current_response_;

  cloud_devices::CloudDeviceDescription ticket_;
  cloud_devices::CloudDeviceDescription capabilities_;

  scoped_refptr<base::RefCountedBytes> data_;
  base::FilePath pwg_file_path_;

  bool use_pdf_;
  bool has_extended_workflow_;
  bool started_;
  bool offline_;
  gfx::Size page_size_;

  std::string user_;
  std::string jobname_;

  std::string jobid_;

  int invalid_job_retries_;

  std::unique_ptr<PrivetURLFetcher> url_fetcher_;
  std::unique_ptr<PrivetJSONOperation> info_operation_;
  std::unique_ptr<printing::PWGRasterConverter> pwg_raster_converter_;

  base::WeakPtrFactory<PrivetLocalPrintOperationImpl> weak_factory_;
};
#endif  // ENABLE_PRINT_PREVIEW

class PrivetHTTPClientImpl : public PrivetHTTPClient {
 public:
  PrivetHTTPClientImpl(
      const std::string& name,
      const net::HostPortPair& host_port,
      const scoped_refptr<net::URLRequestContextGetter>& context_getter);
  ~PrivetHTTPClientImpl() override;

  // PrivetHTTPClient implementation.
  const std::string& GetName() override;
  std::unique_ptr<PrivetJSONOperation> CreateInfoOperation(
      const PrivetJSONOperation::ResultCallback& callback) override;
  std::unique_ptr<PrivetURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      PrivetURLFetcher::Delegate* delegate) override;
  void RefreshPrivetToken(
      const PrivetURLFetcher::TokenCallback& token_callback) override;

 private:
  typedef std::vector<PrivetURLFetcher::TokenCallback> TokenCallbackVector;

  void OnPrivetInfoDone(const base::DictionaryValue* value);

  std::string name_;
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  net::HostPortPair host_port_;

  std::unique_ptr<PrivetJSONOperation> info_operation_;
  TokenCallbackVector token_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PrivetHTTPClientImpl);
};

class PrivetV1HTTPClientImpl : public PrivetV1HTTPClient {
 public:
  explicit PrivetV1HTTPClientImpl(
      std::unique_ptr<PrivetHTTPClient> info_client);
  ~PrivetV1HTTPClientImpl() override;

  const std::string& GetName() override;
  std::unique_ptr<PrivetJSONOperation> CreateInfoOperation(
      const PrivetJSONOperation::ResultCallback& callback) override;
  std::unique_ptr<PrivetRegisterOperation> CreateRegisterOperation(
      const std::string& user,
      PrivetRegisterOperation::Delegate* delegate) override;
  std::unique_ptr<PrivetJSONOperation> CreateCapabilitiesOperation(
      const PrivetJSONOperation::ResultCallback& callback) override;
  std::unique_ptr<PrivetLocalPrintOperation> CreateLocalPrintOperation(
      PrivetLocalPrintOperation::Delegate* delegate) override;

 private:
  PrivetHTTPClient* info_client() { return info_client_.get(); }

  std::unique_ptr<PrivetHTTPClient> info_client_;

  DISALLOW_COPY_AND_ASSIGN(PrivetV1HTTPClientImpl);
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_HTTP_IMPL_H_
