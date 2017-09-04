/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "bearercloud.h"
#include "cloud.h"

#include <QGraphicsTextItem>
#include <QTimer>
#include <QDateTime>
#include <QHostInfo>

#include <QDebug>

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//! [0]
BearerCloud::BearerCloud(QObject *parent)
:   QGraphicsScene(parent), timerId(0)
{
    setSceneRect(-300, -300, 600, 600);

    qsrand(QDateTime::currentDateTime().toTime_t());

    offset[QNetworkConfiguration::Active] = 2 * M_PI * qrand() / RAND_MAX;
    offset[QNetworkConfiguration::Discovered] = offset[QNetworkConfiguration::Active] + M_PI / 6;
    offset[QNetworkConfiguration::Defined] = offset[QNetworkConfiguration::Discovered] - M_PI / 6;
    offset[QNetworkConfiguration::Undefined] = offset[QNetworkConfiguration::Undefined] + M_PI / 6;

    thisDevice = new QGraphicsTextItem(QHostInfo::localHostName());
    thisDevice->setData(0, QLatin1String("This Device"));
    thisDevice->setPos(thisDevice->boundingRect().width() / -2,
                       thisDevice->boundingRect().height() / -2);
    addItem(thisDevice);

    qreal radius = Cloud::getRadiusForState(QNetworkConfiguration::Active);
    QGraphicsEllipseItem *orbit = new QGraphicsEllipseItem(-radius, -radius, 2*radius, 2*radius);
    orbit->setPen(QColor(Qt::green));
    addItem(orbit);
    radius = Cloud::getRadiusForState(QNetworkConfiguration::Discovered);
    orbit = new QGraphicsEllipseItem(-radius, -radius, 2*radius, 2*radius);
    orbit->setPen(QColor(Qt::blue));
    addItem(orbit);
    radius = Cloud::getRadiusForState(QNetworkConfiguration::Defined);
    orbit = new QGraphicsEllipseItem(-radius, -radius, 2*radius, 2*radius);
    orbit->setPen(QColor(Qt::darkGray));
    addItem(orbit);
    radius = Cloud::getRadiusForState(QNetworkConfiguration::Undefined);
    orbit = new QGraphicsEllipseItem(-radius, -radius, 2*radius, 2*radius);
    orbit->setPen(QColor(Qt::lightGray));
    addItem(orbit);

    connect(&manager, SIGNAL(configurationAdded(QNetworkConfiguration)),
            this, SLOT(configurationAdded(QNetworkConfiguration)));
    connect(&manager, SIGNAL(configurationRemoved(QNetworkConfiguration)),
            this, SLOT(configurationRemoved(QNetworkConfiguration)));
    connect(&manager, SIGNAL(configurationChanged(QNetworkConfiguration)),
            this, SLOT(configurationChanged(QNetworkConfiguration)));

    QTimer::singleShot(0, this, SLOT(updateConfigurations()));
}
//! [0]

BearerCloud::~BearerCloud()
{
}

void BearerCloud::cloudMoved()
{
    if (!timerId)
        timerId = startTimer(1000 / 25);
}

void BearerCloud::timerEvent(QTimerEvent *)
{
    std::vector<Cloud *> clouds;
    const auto graphicsItems = items();
    clouds.reserve(graphicsItems.size());
    for (QGraphicsItem *item : graphicsItems) {
        if (Cloud *cloud = qgraphicsitem_cast<Cloud *>(item))
            clouds.push_back(cloud);
    }

    for (Cloud *cloud : clouds)
        cloud->calculateForces();

    bool cloudsMoved = false;
    for (Cloud *cloud : clouds)
        cloudsMoved |= cloud->advance();

    if (!cloudsMoved) {
        killTimer(timerId);
        timerId = 0;
    }
}

//! [2]
void BearerCloud::configurationAdded(const QNetworkConfiguration &config)
{
    const QNetworkConfiguration::StateFlags state = config.state();

    configStates.insert(state, config.identifier());

    const qreal radius = Cloud::getRadiusForState(state);
    const int count = configStates.count(state);
    const qreal angle = 2 * M_PI / count;

    Cloud *item = new Cloud(config);
    configurations.insert(config.identifier(), item);

    item->setPos(radius * cos((count-1) * angle + offset[state]),
                 radius * sin((count-1) * angle + offset[state]));

    addItem(item);

    cloudMoved();
}
//! [2]

//! [3]
void BearerCloud::configurationRemoved(const QNetworkConfiguration &config)
{
    const auto id = config.identifier();
    for (auto it = configStates.begin(), end = configStates.end(); it != end; /* erasing */) {
        if (it.value() == id)
            it = configStates.erase(it);
        else
            ++it;
    }

    Cloud *item = configurations.take(config.identifier());

    item->setFinalScale(0.0);
    item->setDeleteAfterAnimation(true);

    cloudMoved();
}
//! [3]

//! [4]
void BearerCloud::configurationChanged(const QNetworkConfiguration &config)
{
    const auto id = config.identifier();
    for (auto it = configStates.begin(), end = configStates.end(); it != end; /* erasing */) {
        if (it.value() == id)
            it = configStates.erase(it);
        else
            ++it;
    }

    configStates.insert(config.state(), id);

    cloudMoved();
}
//! [4]

//! [1]
void BearerCloud::updateConfigurations()
{
    const auto allConfigurations = manager.allConfigurations();
    for (const QNetworkConfiguration &config : allConfigurations)
        configurationAdded(config);

    cloudMoved();
}
//! [1]

