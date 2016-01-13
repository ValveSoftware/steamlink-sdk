// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "win8/metro_driver/stdafx.h"
#include "win8/metro_driver/toast_notification_handler.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"

#include "win8/metro_driver/winrt_utils.h"

typedef winfoundtn::ITypedEventHandler<
    winui::Notifications::ToastNotification*, IInspectable*>
    ToastActivationHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Notifications::ToastNotification*,
    winui::Notifications::ToastDismissedEventArgs*> ToastDismissedHandler;

namespace {

// Helper function to return the text node root identified by the index passed
// in.
HRESULT GetTextNodeRoot(
    unsigned int index,
    winxml::Dom::IXmlDocument* xml_doc,
    winxml::Dom::IXmlNode** node) {
  DCHECK(xml_doc);
  DCHECK(node);

  mswr::ComPtr<winxml::Dom::IXmlElement> document_element;
  HRESULT hr = xml_doc->get_DocumentElement(&document_element);
  CheckHR(hr);

  mswr::ComPtr<winxml::Dom::IXmlNodeList> elements;
  mswrw::HString tag_name;
  tag_name.Attach(MakeHString(L"text"));
  hr = document_element->GetElementsByTagName(tag_name.Get(),
                                              &elements);
  CheckHR(hr);

  unsigned int count = 0;
  elements->get_Length(&count);

  if (index > count) {
    DVLOG(1) << "Invalid text node index passed in : " << index;
    return E_FAIL;
  }
  hr = elements->Item(index, node);
  CheckHR(hr);
  return hr;
}

// Helper function to append a text element to the text section in the
// XML document passed in.
// The index parameter identifies which text node we append to.
HRESULT CreateTextNode(winxml::Dom::IXmlDocument* xml_doc,
                       int index,
                       const base::string16& text_string) {
  DCHECK(xml_doc);

  mswr::ComPtr<winxml::Dom::IXmlElement> document_element;
  HRESULT hr = xml_doc->get_DocumentElement(&document_element);
  CheckHR(hr);

  mswr::ComPtr<winxml::Dom::IXmlText> xml_text_node;
  mswrw::HString data_hstring;
  data_hstring.Attach(MakeHString(text_string.c_str()));
  hr = xml_doc->CreateTextNode(data_hstring.Get(), &xml_text_node);
  CheckHR(hr);

  mswr::ComPtr<winxml::Dom::IXmlNode> created_node;
  hr = xml_text_node.CopyTo(
      winxml::Dom::IID_IXmlNode,
      reinterpret_cast<void**>(created_node.GetAddressOf()));
  CheckHR(hr);

  mswr::ComPtr<winxml::Dom::IXmlNode> text_node_root;
  hr = GetTextNodeRoot(index, xml_doc, &text_node_root);
  CheckHR(hr);

  mswr::ComPtr<winxml::Dom::IXmlNode> appended_node;
  hr = text_node_root->AppendChild(created_node.Get(), &appended_node);
  CheckHR(hr);
  return hr;
}

}  // namespace

ToastNotificationHandler::DesktopNotification::DesktopNotification(
    const char* notification_origin,
    const char* notification_icon,
    const wchar_t* notification_title,
    const wchar_t* notification_body,
    const wchar_t* notification_display_source,
    const char* notification_id,
    base::win::MetroNotificationClickedHandler handler,
    const wchar_t* handler_context)
    : origin_url(notification_origin),
      icon_url(notification_icon),
      title(notification_title),
      body(notification_body),
      display_source(notification_display_source),
      id(notification_id),
      notification_handler(handler) {
  if (handler_context)
    notification_context = handler_context;
}

ToastNotificationHandler::DesktopNotification::DesktopNotification()
    : notification_handler(NULL) {
}

ToastNotificationHandler::ToastNotificationHandler() {
  DVLOG(1) << __FUNCTION__;
}

ToastNotificationHandler::~ToastNotificationHandler() {
  DVLOG(1) << __FUNCTION__;

  if (notifier_ && notification_)
    CancelNotification();
}

void ToastNotificationHandler::DisplayNotification(
    const DesktopNotification& notification) {
  DVLOG(1) << __FUNCTION__;

  DCHECK(notifier_.Get() == NULL);
  DCHECK(notification_.Get() == NULL);

  notification_info_ = notification;

  mswr::ComPtr<winui::Notifications::IToastNotificationManagerStatics>
      toast_manager;

  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_Notifications_ToastNotificationManager,
      toast_manager.GetAddressOf());
  CheckHR(hr);

  mswr::ComPtr<winxml::Dom::IXmlDocument> toast_xml;
  hr = toast_manager->GetTemplateContent(
      winui::Notifications::ToastTemplateType_ToastText02,
      &toast_xml);
  CheckHR(hr);

  if (!toast_xml)
    return;

  mswr::ComPtr<winxml::Dom::IXmlElement> document_element;
  hr = toast_xml->get_DocumentElement(&document_element);
  CheckHR(hr);

  if (!document_element)
    return;

  hr = CreateTextNode(toast_xml.Get(), 0, notification.title);
  CheckHR(hr);

  hr = CreateTextNode(toast_xml.Get(), 1, notification.body);
  CheckHR(hr);

  mswrw::HString duration_attribute_name;
  duration_attribute_name.Attach(MakeHString(L"duration"));
  mswrw::HString duration_attribute_value;
  duration_attribute_value.Attach(MakeHString(L"long"));

  hr = document_element->SetAttribute(duration_attribute_name.Get(),
                                      duration_attribute_value.Get());
  CheckHR(hr);

  // TODO(ananta)
  // We should set the image and launch params attribute in the notification
  // XNL as described here: http://msdn.microsoft.com/en-us/library/hh465448
  // To set the image we may have to extract the image and specify it in the
  // following url form. ms-appx:///images/foo.png
  // The launch params as described don't get passed back to us via the
  // winapp::Activation::ILaunchActivatedEventArgs argument. Needs to be
  // investigated.
  mswr::ComPtr<winui::Notifications::IToastNotificationFactory>
      toast_notification_factory;
  hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_Notifications_ToastNotification,
      toast_notification_factory.GetAddressOf());
  CheckHR(hr);

  hr = toast_notification_factory->CreateToastNotification(
      toast_xml.Get(), &notification_);
  CheckHR(hr);

  base::FilePath chrome_path;
  if (!PathService::Get(base::FILE_EXE, &chrome_path)) {
    NOTREACHED() << "Failed to get chrome exe path";
    return;
  }

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  bool is_per_user_install = InstallUtil::IsPerUserInstall(
      chrome_path.value().c_str());
  base::string16 appid =
      ShellUtil::GetBrowserModelId(dist, is_per_user_install);
  DVLOG(1) << "Chrome Appid is " << appid.c_str();

  mswrw::HString app_user_model_id;
  app_user_model_id.Attach(MakeHString(appid));

  hr = toast_manager->CreateToastNotifierWithId(app_user_model_id.Get(),
                                                &notifier_);
  CheckHR(hr);

  hr = notification_->add_Activated(
      mswr::Callback<ToastActivationHandler>(
          this, &ToastNotificationHandler::OnActivate).Get(),
      &activated_token_);
  CheckHR(hr);

  hr = notifier_->Show(notification_.Get());
  CheckHR(hr);
}

void ToastNotificationHandler::CancelNotification() {
  DVLOG(1) << __FUNCTION__;

  DCHECK(notifier_);
  DCHECK(notification_);

  notifier_->Hide(notification_.Get());
}

HRESULT ToastNotificationHandler::OnActivate(
    winui::Notifications::IToastNotification* notification,
    IInspectable* inspectable) {
  // TODO(ananta)
  // We should pass back information from the notification like the source url
  // etc to ChromeAppView which would enable it to ensure that the
  // correct tab in chrome is activated.
  DVLOG(1) << __FUNCTION__;

  if (notification_info_.notification_handler) {
    notification_info_.notification_handler(
        notification_info_.notification_context.c_str());
  }
  return S_OK;
}
