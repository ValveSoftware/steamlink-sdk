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

#ifndef PatternistSDK_TestSuite_H
#define PatternistSDK_TestSuite_H

#include <QDate>
#include <QString>

#include "TestContainer.h"

QT_BEGIN_NAMESPACE

class QIODevice;
class QUrl;
class QVariant;

namespace QPatternistSDK
{
    class TestCase;
    class TestSuiteResult;

    /**
     * @short Represents a test suite in the W3C XML Query Test Suite format.
     *
     * TestSuite contains the test suite's test cases and meta data.
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT TestSuite : public TestContainer
    {
    public:
        /**
         * Describes the type of test suite.
         */
        enum SuiteType
        {
            XQuerySuite,    ///< The test suite for XQuery
            XsltSuite,      ///< The test suite for XSLT
            XsdSuite        ///< The test suite for XML Schema
        };

        TestSuite();

        virtual QVariant data(const Qt::ItemDataRole role, int column) const;

        /**
         * The version of the catalog test suite. For example, "0.8.0".
         */
        QString version() const;

        /**
         * When the catalog was designed, last modified.
         */
        QDate designDate() const;

        void setVersion(const QString &version);
        void setDesignDate(const QDate &version);

        /**
         * @return always @c null
         */
        virtual TestContainer *parent() const;

        /**
         * Creates and returns a pointer to a TestSuite instance, which
         * was instantiated from the XQuery Test Suite catalog file @p catalogFile.
         *
         * If loading went wrong, @c null is returned and @p errorMsg is set with a
         * human readable message string. However, @p catalogFile is not validated;
         * if the XML file is not valid against the XQTS task force's W3C XML Schema, the
         * behavior and result for this function is undefined.
         *
         * This function blocks. Currently is only local files supported.
         */
        static TestSuite *openCatalog(const QUrl &catalogFile,
                                      QString &errorMsg,
                                      const bool useExclusionList,
                                      SuiteType type);

        void toXML(XMLWriter &receiver, TestCase *const tc) const;

        /**
         * Evaluates all the test cases in this TestSuite, and returns
         * it all in a TestSuiteResult.
         */
        TestSuiteResult *runSuite();

    private:
        /**
         * Essentially similar to open(const QUrl &, QString &errorMsg),
         * with the difference that it takes directly a QIODevice as input,
         * as opposed to a file name locating the catalog file to read.
         *
         * @param input the test suite catalog
         * @param fileName this URI is used for resolving relative paths inside
         * the catalog file into absolute.
         * @param errorMsg if an error occurs, this QString is set to contain the message.
         * Whether an error occurred can therefore be determined by checking if this variable
         * still is @c null after the call
         * @param useExclusionList whether the excludeTestGroups.txt file should be used
         * to exclude test groups for this catalog
         */
        static TestSuite *openCatalog(QIODevice *input,
                                      QString &errorMsg,
                                      const QUrl &fileName,
                                      const bool useExclusionList,
                                      SuiteType type);
        QString m_version;
        QDate m_designDate;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
