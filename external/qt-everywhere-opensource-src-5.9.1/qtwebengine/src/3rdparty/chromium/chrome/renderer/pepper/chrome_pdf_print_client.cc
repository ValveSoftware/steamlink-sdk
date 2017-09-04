// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/chrome_pdf_print_client.h"

#include "components/printing/renderer/print_web_view_helper.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"

namespace {

blink::WebElement GetWebElement(PP_Instance instance_id) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return blink::WebElement();
  return instance->GetContainer()->element();
}

printing::PrintWebViewHelper* GetPrintWebViewHelper(
    const blink::WebElement& element) {
  if (element.isNull())
    return nullptr;
  auto* render_frame =
      content::RenderFrame::FromWebFrame(element.document().frame());
  return printing::PrintWebViewHelper::Get(render_frame);
}

}  // namespace

ChromePDFPrintClient::ChromePDFPrintClient() {}

ChromePDFPrintClient::~ChromePDFPrintClient() {}

bool ChromePDFPrintClient::IsPrintingEnabled(PP_Instance instance_id) {
  blink::WebElement element = GetWebElement(instance_id);
  printing::PrintWebViewHelper* helper = GetPrintWebViewHelper(element);
  return helper && helper->IsPrintingEnabled();
}

bool ChromePDFPrintClient::Print(PP_Instance instance_id) {
  blink::WebElement element = GetWebElement(instance_id);
  printing::PrintWebViewHelper* helper = GetPrintWebViewHelper(element);
  if (!helper)
    return false;
  helper->PrintNode(element);
  return true;
}
