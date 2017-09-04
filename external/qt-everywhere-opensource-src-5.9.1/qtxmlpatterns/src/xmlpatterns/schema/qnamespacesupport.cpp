/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
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

    for (auto it = m_ns.cbegin(), end = m_ns.cend(); it != end; ++it)
        bindings.append(QXmlName(it.value(), StandardLocalNames::empty, it.key()));

    return bindings;
}

QT_END_NAMESPACE
