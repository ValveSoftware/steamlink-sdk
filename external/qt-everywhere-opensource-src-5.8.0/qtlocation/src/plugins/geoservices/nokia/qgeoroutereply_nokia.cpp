/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
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
