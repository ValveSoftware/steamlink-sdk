// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
struct AXEventNotificationDetails;
}  // namespace content

namespace extensions {
class AutomationActionAdapter;

namespace api {
namespace automation_internal {
namespace PerformAction {
struct Params;
}  // namespace PerformAction
}  // namespace automation_internal
}  // namespace api
}  // namespace extensions

namespace ui {
struct AXNodeData;
}

namespace extensions {

// Implementation of the chrome.automation API.
class AutomationInternalEnableTabFunction
    : public ChromeUIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.enableTab",
                             AUTOMATIONINTERNAL_ENABLETAB)
 protected:
  ~AutomationInternalEnableTabFunction() override {}

  ExtensionFunction::ResponseAction Run() override;
};

class AutomationInternalPerformActionFunction
    : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.performAction",
                             AUTOMATIONINTERNAL_PERFORMACTION)
 protected:
  ~AutomationInternalPerformActionFunction() override {}

  ExtensionFunction::ResponseAction Run() override;

 private:
  // Helper function to route an action to an action adapter.
  ExtensionFunction::ResponseAction RouteActionToAdapter(
      api::automation_internal::PerformAction::Params* params,
      AutomationActionAdapter* adapter);
};

class AutomationInternalEnableFrameFunction : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.enableFrame",
                             AUTOMATIONINTERNAL_PERFORMACTION)
 protected:
  ~AutomationInternalEnableFrameFunction() override {}

  ExtensionFunction::ResponseAction Run() override;
};

class AutomationInternalEnableDesktopFunction
    : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.enableDesktop",
                             AUTOMATIONINTERNAL_ENABLEDESKTOP)
 protected:
  ~AutomationInternalEnableDesktopFunction() override {}

  ResponseAction Run() override;
};

class AutomationInternalQuerySelectorFunction
    : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.querySelector",
                             AUTOMATIONINTERNAL_ENABLEDESKTOP)

 public:
  typedef base::Callback<void(const std::string& error,
                              int result_acc_obj_id)> Callback;

 protected:
  ~AutomationInternalQuerySelectorFunction() override {}

  ResponseAction Run() override;

 private:
  void OnResponse(const std::string& error, int result_acc_obj_id);

  // Used for assigning a unique ID to each request so that the response can be
  // routed appropriately.
  static int query_request_id_counter_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_H_
