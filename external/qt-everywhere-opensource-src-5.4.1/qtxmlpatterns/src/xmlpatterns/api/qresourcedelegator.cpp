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

#include "qresourcedelegator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

bool ResourceDelegator::isUnparsedTextAvailable(const QUrl &uri,
                                                const QString &encoding)
{
    return m_parentLoader->isUnparsedTextAvailable(uri, encoding);
}

ItemType::Ptr ResourceDelegator::announceUnparsedText(const QUrl &uri)
{
    return m_parentLoader->announceUnparsedText(uri);
}

Item ResourceDelegator::openUnparsedText(const QUrl &uri,
                                         const QString &encoding,
                                         const ReportContext::Ptr &context,
                                         const SourceLocationReflection *const where)
{
    return m_parentLoader->openUnparsedText(uri, encoding, context, where);
}

Item ResourceDelegator::openDocument(const QUrl &uri,
                                     const ReportContext::Ptr &context)
{
    if(m_needsOverride.contains(uri))
        return m_forDeviceLoader->openDocument(uri, context);
    else
        return m_parentLoader->openDocument(uri, context);
}

SequenceType::Ptr ResourceDelegator::announceDocument(const QUrl &uri, const Usage usageHint)
{
    return m_parentLoader->announceDocument(uri, usageHint);
}

bool ResourceDelegator::isDocumentAvailable(const QUrl &uri)
{
    return m_parentLoader->isDocumentAvailable(uri);
}

Item::Iterator::Ptr ResourceDelegator::openCollection(const QUrl &uri)
{
    return m_parentLoader->openCollection(uri);
}

SequenceType::Ptr ResourceDelegator::announceCollection(const QUrl &uri)
{
    return m_parentLoader->announceCollection(uri);
}

QSet<QUrl> ResourceDelegator::deviceURIs() const
{
    QSet<QUrl> uris(m_needsOverride);
    uris |= m_forDeviceLoader->deviceURIs();
    return uris;
}

QT_END_NAMESPACE
