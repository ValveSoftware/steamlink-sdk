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

#include <QtCore/qurl.h>
#include <QtCore/qvariant.h>
#include <QtCore/QPointer>

#include <qmediaplaylist.h>
#include "qmediacontent.h"

QT_BEGIN_NAMESPACE

static void qRegisterMediaContentMetaTypes()
{
    qRegisterMetaType<QMediaContent>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterMediaContentMetaTypes)


class QMediaContentPrivate : public QSharedData
{
public:
    QMediaContentPrivate():
        isPlaylistOwned(false)
    {}

    QMediaContentPrivate(const QMediaResourceList &r):
        resources(r),
        isPlaylistOwned(false)
    {}

    QMediaContentPrivate(const QMediaContentPrivate &other):
        QSharedData(other),
        resources(other.resources),
        playlist(other.playlist),
        isPlaylistOwned(false)
    {}

    QMediaContentPrivate(QMediaPlaylist *pls, const QUrl &url, bool isOwn):
        playlist(pls),
        isPlaylistOwned(isOwn)
    {
        resources << QMediaResource(url);
    }

    ~QMediaContentPrivate()
    {
        if (isPlaylistOwned && !playlist.isNull())
            playlist.data()->deleteLater();
    }

    bool operator ==(const QMediaContentPrivate &other) const
    {
        return resources == other.resources && playlist == other.playlist;
    }

    QMediaResourceList resources;

    QPointer<QMediaPlaylist> playlist;
    bool isPlaylistOwned;
private:
    QMediaContentPrivate& operator=(const QMediaContentPrivate &other);
};


/*!
    \class QMediaContent

    \brief The QMediaContent class provides access to the resources relating to a media content.

    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_playback

    QMediaContent is used within the multimedia framework as the logical handle
    to media content.  A QMediaContent object is composed of one or more
    \l {QMediaResource}s where each resource provides the URL and format
    information of a different encoding of the content.

    A non-null QMediaContent will always have a primary or canonical reference to
    the content available through the canonicalUrl() or canonicalResource()
    methods, any additional resources are optional.

    Alternatively QMediaContent can represent a playlist and contain a pointer to a
    valid QMediaPlaylist object. In this case URL is optional and can either be empty
    or point to the playlist URL.
*/


/*!
    Constructs a null QMediaContent.
*/

QMediaContent::QMediaContent()
{
}

/*!
    Constructs a media content with \a url providing a reference to the content.
*/

QMediaContent::QMediaContent(const QUrl &url):
    d(new QMediaContentPrivate)
{
    d->resources << QMediaResource(url);
}

/*!
    Constructs a media content with \a request providing a reference to the content.

    This constructor can be used to reference media content via network protocols such as HTTP.
    This may include additional information required to obtain the resource, such as Cookies or HTTP headers.
*/

QMediaContent::QMediaContent(const QNetworkRequest &request):
    d(new QMediaContentPrivate)
{
    d->resources << QMediaResource(request);
}

/*!
    Constructs a media content with \a resource providing a reference to the content.
*/

QMediaContent::QMediaContent(const QMediaResource &resource):
    d(new QMediaContentPrivate)
{
    d->resources << resource;
}

/*!
    Constructs a media content with \a resources providing a reference to the content.
*/

QMediaContent::QMediaContent(const QMediaResourceList &resources):
    d(new QMediaContentPrivate(resources))
{
}

/*!
    Constructs a copy of the media content \a other.
*/

QMediaContent::QMediaContent(const QMediaContent &other):
    d(other.d)
{
}

/*!
    Constructs a media content with \a playlist.

    \a contentUrl of a playlist is an optional parameter and can be empty.

    Set \a takeOwnership to true if you want QMediaContent to take ownership of the playlist.
    \a takeOwnership is set to false by default.
*/

QMediaContent::QMediaContent(QMediaPlaylist *playlist, const QUrl &contentUrl, bool takeOwnership):
    d(new QMediaContentPrivate(playlist, contentUrl, takeOwnership))
{
}

/*!
    Destroys the media content object.
*/

QMediaContent::~QMediaContent()
{
}

/*!
    Assigns the value of \a other to this media content.
*/

QMediaContent& QMediaContent::operator=(const QMediaContent &other)
{
    d = other.d;
    return *this;
}

/*!
    Returns true if \a other is equivalent to this media content; false otherwise.
*/

bool QMediaContent::operator==(const QMediaContent &other) const
{
    return (d.constData() == 0 && other.d.constData() == 0) ||
            (d.constData() != 0 && other.d.constData() != 0 &&
             *d.constData() == *other.d.constData());
}

/*!
    Returns true if \a other is not equivalent to this media content; false otherwise.
*/

bool QMediaContent::operator!=(const QMediaContent &other) const
{
    return !(*this == other);
}

/*!
    Returns true if this media content is null (uninitialized); false otherwise.
*/

bool QMediaContent::isNull() const
{
    return d.constData() == 0;
}

/*!
    Returns a QUrl that represents that canonical resource for this media content.
*/

QUrl QMediaContent::canonicalUrl() const
{
    return canonicalResource().url();
}

/*!
    Returns a QNetworkRequest that represents that canonical resource for this media content.
*/

QNetworkRequest QMediaContent::canonicalRequest() const
{
    return canonicalResource().request();
}

/*!
    Returns a QMediaResource that represents that canonical resource for this media content.
*/

QMediaResource QMediaContent::canonicalResource() const
{
    return d.constData() != 0
            ?  d->resources.value(0)
            : QMediaResource();
}

/*!
    Returns a list of alternative resources for this media content.  The first item in this list
    is always the canonical resource.
*/

QMediaResourceList QMediaContent::resources() const
{
    return d.constData() != 0
            ? d->resources
            : QMediaResourceList();
}

/*!
    Returns a playlist for this media content or 0 if this QMediaContent is not a playlist.
*/

QMediaPlaylist *QMediaContent::playlist() const
{
    return d.constData() != 0
            ? d->playlist.data()
            : 0;
}

QT_END_NAMESPACE

