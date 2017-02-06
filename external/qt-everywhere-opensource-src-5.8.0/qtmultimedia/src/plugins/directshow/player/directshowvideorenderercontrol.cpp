/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "directshowvideorenderercontrol.h"

#include "videosurfacefilter.h"

#ifdef HAVE_EVR
#include "evrcustompresenter.h"
#endif

#include <qabstractvideosurface.h>

DirectShowVideoRendererControl::DirectShowVideoRendererControl(DirectShowEventLoop *loop, QObject *parent)
    : QVideoRendererControl(parent)
    , m_loop(loop)
    , m_surface(0)
    , m_filter(0)
#ifdef HAVE_EVR
    , m_evrPresenter(0)
#endif
{
}

DirectShowVideoRendererControl::~DirectShowVideoRendererControl()
{
#ifdef HAVE_EVR
    if (m_evrPresenter) {
        m_evrPresenter->setSurface(Q_NULLPTR);
        m_evrPresenter->Release();
    }
#endif
    if (m_filter)
        m_filter->Release();
}

QAbstractVideoSurface *DirectShowVideoRendererControl::surface() const
{
    return m_surface;
}

void DirectShowVideoRendererControl::setSurface(QAbstractVideoSurface *surface)
{
    if (m_surface == surface)
        return;

#ifdef HAVE_EVR
    if (m_evrPresenter) {
        m_evrPresenter->setSurface(Q_NULLPTR);
        m_evrPresenter->Release();
        m_evrPresenter = 0;
    }
#endif

    if (m_filter) {
        m_filter->Release();
        m_filter = 0;
    }

    m_surface = surface;

    if (m_surface) {
#ifdef HAVE_EVR
        m_filter = com_new<IBaseFilter>(clsid_EnhancedVideoRenderer);
        m_evrPresenter = new EVRCustomPresenter(m_surface);
        if (!m_evrPresenter->isValid() || !qt_evr_setCustomPresenter(m_filter, m_evrPresenter)) {
            m_filter->Release();
            m_filter = 0;
            m_evrPresenter->Release();
            m_evrPresenter = 0;
        }

        if (!m_filter)
#endif
        {
            m_filter = new VideoSurfaceFilter(m_surface, m_loop);
        }
    }

    emit filterChanged();
}

IBaseFilter *DirectShowVideoRendererControl::filter()
{
    return m_filter;
}
