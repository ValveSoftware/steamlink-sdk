// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/automation_internal/automation_internal_api.h"

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/automation_internal/automation_action_adapter.h"
#include "chrome/browser/extensions/api/automation_internal/automation_event_router.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/automation_api_constants.h"
#include "chrome/common/extensions/api/automation_internal.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "chrome/common/extensions/manifest_handlers/automation.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/accessibility/ax_action_data.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#endif

namespace extensions {
class AutomationWebContentsObserver;
}  // namespace extensions

DEFINE_WEB_CONTENTS_USER_DATA_KEY(extensions::AutomationWebContentsObserver);

namespace extensions {

namespace {

const char kCannotRequestAutomationOnPage[] =
    "Cannot request automation tree on url \"*\". "
    "Extension manifest must request permission to access this host.";
const char kRendererDestroyed[] = "The tab was closed.";
const char kNoMainFrame[] = "No main frame.";
const char kNoDocument[] = "No document.";
const char kNodeDestroyed[] =
    "domQuerySelector sent on node which is no longer in the tree.";

// Handles sending and receiving IPCs for a single querySelector request. On
// creation, sends the request IPC, and is destroyed either when the response is
// received or the renderer is destroyed.
class QuerySelectorHandler : public content::WebContentsObserver {
 public:
  QuerySelectorHandler(
      content::WebContents* web_contents,
      int request_id,
      int acc_obj_id,
      const base::string16& query,
      const extensions::AutomationInternalQuerySelectorFunction::Callback&
          callback)
      : content::WebContentsObserver(web_contents),
        request_id_(request_id),
        callback_(callback) {
    content::RenderViewHost* rvh = web_contents->GetRenderViewHost();

    rvh->Send(new ExtensionMsg_AutomationQuerySelector(
        rvh->GetRoutingID(), request_id, acc_obj_id, query));
  }

  ~QuerySelectorHandler() override {}

  bool OnMessageReceived(const IPC::Message& message) override {
    if (message.type() != ExtensionHostMsg_AutomationQuerySelector_Result::ID)
      return false;

    // There may be several requests in flight; check this response matches.
    int message_request_id = 0;
    base::PickleIterator iter(message);
    if (!iter.ReadInt(&message_request_id))
      return false;

    if (message_request_id != request_id_)
      return false;

    IPC_BEGIN_MESSAGE_MAP(QuerySelectorHandler, message)
      IPC_MESSAGE_HANDLER(ExtensionHostMsg_AutomationQuerySelector_Result,
                          OnQueryResponse)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void WebContentsDestroyed() override {
    callback_.Run(kRendererDestroyed, 0);
    delete this;
  }

 private:
  void OnQueryResponse(int request_id,
                       ExtensionHostMsg_AutomationQuerySelector_Error error,
                       int result_acc_obj_id) {
    std::string error_string;
    switch (error.value) {
    case ExtensionHostMsg_AutomationQuerySelector_Error::kNone:
      error_string = "";
      break;
    case ExtensionHostMsg_AutomationQuerySelector_Error::kNoMainFrame:
      error_string = kNoMainFrame;
      break;
    case ExtensionHostMsg_AutomationQuerySelector_Error::kNoDocument:
      error_string = kNoDocument;
      break;
    case ExtensionHostMsg_AutomationQuerySelector_Error::kNodeDestroyed:
      error_string = kNodeDestroyed;
      break;
    }
    callback_.Run(error_string, result_acc_obj_id);
    delete this;
  }

  int request_id_;
  const extensions::AutomationInternalQuerySelectorFunction::Callback callback_;
};

bool CanRequestAutomation(const Extension* extension,
                          const AutomationInfo* automation_info,
                          const content::WebContents* contents) {
  if (automation_info->desktop)
    return true;

  const GURL& url = contents->GetURL();
  // TODO(aboxhall): check for webstore URL
  if (automation_info->matches.MatchesURL(url))
    return true;

  int tab_id = ExtensionTabUtil::GetTabId(contents);
  std::string unused_error;
  return extension->permissions_data()->CanAccessPage(extension, url, tab_id,
                                                      &unused_error);
}

// Helper class that implements an action adapter for a |RenderFrameHost|.
class RenderFrameHostActionAdapter : public AutomationActionAdapter {
 public:
  explicit RenderFrameHostActionAdapter(content::RenderFrameHost* rfh)
      : rfh_(rfh) {}

  virtual ~RenderFrameHostActionAdapter() {}

  // AutomationActionAdapter implementation.
  void PerformAction(const ui::AXActionData& data) override {
    rfh_->AccessibilityPerformAction(data);
  }

 private:
  content::RenderFrameHost* rfh_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostActionAdapter);
};

}  // namespace

// Helper class that receives accessibility data from |WebContents|.
class AutomationWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AutomationWebContentsObserver> {
 public:
  ~AutomationWebContentsObserver() override {}

  // content::WebContentsObserver overrides.
  void AccessibilityEventReceived(
      const std::vector<content::AXEventNotificationDetails>& details)
      override {
    for (const auto& event : details) {
      ExtensionMsg_AccessibilityEventParams params;
      params.tree_id = event.ax_tree_id;
      params.id = event.id;
      params.event_type = event.event_type;
      params.update = event.update;
      params.location_offset =
          web_contents()->GetContainerBounds().OffsetFromOrigin();
      params.event_from = event.event_from;

      AutomationEventRouter* router = AutomationEventRouter::GetInstance();
      router->DispatchAccessibilityEvent(params);
    }
  }

  void AccessibilityLocationChangesReceived(
      const std::vector<content::AXLocationChangeNotificationDetails>& details)
      override {
    for (const auto& src : details) {
      ExtensionMsg_AccessibilityLocationChangeParams dst;
      dst.id = src.id;
      dst.tree_id = src.ax_tree_id;
      dst.new_location = src.new_location;
      AutomationEventRouter* router = AutomationEventRouter::GetInstance();
      router->DispatchAccessibilityLocationChange(dst);
    }
  }

  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    int tree_id = render_frame_host->GetAXTreeID();
    AutomationEventRouter::GetInstance()->DispatchTreeDestroyedEvent(
        tree_id,
        browser_context_);
  }

 private:
  friend class content::WebContentsUserData<AutomationWebContentsObserver>;

  explicit AutomationWebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        browser_context_(web_contents->GetBrowserContext()) {}

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(AutomationWebContentsObserver);
};

ExtensionFunction::ResponseAction
AutomationInternalEnableTabFunction::Run() {
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  EXTENSION_FUNCTION_VALIDATE(automation_info);

  using api::automation_internal::EnableTab::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  content::WebContents* contents = NULL;
  if (params->args.tab_id.get()) {
    int tab_id = *params->args.tab_id;
    if (!ExtensionTabUtil::GetTabById(tab_id,
                                      GetProfile(),
                                      include_incognito(),
                                      NULL, /* browser out param*/
                                      NULL, /* tab_strip out param */
                                      &contents,
                                      NULL /* tab_index out param */)) {
      return RespondNow(
          Error(tabs_constants::kTabNotFoundError, base::IntToString(tab_id)));
    }
  } else {
    contents = GetCurrentBrowser()->tab_strip_model()->GetActiveWebContents();
    if (!contents)
      return RespondNow(Error("No active tab"));
  }

  content::RenderFrameHost* rfh = contents->GetMainFrame();
  if (!rfh)
    return RespondNow(Error("Could not enable accessibility for active tab"));

  if (!CanRequestAutomation(extension(), automation_info, contents)) {
    return RespondNow(
        Error(kCannotRequestAutomationOnPage, contents->GetURL().spec()));
  }

  AutomationWebContentsObserver::CreateForWebContents(contents);
  contents->EnableTreeOnlyAccessibilityMode();

  int ax_tree_id = rfh->GetAXTreeID();

  // This gets removed when the extension process dies.
  AutomationEventRouter::GetInstance()->RegisterListenerForOneTree(
      extension_id(),
      source_process_id(),
      params->args.routing_id,
      ax_tree_id);

  return RespondNow(ArgumentList(
      api::automation_internal::EnableTab::Results::Create(ax_tree_id)));
}

ExtensionFunction::ResponseAction AutomationInternalEnableFrameFunction::Run() {
  // TODO(dtseng): Limited to desktop tree for now pending out of proc iframes.
  using api::automation_internal::EnableFrame::Params;

  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromAXTreeID(params->tree_id);
  if (!rfh)
    return RespondNow(Error("unable to load tab"));

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(rfh);
  AutomationWebContentsObserver::CreateForWebContents(contents);

  // Only call this if this is the root of a frame tree, to avoid resetting
  // the accessibility state multiple times.
  if (!rfh->GetParent())
    contents->EnableTreeOnlyAccessibilityMode();

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AutomationInternalPerformActionFunction::Run() {
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  EXTENSION_FUNCTION_VALIDATE(automation_info && automation_info->interact);

  using api::automation_internal::PerformAction::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->args.tree_id == api::automation::kDesktopTreeID) {
#if defined(USE_AURA)
    return RouteActionToAdapter(params.get(),
                                AutomationManagerAura::GetInstance());
#else
    NOTREACHED();
    return RespondNow(Error("Unexpected action on desktop automation tree;"
                            " platform does not support desktop automation"));
#endif  // defined(USE_AURA)
  }
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromAXTreeID(params->args.tree_id);
  if (!rfh)
    return RespondNow(Error("Ignoring action on destroyed node"));

  const content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!CanRequestAutomation(extension(), automation_info, contents)) {
    return RespondNow(
        Error(kCannotRequestAutomationOnPage, contents->GetURL().spec()));
  }

  RenderFrameHostActionAdapter adapter(rfh);
  return RouteActionToAdapter(params.get(), &adapter);
}

ExtensionFunction::ResponseAction
AutomationInternalPerformActionFunction::RouteActionToAdapter(
    api::automation_internal::PerformAction::Params* params,
    AutomationActionAdapter* adapter) {
  ui::AXActionData action;
  action.target_node_id = params->args.automation_node_id;
  switch (params->args.action_type) {
    case api::automation_internal::ACTION_TYPE_DODEFAULT:
      action.action = ui::AX_ACTION_DO_DEFAULT;
      adapter->PerformAction(action);
      break;
    case api::automation_internal::ACTION_TYPE_FOCUS:
      action.action = ui::AX_ACTION_SET_FOCUS;
      adapter->PerformAction(action);
      break;
    case api::automation_internal::ACTION_TYPE_MAKEVISIBLE:
      action.action = ui::AX_ACTION_SCROLL_TO_MAKE_VISIBLE;
      adapter->PerformAction(action);
      break;
    case api::automation_internal::ACTION_TYPE_SETSELECTION: {
      api::automation_internal::SetSelectionParams selection_params;
      EXTENSION_FUNCTION_VALIDATE(
          api::automation_internal::SetSelectionParams::Populate(
              params->opt_args.additional_properties, &selection_params));
      action.anchor_node_id = params->args.automation_node_id;
      action.anchor_offset = selection_params.anchor_offset;
      action.focus_node_id = selection_params.focus_node_id;
      action.focus_offset = selection_params.focus_offset;
      action.action = ui::AX_ACTION_SET_SELECTION;
      adapter->PerformAction(action);
      break;
    }
    case api::automation_internal::ACTION_TYPE_SHOWCONTEXTMENU: {
      action.action = ui::AX_ACTION_SHOW_CONTEXT_MENU;
      adapter->PerformAction(action);
      break;
    }
    case api::automation_internal::ACTION_TYPE_SETACCESSIBILITYFOCUS: {
      action.action = ui::AX_ACTION_SET_ACCESSIBILITY_FOCUS;
      adapter->PerformAction(action);
      break;
    }
    case api::automation_internal::
        ACTION_TYPE_SETSEQUENTIALFOCUSNAVIGATIONSTARTINGPOINT: {
      action.action =
          ui::AX_ACTION_SET_SEQUENTIAL_FOCUS_NAVIGATION_STARTING_POINT;
      adapter->PerformAction(action);
      break;
    }
    default:
      NOTREACHED();
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AutomationInternalEnableDesktopFunction::Run() {
#if defined(USE_AURA)
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  if (!automation_info || !automation_info->desktop)
    return RespondNow(Error("desktop permission must be requested"));

  using api::automation_internal::EnableDesktop::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // This gets removed when the extension process dies.
  AutomationEventRouter::GetInstance()->RegisterListenerWithDesktopPermission(
      extension_id(),
      source_process_id(),
      params->routing_id);

  AutomationManagerAura::GetInstance()->Enable(browser_context());
  return RespondNow(NoArguments());
#else
  return RespondNow(Error("getDesktop is unsupported by this platform"));
#endif  // defined(USE_AURA)
}

// static
int AutomationInternalQuerySelectorFunction::query_request_id_counter_ = 0;

ExtensionFunction::ResponseAction
AutomationInternalQuerySelectorFunction::Run() {
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  EXTENSION_FUNCTION_VALIDATE(automation_info);

  using api::automation_internal::QuerySelector::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->args.tree_id == api::automation::kDesktopTreeID) {
    return RespondNow(
        Error("domQuerySelector queries may not be used on the desktop."));
  }
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromAXTreeID(params->args.tree_id);
  if (!rfh)
    return RespondNow(Error("domQuerySelector query sent on destroyed tree."));

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(rfh);

  int request_id = query_request_id_counter_++;
  base::string16 selector = base::UTF8ToUTF16(params->args.selector);

  // QuerySelectorHandler handles IPCs and deletes itself on completion.
  new QuerySelectorHandler(
      contents, request_id, params->args.automation_node_id, selector,
      base::Bind(&AutomationInternalQuerySelectorFunction::OnResponse, this));

  return RespondLater();
}

void AutomationInternalQuerySelectorFunction::OnResponse(
    const std::string& error,
    int result_acc_obj_id) {
  if (!error.empty()) {
    Respond(Error(error));
    return;
  }

  Respond(
      OneArgument(base::MakeUnique<base::FundamentalValue>(result_acc_obj_id)));
}

}  // namespace extensions
