/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>
#include <QtCore/qdebug.h>
#include <QtCore/qbuffer.h>
#include <QtNetwork/qnetworkconfiguration.h>

#include <qgraphicsvideoitem.h>
#include <qabstractvideosurface.h>
#include <qmediaplayer.h>
#include <qmediaplayercontrol.h>

#include "mockmediaserviceprovider.h"
#include "mockmediaplayerservice.h"
#include "mockvideosurface.h"

QT_USE_NAMESPACE

class tst_QMediaPlayerWidgets: public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void testSetVideoOutput();
    void testSetVideoOutputNoService();
    void testSetVideoOutputNoControl();

private:
    MockMediaServiceProvider *provider;
    MockMediaPlayerService  *mockService;
};

void tst_QMediaPlayerWidgets::initTestCase()
{
    provider = new MockMediaServiceProvider;
    QMediaServiceProvider::setDefaultServiceProvider(provider);
}

void tst_QMediaPlayerWidgets::cleanupTestCase()
{
    delete provider;
}

void tst_QMediaPlayerWidgets::init()
{
    mockService = new MockMediaPlayerService;
    provider->service = mockService;
}

void tst_QMediaPlayerWidgets::cleanup()
{
}

void tst_QMediaPlayerWidgets::testSetVideoOutput()
{
    QVideoWidget widget;
    QGraphicsVideoItem item;
    MockVideoSurface surface;

    QMediaPlayer player;

    player.setVideoOutput(&widget);
    QVERIFY(widget.mediaObject() == &player);

    player.setVideoOutput(&item);
    QVERIFY(widget.mediaObject() == 0);
    QVERIFY(item.mediaObject() == &player);

    player.setVideoOutput(reinterpret_cast<QVideoWidget *>(0));
    QVERIFY(item.mediaObject() == 0);

    player.setVideoOutput(&widget);
    QVERIFY(widget.mediaObject() == &player);

    player.setVideoOutput(reinterpret_cast<QGraphicsVideoItem *>(0));
    QVERIFY(widget.mediaObject() == 0);

    player.setVideoOutput(&surface);
    QVERIFY(mockService->rendererControl->surface() == &surface);

    player.setVideoOutput(reinterpret_cast<QAbstractVideoSurface *>(0));
    QVERIFY(mockService->rendererControl->surface() == 0);

    player.setVideoOutput(&surface);
    QVERIFY(mockService->rendererControl->surface() == &surface);

    player.setVideoOutput(&widget);
    QVERIFY(mockService->rendererControl->surface() == 0);
    QVERIFY(widget.mediaObject() == &player);

    player.setVideoOutput(&surface);
    QVERIFY(mockService->rendererControl->surface() == &surface);
    QVERIFY(widget.mediaObject() == 0);
}


void tst_QMediaPlayerWidgets::testSetVideoOutputNoService()
{
    QVideoWidget widget;
    QGraphicsVideoItem item;
    MockVideoSurface surface;

    provider->service = 0;
    QMediaPlayer player;

    player.setVideoOutput(&widget);
    QVERIFY(widget.mediaObject() == 0);

    player.setVideoOutput(&item);
    QVERIFY(item.mediaObject() == 0);

    player.setVideoOutput(&surface);
    // Nothing we can verify here other than it doesn't assert.
}

void tst_QMediaPlayerWidgets::testSetVideoOutputNoControl()
{
    QVideoWidget widget;
    QGraphicsVideoItem item;
    MockVideoSurface surface;

    MockMediaPlayerService service;
    service.rendererRef = 1;
    service.windowRef = 1;

    provider->service = &service;

    QMediaPlayer player;

    player.setVideoOutput(&widget);
    QVERIFY(widget.mediaObject() == 0);

    player.setVideoOutput(&item);
    QVERIFY(item.mediaObject() == 0);

    player.setVideoOutput(&surface);
    QVERIFY(service.rendererControl->surface() == 0);
}

QTEST_MAIN(tst_QMediaPlayerWidgets)
#include "tst_qmediaplayerwidgets.moc"
