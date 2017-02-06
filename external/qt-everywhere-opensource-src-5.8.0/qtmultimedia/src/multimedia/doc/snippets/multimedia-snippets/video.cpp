/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Mobility Components.
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

/* Video related snippets */
#include "qvideorenderercontrol.h"
#include "qmediaservice.h"
#include "qmediaplayer.h"
#include "qabstractvideosurface.h"
#include "qvideowidgetcontrol.h"
#include "qvideowindowcontrol.h"
#include "qgraphicsvideoitem.h"
#include "qmediaplaylist.h"
#include "qvideosurfaceformat.h"

#include <QFormLayout>
#include <QGraphicsView>

//! [Derived Surface]
class MyVideoSurface : public QAbstractVideoSurface
{
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
            QAbstractVideoBuffer::HandleType handleType = QAbstractVideoBuffer::NoHandle) const
    {
        Q_UNUSED(handleType);

        // Return the formats you will support
        return QList<QVideoFrame::PixelFormat>() << QVideoFrame::Format_RGB565;
    }

    bool present(const QVideoFrame &frame)
    {
        Q_UNUSED(frame);
        // Handle the frame and do your processing

        return true;
    }
};
//! [Derived Surface]

//! [Video producer]
class MyVideoProducer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractVideoSurface *videoSurface READ videoSurface WRITE setVideoSurface)

public:
    QAbstractVideoSurface* videoSurface() const { return m_surface; }

    void setVideoSurface(QAbstractVideoSurface *surface)
    {
        if (m_surface != surface && m_surface && m_surface->isActive()) {
            m_surface->stop();
        }
        m_surface = surface;
        if (m_surface)
            m_surface->start(m_format);
    }

    // ...

public slots:
    void onNewVideoContentReceived(const QVideoFrame &frame)
    {
        if (m_surface)
            m_surface->present(frame);
    }

private:
    QAbstractVideoSurface *m_surface;
    QVideoSurfaceFormat m_format;
};

//! [Video producer]


class VideoExample : public QObject {
    Q_OBJECT
public:
    void VideoGraphicsItem();
    void VideoRendererControl();
    void VideoWidget();
    void VideoWindowControl();
    void VideoWidgetControl();

private:
    // Common naming
    QMediaService *mediaService;
    QMediaPlaylist *playlist;
    QVideoWidget *videoWidget;
    QWidget *widget;
    QFormLayout *layout;
    QAbstractVideoSurface *myVideoSurface;
    QMediaPlayer *player;
    QMediaContent video;
    QGraphicsView *graphicsView;
};

void VideoExample::VideoRendererControl()
{
    //! [Video renderer control]
    QVideoRendererControl *rendererControl = mediaService->requestControl<QVideoRendererControl *>();
    rendererControl->setSurface(myVideoSurface);
    //! [Video renderer control]
}

void VideoExample::VideoWidget()
{
    //! [Video widget]
    player = new QMediaPlayer;

    playlist = new QMediaPlaylist(player);
    playlist->addMedia(QUrl("http://example.com/myclip1.mp4"));
    playlist->addMedia(QUrl("http://example.com/myclip2.mp4"));

    videoWidget = new QVideoWidget;
    player->setVideoOutput(videoWidget);

    videoWidget->show();
    playlist->setCurrentIndex(1);
    player->play();
    //! [Video widget]

    player->stop();

    //! [Setting surface in player]
    player->setVideoOutput(myVideoSurface);
    //! [Setting surface in player]
}

void VideoExample::VideoWidgetControl()
{
    //! [Video widget control]
    QVideoWidgetControl *widgetControl = mediaService->requestControl<QVideoWidgetControl *>();
    layout->addWidget(widgetControl->videoWidget());
    //! [Video widget control]
}

void VideoExample::VideoWindowControl()
{
    //! [Video window control]
    QVideoWindowControl *windowControl = mediaService->requestControl<QVideoWindowControl *>();
    windowControl->setWinId(widget->winId());
    windowControl->setDisplayRect(widget->rect());
    windowControl->setAspectRatioMode(Qt::KeepAspectRatio);
    //! [Video window control]
}

void VideoExample::VideoGraphicsItem()
{
    //! [Video graphics item]
    player = new QMediaPlayer(this);

    QGraphicsVideoItem *item = new QGraphicsVideoItem;
    player->setVideoOutput(item);
    graphicsView->scene()->addItem(item);
    graphicsView->show();

    player->setMedia(QUrl("http://example.com/myclip4.ogv"));
    player->play();
    //! [Video graphics item]
}
