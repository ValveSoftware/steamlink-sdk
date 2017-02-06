// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/full_card_request.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {
namespace payments {

FullCardRequest::FullCardRequest(AutofillClient* autofill_client,
                                 payments::PaymentsClient* payments_client,
                                 PersonalDataManager* personal_data_manager)
    : autofill_client_(autofill_client),
      payments_client_(payments_client),
      personal_data_manager_(personal_data_manager),
      delegate_(nullptr),
      should_unmask_card_(false),
      weak_ptr_factory_(this) {
  DCHECK(autofill_client_);
  DCHECK(payments_client_);
  DCHECK(personal_data_manager_);
}

FullCardRequest::~FullCardRequest() {}

void FullCardRequest::GetFullCard(const CreditCard& card,
                                  AutofillClient::UnmaskCardReason reason,
                                  base::WeakPtr<Delegate> delegate) {
  DCHECK(delegate);

  // Only request can be active a time. If the member variable |delegate_| is
  // already set, then immediately reject the new request through the method
  // parameter |delegate|.
  if (delegate_) {
    delegate->OnFullCardError();
    return;
  }

  delegate_ = delegate;
  request_.reset(new payments::PaymentsClient::UnmaskRequestDetails);
  request_->card = card;
  should_unmask_card_ = card.record_type() == CreditCard::MASKED_SERVER_CARD ||
                        (card.record_type() == CreditCard::FULL_SERVER_CARD &&
                         card.ShouldUpdateExpiration(base::Time::Now()));
  if (should_unmask_card_)
    payments_client_->Prepare();

  autofill_client_->ShowUnmaskPrompt(request_->card, reason,
                                     weak_ptr_factory_.GetWeakPtr());

  if (should_unmask_card_) {
    autofill_client_->LoadRiskData(
        base::Bind(&FullCardRequest::OnDidGetUnmaskRiskData,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

bool FullCardRequest::IsGettingFullCard() const {
  return !!request_;
}

void FullCardRequest::OnUnmaskResponse(const UnmaskResponse& response) {
  if (!response.exp_month.empty())
    request_->card.SetRawInfo(CREDIT_CARD_EXP_MONTH, response.exp_month);

  if (!response.exp_year.empty())
    request_->card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, response.exp_year);

  if (request_->card.record_type() == CreditCard::LOCAL_CARD &&
      !request_->card.guid().empty() &&
      (!response.exp_month.empty() || !response.exp_year.empty())) {
    personal_data_manager_->UpdateCreditCard(request_->card);
  }

  if (!should_unmask_card_) {
    if (delegate_)
      delegate_->OnFullCardDetails(request_->card, response.cvc);
    Reset();
    autofill_client_->OnUnmaskVerificationResult(AutofillClient::SUCCESS);
    return;
  }

  request_->user_response = response;
  if (!request_->risk_data.empty()) {
    real_pan_request_timestamp_ = base::Time::Now();
    payments_client_->UnmaskCard(*request_);
  }
}

void FullCardRequest::OnUnmaskPromptClosed() {
  if (delegate_)
    delegate_->OnFullCardError();

  Reset();
}

void FullCardRequest::OnDidGetUnmaskRiskData(const std::string& risk_data) {
  request_->risk_data = risk_data;
  if (!request_->user_response.cvc.empty()) {
    real_pan_request_timestamp_ = base::Time::Now();
    payments_client_->UnmaskCard(*request_);
  }
}

void FullCardRequest::OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                                      const std::string& real_pan) {
  AutofillMetrics::LogRealPanDuration(
      base::Time::Now() - real_pan_request_timestamp_, result);

  switch (result) {
    // Wait for user retry.
    case AutofillClient::TRY_AGAIN_FAILURE:
      break;

    // Neither PERMANENT_FAILURE nor NETWORK_ERROR allow retry.
    case AutofillClient::PERMANENT_FAILURE:
    // Intentional fall through.
    case AutofillClient::NETWORK_ERROR: {
      if (delegate_)
        delegate_->OnFullCardError();
      Reset();
      break;
    }

    case AutofillClient::SUCCESS: {
      DCHECK(!real_pan.empty());
      request_->card.set_record_type(CreditCard::FULL_SERVER_CARD);
      request_->card.SetNumber(base::UTF8ToUTF16(real_pan));
      request_->card.SetServerStatus(CreditCard::OK);

      if (request_->user_response.should_store_pan)
        personal_data_manager_->UpdateServerCreditCard(request_->card);

      if (delegate_)
        delegate_->OnFullCardDetails(request_->card,
                                     request_->user_response.cvc);
      Reset();
      break;
    }

    case AutofillClient::NONE:
      NOTREACHED();
      break;
  }

  autofill_client_->OnUnmaskVerificationResult(result);
}

void FullCardRequest::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  payments_client_->CancelRequest();
  delegate_ = nullptr;
  request_.reset();
  should_unmask_card_ = false;
}

}  // namespace payments
}  // namespace autofill
