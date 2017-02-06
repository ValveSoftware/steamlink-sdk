/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickanimatedimage_p.h"
#include "qquickanimatedimage_p_p.h"

#include <QtGui/qguiapplication.h>
#include <QtQml/qqmlinfo.h>
#include <QtQml/qqmlfile.h>
#include <QtQml/qqmlengine.h>
#include <QtGui/qmovie.h>
#if QT_CONFIG(qml_network)
#include <QtNetwork/qnetworkrequest.h>
#include <QtNetwork/qnetworkreply.h>
#endif

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

    \b Note: When animated images are cached, every frame of the animation will be cached.

    Set cache to false if you are playing a long or large animation and you
    want to conserve memory.

    If the image data comes from a sequential device (e.g. a socket),
    AnimatedImage can only loop if cache is set to true.

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
    QObject::connect(this, &QQuickImageBase::cacheChanged, this, &QQuickAnimatedImage::onCacheChanged);
}

QQuickAnimatedImage::~QQuickAnimatedImage()
{
    Q_D(QQuickAnimatedImage);
#if QT_CONFIG(qml_network)
    if (d->reply)
        d->reply->deleteLater();
#endif
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

#if QT_CONFIG(qml_network)
    if (d->reply) {
        d->reply->deleteLater();
        d->reply = 0;
    }
#endif

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

        d->currentSourceSize = QSize(0, 0);
        if (d->currentSourceSize != d->oldSourceSize) {
            d->oldSourceSize = d->currentSourceSize;
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
#if QT_CONFIG(qml_network)
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
#endif
        }
    }
}

#define ANIMATEDIMAGE_MAXIMUM_REDIRECT_RECURSION 16

void QQuickAnimatedImage::movieRequestFinished()
{

    Q_D(QQuickAnimatedImage);

#if QT_CONFIG(qml_network)
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
#endif

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

        d->currentSourceSize = QSize(0, 0);
        if (d->currentSourceSize != d->oldSourceSize) {
            d->oldSourceSize = d->currentSourceSize;
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
    if (d->cache)
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

    if (d->_movie)
        d->currentSourceSize = d->_movie->currentPixmap().size();
    else
        d->currentSourceSize = QSize(0, 0);

    if (d->currentSourceSize != d->oldSourceSize) {
        d->oldSourceSize = d->currentSourceSize;
        emit sourceSizeChanged();
    }
}

void QQuickAnimatedImage::movieUpdate()
{
    Q_D(QQuickAnimatedImage);

    if (!d->cache) {
        qDeleteAll(d->frameMap);
        d->frameMap.clear();
    }

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

void QQuickAnimatedImage::onCacheChanged()
{
    Q_D(QQuickAnimatedImage);
    if (!cache()) {
        qDeleteAll(d->frameMap);
        d->frameMap.clear();
        if (d->_movie) {
            d->_movie->setCacheMode(QMovie::CacheNone);
        }
    } else {
        if (d->_movie) {
            d->_movie->setCacheMode(QMovie::CacheAll);
        }
    }
}

QSize QQuickAnimatedImage::sourceSize()
{
    Q_D(QQuickAnimatedImage);
    return d->currentSourceSize;
}

void QQuickAnimatedImage::componentComplete()
{
    QQuickItem::componentComplete(); // NOT QQuickImage
    load();
}

QT_END_NAMESPACE
