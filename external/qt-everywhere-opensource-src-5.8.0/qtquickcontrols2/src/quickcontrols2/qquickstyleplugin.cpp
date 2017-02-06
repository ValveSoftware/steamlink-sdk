/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickstyleplugin_p.h"
#include "qquickproxytheme_p.h"
#include "qquickstyle.h"

#include <QtGui/private/qguiapplication_p.h>

QT_BEGIN_NAMESPACE

QQuickStylePlugin::QQuickStylePlugin(QObject *parent) : QQmlExtensionPlugin(parent)
{
}

QQuickStylePlugin::~QQuickStylePlugin()
{
}

void QQuickStylePlugin::registerTypes(const char *uri)
{
    Q_UNUSED(uri);
}

void QQuickStylePlugin::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_UNUSED(engine);
    Q_UNUSED(uri);

    // make sure not to re-create the proxy theme if initializeEngine()
    // is called multiple times, like in case of qml2puppet (QTBUG-54995)
    if (!m_theme.isNull())
        return;

    const QString style = name();
    if (!style.isEmpty() && style.compare(QQuickStyle::name(), Qt::CaseInsensitive) == 0) {
        m_theme.reset(createTheme());
        if (m_theme)
            QGuiApplicationPrivate::platform_theme = m_theme.data();
    }
}

QString QQuickStylePlugin::name() const
{
    return QString();
}

QQuickProxyTheme *QQuickStylePlugin::createTheme() const
{
    return nullptr;
}

/*
    Returns either a file system path if Qt was built as shared libraries,
    or a QRC path if Qt was built statically.
*/
QUrl QQuickStylePlugin::typeUrl(const QString &name) const
{
#ifdef QT_STATIC
    QString url = QLatin1String("qrc") + baseUrl().path();
#else
    QString url = baseUrl().toString();
#endif
    if (!name.isEmpty())
        url += QLatin1Char('/') + name;
    return QUrl(url);
}

QT_END_NAMESPACE
