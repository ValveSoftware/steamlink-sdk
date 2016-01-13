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
#include <private/qdeclarativeengine_p.h>

#include <qtest.h>
#include <qdeclarativedatatest.h>
#include <QDebug>
#include <QDeclarativeEngine>
#include <QFontDatabase>
#include <QFileInfo>
#include <QDeclarativeComponent>
#include <QDesktopServices>
#include <QDir>
#include <QVector3D>
#include <QCryptographicHash>
#include <QDeclarativeItem>
#include <QSignalSpy>

class tst_qdeclarativeqt : public QDeclarativeDataTest
{
    Q_OBJECT
public:
    tst_qdeclarativeqt() {}

private slots:
    void enums();
    void rgba();
    void hsla();
    void rect();
    void point();
    void size();
    void vector();
    void lighter();
    void darker();
    void tint();
    void openUrlExternally();
    void openUrlExternally_pragmaLibrary();
    void md5();
    void createComponent();
    void createComponent_pragmaLibrary();
    void createQmlObject();
    void consoleLog();
    void dateTimeFormatting();
    void dateTimeFormatting_data();
    void isQtObject();
    void btoa();
    void atob();
    void fontFamilies();
    void quit();

private:
    QDeclarativeEngine engine;
};

void tst_qdeclarativeqt::enums()
{
    QDeclarativeComponent component(&engine, testFileUrl("enums.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test1").toInt(), (int)Qt::Key_Escape);
    QCOMPARE(object->property("test2").toInt(), (int)Qt::DescendingOrder);
    QCOMPARE(object->property("test3").toInt(), (int)Qt::ElideMiddle);
    QCOMPARE(object->property("test4").toInt(), (int)Qt::AlignRight);

    delete object;
}

void tst_qdeclarativeqt::rgba()
{
    QDeclarativeComponent component(&engine, testFileUrl("rgba.qml"));

    QString warning1 = component.url().toString() + ":6: Error: Qt.rgba(): Invalid arguments";
    QString warning2 = component.url().toString() + ":7: Error: Qt.rgba(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);


    QCOMPARE(qvariant_cast<QColor>(object->property("test1")), QColor::fromRgbF(1, 0, 0, 0.8));
    QCOMPARE(qvariant_cast<QColor>(object->property("test2")), QColor::fromRgbF(1, 0.5, 0.3, 1));
    QCOMPARE(qvariant_cast<QColor>(object->property("test3")), QColor());
    QCOMPARE(qvariant_cast<QColor>(object->property("test4")), QColor());
    QCOMPARE(qvariant_cast<QColor>(object->property("test5")), QColor::fromRgbF(1, 1, 1, 1));
    QCOMPARE(qvariant_cast<QColor>(object->property("test6")), QColor::fromRgbF(0, 0, 0, 0));

    delete object;
}

void tst_qdeclarativeqt::hsla()
{
    QDeclarativeComponent component(&engine, testFileUrl("hsla.qml"));

    QString warning1 = component.url().toString() + ":6: Error: Qt.hsla(): Invalid arguments";
    QString warning2 = component.url().toString() + ":7: Error: Qt.hsla(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(qvariant_cast<QColor>(object->property("test1")), QColor::fromHslF(1, 0, 0, 0.8));
    QCOMPARE(qvariant_cast<QColor>(object->property("test2")), QColor::fromHslF(1, 0.5, 0.3, 1));
    QCOMPARE(qvariant_cast<QColor>(object->property("test3")), QColor());
    QCOMPARE(qvariant_cast<QColor>(object->property("test4")), QColor());
    QCOMPARE(qvariant_cast<QColor>(object->property("test5")), QColor::fromHslF(1, 1, 1, 1));
    QCOMPARE(qvariant_cast<QColor>(object->property("test6")), QColor::fromHslF(0, 0, 0, 0));

    delete object;
}

void tst_qdeclarativeqt::rect()
{
    QDeclarativeComponent component(&engine, testFileUrl("rect.qml"));

    QString warning1 = component.url().toString() + ":6: Error: Qt.rect(): Invalid arguments";
    QString warning2 = component.url().toString() + ":7: Error: Qt.rect(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(qvariant_cast<QRectF>(object->property("test1")), QRectF(10, 13, 100, 109));
    QCOMPARE(qvariant_cast<QRectF>(object->property("test2")), QRectF(-10, 13, 100, 109.6));
    QCOMPARE(qvariant_cast<QRectF>(object->property("test3")), QRectF());
    QCOMPARE(qvariant_cast<QRectF>(object->property("test4")), QRectF());
    QCOMPARE(qvariant_cast<QRectF>(object->property("test5")), QRectF(10, 13, 100, -109));

    delete object;
}

void tst_qdeclarativeqt::point()
{
    QDeclarativeComponent component(&engine, testFileUrl("point.qml"));

    QString warning1 = component.url().toString() + ":6: Error: Qt.point(): Invalid arguments";
    QString warning2 = component.url().toString() + ":7: Error: Qt.point(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(qvariant_cast<QPointF>(object->property("test1")), QPointF(19, 34));
    QCOMPARE(qvariant_cast<QPointF>(object->property("test2")), QPointF(-3, 109.2));
    QCOMPARE(qvariant_cast<QPointF>(object->property("test3")), QPointF());
    QCOMPARE(qvariant_cast<QPointF>(object->property("test4")), QPointF());

    delete object;
}

void tst_qdeclarativeqt::size()
{
    QDeclarativeComponent component(&engine, testFileUrl("size.qml"));

    QString warning1 = component.url().toString() + ":7: Error: Qt.size(): Invalid arguments";
    QString warning2 = component.url().toString() + ":8: Error: Qt.size(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(qvariant_cast<QSizeF>(object->property("test1")), QSizeF(19, 34));
    QCOMPARE(qvariant_cast<QSizeF>(object->property("test2")), QSizeF(3, 109.2));
    QCOMPARE(qvariant_cast<QSizeF>(object->property("test3")), QSizeF(-3, 10));
    QCOMPARE(qvariant_cast<QSizeF>(object->property("test4")), QSizeF());
    QCOMPARE(qvariant_cast<QSizeF>(object->property("test5")), QSizeF());

    delete object;
}

void tst_qdeclarativeqt::vector()
{
    QDeclarativeComponent component(&engine, testFileUrl("vector.qml"));

    QString warning1 = component.url().toString() + ":6: Error: Qt.vector(): Invalid arguments";
    QString warning2 = component.url().toString() + ":7: Error: Qt.vector(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(qvariant_cast<QVector3D>(object->property("test1")), QVector3D(1, 0, 0.9f));
    QCOMPARE(qvariant_cast<QVector3D>(object->property("test2")), QVector3D(102, -10, -982.1f));
    QCOMPARE(qvariant_cast<QVector3D>(object->property("test3")), QVector3D());
    QCOMPARE(qvariant_cast<QVector3D>(object->property("test4")), QVector3D());

    delete object;
}

void tst_qdeclarativeqt::lighter()
{
    QDeclarativeComponent component(&engine, testFileUrl("lighter.qml"));

    QString warning1 = component.url().toString() + ":5: Error: Qt.lighter(): Invalid arguments";
    QString warning2 = component.url().toString() + ":10: Error: Qt.lighter(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(qvariant_cast<QColor>(object->property("test1")), QColor::fromRgbF(1, 0.8, 0.3).lighter());
    QCOMPARE(qvariant_cast<QColor>(object->property("test2")), QColor());
    QCOMPARE(qvariant_cast<QColor>(object->property("test3")), QColor::fromRgbF(1, 0.8, 0.3).lighter(180));
    QCOMPARE(qvariant_cast<QColor>(object->property("test4")), QColor("red").lighter());
    QCOMPARE(qvariant_cast<QColor>(object->property("test5")), QColor());
    QCOMPARE(qvariant_cast<QColor>(object->property("test6")), QColor());

    delete object;
}

void tst_qdeclarativeqt::darker()
{
    QDeclarativeComponent component(&engine, testFileUrl("darker.qml"));

    QString warning1 = component.url().toString() + ":5: Error: Qt.darker(): Invalid arguments";
    QString warning2 = component.url().toString() + ":10: Error: Qt.darker(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(qvariant_cast<QColor>(object->property("test1")), QColor::fromRgbF(1, 0.8, 0.3).darker());
    QCOMPARE(qvariant_cast<QColor>(object->property("test2")), QColor());
    QCOMPARE(qvariant_cast<QColor>(object->property("test3")), QColor::fromRgbF(1, 0.8, 0.3).darker(280));
    QCOMPARE(qvariant_cast<QColor>(object->property("test4")), QColor("red").darker());
    QCOMPARE(qvariant_cast<QColor>(object->property("test5")), QColor());
    QCOMPARE(qvariant_cast<QColor>(object->property("test6")), QColor());

    delete object;
}

void tst_qdeclarativeqt::tint()
{
    QDeclarativeComponent component(&engine, testFileUrl("tint.qml"));

    QString warning1 = component.url().toString() + ":7: Error: Qt.tint(): Invalid arguments";
    QString warning2 = component.url().toString() + ":8: Error: Qt.tint(): Invalid arguments";

    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(qvariant_cast<QColor>(object->property("test1")), QColor::fromRgbF(0, 0, 1));
    QCOMPARE(qvariant_cast<QColor>(object->property("test2")), QColor::fromRgbF(1, 0, 0));
    QColor test3 = qvariant_cast<QColor>(object->property("test3"));
    QCOMPARE(test3.rgba(), 0xFF7F0080);
    QCOMPARE(qvariant_cast<QColor>(object->property("test4")), QColor());
    QCOMPARE(qvariant_cast<QColor>(object->property("test5")), QColor());

    delete object;
}

class MyUrlHandler : public QObject
{
    Q_OBJECT
public:
    MyUrlHandler() : called(0) { }
    int called;
    QUrl last;

public slots:
    void noteCall(const QUrl &url) { called++; last = url; }
};

void tst_qdeclarativeqt::openUrlExternally()
{
    MyUrlHandler handler;

    QDesktopServices::setUrlHandler("test", &handler, "noteCall");
    QDesktopServices::setUrlHandler("file", &handler, "noteCall");

    QDeclarativeComponent component(&engine, testFileUrl("openUrlExternally.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(handler.called,1);
    QCOMPARE(handler.last, QUrl("test:url"));

    object->setProperty("testFile", true);

    QCOMPARE(handler.called,2);
    QCOMPARE(handler.last, testFileUrl("test.html"));

    QDesktopServices::unsetUrlHandler("test");
    QDesktopServices::unsetUrlHandler("file");
}

void tst_qdeclarativeqt::openUrlExternally_pragmaLibrary()
{
    MyUrlHandler handler;

    QDesktopServices::setUrlHandler("test", &handler, "noteCall");
    QDesktopServices::setUrlHandler("file", &handler, "noteCall");

    QDeclarativeComponent component(&engine, testFileUrl("openUrlExternally_lib.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(handler.called,1);
    QCOMPARE(handler.last, QUrl("test:url"));

    object->setProperty("testFile", true);

    QCOMPARE(handler.called,2);
    QCOMPARE(handler.last, testFileUrl("test.html"));

    QDesktopServices::unsetUrlHandler("test");
    QDesktopServices::unsetUrlHandler("file");
}

void tst_qdeclarativeqt::md5()
{
    QDeclarativeComponent component(&engine, testFileUrl("md5.qml"));

    QString warning1 = component.url().toString() + ":4: Error: Qt.md5(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test2").toString(), QLatin1String(QCryptographicHash::hash("Hello World", QCryptographicHash::Md5).toHex()));

    delete object;
}

void tst_qdeclarativeqt::createComponent()
{
    QDeclarativeComponent component(&engine, testFileUrl("createComponent.qml"));

    QString warning1 = component.url().toString() + ":9: Error: Qt.createComponent(): Invalid arguments";
    QString warning2 = component.url().toString() + ":10: Error: Qt.createComponent(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("absoluteUrl").toString(), QString("http://www.example.com/test.qml"));
    QCOMPARE(object->property("relativeUrl").toString(), testFileUrl("createComponentData.qml").toString());

    delete object;
}

void tst_qdeclarativeqt::createComponent_pragmaLibrary()
{
    // Currently, just loading createComponent_lib.qml causes crash on some platforms
    QDeclarativeComponent component(&engine, testFileUrl("createComponent_lib.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("status").toInt(), int(QDeclarativeComponent::Ready));
    QCOMPARE(object->property("readValue").toInt(), int(1913));
    delete object;
}

void tst_qdeclarativeqt::createQmlObject()
{
    QDeclarativeComponent component(&engine, testFileUrl("createQmlObject.qml"));

    QString warning1 = component.url().toString() + ":7: Error: Qt.createQmlObject(): Invalid arguments";
    QString warning2 = component.url().toString()+ ":10: Error: Qt.createQmlObject() failed to create object:     " + testFileUrl("inline").toString() + ":2:10: Blah is not a type\n";
    QString warning3 = component.url().toString()+ ":11: Error: Qt.createQmlObject() failed to create object:     " + testFileUrl("main.qml").toString() + ":4:1: Duplicate property name\n";
    QString warning4 = component.url().toString()+ ":9: Error: Qt.createQmlObject(): Missing parent object";
    QString warning5 = component.url().toString()+ ":8: Error: Qt.createQmlObject(): Invalid arguments";
    QString warning6 = "RunTimeError:  Qt.createQmlObject() failed to create object:     " + testFileUrl("inline").toString() + ":3: Cannot assign object type QObject with no default method\n";

    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning3));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning4));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning5));
    QTest::ignoreMessage(QtDebugMsg, qPrintable(warning6));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("emptyArg").toBool(), true);
    QCOMPARE(object->property("success").toBool(), true);

    QDeclarativeItem *item = qobject_cast<QDeclarativeItem *>(object);
    QVERIFY(item != 0);
    QVERIFY(item->childItems().count() == 1);

    delete object;
}

void tst_qdeclarativeqt::consoleLog()
{
    QTest::ignoreMessage(QtDebugMsg, "completed ok");
    QTest::ignoreMessage(QtDebugMsg, "completed ok");
    QDeclarativeComponent component(&engine, testFileUrl("consoleLog.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);
    delete object;
}

void tst_qdeclarativeqt::dateTimeFormatting()
{
    QFETCH(QString, method);
    QFETCH(QStringList, inputProperties);
    QFETCH(QStringList, expectedResults);

    QDate date(2008,12,24);
    QTime time(14,15,38,200);
    QDateTime dateTime(date, time);

    QDeclarativeEngine eng;

    eng.rootContext()->setContextProperty("qdate", date);
    eng.rootContext()->setContextProperty("qtime", time);
    eng.rootContext()->setContextProperty("qdatetime", dateTime);

    QDeclarativeComponent component(&eng, testFileUrl("formatting.qml"));

    QStringList warnings;
    warnings << component.url().toString() + ":37: Error: Qt.formatDate(): Invalid date format"
        << component.url().toString() + ":36: Error: Qt.formatDate(): Invalid arguments"
        << component.url().toString() + ":40: Error: Qt.formatTime(): Invalid time format"
        << component.url().toString() + ":39: Error: Qt.formatTime(): Invalid arguments"
        << component.url().toString() + ":43: Error: Qt.formatDateTime(): Invalid datetime format"
        << component.url().toString() + ":42: Error: Qt.formatDateTime(): Invalid arguments";

    foreach (const QString &warning, warnings)
        QTest::ignoreMessage(QtWarningMsg, qPrintable(warning));

    QObject *object = component.create();
    QVERIFY2(component.errorString().isEmpty(), qPrintable(component.errorString()));
    QVERIFY(object != 0);

    QVERIFY(inputProperties.count() > 0);

    QVariant result;
    foreach(const QString &prop, inputProperties) {
        QVERIFY(QMetaObject::invokeMethod(object, method.toUtf8().constData(),
                Q_RETURN_ARG(QVariant, result),
                Q_ARG(QVariant, prop)));

        QStringList output = result.toStringList();
        for (int i=0; i<output.count(); i++)
            QCOMPARE(output[i], expectedResults[i]);
    }

    delete object;
}

void tst_qdeclarativeqt::dateTimeFormatting_data()
{
    QTest::addColumn<QString>("method");
    QTest::addColumn<QStringList>("inputProperties");
    QTest::addColumn<QStringList>("expectedResults");

    QDate date(2008,12,24);
    QTime time(14,15,38,200);
    QDateTime dateTime(date, time);

    QTest::newRow("formatDate")
        << "formatDate"
        << (QStringList() << "dateFromString" << "jsdate" << "qdate" << "qdatetime")
        << (QStringList() << date.toString(Qt::DefaultLocaleShortDate)
                          << date.toString(Qt::DefaultLocaleLongDate)
                          << date.toString("ddd MMMM d yy"));

    QTest::newRow("formatTime")
        << "formatTime"
        << (QStringList() << "jsdate" << "qtime" << "qdatetime")
        << (QStringList() << time.toString(Qt::DefaultLocaleShortDate)
                          << time.toString(Qt::DefaultLocaleLongDate)
                          << time.toString("H:m:s a")
                          << time.toString("hh:mm:ss.zzz"));

    QTest::newRow("formatDateTime")
        << "formatDateTime"
        << (QStringList() << "jsdate" << "qdatetime")
        << (QStringList() << dateTime.toString(Qt::DefaultLocaleShortDate)
                          << dateTime.toString(Qt::DefaultLocaleLongDate)
                          << dateTime.toString("M/d/yy H:m:s a"));
}

void tst_qdeclarativeqt::isQtObject()
{
    QDeclarativeComponent component(&engine, testFileUrl("isQtObject.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test1").toBool(), true);
    QCOMPARE(object->property("test2").toBool(), false);
    QCOMPARE(object->property("test3").toBool(), false);
    QCOMPARE(object->property("test4").toBool(), false);
    QCOMPARE(object->property("test5").toBool(), false);

    delete object;
}

void tst_qdeclarativeqt::btoa()
{
    QDeclarativeComponent component(&engine, testFileUrl("btoa.qml"));

    QString warning1 = component.url().toString() + ":4: Error: Qt.btoa(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test2").toString(), QString("SGVsbG8gd29ybGQh"));

    delete object;
}

void tst_qdeclarativeqt::atob()
{
    QDeclarativeComponent component(&engine, testFileUrl("atob.qml"));

    QString warning1 = component.url().toString() + ":4: Error: Qt.atob(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test2").toString(), QString("Hello world!"));

    delete object;
}

void tst_qdeclarativeqt::fontFamilies()
{
    QDeclarativeComponent component(&engine, testFileUrl("fontFamilies.qml"));

    QString warning1 = component.url().toString() + ":4: Error: Qt.fontFamilies(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QFontDatabase database;
    QCOMPARE(object->property("test2"), QVariant::fromValue(database.families()));

    delete object;
}

void tst_qdeclarativeqt::quit()
{
    QDeclarativeComponent component(&engine, testFileUrl("quit.qml"));

    QSignalSpy spy(&engine, SIGNAL(quit()));
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(spy.count(), 1);

    delete object;
}

QTEST_MAIN(tst_qdeclarativeqt)

#include "tst_qdeclarativeqt.moc"
