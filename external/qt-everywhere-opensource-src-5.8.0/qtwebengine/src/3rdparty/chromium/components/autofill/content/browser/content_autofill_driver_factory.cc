// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver_factory.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"

namespace autofill {

const char ContentAutofillDriverFactory::
    kContentAutofillDriverFactoryWebContentsUserDataKey[] =
        "web_contents_autofill_driver_factory";

ContentAutofillDriverFactory::~ContentAutofillDriverFactory() {}

// static
void ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
    content::WebContents* contents,
    AutofillClient* client,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager) {
  if (FromWebContents(contents))
    return;

  auto new_factory = base::WrapUnique(new ContentAutofillDriverFactory(
      contents, client, app_locale, enable_download_manager));
  const std::vector<content::RenderFrameHost*> frames =
      contents->GetAllFrames();
  for (content::RenderFrameHost* frame : frames) {
    if (frame->IsRenderFrameLive())
      new_factory->RenderFrameCreated(frame);
  }

  contents->SetUserData(kContentAutofillDriverFactoryWebContentsUserDataKey,
                        new_factory.release());
}

// static
ContentAutofillDriverFactory* ContentAutofillDriverFactory::FromWebContents(
    content::WebContents* contents) {
  return static_cast<ContentAutofillDriverFactory*>(contents->GetUserData(
      kContentAutofillDriverFactoryWebContentsUserDataKey));
}

// static
void ContentAutofillDriverFactory::BindAutofillDriver(
    content::RenderFrameHost* render_frame_host,
    mojom::AutofillDriverRequest request) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  // We try to bind to the driver of this render frame host,
  // but if driver is not ready for this render frame host for now,
  // the request will be just dropped, this would cause closing the message pipe
  // which would raise connection error to peer side.
  // Peer side could reconnect later when needed.
  if (!web_contents)
    return;

  ContentAutofillDriverFactory* factory =
      ContentAutofillDriverFactory::FromWebContents(web_contents);
  if (!factory)
    return;

  ContentAutofillDriver* driver = factory->DriverForFrame(render_frame_host);
  if (driver)
    driver->BindRequest(std::move(request));
}

ContentAutofillDriverFactory::ContentAutofillDriverFactory(
    content::WebContents* web_contents,
    AutofillClient* client,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : content::WebContentsObserver(web_contents),
      client_(client),
      app_locale_(app_locale),
      enable_download_manager_(enable_download_manager) {}

ContentAutofillDriver* ContentAutofillDriverFactory::DriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto mapping = frame_driver_map_.find(render_frame_host);
  return mapping == frame_driver_map_.end() ? nullptr : mapping->second.get();
}

bool ContentAutofillDriverFactory::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  return frame_driver_map_[render_frame_host]->HandleMessage(message);
}

void ContentAutofillDriverFactory::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  auto insertion_result =
      frame_driver_map_.insert(std::make_pair(render_frame_host, nullptr));
  // This is called twice for the main frame.
  if (insertion_result.second) {  // This was the first time.
    insertion_result.first->second = base::WrapUnique(new ContentAutofillDriver(
        render_frame_host, client_, app_locale_, enable_download_manager_));
  }
}

void ContentAutofillDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  frame_driver_map_.erase(render_frame_host);
}

void ContentAutofillDriverFactory::DidNavigateAnyFrame(
    content::RenderFrameHost* render_frame_host,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  frame_driver_map_[render_frame_host]->DidNavigateFrame(details, params);
}

void ContentAutofillDriverFactory::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted())
    client_->HideAutofillPopup();
}

void ContentAutofillDriverFactory::WasHidden() {
  client_->HideAutofillPopup();
}

}  // namespace autofill
