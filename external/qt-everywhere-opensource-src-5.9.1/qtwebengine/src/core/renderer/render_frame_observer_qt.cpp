/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// This is based on chrome/renderer/pepper/pepper_helper.cc:
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "render_frame_observer_qt.h"

#include "base/memory/ptr_util.h"
#include "chrome/renderer/pepper/pepper_shared_memory_message_filter.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/host/ppapi_host.h"

#include "renderer/pepper/pepper_renderer_host_factory_qt.h"

namespace QtWebEngineCore {

RenderFrameObserverQt::RenderFrameObserverQt(content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame)
    , RenderFrameObserverTracker<RenderFrameObserverQt>(render_frame)
    , m_isFrameDetached(false)
{
}

RenderFrameObserverQt::~RenderFrameObserverQt()
{
}

#if defined(ENABLE_PLUGINS)
void RenderFrameObserverQt::DidCreatePepperPlugin(content::RendererPpapiHost* host)
{
    host->GetPpapiHost()->AddHostFactoryFilter(
                base::WrapUnique(new PepperRendererHostFactoryQt(host)));
    host->GetPpapiHost()->AddInstanceMessageFilter(
                base::WrapUnique(new PepperSharedMemoryMessageFilter(host)));
}
#endif

void RenderFrameObserverQt::FrameDetached()
{
    m_isFrameDetached = true;
}

bool RenderFrameObserverQt::isFrameDetached() const
{
    return m_isFrameDetached;
}

} // namespace QtWebEngineCore
