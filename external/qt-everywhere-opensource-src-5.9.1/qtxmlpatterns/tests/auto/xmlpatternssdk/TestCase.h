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

#ifndef PatternistSDK_TestCase_H
#define PatternistSDK_TestCase_H

#include <QtXmlPatterns/QXmlQuery>

#include <private/qexternalvariableloader_p.h>

#include "ErrorHandler.h"
#include "TestBaseLine.h"
#include "Global.h"

#include "TestItem.h"

QT_BEGIN_NAMESPACE

class QDate;
class QString;
class QUrl;

namespace QPatternistSDK
{
    class XMLWriter;

    /**
     * @short A generic abstract base class for test cases.
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT TestCase : public TestItem
    {
    public:
        /**
         * Corresponds to the simpleType test:scenarios-enum
         */
        enum Scenario
        {
            /**
             * The test case should evaluate normally and that the output
             * should match the supplied base line.
             */
            Standard        = 1,

            /**
             * The test case should result in a static error, a parser error.
             */
            ParseError      = 2,

            /**
             * The test case should result in a dynamic error, a runtime error.
             */
            RuntimeError    = 4,

            Trivial         = 8,

            /**
             * ParseError and RuntimeError OR'd.
             */
            AnyError        = RuntimeError | ParseError

        };

        TestCase();
        virtual ~TestCase();

        /**
         * Executes the test, and returns the result. The returned list
         * will always contain exactly one TestResult.
         *
         * @p stage is ignored when running out-of-process.
         */
        virtual TestResult::List execute(const ExecutionStage stage,
                                         TestSuite *ts);

        /**
         * Determines the corresponding Scenario enumerator from the string
         * representation @p string.
         *
         * The following mappings are in effect:
         * @arg @c Standard "standard"
         * @arg @c ParseError "parse-error"
         * @arg @c RuntimeError "runtime-error"
         */
        static Scenario scenarioFromString(const QString &string);

        /**
         * @return always @c true
         */
        virtual bool isFinalNode() const;

        /**
         * Calling this function makes no sense, so it always
         * performs an Q_ASSERT check.
         */
        virtual void appendChild(TreeItem *);

        /**
         * Calling this function makes no sense, so it always
         * performs an Q_ASSERT check.
         */
        virtual TreeItem *child(const unsigned int) const;

        /**
         * @return always zero
         */
        virtual unsigned int childCount() const;

        /**
         * @return always an empty list.
         */
        virtual TreeItem::List children() const;

        /**
         * A description of the test case for human consumption.
         */
        virtual QString description() const = 0;

        /**
         * The title of the test. This can be the identifier of the test, for example.
         */
        virtual QString title() const = 0;

        /**
         * Whether this test case only make use of XPath features.
         *
         * @returns @c false if the test case exercises any XQuery feature
         * which is not available in XPath 2.0.
         */
        virtual bool isXPath() const = 0;

        /**
         * The full name of the creator of the test case. For example, "Frans Englich".
         */
        virtual QString creator() const = 0;

        /**
         * The date of when the test case was created or last modified.
         */
        virtual QDate lastModified() const = 0;

        /**
         * The test's source code. That is, the XPath/XQuery code for the test.
         *
         * @param ok the function sets this value to @c false if loading the query
         * failed, and returns a description of the error for human consumption. If
         * everything went ok, @p ok is set to @c true, and the query is returned.
         */
        virtual QString sourceCode(bool &ok) const = 0;

        /**
         * The path to the file containing the code of the test case.
         */
        virtual QUrl testCasePath() const = 0;

        /**
         * The test case's identifier. For example, "Literals001".
         */
        virtual QString name() const = 0;

        /**
         * What kind of test this is. For example, whether the test case
         * should result in a parser error or should evaluate without errors.
         *
         * The vast common case is that one Scenario is returned; the bit signifiance
         * is for the TestCase sub-class UserTestCase.
         */
        virtual Scenario scenario() const = 0;

        static QString displayName(const Scenario scen);

        /**
         * @returns the valid test baselines for this test case. If only
         * one outcome is valid, the returned list only contains
         * that baseline.
         */
        virtual TestBaseLine::List baseLines() const = 0;

        virtual TestResult *testResult() const;

        virtual ResultSummary resultSummary() const;

        void toXML(XMLWriter &receiver) const;

        virtual QPatternist::ExternalVariableLoader::Ptr externalVariableLoader() const = 0;

        /**
         * @short The XML document that should be used as focus. If none should
         * be used, and hence the focus be undefined, a default constructed
         * QUrl is returned.
         */
        virtual QUrl contextItemSource() const = 0;

        /**
         * Returns by default QXmlQuery::XQuery10.
         */
        virtual QXmlQuery::QueryLanguage language() const;

        virtual QXmlName initialTemplateName() const;
    private:
        TestResult::List execute(const ExecutionStage stage);
        TestResult *createTestResult(const TestResult::Status status,
                                           const QString &comment) const;

        QPointer<TestResult>    m_result;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
