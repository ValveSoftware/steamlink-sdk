// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or Multiply-included shared traits file depending on circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#define CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebContentSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebPageVisibilityState.h"
#include "webkit/common/resource_type.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(ResourceType::Type, ResourceType::LAST_TYPE - 1)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebContentSecurityPolicyType,
                          blink::WebContentSecurityPolicyTypeLast)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(blink::WebInputEvent::Type,
                              blink::WebInputEvent::TypeFirst,
                              blink::WebInputEvent::TypeLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebPageVisibilityState,
                          blink::WebPageVisibilityStateLast)

IPC_STRUCT_TRAITS_BEGIN(blink::WebCompositionUnderline)
  IPC_STRUCT_TRAITS_MEMBER(startOffset)
  IPC_STRUCT_TRAITS_MEMBER(endOffset)
  IPC_STRUCT_TRAITS_MEMBER(color)
  IPC_STRUCT_TRAITS_MEMBER(thick)
  IPC_STRUCT_TRAITS_MEMBER(backgroundColor)
IPC_STRUCT_TRAITS_END()

#endif  // CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
