/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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

#include "qvideooutputorientationhandler_p.h"

#include <QGuiApplication>
#include <QScreen>

QT_BEGIN_NAMESPACE

QVideoOutputOrientationHandler::QVideoOutputOrientationHandler(QObject *parent)
    : QObject(parent)
    , m_currentOrientation(0)
{
    QScreen *screen = QGuiApplication::primaryScreen();

    // we want to be informed about all orientation changes
    screen->setOrientationUpdateMask(Qt::PortraitOrientation|Qt::LandscapeOrientation
                                     |Qt::InvertedPortraitOrientation|Qt::InvertedLandscapeOrientation);

    connect(screen, SIGNAL(orientationChanged(Qt::ScreenOrientation)),
            this, SLOT(screenOrientationChanged(Qt::ScreenOrientation)));

    screenOrientationChanged(screen->orientation());
}

int QVideoOutputOrientationHandler::currentOrientation() const
{
    return m_currentOrientation;
}

void QVideoOutputOrientationHandler::screenOrientationChanged(Qt::ScreenOrientation orientation)
{
    const QScreen *screen = QGuiApplication::primaryScreen();

    const int angle = (360 - screen->angleBetween(screen->nativeOrientation(), orientation)) % 360;

    if (angle == m_currentOrientation)
        return;

    m_currentOrientation = angle;
    emit orientationChanged(m_currentOrientation);
}

QT_END_NAMESPACE
