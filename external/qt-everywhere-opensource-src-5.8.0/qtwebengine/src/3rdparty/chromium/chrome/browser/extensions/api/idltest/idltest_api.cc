// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/idltest/idltest_api.h"

#include <stddef.h>

#include <memory>

#include "base/values.h"

using base::BinaryValue;

namespace {

std::unique_ptr<base::ListValue> CopyBinaryValueToIntegerList(
    const BinaryValue* input) {
  std::unique_ptr<base::ListValue> output(new base::ListValue());
  const char* input_buffer = input->GetBuffer();
  for (size_t i = 0; i < input->GetSize(); i++) {
    output->AppendInteger(input_buffer[i]);
  }
  return output;
}

}  // namespace

bool IdltestSendArrayBufferFunction::RunSync() {
  BinaryValue* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_ != NULL && args_->GetBinary(0, &input));
  SetResult(CopyBinaryValueToIntegerList(input));
  return true;
}

bool IdltestSendArrayBufferViewFunction::RunSync() {
  BinaryValue* input = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_ != NULL && args_->GetBinary(0, &input));
  SetResult(CopyBinaryValueToIntegerList(input));
  return true;
}

bool IdltestGetArrayBufferFunction::RunSync() {
  std::string hello = "hello world";
  SetResult(BinaryValue::CreateWithCopiedBuffer(hello.c_str(), hello.size()));
  return true;
}
