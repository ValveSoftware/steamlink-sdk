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
#include <private/qqmlengine_p.h>

#include <qtest.h>
#include <QDebug>
#include <QQmlEngine>
#include <QFontDatabase>
#include <QFileInfo>
#include <QQmlComponent>
#include <QDesktopServices>
#include <QDir>
#include <QCryptographicHash>
#include <QtQuick/QQuickItem>
#include <QSignalSpy>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QQuaternion>
#include <QMatrix4x4>
#include <QFont>
#include "../../shared/util.h"

// Copied from tst_qdatetime.cpp
#ifdef Q_OS_WIN
# include <qt_windows.h>
# include <time.h>
#  if defined(Q_OS_WINRT)
#    define tzset()
#  endif
#endif

class tst_qqmlqt : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlqt() {}

private slots:
    void initTestCase();
    void enums();
    void rgba();
    void hsla();
    void hsva();
    void colorEqual();
    void rect();
    void point();
    void size();
    void vector2d();
    void vector3d();
    void vector4d();
    void quaternion();
    void matrix4x4();
    void font();
    void lighter();
    void darker();
    void tint();
    void openUrlExternally();
    void openUrlExternally_pragmaLibrary();
    void md5();
    void createComponent();
    void createComponent_pragmaLibrary();
    void createQmlObject();
    void dateTimeConversion();
    void dateTimeFormatting();
    void dateTimeFormatting_data();
    void dateTimeFormattingVariants();
    void dateTimeFormattingVariants_data();
    void isQtObject();
    void btoa();
    void atob();
    void fontFamilies();
    void quit();
    void exit();
    void resolvedUrl();
    void later_data();
    void later();
    void qtObjectContents();

    void timeRoundtrip_data();
    void timeRoundtrip();

private:
    QQmlEngine engine;
};

// for callLater()
class TestElement : public QQuickItem
{
    Q_OBJECT
public:
    TestElement() : m_intptr(new int) {}
    ~TestElement() { delete m_intptr; }

    Q_INVOKABLE void dangerousFunction() {
        delete m_intptr;
        m_intptr = new int;
        *m_intptr = 5;
    }
private:
    int *m_intptr;
};

// for callLater()
class TestModuleApi : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int intProp READ intProp WRITE setIntProp NOTIFY intPropChanged)

public:
    TestModuleApi() : m_int(0) {}
    ~TestModuleApi() {}

    int intProp() const { return m_int; }
    void setIntProp(int v) { m_int = v; emit intPropChanged(); }

    Q_INVOKABLE void testFunc() { ++m_int; emit intPropChanged(); }
    Q_INVOKABLE void resetIntProp() { m_int = 0; emit intPropChanged(); }

signals:
    void intPropChanged();

private:
    int m_int;
};

static QObject *test_module_api_factory(QQmlEngine *engine, QJSEngine *scriptEngine)
{
   Q_UNUSED(engine)
   Q_UNUSED(scriptEngine)
   TestModuleApi *api = new TestModuleApi;
   return api;
}

void tst_qqmlqt::initTestCase()
{
    QQmlDataTest::initTestCase();
    qmlRegisterSingletonType<TestModuleApi>("LaterImports", 1, 0, "SingletonType", test_module_api_factory);
    qmlRegisterType<TestElement>("LaterImports", 1, 0, "TestElement");
}

void tst_qqmlqt::enums()
{
    QQmlComponent component(&engine, testFileUrl("enums.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test1").toInt(), (int)Qt::Key_Escape);
    QCOMPARE(object->property("test2").toInt(), (int)Qt::DescendingOrder);
    QCOMPARE(object->property("test3").toInt(), (int)Qt::ElideMiddle);
    QCOMPARE(object->property("test4").toInt(), (int)Qt::AlignRight);

    delete object;
}

void tst_qqmlqt::rgba()
{
    QQmlComponent component(&engine, testFileUrl("rgba.qml"));

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

void tst_qqmlqt::hsla()
{
    QQmlComponent component(&engine, testFileUrl("hsla.qml"));

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

void tst_qqmlqt::hsva()
{
    QQmlComponent component(&engine, testFileUrl("hsva.qml"));

    QString warning1 = component.url().toString() + ":6: Error: Qt.hsva(): Invalid arguments";
    QString warning2 = component.url().toString() + ":7: Error: Qt.hsva(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(qvariant_cast<QColor>(object->property("test1")), QColor::fromHsvF(1, 0, 0, 0.8));
    QCOMPARE(qvariant_cast<QColor>(object->property("test2")), QColor::fromHsvF(1, 0.5, 0.3, 1));
    QCOMPARE(qvariant_cast<QColor>(object->property("test3")), QColor());
    QCOMPARE(qvariant_cast<QColor>(object->property("test4")), QColor());
    QCOMPARE(qvariant_cast<QColor>(object->property("test5")), QColor::fromHsvF(1, 1, 1, 1));
    QCOMPARE(qvariant_cast<QColor>(object->property("test6")), QColor::fromHsvF(0, 0, 0, 0));

    delete object;
}

void tst_qqmlqt::colorEqual()
{
    QQmlComponent component(&engine, testFileUrl("colorEqual.qml"));

    QTest::ignoreMessage(QtWarningMsg, qPrintable(component.url().toString() + ":6: Error: Qt.colorEqual(): Invalid arguments"));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(component.url().toString() + ":7: Error: Qt.colorEqual(): Invalid arguments"));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(component.url().toString() + ":9: Error: Qt.colorEqual(): Invalid color name"));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(component.url().toString() + ":10: Error: Qt.colorEqual(): Invalid color name"));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(component.url().toString() + ":12: Error: Qt.colorEqual(): Invalid arguments"));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(component.url().toString() + ":13: Error: Qt.colorEqual(): Invalid arguments"));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(component.url().toString() + ":17: Error: Qt.colorEqual(): Invalid arguments"));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(component.url().toString() + ":18: Error: Qt.colorEqual(): Invalid arguments"));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(component.url().toString() + ":34: Error: Qt.colorEqual(): Invalid color name"));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(component.url().toString() + ":35: Error: Qt.colorEqual(): Invalid color name"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test1a").toBool(), false);
    QCOMPARE(object->property("test1b").toBool(), false);
    QCOMPARE(object->property("test1c").toBool(), false);
    QCOMPARE(object->property("test1d").toBool(), false);
    QCOMPARE(object->property("test1e").toBool(), false);
    QCOMPARE(object->property("test1f").toBool(), false);
    QCOMPARE(object->property("test1g").toBool(), false);
    QCOMPARE(object->property("test1h").toBool(), false);

    QCOMPARE(object->property("test2a").toBool(), true);
    QCOMPARE(object->property("test2b").toBool(), true);
    QCOMPARE(object->property("test2c").toBool(), true);
    QCOMPARE(object->property("test2d").toBool(), true);
    QCOMPARE(object->property("test2e").toBool(), true);
    QCOMPARE(object->property("test2f").toBool(), true);
    QCOMPARE(object->property("test2g").toBool(), true);
    QCOMPARE(object->property("test2h").toBool(), true);
    QCOMPARE(object->property("test2i").toBool(), false);
    QCOMPARE(object->property("test2j").toBool(), false);
    QCOMPARE(object->property("test2k").toBool(), false);
    QCOMPARE(object->property("test2l").toBool(), false);
    QCOMPARE(object->property("test2m").toBool(), false);
    QCOMPARE(object->property("test2n").toBool(), false);

    QCOMPARE(object->property("test3a").toBool(), true);
    QCOMPARE(object->property("test3b").toBool(), true);
    QCOMPARE(object->property("test3c").toBool(), true);
    QCOMPARE(object->property("test3d").toBool(), true);
    QCOMPARE(object->property("test3e").toBool(), true);
    QCOMPARE(object->property("test3f").toBool(), true);
    QCOMPARE(object->property("test3g").toBool(), false);
    QCOMPARE(object->property("test3h").toBool(), false);
    QCOMPARE(object->property("test3i").toBool(), true);
    QCOMPARE(object->property("test3j").toBool(), true);
    QCOMPARE(object->property("test3k").toBool(), true);
    QCOMPARE(object->property("test3l").toBool(), true);
    QCOMPARE(object->property("test3m").toBool(), true);
    QCOMPARE(object->property("test3n").toBool(), true);

    QCOMPARE(object->property("test4a").toBool(), true);
    QCOMPARE(object->property("test4b").toBool(), true);
    QCOMPARE(object->property("test4c").toBool(), false);
    QCOMPARE(object->property("test4d").toBool(), false);
    QCOMPARE(object->property("test4e").toBool(), false);
    QCOMPARE(object->property("test4f").toBool(), false);
    QCOMPARE(object->property("test4g").toBool(), false);
    QCOMPARE(object->property("test4h").toBool(), false);
    QCOMPARE(object->property("test4i").toBool(), false);
    QCOMPARE(object->property("test4j").toBool(), false);

    QCOMPARE(object->property("test5a").toBool(), true);
    QCOMPARE(object->property("test5b").toBool(), true);
    QCOMPARE(object->property("test5c").toBool(), true);
    QCOMPARE(object->property("test5d").toBool(), false);
    QCOMPARE(object->property("test5e").toBool(), false);

    QCOMPARE(object->property("test6a").toBool(), true);
    QCOMPARE(object->property("test6b").toBool(), true);
    QCOMPARE(object->property("test6c").toBool(), true);
    QCOMPARE(object->property("test6d").toBool(), false);
    QCOMPARE(object->property("test6e").toBool(), false);

    delete object;
}

void tst_qqmlqt::rect()
{
    QQmlComponent component(&engine, testFileUrl("rect.qml"));

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

void tst_qqmlqt::point()
{
    QQmlComponent component(&engine, testFileUrl("point.qml"));

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

void tst_qqmlqt::size()
{
    QQmlComponent component(&engine, testFileUrl("size.qml"));

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

void tst_qqmlqt::vector2d()
{
    QQmlComponent component(&engine, testFileUrl("vector2.qml"));

    QString warning1 = component.url().toString() + ":6: Error: Qt.vector2d(): Invalid arguments";
    QString warning2 = component.url().toString() + ":7: Error: Qt.vector2d(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(qvariant_cast<QVector2D>(object->property("test1")), QVector2D(1, 0.9f));
    QCOMPARE(qvariant_cast<QVector2D>(object->property("test2")), QVector2D(102, -982.1f));
    QCOMPARE(qvariant_cast<QVector2D>(object->property("test3")), QVector2D());
    QCOMPARE(qvariant_cast<QVector2D>(object->property("test4")), QVector2D());

    delete object;
}

void tst_qqmlqt::vector3d()
{
    QQmlComponent component(&engine, testFileUrl("vector.qml"));

    QString warning1 = component.url().toString() + ":6: Error: Qt.vector3d(): Invalid arguments";
    QString warning2 = component.url().toString() + ":7: Error: Qt.vector3d(): Invalid arguments";
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

void tst_qqmlqt::vector4d()
{
    QQmlComponent component(&engine, testFileUrl("vector4.qml"));

    QString warning1 = component.url().toString() + ":6: Error: Qt.vector4d(): Invalid arguments";
    QString warning2 = component.url().toString() + ":7: Error: Qt.vector4d(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(qvariant_cast<QVector4D>(object->property("test1")), QVector4D(1, 0, 0.9f, 0.6f));
    QCOMPARE(qvariant_cast<QVector4D>(object->property("test2")), QVector4D(102, -10, -982.1f, 10));
    QCOMPARE(qvariant_cast<QVector4D>(object->property("test3")), QVector4D());
    QCOMPARE(qvariant_cast<QVector4D>(object->property("test4")), QVector4D());

    delete object;
}

void tst_qqmlqt::quaternion()
{
    QQmlComponent component(&engine, testFileUrl("quaternion.qml"));

    QString warning1 = component.url().toString() + ":6: Error: Qt.quaternion(): Invalid arguments";
    QString warning2 = component.url().toString() + ":7: Error: Qt.quaternion(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(qvariant_cast<QQuaternion>(object->property("test1")), QQuaternion(2, 17, 0.9f, 0.6f));
    QCOMPARE(qvariant_cast<QQuaternion>(object->property("test2")), QQuaternion(102, -10, -982.1f, 10));
    QCOMPARE(qvariant_cast<QQuaternion>(object->property("test3")), QQuaternion());
    QCOMPARE(qvariant_cast<QQuaternion>(object->property("test4")), QQuaternion());

    delete object;
}

void tst_qqmlqt::matrix4x4()
{
    QQmlComponent component(&engine, testFileUrl("matrix4x4.qml"));

    QString warning1 = component.url().toString() + ":6: Error: Qt.matrix4x4(): Invalid arguments";
    QString warning2 = component.url().toString() + ":7: Error: Qt.matrix4x4(): Invalid argument: not a valid matrix4x4 values array";
    QString warning3 = component.url().toString() + ":8: Error: Qt.matrix4x4(): Invalid argument: not a valid matrix4x4 values array";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning3));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(qvariant_cast<QMatrix4x4>(object->property("test1")), QMatrix4x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
    QCOMPARE(qvariant_cast<QMatrix4x4>(object->property("test2")), QMatrix4x4(1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4));
    QCOMPARE(qvariant_cast<QMatrix4x4>(object->property("test3")), QMatrix4x4());
    QCOMPARE(qvariant_cast<QMatrix4x4>(object->property("test4")), QMatrix4x4());
    QCOMPARE(qvariant_cast<QMatrix4x4>(object->property("test5")), QMatrix4x4());

    delete object;
}

void tst_qqmlqt::font()
{
    QQmlComponent component(&engine, testFileUrl("font.qml"));

    QString warning1 = component.url().toString() + ":6: Error: Qt.font(): Invalid arguments";
    QString warning2 = component.url().toString() + ":7: Error: Qt.font(): Invalid argument: no valid font subproperties specified";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(qvariant_cast<QFont>(object->property("test1")), QFont("Arial", 22));
    QCOMPARE(qvariant_cast<QFont>(object->property("test2")), QFont("Arial", 20, QFont::DemiBold, true));
    QCOMPARE(qvariant_cast<QFont>(object->property("test3")), QFont());
    QCOMPARE(qvariant_cast<QFont>(object->property("test4")), QFont());

    delete object;
}

void tst_qqmlqt::lighter()
{
    QQmlComponent component(&engine, testFileUrl("lighter.qml"));

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

void tst_qqmlqt::darker()
{
    QQmlComponent component(&engine, testFileUrl("darker.qml"));

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

void tst_qqmlqt::tint()
{
    QQmlComponent component(&engine, testFileUrl("tint.qml"));

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

void tst_qqmlqt::openUrlExternally()
{
    MyUrlHandler handler;

    QDesktopServices::setUrlHandler("test", &handler, "noteCall");
    QDesktopServices::setUrlHandler("file", &handler, "noteCall");

    QQmlComponent component(&engine, testFileUrl("openUrlExternally.qml"));
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

void tst_qqmlqt::openUrlExternally_pragmaLibrary()
{
    MyUrlHandler handler;

    QDesktopServices::setUrlHandler("test", &handler, "noteCall");
    QDesktopServices::setUrlHandler("file", &handler, "noteCall");

    QQmlComponent component(&engine, testFileUrl("openUrlExternally_lib.qml"));
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

void tst_qqmlqt::md5()
{
    QQmlComponent component(&engine, testFileUrl("md5.qml"));

    QString warning1 = component.url().toString() + ":4: Error: Qt.md5(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test2").toString(), QLatin1String(QCryptographicHash::hash("Hello World", QCryptographicHash::Md5).toHex()));

    delete object;
}

void tst_qqmlqt::createComponent()
{
    {
    QQmlComponent component(&engine, testFileUrl("createComponent.qml"));

    QString warning1 = component.url().toString() + ":9: Error: Qt.createComponent(): Invalid arguments";
    QString warning2 = component.url().toString() + ":10: Error: Qt.createComponent(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("absoluteUrl").toString(), QString("http://www.example.com/test.qml"));
    QCOMPARE(object->property("relativeUrl").toString(), testFileUrl("createComponentData.qml").toString());

    QTRY_VERIFY(object->property("asyncResult").toBool());

    delete object;
    }

    // simultaneous sync and async compilation
    {
    QQmlComponent component(&engine, testFileUrl("createComponent.2.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);
    QTRY_VERIFY(object->property("success").toBool());
    delete object;
    }
}

void tst_qqmlqt::createComponent_pragmaLibrary()
{
    // Currently, just loading createComponent_lib.qml causes crash on some platforms
    QQmlComponent component(&engine, testFileUrl("createComponent_lib.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("status").toInt(), int(QQmlComponent::Ready));
    QCOMPARE(object->property("readValue").toInt(), int(1913));
    delete object;
}

void tst_qqmlqt::createQmlObject()
{
    QQmlComponent component(&engine, testFileUrl("createQmlObject.qml"));

    QString warning1 = component.url().toString() + ":7: Error: Qt.createQmlObject(): Invalid arguments";
    QString warning2 = component.url().toString()+ ":10: Error: Qt.createQmlObject(): failed to create object: \n    " + testFileUrl("inline").toString() + ":2:10: Blah is not a type";
    QString warning3 = component.url().toString()+ ":11: Error: Qt.createQmlObject(): failed to create object: \n    " + testFileUrl("main.qml").toString() + ":4:14: Duplicate property name";
    QString warning4 = component.url().toString()+ ":9: Error: Qt.createQmlObject(): Missing parent object";
    QString warning5 = component.url().toString()+ ":8: Error: Qt.createQmlObject(): Invalid arguments";
    QString warning6 = "RunTimeError:  Qt.createQmlObject(): failed to create object: \n    " + testFileUrl("inline").toString() + ":3:16: Cannot assign object type QObject with no default method";
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

    QQuickItem *item = qobject_cast<QQuickItem *>(object);
    QVERIFY(item != 0);
    QCOMPARE(item->childItems().count(), 1);

    delete object;
}


void tst_qqmlqt::dateTimeConversion()
{
    QDate date(2008,12,24);
    QTime time(14,15,38,200);
    QDateTime dateTime(date, time);
    //Note that when converting Date to QDateTime they can argue over historical DST data when converting to local time.
    //Tests should use UTC or recent dates.
    QDateTime dateTime2(QDate(2852,12,31), QTime(23,59,59,500));
    QDateTime dateTime3(QDate(2000,1,1), QTime(0,0,0,0));
    QDateTime dateTime4(QDate(2001,2,2), QTime(0,0,0,0));
    QDateTime dateTime5(QDate(1999,1,1), QTime(2,3,4,0));
    QDateTime dateTime6(QDate(2008,2,24), QTime(14,15,38,200));
    QDateTime dateTime7(QDate(1970,1,1), QTime(0,0,0,0), Qt::UTC);
    QDateTime dateTime8(QDate(1586,2,2), QTime(0,0,0,0), Qt::UTC);
    QDateTime dateTime9(QDate(955,1,1), QTime(0,0,0,0), Qt::UTC);
    QDateTime dateTime10(QDate(113,2,24), QTime(14,15,38,200), Qt::UTC);

    QQmlEngine eng;
    QQmlComponent component(&eng, testFileUrl("dateTimeConversion.qml"));
    QObject *obj = component.create();

    QCOMPARE(obj->property("qdate").toDate(), date);
    QCOMPARE(obj->property("qtime").toTime(), time);
    QCOMPARE(obj->property("qdatetime").toDateTime(), dateTime);
    QCOMPARE(obj->property("qdatetime2").toDateTime(), dateTime2);
    QCOMPARE(obj->property("qdatetime3").toDateTime(), dateTime3);
    QCOMPARE(obj->property("qdatetime4").toDateTime(), dateTime4);
    QCOMPARE(obj->property("qdatetime5").toDateTime(), dateTime5);
    QCOMPARE(obj->property("qdatetime6").toDateTime(), dateTime6);
    QCOMPARE(obj->property("qdatetime7").toDateTime(), dateTime7);
    QCOMPARE(obj->property("qdatetime8").toDateTime(), dateTime8);
    QCOMPARE(obj->property("qdatetime9").toDateTime(), dateTime9);
    QCOMPARE(obj->property("qdatetime10").toDateTime(), dateTime10);
}

void tst_qqmlqt::dateTimeFormatting()
{
    QFETCH(QString, method);
    QFETCH(QStringList, inputProperties);
    QFETCH(QStringList, expectedResults);

    QDate date(2008,12,24);
    QTime time(14,15,38,200);
    QDateTime dateTime(date, time);

    QQmlEngine eng;

    eng.rootContext()->setContextProperty("qdate", date);
    eng.rootContext()->setContextProperty("qtime", time);
    eng.rootContext()->setContextProperty("qdatetime", dateTime);

    QQmlComponent component(&eng, testFileUrl("formatting.qml"));

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
        QCOMPARE(output.size(), expectedResults.size());
        for (int i=0; i<output.count(); i++)
            QCOMPARE(output[i], expectedResults[i]);
    }

    delete object;
}

void tst_qqmlqt::dateTimeFormatting_data()
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

void tst_qqmlqt::dateTimeFormattingVariants()
{
    QFETCH(QString, method);
    QFETCH(QVariant, variant);
    QFETCH(QStringList, expectedResults);

    QQmlEngine eng;
    eng.rootContext()->setContextProperty("qvariant", variant);
    QQmlComponent component(&eng, testFileUrl("formatting.qml"));

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

    QVariant result;
    QVERIFY(QMetaObject::invokeMethod(object, method.toUtf8().constData(),
            Q_RETURN_ARG(QVariant, result),
            Q_ARG(QVariant, QString(QLatin1String("qvariant")))));
    QStringList output = result.toStringList();
    QCOMPARE(output, expectedResults);

    delete object;
}

void tst_qqmlqt::dateTimeFormattingVariants_data()
{
    QTest::addColumn<QString>("method");
    QTest::addColumn<QVariant>("variant");
    QTest::addColumn<QStringList>("expectedResults");

    QDateTime temporary;

    QTime time(11, 16, 39, 755);
    temporary = QDateTime(QDate(1970,1,1), time);
    QTest::newRow("formatTime, qtime") << "formatTime" << QVariant::fromValue(time) << (QStringList() << temporary.time().toString(Qt::DefaultLocaleShortDate) << temporary.time().toString(Qt::DefaultLocaleLongDate) << temporary.time().toString("H:m:s a") << temporary.time().toString("hh:mm:ss.zzz"));

    QDate date(2011,5,31);
    temporary = QDateTime(date);
    QTest::newRow("formatDate, qdate") << "formatDate" << QVariant::fromValue(date) << (QStringList() << temporary.date().toString(Qt::DefaultLocaleShortDate) << temporary.date().toString(Qt::DefaultLocaleLongDate) << temporary.date().toString("ddd MMMM d yy"));
    QTest::newRow("formatDateTime, qdate") << "formatDateTime" << QVariant::fromValue(date) << (QStringList() << temporary.toString(Qt::DefaultLocaleShortDate) << temporary.toString(Qt::DefaultLocaleLongDate) << temporary.toString("M/d/yy H:m:s a"));
    QTest::newRow("formatTime, qdate") << "formatTime" << QVariant::fromValue(date) << (QStringList() << temporary.time().toString(Qt::DefaultLocaleShortDate) << temporary.time().toString(Qt::DefaultLocaleLongDate) << temporary.time().toString("H:m:s a") << temporary.time().toString("hh:mm:ss.zzz"));

    QDateTime dateTime(date, time);
    temporary = dateTime;
    QTest::newRow("formatDate, qdatetime") << "formatDate" << QVariant::fromValue(dateTime) << (QStringList() << temporary.date().toString(Qt::DefaultLocaleShortDate) << temporary.date().toString(Qt::DefaultLocaleLongDate) << temporary.date().toString("ddd MMMM d yy"));
    QTest::newRow("formatDateTime, qdatetime") << "formatDateTime" << QVariant::fromValue(dateTime) << (QStringList() << temporary.toString(Qt::DefaultLocaleShortDate) << temporary.toString(Qt::DefaultLocaleLongDate) << temporary.toString("M/d/yy H:m:s a"));
    QTest::newRow("formatTime, qdatetime") << "formatTime" << QVariant::fromValue(dateTime) << (QStringList() << temporary.time().toString(Qt::DefaultLocaleShortDate) << temporary.time().toString(Qt::DefaultLocaleLongDate) << temporary.time().toString("H:m:s a") << temporary.time().toString("hh:mm:ss.zzz"));

    QString string(QLatin1String("2011/05/31 11:16:39.755"));
    temporary = QDateTime::fromString(string, "yyyy/MM/dd HH:mm:ss.zzz");
    QTest::newRow("formatDate, qstring") << "formatDate" << QVariant::fromValue(string) << (QStringList() << temporary.date().toString(Qt::DefaultLocaleShortDate) << temporary.date().toString(Qt::DefaultLocaleLongDate) << temporary.date().toString("ddd MMMM d yy"));
    QTest::newRow("formatDateTime, qstring") << "formatDateTime" << QVariant::fromValue(string) << (QStringList() << temporary.toString(Qt::DefaultLocaleShortDate) << temporary.toString(Qt::DefaultLocaleLongDate) << temporary.toString("M/d/yy H:m:s a"));
    QTest::newRow("formatTime, qstring") << "formatTime" << QVariant::fromValue(string) << (QStringList() << temporary.time().toString(Qt::DefaultLocaleShortDate) << temporary.time().toString(Qt::DefaultLocaleLongDate) << temporary.time().toString("H:m:s a") << temporary.time().toString("hh:mm:ss.zzz"));

    QColor color(Qt::red);
    temporary = QVariant::fromValue(color).toDateTime();
    QTest::newRow("formatDate, qcolor") << "formatDate" << QVariant::fromValue(color) << (QStringList() << temporary.date().toString(Qt::DefaultLocaleShortDate) << temporary.date().toString(Qt::DefaultLocaleLongDate) << temporary.date().toString("ddd MMMM d yy"));
    QTest::newRow("formatDateTime, qcolor") << "formatDateTime" << QVariant::fromValue(color) << (QStringList() << temporary.toString(Qt::DefaultLocaleShortDate) << temporary.toString(Qt::DefaultLocaleLongDate) << temporary.toString("M/d/yy H:m:s a"));
    QTest::newRow("formatTime, qcolor") << "formatTime" << QVariant::fromValue(color) << (QStringList() << temporary.time().toString(Qt::DefaultLocaleShortDate) << temporary.time().toString(Qt::DefaultLocaleLongDate) << temporary.time().toString("H:m:s a") << temporary.time().toString("hh:mm:ss.zzz"));

    int integer(4);
    temporary = QVariant::fromValue(integer).toDateTime();
    QTest::newRow("formatDate, int") << "formatDate" << QVariant::fromValue(integer) << (QStringList() << temporary.date().toString(Qt::DefaultLocaleShortDate) << temporary.date().toString(Qt::DefaultLocaleLongDate) << temporary.date().toString("ddd MMMM d yy"));
    QTest::newRow("formatDateTime, int") << "formatDateTime" << QVariant::fromValue(integer) << (QStringList() << temporary.toString(Qt::DefaultLocaleShortDate) << temporary.toString(Qt::DefaultLocaleLongDate) << temporary.toString("M/d/yy H:m:s a"));
    QTest::newRow("formatTime, int") << "formatTime" << QVariant::fromValue(integer) << (QStringList() << temporary.time().toString(Qt::DefaultLocaleShortDate) << temporary.time().toString(Qt::DefaultLocaleLongDate) << temporary.time().toString("H:m:s a") << temporary.time().toString("hh:mm:ss.zzz"));
}

void tst_qqmlqt::isQtObject()
{
    QQmlComponent component(&engine, testFileUrl("isQtObject.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test1").toBool(), true);
    QCOMPARE(object->property("test2").toBool(), false);
    QCOMPARE(object->property("test3").toBool(), false);
    QCOMPARE(object->property("test4").toBool(), false);
    QCOMPARE(object->property("test5").toBool(), false);

    delete object;
}

void tst_qqmlqt::btoa()
{
    QQmlComponent component(&engine, testFileUrl("btoa.qml"));

    QString warning1 = component.url().toString() + ":4: Error: Qt.btoa(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test2").toString(), QString("SGVsbG8gd29ybGQh"));

    delete object;
}

void tst_qqmlqt::atob()
{
    QQmlComponent component(&engine, testFileUrl("atob.qml"));

    QString warning1 = component.url().toString() + ":4: Error: Qt.atob(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test2").toString(), QString("Hello world!"));

    delete object;
}

void tst_qqmlqt::fontFamilies()
{
    QQmlComponent component(&engine, testFileUrl("fontFamilies.qml"));

    QString warning1 = component.url().toString() + ":4: Error: Qt.fontFamilies(): Invalid arguments";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QFontDatabase database;
    QCOMPARE(object->property("test2"), QVariant::fromValue(database.families()));

    delete object;
}

void tst_qqmlqt::quit()
{
    QQmlComponent component(&engine, testFileUrl("quit.qml"));

    QSignalSpy spy(&engine, SIGNAL(quit()));
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(spy.count(), 1);

    delete object;
}

void tst_qqmlqt::exit()
{
    QQmlComponent component(&engine, testFileUrl("exit.qml"));

    QSignalSpy spy(&engine, &QQmlEngine::exit);
    QObject *object = component.create();
    QVERIFY(object != Q_NULLPTR);
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QVERIFY(arguments.at(0).toInt() == object->property("returnCode").toInt());

    delete object;
}

void tst_qqmlqt::resolvedUrl()
{
    QQmlComponent component(&engine, testFileUrl("resolvedUrl.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("result").toString(), component.url().toString());
    QCOMPARE(object->property("isString").toBool(), true);

    delete object;
}

void tst_qqmlqt::later_data()
{
    QTest::addColumn<QString>("function");
    QTest::addColumn<QStringList>("expectedWarnings");
    QTest::addColumn<QStringList>("propNames");
    QTest::addColumn<QVariantList>("values");

    QVariant vtrue = QVariant(true);

    QTest::newRow("callLater from onCompleted")
            << QString()
            << QStringList()
            << (QStringList() << "test1_1" << "test2_1" << "processEvents" << "test1_2" << "test2_2")
            << (QVariantList() << vtrue << vtrue << QVariant() << vtrue << vtrue);

    QTest::newRow("trigger Qt.callLater() via repeater")
            << QString(QLatin1String("test2"))
            << QStringList()
            << (QStringList() << "processEvents" << "test2_2")
            << (QVariantList() << QVariant() << vtrue);

    QTest::newRow("recursive Qt.callLater()")
            << QString(QLatin1String("test3"))
            << QStringList()
            << (QStringList() << "processEvents" << "test3_1" << "processEvents" << "test3_2" << "processEvents" << "test3_3")
            << (QVariantList() << QVariant() << vtrue << QVariant() << vtrue << QVariant() << vtrue);

    QTest::newRow("nonexistent function")
            << QString(QLatin1String("test4"))
            << (QStringList() << QString(testFileUrl("later.qml").toString() + QLatin1String(":70: ReferenceError: functionThatDoesNotExist is not defined")))
            << QStringList()
            << QVariantList();

    QTest::newRow("callLater with different args")
            << QString(QLatin1String("test5"))
            << QStringList()
            << (QStringList() << "processEvents" << "test5_1")
            << (QVariantList() << QVariant() << vtrue);

    QTest::newRow("delayed call ordering")
            << QString(QLatin1String("test6"))
            << QStringList()
            << (QStringList() << "processEvents" << "test6_1")
            << (QVariantList() << QVariant() << vtrue);

    QTest::newRow("invoke module api invokable")
            << QString(QLatin1String("test9"))
            << QStringList()
            << (QStringList() << "processEvents" << "test9_1" << "processEvents")
            << (QVariantList() << QVariant() << QVariant(1) << QVariant());

    QTest::newRow("invoke function of deleted QObject via callLater() causing deletion")
            << QString(QLatin1String("test10"))
            << (QStringList() << QString(testFileUrl("LaterComponent.qml").toString() + QLatin1String(":8: ReferenceError: dangerousFunction is not defined (exception occurred during delayed function evaluation)")))
            << (QStringList() << "processEvents" << "test10_1" << "processEvents")
            << (QVariantList() << QVariant() << QVariant(0) << QVariant());

    QTest::newRow("invoke function of deleted QObject via callLater() after deletion")
            << QString(QLatin1String("test11"))
            << QStringList()
            << (QStringList() << "collectGarbage" << "processEvents" << "test11_1" << "processEvents")
            << (QVariantList() << QVariant() << QVariant() << QVariant(1) << QVariant());

    QTest::newRow("invoke function which has no script origin")
            << QString(QLatin1String("test14"))
            << QStringList()
            << (QStringList() << "collectGarbage")
            << (QVariantList() << QVariant());
}

void tst_qqmlqt::later()
{
    QFETCH(QString, function);
    QFETCH(QStringList, expectedWarnings);
    QFETCH(QStringList, propNames);
    QFETCH(QVariantList, values);

    foreach (const QString &w, expectedWarnings)
        QTest::ignoreMessage(QtWarningMsg, qPrintable(w));

    QQmlComponent component(&engine, testFileUrl("later.qml"));
    QObject *root = component.create();
    QVERIFY(root != 0);

    if (!function.isEmpty())
        QMetaObject::invokeMethod(root, qPrintable(function));

    for (int i = 0; i < propNames.size(); ++i) {
        if (propNames.at(i) == QLatin1String("processEvents")) {
            QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
            QCoreApplication::processEvents();
        } else if (propNames.at(i) == QLatin1String("collectGarbage")) {
            engine.collectGarbage();
        } else {
            QCOMPARE(root->property(qPrintable(propNames.at(i))), values.at(i));
        }
    }

    delete root;
}

void tst_qqmlqt::qtObjectContents()
{
    struct StaticQtMetaObject : public QObject
    {
        static const QMetaObject *get()
        { return &staticQtMetaObject; }
    };

    QQmlComponent component(&engine, testFileUrl("qtObjectContents.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QVERIFY(object->property("values").canConvert<QJSValue>());
    QVariantMap values = object->property("values").value<QJSValue>().toVariant().toMap();

    QSet<const char *> keys;
    int uniqueKeys = 0;
    const QMetaObject *qtMetaObject = StaticQtMetaObject::get();
    for (int ii = 0; ii < qtMetaObject->enumeratorCount(); ++ii) {
        QMetaEnum enumerator = qtMetaObject->enumerator(ii);
        for (int jj = 0; jj < enumerator.keyCount(); ++jj) {
            auto key = enumerator.key(jj);
//            qDebug() << "key:" << key;
            if (!keys.contains(key)) {
                ++uniqueKeys;
                keys.insert(key);
            }
            QVERIFY(values.contains(key));
            QVariant value = values.value(key);
            QVERIFY(value.canConvert<int>());
            QCOMPARE(value.toInt(), enumerator.value(jj));
        }
    }
    QVERIFY(values.contains("Asynchronous"));
    QCOMPARE(values.value("Asynchronous").toInt(), 0);
    ++uniqueKeys;
    QVERIFY(values.contains("Synchronous"));
    QCOMPARE(values.value("Synchronous").toInt(), 1);
    ++uniqueKeys;
    QCOMPARE(values.count(), uniqueKeys);

    delete object;
}

class TimeProvider: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QTime time READ time WRITE setTime NOTIFY timeChanged)

public:
    TimeProvider(const QTime &t)
        : m_getTime(t)
    {}

    QTime time() const { return m_getTime; }
    void setTime(const QTime &t) { m_putTime = t; emit timeChanged(); }

signals:
    void timeChanged();

public:
    QTime m_getTime, m_putTime;
};

class TimeZoneSwitch
{
public:
    TimeZoneSwitch(const char *newZone)
        : doChangeZone(qstrcmp(newZone, "localtime") == 0)
    {
        if (!doChangeZone)
            return;

        hadOldZone = qEnvironmentVariableIsSet("TZ");
        if (hadOldZone) {
            oldZone = qgetenv("TZ");
        }
        qputenv("TZ", newZone);
        tzset();
    }

    ~TimeZoneSwitch()
    {
        if (!doChangeZone)
            return;

        if (hadOldZone)
            qputenv("TZ", oldZone);
        else
            qunsetenv("TZ");
        tzset();
    }

private:
    bool doChangeZone;
    bool hadOldZone;
    QByteArray oldZone;
};

void tst_qqmlqt::timeRoundtrip_data()
{
    QTest::addColumn<QTime>("time");

    // Local timezone:
    QTest::newRow("localtime") << QTime(0, 0, 0);

    // No DST:
    QTest::newRow("UTC") << QTime(0, 0, 0);
    QTest::newRow("Europe/Amsterdam") << QTime(1, 0, 0);
    QTest::newRow("Asia/Jakarta") << QTime(7, 0, 0);

    // DST:
    QTest::newRow("Namibia/Windhoek") << QTime(1, 0, 0);
    QTest::newRow("Australia/Adelaide") << QTime(10, 0, 0);
    QTest::newRow("Australia/Hobart") << QTime(10, 0, 0);
    QTest::newRow("Pacific/Auckland") << QTime(12, 0, 0);
    QTest::newRow("Pacific/Samoa") << QTime(13, 0, 0);
}

void tst_qqmlqt::timeRoundtrip()
{
#ifdef Q_OS_WIN
    QSKIP("On Windows, the DateObject doesn't handle DST transitions correctly when the timezone is not localtime."); // I.e.: for this test.
#endif

    TimeZoneSwitch tzs(QTest::currentDataTag());
    QFETCH(QTime, time);

    TimeProvider tp(time);

    QQmlEngine eng;
    eng.rootContext()->setContextProperty(QLatin1String("tp"), &tp);
    QQmlComponent component(&eng, testFileUrl("timeRoundtrip.qml"));
    QObject *obj = component.create();
    QVERIFY(obj != 0);

    // QML reads m_getTime and saves the result as m_putTime; this should come out the same, without
    // any perturbation (e.g. by DST effects) from converting from QTime to V4's Date and back
    // again.
    QCOMPARE(tp.m_getTime, tp.m_putTime);
}

QTEST_MAIN(tst_qqmlqt)

#include "tst_qqmlqt.moc"
