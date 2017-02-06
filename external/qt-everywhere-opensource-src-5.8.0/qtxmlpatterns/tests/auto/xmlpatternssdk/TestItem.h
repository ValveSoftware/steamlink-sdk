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

#ifndef PatternistSDK_TestItem_H
#define PatternistSDK_TestItem_H

#include "TestResult.h"
#include "TreeItem.h"

QT_BEGIN_NAMESPACE

template<typename A, typename B> struct QPair;

namespace QPatternistSDK
{
    class XMLWriter;
    class TestSuite;

    /**
     * @short base class for all classes which
     * represent an element in an XQuery Test Suite catalog.
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT TestItem : public TreeItem
    {
    public:

        /**
         * Determines how far an test case execution should go.
         */
        enum ExecutionStage
        {
            /**
             * The query will not be run. It will only go through the (whole) compilation stage.
             */
            CompileOnly = 1,

            /**
             * The query will be compiled and run, as ordinary.
             */
            CompileAndRun
        };

        /**
         * Represents a summary of test results for a collection
         * of tests. QPair::first contains the amount of
         * passed tests; QPair::second contains the count of
         * all tests. For example TestCase::summary() returns
         * ResultSummary(0, 1) or ResultSummary(1, 1) depending
         * on whether the TestCase have succeeded or not.
         */
        typedef QPair<int, int> ResultSummary;

        /**
         * Executes the test case(s) this TestItem represents,
         * and return the TestResult. For example, the TestGroup
         * returns the result of its children concatenated, while
         * TestCase always returns a list containing one
         * TestResult(what it evaluated to).
         */
        virtual TestResult::List execute(const ExecutionStage stage,
                                         TestSuite *ts) = 0;

        /**
         * @todo Rename this function. Perhaps create a type() hierarchy
         * instead.
         */
        virtual bool isFinalNode() const = 0;

        /**
         * @returns a ResultSummary for this TestItem.
         */
        virtual ResultSummary resultSummary() const = 0;

        /**
         * Serializes into the corresponding elements attributes
         * specified in XQTSCatalog.xsd.
         *
         * @note Sub-classes must assume the XQTSCatalog namespace
         * is the default namespace, and not add any namespace declarations.
         */
        //virtual void toXML(XMLWriter &receiver) const = 0;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
