/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
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
#include "../shared/testmodel.h"

void TestPlugin::registerTypes(const char *uri)
{
    // cpp models
    qmlRegisterType<TestObject>(uri, 1, 0, "TestObject");
    qmlRegisterType<TestItemModel>(uri, 1, 0, "TestItemModel");
    qmlRegisterType<TestModel>(uri, 1, 0, "TreeModel");
    qmlRegisterType<TestFetchAppendModel>(uri, 1, 0, "TestFetchAppendModel");
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
