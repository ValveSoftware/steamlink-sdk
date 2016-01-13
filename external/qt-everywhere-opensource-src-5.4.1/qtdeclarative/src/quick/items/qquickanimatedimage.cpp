/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickanimatedimage_p.h"
#include "qquickanimatedimage_p_p.h"

#ifndef QT_NO_MOVIE

#include <QtGui/qguiapplication.h>
#include <QtQml/qqmlinfo.h>
#include <QtQml/qqmlfile.h>
#include <QtQml/qqmlengine.h>
#include <QtGui/qmovie.h>
#include <QtNetwork/qnetworkrequest.h>
#include <QtNetwork/qnetworkreply.h>

QT_BEGIN_NAMESPACE

QQuickPixmap* QQuickAnimatedImagePrivate::infoForCurrentFrame(QQmlEngine *engine)
{
    if (!_movie)
        return 0;

    int current = _movie->currentFrameNumber();
    if (!frameMap.contains(current)) {
        QUrl requestedUrl;
        QQuickPixmap *pixmap = 0;
        if (engine && !_movie->fileName().isEmpty()) {
            requestedUrl.setUrl(QString::fromUtf8("quickanimatedimage://%1#%2")
                                .arg(_movie->fileName())
                                .arg(current));
        }
        if (!requestedUrl.isEmpty()) {
            if (QQuickPixmap::isCached(requestedUrl, QSize()))
                pixmap = new QQuickPixmap(engine, requestedUrl);
            else
                pixmap = new QQuickPixmap(requestedUrl, _movie->currentImage());
        } else {
            pixmap = new QQuickPixmap;
            pixmap->setImage(_movie->currentImage());
        }
        frameMap.insert(current, pixmap);
    }

    return frameMap.value(current);
}

/*!
    \qmltype AnimatedImage
    \instantiates QQuickAnimatedImage
    \inqmlmodule QtQuick
    \inherits Image
    \brief Plays animations stored as a series of images
    \ingroup qtquick-visual

    The AnimatedImage type extends the features of the \l Image type, providing
    a way to play animations stored as images containing a series of frames,
    such as those stored in GIF files.

    Information about the current frame and total length of the animation can be
    obtained using the \l currentFrame and \l frameCount properties. You can
    start, pause and stop the animation by changing the values of the \l playing
    and \l paused properties.

    The full list of supported formats can be determined with QMovie::supportedFormats().

    \section1 Example Usage

    \beginfloatleft
    \image animatedimageitem.gif
    \endfloat

    The following QML shows how to display an animated image and obtain information
    about its state, such as the current frame and total number of frames.
    The result is an animated image with a simple progress indicator underneath it.

    \b Note: Unlike images, animated images are not cached or shared internally.

    \clearfloat
    \snippet qml/animatedimage.qml document

    \sa BorderImage, Image
*/

/*!
    \qmlproperty url QtQuick::AnimatedImage::source

    This property holds the URL that refers to the source image.

    AnimatedImage can handle any image format supported by Qt, loaded from any
    URL scheme supported by Qt.

    \sa QQuickImageProvider
*/

QQuickAnimatedImage::QQuickAnimatedImage(QQuickItem *parent)
    : QQuickImage(*(new QQuickAnimatedImagePrivate), parent)
{
}

QQuickAnimatedImage::~QQuickAnimatedImage()
{
    Q_D(QQuickAnimatedImage);
    if (d->reply)
        d->reply->deleteLater();
    delete d->_movie;
    qDeleteAll(d->frameMap);
    d->frameMap.clear();
}

/*!
  \qmlproperty bool QtQuick::AnimatedImage::paused
  This property holds whether the animated image is paused.

  By default, this property is false. Set it to true when you want to pause
  the animation.
*/

bool QQuickAnimatedImage::isPaused() const
{
    Q_D(const QQuickAnimatedImage);
    if (!d->_movie)
        return d->paused;
    return d->_movie->state()==QMovie::Paused;
}

void QQuickAnimatedImage::setPaused(bool pause)
{
    Q_D(QQuickAnimatedImage);
    if (pause == d->paused)
        return;
    if (!d->_movie) {
        d->paused = pause;
        emit pausedChanged();
    } else {
        d->_movie->setPaused(pause);
    }
}

/*!
  \qmlproperty bool QtQuick::AnimatedImage::playing
  This property holds whether the animated image is playing.

  By default, this property is true, meaning that the animation
  will start playing immediately.

  \b Note: this property is affected by changes to the actual playing
  state of AnimatedImage. If non-animated images are used, \a playing
  will need to be manually set to \a true in order to animate
  following images.
  \qml
  AnimatedImage {
      onStatusChanged: playing = (status == AnimatedImage.Ready)
  }
  \endqml
*/

bool QQuickAnimatedImage::isPlaying() const
{
    Q_D(const QQuickAnimatedImage);
    if (!d->_movie)
        return d->playing;
    return d->_movie->state()!=QMovie::NotRunning;
}

void QQuickAnimatedImage::setPlaying(bool play)
{
    Q_D(QQuickAnimatedImage);
    if (play == d->playing)
        return;
    if (!d->_movie) {
        d->playing = play;
        emit playingChanged();
        return;
    }
    if (play)
        d->_movie->start();
    else
        d->_movie->stop();
}

/*!
  \qmlproperty int QtQuick::AnimatedImage::currentFrame
  \qmlproperty int QtQuick::AnimatedImage::frameCount

  currentFrame is the frame that is currently visible. By monitoring this property
  for changes, you can animate other items at the same time as the image.

  frameCount is the number of frames in the animation. For some animation formats,
  frameCount is unknown and has a value of zero.
*/
int QQuickAnimatedImage::currentFrame() const
{
    Q_D(const QQuickAnimatedImage);
    if (!d->_movie)
        return d->preset_currentframe;
    return d->_movie->currentFrameNumber();
}

void QQuickAnimatedImage::setCurrentFrame(int frame)
{
    Q_D(QQuickAnimatedImage);
    if (!d->_movie) {
        d->preset_currentframe = frame;
        return;
    }
    d->_movie->jumpToFrame(frame);
}

int QQuickAnimatedImage::frameCount() const
{
    Q_D(const QQuickAnimatedImage);
    if (!d->_movie)
        return 0;
    return d->_movie->frameCount();
}

void QQuickAnimatedImage::setSource(const QUrl &url)
{
    Q_D(QQuickAnimatedImage);
    if (url == d->url)
        return;

    if (d->reply) {
        d->reply->deleteLater();
        d->reply = 0;
    }

    d->setImage(QImage());
    qDeleteAll(d->frameMap);
    d->frameMap.clear();

    d->oldPlaying = isPlaying();
    if (d->_movie) {
        delete d->_movie;
        d->_movie = 0;
    }

    d->url = url;
    emit sourceChanged(d->url);

    if (isComponentComplete())
        load();
}

void QQuickAnimatedImage::load()
{
    Q_D(QQuickAnimatedImage);

    if (d->url.isEmpty()) {
        if (d->progress != 0) {
            d->progress = 0;
            emit progressChanged(d->progress);
        }

        d->setImage(QImage());
        d->status = Null;
        emit statusChanged(d->status);

        if (sourceSize() != d->oldSourceSize) {
            d->oldSourceSize = sourceSize();
            emit sourceSizeChanged();
        }
        if (isPlaying() != d->oldPlaying)
            emit playingChanged();
    } else {
        const qreal targetDevicePixelRatio = (window() ? window()->effectiveDevicePixelRatio() : qApp->devicePixelRatio());
        d->devicePixelRatio = 1.0;

        QUrl loadUrl = d->url;
        resolve2xLocalFile(d->url, targetDevicePixelRatio, &loadUrl, &d->devicePixelRatio);
        QString lf = QQmlFile::urlToLocalFileOrQrc(loadUrl);

        if (!lf.isEmpty()) {
            d->_movie = new QMovie(lf);
            movieRequestFinished();
        } else {
            if (d->status != Loading) {
                d->status = Loading;
                emit statusChanged(d->status);
            }
            if (d->progress != 0) {
                d->progress = 0;
                emit progressChanged(d->progress);
            }
            QNetworkRequest req(d->url);
            req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);

            d->reply = qmlEngine(this)->networkAccessManager()->get(req);
            QObject::connect(d->reply, SIGNAL(finished()),
                            this, SLOT(movieRequestFinished()));
            QObject::connect(d->reply, SIGNAL(downloadProgress(qint64,qint64)),
                            this, SLOT(requestProgress(qint64,qint64)));
        }
    }
}

#define ANIMATEDIMAGE_MAXIMUM_REDIRECT_RECURSION 16

void QQuickAnimatedImage::movieRequestFinished()
{
    Q_D(QQuickAnimatedImage);

    if (d->reply) {
        d->redirectCount++;
        if (d->redirectCount < ANIMATEDIMAGE_MAXIMUM_REDIRECT_RECURSION) {
            QVariant redirect = d->reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
            if (redirect.isValid()) {
                QUrl url = d->reply->url().resolved(redirect.toUrl());
                d->reply->deleteLater();
                setSource(url);
                return;
            }
        }

        d->redirectCount=0;
        d->_movie = new QMovie(d->reply);
    }

    if (!d->_movie->isValid()) {
        qmlInfo(this) << "Error Reading Animated Image File " << d->url.toString();
        delete d->_movie;
        d->_movie = 0;
        d->setImage(QImage());
        if (d->progress != 0) {
            d->progress = 0;
            emit progressChanged(d->progress);
        }
        d->status = Error;
        emit statusChanged(d->status);

        if (sourceSize() != d->oldSourceSize) {
            d->oldSourceSize = sourceSize();
            emit sourceSizeChanged();
        }
        if (isPlaying() != d->oldPlaying)
            emit playingChanged();
        return;
    }

    connect(d->_movie, SIGNAL(stateChanged(QMovie::MovieState)),
            this, SLOT(playingStatusChanged()));
    connect(d->_movie, SIGNAL(frameChanged(int)),
            this, SLOT(movieUpdate()));
    d->_movie->setCacheMode(QMovie::CacheAll);

    d->status = Ready;
    emit statusChanged(d->status);

    if (d->progress != 1.0) {
        d->progress = 1.0;
        emit progressChanged(d->progress);
    }

    bool pausedAtStart = d->paused;
    if (d->playing) {
        d->_movie->start();
    }
    if (pausedAtStart)
        d->_movie->setPaused(true);
    if (d->paused || !d->playing) {
        d->_movie->jumpToFrame(d->preset_currentframe);
        d->preset_currentframe = 0;
    }
    d->setPixmap(*d->infoForCurrentFrame(qmlEngine(this)));

    if (isPlaying() != d->oldPlaying)
        emit playingChanged();
    if (sourceSize() != d->oldSourceSize) {
        d->oldSourceSize = sourceSize();
        emit sourceSizeChanged();
    }
}

void QQuickAnimatedImage::movieUpdate()
{
    Q_D(QQuickAnimatedImage);

    if (d->_movie) {
        d->setPixmap(*d->infoForCurrentFrame(qmlEngine(this)));
        emit frameChanged();
    }
}

void QQuickAnimatedImage::playingStatusChanged()
{
    Q_D(QQuickAnimatedImage);

    if ((d->_movie->state() != QMovie::NotRunning) != d->playing) {
        d->playing = (d->_movie->state() != QMovie::NotRunning);
        emit playingChanged();
    }
    if ((d->_movie->state() == QMovie::Paused) != d->paused) {
        d->paused = (d->_movie->state() == QMovie::Paused);
        emit pausedChanged();
    }
}

QSize QQuickAnimatedImage::sourceSize()
{
    Q_D(QQuickAnimatedImage);
    if (!d->_movie)
        return QSize(0, 0);
    return QSize(d->_movie->currentPixmap().size());
}

void QQuickAnimatedImage::componentComplete()
{
    QQuickItem::componentComplete(); // NOT QQuickImage
    load();
}

QT_END_NAMESPACE

#endif // QT_NO_MOVIE
