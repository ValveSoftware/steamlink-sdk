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

#ifndef PatternistSDK_TestBaseLine_H
#define PatternistSDK_TestBaseLine_H

#include <QString>

#include "Global.h"
#include "TestResult.h"

QT_BEGIN_NAMESPACE

class QDomNamedNodeMap;
class QDomNode;
class QDomNodeList;
template<typename T> class QList;

namespace QPatternistSDK
{
    /**
     * @short Represents an expected test result for a test case.
     *
     * TestBaseLine represents a valid outcome for a test case,
     * the "base line". A XQTS test case can have many different valid
     * base lines, and one TestBaseLine instance represents on of them.
     *
     * Of highest interest, TestBaseLine have the function scan() and
     * scanErrors(), which allows serialized output to be
     * compared to the base line.
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT TestBaseLine
    {
    public:
        typedef QList<TestBaseLine *> List;

        /**
         * Identifies what kind of comparator to use. The documentation
         * for each enumerator is copied from
         * <a href="http://www.w3.org/XML/Query/test-suite/Guidelines
         * for Running the XML Query Test Suite.html">Guidelines
         * for Running the XML Query Test Suite</a>
         */
        enum Type
        {
            /**
             * The test harness must canonicalize both, the actual result
             * and the expected result according to the "Canonical XML" recommendation [2],
             * which refers to a number of open-source implementations. Byte-comparison can
             * then be applied to the resulting XML documents. If the test harness does
             * this process in a different manner, it must be documented.
             */
            XML,

            /**
             * For XML fragments, the same root node must be created for both,
             * implementation result and test suite result. The resulting XML
             * can be compared using XML comparison.
             */
            Fragment,

            /**
             * Text (that has been produced by XML serialization) is compared
             * using byte-comparison.
             */
            Text,

            /**
             * No comparison needs to be applied; the result is always @c true if
             * the implementation successfully executes the test case.
             */
            Ignore,

            /**
             * A human is required to make the call about correctness of the result
             * according to the description in the test case.
             */
            Inspect,

            /**
             * The expected result of the test case is an error, identified as an
             * eight-character error code (e.g., XPST0003). The result of a test is
             * @c true, if the implementation raises an error. However, raising an error
             * because an implementation does not support the feature is not
             * considered a correct result.
             */
            ExpectedError,

            /**
             * A special comparison for the schema validation tests. The details
             * can only be 'true' or 'false' depending on whether it is a valid
             * schema or not.
             */
            SchemaIsValid
        };

        /**
         * Takes a string identifying a comparator either in the XSL-T or the
         * XQuery test suite, and returns an enum value for it.
         *
         * If the value is unknown, the code asserts.
         */
        static Type identifierFromString(const QString &string);

        /**
         * @returns a display name for @p id. For example, if Inspect was passed, "Inspect"
         * would be returned.
         */
        static QString displayName(const Type id);

        /**
         * Compares @p items(typically the result of an evaluation) against
         * the base lines @p lines.
         *
         * @returns the status of the first base line which passes,
         * otherwise TestResult::Fail.
         */
        static TestResult::Status scan(const QString &serialized,
                                       const TestBaseLine::List &lines);

        static TestResult::Status scanErrors(const ErrorHandler::Message::List &errors,
                                             const TestBaseLine::List &lines);

        /**
         * Constructs a TestBaseLine of type @p type.
         */
        TestBaseLine(const Type type);

        /**
         * What @p details contains depends on the type(). If the type() is ExpectedError,
         * @p details contains the relevant error code. If the type() is one which compares
         * result against a base line, it is a filename locating the baseline.
         */
        void setDetails(const QString &details);

        Type type() const;

        QString details() const;

        void toXML(XMLWriter &receiver) const;

    protected:
        TestResult::Status verify(const QString &serializedInput) const;

    private:
        static bool isDeepEqual(const QDomNode &n1, const QDomNode &n2);

        /**
         * @returns @c true if the nodes in @p cl1 are equal to @p cl2, by calling isDeepEqual()
         * for each pair.
         */
        static bool isChildrenDeepEqual(const QDomNodeList &cl1, const QDomNodeList &cl2);

        /**
         * Considers @p cl1 and @p cl2 to be lists containing attributes. The list are equal
         * if they contain attributes by same value and name, but regardless of order.
         */
        static bool isAttributesEqual(const QDomNamedNodeMap &cl1, const QDomNamedNodeMap &cl2);
        const Type m_type;
        QString m_details;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
