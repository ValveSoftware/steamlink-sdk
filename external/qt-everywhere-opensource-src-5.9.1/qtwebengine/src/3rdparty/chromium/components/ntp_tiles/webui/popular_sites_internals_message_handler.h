// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_WEBUI_POPULAR_SITES_HANDLER_H_
#define COMPONENTS_NTP_TILES_WEBUI_POPULAR_SITES_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace base {
class ListValue;
}  // namespace base

namespace ntp_tiles {

class PopularSites;
class PopularSitesInternalsMessageHandlerClient;

// Implements the WebUI message handler for chrome://popular-sites-internals/
//
// Because content and iOS use different implementations of WebUI, this class
// implements the generic portion and depends on the embedder to inject a bridge
// to the embedder's API. It cannot itself implement either API directly.
class PopularSitesInternalsMessageHandler {
 public:
  explicit PopularSitesInternalsMessageHandler(
      PopularSitesInternalsMessageHandlerClient* web_ui);
  ~PopularSitesInternalsMessageHandler();

  // Called when the WebUI page's JavaScript has loaded and it is ready to
  // receive RegisterMessageCallback() calls.
  void RegisterMessages();

 private:
  // Callbacks registered in RegisterMessages().
  void HandleRegisterForEvents(const base::ListValue* args);
  void HandleUpdate(const base::ListValue* args);
  void HandleViewJson(const base::ListValue* args);

  void SendOverrides();
  void SendDownloadResult(bool success);
  void SendSites();
  void SendJson(const std::string& json);

  // Completion handler for popular_sites_->StartFetch().
  void OnPopularSitesAvailable(bool explicit_request, bool success);

  // Bridge to embedder's API.
  PopularSitesInternalsMessageHandlerClient* web_ui_;

  std::unique_ptr<PopularSites> popular_sites_;

  base::WeakPtrFactory<PopularSitesInternalsMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PopularSitesInternalsMessageHandler);
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_WEBUI_POPULAR_SITES_HANDLER_H_
