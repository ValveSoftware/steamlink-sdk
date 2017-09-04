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


#include <QtTest/QtTest>

#include "../../../src/plugins/imageformats/svg/qsvgiohandler.cpp"
#include <QImage>
#include <QStringList>
#include <QVector>

#ifndef SRCDIR
#define SRCDIR
#endif


class tst_QSvgPlugin : public QObject
{
Q_OBJECT

public:
    tst_QSvgPlugin();
    virtual ~tst_QSvgPlugin();

private slots:
    void checkSize_data();
    void checkSize();
};



tst_QSvgPlugin::tst_QSvgPlugin()
{
}

tst_QSvgPlugin::~tst_QSvgPlugin()
{
}

void tst_QSvgPlugin::checkSize_data()
{
    QTest::addColumn<QString>("filename");
    QTest::addColumn<int>("imageHeight");
    QTest::addColumn<int>("imageWidth");

    QTest::newRow("square")              << SRCDIR "square.svg"              <<  50 <<  50;
    QTest::newRow("square_size")         << SRCDIR "square_size.svg"         << 200 << 200;
    QTest::newRow("square_size_viewbox") << SRCDIR "square_size_viewbox.svg" << 200 << 200;
    QTest::newRow("square_viewbox")      << SRCDIR "square_viewbox.svg"      << 100 << 100;
    QTest::newRow("tall")                << SRCDIR "tall.svg"                <<  50 <<  25;
    QTest::newRow("tall_size")           << SRCDIR "tall_size.svg"           << 200 << 100;
    QTest::newRow("tall_size_viewbox")   << SRCDIR "tall_size_viewbox.svg"   << 200 << 100;
    QTest::newRow("tall_viewbox")        << SRCDIR "tall_viewbox.svg"        << 100 <<  50;
    QTest::newRow("wide")                << SRCDIR "wide.svg"                <<  25 <<  50;
    QTest::newRow("wide_size")           << SRCDIR "wide_size.svg"           << 100 << 200;
    QTest::newRow("wide_size_viewbox")   << SRCDIR "wide_size_viewbox.svg"   << 100 << 200;
    QTest::newRow("wide_viewbox")        << SRCDIR "wide_viewbox.svg"        <<  50 << 100;
}

void tst_QSvgPlugin::checkSize()
{
    QFETCH(QString, filename);
    QFETCH(int, imageHeight);
    QFETCH(int, imageWidth);

    QFile file(filename);
    file.open(QIODevice::ReadOnly);

    QSvgIOHandler plugin;
    plugin.setDevice(&file);

    QImage image;
    plugin.read(&image);

    file.close();

    QCOMPARE(imageHeight, image.height());
    QCOMPARE(imageWidth, image.width());
}


QTEST_MAIN(tst_QSvgPlugin)
#include "tst_qsvgplugin.moc"
