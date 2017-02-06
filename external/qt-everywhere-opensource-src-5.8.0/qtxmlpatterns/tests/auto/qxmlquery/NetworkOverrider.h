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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#ifndef NetworkOverrider_h
#define NetworkOverrider_h

#include <QtNetwork/QNetworkAccessManager>

/*!
 \class MessageSilencer
 \internal
 \since 4.5
 \brief A message handler for QXmlQuery that simply discards the messages, such
 that they aren't printed to \c stderr.
 */
class NetworkOverrider : public QNetworkAccessManager
{
public:
    NetworkOverrider(const QUrl &rewriteFrom,
                     const QUrl &rewriteTo);

    virtual QNetworkReply *createRequest(Operation op,
                                         const QNetworkRequest &req,
                                         QIODevice *outgoingData);
    bool isValid() const;

private:
    const QUrl m_rewriteFrom;
    const QUrl m_rewriteTo;
};

NetworkOverrider::NetworkOverrider(const QUrl &rewriteFrom,
                                   const QUrl &rewriteTo)
    : m_rewriteFrom(rewriteFrom)
    , m_rewriteTo(rewriteTo)
{
}

QNetworkReply *NetworkOverrider::createRequest(Operation op,
                                               const QNetworkRequest &req,
                                               QIODevice *outgoingData)
{
    QNetworkRequest newReq(req);

    if(req.url() == m_rewriteFrom)
        newReq.setUrl(m_rewriteTo);

    return QNetworkAccessManager::createRequest(op, newReq, outgoingData);
}

bool NetworkOverrider::isValid() const
{
    return m_rewriteFrom.isValid() && m_rewriteTo.isValid();
}
#endif
