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
