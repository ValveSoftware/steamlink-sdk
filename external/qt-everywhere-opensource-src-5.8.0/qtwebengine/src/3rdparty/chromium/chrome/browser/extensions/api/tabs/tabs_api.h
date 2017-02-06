// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/common/extensions/api/tabs.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/api/execute_code_function.h"
#include "extensions/browser/api/web_contents_capture_client.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/user_script.h"
#include "url/gurl.h"

class GURL;
class SkBitmap;
class TabStripModel;

namespace base {
class DictionaryValue;
}

namespace content {
class WebContents;
}

namespace ui {
class ListSelectionModel;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {

// Converts a ZoomMode to its ZoomSettings representation.
void ZoomModeToZoomSettings(zoom::ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings);

// Windows
class WindowsGetFunction : public ChromeSyncExtensionFunction {
  ~WindowsGetFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("windows.get", WINDOWS_GET)
};
class WindowsGetCurrentFunction : public ChromeSyncExtensionFunction {
  ~WindowsGetCurrentFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("windows.getCurrent", WINDOWS_GETCURRENT)
};
class WindowsGetLastFocusedFunction : public ChromeSyncExtensionFunction {
  ~WindowsGetLastFocusedFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("windows.getLastFocused", WINDOWS_GETLASTFOCUSED)
};
class WindowsGetAllFunction : public ChromeSyncExtensionFunction {
  ~WindowsGetAllFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("windows.getAll", WINDOWS_GETALL)
};
class WindowsCreateFunction : public ChromeSyncExtensionFunction {
  ~WindowsCreateFunction() override {}
  bool RunSync() override;
  // Returns whether the window should be created in incognito mode.
  // |create_data| are the options passed by the extension. It may be NULL.
  // |urls| is the list of urls to open. If we are creating an incognito window,
  // the function will remove these urls which may not be opened in incognito
  // mode.  If window creation leads the browser into an erroneous state,
  // |is_error| is set to true (also, error_ member variable is assigned
  // the proper error message).
  bool ShouldOpenIncognitoWindow(
      const api::windows::Create::Params::CreateData* create_data,
      std::vector<GURL>* urls,
      bool* is_error);
  DECLARE_EXTENSION_FUNCTION("windows.create", WINDOWS_CREATE)
};
class WindowsUpdateFunction : public ChromeSyncExtensionFunction {
  ~WindowsUpdateFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("windows.update", WINDOWS_UPDATE)
};
class WindowsRemoveFunction : public ChromeSyncExtensionFunction {
  ~WindowsRemoveFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("windows.remove", WINDOWS_REMOVE)
};

// Tabs
class TabsGetFunction : public ChromeSyncExtensionFunction {
  ~TabsGetFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("tabs.get", TABS_GET)
};
class TabsGetCurrentFunction : public ChromeSyncExtensionFunction {
  ~TabsGetCurrentFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getCurrent", TABS_GETCURRENT)
};
class TabsGetSelectedFunction : public ChromeSyncExtensionFunction {
  ~TabsGetSelectedFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getSelected", TABS_GETSELECTED)
};
class TabsGetAllInWindowFunction : public ChromeSyncExtensionFunction {
  ~TabsGetAllInWindowFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getAllInWindow", TABS_GETALLINWINDOW)
};
class TabsQueryFunction : public ChromeSyncExtensionFunction {
  ~TabsQueryFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("tabs.query", TABS_QUERY)
};
class TabsCreateFunction : public ChromeSyncExtensionFunction {
  ~TabsCreateFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("tabs.create", TABS_CREATE)
};
class TabsDuplicateFunction : public ChromeSyncExtensionFunction {
  ~TabsDuplicateFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("tabs.duplicate", TABS_DUPLICATE)
};
class TabsHighlightFunction : public ChromeSyncExtensionFunction {
  ~TabsHighlightFunction() override {}
  bool RunSync() override;
  bool HighlightTab(TabStripModel* tabstrip,
                    ui::ListSelectionModel* selection,
                    int *active_index,
                    int index);
  DECLARE_EXTENSION_FUNCTION("tabs.highlight", TABS_HIGHLIGHT)
};
class TabsUpdateFunction : public ChromeAsyncExtensionFunction {
 public:
  TabsUpdateFunction();

 protected:
  ~TabsUpdateFunction() override {}
  virtual bool UpdateURL(const std::string& url,
                         int tab_id,
                         bool* is_async);
  virtual void PopulateResult();

  content::WebContents* web_contents_;

 private:
  bool RunAsync() override;
  void OnExecuteCodeFinished(const std::string& error,
                             const GURL& on_url,
                             const base::ListValue& script_result);

  DECLARE_EXTENSION_FUNCTION("tabs.update", TABS_UPDATE)
};
class TabsMoveFunction : public ChromeSyncExtensionFunction {
  ~TabsMoveFunction() override {}
  bool RunSync() override;
  bool MoveTab(int tab_id,
               int* new_index,
               int iteration,
               base::ListValue* tab_values,
               int* window_id);
  DECLARE_EXTENSION_FUNCTION("tabs.move", TABS_MOVE)
};
class TabsReloadFunction : public ChromeSyncExtensionFunction {
  ~TabsReloadFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("tabs.reload", TABS_RELOAD)
};
class TabsRemoveFunction : public ChromeSyncExtensionFunction {
  ~TabsRemoveFunction() override {}
  bool RunSync() override;
  bool RemoveTab(int tab_id);
  DECLARE_EXTENSION_FUNCTION("tabs.remove", TABS_REMOVE)
};
class TabsDetectLanguageFunction : public ChromeAsyncExtensionFunction,
                                   public content::NotificationObserver {
 private:
  ~TabsDetectLanguageFunction() override {}
  bool RunAsync() override;

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;
  void GotLanguage(const std::string& language);
  content::NotificationRegistrar registrar_;
  DECLARE_EXTENSION_FUNCTION("tabs.detectLanguage", TABS_DETECTLANGUAGE)
};

class TabsCaptureVisibleTabFunction
    : public extensions::WebContentsCaptureClient,
      public AsyncExtensionFunction {
 public:
  TabsCaptureVisibleTabFunction();
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // ExtensionFunction implementation.
  bool HasPermission() override;
  bool RunAsync() override;

 protected:
  ~TabsCaptureVisibleTabFunction() override {}

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  content::WebContents* GetWebContentsForID(int window_id);

  // extensions::WebContentsCaptureClient:
  bool IsScreenshotEnabled() override;
  bool ClientAllowsTransparency() override;
  void OnCaptureSuccess(const SkBitmap& bitmap) override;
  void OnCaptureFailure(FailureReason reason) override;

  DECLARE_EXTENSION_FUNCTION("tabs.captureVisibleTab", TABS_CAPTUREVISIBLETAB)
};

// Implement API call tabs.executeScript and tabs.insertCSS.
class ExecuteCodeInTabFunction : public ExecuteCodeFunction {
 public:
  ExecuteCodeInTabFunction();

 protected:
  ~ExecuteCodeInTabFunction() override;

  // ExtensionFunction:
  bool HasPermission() override;

  // Initialize the |execute_tab_id_| and |details_| if they haven't already
  // been. Returns whether initialization was successful.
  bool Init() override;
  bool CanExecuteScriptOnPage() override;
  ScriptExecutor* GetScriptExecutor() override;
  bool IsWebView() const override;
  const GURL& GetWebViewSrc() const override;

 private:
  const ChromeExtensionFunctionDetails chrome_details_;

  // Id of tab which executes code.
  int execute_tab_id_;
};

class TabsExecuteScriptFunction : public ExecuteCodeInTabFunction {
 protected:
  bool ShouldInsertCSS() const override;

 private:
  ~TabsExecuteScriptFunction() override {}

  void OnExecuteCodeFinished(const std::string& error,
                             const GURL& on_url,
                             const base::ListValue& script_result) override;

  DECLARE_EXTENSION_FUNCTION("tabs.executeScript", TABS_EXECUTESCRIPT)
};

class TabsInsertCSSFunction : public ExecuteCodeInTabFunction {
 private:
  ~TabsInsertCSSFunction() override {}

  bool ShouldInsertCSS() const override;

  DECLARE_EXTENSION_FUNCTION("tabs.insertCSS", TABS_INSERTCSS)
};

class ZoomAPIFunction : public ChromeAsyncExtensionFunction {
 protected:
  ~ZoomAPIFunction() override {}

  // Gets the WebContents for |tab_id| if it is specified. Otherwise get the
  // WebContents for the active tab in the current window. Calling this function
  // may set error_.
  //
  // TODO(...) many other tabs API functions use similar behavior. There should
  // be a way to share this implementation somehow.
  content::WebContents* GetWebContents(int tab_id);
};

class TabsSetZoomFunction : public ZoomAPIFunction {
 private:
  ~TabsSetZoomFunction() override {}

  bool RunAsync() override;

  DECLARE_EXTENSION_FUNCTION("tabs.setZoom", TABS_SETZOOM)
};

class TabsGetZoomFunction : public ZoomAPIFunction {
 private:
  ~TabsGetZoomFunction() override {}

  bool RunAsync() override;

  DECLARE_EXTENSION_FUNCTION("tabs.getZoom", TABS_GETZOOM)
};

class TabsSetZoomSettingsFunction : public ZoomAPIFunction {
 private:
  ~TabsSetZoomSettingsFunction() override {}

  bool RunAsync() override;

  DECLARE_EXTENSION_FUNCTION("tabs.setZoomSettings", TABS_SETZOOMSETTINGS)
};

class TabsGetZoomSettingsFunction : public ZoomAPIFunction {
 private:
  ~TabsGetZoomSettingsFunction() override {}

  bool RunAsync() override;

  DECLARE_EXTENSION_FUNCTION("tabs.getZoomSettings", TABS_GETZOOMSETTINGS)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
