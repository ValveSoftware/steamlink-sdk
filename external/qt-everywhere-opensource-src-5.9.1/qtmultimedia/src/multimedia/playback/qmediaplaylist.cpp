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

#include "qmediaplaylist.h"
#include "qmediaplaylist_p.h"
#include "qmediaplaylistprovider_p.h"
#include "qmediaplaylistioplugin_p.h"
#include "qmedianetworkplaylistprovider_p.h"
#include "qmediaservice.h"
#include "qmediaplaylistcontrol_p.h"
#include "qmediaplayercontrol.h"

#include <QtCore/qlist.h>
#include <QtCore/qfile.h>
#include <QtCore/qurl.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qcoreapplication.h>

#include "qmediapluginloader_p.h"

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC_WITH_ARGS(QMediaPluginLoader, playlistIOLoader,
        (QMediaPlaylistIOInterface_iid, QLatin1String("playlistformats"), Qt::CaseInsensitive))

static void qRegisterMediaPlaylistMetaTypes()
{
    qRegisterMetaType<QMediaPlaylist::Error>();
    qRegisterMetaType<QMediaPlaylist::PlaybackMode>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterMediaPlaylistMetaTypes)


/*!
    \class QMediaPlaylist
    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_playback


    \brief The QMediaPlaylist class provides a list of media content to play.

    QMediaPlaylist is intended to be used with other media objects,
    like QMediaPlayer.

    QMediaPlaylist allows to access the service intrinsic playlist functionality
    if available, otherwise it provides the local memory playlist implementation.

    \snippet multimedia-snippets/media.cpp Movie playlist

    Depending on playlist source implementation, most of the playlist mutating
    operations can be asynchronous.

    \sa QMediaContent
*/


/*!
    \enum QMediaPlaylist::PlaybackMode

    The QMediaPlaylist::PlaybackMode describes the order items in playlist are played.

    \value CurrentItemOnce    The current item is played only once.

    \value CurrentItemInLoop  The current item is played repeatedly in a loop.

    \value Sequential         Playback starts from the current and moves through each successive item until the last is reached and then stops.
                              The next item is a null item when the last one is currently playing.

    \value Loop               Playback restarts at the first item after the last has finished playing.

    \value Random             Play items in random order.
*/



/*!
  Create a new playlist object with the given \a parent.
*/

QMediaPlaylist::QMediaPlaylist(QObject *parent)
    : QObject(parent)
    , d_ptr(new QMediaPlaylistPrivate)
{
    Q_D(QMediaPlaylist);

    d->q_ptr = this;
    d->networkPlaylistControl = new QMediaNetworkPlaylistControl(this);

    setMediaObject(0);
}

/*!
  Destroys the playlist.
  */

QMediaPlaylist::~QMediaPlaylist()
{
    Q_D(QMediaPlaylist);

    if (d->mediaObject)
        d->mediaObject->unbind(this);

    delete d_ptr;
}

/*!
  Returns the QMediaObject instance that this QMediaPlaylist is bound too,
  or 0 otherwise.
*/
QMediaObject *QMediaPlaylist::mediaObject() const
{
    return d_func()->mediaObject;
}

/*!
  \internal
  If \a mediaObject is null or doesn't have an intrinsic playlist,
  internal local memory playlist source will be created.
*/
bool QMediaPlaylist::setMediaObject(QMediaObject *mediaObject)
{
    Q_D(QMediaPlaylist);

    if (mediaObject && mediaObject == d->mediaObject)
        return true;

    QMediaService *service = mediaObject
            ? mediaObject->service() : 0;

    QMediaPlaylistControl *newControl = 0;

    if (service)
        newControl = qobject_cast<QMediaPlaylistControl*>(service->requestControl(QMediaPlaylistControl_iid));

    if (!newControl)
        newControl = d->networkPlaylistControl;

    if (d->control != newControl) {
        int removedStart = -1;
        int removedEnd = -1;
        int insertedStart = -1;
        int insertedEnd = -1;

        if (d->control) {
            QMediaPlaylistProvider *playlist = d->control->playlistProvider();
            disconnect(playlist, SIGNAL(loadFailed(QMediaPlaylist::Error,QString)),
                    this, SLOT(_q_loadFailed(QMediaPlaylist::Error,QString)));

            disconnect(playlist, SIGNAL(mediaChanged(int,int)), this, SIGNAL(mediaChanged(int,int)));
            disconnect(playlist, SIGNAL(mediaAboutToBeInserted(int,int)), this, SIGNAL(mediaAboutToBeInserted(int,int)));
            disconnect(playlist, SIGNAL(mediaInserted(int,int)), this, SIGNAL(mediaInserted(int,int)));
            disconnect(playlist, SIGNAL(mediaAboutToBeRemoved(int,int)), this, SIGNAL(mediaAboutToBeRemoved(int,int)));
            disconnect(playlist, SIGNAL(mediaRemoved(int,int)), this, SIGNAL(mediaRemoved(int,int)));

            disconnect(playlist, SIGNAL(loaded()), this, SIGNAL(loaded()));

            disconnect(d->control, SIGNAL(playbackModeChanged(QMediaPlaylist::PlaybackMode)),
                    this, SIGNAL(playbackModeChanged(QMediaPlaylist::PlaybackMode)));
            disconnect(d->control, SIGNAL(currentIndexChanged(int)),
                    this, SIGNAL(currentIndexChanged(int)));
            disconnect(d->control, SIGNAL(currentMediaChanged(QMediaContent)),
                    this, SIGNAL(currentMediaChanged(QMediaContent)));

            // Copy playlist items, sync playback mode and sync current index between
            // old control and new control
            d->syncControls(d->control, newControl,
                            &removedStart, &removedEnd,
                            &insertedStart, &insertedEnd);

            if (d->mediaObject)
                d->mediaObject->service()->releaseControl(d->control);
        }

        d->control = newControl;
        QMediaPlaylistProvider *playlist = d->control->playlistProvider();
        connect(playlist, SIGNAL(loadFailed(QMediaPlaylist::Error,QString)),
                this, SLOT(_q_loadFailed(QMediaPlaylist::Error,QString)));

        connect(playlist, SIGNAL(mediaChanged(int,int)), this, SIGNAL(mediaChanged(int,int)));
        connect(playlist, SIGNAL(mediaAboutToBeInserted(int,int)), this, SIGNAL(mediaAboutToBeInserted(int,int)));
        connect(playlist, SIGNAL(mediaInserted(int,int)), this, SIGNAL(mediaInserted(int,int)));
        connect(playlist, SIGNAL(mediaAboutToBeRemoved(int,int)), this, SIGNAL(mediaAboutToBeRemoved(int,int)));
        connect(playlist, SIGNAL(mediaRemoved(int,int)), this, SIGNAL(mediaRemoved(int,int)));

        connect(playlist, SIGNAL(loaded()), this, SIGNAL(loaded()));

        connect(d->control, SIGNAL(playbackModeChanged(QMediaPlaylist::PlaybackMode)),
                this, SIGNAL(playbackModeChanged(QMediaPlaylist::PlaybackMode)));
        connect(d->control, SIGNAL(currentIndexChanged(int)),
                this, SIGNAL(currentIndexChanged(int)));
        connect(d->control, SIGNAL(currentMediaChanged(QMediaContent)),
                this, SIGNAL(currentMediaChanged(QMediaContent)));

        if (removedStart != -1 && removedEnd != -1) {
            emit mediaAboutToBeRemoved(removedStart, removedEnd);
            emit mediaRemoved(removedStart, removedEnd);
        }

        if (insertedStart != -1 && insertedEnd != -1) {
            emit mediaAboutToBeInserted(insertedStart, insertedEnd);
            emit mediaInserted(insertedStart, insertedEnd);
        }
    }

    d->mediaObject = mediaObject;

    return true;
}

/*!
  \property QMediaPlaylist::playbackMode

  This property defines the order that items in the playlist are played.

  \sa QMediaPlaylist::PlaybackMode
*/

QMediaPlaylist::PlaybackMode QMediaPlaylist::playbackMode() const
{
    return d_func()->control->playbackMode();
}

void QMediaPlaylist::setPlaybackMode(QMediaPlaylist::PlaybackMode mode)
{
    Q_D(QMediaPlaylist);
    d->control->setPlaybackMode(mode);
}

/*!
  Returns position of the current media content in the playlist.
*/
int QMediaPlaylist::currentIndex() const
{
    return d_func()->control->currentIndex();
}

/*!
  Returns the current media content.
*/

QMediaContent QMediaPlaylist::currentMedia() const
{
    return d_func()->playlist()->media(currentIndex());
}

/*!
  Returns the index of the item, which would be current after calling next()
  \a steps times.

  Returned value depends on the size of playlist, current position
  and playback mode.

  \sa QMediaPlaylist::playbackMode(), previousIndex()
*/
int QMediaPlaylist::nextIndex(int steps) const
{
    return d_func()->control->nextIndex(steps);
}

/*!
  Returns the index of the item, which would be current after calling previous()
  \a steps times.

  \sa QMediaPlaylist::playbackMode(), nextIndex()
*/

int QMediaPlaylist::previousIndex(int steps) const
{
    return d_func()->control->previousIndex(steps);
}


/*!
  Returns the number of items in the playlist.

  \sa isEmpty()
  */
int QMediaPlaylist::mediaCount() const
{
    return d_func()->playlist()->mediaCount();
}

/*!
  Returns true if the playlist contains no items, otherwise returns false.

  \sa mediaCount()
  */
bool QMediaPlaylist::isEmpty() const
{
    return mediaCount() == 0;
}

/*!
  Returns true if the playlist can be modified, otherwise returns false.

  \sa mediaCount()
  */
bool QMediaPlaylist::isReadOnly() const
{
    return d_func()->playlist()->isReadOnly();
}

/*!
  Returns the media content at \a index in the playlist.
*/

QMediaContent QMediaPlaylist::media(int index) const
{
    return d_func()->playlist()->media(index);
}

/*!
  Append the media \a content to the playlist.

  Returns true if the operation is successful, otherwise returns false.
  */
bool QMediaPlaylist::addMedia(const QMediaContent &content)
{
    return d_func()->control->playlistProvider()->addMedia(content);
}

/*!
  Append multiple media content \a items to the playlist.

  Returns true if the operation is successful, otherwise returns false.
  */
bool QMediaPlaylist::addMedia(const QList<QMediaContent> &items)
{
    return d_func()->control->playlistProvider()->addMedia(items);
}

/*!
  Insert the media \a content to the playlist at position \a pos.

  Returns true if the operation is successful, otherwise returns false.
*/

bool QMediaPlaylist::insertMedia(int pos, const QMediaContent &content)
{
    QMediaPlaylistProvider *playlist = d_func()->playlist();
    return playlist->insertMedia(qBound(0, pos, playlist->mediaCount()), content);
}

/*!
  Insert multiple media content \a items to the playlist at position \a pos.

  Returns true if the operation is successful, otherwise returns false.
*/

bool QMediaPlaylist::insertMedia(int pos, const QList<QMediaContent> &items)
{
    QMediaPlaylistProvider *playlist = d_func()->playlist();
    return playlist->insertMedia(qBound(0, pos, playlist->mediaCount()), items);
}

/*!
  Move the item from position \a from to position \a to.

  Returns true if the operation is successful, otherwise false.

  \since 5.7
*/
bool QMediaPlaylist::moveMedia(int from, int to)
{
    QMediaPlaylistProvider *playlist = d_func()->playlist();
    return playlist->moveMedia(qBound(0, from, playlist->mediaCount()),
                               qBound(0, to, playlist->mediaCount()));
}

/*!
  Remove the item from the playlist at position \a pos.

  Returns true if the operation is successful, otherwise return false.
  */
bool QMediaPlaylist::removeMedia(int pos)
{
    QMediaPlaylistProvider *playlist = d_func()->playlist();
    if (pos >= 0 && pos < playlist->mediaCount())
        return playlist->removeMedia(pos);
    else
        return false;
}

/*!
  Remove items in the playlist from \a start to \a end inclusive.

  Returns true if the operation is successful, otherwise return false.
  */
bool QMediaPlaylist::removeMedia(int start, int end)
{
    QMediaPlaylistProvider *playlist = d_func()->playlist();
    start = qMax(0, start);
    end = qMin(end, playlist->mediaCount() - 1);
    if (start <= end)
        return playlist->removeMedia(start, end);
    else
        return false;
}

/*!
  Remove all the items from the playlist.

  Returns true if the operation is successful, otherwise return false.
  */
bool QMediaPlaylist::clear()
{
    Q_D(QMediaPlaylist);
    return d->playlist()->clear();
}

bool QMediaPlaylistPrivate::readItems(QMediaPlaylistReader *reader)
{
    QList<QMediaContent> items;

    while (!reader->atEnd())
        items.append(reader->readItem());

    return playlist()->addMedia(items);
}

bool QMediaPlaylistPrivate::writeItems(QMediaPlaylistWriter *writer)
{
    for (int i=0; i<playlist()->mediaCount(); i++) {
        if (!writer->writeItem(playlist()->media(i)))
            return false;
    }
    writer->close();
    return true;
}

/*!
 * \internal
 * Copy playlist items, sync playback mode and sync current index between old control and new control
*/
void QMediaPlaylistPrivate::syncControls(QMediaPlaylistControl *oldControl, QMediaPlaylistControl *newControl,
                                         int *removedStart, int *removedEnd,
                                         int *insertedStart, int *insertedEnd)
{
    Q_ASSERT(oldControl != NULL && newControl != NULL);
    Q_ASSERT(removedStart != NULL && removedEnd != NULL
            && insertedStart != NULL && insertedEnd != NULL);

    QMediaPlaylistProvider *oldPlaylist = oldControl->playlistProvider();
    QMediaPlaylistProvider *newPlaylist = newControl->playlistProvider();

    Q_ASSERT(oldPlaylist != NULL && newPlaylist != NULL);

    *removedStart = -1;
    *removedEnd = -1;
    *insertedStart = -1;
    *insertedEnd = -1;

    if (newPlaylist->isReadOnly()) {
        // we can't transfer the items from the old control.
        // Report these items as removed.
        if (oldPlaylist->mediaCount() > 0) {
            *removedStart = 0;
            *removedEnd = oldPlaylist->mediaCount() - 1;
        }
        // The new control might have some items that can't be cleared.
        // Report these as inserted.
        if (newPlaylist->mediaCount() > 0) {
            *insertedStart = 0;
            *insertedEnd = newPlaylist->mediaCount() - 1;
        }
    } else {
        const int oldPlaylistSize = oldPlaylist->mediaCount();

        newPlaylist->clear();
        for (int i = 0; i < oldPlaylistSize; ++i)
            newPlaylist->addMedia(oldPlaylist->media(i));
    }

    newControl->setPlaybackMode(oldControl->playbackMode());
    newControl->setCurrentIndex(oldControl->currentIndex());
}

/*!
  Load playlist using network \a request. If \a format is specified, it is used,
  otherwise format is guessed from playlist name and data.

  New items are appended to playlist.

  QMediaPlaylist::loaded() signal is emitted if playlist was loaded successfully,
  otherwise the playlist emits loadFailed().
*/
void QMediaPlaylist::load(const QNetworkRequest &request, const char *format)
{
    Q_D(QMediaPlaylist);

    d->error = NoError;
    d->errorString.clear();

    if (d->playlist()->load(request,format))
        return;

    if (isReadOnly()) {
        d->error = AccessDeniedError;
        d->errorString = tr("Could not add items to read only playlist.");
        emit loadFailed();
        return;
    }

    const auto keys = playlistIOLoader()->keys();
    for (QString const& key : keys) {
        QMediaPlaylistIOInterface* plugin = qobject_cast<QMediaPlaylistIOInterface*>(playlistIOLoader()->instance(key));
        if (plugin && plugin->canRead(request.url(), format)) {
            QMediaPlaylistReader *reader = plugin->createReader(request.url(), QByteArray(format));
            if (reader && d->readItems(reader)) {
                delete reader;
                emit loaded();
                return;
            }
            delete reader;
        }
    }

    d->error = FormatNotSupportedError;
    d->errorString = tr("Playlist format is not supported");
    emit loadFailed();

    return;
}

/*!
  Load playlist from \a location. If \a format is specified, it is used,
  otherwise format is guessed from location name and data.

  New items are appended to playlist.

  QMediaPlaylist::loaded() signal is emitted if playlist was loaded successfully,
  otherwise the playlist emits loadFailed().
*/

void QMediaPlaylist::load(const QUrl &location, const char *format)
{
    load(QNetworkRequest(location), format);
}

/*!
  Load playlist from QIODevice \a device. If \a format is specified, it is used,
  otherwise format is guessed from device data.

  New items are appended to playlist.

  QMediaPlaylist::loaded() signal is emitted if playlist was loaded successfully,
  otherwise the playlist emits loadFailed().
*/
void QMediaPlaylist::load(QIODevice * device, const char *format)
{
    Q_D(QMediaPlaylist);

    d->error = NoError;
    d->errorString.clear();

    if (d->playlist()->load(device,format))
        return;

    if (isReadOnly()) {
        d->error = AccessDeniedError;
        d->errorString = tr("Could not add items to read only playlist.");
        emit loadFailed();
        return;
    }

    const auto keys = playlistIOLoader()->keys();
    for (QString const& key : keys) {
        QMediaPlaylistIOInterface* plugin = qobject_cast<QMediaPlaylistIOInterface*>(playlistIOLoader()->instance(key));
        if (plugin && plugin->canRead(device,format)) {
            QMediaPlaylistReader *reader = plugin->createReader(device,QByteArray(format));
            if (reader && d->readItems(reader)) {
                delete reader;
                emit loaded();
                return;
            }
            delete reader;
        }
    }

    d->error = FormatNotSupportedError;
    d->errorString = tr("Playlist format is not supported");
    emit loadFailed();

    return;
}

/*!
  Save playlist to \a location. If \a format is specified, it is used,
  otherwise format is guessed from location name.

  Returns true if playlist was saved successfully, otherwise returns false.
  */
bool QMediaPlaylist::save(const QUrl &location, const char *format)
{
    Q_D(QMediaPlaylist);

    d->error = NoError;
    d->errorString.clear();

    if (d->playlist()->save(location,format))
        return true;

    QFile file(location.toLocalFile());

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        d->error = AccessDeniedError;
        d->errorString = tr("The file could not be accessed.");
        return false;
    }

    return save(&file, format);
}

/*!
  Save playlist to QIODevice \a device using format \a format.

  Returns true if playlist was saved successfully, otherwise returns false.
*/
bool QMediaPlaylist::save(QIODevice * device, const char *format)
{
    Q_D(QMediaPlaylist);

    d->error = NoError;
    d->errorString.clear();

    if (d->playlist()->save(device,format))
        return true;

    const auto keys = playlistIOLoader()->keys();
    for (QString const& key : keys) {
        QMediaPlaylistIOInterface* plugin = qobject_cast<QMediaPlaylistIOInterface*>(playlistIOLoader()->instance(key));
        if (plugin && plugin->canWrite(device,format)) {
            QMediaPlaylistWriter *writer = plugin->createWriter(device,QByteArray(format));
            if (writer && d->writeItems(writer)) {
                delete writer;
                return true;
            }
            delete writer;
        }
    }

    d->error = FormatNotSupportedError;
    d->errorString = tr("Playlist format is not supported.");

    return false;
}

/*!
    Returns the last error condition.
*/
QMediaPlaylist::Error QMediaPlaylist::error() const
{
    return d_func()->error;
}

/*!
    Returns the string describing the last error condition.
*/
QString QMediaPlaylist::errorString() const
{
    return d_func()->errorString;
}

/*!
  Shuffle items in the playlist.
*/
void QMediaPlaylist::shuffle()
{
    d_func()->playlist()->shuffle();
}


/*!
    Advance to the next media content in playlist.
*/
void QMediaPlaylist::next()
{
    d_func()->control->next();
}

/*!
    Return to the previous media content in playlist.
*/
void QMediaPlaylist::previous()
{
    d_func()->control->previous();
}

/*!
    Activate media content from playlist at position \a playlistPosition.
*/

void QMediaPlaylist::setCurrentIndex(int playlistPosition)
{
    d_func()->control->setCurrentIndex(playlistPosition);
}

/*!
    \fn void QMediaPlaylist::mediaInserted(int start, int end)

    This signal is emitted after media has been inserted into the playlist.
    The new items are those between \a start and \a end inclusive.
 */

/*!
    \fn void QMediaPlaylist::mediaRemoved(int start, int end)

    This signal is emitted after media has been removed from the playlist.
    The removed items are those between \a start and \a end inclusive.
 */

/*!
    \fn void QMediaPlaylist::mediaChanged(int start, int end)

    This signal is emitted after media has been changed in the playlist
    between \a start and \a end positions inclusive.
 */

/*!
    \fn void QMediaPlaylist::currentIndexChanged(int position)

    Signal emitted when playlist position changed to \a position.
*/

/*!
    \fn void QMediaPlaylist::playbackModeChanged(QMediaPlaylist::PlaybackMode mode)

    Signal emitted when playback mode changed to \a mode.
*/

/*!
    \fn void QMediaPlaylist::mediaAboutToBeInserted(int start, int end)

    Signal emitted when items are to be inserted at \a start and ending at \a end.
*/

/*!
    \fn void QMediaPlaylist::mediaAboutToBeRemoved(int start, int end)

    Signal emitted when item are to be deleted at \a start and ending at \a end.
*/

/*!
    \fn void QMediaPlaylist::currentMediaChanged(const QMediaContent &content)

    Signal emitted when current media changes to \a content.
*/

/*!
    \property QMediaPlaylist::currentIndex
    \brief Current position.
*/

/*!
    \property QMediaPlaylist::currentMedia
    \brief Current media content.
*/

/*!
    \fn QMediaPlaylist::loaded()

    Signal emitted when playlist finished loading.
*/

/*!
    \fn QMediaPlaylist::loadFailed()

    Signal emitted if failed to load playlist.
*/

/*!
    \enum QMediaPlaylist::Error

    This enum describes the QMediaPlaylist error codes.

    \value NoError                 No errors.
    \value FormatError             Format error.
    \value FormatNotSupportedError Format not supported.
    \value NetworkError            Network error.
    \value AccessDeniedError       Access denied error.
*/

#include "moc_qmediaplaylist.cpp"
#include "moc_qmediaplaylist_p.cpp"
QT_END_NAMESPACE

