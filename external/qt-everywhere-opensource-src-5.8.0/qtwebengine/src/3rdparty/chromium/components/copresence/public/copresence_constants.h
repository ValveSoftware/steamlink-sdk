// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CONSTANTS_H_
#define COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CONSTANTS_H_

#include <google/protobuf/repeated_field.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "components/copresence/tokens.h"
#include "media/base/channel_layout.h"

namespace media {
class AudioBusRefCounted;
}

namespace copresence {

class Directive;
class SubscribedMessage;

// Callback to pass a list of directives back to CopresenceState.
using DirectivesCallback = base::Callback<void(const std::vector<Directive>&)>;

// Callback to pass around a list of SubscribedMessages.
using MessagesCallback = base::Callback<void(
    const google::protobuf::RepeatedPtrField<SubscribedMessage>&)>;

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CONSTANTS_H_
