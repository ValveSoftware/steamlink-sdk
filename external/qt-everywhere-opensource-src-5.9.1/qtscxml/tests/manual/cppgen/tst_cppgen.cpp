/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
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

#include <QtScxml/scxmlcppdumper.h>

using namespace Scxml;

class TestCppGen: public QObject
{
    Q_OBJECT

private slots:
    void idMangling_data();
    void idMangling();
};

void TestCppGen::idMangling_data()
{
    QTest::addColumn<QString>("unmangled");
    QTest::addColumn<QString>("mangled");

    QTest::newRow("One:Two") << "One:Two" << "One_colon_Two";
    QTest::newRow("one-piece") << "one-piece" << "one_dash_piece";
    QTest::newRow("two_words") << "two_words" << "two__words";
    QTest::newRow("me@work") << "me@work" << "me_at_work";
}

void TestCppGen::idMangling()
{
    QFETCH(QString, unmangled);
    QFETCH(QString, mangled);

    QCOMPARE(CppDumper::mangleId(unmangled), mangled);
}

QTEST_MAIN(TestCppGen)
#include "tst_cppgen.moc"
