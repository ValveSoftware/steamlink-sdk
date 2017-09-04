// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "mojo/public/cpp/bindings/lib/validate_params.h"
#include "mojo/public/cpp/bindings/lib/validation_context.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {

// Checks whether decoding the pointer will overflow and produce a pointer
// smaller than |offset|.
inline bool ValidateEncodedPointer(const uint64_t* offset) {
  // - Make sure |*offset| is no more than 32-bits.
  // - Cast |offset| to uintptr_t so overflow behavior is well defined across
  //   32-bit and 64-bit systems.
  return *offset <= std::numeric_limits<uint32_t>::max() &&
         (reinterpret_cast<uintptr_t>(offset) +
              static_cast<uint32_t>(*offset) >=
          reinterpret_cast<uintptr_t>(offset));
}

template <typename T>
bool ValidatePointer(const Pointer<T>& input,
                     ValidationContext* validation_context) {
  bool result = ValidateEncodedPointer(&input.offset);
  if (!result)
    ReportValidationError(validation_context, VALIDATION_ERROR_ILLEGAL_POINTER);

  return result;
}

// Validates that |data| contains a valid struct header, in terms of alignment
// and size (i.e., the |num_bytes| field of the header is sufficient for storing
// the header itself). Besides, it checks that the memory range
// [data, data + num_bytes) is not marked as occupied by other objects in
// |validation_context|. On success, the memory range is marked as occupied.
// Note: Does not verify |version| or that |num_bytes| is correct for the
// claimed version.
MOJO_CPP_BINDINGS_EXPORT bool ValidateStructHeaderAndClaimMemory(
    const void* data,
    ValidationContext* validation_context);

// Validates that |data| contains a valid union header, in terms of alignment
// and size. It checks that the memory range [data, data + kUnionDataSize) is
// not marked as occupied by other objects in |validation_context|. On success,
// the memory range is marked as occupied.
MOJO_CPP_BINDINGS_EXPORT bool ValidateNonInlinedUnionHeaderAndClaimMemory(
    const void* data,
    ValidationContext* validation_context);

// Validates that the message is a request which doesn't expect a response.
MOJO_CPP_BINDINGS_EXPORT bool ValidateMessageIsRequestWithoutResponse(
    const Message* message,
    ValidationContext* validation_context);

// Validates that the message is a request expecting a response.
MOJO_CPP_BINDINGS_EXPORT bool ValidateMessageIsRequestExpectingResponse(
    const Message* message,
    ValidationContext* validation_context);

// Validates that the message is a response.
MOJO_CPP_BINDINGS_EXPORT bool ValidateMessageIsResponse(
    const Message* message,
    ValidationContext* validation_context);

// Validates that the message payload is a valid struct of type ParamsType.
template <typename ParamsType>
bool ValidateMessagePayload(const Message* message,
                            ValidationContext* validation_context) {
  return ParamsType::Validate(message->payload(), validation_context);
}

// The following Validate.*NonNullable() functions validate that the given
// |input| is not null/invalid.
template <typename T>
bool ValidatePointerNonNullable(const T& input,
                                const char* error_message,
                                ValidationContext* validation_context) {
  if (input.offset)
    return true;

  ReportValidationError(validation_context,
                        VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
                        error_message);
  return false;
}

template <typename T>
bool ValidateInlinedUnionNonNullable(const T& input,
                                     const char* error_message,
                                     ValidationContext* validation_context) {
  if (!input.is_null())
    return true;

  ReportValidationError(validation_context,
                        VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
                        error_message);
  return false;
}

MOJO_CPP_BINDINGS_EXPORT bool IsHandleOrInterfaceValid(
    const AssociatedInterface_Data& input);
MOJO_CPP_BINDINGS_EXPORT bool IsHandleOrInterfaceValid(
    const AssociatedInterfaceRequest_Data& input);
MOJO_CPP_BINDINGS_EXPORT bool IsHandleOrInterfaceValid(
    const Interface_Data& input);
MOJO_CPP_BINDINGS_EXPORT bool IsHandleOrInterfaceValid(
    const Handle_Data& input);

MOJO_CPP_BINDINGS_EXPORT bool ValidateHandleOrInterfaceNonNullable(
    const AssociatedInterface_Data& input,
    const char* error_message,
    ValidationContext* validation_context);
MOJO_CPP_BINDINGS_EXPORT bool ValidateHandleOrInterfaceNonNullable(
    const AssociatedInterfaceRequest_Data& input,
    const char* error_message,
    ValidationContext* validation_context);
MOJO_CPP_BINDINGS_EXPORT bool ValidateHandleOrInterfaceNonNullable(
    const Interface_Data& input,
    const char* error_message,
    ValidationContext* validation_context);
MOJO_CPP_BINDINGS_EXPORT bool ValidateHandleOrInterfaceNonNullable(
    const Handle_Data& input,
    const char* error_message,
    ValidationContext* validation_context);

template <typename T>
bool ValidateContainer(const Pointer<T>& input,
                       ValidationContext* validation_context,
                       const ContainerValidateParams* validate_params) {
  ValidationContext::ScopedDepthTracker depth_tracker(validation_context);
  if (validation_context->ExceedsMaxDepth()) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MAX_RECURSION_DEPTH);
    return false;
  }
  return ValidatePointer(input, validation_context) &&
         T::Validate(input.Get(), validation_context, validate_params);
}

template <typename T>
bool ValidateStruct(const Pointer<T>& input,
                    ValidationContext* validation_context) {
  ValidationContext::ScopedDepthTracker depth_tracker(validation_context);
  if (validation_context->ExceedsMaxDepth()) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MAX_RECURSION_DEPTH);
    return false;
  }
  return ValidatePointer(input, validation_context) &&
         T::Validate(input.Get(), validation_context);
}

template <typename T>
bool ValidateInlinedUnion(const T& input,
                          ValidationContext* validation_context) {
  ValidationContext::ScopedDepthTracker depth_tracker(validation_context);
  if (validation_context->ExceedsMaxDepth()) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MAX_RECURSION_DEPTH);
    return false;
  }
  return T::Validate(&input, validation_context, true);
}

template <typename T>
bool ValidateNonInlinedUnion(const Pointer<T>& input,
                             ValidationContext* validation_context) {
  ValidationContext::ScopedDepthTracker depth_tracker(validation_context);
  if (validation_context->ExceedsMaxDepth()) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MAX_RECURSION_DEPTH);
    return false;
  }
  return ValidatePointer(input, validation_context) &&
         T::Validate(input.Get(), validation_context, false);
}

MOJO_CPP_BINDINGS_EXPORT bool ValidateHandleOrInterface(
    const AssociatedInterface_Data& input,
    ValidationContext* validation_context);
MOJO_CPP_BINDINGS_EXPORT bool ValidateHandleOrInterface(
    const AssociatedInterfaceRequest_Data& input,
    ValidationContext* validation_context);
MOJO_CPP_BINDINGS_EXPORT bool ValidateHandleOrInterface(
    const Interface_Data& input,
    ValidationContext* validation_context);
MOJO_CPP_BINDINGS_EXPORT bool ValidateHandleOrInterface(
    const Handle_Data& input,
    ValidationContext* validation_context);

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_
