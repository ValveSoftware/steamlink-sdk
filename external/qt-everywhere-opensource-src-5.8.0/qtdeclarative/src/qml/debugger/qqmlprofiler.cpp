/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlprofiler_p.h"
#include "qqmldebugservice_p.h"

QT_BEGIN_NAMESPACE

QQmlProfiler::QQmlProfiler() : featuresEnabled(0)
{
    static int metatype = qRegisterMetaType<QVector<QQmlProfilerData> >();
    static int metatype2 = qRegisterMetaType<QQmlProfiler::LocationHash> ();
    Q_UNUSED(metatype);
    Q_UNUSED(metatype2);
    m_timer.start();
}

void QQmlProfiler::startProfiling(quint64 features)
{
    featuresEnabled = features;
}

void QQmlProfiler::stopProfiling()
{
    featuresEnabled = false;
    reportData(true);
    m_locations.clear();
}

void QQmlProfiler::reportData(bool trackLocations)
{
    LocationHash resolved;
    resolved.reserve(m_locations.size());
    for (auto it = m_locations.begin(), end = m_locations.end(); it != end; ++it) {
        if (!trackLocations || !it->sent) {
            resolved.insert(it.key(), it.value());
            if (trackLocations)
                it->sent = true;
        }
    }

    QVector<QQmlProfilerData> data;
    data.swap(m_data);
    emit dataReady(data, resolved);
}

QT_END_NAMESPACE
