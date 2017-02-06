// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/message_header_validator.h"

#include "mojo/public/cpp/bindings/lib/validation_context.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/lib/validation_util.h"

namespace mojo {
namespace {

bool IsValidMessageHeader(const internal::MessageHeader* header,
                          internal::ValidationContext* validation_context) {
  // NOTE: Our goal is to preserve support for future extension of the message
  // header. If we encounter fields we do not understand, we must ignore them.

  // Extra validation of the struct header:
  if (header->version == 0) {
    if (header->num_bytes != sizeof(internal::MessageHeader)) {
      internal::ReportValidationError(
          validation_context,
          internal::VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
      return false;
    }
  } else if (header->version == 1) {
    if (header->num_bytes != sizeof(internal::MessageHeaderWithRequestID)) {
      internal::ReportValidationError(
          validation_context,
          internal::VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
      return false;
    }
  } else if (header->version > 1) {
    if (header->num_bytes < sizeof(internal::MessageHeaderWithRequestID)) {
      internal::ReportValidationError(
          validation_context,
          internal::VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
      return false;
    }
  }

  // Validate flags (allow unknown bits):

  // These flags require a RequestID.
  if (header->version < 1 && ((header->flags & Message::kFlagExpectsResponse) ||
                              (header->flags & Message::kFlagIsResponse))) {
    internal::ReportValidationError(
        validation_context,
        internal::VALIDATION_ERROR_MESSAGE_HEADER_MISSING_REQUEST_ID);
    return false;
  }

  // These flags are mutually exclusive.
  if ((header->flags & Message::kFlagExpectsResponse) &&
      (header->flags & Message::kFlagIsResponse)) {
    internal::ReportValidationError(
        validation_context,
        internal::VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS);
    return false;
  }

  return true;
}

}  // namespace

MessageHeaderValidator::MessageHeaderValidator(MessageReceiver* sink)
    : MessageHeaderValidator("MessageHeaderValidator", sink) {}

MessageHeaderValidator::MessageHeaderValidator(const std::string& description,
                                               MessageReceiver* sink)
    : MessageFilter(sink), description_(description) {
}

void MessageHeaderValidator::SetDescription(const std::string& description) {
  description_ = description;
}

bool MessageHeaderValidator::Accept(Message* message) {
  // Pass 0 as number of handles because we don't expect any in the header, even
  // if |message| contains handles.
  internal::ValidationContext validation_context(
      message->data(), message->data_num_bytes(), 0, message, description_);

  if (!internal::ValidateStructHeaderAndClaimMemory(message->data(),
                                                    &validation_context))
    return false;

  if (!IsValidMessageHeader(message->header(), &validation_context))
    return false;

  return sink_->Accept(message);
}

}  // namespace mojo
