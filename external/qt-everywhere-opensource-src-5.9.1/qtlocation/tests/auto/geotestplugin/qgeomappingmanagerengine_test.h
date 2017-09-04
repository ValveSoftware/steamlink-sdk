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

#ifndef QGEOMAPPINGMANAGERENGINE_TEST_H
#define QGEOMAPPINGMANAGERENGINE_TEST_H

#include <qgeoserviceprovider.h>
#include <qgeomappingmanagerengine.h>
#include <QLocale>
#include <QPainter>
#include <QPixmap>
#include <QByteArray>
#include <QBuffer>
#include <qgeotiledmapreply.h>
#include "qgeomaptype.h"
#include "qgeotilespec.h"
#include "qgeocameracapabilities_p.h"

#include <QTimer>
#include <QDebug>
#include <QTimerEvent>

QT_USE_NAMESPACE


class TiledMapReplyTest :public QGeoTiledMapReply
{
    Q_OBJECT
public:
    TiledMapReplyTest(const QGeoTileSpec &spec, QObject *parent=0): QGeoTiledMapReply (spec, parent) {}
    void callSetError ( Error error, const QString & errorString ) {setError(error, errorString);}
    void callSetFinished ( bool finished ) { setFinished(finished);}
    void callSetCached(bool cached) { setFinished(cached);}
    void callSetMapImageData(const QByteArray &data) { setMapImageData(data); }
    void callSetMapImageFormat(const QString &format) { setMapImageFormat(format); }
    void abort() { emit aborted(); }

Q_SIGNALS:
    void aborted();
};

class QGeoMappingManagerEngineTest: public QGeoMappingManagerEngine

{
Q_OBJECT
public:
    QGeoMappingManagerEngineTest(const QMap<QString, QVariant> &parameters,
        QGeoServiceProvider::Error *error, QString *errorString) :
        QGeoMappingManagerEngine(parameters),
        finishRequestImmediately_(true),
        mappingReply_(0),
        timerId_(0),
        errorCode_(QGeoTiledMapReply::NoError)
    {
        Q_UNUSED(error)
        Q_UNUSED(errorString)
        if (parameters.contains("finishRequestImmediately"))
            finishRequestImmediately_ = qvariant_cast<bool>(parameters.value("finishRequestImmediately"));
        setLocale(QLocale (QLocale::German, QLocale::Germany));
        QGeoCameraCapabilities capabilities;
        capabilities.setMinimumZoomLevel(0.0);
        capabilities.setMaximumZoomLevel(20.0);
        capabilities.setSupportsBearing(true);
        setCameraCapabilities(capabilities);
    }

    void init()
    {
        setTileSize(256);
        QList<QGeoMapType> types;
        types << QGeoMapType(QGeoMapType::StreetMap,tr("Street Map"),tr("Test Street Map"), false, 1);
        setSupportedMapTypes(types);
        QGeoMappingManagerEngine::init();
    }

    QGeoTiledMapReply* getTileImage(const QGeoTileSpec &spec)
    {
        mappingReply_ = new TiledMapReplyTest(spec, this);

        QImage im(256, 256, QImage::Format_RGB888);
        im.fill(QColor("lightgray"));
        QRectF rect;
        QString text("X: " + QString::number(spec.x()) + "\nY: " + QString::number(spec.y()) + "\nZ: " + QString::number(spec.zoom()));
        rect.setWidth(250);
        rect.setHeight(250);
        rect.setLeft(3);
        rect.setTop(3);
        QPainter painter;
        QPen pen(QColor("firebrick"));
        painter.begin(&im);
        painter.setPen(pen);
        painter.setFont( QFont("Times", 35, 10, false));
        painter.drawText(rect, text);
        // different border color for vertically and horizontally adjacent frames
        if ((spec.x() + spec.y()) % 2 == 0)
            pen.setColor(QColor("yellow"));
        pen.setWidth(5);
        painter.setPen(pen);
        painter.drawRect(0,0,255,255);
        painter.end();
        QPixmap pm = QPixmap::fromImage(im);
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        pm.save(&buffer, "PNG");

        mappingReply_->callSetMapImageData(bytes);
        mappingReply_->callSetMapImageFormat("png");
        mappingReply_->callSetFinished(true);

        return static_cast<QGeoTiledMapReply*>(mappingReply_);
    }

public Q_SLOTS:
    void requestAborted()
    {
        if (timerId_) {
            killTimer(timerId_);
            timerId_ = 0;
        }
        errorString_ = "";
        errorCode_ = QGeoTiledMapReply::NoError;
    }

protected:
     void timerEvent(QTimerEvent *event)
     {
         Q_ASSERT(timerId_ == event->timerId());
         Q_ASSERT(mappingReply_);
         killTimer(timerId_);
         timerId_ = 0;
         if (errorCode_) {
             mappingReply_->callSetError(errorCode_, errorString_);
             emit tileError(mappingReply_->tileSpec(), errorString_);
        } else {
             mappingReply_->callSetError(QGeoTiledMapReply::NoError, "no error");
             mappingReply_->callSetFinished(true);
         }
         // emit finished(mappingReply_); todo tileFinished
     }

private:
    bool finishRequestImmediately_;
    TiledMapReplyTest* mappingReply_;
    int timerId_;
    QGeoTiledMapReply::Error errorCode_;
    QString errorString_;
};

#endif
