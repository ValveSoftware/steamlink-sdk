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

#ifndef DIRECTSHOWUTILS_H
#define DIRECTSHOWUTILS_H

#include "directshowglobal.h"

QT_BEGIN_NAMESPACE

namespace DirectShowUtils
{
template <typename T>
void safeRelease(T **iface) {
    if (!iface)
        return;

    if (!*iface)
        return;

    (*iface)->Release();
    *iface = nullptr;
}

template <typename T>
struct ScopedSafeRelease
{
    T **iunknown;
    ~ScopedSafeRelease()
    {
        DirectShowUtils::safeRelease(iunknown);
    }
};

bool getPin(IBaseFilter *filter, PIN_DIRECTION pinDirection, IPin **pin, HRESULT *hrOut);
bool isPinConnected(IPin *pin, HRESULT *hrOut = nullptr);
bool hasPinDirection(IPin *pin, PIN_DIRECTION direction, HRESULT *hrOut = nullptr);
bool matchPin(IPin *pin, PIN_DIRECTION pinDirection, BOOL shouldBeConnected, HRESULT *hrOut = nullptr);
bool findUnconnectedPin(IBaseFilter *filter, PIN_DIRECTION pinDirection, IPin **pin, HRESULT *hrOut = nullptr);
bool connectFilters(IGraphBuilder *graph, IPin *outputPin, IBaseFilter *filter, HRESULT *hrOut = nullptr);
bool connectFilters(IGraphBuilder *graph, IBaseFilter *filter, IPin *inputPin, HRESULT *hrOut = nullptr);
bool connectFilters(IGraphBuilder *graph,
                    IBaseFilter *upstreamFilter,
                    IBaseFilter *downstreamFilter,
                    bool autoConnect = false,
                    HRESULT *hrOut = nullptr);
}

QT_END_NAMESPACE

#endif // DIRECTSHOWUTILS_H
