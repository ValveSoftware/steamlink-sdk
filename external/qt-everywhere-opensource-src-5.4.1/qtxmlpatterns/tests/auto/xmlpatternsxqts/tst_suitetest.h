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

private:
    /**
     * An absolute path to the catalog.
     */
    QString         m_catalogPath;
    const QString   m_existingBaseline;
    const QString   m_candidateBaseline;
    const bool      m_abortRun;
    const SuiteType m_suiteType;
};

#endif

// vim: et:ts=4:sw=4:sts=4
