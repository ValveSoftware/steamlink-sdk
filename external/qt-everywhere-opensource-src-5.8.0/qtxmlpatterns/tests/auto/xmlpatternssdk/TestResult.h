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

#ifndef PatternistSDK_TestResult_H
#define PatternistSDK_TestResult_H

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <QtXmlPatterns/private/qitem_p.h>
#include "ErrorHandler.h"

#include "ASTItem.h"


QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    class ASTItem;
    class XMLWriter;

    /**
     * @short represents the result produced by running a test case.
     *
     * This information TestResult houses is:
     *
     * - The result status() of the run. Whether the test case succeeded or not, for example.
     * - The astTree() which reflects the compiled test case
     * - The messages issued when compiling and running the test case, retrievable via messages()
     * - The data -- XPath Data Model items() -- the test case evaluated to, if any.
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT TestResult : public QObject
    {
        Q_OBJECT

    public:
        enum Status
        {
            /**
             * Used when the status is unknown.
             */
            Unknown = 0,

            /**
             * The test case passed.
             */
            Pass,

            /**
             * The test case failed.
             */
            Fail,

            /**
             * The test was not run. Similar to "SKIP".
             */
            NotTested
        };

        /**
         * A list of TestResult instances.
         */
        typedef QList<QPointer<TestResult> > List;

        /**
         * Constructs a TestResult.
         *
         * @param testName the name of the test. For example, @c Literal-001.
         * @param astTree may be @c null, signalling no AST being available, or point to one.
         * @param status the result status of running the test-case. Whether the test-case
         * passed or failed, and so forth.
         * @param errors the errors and warnings that were reported while running the test-case
         * @param items the XDM items that were outputted, if any
         * @param serialized the output when serialized
         */
        TestResult(const QString &testName,
                   const Status status,
                   ASTItem *astTree,
                   const ErrorHandler::Message::List &errors,
                   const QPatternist::Item::List &items,
                   const QString &serialized);

        virtual ~TestResult();

        Status status() const;

        QString comment() const;
        void setComment(const QString &comment);

        QPatternist::Item::List items() const;

        ErrorHandler::Message::List messages() const;

        /**
         * Serializes itself to @p receiver, into a test-case element,
         * as per @c XQTSResult.xsd.
         */
        void toXML(XMLWriter &receiver) const;

        ASTItem *astTree() const;

        /**
         * @returns a string representation for @p status, as per the anonymous
         * type inside the type test-case, in @c XQTSResult.xsd. For example, if @p status
         * is NotTested, is "not tested" returned.
         */
        static QString displayName(const TestResult::Status status);

        static Status statusFromString(const QString &string);

        /**
         * @returns the output of this test result(if any) as when
         * being serialized.
         */
        QString asSerialized() const;

    private:
        const Status                        m_status;
        QString                             m_comment;
        const ErrorHandler::Message::List   m_messages;
        QPointer<ASTItem>                   m_astTree;
        QString                             m_testName;
        const QPatternist::Item::List       m_items;
        const QString                       m_asSerialized;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
