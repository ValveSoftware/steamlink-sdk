/****************************************************************************
**
** Copyright (C) 2015 Paul Lemire
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qtickclockservice_p.h"
#include "qtickclock_p.h"
#include "qabstractframeadvanceservice_p.h"
#include "qabstractframeadvanceservice_p_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

class QTickClockServicePrivate : public QAbstractFrameAdvanceServicePrivate
{
public:
    QTickClockServicePrivate()
        : QAbstractFrameAdvanceServicePrivate(QStringLiteral("Default Frame Advance Service implementation"))
    {
        m_clock.setTickFrequency(60);
        m_clock.start();
    }

    QTickClock m_clock;
};

/* !\internal
    \class Qt3DCore::QTickClockService
    \inmodule Qt3DCore
    \brief Default QAbstractFrameAdvanceService implementation.

    This default QAbstractFrameAdvanceService implementation has a frequency of 60 Hz.
*/
QTickClockService::QTickClockService()
    : QAbstractFrameAdvanceService(*new QTickClockServicePrivate())
{
}

QTickClockService::~QTickClockService()
{
}

qint64 QTickClockService::waitForNextFrame()
{
    Q_D(QTickClockService);
    return d->m_clock.waitForNextTick();
}

/*
    Starts the inner tick clock used by the service.
 */
void QTickClockService::start()
{
    Q_D(QTickClockService);
    d->m_clock.start();
}

void QTickClockService::stop()
{
}

} // Qt3D

QT_END_NAMESPACE
