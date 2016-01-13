/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickapplication_p.h"

#include <private/qobject_p.h>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>
#include <QtGui/QGuiApplication>
#include <QtCore/QDebug>
#include <QtQml/private/qqmlglobal_p.h>

QT_BEGIN_NAMESPACE

/*
    This object and its properties are documented as part of the Qt object,
    in qqmlengine.cpp
*/

QQuickApplication::QQuickApplication(QObject *parent)
    : QQmlApplication(parent)
{
    if (qApp) {
        connect(qApp, SIGNAL(layoutDirectionChanged(Qt::LayoutDirection)),
                this, SIGNAL(layoutDirectionChanged()));
        connect(qApp, SIGNAL(applicationStateChanged(Qt::ApplicationState)),
                this, SIGNAL(stateChanged(Qt::ApplicationState)));
        connect(qApp, SIGNAL(applicationStateChanged(Qt::ApplicationState)),
                this, SIGNAL(activeChanged()));
    }
}

QQuickApplication::~QQuickApplication()
{
}

bool QQuickApplication::active() const
{
    return QGuiApplication::applicationState() == Qt::ApplicationActive;
}

Qt::LayoutDirection QQuickApplication::layoutDirection() const
{
    return QGuiApplication::layoutDirection();
}

bool QQuickApplication::supportsMultipleWindows() const
{
    return QGuiApplicationPrivate::platformIntegration()->hasCapability(QPlatformIntegration::MultipleWindows);
}

Qt::ApplicationState QQuickApplication::state() const
{
    return QGuiApplication::applicationState();
}

QT_END_NAMESPACE
