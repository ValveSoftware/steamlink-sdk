/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
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
** This file is part of the Nokia services plugin for the Maps and
** Navigation API.  The use of these services, whether by use of the
** plugin or by other means, is governed by the terms and conditions
** described by the file NOKIA_TERMS_AND_CONDITIONS.txt in
** this package, located in the directory containing the Nokia services
** plugin source code.
**
****************************************************************************/

#include "qgeoroutereply_nokia.h"
#include "qgeoroutexmlparser.h"
#include "qgeoerror_messages.h"

#include <qgeorouterequest.h>

#include <QtCore/QCoreApplication>

Q_DECLARE_METATYPE(QList<QGeoRoute>)

QT_BEGIN_NAMESPACE

QGeoRouteReplyNokia::QGeoRouteReplyNokia(const QGeoRouteRequest &request,
                                         const QList<QNetworkReply *> &replies,
                                         QObject *parent)
:   QGeoRouteReply(request, parent), m_replies(replies), m_parsers(0)
{
    qRegisterMetaType<QList<QGeoRoute> >();

    foreach (QNetworkReply *reply, m_replies) {
        connect(reply, SIGNAL(finished()), this, SLOT(networkFinished()));
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(networkError(QNetworkReply::NetworkError)));
    }
}

QGeoRouteReplyNokia::~QGeoRouteReplyNokia()
{
    abort();
}

void QGeoRouteReplyNokia::abort()
{
    if (m_replies.isEmpty() && !m_parsers)
        return;

    foreach (QNetworkReply *reply, m_replies) {
        reply->abort();
        reply->deleteLater();
    }
    m_replies.clear();
    m_parsers = 0;
}

void QGeoRouteReplyNokia::networkFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    if (reply->error() != QNetworkReply::NoError)
        return;

    QGeoRouteXmlParser *parser = new QGeoRouteXmlParser(request());
    connect(parser, SIGNAL(results(QList<QGeoRoute>)),
            this, SLOT(appendResults(QList<QGeoRoute>)));
    connect(parser, SIGNAL(error(QString)), this, SLOT(parserError(QString)));

    ++m_parsers;
    parser->parse(reply->readAll());

    m_replies.removeOne(reply);
    reply->deleteLater();
}

void QGeoRouteReplyNokia::networkError(QNetworkReply::NetworkError error)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    if (error == QNetworkReply::UnknownContentError) {
        QGeoRouteXmlParser *parser = new QGeoRouteXmlParser(request());
        connect(parser, SIGNAL(results(QList<QGeoRoute>)),
                this, SLOT(appendResults(QList<QGeoRoute>)));
        connect(parser, SIGNAL(error(QString)), this, SLOT(parserError(QString)));

        ++m_parsers;
        parser->parse(reply->readAll());

        m_replies.removeOne(reply);
        reply->deleteLater();
    } else {
        setError(QGeoRouteReply::CommunicationError, reply->errorString());
        abort();
    }
}

void QGeoRouteReplyNokia::appendResults(const QList<QGeoRoute> &routes)
{
    if (!m_parsers)
        return;

    --m_parsers;
    addRoutes(routes);

    if (!m_parsers && m_replies.isEmpty())
        setFinished(true);
}

void QGeoRouteReplyNokia::parserError(const QString &errorString)
{
    Q_UNUSED(errorString)

    --m_parsers;

    setError(QGeoRouteReply::ParseError,
             QCoreApplication::translate(NOKIA_PLUGIN_CONTEXT_NAME, RESPONSE_NOT_RECOGNIZABLE));
    abort();
}

QT_END_NAMESPACE
