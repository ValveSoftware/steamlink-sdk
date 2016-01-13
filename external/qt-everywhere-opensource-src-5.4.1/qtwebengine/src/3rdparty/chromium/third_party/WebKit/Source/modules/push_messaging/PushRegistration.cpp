// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushRegistration.h"

namespace WebCore {

PushRegistration::PushRegistration(const String& pushEndpoint, const String& pushRegistrationId)
    : m_pushEndpoint(pushEndpoint)
    , m_pushRegistrationId(pushRegistrationId)
{
    ScriptWrappable::init(this);
}

PushRegistration::~PushRegistration()
{
}

} // namespace WebCore
