/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Graphical Effects module of the Qt Toolkit.
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

#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>
#include <QtQml/qqmlengine.h>

#include "qgfxsourceproxy_p.h"
#include "qgfxshaderbuilder_p.h"

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtGraphicalEffects_private);
#endif
}

QT_BEGIN_NAMESPACE

static QObject *qgfxshaderbuilder_provider(QQmlEngine *, QJSEngine *)
{
    return new QGfxShaderBuilder();
}

class QtGraphicalEffectsPrivatePlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    QtGraphicalEffectsPrivatePlugin(QObject *parent = 0) : QQmlExtensionPlugin(parent) { initResources(); }
    virtual void registerTypes(const char *uri)
    {
        Q_ASSERT(QByteArray(uri) == QByteArrayLiteral("QtGraphicalEffects.private"));
        qmlRegisterType<QGfxSourceProxy>(uri, 1, 0, "SourceProxy");
        qmlRegisterSingletonType<QGfxShaderBuilder>(uri, 1, 0, "ShaderBuilder", qgfxshaderbuilder_provider);
    }
};

QT_END_NAMESPACE

#include "plugin.moc"
