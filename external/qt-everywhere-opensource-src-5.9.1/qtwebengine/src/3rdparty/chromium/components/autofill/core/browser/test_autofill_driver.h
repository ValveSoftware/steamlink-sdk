// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/autofill/core/browser/autofill_driver.h"

namespace base {
class SequencedWorkerPoolOwner;
}

namespace autofill {

// This class is only for easier writing of tests.
class TestAutofillDriver : public AutofillDriver {
 public:
  TestAutofillDriver();
  ~TestAutofillDriver() override;

  // AutofillDriver implementation.
  bool IsOffTheRecord() const override;
  // Returns the value passed in to the last call to |SetURLRequestContext()|
  // or NULL if that method has never been called.
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

  // Methods that tests can use to specialize functionality.

  // Sets the URL request context for this instance. |url_request_context|
  // should outlive this instance.
  void SetURLRequestContext(net::URLRequestContextGetter* url_request_context);

 private:
  std::unique_ptr<base::SequencedWorkerPoolOwner> blocking_pool_owner_;
  net::URLRequestContextGetter* url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDriver);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
