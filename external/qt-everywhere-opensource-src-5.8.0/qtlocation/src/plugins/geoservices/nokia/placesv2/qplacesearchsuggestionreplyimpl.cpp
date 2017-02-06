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

#include "qplacesearchsuggestionreplyimpl.h"
#include "../qgeoerror_messages.h"

#include <QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

QT_BEGIN_NAMESPACE

QPlaceSearchSuggestionReplyImpl::QPlaceSearchSuggestionReplyImpl(QNetworkReply *reply,
                                                                 QObject *parent)
:   QPlaceSearchSuggestionReply(parent), m_reply(reply)
{
    if (!m_reply)
        return;

    m_reply->setParent(this);
    connect(m_reply, SIGNAL(finished()), this, SLOT(replyFinished()));
}

QPlaceSearchSuggestionReplyImpl::~QPlaceSearchSuggestionReplyImpl()
{
}

void QPlaceSearchSuggestionReplyImpl::abort()
{
    if (m_reply)
        m_reply->abort();
}

void QPlaceSearchSuggestionReplyImpl::setError(QPlaceReply::Error error_,
                                               const QString &errorString)
{
    QPlaceReply::setError(error_, errorString);
    emit error(error_, errorString);
    setFinished(true);
    emit finished();
}

void QPlaceSearchSuggestionReplyImpl::replyFinished()
{
    if (m_reply->error() != QNetworkReply::NoError) {
        switch (m_reply->error()) {
        case QNetworkReply::OperationCanceledError:
            setError(CancelError, "Request canceled.");
            break;
        default:
            setError(CommunicationError, "Network error.");
        }
        return;
    }

    QJsonDocument document = QJsonDocument::fromJson(m_reply->readAll());
    if (!document.isObject()) {
        setError(ParseError, QCoreApplication::translate(NOKIA_PLUGIN_CONTEXT_NAME, PARSE_ERROR));
        emit error(error(), errorString());
        return;
    }

    QJsonObject object = document.object();

    QJsonArray suggestions = object.value(QStringLiteral("suggestions")).toArray();

    QStringList s;
    for (int i = 0; i < suggestions.count(); ++i) {
        QJsonValue v = suggestions.at(i);
        if (v.isString())
            s.append(v.toString());
    }

    setSuggestions(s);

    m_reply->deleteLater();
    m_reply = 0;

    setFinished(true);
    emit finished();
}

QT_END_NAMESPACE
