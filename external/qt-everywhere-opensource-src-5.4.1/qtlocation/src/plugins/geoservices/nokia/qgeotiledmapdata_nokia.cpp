/****************************************************************************
 **
 ** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtLocation module of the Qt Toolkit.
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
 ** This file is part of the Nokia services plugin for the Maps and
 ** Navigation API.  The use of these services, whether by use of the
 ** plugin or by other means, is governed by the terms and conditions
 ** described by the file NOKIA_TERMS_AND_CONDITIONS.txt in
 ** this package, located in the directory containing the Nokia services
 ** plugin source code.
 **
 ****************************************************************************/

#include "qgeotiledmapdata_nokia.h"
#include "qgeotiledmappingmanagerengine_nokia.h"
#include "qgeomapcontroller_p.h"

#include <QDebug>
#include <QObject>
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QImage>
#include <QRect>

#include <QStaticText>

QT_BEGIN_NAMESPACE

/*!
 Constructs a new tiled map data object, which stores the map data required by
 \a geoMap and makes use of the functionality provided by \a engine.
 */
QGeoTiledMapDataNokia::QGeoTiledMapDataNokia(QGeoTiledMappingManagerEngineNokia *engine, QObject *parent /*= 0*/) :
    QGeoTiledMapData(engine, parent),
    logo(":/images/logo.png") // HERE logo image
{}

QGeoTiledMapDataNokia::~QGeoTiledMapDataNokia() {}

void QGeoTiledMapDataNokia::evaluateCopyrights(const QSet<QGeoTileSpec> &visibleTiles)
{
    const int copyrightsMargin = 10;
    const int spaceToLogo = 4;
    const int blurRate = 1;
    const int fontSize = 10;

    QGeoTiledMappingManagerEngineNokia *engineNokia = static_cast<QGeoTiledMappingManagerEngineNokia *>(engine());
    const QString copyrightsString = engineNokia->evaluateCopyrightsText(activeMapType(), mapController()->zoom(), visibleTiles);

    if (width() > 0 && height() > 0 && ((copyrightsString.isNull() && copyrightsSlab.isNull()) || copyrightsString != lastCopyrightsString)) {
        QFont font("Sans Serif");
        font.setPixelSize(fontSize);
        font.setStyleHint(QFont::SansSerif);
        font.setWeight(QFont::Bold);

        QRect textBounds = QFontMetrics(font).boundingRect(0, 0, width(), height(), Qt::AlignBottom | Qt::AlignLeft | Qt::TextWordWrap, copyrightsString);

        copyrightsSlab = QImage(logo.width() + textBounds.width() + spaceToLogo + blurRate * 2,
                                qMax(logo.height(), textBounds.height() + blurRate * 2),
                                QImage::Format_ARGB32_Premultiplied);
        copyrightsSlab.fill(Qt::transparent);

        QPainter painter(&copyrightsSlab);
        painter.drawImage(QPoint(0, copyrightsSlab.height() - logo.height()), logo);
        painter.setFont(font);
        painter.setPen(QColor(0, 0, 0, 64));
        painter.translate(spaceToLogo + logo.width(), -blurRate);
        for (int x=-blurRate; x<=blurRate; ++x) {
            for (int y=-blurRate; y<=blurRate; ++y) {
                painter.drawText(x, y, textBounds.width(), copyrightsSlab.height(),
                                 Qt::AlignBottom | Qt::AlignLeft | Qt::TextWordWrap,
                                 copyrightsString);
            }
        }
        painter.setPen(Qt::white);
        painter.drawText(0, 0, textBounds.width(), copyrightsSlab.height(),
                         Qt::AlignBottom | Qt::AlignLeft | Qt::TextWordWrap,
                         copyrightsString);
        painter.end();

        QPoint copyrightsPos(copyrightsMargin, height() - (copyrightsSlab.height() + copyrightsMargin));
        lastCopyrightsPos = copyrightsPos;
        emit copyrightsChanged(copyrightsSlab, copyrightsPos);

        lastCopyrightsString = copyrightsString;
    }

    QPoint copyrightsPos(copyrightsMargin, height() - (copyrightsSlab.height() + copyrightsMargin));
    if (copyrightsPos != lastCopyrightsPos) {
        lastCopyrightsPos = copyrightsPos;
        emit copyrightsChanged(copyrightsSlab, copyrightsPos);
    }
}

int QGeoTiledMapDataNokia::mapVersion()
{
    QGeoTiledMappingManagerEngineNokia *engineNokia = static_cast<QGeoTiledMappingManagerEngineNokia *>(engine());
    return engineNokia->mapVersion();
}

QT_END_NAMESPACE
