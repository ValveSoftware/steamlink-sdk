// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_tokenizer.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/interfaces/launcher/launcher.mojom.h"
#include "mojo/services/public/interfaces/network/network_service.mojom.h"
#include "mojo/services/public/interfaces/network/url_loader.mojom.h"
#include "url/gurl.h"

namespace mojo {
namespace launcher {

class LauncherApp;

class LauncherConnection : public InterfaceImpl<Launcher> {
 public:
  explicit LauncherConnection(LauncherApp* app) : app_(app) {}
  virtual ~LauncherConnection() {}

 private:
  // Overridden from Launcher:
  virtual void Launch(const String& url) OVERRIDE;

  LauncherApp* app_;

  DISALLOW_COPY_AND_ASSIGN(LauncherConnection);
};

class LaunchInstance : public URLLoaderClient {
 public:
  LaunchInstance(LauncherApp* app,
                 LauncherClient* client,
                 const String& url);
  virtual ~LaunchInstance() {}

 private:
  // Overridden from URLLoaderClient:
  virtual void OnReceivedRedirect(URLResponsePtr response,
                                  const String& new_url,
                                  const String& new_method) OVERRIDE {
  }
  virtual void OnReceivedResponse(URLResponsePtr response) OVERRIDE;
  virtual void OnReceivedError(NetworkErrorPtr error) OVERRIDE {
    ScheduleDestroy();
  }
  virtual void OnReceivedEndOfResponseBody() OVERRIDE {
    ScheduleDestroy();
  }

  std::string GetContentType(const Array<String>& headers) {
    for (size_t i = 0; i < headers.size(); ++i) {
      base::StringTokenizer t(headers[i], ": ;=");
      while (t.GetNext()) {
        if (!t.token_is_delim() && t.token() == "Content-Type") {
          while (t.GetNext()) {
            if (!t.token_is_delim())
              return t.token();
          }
        }
      }
    }
    return "";
  }

  void ScheduleDestroy() {
    if (destroy_scheduled_)
      return;
    destroy_scheduled_ = true;
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  LauncherApp* app_;
  bool destroy_scheduled_;
  LauncherClient* client_;
  URLLoaderPtr url_loader_;
  ScopedDataPipeConsumerHandle response_body_stream_;

  DISALLOW_COPY_AND_ASSIGN(LaunchInstance);
};

class LauncherApp : public Application {
 public:
  LauncherApp() {
    handler_map_["text/html"] = "mojo:mojo_html_viewer";
    handler_map_["image/png"] = "mojo:mojo_image_viewer";
  }
  virtual ~LauncherApp() {}

  URLLoaderPtr CreateURLLoader() {
    URLLoaderPtr loader;
    network_service_->CreateURLLoader(Get(&loader));
    return loader.Pass();
  }

  std::string GetHandlerForContentType(const std::string& content_type) {
    HandlerMap::const_iterator it = handler_map_.find(content_type);
    return it != handler_map_.end() ? it->second : "";
  }

 private:
  typedef std::map<std::string, std::string> HandlerMap;

  // Overridden from Application:
  virtual void Initialize() OVERRIDE {
    AddService<LauncherConnection>(this);
    ConnectTo("mojo:mojo_network_service", &network_service_);
  }

  HandlerMap handler_map_;

  NetworkServicePtr network_service_;

  DISALLOW_COPY_AND_ASSIGN(LauncherApp);
};

void LauncherConnection::Launch(const String& url_string) {
  GURL url(url_string.To<std::string>());

  // For Mojo URLs, the handler can always be found at the origin.
  // TODO(aa): Return error for invalid URL?
  if (url.is_valid() && url.SchemeIs("mojo")) {
    client()->OnLaunch(url_string,
                       url.GetOrigin().spec(),
                       navigation::ResponseDetailsPtr());
    return;
  }

  new LaunchInstance(app_, client(), url_string);
}

LaunchInstance::LaunchInstance(LauncherApp* app,
                               LauncherClient* client,
                               const String& url)
    : app_(app),
      destroy_scheduled_(false),
      client_(client) {
  url_loader_ = app_->CreateURLLoader();
  url_loader_.set_client(this);

  URLRequestPtr request(URLRequest::New());
  request->url = url;
  request->method = "GET";
  request->auto_follow_redirects = true;

  DataPipe data_pipe;
  response_body_stream_ = data_pipe.consumer_handle.Pass();

  url_loader_->Start(request.Pass(), data_pipe.producer_handle.Pass());
}

void LaunchInstance::OnReceivedResponse(URLResponsePtr response) {
  std::string content_type = GetContentType(response->headers);
  std::string handler_url = app_->GetHandlerForContentType(content_type);
  if (!handler_url.empty()) {
    navigation::ResponseDetailsPtr nav_response(
        navigation::ResponseDetails::New());
    nav_response->response = response.Pass();
    nav_response->response_body_stream = response_body_stream_.Pass();
    client_->OnLaunch(nav_response->response->url, handler_url,
                      nav_response.Pass());
  }
}

}  // namespace launcher

// static
Application* Application::Create() {
  return new launcher::LauncherApp;
}

}  // namespace mojo
