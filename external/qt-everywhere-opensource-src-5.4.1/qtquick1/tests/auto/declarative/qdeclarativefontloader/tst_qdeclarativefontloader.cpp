/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <qtest.h>
#include <qdeclarativedatatest.h>
#include <QtTest/QSignalSpy>
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <QtDeclarative/qdeclarativecontext.h>
#include <private/qdeclarativefontloader_p.h>
#include "../shared/testhttpserver.h"

#define SERVER_PORT 14448

class tst_qdeclarativefontloader : public QDeclarativeDataTest
{
    Q_OBJECT
public:
    tst_qdeclarativefontloader();
    virtual void initTestCase();

private slots:
    void init();
    void noFont();
    void namedFont();
    void localFont();
    void failLocalFont();
    void webFont();
    void redirWebFont();
    void failWebFont();
    void changeFont();

private:
    QDeclarativeEngine engine;
    TestHTTPServer server;
};

tst_qdeclarativefontloader::tst_qdeclarativefontloader() :
    server(SERVER_PORT)
{
}

void tst_qdeclarativefontloader::initTestCase()
{
    QDeclarativeDataTest::initTestCase();
    QVERIFY(server.serveDirectory(dataDirectory()));
}

void tst_qdeclarativefontloader::init()
{
    QVERIFY(server.isValid());
}

void tst_qdeclarativefontloader::noFont()
{
    QString componentStr = "import QtQuick 1.0\nFontLoader { }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeFontLoader *fontObject = qobject_cast<QDeclarativeFontLoader*>(component.create());

    QVERIFY(fontObject != 0);
    QCOMPARE(fontObject->name(), QString(""));
    QCOMPARE(fontObject->source(), QUrl(""));
    QTRY_VERIFY(fontObject->status() == QDeclarativeFontLoader::Null);

    delete fontObject;
}

void tst_qdeclarativefontloader::namedFont()
{
    QString componentStr = "import QtQuick 1.0\nFontLoader { name: \"Helvetica\" }";
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeFontLoader *fontObject = qobject_cast<QDeclarativeFontLoader*>(component.create());

    QVERIFY(fontObject != 0);
    QCOMPARE(fontObject->source(), QUrl(""));
    QCOMPARE(fontObject->name(), QString("Helvetica"));
    QTRY_VERIFY(fontObject->status() == QDeclarativeFontLoader::Ready);
}

void tst_qdeclarativefontloader::localFont()
{
    // Format a local file URL, else "C:\foo" is interpreted as an
    // URL with protocol 'C' on Windows.
    const QString localFileUrl = QUrl::fromLocalFile(directory() + QStringLiteral("/data/tarzeau_ocr_a.ttf")).toString();
    const QString componentStr = QStringLiteral("import QtQuick 1.0\nFontLoader { source: \"")
                                 + localFileUrl  + QStringLiteral("\" }");
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeFontLoader *fontObject = qobject_cast<QDeclarativeFontLoader*>(component.create());

    QVERIFY(fontObject != 0);
    QVERIFY(fontObject->source() != QUrl(""));
    QTRY_COMPARE(fontObject->name(), QString("OCRA"));
    QTRY_VERIFY(fontObject->status() == QDeclarativeFontLoader::Ready);
}

void tst_qdeclarativefontloader::failLocalFont()
{
    const QString urlString = testFileUrl("dummy.ttf").toString();
    const QString componentStr =
        QStringLiteral("import QtQuick 1.0\nFontLoader { source: \"")
        + urlString + QStringLiteral("\" }");
    const QByteArray message = QByteArrayLiteral("<Unknown File>:2:1: QML FontLoader: Cannot load font: \"")
                               + urlString.toUtf8() + '"';
    QTest::ignoreMessage(QtWarningMsg, message.constData());
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeFontLoader *fontObject = qobject_cast<QDeclarativeFontLoader*>(component.create());

    QVERIFY(fontObject != 0);
    QVERIFY(fontObject->source() != QUrl(""));
    QTRY_COMPARE(fontObject->name(), QString(""));
    QTRY_VERIFY(fontObject->status() == QDeclarativeFontLoader::Error);
}

void tst_qdeclarativefontloader::webFont()
{
    QString componentStr = "import QtQuick 1.0\nFontLoader { source: \"http://localhost:14448/tarzeau_ocr_a.ttf\" }";
    QDeclarativeComponent component(&engine);

    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeFontLoader *fontObject = qobject_cast<QDeclarativeFontLoader*>(component.create());

    QVERIFY(fontObject != 0);
    QVERIFY(fontObject->source() != QUrl(""));
    QTRY_COMPARE(fontObject->name(), QString("OCRA"));
    QTRY_VERIFY(fontObject->status() == QDeclarativeFontLoader::Ready);
}

void tst_qdeclarativefontloader::redirWebFont()
{
    server.addRedirect("olddir/oldname.ttf","../tarzeau_ocr_a.ttf");

    QString componentStr = "import QtQuick 1.0\nFontLoader { source: \"http://localhost:14448/olddir/oldname.ttf\" }";
    QDeclarativeComponent component(&engine);

    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeFontLoader *fontObject = qobject_cast<QDeclarativeFontLoader*>(component.create());

    QVERIFY(fontObject != 0);
    QVERIFY(fontObject->source() != QUrl(""));
    QTRY_COMPARE(fontObject->name(), QString("OCRA"));
    QTRY_VERIFY(fontObject->status() == QDeclarativeFontLoader::Ready);
}

void tst_qdeclarativefontloader::failWebFont()
{
    QString componentStr = "import QtQuick 1.0\nFontLoader { source: \"http://localhost:14448/nonexist.ttf\" }";
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>:2:1: QML FontLoader: Cannot load font: \"http://localhost:14448/nonexist.ttf\"");
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeFontLoader *fontObject = qobject_cast<QDeclarativeFontLoader*>(component.create());

    QVERIFY(fontObject != 0);
    QVERIFY(fontObject->source() != QUrl(""));
    QTRY_COMPARE(fontObject->name(), QString(""));
    QTRY_VERIFY(fontObject->status() == QDeclarativeFontLoader::Error);
}

void tst_qdeclarativefontloader::changeFont()
{
    QString componentStr = "import QtQuick 1.0\nFontLoader { source: font }";
    QDeclarativeContext *ctxt = engine.rootContext();
    const QUrl fontUrl = testFileUrl("tarzeau_ocr_a.ttf");
    ctxt->setContextProperty("font", fontUrl);
    QDeclarativeComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QDeclarativeFontLoader *fontObject = qobject_cast<QDeclarativeFontLoader*>(component.create());

    QVERIFY(fontObject != 0);

    QSignalSpy nameSpy(fontObject, SIGNAL(nameChanged()));
    QSignalSpy statusSpy(fontObject, SIGNAL(statusChanged()));

    QTRY_VERIFY(fontObject->status() == QDeclarativeFontLoader::Ready);
    QCOMPARE(nameSpy.count(), 0);
    QCOMPARE(statusSpy.count(), 0);
    QTRY_COMPARE(fontObject->name(), QString("OCRA"));

    ctxt->setContextProperty("font", "http://localhost:14448/daniel.ttf");
    QTRY_VERIFY(fontObject->status() == QDeclarativeFontLoader::Loading);
    QTRY_VERIFY(fontObject->status() == QDeclarativeFontLoader::Ready);
    QCOMPARE(nameSpy.count(), 1);
    QCOMPARE(statusSpy.count(), 2);
    QTRY_COMPARE(fontObject->name(), QString("Daniel"));

    ctxt->setContextProperty("font", fontUrl);
    QTRY_VERIFY(fontObject->status() == QDeclarativeFontLoader::Ready);
    QCOMPARE(nameSpy.count(), 2);
    QCOMPARE(statusSpy.count(), 2);
    QTRY_COMPARE(fontObject->name(), QString("OCRA"));

    ctxt->setContextProperty("font", "http://localhost:14448/daniel.ttf");
    QTRY_VERIFY(fontObject->status() == QDeclarativeFontLoader::Ready);
    QCOMPARE(nameSpy.count(), 3);
    QCOMPARE(statusSpy.count(), 2);
    QTRY_COMPARE(fontObject->name(), QString("Daniel"));
}

QTEST_MAIN(tst_qdeclarativefontloader)

#include "tst_qdeclarativefontloader.moc"
