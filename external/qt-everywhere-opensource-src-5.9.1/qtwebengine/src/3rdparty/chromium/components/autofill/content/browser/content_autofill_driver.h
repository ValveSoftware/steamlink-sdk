// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_

#include <memory>
#include <string>

#include "base/supports_user_data.h"
#include "components/autofill/content/common/autofill_agent.mojom.h"
#include "components/autofill/content/common/autofill_driver.mojom.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
struct FrameNavigateParams;
struct LoadCommittedDetails;
}

namespace autofill {

class AutofillClient;

// Class that drives autofill flow in the browser process based on
// communication from the renderer and from the external world. There is one
// instance per RenderFrameHost.
class ContentAutofillDriver : public AutofillDriver,
                              public mojom::AutofillDriver {
 public:
  ContentAutofillDriver(
      content::RenderFrameHost* render_frame_host,
      AutofillClient* client,
      const std::string& app_locale,
      AutofillManager::AutofillDownloadManagerState enable_download_manager);
  ~ContentAutofillDriver() override;

  // Gets the driver for |render_frame_host|.
  static ContentAutofillDriver* GetForRenderFrameHost(
      content::RenderFrameHost* render_frame_host);

  void BindRequest(mojom::AutofillDriverRequest request);

  // AutofillDriver:
  bool IsOffTheRecord() const override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  base::SequencedWorkerPool* GetBlockingPool() override;
  bool RendererIsAvailable() override;
  void SendFormDataToRenderer(int query_id,
                              RendererFormDataAction action,
                              const FormData& data) override;
  void PropagateAutofillPredictions(
      const std::vector<autofill::FormStructure*>& forms) override;
  void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) override;
  void RendererShouldAcceptDataListSuggestion(
      const base::string16& value) override;
  void RendererShouldClearFilledForm() override;
  void RendererShouldClearPreviewedForm() override;
  void RendererShouldFillFieldWithValue(const base::string16& value) override;
  void RendererShouldPreviewFieldWithValue(
      const base::string16& value) override;
  void PopupHidden() override;
  gfx::RectF TransformBoundingBoxToViewportCoordinates(
      const gfx::RectF& bounding_box) override;
  void DidInteractWithCreditCardForm() override;

  // mojom::AutofillDriver:
  void FirstUserGestureObserved() override;
  void FormsSeen(const std::vector<FormData>& forms,
                 base::TimeTicks timestamp) override;
  void WillSubmitForm(const FormData& form, base::TimeTicks timestamp) override;
  void FormSubmitted(const FormData& form) override;
  void TextFieldDidChange(const FormData& form,
                          const FormFieldData& field,
                          base::TimeTicks timestamp) override;
  void QueryFormFieldAutofill(int32_t id,
                              const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override;
  void HidePopup() override;
  void PingAck() override;
  void FocusNoLongerOnForm() override;
  void DidFillAutofillFormData(const FormData& form,
                               base::TimeTicks timestamp) override;
  void DidPreviewAutofillFormData() override;
  void DidEndTextFieldEditing() override;
  void SetDataList(const std::vector<base::string16>& values,
                   const std::vector<base::string16>& labels) override;

  // Called when the frame has navigated.
  void DidNavigateFrame(const content::LoadCommittedDetails& details,
                        const content::FrameNavigateParams& params);

  // Tells the render frame that a user gesture was observed
  // somewhere in the tab (including in a different frame).
  void NotifyFirstUserGestureObservedInTab();

  AutofillExternalDelegate* autofill_external_delegate() {
    return &autofill_external_delegate_;
  }

  AutofillManager* autofill_manager() { return autofill_manager_.get(); }
  content::RenderFrameHost* render_frame_host() { return render_frame_host_; }

  const mojom::AutofillAgentPtr& GetAutofillAgent();

 protected:
  // Sets the manager to |manager| and sets |manager|'s external delegate
  // to |autofill_external_delegate_|. Takes ownership of |manager|.
  void SetAutofillManager(std::unique_ptr<AutofillManager> manager);

 private:
  // Weak ref to the RenderFrameHost the driver is associated with. Should
  // always be non-NULL and valid for lifetime of |this|.
  content::RenderFrameHost* const render_frame_host_;

  // The per-tab client.
  AutofillClient* client_;

  // AutofillManager instance via which this object drives the shared Autofill
  // code.
  std::unique_ptr<AutofillManager> autofill_manager_;

  // AutofillExternalDelegate instance that this object instantiates in the
  // case where the Autofill native UI is enabled.
  AutofillExternalDelegate autofill_external_delegate_;

  mojo::Binding<mojom::AutofillDriver> binding_;

  mojom::AutofillAgentPtr autofill_agent_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_
