/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
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

// Video related snippets
// Extracted from src/multimedia/doc/snippets/multimedia-snippets/video.cpp
#include "qmediaservice.h"
#include "qmediaplayer.h"
#include "qvideowidgetcontrol.h"
#include "qvideowindowcontrol.h"
#include "qgraphicsvideoitem.h"
#include "qmediaplaylist.h"

#include <QFormLayout>
#include <QGraphicsView>

class VideoExample : public QObject {
    Q_OBJECT
public:
    void VideoGraphicsItem();
    void VideoWidget();
    void VideoWidgetControl();

private:
    // Common naming
    QMediaService *mediaService;
    QMediaPlaylist *playlist;
    QVideoWidget *videoWidget;
    QFormLayout *layout;
    QMediaPlayer *player;
    QGraphicsView *graphicsView;
};

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
}

void VideoExample::VideoWidgetControl()
{
    //! [Video widget control]
    QVideoWidgetControl *widgetControl = mediaService->requestControl<QVideoWidgetControl *>();
    layout->addWidget(widgetControl->videoWidget());
    //! [Video widget control]
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
