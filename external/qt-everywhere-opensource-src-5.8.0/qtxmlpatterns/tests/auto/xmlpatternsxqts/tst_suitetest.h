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


#ifndef Q_tst_SuiteTest
#define Q_tst_SuiteTest

#include <QtCore/QObject>
#include "../qxmlquery/TestFundament.h"

/*!
 \class tst_SuiteTest
 \internal
 \since 4.5
 \brief Base class for tst_XmlPatternsXQTS, tst_XmlPatternsXSLTS and tst_XmlPatternsXSDTS.
 */
class tst_SuiteTest : public QObject
                    , private TestFundament
{
    Q_OBJECT

public:
    enum SuiteType
    {
        XQuerySuite,
        XsltSuite,
        XsdSuite
    };

protected:
    /**
     * @p isXSLT is @c true if the catalog opened is an
     * XSL-T test suite.
     *
     * @p alwaysRun is @c true if the test should always be run,
     * regardless of if the file runTests exists.
     */
    tst_SuiteTest(SuiteType type,
                  const bool alwaysRun = false);

    /**
     * The reason why we pass in a mutable reference and have void as return
     * value instead of simply returning the string, is that we in some
     * implementations use QTestLib's macros such as QVERIFY, which contains
     * return statements. Yay for QTestLib.
     */
    virtual void catalogPath(QString &write) const = 0;

    bool dontRun() const;

private Q_SLOTS:
    void initTestCase();
    void runTestSuite() const;
    void checkTestSuiteResult() const;

protected:
    /**
     * An absolute path to the catalog.
     */
    QString         m_catalogPath;
    QString   m_existingBaseline;
    const QString   m_candidateBaseline;
    const bool      m_abortRun;
    const SuiteType m_suiteType;
};

#endif

// vim: et:ts=4:sw=4:sts=4
