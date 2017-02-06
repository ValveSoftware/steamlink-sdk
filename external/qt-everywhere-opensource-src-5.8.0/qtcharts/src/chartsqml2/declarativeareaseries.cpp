/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "declarativeareaseries.h"
#include "declarativelineseries.h"

QT_CHARTS_BEGIN_NAMESPACE

DeclarativeAreaSeries::DeclarativeAreaSeries(QObject *parent) :
    QAreaSeries(parent),
    m_axes(new DeclarativeAxes(this))
{
    connect(m_axes, SIGNAL(axisXChanged(QAbstractAxis*)), this, SIGNAL(axisXChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYChanged(QAbstractAxis*)), this, SIGNAL(axisYChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisXTopChanged(QAbstractAxis*)), this, SIGNAL(axisXTopChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYRightChanged(QAbstractAxis*)), this, SIGNAL(axisYRightChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisXChanged(QAbstractAxis*)), this, SIGNAL(axisAngularChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYChanged(QAbstractAxis*)), this, SIGNAL(axisRadialChanged(QAbstractAxis*)));
    connect(this, SIGNAL(brushChanged()), this, SLOT(handleBrushChanged()));
}

void DeclarativeAreaSeries::setUpperSeries(DeclarativeLineSeries *series)
{
    QAreaSeries::setUpperSeries(series);
}

DeclarativeLineSeries *DeclarativeAreaSeries::upperSeries() const
{
    return qobject_cast<DeclarativeLineSeries *>(QAreaSeries::upperSeries());
}

void DeclarativeAreaSeries::setLowerSeries(DeclarativeLineSeries *series)
{
    QAreaSeries::setLowerSeries(series);
}

DeclarativeLineSeries *DeclarativeAreaSeries::lowerSeries() const
{
    return qobject_cast<DeclarativeLineSeries *>(QAreaSeries::lowerSeries());
}

qreal DeclarativeAreaSeries::borderWidth() const
{
    return pen().widthF();
}

void DeclarativeAreaSeries::setBorderWidth(qreal width)
{
    if (width != pen().widthF()) {
        QPen p = pen();
        p.setWidthF(width);
        setPen(p);
        emit borderWidthChanged(width);
    }
}

QString DeclarativeAreaSeries::brushFilename() const
{
    return m_brushFilename;
}

void DeclarativeAreaSeries::setBrushFilename(const QString &brushFilename)
{
    QImage brushImage(brushFilename);
    if (QAreaSeries::brush().textureImage() != brushImage) {
        QBrush brush = QAreaSeries::brush();
        brush.setTextureImage(brushImage);
        QAreaSeries::setBrush(brush);
        m_brushFilename = brushFilename;
        m_brushImage = brushImage;
        emit brushFilenameChanged(brushFilename);
    }
}

void DeclarativeAreaSeries::handleBrushChanged()
{
    // If the texture image of the brush has changed along the brush
    // the brush file name needs to be cleared.
    if (!m_brushFilename.isEmpty() && QAreaSeries::brush().textureImage() != m_brushImage) {
        m_brushFilename.clear();
        emit brushFilenameChanged(QString(""));
    }
}

void DeclarativeAreaSeries::setBrush(const QBrush &brush)
{
    QAreaSeries::setBrush(brush);
    emit brushChanged();
}

QBrush DeclarativeAreaSeries::brush() const
{
    return QAreaSeries::brush();
}

#include "moc_declarativeareaseries.cpp"

QT_CHARTS_END_NAMESPACE
