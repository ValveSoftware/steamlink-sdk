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
