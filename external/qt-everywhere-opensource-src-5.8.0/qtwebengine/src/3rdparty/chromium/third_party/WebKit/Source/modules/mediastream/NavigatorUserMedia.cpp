// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediastream/NavigatorUserMedia.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/mediastream/MediaDevices.h"
#include "platform/Logging.h"

namespace blink {

NavigatorUserMedia::NavigatorUserMedia(ExecutionContext* context)
    : m_mediaDevices(MediaDevices::create(context))
{
}

const char* NavigatorUserMedia::supplementName()
{
    return "NavigatorUserMedia";
}

NavigatorUserMedia& NavigatorUserMedia::from(Navigator& navigator)
{
    NavigatorUserMedia* supplement = static_cast<NavigatorUserMedia*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        ExecutionContext* context = navigator.frame() ? navigator.frame()->document() : nullptr;
        supplement = new NavigatorUserMedia(context);
        provideTo(navigator, supplementName(), supplement);
    }
    return *supplement;
}

MediaDevices* NavigatorUserMedia::getMediaDevices()
{
    return m_mediaDevices;
}

MediaDevices* NavigatorUserMedia::mediaDevices(Navigator& navigator)
{
    return NavigatorUserMedia::from(navigator).getMediaDevices();
}

DEFINE_TRACE(NavigatorUserMedia)
{
    visitor->trace(m_mediaDevices);
    Supplement<Navigator>::trace(visitor);
}

} // namespace blink
