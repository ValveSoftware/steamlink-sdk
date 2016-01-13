/****************************************************************************
**
** Copyright (C) 2012 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "bbguihelper.h"

#include <QtCore/QAbstractEventDispatcher>
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <bps/navigator.h>

BbGuiHelper::BbGuiHelper(QObject *parent)
    : QObject(parent),
      m_currentOrientation(0),
      m_applicationActive(true)
{
    readApplicationActiveState();
    readOrientation();

    QCoreApplication::eventDispatcher()->installNativeEventFilter(this);
}

BbGuiHelper::~BbGuiHelper()
{
    QCoreApplication::eventDispatcher()->removeNativeEventFilter(this);
}

int BbGuiHelper::currentOrientation() const
{
    return m_currentOrientation;
}

bool BbGuiHelper::applicationActive() const
{
    return m_applicationActive;
}

bool BbGuiHelper::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(result);
    Q_UNUSED(eventType);

    bps_event_t * const event = static_cast<bps_event_t *>(message);
    if (event && bps_event_get_domain(event) == navigator_get_domain()) {
        const int code = bps_event_get_code(event);
        if (code == NAVIGATOR_ORIENTATION) {
            const int newOrientation = navigator_event_get_orientation_angle(event);
            if (newOrientation != m_currentOrientation) {
                m_currentOrientation = newOrientation;
                emit orientationChanged();
            }
        } else if (code == NAVIGATOR_WINDOW_STATE) {
            const bool appActive =
                    (navigator_event_get_window_state(event) == NAVIGATOR_WINDOW_FULLSCREEN);
            if (m_applicationActive != appActive) {
                m_applicationActive = appActive;
                emit applicationActiveChanged();
            }
        }
    }

    return false;
}

void BbGuiHelper::readApplicationActiveState()
{
    const QLatin1String fileName("/pps/services/navigator/state");
    QFile navigatorState(fileName);
    if (!navigatorState.open(QFile::ReadOnly))
        return;

    const QString separator(QLatin1String("::"));
    QTextStream stream(&navigatorState);
    Q_FOREVER {
        const QString line = stream.readLine();
        if (line.isNull())
            break;

        const int separatorPos = line.indexOf(separator);
        if (separatorPos != -1) {
            const QString key = line.left(separatorPos);
            const QString value = line.mid(separatorPos + separator.length());

            if (key.endsWith(QLatin1String("fullscreen"))) {
                bool ok = false;
                const int fullscreenPid = value.toInt(&ok);
                if (ok)
                    m_applicationActive = (fullscreenPid == QCoreApplication::applicationPid());
                break;
            }
        }
    }
}

void BbGuiHelper::readOrientation()
{
    // There is no API to get the current orientation at the moment.
    // Therefore, we assume that the initial orientation that is set in the environment variable
    // hasn't changed yet.
    // This assumptions don't always hold, but it is the best we got so far.
    // The navigator will at least inform us about updates.
    const QByteArray orientationText = qgetenv("ORIENTATION");
    if (!orientationText.isEmpty())
        m_currentOrientation = orientationText.toInt();
}
