/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/qtest.h>
#include "../shared/visualtestutil.h"

#include <QtGui/qfont.h>
#include <QtGui/qpa/qplatformtheme.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuickTemplates2/private/qquickapplicationwindow_p.h>
#include <QtQuickTemplates2/private/qquickcontrol_p.h>
#include <QtQuickTemplates2/private/qquickpopup_p.h>
#include <QtQuickControls2/private/qquickproxytheme_p.h>

using namespace QQuickVisualTestUtil;

class tst_font : public QQmlDataTest
{
    Q_OBJECT

private slots:
    void font_data();
    void font();

    void inheritance_data();
    void inheritance();

    void defaultFont_data();
    void defaultFont();
};

void tst_font::font_data()
{
    QTest::addColumn<QString>("testFile");
    QTest::addColumn<QFont>("expectedFont");

    QTest::newRow("Control") << "font-control-default.qml" << QFont();
    QTest::newRow("AppWindow") << "font-appwindow-default.qml" << QFont();
    QTest::newRow("Popup") << "font-popup-default.qml" << QFont();

    QFont customFont;
    customFont.setCapitalization(QFont::AllUppercase);
    customFont.setFamily("Courier");
    customFont.setItalic(true);
    customFont.setPixelSize(60);
    customFont.setStrikeOut(true);
    customFont.setUnderline(true);
    customFont.setWeight(QFont::DemiBold);

    QTest::newRow("Control:custom") << "font-control-custom.qml" << customFont;
    QTest::newRow("AppWindow:custom") << "font-appwindow-custom.qml" << customFont;
    QTest::newRow("Popup:custom") << "font-popup-custom.qml" << customFont;
}

void tst_font::font()
{
    QFETCH(QString, testFile);
    QFETCH(QFont, expectedFont);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl(testFile));

    QScopedPointer<QObject> object(component.create());
    QVERIFY2(!object.isNull(), qPrintable(component.errorString()));

    QVariant var = object->property("font");
    QVERIFY(var.isValid());

    QFont actualFont = var.value<QFont>();
    QCOMPARE(actualFont, expectedFont);
}

void tst_font::inheritance_data()
{
    QTest::addColumn<QString>("testFile");

    QTest::newRow("Control") << "inheritance-control.qml";
    QTest::newRow("Child Control") << "inheritance-childcontrol.qml";
    QTest::newRow("Dynamic Control") << "inheritance-dynamiccontrol.qml";
    QTest::newRow("Dynamic Child Control") << "inheritance-dynamicchildcontrol.qml";

    QTest::newRow("Popup") << "inheritance-popup.qml";
    QTest::newRow("Child Popup") << "inheritance-childpopup.qml";
    QTest::newRow("Dynamic Popup") << "inheritance-dynamicpopup.qml";
    QTest::newRow("Dynamic Child Popup") << "inheritance-dynamicchildpopup.qml";
}

void tst_font::inheritance()
{
    QFETCH(QString, testFile);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl(testFile));

    QScopedPointer<QQuickApplicationWindow> window(qobject_cast<QQuickApplicationWindow *>(component.create()));
    QVERIFY2(!window.isNull(), qPrintable(component.errorString()));

    QObject *control = window->property("control").value<QObject *>();
    QObject *child = window->property("child").value<QObject *>();
    QObject *grandChild = window->property("grandChild").value<QObject *>();
    QVERIFY(control && child && grandChild);

    QCOMPARE(window->font(), QFont());

    QCOMPARE(control->property("font").value<QFont>(), QFont());
    QCOMPARE(child->property("font").value<QFont>(), QFont());
    QCOMPARE(grandChild->property("font").value<QFont>(), QFont());

    QFont childFont;
    childFont.setFamily("Arial");
    childFont.setPixelSize(80);
    childFont.setItalic(true);
    child->setProperty("font", childFont);
    QCOMPARE(child->property("font").value<QFont>(), childFont);
    QCOMPARE(grandChild->property("font").value<QFont>(), childFont);

    QFont grandChildFont(childFont);
    grandChildFont.setFamily("Times New Roman");
    grandChildFont.setUnderline(true);
    grandChild->setProperty("font", grandChildFont);
    QCOMPARE(child->property("font").value<QFont>(), childFont);
    QCOMPARE(grandChild->property("font").value<QFont>(), grandChildFont);

    QFont windowFont;
    windowFont.setWeight(QFont::Thin);
    window->setFont(windowFont);
    QCOMPARE(window->font(), windowFont);
    QCOMPARE(control->property("font").value<QFont>(), windowFont);

    childFont.setWeight(QFont::Thin);
    QCOMPARE(child->property("font").value<QFont>(), childFont);

    grandChildFont.setWeight(QFont::Thin);
    QCOMPARE(grandChild->property("font").value<QFont>(), grandChildFont);

    child->setProperty("font", QVariant());
    QCOMPARE(child->property("font").value<QFont>(), windowFont);
    QCOMPARE(grandChild->property("font").value<QFont>(), grandChildFont);

    grandChild->setProperty("font", QVariant());
    QCOMPARE(grandChild->property("font").value<QFont>(), windowFont);
}

class TestFontTheme : public QQuickProxyTheme
{
public:
    TestFontTheme(QPlatformTheme *theme) : QQuickProxyTheme(theme)
    {
        std::fill(fonts, fonts + QPlatformTheme::NFonts, static_cast<QFont *>(0));

        for (int i = QPlatformTheme::SystemFont; i < QPlatformTheme::NFonts; ++i) {
            QFont font = QFont();
            font.setPixelSize(i + 10);
            fonts[i] = new QFont(font);
        }

        QGuiApplicationPrivate::platform_theme = this;
    }

    const QFont *font(Font type = SystemFont) const override
    {
        return fonts[type];
    }

private:
    QFont *fonts[QPlatformTheme::NFonts];
};

Q_DECLARE_METATYPE(QPlatformTheme::Font)

void tst_font::defaultFont_data()
{
    QTest::addColumn<QString>("control");
    QTest::addColumn<QPlatformTheme::Font>("fontType");

    QTest::newRow("AbstractButton") << "AbstractButton" << QPlatformTheme::SystemFont;
    QTest::newRow("ApplicationWindow") << "ApplicationWindow" << QPlatformTheme::SystemFont;
    QTest::newRow("Button") << "Button" << QPlatformTheme::PushButtonFont;
    QTest::newRow("CheckBox") << "CheckBox" << QPlatformTheme::CheckBoxFont;
    QTest::newRow("CheckDelegate") << "CheckDelegate" << QPlatformTheme::ListViewFont;
    QTest::newRow("ComboBox") << "ComboBox" << QPlatformTheme::ComboMenuItemFont;
    QTest::newRow("Container") << "Container" << QPlatformTheme::SystemFont;
    QTest::newRow("Control") << "Control" << QPlatformTheme::SystemFont;
    QTest::newRow("Dial") << "Dial" << QPlatformTheme::SystemFont;
    QTest::newRow("Dialog") << "Dialog" << QPlatformTheme::SystemFont;
    QTest::newRow("DialogButtonBox") << "DialogButtonBox" << QPlatformTheme::SystemFont;
    QTest::newRow("Drawer") << "Drawer" << QPlatformTheme::SystemFont;
    QTest::newRow("Frame") << "Frame" << QPlatformTheme::SystemFont;
    QTest::newRow("GroupBox") << "GroupBox" << QPlatformTheme::GroupBoxTitleFont;
    QTest::newRow("ItemDelegate") << "ItemDelegate" << QPlatformTheme::ItemViewFont;
    QTest::newRow("Label") << "Label" << QPlatformTheme::LabelFont;
    QTest::newRow("Menu") << "Menu" << QPlatformTheme::MenuFont;
    QTest::newRow("MenuItem") << "MenuItem" << QPlatformTheme::MenuItemFont;
    QTest::newRow("MenuSeparator") << "MenuSeparator" << QPlatformTheme::SystemFont;
    QTest::newRow("Page") << "Page" << QPlatformTheme::SystemFont;
    QTest::newRow("Pane") << "Pane" << QPlatformTheme::SystemFont;
    QTest::newRow("Popup") << "Popup" << QPlatformTheme::SystemFont;
    QTest::newRow("ProgressBar") << "ProgressBar" << QPlatformTheme::SystemFont;
    QTest::newRow("RadioButton") << "RadioButton" << QPlatformTheme::RadioButtonFont;
    QTest::newRow("RadioDelegate") << "RadioDelegate" << QPlatformTheme::ListViewFont;
    QTest::newRow("RangeSlider") << "RangeSlider" << QPlatformTheme::SystemFont;
    QTest::newRow("RoundButton") << "RoundButton" << QPlatformTheme::PushButtonFont;
    QTest::newRow("ScrollBar") << "ScrollBar" << QPlatformTheme::SystemFont;
    QTest::newRow("ScrollIndicator") << "ScrollIndicator" << QPlatformTheme::SystemFont;
    QTest::newRow("Slider") << "Slider" << QPlatformTheme::SystemFont;
    QTest::newRow("SpinBox") << "SpinBox" << QPlatformTheme::EditorFont;
    QTest::newRow("SwipeDelegate") << "SwipeDelegate" << QPlatformTheme::ListViewFont;
    QTest::newRow("Switch") << "Switch" << QPlatformTheme::SystemFont; // ### TODO: add QPlatformTheme::SwitchFont
    QTest::newRow("SwitchDelegate") << "SwitchDelegate" << QPlatformTheme::ListViewFont;
    QTest::newRow("TabBar") << "TabBar" << QPlatformTheme::SystemFont;
    QTest::newRow("TabButton") << "TabButton" << QPlatformTheme::TabButtonFont;
    QTest::newRow("TextArea") << "TextArea" << QPlatformTheme::EditorFont;
    QTest::newRow("TextField") << "TextField" << QPlatformTheme::EditorFont;
    QTest::newRow("ToolBar") << "ToolBar" << QPlatformTheme::SystemFont;
    QTest::newRow("ToolButton") << "ToolButton" << QPlatformTheme::ToolButtonFont;
    QTest::newRow("ToolSeparator") << "ToolSeparator" << QPlatformTheme::SystemFont;
    QTest::newRow("ToolTip") << "ToolTip" << QPlatformTheme::TipLabelFont;
    QTest::newRow("Tumbler") << "Tumbler" << QPlatformTheme::SystemFont;
}

void tst_font::defaultFont()
{
    QFETCH(QString, control);
    QFETCH(QPlatformTheme::Font, fontType);

    TestFontTheme theme(QGuiApplicationPrivate::platform_theme);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QString("import QtQuick.Controls 2.2; %1 { }").arg(control).toUtf8(), QUrl());

    QScopedPointer<QObject> object(component.create());
    QVERIFY2(!object.isNull(), qPrintable(component.errorString()));

    QVariant var = object->property("font");
    QVERIFY(var.isValid());

    const QFont *expectedFont = theme.font(fontType);
    QVERIFY(expectedFont);

    QFont actualFont = var.value<QFont>();

    if (actualFont != *expectedFont) {
        qDebug() << QTest::currentDataTag() << actualFont << *expectedFont;
    }

    QCOMPARE(actualFont, *expectedFont);
}

QTEST_MAIN(tst_font)

#include "tst_font.moc"
