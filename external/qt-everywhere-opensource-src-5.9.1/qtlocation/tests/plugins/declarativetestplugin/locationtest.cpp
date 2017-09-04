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

#include "qdeclarativepinchgenerator_p.h"
#include "qdeclarativelocationtestmodel_p.h"
#include "testhelper.h"

#include <QtQml/QQmlExtensionPlugin>
#include <QtQml/qqml.h>

#include <QDebug>

QT_BEGIN_NAMESPACE

static QObject *helper_factory(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    TestHelper *helper = new TestHelper();
    return helper;
}

class QLocationDeclarativeTestModule: public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    virtual void registerTypes(const char* uri)
    {
        if (QLatin1String(uri) == QLatin1String("QtLocation.Test")) {
            qmlRegisterType<QDeclarativePinchGenerator>(uri, 5, 5, "PinchGenerator");
            qmlRegisterType<QDeclarativeLocationTestModel>(uri, 5, 5, "TestModel");
            qmlRegisterSingletonType<TestHelper>(uri, 5, 6, "LocationTestHelper", helper_factory);
        } else {
            qWarning() << "Unsupported URI given to load location test QML plugin: " << QLatin1String(uri);
        }
    }
};

#include "locationtest.moc"

QT_END_NAMESPACE

