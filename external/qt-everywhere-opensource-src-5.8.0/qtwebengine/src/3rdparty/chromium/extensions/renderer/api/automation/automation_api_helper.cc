// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/automation/automation_api_helper.h"

#include "content/public/renderer/render_view.h"
#include "extensions/common/extension_messages.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebExceptionCode.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace extensions {

AutomationApiHelper::AutomationApiHelper(content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {
}

AutomationApiHelper::~AutomationApiHelper() {
}

bool AutomationApiHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AutomationApiHelper, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AutomationQuerySelector, OnQuerySelector)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AutomationApiHelper::OnDestruct() {
  delete this;
}

void AutomationApiHelper::OnQuerySelector(int request_id,
                                          int acc_obj_id,
                                          const base::string16& selector) {
  ExtensionHostMsg_AutomationQuerySelector_Error error;
  if (!render_view() || !render_view()->GetWebView() ||
      !render_view()->GetWebView()->mainFrame()) {
    error.value = ExtensionHostMsg_AutomationQuerySelector_Error::kNoMainFrame;
    Send(new ExtensionHostMsg_AutomationQuerySelector_Result(
        routing_id(), request_id, error, 0));
    return;
  }
  blink::WebDocument document =
      render_view()->GetWebView()->mainFrame()->document();
  if (document.isNull()) {
      error.value =
          ExtensionHostMsg_AutomationQuerySelector_Error::kNoDocument;
    Send(new ExtensionHostMsg_AutomationQuerySelector_Result(
        routing_id(), request_id, error, 0));
    return;
  }
  blink::WebNode start_node = document;
  if (acc_obj_id > 0) {
    blink::WebAXObject start_acc_obj =
        document.accessibilityObjectFromID(acc_obj_id);
    if (start_acc_obj.isNull()) {
      error.value =
          ExtensionHostMsg_AutomationQuerySelector_Error::kNodeDestroyed;
      Send(new ExtensionHostMsg_AutomationQuerySelector_Result(
          routing_id(), request_id, error, 0));
      return;
    }

    start_node = start_acc_obj.node();
    while (start_node.isNull()) {
      start_acc_obj = start_acc_obj.parentObject();
      start_node = start_acc_obj.node();
    }
  }
  blink::WebString web_selector(selector);
  blink::WebElement result_element = start_node.querySelector(web_selector);
  int result_acc_obj_id = 0;
  if (!result_element.isNull()) {
    blink::WebAXObject result_acc_obj = result_element.accessibilityObject();
    if (!result_acc_obj.isDetached()) {
      while (result_acc_obj.accessibilityIsIgnored())
        result_acc_obj = result_acc_obj.parentObject();

      result_acc_obj_id = result_acc_obj.axID();
    }
  }
  Send(new ExtensionHostMsg_AutomationQuerySelector_Result(
      routing_id(), request_id, error, result_acc_obj_id));
}

}  // namespace extensions
