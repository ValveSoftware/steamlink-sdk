/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include <QtQml/QQmlEngine>
#include <QtQml/QQmlExtensionPlugin>

#include "qqmlndefrecord.h"
#include "qdeclarativenearfield_p.h"
#include "qdeclarativendeffilter_p.h"
#include "qdeclarativendeftextrecord_p.h"
#include "qdeclarativendefurirecord_p.h"
#include "qdeclarativendefmimerecord_p.h"

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtNfc);
#endif
}

QT_USE_NAMESPACE

class QNfcQmlPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    QNfcQmlPlugin(QObject *parent = 0) : QQmlExtensionPlugin(parent) { initResources(); }
    void registerTypes(const char *uri)
    {
        Q_ASSERT(uri == QStringLiteral("QtNfc"));

        // @uri QtNfc

        // Register the 5.0 types
        int major = 5;
        int minor = 0;

        qmlRegisterType<QDeclarativeNearField>(uri, major, minor, "NearField");
        qmlRegisterType<QDeclarativeNdefFilter>(uri, major, minor, "NdefFilter");
        qmlRegisterType<QQmlNdefRecord>(uri, major, minor, "NdefRecord");
        qmlRegisterType<QDeclarativeNdefTextRecord>(uri, major, minor, "NdefTextRecord");
        qmlRegisterType<QDeclarativeNdefUriRecord>(uri, major, minor, "NdefUriRecord");
        qmlRegisterType<QDeclarativeNdefMimeRecord>(uri, major, minor, "NdefMimeRecord");

        // Register the 5.2 types
        minor = 2;
        qmlRegisterType<QDeclarativeNearField>(uri, major, minor, "NearField");
        qmlRegisterType<QDeclarativeNdefFilter>(uri, major, minor, "NdefFilter");
        qmlRegisterType<QQmlNdefRecord>(uri, major, minor, "NdefRecord");
        qmlRegisterType<QDeclarativeNdefTextRecord>(uri, major, minor, "NdefTextRecord");
        qmlRegisterType<QDeclarativeNdefUriRecord>(uri, major, minor, "NdefUriRecord");
        qmlRegisterType<QDeclarativeNdefMimeRecord>(uri, major, minor, "NdefMimeRecord");

        // Register the 5.4 types
        // introduces 5.4 version, other existing 5.2 exports become automatically available under 5.2-5.4l
        minor = 4;
        qmlRegisterType<QDeclarativeNearField>(uri, major, minor, "NearField");

        // Register the 5.5 types
        minor = 5;
        qmlRegisterType<QDeclarativeNearField, 1>(uri, major, minor, "NearField");

        // Register the 5.6 - 5.8 types
        minor = 8;
        qmlRegisterType<QDeclarativeNearField, 1>(uri, major, minor, "NearField");

    }
};

#include "plugin.moc"

