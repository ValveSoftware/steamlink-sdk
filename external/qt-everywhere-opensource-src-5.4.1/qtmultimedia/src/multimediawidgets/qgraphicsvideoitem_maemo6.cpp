/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#include "qgraphicsvideoitem.h"

#include <QtCore/qcoreevent.h>
#include <QtCore/qpointer.h>
#include <QtCore/qbasictimer.h>

#include <QtWidgets/qgraphicsscene.h>

#include <qmediaobject.h>
#include <qmediaservice.h>
#include <qpaintervideosurface_p.h>
#include <qeglimagetexturesurface_p.h>
#include <qvideorenderercontrol.h>

#include <qvideosurfaceformat.h>

#include <X11/Xlib.h>

#if !defined(QT_NO_OPENGL) && !defined(QT_OPENGL_ES_1_CL) && !defined(QT_OPENGL_ES_1)
#include <QtOpenGL/qgl.h>
#endif

//#define ENABLE_OVERLAY

namespace
{
//XInitThreads is necessary for gltexturesink element.
//To ensure it's called before main() it's better to link to
//libQtMultimedia.so directly, not when QML multimedia plugin is loaded.
class InitThreads
{
public:
    InitThreads()
    {
        XInitThreads();
    }
} _initThreads;
}

Q_DECLARE_METATYPE(QVideoSurfaceFormat)

QT_BEGIN_NAMESPACE

class QGraphicsVideoItemPrivate
{
public:
    QGraphicsVideoItemPrivate()
        : q_ptr(0)
        , surface(0)
        , mediaObject(0)
        , service(0)
        , rendererControl(0)
        , aspectRatioMode(Qt::KeepAspectRatio)
        , updatePaintDevice(true)
        , rect(0.0, 0.0, 320, 240)
    {
    }

    QGraphicsVideoItem *q_ptr;

    QEglImageTextureSurface *surface;
    QPointer<QMediaObject> mediaObject;
    QMediaService *service;
    QVideoRendererControl *rendererControl;
    Qt::AspectRatioMode aspectRatioMode;
    bool updatePaintDevice;
    QRectF rect;
    QRectF boundingRect;
    QRectF sourceRect;
    QSizeF nativeSize;

    void clearService();
    void updateRects();

    void _q_present();
    void _q_formatChanged(const QVideoSurfaceFormat &format);
    void _q_updateNativeSize();
    void _q_serviceDestroyed();
};

void QGraphicsVideoItemPrivate::clearService()
{
    if (rendererControl) {
        surface->stop();
        rendererControl->setSurface(0);
        service->releaseControl(rendererControl);
        rendererControl = 0;
    }
    if (service) {
        QObject::disconnect(service, SIGNAL(destroyed()), q_ptr, SLOT(_q_serviceDestroyed()));
        service = 0;
    }
}

void QGraphicsVideoItemPrivate::updateRects()
{
    q_ptr->prepareGeometryChange();

    if (nativeSize.isEmpty()) {
        //this is necessary for item to receive the
        //first paint event and configure video surface.
        boundingRect = rect;
    } else if (aspectRatioMode == Qt::IgnoreAspectRatio) {
        boundingRect = rect;
        sourceRect = QRectF(0, 0, 1, 1);
    } else if (aspectRatioMode == Qt::KeepAspectRatio) {
        QSizeF size = nativeSize;
        size.scale(rect.size(), Qt::KeepAspectRatio);

        boundingRect = QRectF(0, 0, size.width(), size.height());
        boundingRect.moveCenter(rect.center());

        sourceRect = QRectF(0, 0, 1, 1);
    } else if (aspectRatioMode == Qt::KeepAspectRatioByExpanding) {
        boundingRect = rect;

        QSizeF size = rect.size();
        size.scale(nativeSize, Qt::KeepAspectRatio);

        sourceRect = QRectF(
                0, 0, size.width() / nativeSize.width(), size.height() / nativeSize.height());
        sourceRect.moveCenter(QPointF(0.5, 0.5));
    }
}

void QGraphicsVideoItemPrivate::_q_present()
{
    if (q_ptr->isObscured()) {
        q_ptr->update(boundingRect);
        surface->setReady(true);
    } else {
        q_ptr->update(boundingRect);
    }
}

void QGraphicsVideoItemPrivate::_q_updateNativeSize()
{
    QSize size = surface->surfaceFormat().sizeHint();
    if (size.isEmpty())
        size = rendererControl->property("nativeSize").toSize();

    if (nativeSize != size) {
        nativeSize = size;

        updateRects();
        emit q_ptr->nativeSizeChanged(nativeSize);
    }
}

void QGraphicsVideoItemPrivate::_q_serviceDestroyed()
{
    rendererControl = 0;
    service = 0;

    surface->stop();
}


/*
    \class QGraphicsVideoItem


    \brief The QGraphicsVideoItem class provides a graphics item which display video produced by a QMediaObject.

    \inmodule QtMultimediaWidgets
    \ingroup multimedia

    Attaching a QGraphicsVideoItem to a QMediaObject allows it to display
    the video or image output of that media object.  A QGraphicsVideoItem
    is attached to a media object by passing a pointer to the QMediaObject
    to the setMediaObject() function.

    \code
    player = new QMediaPlayer(this);

    QGraphicsVideoItem *item = new QGraphicsVideoItem;
    player->setVideoOutput(item);
    graphicsView->scene()->addItem(item);
    graphicsView->show();

    player->setMedia(video);
    player->play();
    \endcode

    \b {Note}: Only a single display output can be attached to a media
    object at one time.

    \sa QMediaObject, QMediaPlayer, QVideoWidget
*/

/*
    Constructs a graphics item that displays video.

    The \a parent is passed to QGraphicsItem.
*/
QGraphicsVideoItem::QGraphicsVideoItem(QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , d_ptr(new QGraphicsVideoItemPrivate)
{
    d_ptr->q_ptr = this;
    d_ptr->surface = new QEglImageTextureSurface(this);

    qRegisterMetaType<QVideoSurfaceFormat>();

    connect(d_ptr->surface, SIGNAL(frameChanged()), this, SLOT(_q_present()));
    connect(d_ptr->surface, SIGNAL(surfaceFormatChanged(QVideoSurfaceFormat)),
            this, SLOT(_q_updateNativeSize()), Qt::QueuedConnection);
}

/*
    Destroys a video graphics item.
*/
QGraphicsVideoItem::~QGraphicsVideoItem()
{
    if (d_ptr->rendererControl) {
        d_ptr->rendererControl->setSurface(0);
        d_ptr->service->releaseControl(d_ptr->rendererControl);
    }

    delete d_ptr->surface;
    delete d_ptr;
}

/*
    \property QGraphicsVideoItem::mediaObject
    \brief the media object which provides the video displayed by a graphics
    item.
*/

QMediaObject *QGraphicsVideoItem::mediaObject() const
{
    return d_func()->mediaObject;
}

/*
  \internal
*/
bool QGraphicsVideoItem::setMediaObject(QMediaObject *object)
{
    Q_D(QGraphicsVideoItem);

    if (object == d->mediaObject)
        return true;

    d->clearService();

    d->mediaObject = object;

    if (d->mediaObject) {
        d->service = d->mediaObject->service();

        if (d->service) {
            QMediaControl *control = d->service->requestControl(QVideoRendererControl_iid);
            if (control) {
                d->rendererControl = qobject_cast<QVideoRendererControl *>(control);

                if (d->rendererControl) {
                    connect(d->rendererControl, SIGNAL(nativeSizeChanged()),
                            this, SLOT(_q_updateNativeSize()), Qt::QueuedConnection);
                    d->_q_updateNativeSize();
                    //don't set the surface untill the item is painted
                    //at least once and the surface is configured
                    if (!d->updatePaintDevice)
                        d->rendererControl->setSurface(d->surface);
                    else
                        update(boundingRect());

                    connect(d->service, SIGNAL(destroyed()), this, SLOT(_q_serviceDestroyed()));

                    return true;
                }
                if (control)
                    d->service->releaseControl(control);
            }
        }
    }

    d->mediaObject = 0;
    return false;
}

/*
    \property QGraphicsVideoItem::aspectRatioMode
    \brief how a video is scaled to fit the graphics item's size.
*/

Qt::AspectRatioMode QGraphicsVideoItem::aspectRatioMode() const
{
    return d_func()->aspectRatioMode;
}

void QGraphicsVideoItem::setAspectRatioMode(Qt::AspectRatioMode mode)
{
    Q_D(QGraphicsVideoItem);

    d->aspectRatioMode = mode;
    d->updateRects();
}

/*
    \property QGraphicsVideoItem::offset
    \brief the video item's offset.

    QGraphicsVideoItem will draw video using the offset for its top left
    corner.
*/

QPointF QGraphicsVideoItem::offset() const
{
    return d_func()->rect.topLeft();
}

void QGraphicsVideoItem::setOffset(const QPointF &offset)
{
    Q_D(QGraphicsVideoItem);

    d->rect.moveTo(offset);
    d->updateRects();
}

/*
    \property QGraphicsVideoItem::size
    \brief the video item's size.

    QGraphicsVideoItem will draw video scaled to fit size according to its
    fillMode.
*/

QSizeF QGraphicsVideoItem::size() const
{
    return d_func()->rect.size();
}

void QGraphicsVideoItem::setSize(const QSizeF &size)
{
    Q_D(QGraphicsVideoItem);

    d->rect.setSize(size.isValid() ? size : QSizeF(0, 0));
    d->updateRects();
}

/*
    \property QGraphicsVideoItem::nativeSize
    \brief the native size of the video.
*/

QSizeF QGraphicsVideoItem::nativeSize() const
{
    return d_func()->nativeSize;
}

/*
    \fn QGraphicsVideoItem::nativeSizeChanged(const QSizeF &size)

    Signals that the native \a size of the video has changed.
*/

/*
    \reimp
*/
QRectF QGraphicsVideoItem::boundingRect() const
{
    return d_func()->boundingRect;
}

/*
    \reimp
*/
void QGraphicsVideoItem::paint(
        QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_D(QGraphicsVideoItem);

    Q_UNUSED(option);
    Q_UNUSED(widget);

    if (d->surface && d->rendererControl && d->updatePaintDevice) {
        d->updatePaintDevice = false;

        if (widget)
            d->rendererControl->setProperty("winId", qulonglong(widget->winId()));

#if !defined(QT_NO_OPENGL) && !defined(QT_OPENGL_ES_1_CL) && !defined(QT_OPENGL_ES_1)
        if (widget)
            connect(widget, SIGNAL(destroyed()), d->surface, SLOT(viewportDestroyed()));

        d->surface->setGLContext(const_cast<QGLContext *>(QGLContext::currentContext()));
#endif
        if (d->rendererControl->surface() != d->surface)
            d->rendererControl->setSurface(d->surface);
    }


    //overlay doesn't work reliably

    //check if the item is obscured:
#ifdef ENABLE_OVERLAY
    if (!isObscured()) {
        bool obscured = false;

        if (scene()) {
            foreach (QGraphicsItem *item,
                     scene()->items(mapToScene(boundingRect()), Qt::IntersectsItemBoundingRect) ) {
                if (item->flags() & QGraphicsItem::ItemHasNoContents)
                    continue;

                if (item == this)
                    break;

                if (collidesWithItem(item)) {
                    obscured = true;
                    break;
                }
            }
        }

        d->rendererControl->setProperty("overlayEnabled", !obscured);
    }

    if (d->rendererControl->property("overlayEnabled").toBool()) {
        QTransform transform = painter->combinedTransform();
        QRect overlayRect = transform.mapRect(d->boundingRect).toRect();

        d->rendererControl->setProperty("overlayGeometry", overlayRect);
        QMetaObject::invokeMethod(d->rendererControl, "repaintOverlay");

        painter->fillRect(d->boundingRect,
                          d->rendererControl->property("colorKey").value<QColor>());
    } else
#endif //ENABLE_OVERLAY
    {
        if (d->surface && d->surface->isActive()) {
            d->surface->paint(painter, d->boundingRect, d->sourceRect);
            d->surface->setReady(true);
        }
    }
}

/*
    \reimp

    \internal
*/
QVariant QGraphicsVideoItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    return QGraphicsItem::itemChange(change, value);
}

/*
  \internal
*/
void QGraphicsVideoItem::timerEvent(QTimerEvent *event)
{
    QGraphicsObject::timerEvent(event);
}

#include "moc_qgraphicsvideoitem.cpp"
QT_END_NAMESPACE
