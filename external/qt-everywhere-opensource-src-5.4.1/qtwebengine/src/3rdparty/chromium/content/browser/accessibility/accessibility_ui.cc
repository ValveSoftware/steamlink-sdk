// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/accessibility/accessibility_tree_formatter.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/view_message_enums.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "grit/content_resources.h"
#include "net/base/escape.h"

static const char kDataFile[] = "targets-data.json";

static const char kProcessIdField[]  = "processId";
static const char kRouteIdField[]  = "routeId";
static const char kUrlField[]  = "url";
static const char kNameField[]  = "name";
static const char kFaviconUrlField[] = "favicon_url";
static const char kPidField[]  = "pid";
static const char kAccessibilityModeField[] = "a11y_mode";

namespace content {

namespace {

base::DictionaryValue* BuildTargetDescriptor(
    const GURL& url,
    const std::string& name,
    const GURL& favicon_url,
    int process_id,
    int route_id,
    AccessibilityMode accessibility_mode,
    base::ProcessHandle handle = base::kNullProcessHandle) {
  base::DictionaryValue* target_data = new base::DictionaryValue();
  target_data->SetInteger(kProcessIdField, process_id);
  target_data->SetInteger(kRouteIdField, route_id);
  target_data->SetString(kUrlField, url.spec());
  target_data->SetString(kNameField, net::EscapeForHTML(name));
  target_data->SetInteger(kPidField, base::GetProcId(handle));
  target_data->SetString(kFaviconUrlField, favicon_url.spec());
  target_data->SetInteger(kAccessibilityModeField,
                          accessibility_mode);
  return target_data;
}

base::DictionaryValue* BuildTargetDescriptor(RenderViewHost* rvh) {
  WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
  std::string title;
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(rvh);
  AccessibilityMode accessibility_mode = rwhi->accessibility_mode();

  GURL url;
  GURL favicon_url;
  if (web_contents) {
    // TODO(nasko): Fix the following code to use a consistent set of data
    // across the URL, title, and favicon.
    url = web_contents->GetURL();
    title = base::UTF16ToUTF8(web_contents->GetTitle());
    NavigationController& controller = web_contents->GetController();
    NavigationEntry* entry = controller.GetVisibleEntry();
    if (entry != NULL && entry->GetURL().is_valid())
      favicon_url = entry->GetFavicon().url;
  }

  return BuildTargetDescriptor(url,
                               title,
                               favicon_url,
                               rvh->GetProcess()->GetID(),
                               rvh->GetRoutingID(),
                               accessibility_mode);
}

bool HandleRequestCallback(BrowserContext* current_context,
                           const std::string& path,
                           const WebUIDataSource::GotDataCallback& callback) {
  if (path != kDataFile)
    return false;
  scoped_ptr<base::ListValue> rvh_list(new base::ListValue());

  scoped_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());

  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    // Ignore processes that don't have a connection, such as crashed tabs.
    if (!widget->GetProcess()->HasConnection())
      continue;
    if (!widget->IsRenderView())
        continue;
    RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(widget);
    BrowserContext* context = rwhi->GetProcess()->GetBrowserContext();
    if (context != current_context)
      continue;

    RenderViewHost* rvh = RenderViewHost::From(widget);
    rvh_list->Append(BuildTargetDescriptor(rvh));
  }

  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->Set("list", rvh_list.release());
  scoped_ptr<base::FundamentalValue> a11y_mode(base::Value::CreateIntegerValue(
      BrowserAccessibilityStateImpl::GetInstance()->accessibility_mode()));
  data->Set("global_a11y_mode", a11y_mode.release());

  std::string json_string;
  base::JSONWriter::Write(data.get(), &json_string);

  callback.Run(base::RefCountedString::TakeString(&json_string));
  return true;
}

}  // namespace

AccessibilityUI::AccessibilityUI(WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://accessibility source.
  WebUIDataSource* html_source =
      WebUIDataSource::Create(kChromeUIAccessibilityHost);
  html_source->SetUseJsonJSFormatV2();

  web_ui->RegisterMessageCallback(
      "toggleAccessibility",
      base::Bind(&AccessibilityUI::ToggleAccessibility,
                 base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "toggleGlobalAccessibility",
      base::Bind(&AccessibilityUI::ToggleGlobalAccessibility,
                 base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "requestAccessibilityTree",
      base::Bind(&AccessibilityUI::RequestAccessibilityTree,
                 base::Unretained(this)));

  // Add required resources.
  html_source->SetJsonPath("strings.js");
  html_source->AddResourcePath("accessibility.css", IDR_ACCESSIBILITY_CSS);
  html_source->AddResourcePath("accessibility.js", IDR_ACCESSIBILITY_JS);
  html_source->SetDefaultResource(IDR_ACCESSIBILITY_HTML);
  html_source->SetRequestFilter(
      base::Bind(&HandleRequestCallback,
                 web_ui->GetWebContents()->GetBrowserContext()));

  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, html_source);
}

AccessibilityUI::~AccessibilityUI() {}

void AccessibilityUI::ToggleAccessibility(const base::ListValue* args) {
  std::string process_id_str;
  std::string route_id_str;
  int process_id;
  int route_id;
  CHECK_EQ(2U, args->GetSize());
  CHECK(args->GetString(0, &process_id_str));
  CHECK(args->GetString(1, &route_id_str));
  CHECK(base::StringToInt(process_id_str, &process_id));
  CHECK(base::StringToInt(route_id_str, &route_id));

  RenderViewHost* rvh = RenderViewHost::FromID(process_id, route_id);
  if (!rvh)
    return;
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(rvh);
  if (!rwhi)
    return;
  AccessibilityMode mode = rwhi->accessibility_mode();
  if ((mode & AccessibilityModeComplete) != AccessibilityModeComplete)
    rwhi->AddAccessibilityMode(AccessibilityModeComplete);
  else
    rwhi->ResetAccessibilityMode();
}

void AccessibilityUI::ToggleGlobalAccessibility(const base::ListValue* args) {
  BrowserAccessibilityStateImpl* state =
      BrowserAccessibilityStateImpl::GetInstance();
  AccessibilityMode mode = state->accessibility_mode();
  if ((mode & AccessibilityModeComplete) != AccessibilityModeComplete)
    state->EnableAccessibility();
  else
    state->DisableAccessibility();
}

void AccessibilityUI::RequestAccessibilityTree(const base::ListValue* args) {
  std::string process_id_str;
  std::string route_id_str;
  int process_id;
  int route_id;
  CHECK_EQ(2U, args->GetSize());
  CHECK(args->GetString(0, &process_id_str));
  CHECK(args->GetString(1, &route_id_str));
  CHECK(base::StringToInt(process_id_str, &process_id));
  CHECK(base::StringToInt(route_id_str, &route_id));

  RenderViewHost* rvh = RenderViewHost::FromID(process_id, route_id);
  if (!rvh) {
    scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
    result->SetInteger(kProcessIdField, process_id);
    result->SetInteger(kRouteIdField, route_id);
    result->Set("error", new base::StringValue("Renderer no longer exists."));
    web_ui()->CallJavascriptFunction("accessibility.showTree", *(result.get()));
    return;
  }

  scoped_ptr<base::DictionaryValue> result(BuildTargetDescriptor(rvh));
  RenderWidgetHostViewBase* host_view = static_cast<RenderWidgetHostViewBase*>(
      WebContents::FromRenderViewHost(rvh)->GetRenderWidgetHostView());
  if (!host_view) {
    result->Set("error",
                new base::StringValue("Could not get accessibility tree."));
    web_ui()->CallJavascriptFunction("accessibility.showTree", *(result.get()));
    return;
  }
  scoped_ptr<AccessibilityTreeFormatter> formatter(
      AccessibilityTreeFormatter::Create(rvh));
  base::string16 accessibility_contents_utf16;
  BrowserAccessibilityManager* manager =
      host_view->GetBrowserAccessibilityManager();
  if (!manager) {
    result->Set("error",
                new base::StringValue("Could not get accessibility tree."));
    web_ui()->CallJavascriptFunction("accessibility.showTree", *(result.get()));
    return;
  }
  std::vector<AccessibilityTreeFormatter::Filter> filters;
  filters.push_back(AccessibilityTreeFormatter::Filter(
      base::ASCIIToUTF16("*"),
      AccessibilityTreeFormatter::Filter::ALLOW));
  formatter->SetFilters(filters);
  formatter->FormatAccessibilityTree(&accessibility_contents_utf16);

  result->Set("tree",
              new base::StringValue(
                  base::UTF16ToUTF8(accessibility_contents_utf16)));
  web_ui()->CallJavascriptFunction("accessibility.showTree", *(result.get()));
}

}  // namespace content
