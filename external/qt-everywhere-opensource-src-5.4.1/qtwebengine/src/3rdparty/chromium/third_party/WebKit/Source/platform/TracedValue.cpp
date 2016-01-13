// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "platform/TracedValue.h"

#include "platform/JSONValues.h"

namespace WebCore {

TracedValue::TracedValue(PassRefPtr<JSONValue> value)
    : m_value(value)
{
}

String TracedValue::asTraceFormat() const
{
    return m_value->toJSONString();
}

TracedValue::~TracedValue()
{
}

}
