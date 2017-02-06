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

#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include "qiodevicedelegate_p.h"
#include "quriloader_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

URILoader::URILoader(QObject *const parent,
                     const NamePool::Ptr &np,
                     const VariableLoader::Ptr &l) : QNetworkAccessManager(parent)
                                                   , m_variableNS(QLatin1String("tag:trolltech.com,2007:QtXmlPatterns:QIODeviceVariable:"))
                                                   , m_namePool(np)
                                                   , m_variableLoader(l)
{
    Q_ASSERT(m_variableLoader);
}

QNetworkReply *URILoader::createRequest(Operation op, const QNetworkRequest &req, QIODevice *outgoingData)
{
    const QString requestedUrl(req.url().toString());

    /* We got a QIODevice variable. */
    const QString name(requestedUrl.right(requestedUrl.length() - m_variableNS.length()));

    const QVariant variant(m_variableLoader->valueFor(m_namePool->allocateQName(QString(), name, QString())));

    if(!variant.isNull() && variant.userType() == qMetaTypeId<QIODevice *>())
        return new QIODeviceDelegate(qvariant_cast<QIODevice *>(variant));
    else
    {
        /* If we're entering this code path, the variable URI identified a variable
         * which we don't have, which means we either have a bug, or the user had
         * crafted an invalid URI manually. */

        return QNetworkAccessManager::createRequest(op, req, outgoingData);
    }
}

QT_END_NAMESPACE

