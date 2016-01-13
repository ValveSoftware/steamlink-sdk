/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#include "qnamespacesupport_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

NamespaceSupport::NamespaceSupport()
{
}

NamespaceSupport::NamespaceSupport(const NamePool::Ptr &namePool)
    : m_namePool(namePool)
{
    // the XML namespace
    m_ns.insert(StandardPrefixes::xml, StandardNamespaces::xml);
}

void NamespaceSupport::setPrefix(const QXmlName::PrefixCode prefixCode, const QXmlName::NamespaceCode namespaceCode)
{
    m_ns.insert(prefixCode, namespaceCode);
}

void NamespaceSupport::setPrefixes(const QXmlStreamNamespaceDeclarations &declarations)
{
    for (int i = 0; i < declarations.count(); i++) {
        const QXmlStreamNamespaceDeclaration declaration = declarations.at(i);

        const QXmlName::PrefixCode prefixCode = m_namePool->allocatePrefix(declaration.prefix().toString());
        const QXmlName::NamespaceCode namespaceCode = m_namePool->allocateNamespace(declaration.namespaceUri().toString());
        m_ns.insert(prefixCode, namespaceCode);
    }
}

void NamespaceSupport::setTargetNamespace(const QXmlName::NamespaceCode namespaceCode)
{
    m_ns.insert(0, namespaceCode);
}

QXmlName::PrefixCode NamespaceSupport::prefix(const QXmlName::NamespaceCode namespaceCode) const
{
    NamespaceHash::const_iterator itc, it = m_ns.constBegin();
    while ((itc=it) != m_ns.constEnd()) {
        ++it;
        if (*itc == namespaceCode)
            return itc.key();
    }
    return 0;
}

QXmlName::NamespaceCode NamespaceSupport::uri(const QXmlName::PrefixCode prefixCode) const
{
    return m_ns.value(prefixCode);
}

bool NamespaceSupport::processName(const QString& qname, NameType type, QXmlName &name) const
{
    int len = qname.size();
    const QChar *data = qname.constData();
    for (int pos = 0; pos < len; ++pos) {
        if (data[pos] == QLatin1Char(':')) {
            const QXmlName::PrefixCode prefixCode = m_namePool->allocatePrefix(qname.left(pos));
            if (!m_ns.contains(prefixCode))
                return false;
            const QXmlName::NamespaceCode namespaceCode = uri(prefixCode);
            const QXmlName::LocalNameCode localNameCode = m_namePool->allocateLocalName(qname.mid(pos + 1));
            name = QXmlName(namespaceCode, localNameCode, prefixCode);
            return true;
        }
    }

    // there was no ':'
    QXmlName::NamespaceCode namespaceCode = 0;
    // attributes don't take default namespace
    if (type == ElementName && !m_ns.isEmpty()) {
        namespaceCode = m_ns.value(0);   // get default namespace
    }

    const QXmlName::LocalNameCode localNameCode = m_namePool->allocateLocalName(qname);
    name = QXmlName(namespaceCode, localNameCode, 0);

    return true;
}

void NamespaceSupport::pushContext()
{
    m_nsStack.push(m_ns);
}

void NamespaceSupport::popContext()
{
    m_ns.clear();
    if(!m_nsStack.isEmpty())
        m_ns = m_nsStack.pop();
}

QList<QXmlName> NamespaceSupport::namespaceBindings() const
{
    QList<QXmlName> bindings;

    QHashIterator<QXmlName::PrefixCode, QXmlName::NamespaceCode> it(m_ns);
    while (it.hasNext()) {
        it.next();
        bindings.append(QXmlName(it.value(), StandardLocalNames::empty, it.key()));
    }

    return bindings;
}

QT_END_NAMESPACE
