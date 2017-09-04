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

#include <QCoreApplication>
#include <QFile>
#include <QtDebug>
#include <QXmlResultItems>
#include <QXmlNamePool>

#include "Global.h"

#include <private/qcommonsequencetypes_p.h>
#include <private/qxmldebug_p.h>
#include <private/qatomicstring_p.h>

#include "ExternalSourceLoader.h"

using namespace QPatternistSDK;
using namespace QPatternist;

ExternalSourceLoader::ExternalSourceLoader(const VariableMap &varMap,
                                           const QPatternist::ResourceLoader::Ptr &r) : m_variableMap(varMap)
                                                                                      , m_resourceLoader(r)
                                                                                      , m_query(Global::namePoolAsPublic())
{
    Q_ASSERT(m_resourceLoader);
}

QPatternist::SequenceType::Ptr
ExternalSourceLoader::announceExternalVariable(const QXmlName name,
                                               const QPatternist::SequenceType::Ptr &declaredType)
{
    pDebug() << "ExternalSourceLoader::announceExternalVariable()";
    Q_ASSERT(!name.isNull());
    Q_ASSERT(declaredType);
    Q_UNUSED(declaredType); /* Needed when bulding in release mode. */

    if(name.namespaceURI() == QPatternist::StandardNamespaces::empty)
    {
        const VariableValue variable(m_variableMap.value(Global::namePool()->stringForLocalName(name.localName())));

        if(variable.first.isEmpty())
            return QPatternist::SequenceType::Ptr();
        else
        {
            /* If announceDocument() can't load a document for uriForVar, it will return
             * null, which we will too, which is fine, since we can't supply a value for
             * this variable then. */
            if(variable.second == Document)
                return m_resourceLoader->announceDocument(variable.first, QPatternist::ResourceLoader::WillUse);
            else if(variable.second == URI)
            {
                return QPatternist::CommonSequenceTypes::ExactlyOneString;
            }
            else
            {
                /* The type is Query, and we don't pre-load
                 * them. No particular reason, just not worth it. */
                return QPatternist::CommonSequenceTypes::ZeroOrMoreItems;
            }
        }
    }
    else
        return QPatternist::SequenceType::Ptr();
}

QPatternist::Item
ExternalSourceLoader::evaluateSingleton(const QXmlName name,
                                        const QPatternist::DynamicContext::Ptr &context)
{
    Q_ASSERT(!name.isNull());
    const VariableValue variable(m_variableMap.value(Global::namePool()->stringForLocalName(name.localName())));

    if(variable.second == Document)
    {
        Q_ASSERT_X(QFile::exists(variable.first.toLocalFile()), Q_FUNC_INFO,
                   qPrintable(QString::fromLatin1("The file %1 doesn't exist").arg(variable.first.toLocalFile())));
        Q_ASSERT_X(m_resourceLoader->openDocument(variable.first, context), Q_FUNC_INFO,
                   "We're supposed to have the value. If not, an error should have been issued at query compile time.");

        return m_resourceLoader->openDocument(variable.first, context);
    }
    else if(variable.second == Query)
    {
        /* Well, here we open the file and execute it. */
        m_query.setQuery(QUrl::fromLocalFile(variable.first.toLocalFile()));
        Q_ASSERT(m_query.isValid());

        QXmlResultItems result;
        m_query.evaluateTo(&result);

        return QPatternist::Item::fromPublic(result.next());
    }
    else
    {
        Q_ASSERT(variable.second == URI);
        return QPatternist::AtomicString::fromValue(variable.first.toString());
    }
}

// vim: et:ts=4:sw=4:sts=4

