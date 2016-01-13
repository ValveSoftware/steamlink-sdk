/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#include "cloud.h"
#include "bearercloud.h"

#include <qnetworksession.h>

#include <QGraphicsTextItem>
#include <QGraphicsSvgItem>
#include <QGraphicsSceneMouseEvent>
#include <QSvgRenderer>
#include <QPainter>

#include <QDebug>

#include <math.h>

static QMap<QNetworkConfiguration::BearerType, QSvgRenderer *> svgCache;

//! [0]
Cloud::Cloud(const QNetworkConfiguration &config, QGraphicsItem *parent)
:   QGraphicsItem(parent), configuration(config), deleteAfterAnimation(false)
{
    session = new QNetworkSession(configuration, this);
    connect(session, SIGNAL(newConfigurationActivated()),
            this, SLOT(newConfigurationActivated()));
    connect(session, SIGNAL(stateChanged(QNetworkSession::State)),
            this, SLOT(stateChanged(QNetworkSession::State)));

    setFlag(ItemIsMovable);
#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
    setFlag(ItemSendsGeometryChanges);
#endif
    setZValue(1);

    icon = new QGraphicsSvgItem(this);
    text = new QGraphicsTextItem(this);

    currentScale = 0;
    finalScale = 1;
    setTransform(QTransform::fromScale(currentScale, currentScale), false);
    setOpacity(0);

    newConfigurationActivated();
}
//! [0]

Cloud::~Cloud()
{
}

void Cloud::setFinalScale(qreal factor)
{
    finalScale = factor;
}

void Cloud::setDeleteAfterAnimation(bool deleteAfter)
{
    deleteAfterAnimation = deleteAfter;
}

void Cloud::calculateForces()
{
    if (!scene() || scene()->mouseGrabberItem() == this) {
        newPos = pos();
        return;
    }

    // sum up all the forces push this item away
    qreal xvel = 0;
    qreal yvel = 0;
    QLineF orbitForce;
    foreach (QGraphicsItem *item, scene()->items()) {
        // other clouds
        Cloud *cloud = qgraphicsitem_cast<Cloud *>(item);
        if (!cloud && item->data(0) != QLatin1String("This Device"))
            continue;

        qreal factor = 1.0;

        QLineF line(cloud ? item->mapToScene(0, 0) : QPointF(0, 0), mapToScene(0, 0));
        if (item->data(0) == QLatin1String("This Device"))
            orbitForce = line;

        if (cloud)
            factor = cloud->currentScale;

        qreal dx = line.dx();
        qreal dy = line.dy();
        double l = 2.0 * (dx * dx + dy * dy);
        if (l > 0) {
            xvel += factor * dx * 200.0 / l;
            yvel += factor * dy * 200.0 / l;
        }
    }

    // tendency to stay at a fixed orbit
    qreal orbit = getRadiusForState(configuration.state());
    qreal distance = orbitForce.length();

    QLineF unit = orbitForce.unitVector();

    orbitForce.setLength(xvel * unit.dx() + yvel * unit.dy());

    qreal w = 2 - exp(-pow(distance-orbit, 2)/(2 * 50));

    if (distance < orbit) {
        xvel += orbitForce.dx() * w;
        yvel += orbitForce.dy() * w;
    } else {
        xvel -= orbitForce.dx() * w;
        yvel -= orbitForce.dy() * w;
    }

    if (qAbs(xvel) < 0.1 && qAbs(yvel) < 0.1)
        xvel = yvel = 0;

    QRectF sceneRect = scene()->sceneRect();
    newPos = pos() + QPointF(xvel, yvel);
    newPos.setX(qMin(qMax(newPos.x(), sceneRect.left() + 10), sceneRect.right() - 10));
    newPos.setY(qMin(qMax(newPos.y(), sceneRect.top() + 10), sceneRect.bottom() - 10));
}

bool Cloud::advance()
{
    static const qreal scaleDelta = 0.01;

    bool animated = false;

    if (currentScale < finalScale) {
        animated = true;
        currentScale = qMin<qreal>(currentScale + scaleDelta, finalScale);
        setTransform(QTransform::fromScale(currentScale, currentScale), false);
    } else if (currentScale > finalScale) {
        animated = true;
        currentScale = qMax<qreal>(currentScale - scaleDelta, finalScale);
        setTransform(QTransform::fromScale(currentScale, currentScale), false);
    }

    if (newPos != pos()) {
        setPos(newPos);
        animated = true;
    }

    if (opacity() != finalOpacity) {
        animated = true;
        if (qAbs(finalScale - currentScale) > 0.0) {
            // use scale as reference
            setOpacity(opacity() + scaleDelta * (finalOpacity - opacity()) /
                       qAbs(finalScale - currentScale));
        } else {
            setOpacity(finalOpacity);
        }
    }

    if (!animated && deleteAfterAnimation)
        deleteLater();

    return animated;
}

QRectF Cloud::boundingRect() const
{
    return childrenBoundingRect();
}

void Cloud::paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *)
{
}

//! [4]
QVariant Cloud::itemChange(GraphicsItemChange change, const QVariant &value)
{
    switch (change) {
    case ItemPositionHasChanged:
        if (BearerCloud *bearercloud = qobject_cast<BearerCloud *>(scene()))
            bearercloud->cloudMoved();
    default:
        ;
    };

    return QGraphicsItem::itemChange(change, value);
}
//! [4]

//! [3]
void Cloud::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (session->isOpen())
            session->close();
        else
            session->open();

        event->accept();
    }
}
//! [3]

//! [2]
void Cloud::stateChanged(QNetworkSession::State state)
{
    if (configuration.name().isEmpty())
        finalOpacity = qreal(0.1);
    else if (session->state() == QNetworkSession::NotAvailable)
        finalOpacity = 0.5;
    else
        finalOpacity = 1.0;

#if !defined(Q_OS_WINCE)
    QString tooltip;

    if (configuration.name().isEmpty())
        tooltip += tr("<b>HIDDEN NETWORK</b><br>");
    else
        tooltip += tr("<b>%1</b><br>").arg(configuration.name());

#ifndef QT_NO_NETWORKINTERFACE
    const QNetworkInterface interface = session->interface();
    if (interface.isValid())
        tooltip += tr("<br>Interface: %1").arg(interface.humanReadableName());
    tooltip += tr("<br>Id: %1").arg(configuration.identifier());
#endif

    const QString bearerTypeName = configuration.bearerTypeName();
    if (!bearerTypeName.isEmpty())
        tooltip += tr("<br>Bearer: %1").arg(bearerTypeName);

    QString s = tr("<br>State: %1 (%2)");
    switch (state) {
    case QNetworkSession::Invalid:
        s = s.arg(tr("Invalid"));
        break;
    case QNetworkSession::NotAvailable:
        s = s.arg(tr("Not Available"));
        break;
    case QNetworkSession::Connecting:
        s = s.arg(tr("Connecting"));
        break;
    case QNetworkSession::Connected:
        s = s.arg(tr("Connected"));
        break;
    case QNetworkSession::Closing:
        s = s.arg(tr("Closing"));
        break;
    case QNetworkSession::Disconnected:
        s = s.arg(tr("Disconnected"));
        break;
    case QNetworkSession::Roaming:
        s = s.arg(tr("Roaming"));
        break;
    default:
        s = s.arg(tr("Unknown"));
    }

    if (session->isOpen())
        s = s.arg(tr("Open"));
    else
        s = s.arg(tr("Closed"));

    tooltip += s;

    tooltip += tr("<br><br>Active time: %1 seconds").arg(session->activeTime());
    tooltip += tr("<br>Received data: %1 bytes").arg(session->bytesReceived());
    tooltip += tr("<br>Sent data: %1 bytes").arg(session->bytesWritten());

    setToolTip(tooltip);
#else
    Q_UNUSED(state);
#endif
}
//! [2]

//! [1]
void Cloud::newConfigurationActivated()
{
    QNetworkConfiguration::BearerType bearerType = configuration.bearerType();
    if (!svgCache.contains(bearerType)) {
        QSvgRenderer *renderer = 0;
        switch (bearerType) {
        case QNetworkConfiguration::BearerWLAN:
            renderer = new QSvgRenderer(QLatin1String(":wlan.svg"));
            break;
        case QNetworkConfiguration::BearerEthernet:
            renderer = new QSvgRenderer(QLatin1String(":lan.svg"));
            break;
        case QNetworkConfiguration::Bearer2G:
            renderer = new QSvgRenderer(QLatin1String(":cell.svg"));
            break;
        case QNetworkConfiguration::BearerBluetooth:
            renderer = new QSvgRenderer(QLatin1String(":bluetooth.svg"));
            break;
        case QNetworkConfiguration::BearerCDMA2000:
        case QNetworkConfiguration::BearerWCDMA:
        case QNetworkConfiguration::BearerHSPA:
            renderer = new QSvgRenderer(QLatin1String(":umts.svg"));
            break;
        default:
            renderer = new QSvgRenderer(QLatin1String(":unknown.svg"));
        }

        if (renderer)
            svgCache.insert(bearerType, renderer);
    }

    icon->setSharedRenderer(svgCache[bearerType]);

    if (configuration.name().isEmpty()) {
        text->setPlainText(tr("HIDDEN NETWORK"));
    } else {
        if (configuration.type() == QNetworkConfiguration::ServiceNetwork)
            text->setHtml("<b>" + configuration.name() + "</b>");
        else
            text->setPlainText(configuration.name());
    }

    const qreal height = icon->boundingRect().height() + text->boundingRect().height();

    icon->setPos(icon->boundingRect().width() / -2, height / -2);

    text->setPos(text->boundingRect().width() / -2,
                 height / 2 - text->boundingRect().height());

    stateChanged(session->state());
}
//! [1]

qreal Cloud::getRadiusForState(QNetworkConfiguration::StateFlags state)
{
    switch (state) {
    case QNetworkConfiguration::Active:
        return 100;
        break;
    case QNetworkConfiguration::Discovered:
        return 150;
        break;
    case QNetworkConfiguration::Defined:
        return 200;
        break;
    case QNetworkConfiguration::Undefined:
        return 250;
        break;
    default:
        return 300;
    }
}

