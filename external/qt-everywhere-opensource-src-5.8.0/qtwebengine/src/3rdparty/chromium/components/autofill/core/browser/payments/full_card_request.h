// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_FULL_CARD_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_FULL_CARD_REQUEST_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/payments_client.h"

namespace autofill {

class AutofillClient;
class CreditCard;
class PersonalDataManager;

namespace payments {

// Retrieves the full card details, including the pan and the cvc.
class FullCardRequest : public CardUnmaskDelegate {
 public:
  // The interface for receiving the full card details.
  class Delegate {
   public:
    virtual void OnFullCardDetails(const CreditCard& card,
                                   const base::string16& cvc) = 0;
    virtual void OnFullCardError() = 0;
  };

  // The parameters should outlive the FullCardRequest.
  FullCardRequest(AutofillClient* autofill_client,
                  payments::PaymentsClient* payments_client,
                  PersonalDataManager* personal_data_manager);
  ~FullCardRequest();

  // Retrieves the pan and cvc for |card| and invokes
  // Delegate::OnFullCardDetails() or Delegate::OnFullCardError(). Only one
  // request should be active at a time.
  //
  // If the card is local, has a non-empty GUID, and the user has updated its
  // expiration date, then this function will write the new information to
  // autofill table on disk.
  void GetFullCard(const CreditCard& card,
                   AutofillClient::UnmaskCardReason reason,
                   base::WeakPtr<Delegate> delegate);

  // Returns true if there's a pending request to get the full card.
  bool IsGettingFullCard() const;

  // Called by the payments client when a card has been unmasked.
  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       const std::string& real_pan);
 private:
  // CardUnmaskDelegate:
  void OnUnmaskResponse(const UnmaskResponse& response) override;
  void OnUnmaskPromptClosed() override;

  // Called by autofill client when the risk data has been loaded.
  void OnDidGetUnmaskRiskData(const std::string& risk_data);

  // Resets the state of the request.
  void Reset();

  // Responsible for showing the UI that prompts the user for the CVC and/or the
  // updated expiration date.
  AutofillClient* const autofill_client_;

  // Responsible for unmasking a masked server card.
  payments::PaymentsClient* const payments_client_;

  // Responsible for updating the server card on disk after it's been unmasked.
  PersonalDataManager* const personal_data_manager_;

  // Receiver of the full PAN and CVC.
  base::WeakPtr<Delegate> delegate_;

  // The pending request to get a card's full PAN and CVC.
  std::unique_ptr<payments::PaymentsClient::UnmaskRequestDetails> request_;

  // Whether the card unmask request should be sent to the payment server.
  bool should_unmask_card_;

  // The timestamp when the full PAN was requested from a server. For
  // histograms.
  base::Time real_pan_request_timestamp_;

  // Enables destroying FullCardRequest while CVC prompt is showing or a server
  // communication is pending.
  base::WeakPtrFactory<FullCardRequest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FullCardRequest);
};

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_FULL_CARD_REQUEST_H_
