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

#ifndef PatternistSDK_XSLTTestSuiteHandler_H
#define PatternistSDK_XSLTTestSuiteHandler_H

#include <QStack>
#include <QUrl>
#include <QXmlDefaultHandler>

#include "ExternalSourceLoader.h"
#include "TestSuite.h"
#include "XQTSTestCase.h"

QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    class TestBaseLine;
    class TestGroup;

    /**
     * @short Creates a TestSuite from the XQuery Test Suite catalog.
     *
     * The created TestSuite can be retrieved via testSuite().
     *
     * @note XSLTTestSuiteHandler assumes the XML is valid by having been validated
     * against the W3C XML Schema. It has no safety checks for that the XML format
     * is correct but is hard coded for it. Thus, the behavior is undefined if
     * the XML is invalid.
     *
     * @see http://www.w3.org/XML/Group/xslt20-test/TestSuiteStagingArea/catalog.html
     * @see http://www.w3.org/XML/Group/xslt20-test/Documentation/XSLT2Test.htm
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT XSLTTestSuiteHandler : public QXmlDefaultHandler
    {
    public:
        /**
         * @param catalogFile the URI for the catalog file being parsed. This
         * URI is used for creating absolute URIs for files mentioned in
         * the catalog with relative URIs.
         * @param useExclusionList whether excludeTestGroups.txt should be used to ignore
         * test groups when loading
         */
        XSLTTestSuiteHandler(const QUrl &catalogFile);
        virtual bool characters(const QString &ch);

        virtual bool endElement(const QString &namespaceURI,
                                const QString &localName,
                                const QString &qName);
        virtual bool startElement(const QString &namespaceURI,
                                  const QString &localName,
                                  const QString &qName,
                                  const QXmlAttributes &atts);

        virtual TestSuite *testSuite() const;

    private:
        TestGroup *containerFor(const QString &name);

        QHash<QString, TestGroup *>         m_containers;

        TestSuite *                         m_ts;
        XQTSTestCase *                      m_tc;
        TestBaseLine *                      m_baseLine;
        QString                             m_ch;
        const QUrl                          m_catalogFile;

        /**
         * The base URI for where the XQuery query files are found.
         * It is absolute, resolved against catalogFile.
         */
        QUrl                                m_queryOffset;

        QUrl                                m_baselineOffset;
        QUrl                                m_sourceOffset;
        QUrl                                m_currentQueryPath;
        QUrl                                m_currentBaselinePath;

        /**
         * In the XQTSCatalog.xml, each source file in each test is referred to
         * by a key, which can be fully looked up in the @c sources element. This QHash
         * maps the keys to absolute URIs pointing to the source files.
         */
        ExternalSourceLoader::SourceMap     m_sourceMap;

        ExternalSourceLoader::VariableMap   m_tcSourceInputs;

        QPatternist::ResourceLoader::Ptr    m_resourceLoader;

        /**
         * The current value of <tt>input-file/\@variable</tt>.
         */
        QString                             m_currentInputVariable;

        /**
         * The names of the test groups.
         */
        QStack<QString>                     m_testGroupName;

        /**
         * Holds the content of the current <tt>input-URI</tt> element.
         */
        QString                             m_inputURI;
        QString                             m_contextItemSource;
        QString                             m_currentCategory;
        bool                                m_removeTestcase;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
