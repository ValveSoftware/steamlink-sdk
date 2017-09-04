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

#ifndef PatternistSDK_ExternalSourceLoader_H
#define PatternistSDK_ExternalSourceLoader_H

#include <QHash>
#include <QUrl>
#include <QXmlQuery>

#include <private/qdynamiccontext_p.h>
#include <private/qresourceloader_p.h>
#include <private/qexternalvariableloader_p.h>

QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    /**
     * @short Handles external variables in XQTS queries, such as <tt>$input-context</tt>,
     * by loading appropriate XML source files.
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT ExternalSourceLoader : public QPatternist::ExternalVariableLoader
    {
    public:
        enum TargetOfURI
        {
            /**
             * Identifies @c input-file.
             */
            Document,

            /**
             * Identifies @c input-URI.
             */
            URI,

            /**
             * Identifies @c input-query.
             */
            Query
        };

        /**
         * The first is the complete, absolute, final URI to the file to be loaded,
         * and the second is the type of source found at that URI.
         */
        typedef QPair<QUrl, TargetOfURI> VariableValue;

        /**
         * In the XQTSCatalog.xml each source file in each test is referred to
         * by a key, which can be fully looked up in the @c sources element. This QHash
         * maps the keys to absolute URIs pointing to the source file.
         */
        typedef QHash<QString, QUrl> SourceMap;

        /**
         * The first value is the variable name, and the second is the URI identifying
         * the XML source file that's supposed to be loaded as a document.
         *
         * This is one for every test case, except for @c rdb-queries-results-q5,
         * @c rdb-queries-results-q17 and @c rdb-queries-results-q18(at least in XQTS 1.0).
         */
        typedef QHash<QString, VariableValue> VariableMap;

        ExternalSourceLoader(const VariableMap &varMap,
                             const QPatternist::ResourceLoader::Ptr &resourceLoader);

        virtual QPatternist::SequenceType::Ptr
        announceExternalVariable(const QXmlName name,
                                 const QPatternist::SequenceType::Ptr &declaredType);

        virtual QPatternist::Item
        evaluateSingleton(const QXmlName name,
                          const QPatternist::DynamicContext::Ptr &context);

        VariableMap variableMap() const
        {
            return m_variableMap;
        }

    private:
        const VariableMap                       m_variableMap;
        const QPatternist::ResourceLoader::Ptr  m_resourceLoader;
        QXmlQuery                               m_query;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
