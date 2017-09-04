// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_WEBUI_POPULAR_SITES_HANDLER_CLIENT_H_
#define COMPONENTS_NTP_TILES_WEBUI_POPULAR_SITES_HANDLER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"

class PrefService;

namespace base {
class Value;
class ListValue;
class SequencedWorkerPool;
}  // namespace base

namespace ntp_tiles {

class PopularSites;

// Implemented by embedders to hook up PopularSitesInternalsMessageHandler.
class PopularSitesInternalsMessageHandlerClient {
 public:
  // Returns the blocking pool for hte embedder.
  virtual base::SequencedWorkerPool* GetBlockingPool() = 0;

  // Returns the PrefService for the embedder and containing WebUI page.
  virtual PrefService* GetPrefs() = 0;

  // Creates a new PopularSites based on the context pf the WebUI page.
  virtual std::unique_ptr<ntp_tiles::PopularSites> MakePopularSites() = 0;

  // Registers a callback in Javascript. See content::WebUI and web::WebUIIOS.
  virtual void RegisterMessageCallback(
      const std::string& message,
      const base::Callback<void(const base::ListValue*)>& callback) = 0;

  // Invokes a function in Javascript. See content::WebUI and web::WebUIIOS.
  virtual void CallJavascriptFunctionVector(
      const std::string& name,
      const std::vector<const base::Value*>& values) = 0;

  // Helper function for CallJavascriptFunctionVector().
  template <typename... Arg>
  void CallJavascriptFunction(const std::string& name, const Arg&... arg) {
    CallJavascriptFunctionVector(name, {&arg...});
  }

 protected:
  PopularSitesInternalsMessageHandlerClient();
  virtual ~PopularSitesInternalsMessageHandlerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(PopularSitesInternalsMessageHandlerClient);
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_WEBUI_POPULAR_SITES_HANDLER_CLIENT_H_
