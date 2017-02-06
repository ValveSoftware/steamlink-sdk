// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_

#include <stdint.h>

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
bool ValidateEncodedPointer(const uint64_t* offset);

// Validates that |data| contains a valid struct header, in terms of alignment
// and size (i.e., the |num_bytes| field of the header is sufficient for storing
// the header itself). Besides, it checks that the memory range
// [data, data + num_bytes) is not marked as occupied by other objects in
// |validation_context|. On success, the memory range is marked as occupied.
// Note: Does not verify |version| or that |num_bytes| is correct for the
// claimed version.
bool ValidateStructHeaderAndClaimMemory(const void* data,
                                        ValidationContext* validation_context);

// Validates that |data| contains a valid union header, in terms of alignment
// and size. If not inlined, it checks that the memory range
// [data, data + num_bytes) is not marked as occupied by other objects in
// |validation_context|. On success, the memory range is marked as occupied.
bool ValidateUnionHeaderAndClaimMemory(const void* data,
                                       bool inlined,
                                       ValidationContext* validation_context);

// Validates that the message is a request which doesn't expect a response.
bool ValidateMessageIsRequestWithoutResponse(
    const Message* message,
    ValidationContext* validation_context);

// Validates that the message is a request expecting a response.
bool ValidateMessageIsRequestExpectingResponse(
    const Message* message,
    ValidationContext* validation_context);

// Validates that the message is a response.
bool ValidateMessageIsResponse(const Message* message,
                               ValidationContext* validation_context);

// Validates that the message payload is a valid struct of type ParamsType.
template <typename ParamsType>
bool ValidateMessagePayload(const Message* message,
                            ValidationContext* validation_context) {
  return ParamsType::Validate(message->payload(), validation_context);
}

// The following methods validate control messages defined in
// interface_control_messages.mojom.
bool ValidateControlRequest(const Message* message,
                            ValidationContext* validation_context);
bool ValidateControlResponse(const Message* message,
                             ValidationContext* validation_context);

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

bool IsHandleOrInterfaceValid(const AssociatedInterface_Data& input);
bool IsHandleOrInterfaceValid(const AssociatedInterfaceRequest_Data& input);
bool IsHandleOrInterfaceValid(const Interface_Data& input);
bool IsHandleOrInterfaceValid(const Handle_Data& input);

bool ValidateHandleOrInterfaceNonNullable(
    const AssociatedInterface_Data& input,
    const char* error_message,
    ValidationContext* validation_context);
bool ValidateHandleOrInterfaceNonNullable(
    const AssociatedInterfaceRequest_Data& input,
    const char* error_message,
    ValidationContext* validation_context);
bool ValidateHandleOrInterfaceNonNullable(
    const Interface_Data& input,
    const char* error_message,
    ValidationContext* validation_context);
bool ValidateHandleOrInterfaceNonNullable(
    const Handle_Data& input,
    const char* error_message,
    ValidationContext* validation_context);

template <typename T>
bool ValidateArray(const Pointer<Array_Data<T>>& input,
                   ValidationContext* validation_context,
                   const ContainerValidateParams* validate_params) {
  if (!ValidateEncodedPointer(&input.offset)) {
    ReportValidationError(validation_context, VALIDATION_ERROR_ILLEGAL_POINTER);
    return false;
  }

  return Array_Data<T>::Validate(DecodePointerRaw(&input.offset),
                                 validation_context, validate_params);
}

template <typename T>
bool ValidateMap(const Pointer<T>& input,
                 ValidationContext* validation_context,
                 const ContainerValidateParams* validate_params) {
  if (!ValidateEncodedPointer(&input.offset)) {
    ReportValidationError(validation_context, VALIDATION_ERROR_ILLEGAL_POINTER);
    return false;
  }

  return T::Validate(DecodePointerRaw(&input.offset), validation_context,
                     validate_params);
}

template <typename T>
bool ValidateStruct(const Pointer<T>& input,
                    ValidationContext* validation_context) {
  if (!ValidateEncodedPointer(&input.offset)) {
    ReportValidationError(validation_context, VALIDATION_ERROR_ILLEGAL_POINTER);
    return false;
  }

  return T::Validate(DecodePointerRaw(&input.offset), validation_context);
}

template <typename T>
bool ValidateInlinedUnion(const T& input,
                          ValidationContext* validation_context) {
  return T::Validate(&input, validation_context, true);
}

template <typename T>
bool ValidateNonInlinedUnion(const Pointer<T>& input,
                             ValidationContext* validation_context) {
  if (!ValidateEncodedPointer(&input.offset)) {
    ReportValidationError(validation_context, VALIDATION_ERROR_ILLEGAL_POINTER);
    return false;
  }

  return T::Validate(DecodePointerRaw(&input.offset), validation_context,
                     false);
}

bool ValidateHandleOrInterface(const AssociatedInterface_Data& input,
                               ValidationContext* validation_context);
bool ValidateHandleOrInterface(const AssociatedInterfaceRequest_Data& input,
                               ValidationContext* validation_context);
bool ValidateHandleOrInterface(const Interface_Data& input,
                               ValidationContext* validation_context);
bool ValidateHandleOrInterface(const Handle_Data& input,
                               ValidationContext* validation_context);

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATION_UTIL_H_
