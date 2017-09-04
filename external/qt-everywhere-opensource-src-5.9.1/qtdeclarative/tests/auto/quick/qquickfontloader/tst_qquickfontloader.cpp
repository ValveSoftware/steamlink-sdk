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
#include <qtest.h>
#include <QtTest/QSignalSpy>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlcontext.h>
#include <QtQuick/private/qquickfontloader_p.h>
#include "../../shared/util.h"
#include "../../shared/testhttpserver.h"
#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>

class tst_qquickfontloader : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickfontloader();

private slots:
    void initTestCase();
    void noFont();
    void namedFont();
    void localFont();
    void failLocalFont();
    void webFont();
    void redirWebFont();
    void failWebFont();
    void changeFont();
    void changeFontSourceViaState();

private:
    QQmlEngine engine;
    TestHTTPServer server;
};

tst_qquickfontloader::tst_qquickfontloader()
{
}

void tst_qquickfontloader::initTestCase()
{
    QQmlDataTest::initTestCase();
    server.serveDirectory(dataDirectory());
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
}

void tst_qquickfontloader::noFont()
{
    QString componentStr = "import QtQuick 2.0\nFontLoader { }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickFontLoader *fontObject = qobject_cast<QQuickFontLoader*>(component.create());

    QVERIFY(fontObject != 0);
    QCOMPARE(fontObject->name(), QString(""));
    QCOMPARE(fontObject->source(), QUrl(""));
    QTRY_COMPARE(fontObject->status(), QQuickFontLoader::Null);

    delete fontObject;
}

void tst_qquickfontloader::namedFont()
{
    QString componentStr = "import QtQuick 2.0\nFontLoader { name: \"Helvetica\" }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickFontLoader *fontObject = qobject_cast<QQuickFontLoader*>(component.create());

    QVERIFY(fontObject != 0);
    QCOMPARE(fontObject->source(), QUrl(""));
    QCOMPARE(fontObject->name(), QString("Helvetica"));
    QTRY_COMPARE(fontObject->status(), QQuickFontLoader::Ready);
}

void tst_qquickfontloader::localFont()
{
    QString componentStr = "import QtQuick 2.0\nFontLoader { source: \"" + testFileUrl("tarzeau_ocr_a.ttf").toString() + "\" }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickFontLoader *fontObject = qobject_cast<QQuickFontLoader*>(component.create());

    QVERIFY(fontObject != 0);
    QVERIFY(fontObject->source() != QUrl(""));
    QTRY_COMPARE(fontObject->name(), QString("OCRA"));
    QTRY_COMPARE(fontObject->status(), QQuickFontLoader::Ready);
}

void tst_qquickfontloader::failLocalFont()
{
    QString componentStr = "import QtQuick 2.0\nFontLoader { source: \"" + testFileUrl("dummy.ttf").toString() + "\" }";
    QTest::ignoreMessage(QtWarningMsg, QString("<Unknown File>:2:1: QML FontLoader: Cannot load font: \"" + testFileUrl("dummy.ttf").toString() + QLatin1Char('"')).toUtf8().constData());
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickFontLoader *fontObject = qobject_cast<QQuickFontLoader*>(component.create());

    QVERIFY(fontObject != 0);
    QVERIFY(fontObject->source() != QUrl(""));
    QTRY_COMPARE(fontObject->name(), QString(""));
    QTRY_COMPARE(fontObject->status(), QQuickFontLoader::Error);
}

void tst_qquickfontloader::webFont()
{
    QString componentStr = "import QtQuick 2.0\nFontLoader { source: \"" + server.baseUrl().toString() + "/tarzeau_ocr_a.ttf\" }";
    QQmlComponent component(&engine);

    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickFontLoader *fontObject = qobject_cast<QQuickFontLoader*>(component.create());

    QVERIFY(fontObject != 0);
    QVERIFY(fontObject->source() != QUrl(""));
    QTRY_COMPARE(fontObject->name(), QString("OCRA"));
    QTRY_COMPARE(fontObject->status(), QQuickFontLoader::Ready);
}

void tst_qquickfontloader::redirWebFont()
{
    server.addRedirect("olddir/oldname.ttf","../tarzeau_ocr_a.ttf");

    QString componentStr = "import QtQuick 2.0\nFontLoader { source: \"" + server.baseUrl().toString() + "/olddir/oldname.ttf\" }";
    QQmlComponent component(&engine);

    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickFontLoader *fontObject = qobject_cast<QQuickFontLoader*>(component.create());

    QVERIFY(fontObject != 0);
    QVERIFY(fontObject->source() != QUrl(""));
    QTRY_COMPARE(fontObject->name(), QString("OCRA"));
    QTRY_COMPARE(fontObject->status(), QQuickFontLoader::Ready);
}

void tst_qquickfontloader::failWebFont()
{
    QString componentStr = "import QtQuick 2.0\nFontLoader { source: \"" + server.baseUrl().toString() + "/nonexist.ttf\" }";
    const QString expectedError = "<Unknown File>:2:1: QML FontLoader: Cannot load font: \"" + server.baseUrl().toString() + "/nonexist.ttf\"";
    QTest::ignoreMessage(QtWarningMsg, expectedError.toUtf8());
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickFontLoader *fontObject = qobject_cast<QQuickFontLoader*>(component.create());

    QVERIFY(fontObject != 0);
    QVERIFY(fontObject->source() != QUrl(""));
    QTRY_COMPARE(fontObject->name(), QString(""));
    QTRY_COMPARE(fontObject->status(), QQuickFontLoader::Error);
}

void tst_qquickfontloader::changeFont()
{
    QString componentStr = "import QtQuick 2.0\nFontLoader { source: font }";
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("font", testFileUrl("tarzeau_ocr_a.ttf"));
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickFontLoader *fontObject = qobject_cast<QQuickFontLoader*>(component.create());

    QVERIFY(fontObject != 0);

    QSignalSpy nameSpy(fontObject, SIGNAL(nameChanged()));
    QSignalSpy statusSpy(fontObject, SIGNAL(statusChanged()));

    QTRY_COMPARE(fontObject->status(), QQuickFontLoader::Ready);
    QCOMPARE(nameSpy.count(), 0);
    QCOMPARE(statusSpy.count(), 0);
    QTRY_COMPARE(fontObject->name(), QString("OCRA"));

    ctxt->setContextProperty("font", server.urlString("/daniel.ttf"));
    QTRY_COMPARE(fontObject->status(), QQuickFontLoader::Loading);
    QTRY_COMPARE(fontObject->status(), QQuickFontLoader::Ready);
    QCOMPARE(nameSpy.count(), 1);
    QCOMPARE(statusSpy.count(), 2);
    QTRY_COMPARE(fontObject->name(), QString("Daniel"));

    ctxt->setContextProperty("font", testFileUrl("tarzeau_ocr_a.ttf"));
    QTRY_COMPARE(fontObject->status(), QQuickFontLoader::Ready);
    QCOMPARE(nameSpy.count(), 2);
    QCOMPARE(statusSpy.count(), 2);
    QTRY_COMPARE(fontObject->name(), QString("OCRA"));

    ctxt->setContextProperty("font", server.urlString("/daniel.ttf"));
    QTRY_COMPARE(fontObject->status(), QQuickFontLoader::Ready);
    QCOMPARE(nameSpy.count(), 3);
    QCOMPARE(statusSpy.count(), 2);
    QTRY_COMPARE(fontObject->name(), QString("Daniel"));
}

void tst_qquickfontloader::changeFontSourceViaState()
{
    QQuickView window(testFileUrl("qtbug-20268.qml"));
    window.show();
    window.requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    QCOMPARE(&window, qGuiApp->focusWindow());

    QQuickFontLoader *fontObject = qobject_cast<QQuickFontLoader*>(qvariant_cast<QObject *>(window.rootObject()->property("fontloader")));
    QVERIFY(fontObject != 0);
    QTRY_COMPARE(fontObject->status(), QQuickFontLoader::Ready);
    QVERIFY(fontObject->source() != QUrl(""));
    QTRY_COMPARE(fontObject->name(), QString("OCRA"));

    window.rootObject()->setProperty("usename", true);

    // This warning should probably not be printed once QTBUG-20268 is fixed
    QString warning = QString(testFileUrl("qtbug-20268.qml").toString()) +
                              QLatin1String(":13:5: QML FontLoader: Cannot load font: \"\"");
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning));

    QEXPECT_FAIL("", "QTBUG-20268", Abort);
    QTRY_COMPARE(fontObject->status(), QQuickFontLoader::Ready);
    QCOMPARE(window.rootObject()->property("name").toString(), QString("Tahoma"));
}

QTEST_MAIN(tst_qquickfontloader)

#include "tst_qquickfontloader.moc"
