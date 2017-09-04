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

#ifndef PatternistSDK_TestSuiteResult_H
#define PatternistSDK_TestSuiteResult_H

#include <QDate>
#include <QString>

#include "TestResult.h"

QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    /**
     * @short A collection of TestResult instances.
     *
     * A TestSuiteResult gathers all TestResult instances, and provides
     * the toXML() function which serializes it all into a XQuery Test Suite
     * result file, conforming to XQTSResult.xsd.
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT TestSuiteResult
    {
    public:
        ~TestSuiteResult();

        TestSuiteResult(const QString &testSuiteVersion,
                        const QDate &runDate,
                        const TestResult::List &results);

        /**
         * Serializes the test results this TestSuiteResult represents,
         * into XQTS test-suite-result document, conformant to XQTSCatalog.xsd.
         */
        void toXML(XMLWriter &receiver) const;

    private:
        const QString m_testSuiteVersion;
        const QDate m_runDate;
        const TestResult::List m_results;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
