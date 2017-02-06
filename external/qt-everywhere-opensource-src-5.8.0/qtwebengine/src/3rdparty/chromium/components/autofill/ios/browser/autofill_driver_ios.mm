// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/browser/autofill_driver_ios.h"

#include "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_thread.h"
#include "ui/gfx/geometry/rect_f.h"

DEFINE_WEB_STATE_USER_DATA_KEY(autofill::AutofillDriverIOS);

namespace autofill {

// static
void AutofillDriverIOS::CreateForWebStateAndDelegate(
    web::WebState* web_state,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager) {
  if (FromWebState(web_state))
    return;

  web_state->SetUserData(
      UserDataKey(),
      new AutofillDriverIOS(web_state, client, bridge, app_locale,
                            enable_download_manager));
}

AutofillDriverIOS::AutofillDriverIOS(
    web::WebState* web_state,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : web_state_(web_state),
      bridge_(bridge),
      autofill_manager_(this, client, app_locale, enable_download_manager),
      autofill_external_delegate_(&autofill_manager_, this) {
  autofill_manager_.SetExternalDelegate(&autofill_external_delegate_);
}

AutofillDriverIOS::~AutofillDriverIOS() {}

bool AutofillDriverIOS::IsOffTheRecord() const {
  return web_state_->GetBrowserState()->IsOffTheRecord();
}

net::URLRequestContextGetter* AutofillDriverIOS::GetURLRequestContext() {
  return web_state_->GetBrowserState()->GetRequestContext();
}

base::SequencedWorkerPool* AutofillDriverIOS::GetBlockingPool() {
  return web::WebThread::GetBlockingPool();
}

bool AutofillDriverIOS::RendererIsAvailable() {
  return true;
}

void AutofillDriverIOS::SendFormDataToRenderer(
    int query_id,
    RendererFormDataAction action,
    const FormData& data) {
  [bridge_ onFormDataFilled:query_id result:data];
}

void AutofillDriverIOS::PropagateAutofillPredictions(
    const std::vector<autofill::FormStructure*>& forms) {
  autofill_manager_.client()->PropagateAutofillPredictions(nullptr, forms);
};

void AutofillDriverIOS::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
  [bridge_ sendAutofillTypePredictionsToRenderer:forms];
}

void AutofillDriverIOS::RendererShouldAcceptDataListSuggestion(
    const base::string16& value) {
}

void AutofillDriverIOS::RendererShouldClearFilledForm() {
}

void AutofillDriverIOS::RendererShouldClearPreviewedForm() {
}

void AutofillDriverIOS::RendererShouldFillFieldWithValue(
    const base::string16& value) {
}

void AutofillDriverIOS::RendererShouldPreviewFieldWithValue(
    const base::string16& value) {
}

void AutofillDriverIOS::PopupHidden() {
}

gfx::RectF AutofillDriverIOS::TransformBoundingBoxToViewportCoordinates(
    const gfx::RectF& bounding_box) {
  return bounding_box;
}

}  // namespace autofill
