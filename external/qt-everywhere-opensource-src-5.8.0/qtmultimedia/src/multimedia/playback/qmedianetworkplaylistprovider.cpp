/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "qmedianetworkplaylistprovider_p.h"
#include "qmediaplaylistprovider_p.h"
#include "qmediacontent.h"
#include "qmediaobject_p.h"
#include "playlistfileparser_p.h"

QT_BEGIN_NAMESPACE

class QMediaNetworkPlaylistProviderPrivate: public QMediaPlaylistProviderPrivate
{
    Q_DECLARE_NON_CONST_PUBLIC(QMediaNetworkPlaylistProvider)
public:
    bool load(const QNetworkRequest &request);

    QPlaylistFileParser parser;
    QList<QMediaContent> resources;

    void _q_handleParserError(QPlaylistFileParser::ParserError err, const QString &);
    void _q_handleNewItem(const QVariant& content);

    QMediaNetworkPlaylistProvider *q_ptr;
};

bool QMediaNetworkPlaylistProviderPrivate::load(const QNetworkRequest &request)
{
    parser.stop();
    parser.start(request, false);

    return true;
}

void QMediaNetworkPlaylistProviderPrivate::_q_handleParserError(QPlaylistFileParser::ParserError err, const QString &errorMessage)
{
    Q_Q(QMediaNetworkPlaylistProvider);

    QMediaPlaylist::Error playlistError = QMediaPlaylist::NoError;

    switch (err) {
    case QPlaylistFileParser::NoError:
        return;
    case QPlaylistFileParser::FormatError:
        playlistError = QMediaPlaylist::FormatError;
        break;
    case QPlaylistFileParser::FormatNotSupportedError:
        playlistError = QMediaPlaylist::FormatNotSupportedError;
        break;
    case QPlaylistFileParser::NetworkError:
        playlistError = QMediaPlaylist::NetworkError;
        break;
    }

    parser.stop();

    emit q->loadFailed(playlistError, errorMessage);
}

void QMediaNetworkPlaylistProviderPrivate::_q_handleNewItem(const QVariant& content)
{
    Q_Q(QMediaNetworkPlaylistProvider);

    QUrl url;
    if (content.type() == QVariant::Url) {
        url = content.toUrl();
    } else if (content.type() == QVariant::Map) {
        url = content.toMap()[QLatin1String("url")].toUrl();
    } else {
        return;
    }

    q->addMedia(QMediaContent(url));
}

QMediaNetworkPlaylistProvider::QMediaNetworkPlaylistProvider(QObject *parent)
    :QMediaPlaylistProvider(*new QMediaNetworkPlaylistProviderPrivate, parent)
{
    d_func()->q_ptr = this;
    connect(&d_func()->parser, SIGNAL(newItem(QVariant)),
            this, SLOT(_q_handleNewItem(QVariant)));
    connect(&d_func()->parser, SIGNAL(finished()), this, SIGNAL(loaded()));
    connect(&d_func()->parser, SIGNAL(error(QPlaylistFileParser::ParserError,QString)),
            this, SLOT(_q_handleParserError(QPlaylistFileParser::ParserError,QString)));
}

QMediaNetworkPlaylistProvider::~QMediaNetworkPlaylistProvider()
{
}

bool QMediaNetworkPlaylistProvider::isReadOnly() const
{
    return false;
}

bool QMediaNetworkPlaylistProvider::load(const QNetworkRequest &request, const char *format)
{
    Q_UNUSED(format);
    return d_func()->load(request);
}

int QMediaNetworkPlaylistProvider::mediaCount() const
{
    return d_func()->resources.size();
}

QMediaContent QMediaNetworkPlaylistProvider::media(int pos) const
{
    return d_func()->resources.value(pos);
}

bool QMediaNetworkPlaylistProvider::addMedia(const QMediaContent &content)
{
    Q_D(QMediaNetworkPlaylistProvider);

    int pos = d->resources.count();

    emit mediaAboutToBeInserted(pos, pos);
    d->resources.append(content);
    emit mediaInserted(pos, pos);

    return true;
}

bool QMediaNetworkPlaylistProvider::addMedia(const QList<QMediaContent> &items)
{
    Q_D(QMediaNetworkPlaylistProvider);

    if (items.isEmpty())
        return true;

    int pos = d->resources.count();
    int end = pos+items.count()-1;

    emit mediaAboutToBeInserted(pos, end);
    d->resources.append(items);
    emit mediaInserted(pos, end);

    return true;
}


bool QMediaNetworkPlaylistProvider::insertMedia(int pos, const QMediaContent &content)
{
    Q_D(QMediaNetworkPlaylistProvider);

    emit mediaAboutToBeInserted(pos, pos);
    d->resources.insert(pos, content);
    emit mediaInserted(pos,pos);

    return true;
}

bool QMediaNetworkPlaylistProvider::insertMedia(int pos, const QList<QMediaContent> &items)
{
    Q_D(QMediaNetworkPlaylistProvider);

    if (items.isEmpty())
        return true;

    const int last = pos+items.count()-1;

    emit mediaAboutToBeInserted(pos, last);
    for (int i=0; i<items.count(); i++)
        d->resources.insert(pos+i, items.at(i));
    emit mediaInserted(pos, last);

    return true;
}

bool QMediaNetworkPlaylistProvider::moveMedia(int from, int to)
{
    Q_D(QMediaNetworkPlaylistProvider);

    Q_ASSERT(from >= 0 && from < mediaCount());
    Q_ASSERT(to >= 0 && to < mediaCount());

    if (from == to)
        return false;

    const QMediaContent media = d->resources.at(from);
    return removeMedia(from, from) && insertMedia(to, media);
}

bool QMediaNetworkPlaylistProvider::removeMedia(int fromPos, int toPos)
{
    Q_D(QMediaNetworkPlaylistProvider);

    Q_ASSERT(fromPos >= 0);
    Q_ASSERT(fromPos <= toPos);
    Q_ASSERT(toPos < mediaCount());

    emit mediaAboutToBeRemoved(fromPos, toPos);
    d->resources.erase(d->resources.begin()+fromPos, d->resources.begin()+toPos+1);
    emit mediaRemoved(fromPos, toPos);

    return true;
}

bool QMediaNetworkPlaylistProvider::removeMedia(int pos)
{
    Q_D(QMediaNetworkPlaylistProvider);

    emit mediaAboutToBeRemoved(pos, pos);
    d->resources.removeAt(pos);
    emit mediaRemoved(pos, pos);

    return true;
}

bool QMediaNetworkPlaylistProvider::clear()
{
    Q_D(QMediaNetworkPlaylistProvider);
    if (!d->resources.isEmpty()) {
        int lastPos = mediaCount()-1;
        emit mediaAboutToBeRemoved(0, lastPos);
        d->resources.clear();
        emit mediaRemoved(0, lastPos);
    }

    return true;
}

void QMediaNetworkPlaylistProvider::shuffle()
{
    Q_D(QMediaNetworkPlaylistProvider);
    if (!d->resources.isEmpty()) {
        QList<QMediaContent> resources;

        while (!d->resources.isEmpty()) {
            resources.append(d->resources.takeAt(qrand() % d->resources.size()));
        }

        d->resources = resources;
        emit mediaChanged(0, mediaCount()-1);
    }

}

#include "moc_qmedianetworkplaylistprovider_p.cpp"

QT_END_NAMESPACE

