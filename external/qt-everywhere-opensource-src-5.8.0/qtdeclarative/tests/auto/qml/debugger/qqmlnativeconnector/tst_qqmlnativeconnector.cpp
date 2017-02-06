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

#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qtimer.h>
#include <QtTest/qtest.h>
#include <QtQml/qqmlapplicationengine.h>
#include <QtQml/qqmlcomponent.h>

#include <private/qhooks_p.h>

const char testData[] =
"import QtQuick 2.0\n"
"Item {\n"
"    id: item\n"
"    property int a: 0\n"
"    Timer {\n"
"        id: timer;  interval: 1; repeat: true; running: true\n"
"        onTriggered: {\n"
"        a++\n"
"        }\n"
"    }\n"
"}\n";

struct ResolvedHooks
{
    quintptr version;
    quintptr numEntries;
    const char **qt_qmlDebugMessageBuffer;
    int *qt_qmlDebugMessageLength;
    bool (*qt_qmlDebugSendDataToService)(const char *serviceName, const char *hexData);
    bool (*qt_qmlDebugEnableService)(const char *data);
    bool (*qt_qmlDebugDisableService)(const char *data);
    void (*qt_qmlDebugObjectAvailable)();
} *hooks;

class Application : public QGuiApplication
{
    Q_OBJECT
public:
    Application(int &argc, char **argv)
        : QGuiApplication(argc, argv),
          component(&engine)
    {
        component.setData(testData, QUrl("MyStuff"));
        mainObject = component.create();
    }

private slots:
    void testEcho()
    {
        QJsonObject request;
        QJsonObject arguments;
        arguments.insert(QLatin1String("test"), QLatin1String("BUH"));
        request.insert(QLatin1String("command"), QLatin1String("echo"));
        request.insert(QLatin1String("arguments"), arguments);
        QJsonDocument doc;
        doc.setObject(request);
        QByteArray hexdata = doc.toJson(QJsonDocument::Compact).toHex();

        hooks = (ResolvedHooks *)qtHookData[QHooks::Startup];
        QCOMPARE(bool(hooks), true); // Available after connector start only.
        QCOMPARE(hooks->version, quintptr(1));
        QCOMPARE(hooks->numEntries, quintptr(6));
        QCOMPARE(bool(hooks->qt_qmlDebugSendDataToService), true);
        QCOMPARE(bool(hooks->qt_qmlDebugMessageBuffer), true);
        QCOMPARE(bool(hooks->qt_qmlDebugMessageLength), true);
        QCOMPARE(bool(hooks->qt_qmlDebugEnableService), true);

        hooks->qt_qmlDebugEnableService("NativeQmlDebugger");
        hooks->qt_qmlDebugSendDataToService("NativeQmlDebugger", hexdata);
        QByteArray response(*hooks->qt_qmlDebugMessageBuffer, *hooks->qt_qmlDebugMessageLength);

        QCOMPARE(response, QByteArray("NativeQmlDebugger 25 {\"result\":{\"test\":\"BUH\"}}"));
    }

private:
    QQmlApplicationEngine engine;
    QQmlComponent component;
    QObject *mainObject;
};

int main(int argc, char *argv[])
{
    char **argv2 = new char *[argc + 2];
    for (int i = 0; i < argc; ++i)
        argv2[i] = argv[i];
    argv2[argc] = strdup("-qmljsdebugger=native,services:NativeQmlDebugger");
    ++argc;
    argv2[argc] = 0;
    Application app(argc, argv2);
    return QTest::qExec(&app, argc, argv);
}

#include "tst_qqmlnativeconnector.moc"
