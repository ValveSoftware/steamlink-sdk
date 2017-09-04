/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickapplication_p.h"
#include <private/qquickscreen_p.h>
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
        connect(qApp, SIGNAL(applicationDisplayNameChanged()),
                this, SIGNAL(displayNameChanged()));

        connect(qApp, &QGuiApplication::screenAdded, this, &QQuickApplication::updateScreens);
        connect(qApp, &QGuiApplication::screenRemoved, this, &QQuickApplication::updateScreens);
        updateScreens();
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

QFont QQuickApplication::font() const
{
    return QGuiApplication::font();
}

QString QQuickApplication::displayName() const
{
    return QGuiApplication::applicationDisplayName();
}

void QQuickApplication::setDisplayName(const QString &displayName)
{
    return QGuiApplication::setApplicationDisplayName(displayName);
}

int screens_count(QQmlListProperty<QQuickScreenInfo> *prop)
{
    return static_cast<QVector<QQuickScreenInfo *> *>(prop->data)->count();
}

QQuickScreenInfo *screens_at(QQmlListProperty<QQuickScreenInfo> *prop, int idx)
{
    return static_cast<QVector<QQuickScreenInfo *> *>(prop->data)->at(idx);
}

QQmlListProperty<QQuickScreenInfo> QQuickApplication::screens()
{
    return QQmlListProperty<QQuickScreenInfo>(this,
        const_cast<QVector<QQuickScreenInfo *> *>(&m_screens), &screens_count, &screens_at);
}

void QQuickApplication::updateScreens()
{
    const QList<QScreen *> screenList = QGuiApplication::screens();
    m_screens.resize(screenList.count());
    for (int i = 0; i < screenList.count(); ++i) {
        if (!m_screens[i])
            m_screens[i] = new QQuickScreenInfo(this);
        m_screens[i]->setWrappedScreen(screenList[i]);
    }
    emit screensChanged();
}

QT_END_NAMESPACE

#include "moc_qquickapplication_p.cpp"
