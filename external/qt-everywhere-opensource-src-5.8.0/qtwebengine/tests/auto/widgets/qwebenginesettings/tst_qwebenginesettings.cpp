/*
    Copyright (C) 2015 The Qt Company Ltd.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QtTest/QtTest>

#include <qwebengineprofile.h>
#include <qwebenginesettings.h>

class tst_QWebEngineSettings: public QObject {
    Q_OBJECT

private Q_SLOTS:
    void resetAttributes();
    void defaultFontFamily_data();
    void defaultFontFamily();
};

void tst_QWebEngineSettings::resetAttributes()
{
    QWebEngineProfile profile;
    QWebEngineSettings *settings = profile.settings();

    // Attribute
    bool defaultValue = settings->testAttribute(QWebEngineSettings::FullScreenSupportEnabled);
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, !defaultValue);
    QCOMPARE(!defaultValue, settings->testAttribute(QWebEngineSettings::FullScreenSupportEnabled));
    settings->resetAttribute(QWebEngineSettings::FullScreenSupportEnabled);
    QCOMPARE(defaultValue, settings->testAttribute(QWebEngineSettings::FullScreenSupportEnabled));

    // Font family
    QString defaultFamily = settings->fontFamily(QWebEngineSettings::StandardFont);
    QString newFontFamily("PugDog");
    settings->setFontFamily(QWebEngineSettings::StandardFont, newFontFamily);
    QCOMPARE(newFontFamily, settings->fontFamily(QWebEngineSettings::StandardFont));
    settings->resetFontFamily(QWebEngineSettings::StandardFont);
    QCOMPARE(defaultFamily, settings->fontFamily(QWebEngineSettings::StandardFont));

    // Font size
    int defaultSize = settings->fontSize(QWebEngineSettings::MinimumFontSize);
    int newSize = defaultSize + 10;
    settings->setFontSize(QWebEngineSettings::MinimumFontSize, newSize);
    QCOMPARE(newSize, settings->fontSize(QWebEngineSettings::MinimumFontSize));
    settings->resetFontSize(QWebEngineSettings::MinimumFontSize);
    QCOMPARE(defaultSize, settings->fontSize(QWebEngineSettings::MinimumFontSize));
}

void tst_QWebEngineSettings::defaultFontFamily_data()
{
    QTest::addColumn<int>("fontFamily");

    QTest::newRow("StandardFont") << static_cast<int>(QWebEngineSettings::StandardFont);
    QTest::newRow("FixedFont") << static_cast<int>(QWebEngineSettings::FixedFont);
    QTest::newRow("SerifFont") << static_cast<int>(QWebEngineSettings::SerifFont);
    QTest::newRow("SansSerifFont") << static_cast<int>(QWebEngineSettings::SansSerifFont);
    QTest::newRow("CursiveFont") << static_cast<int>(QWebEngineSettings::CursiveFont);
    QTest::newRow("FantasyFont") << static_cast<int>(QWebEngineSettings::FantasyFont);
}

void tst_QWebEngineSettings::defaultFontFamily()
{
    QWebEngineProfile profile;
    QWebEngineSettings *settings = profile.settings();

    QFETCH(int, fontFamily);
    QVERIFY(!settings->fontFamily(static_cast<QWebEngineSettings::FontFamily>(fontFamily)).isEmpty());
}

QTEST_MAIN(tst_QWebEngineSettings)

#include "tst_qwebenginesettings.moc"
