// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebMediaDeviceChangeObserver.h"

#include "core/dom/Document.h"
#include "modules/mediastream/MediaDevices.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

WebMediaDeviceChangeObserver::WebMediaDeviceChangeObserver()
    : m_private(nullptr)
{
}

WebMediaDeviceChangeObserver::WebMediaDeviceChangeObserver(bool unused)
    : m_private(MediaDevices::create(Document::create()))
{
}

WebMediaDeviceChangeObserver::WebMediaDeviceChangeObserver(const WebMediaDeviceChangeObserver& other)
{
    assign(other);
}

WebMediaDeviceChangeObserver& WebMediaDeviceChangeObserver::operator=(const WebMediaDeviceChangeObserver& other)
{
    assign(other);
    return *this;
}

WebMediaDeviceChangeObserver::WebMediaDeviceChangeObserver(MediaDevices* observer)
    : m_private(observer)
{
}

WebMediaDeviceChangeObserver::~WebMediaDeviceChangeObserver()
{
    reset();
}

bool WebMediaDeviceChangeObserver::isNull() const
{
    return m_private.isNull();
}

void WebMediaDeviceChangeObserver::didChangeMediaDevices()
{
    if (m_private.isNull())
        return;

    m_private->didChangeMediaDevices();
}

WebSecurityOrigin WebMediaDeviceChangeObserver::getSecurityOrigin() const
{
    if (m_private.isNull())
        return WebSecurityOrigin();

    return WebSecurityOrigin(m_private->getExecutionContext()->getSecurityOrigin());
}

void WebMediaDeviceChangeObserver::assign(const WebMediaDeviceChangeObserver& other)
{
    m_private = other.m_private;
}

void WebMediaDeviceChangeObserver::reset()
{
    m_private.reset();
}

} // namespace blink
