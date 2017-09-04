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
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include "tst_suitetest.h"

/*!
 \class tst_XmlPatternsXSLTS
 \internal
 \since 4.5
 \brief Test Qt XML Patterns against W3C's XSL-T 2.0 test suite, XSLTS.
 */
class tst_XmlPatternsXSLTS : public tst_SuiteTest
{
    Q_OBJECT
public:
    tst_XmlPatternsXSLTS();
protected:
    virtual void catalogPath(QString &write) const;
};

tst_XmlPatternsXSLTS::tst_XmlPatternsXSLTS() : tst_SuiteTest(tst_SuiteTest::XsltSuite)
{
}

void tst_XmlPatternsXSLTS::catalogPath(QString &write) const
{
    const char testSuite[] = "XSLTS";
    const QString testSuitePath = QFINDTESTDATA(testSuite);
    if (!testSuitePath.isEmpty()) {
         const QString testDirectory = QFileInfo(testSuitePath).absolutePath();
         QVERIFY2(QDir::setCurrent(testDirectory), qPrintable(QStringLiteral("Could not chdir to ") + testDirectory));
         write = QLatin1String(testSuite) + QStringLiteral("/catalogResolved.xml");
    }
}

QTEST_MAIN(tst_XmlPatternsXSLTS)

#include "tst_xmlpatternsxslts.moc"

// vim: et:ts=4:sw=4:sts=4
