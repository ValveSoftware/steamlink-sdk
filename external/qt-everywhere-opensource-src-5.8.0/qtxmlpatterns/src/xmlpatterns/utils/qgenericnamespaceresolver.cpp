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

#include "qnamepool_p.h"

#include "qgenericnamespaceresolver_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

GenericNamespaceResolver::GenericNamespaceResolver(const Bindings &list) : m_bindings(list)
{
}

void GenericNamespaceResolver::addBinding(const QXmlName nb)
{
    if(nb.namespaceURI() == StandardNamespaces::UndeclarePrefix)
        m_bindings.remove(nb.prefix());
    else
        m_bindings.insert(nb.prefix(), nb.namespaceURI());
}

QXmlName::NamespaceCode GenericNamespaceResolver::lookupNamespaceURI(const QXmlName::PrefixCode prefix) const
{
    return m_bindings.value(prefix, NoBinding);
}

NamespaceResolver::Ptr GenericNamespaceResolver::defaultXQueryBindings()
{
    Bindings list;

    list.insert(StandardPrefixes::xml,    StandardNamespaces::xml);
    list.insert(StandardPrefixes::xs,     StandardNamespaces::xs);
    list.insert(StandardPrefixes::xsi,    StandardNamespaces::xsi);
    list.insert(StandardPrefixes::fn,     StandardNamespaces::fn);
    list.insert(StandardPrefixes::local,  StandardNamespaces::local);
    list.insert(StandardPrefixes::empty,  StandardNamespaces::empty);

    return NamespaceResolver::Ptr(new GenericNamespaceResolver(list));
}

NamespaceResolver::Ptr GenericNamespaceResolver::defaultXSLTBindings()
{
    Bindings list;

    list.insert(StandardPrefixes::xml,    StandardNamespaces::xml);
    list.insert(StandardPrefixes::empty,  StandardNamespaces::empty);

    return NamespaceResolver::Ptr(new GenericNamespaceResolver(list));
}

NamespaceResolver::Bindings GenericNamespaceResolver::bindings() const
{
    return m_bindings;
}

QT_END_NAMESPACE
