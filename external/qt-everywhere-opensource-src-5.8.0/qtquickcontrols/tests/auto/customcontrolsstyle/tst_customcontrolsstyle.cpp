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

#include <QTest>
#include <QRegularExpression>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStringListModel>
#include "../shared/util.h"

class tst_customcontrolsstyle : public QQmlDataTest
{
    Q_OBJECT

public:
    tst_customcontrolsstyle() {}

private slots:
    void initTestCase();

    void style_data();
    void style();
    void changeStyle();
};

void tst_customcontrolsstyle::initTestCase()
{
    QQmlDataTest::initTestCase();
}

void tst_customcontrolsstyle::style_data()
{
    QTest::addColumn<QString>("specifiedStyle");
    QTest::addColumn<QString>("expectedStyleName");

    QTest::newRow("NonExistentStyle") << QString::fromLatin1("NonExistentStyle") << QString::fromLatin1("Base");
    QTest::newRow("CustomFileSystemStyle") << directory() + QString::fromLatin1("/Style") << QString::fromLatin1("Style");
    QTest::newRow("BuiltinQrcStyle") << QString::fromLatin1("ResourceStyle") << QString::fromLatin1("ResourceStyle"); // from :/qt-project.org/imports/QtQuick/Controls/Styles
    QTest::newRow("CustomQrcStyle") << QString::fromLatin1(":/Style") << QString::fromLatin1("Style");
}

void tst_customcontrolsstyle::style()
{
    QFETCH(QString, specifiedStyle);
    QFETCH(QString, expectedStyleName);

    qputenv("QT_QUICK_CONTROLS_1_STYLE", specifiedStyle.toLocal8Bit());

    const bool expectBase = expectedStyleName == QLatin1String("Base");

    if (specifiedStyle != expectedStyleName && expectBase) {
        QString regexStr = QString::fromLatin1("WARNING: Cannot find style \"%1\" - fallback: .*%2").arg(specifiedStyle).arg("Base");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(regexStr));
    }

    QQmlEngine engine;

    QQmlComponent component(&engine, testFileUrl("TestComponent.qml"));
    QTRY_COMPARE(component.status(), QQmlComponent::Ready);

    QScopedPointer<QObject> object(component.create());
    QVERIFY(object);

    QCOMPARE(object->property("styleName").toString(), expectedStyleName);

    QVariant returnedValue;
    QMetaObject::invokeMethod(object.data(), "buttonStyleComponent", Q_RETURN_ARG(QVariant, returnedValue));

    QVERIFY(returnedValue.isValid());
    QVERIFY(returnedValue.canConvert<QQmlComponent*>());
    QVERIFY(returnedValue.value<QQmlComponent*>());

    QQuickWindow *window = qobject_cast<QQuickWindow*>(object.data());
    QVERIFY(QTest::qWaitForWindowExposed(window));

    if (!expectBase) {
        QImage windowImage = window->grabWindow();
        QCOMPARE(windowImage.pixel(0, 0), QColor(Qt::red).rgb());
    }
}

// start with Base, switch to custom style later on (for a specific QML engine)
void tst_customcontrolsstyle::changeStyle()
{
    qputenv("QT_QUICK_CONTROLS_1_STYLE", "Base");
    QByteArray importPath = qgetenv("QML2_IMPORT_PATH");
    if (importPath.isEmpty())
        importPath = QFile::encodeName(directory());
    else
        importPath.prepend(QFile::encodeName(directory()) + QDir::listSeparator().toLatin1());
    qputenv("QML2_IMPORT_PATH", importPath);

    QQmlEngine engine;

    QQmlComponent component(&engine, testFileUrl("TestComponent.qml"));
    QTRY_COMPARE(component.status(), QQmlComponent::Ready);

    QScopedPointer<QObject> object(component.create());
    QVERIFY(object);

    QCOMPARE(object->property("styleName").toString(), QString("Base"));

    // Switch to "Style" custom style
    QQmlComponent c(&engine);
    c.setData("import QtQuick 2.1\n"
        "import QtQuick.Controls 1.0\n"
        "import QtQuick.Controls.Private 1.0\n"
        "Item {"
          "Component.onCompleted: {"
            "Settings.styleName = \"Style\";"
          "}"
        "}", QUrl());
    QObject *o = c.create();
    o->deleteLater();

    QCOMPARE(object->property("styleName").toString(), QString("Style"));
    QMetaObject::invokeMethod(object.data(), "buttonStyleComponent");
    QQuickWindow *window = qobject_cast<QQuickWindow*>(object.data());
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QImage windowImage = window->grabWindow();
    QCOMPARE(windowImage.pixel(0, 0), QColor(Qt::blue).rgb());
}

QTEST_MAIN(tst_customcontrolsstyle)

#include "tst_customcontrolsstyle.moc"
