/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#include <QtQml/qqml.h>
#include <QQmlEngine>
#include <QVariant>
#include <QStringList>
#include "testplugin.h"
#include "testcppmodels.h"

void TestPlugin::registerTypes(const char *uri)
{
    // cpp models
    qmlRegisterType<TestObject>(uri, 1, 0, "TestObject");
    qmlRegisterType<TestItemModel>(uri, 1, 0, "TestItemModel");
    qmlRegisterSingletonType<SystemInfo>(uri, 1, 0, "SystemInfo", systeminfo_provider);
}

void TestPlugin::initializeEngine(QQmlEngine *engine, const char * /*uri*/)
{
    QObjectList model_qobjectlist;
    model_qobjectlist << new TestObject(0);
    model_qobjectlist << new TestObject(1);
    model_qobjectlist << new TestObject(2);
    engine->rootContext()->setContextProperty("model_qobjectlist", QVariant::fromValue(model_qobjectlist));

    QStringList model_qstringlist;
    model_qstringlist << QStringLiteral("A");
    model_qstringlist << QStringLiteral("B");
    model_qstringlist << QStringLiteral("C");
    engine->rootContext()->setContextProperty("model_qstringlist", model_qstringlist);

    QList<QVariant> model_qvarlist;
    model_qvarlist << 3;
    model_qvarlist << 2;
    model_qvarlist << 1;
    engine->rootContext()->setContextProperty("model_qvarlist", model_qvarlist);
}
