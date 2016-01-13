// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/interfaces/network/network_service.mojom.h"
#include "mojo/services/public/interfaces/network/url_loader.mojom.h"

namespace mojo {
namespace examples {

class WGetApp : public Application, public URLLoaderClient {
 public:
  virtual void Initialize() MOJO_OVERRIDE {
    ConnectTo("mojo:mojo_network_service", &network_service_);
    Start();
  }

 private:
  virtual void OnReceivedRedirect(URLResponsePtr response,
                                  const String& new_url,
                                  const String& new_method) MOJO_OVERRIDE {
    PrintResponse(response);
  }

  virtual void OnReceivedResponse(URLResponsePtr response) MOJO_OVERRIDE {
    PrintResponse(response);
    PrintResponseBody();
    Start();
  }

  virtual void OnReceivedError(NetworkErrorPtr error) MOJO_OVERRIDE {
    printf("Got error: %d (%s)\n",
        error->code, error->description.get().c_str());
  }

  virtual void OnReceivedEndOfResponseBody() MOJO_OVERRIDE {
    // Ignored.
  }

  void Start() {
    std::string url = PromptForURL();
    printf("Loading: %s\n", url.c_str());

    network_service_->CreateURLLoader(Get(&url_loader_));
    url_loader_.set_client(this);

    URLRequestPtr request(URLRequest::New());
    request->url = url;
    request->method = "GET";
    request->auto_follow_redirects = true;

    DataPipe data_pipe;
    response_body_stream_ = data_pipe.consumer_handle.Pass();

    url_loader_->Start(request.Pass(), data_pipe.producer_handle.Pass());
  }

  std::string PromptForURL() {
    printf("Enter URL> ");
    char buf[1024];
    if (scanf("%1023s", buf) != 1)
      buf[0] = '\0';
    return buf;
  }

  void PrintResponse(const URLResponsePtr& response) {
    printf(">>> Headers <<< \n");
    printf("  %s\n", response->status_line.get().c_str());
    if (response->headers) {
      for (size_t i = 0; i < response->headers.size(); ++i)
        printf("  %s\n", response->headers[i].get().c_str());
    }
  }

  void PrintResponseBody() {
    // Read response body in blocking fashion.
    printf(">>> Body <<<\n");

    for (;;) {
      char buf[512];
      uint32_t num_bytes = sizeof(buf);
      MojoResult result = ReadDataRaw(
          response_body_stream_.get(),
          buf,
          &num_bytes,
          MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        Wait(response_body_stream_.get(),
             MOJO_HANDLE_SIGNAL_READABLE,
             MOJO_DEADLINE_INDEFINITE);
      } else if (result == MOJO_RESULT_OK) {
        fwrite(buf, num_bytes, 1, stdout);
      } else {
        break;
      }
    }

    printf("\n>>> EOF <<<\n");
  }

  NetworkServicePtr network_service_;
  URLLoaderPtr url_loader_;
  ScopedDataPipeConsumerHandle response_body_stream_;
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::WGetApp();
}

}  // namespace mojo
