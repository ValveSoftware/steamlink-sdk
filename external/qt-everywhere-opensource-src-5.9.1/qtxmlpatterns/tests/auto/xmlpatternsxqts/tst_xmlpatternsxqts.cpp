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
#include "tst_suitetest.h"

/*!
 \class tst_XmlPatternsXQTS
 \internal
 \since 4.4
 \brief Tests the actual engine by running W3C's conformance test suite.
 */
class tst_XmlPatternsXQTS : public tst_SuiteTest
{
    Q_OBJECT
public:
    tst_XmlPatternsXQTS();
public:
    virtual void catalogPath(QString &write) const;
};

tst_XmlPatternsXQTS::tst_XmlPatternsXQTS() : tst_SuiteTest(tst_SuiteTest::XQuerySuite)
{
}

void tst_XmlPatternsXQTS::catalogPath(QString &write) const
{
    if(dontRun())
        QSKIP("This test takes too long time to run on the majority of platforms.");

    write = QFINDTESTDATA("../3rdparty/testsuites/XQTS/XQTSCatalog.xml");
    return;
}

QTEST_MAIN(tst_XmlPatternsXQTS)

#include "tst_xmlpatternsxqts.moc"

// vim: et:ts=4:sw=4:sts=4
