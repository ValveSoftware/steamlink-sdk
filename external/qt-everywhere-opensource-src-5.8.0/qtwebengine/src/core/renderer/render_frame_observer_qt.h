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

#ifndef RENDER_FRAME_OBSERVER_QT_H
#define RENDER_FRAME_OBSERVER_QT_H

#include "base/compiler_specific.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"


namespace content {
class RenderFrame;
}

namespace QtWebEngineCore {

class RenderFrameObserverQt
        : public content::RenderFrameObserver
        , public content::RenderFrameObserverTracker<RenderFrameObserverQt>
{
public:
    explicit RenderFrameObserverQt(content::RenderFrame* render_frame);
    ~RenderFrameObserverQt();

#if defined(ENABLE_PLUGINS)
    void DidCreatePepperPlugin(content::RendererPpapiHost* host) override;
#endif
    void OnDestruct() override { }
    void FrameDetached() override;

    bool isFrameDetached() const;

private:
    DISALLOW_COPY_AND_ASSIGN(RenderFrameObserverQt);

    bool m_isFrameDetached;
};

} // namespace QtWebEngineCore

#endif // RENDER_FRAME_OBSERVER_QT_H
