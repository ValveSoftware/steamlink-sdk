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

#include <QtCharts/QHCandlestickModelMapper>
#include <QtCharts/QVCandlestickModelMapper>
#include "declarativeaxes.h"
#include "declarativecandlestickseries.h"

QT_CHARTS_BEGIN_NAMESPACE

DeclarativeCandlestickSet::DeclarativeCandlestickSet(qreal timestamp, QObject *parent)
    : QCandlestickSet(timestamp, parent)
{
    connect(this, SIGNAL(brushChanged()), this, SLOT(handleBrushChanged()));
}

void DeclarativeCandlestickSet::setBrushFilename(const QString &brushFilename)
{
    QImage brushImage(brushFilename);
    if (QCandlestickSet::brush().textureImage() != brushImage) {
        QBrush brush = QCandlestickSet::brush();
        brush.setTextureImage(brushImage);

        QCandlestickSet::setBrush(brush);

        m_brushFilename = brushFilename;
        m_brushImage = brushImage;

        emit brushFilenameChanged(brushFilename);
    }
}

QString DeclarativeCandlestickSet::brushFilename() const
{
    return m_brushFilename;
}

void DeclarativeCandlestickSet::handleBrushChanged()
{
    // If the texture image of the brush has changed along the brush
    // the brush file name needs to be cleared.
    if (!m_brushFilename.isEmpty() && QCandlestickSet::brush().textureImage() != m_brushImage) {
        m_brushFilename.clear();
        emit brushFilenameChanged(QString());
    }
}

// Declarative candlestick series ==================================================================

DeclarativeCandlestickSeries::DeclarativeCandlestickSeries(QQuickItem *parent)
    : QCandlestickSeries(parent),
      m_axes(new DeclarativeAxes(this))
{
    connect(m_axes, SIGNAL(axisXChanged(QAbstractAxis*)),
            this, SIGNAL(axisXChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYChanged(QAbstractAxis*)),
            this, SIGNAL(axisYChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisXTopChanged(QAbstractAxis*)),
            this, SIGNAL(axisXTopChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYRightChanged(QAbstractAxis*)),
            this, SIGNAL(axisYRightChanged(QAbstractAxis*)));

    connect(this, SIGNAL(hovered(bool, QCandlestickSet *)),
            this, SLOT(onHovered(bool, QCandlestickSet *)));
    connect(this, SIGNAL(clicked(QCandlestickSet *)), this, SLOT(onClicked(QCandlestickSet *)));
    connect(this, SIGNAL(pressed(QCandlestickSet *)), this, SLOT(onPressed(QCandlestickSet *)));
    connect(this, SIGNAL(released(QCandlestickSet *)), this, SLOT(onReleased(QCandlestickSet *)));
    connect(this, SIGNAL(doubleClicked(QCandlestickSet *)),
            this, SLOT(onDoubleClicked(QCandlestickSet *)));

    connect(this, SIGNAL(brushChanged()), this, SLOT(handleBrushChanged()));
}

QQmlListProperty<QObject> DeclarativeCandlestickSeries::seriesChildren()
{
    return QQmlListProperty<QObject>(this, 0, &DeclarativeCandlestickSeries::appendSeriesChildren,
                                     0, 0, 0);
}

void DeclarativeCandlestickSeries::setBrushFilename(const QString &brushFilename)
{
    QImage brushImage(brushFilename);
    if (QCandlestickSeries::brush().textureImage() != brushImage) {
        QBrush brush = QCandlestickSeries::brush();
        brush.setTextureImage(brushImage);

        QCandlestickSeries::setBrush(brush);

        m_brushFilename = brushFilename;
        m_brushImage = brushImage;

        emit brushFilenameChanged(brushFilename);
    }
}

QString DeclarativeCandlestickSeries::brushFilename() const
{
    return m_brushFilename;
}

DeclarativeCandlestickSet *DeclarativeCandlestickSeries::at(int index)
{
    QList<QCandlestickSet *> sets = this->sets();
    if (index >= 0 && index < sets.count())
        return qobject_cast<DeclarativeCandlestickSet *>(sets[index]);

    return 0;
}

bool DeclarativeCandlestickSeries::append(DeclarativeCandlestickSet *set)
{
    return QCandlestickSeries::append(qobject_cast<QCandlestickSet *>(set));
}

bool DeclarativeCandlestickSeries::remove(DeclarativeCandlestickSet *set)
{
    return QCandlestickSeries::remove(qobject_cast<QCandlestickSet *>(set));
}

bool DeclarativeCandlestickSeries::append(qreal open, qreal high, qreal low, qreal close,
                                          qreal timestamp)
{
    QCandlestickSet *set = new QCandlestickSet(open, high, low, close, timestamp);
    if (!QCandlestickSeries::append(set)) {
        delete set;
        return false;
    }

    return true;
}

bool DeclarativeCandlestickSeries::remove(qreal timestamp)
{
    for (int i = 0; i < count(); ++i) {
        QCandlestickSet *set = sets().at(i);
        if (set->timestamp() == timestamp)
            return QCandlestickSeries::remove(set);
    }

    return false;
}

bool DeclarativeCandlestickSeries::insert(int index, DeclarativeCandlestickSet *set)
{
    return QCandlestickSeries::insert(index, qobject_cast<QCandlestickSet *>(set));
}

void DeclarativeCandlestickSeries::clear()
{
    QCandlestickSeries::clear();
}

void DeclarativeCandlestickSeries::classBegin()
{
    // do nothing
}

void DeclarativeCandlestickSeries::componentComplete()
{
    foreach (QObject *child, children()) {
        if (qobject_cast<DeclarativeCandlestickSet *>(child)) {
            QCandlestickSeries::append(qobject_cast<DeclarativeCandlestickSet *>(child));
        } else if (qobject_cast<QHCandlestickModelMapper *>(child)) {
            QHCandlestickModelMapper *mapper = qobject_cast<QHCandlestickModelMapper *>(child);
            mapper->setSeries(this);
        } else if (qobject_cast<QVCandlestickModelMapper *>(child)) {
            QVCandlestickModelMapper *mapper = qobject_cast<QVCandlestickModelMapper *>(child);
            mapper->setSeries(this);
        } // else: do nothing
    }
}

void DeclarativeCandlestickSeries::appendSeriesChildren(QQmlListProperty<QObject> *list,
                                                        QObject *element)
{
    // Empty implementation; the children are parsed in componentComplete instead
    Q_UNUSED(list);
    Q_UNUSED(element);
}

void DeclarativeCandlestickSeries::onClicked(QCandlestickSet *set)
{
    emit clicked(qobject_cast<DeclarativeCandlestickSet *>(set));
}

void DeclarativeCandlestickSeries::onHovered(bool status, QCandlestickSet *set)
{
    emit hovered(status, qobject_cast<DeclarativeCandlestickSet *>(set));
}

void DeclarativeCandlestickSeries::onPressed(QCandlestickSet *set)
{
    emit pressed(qobject_cast<DeclarativeCandlestickSet *>(set));
}

void DeclarativeCandlestickSeries::onReleased(QCandlestickSet *set)
{
    emit released(qobject_cast<DeclarativeCandlestickSet *>(set));
}

void DeclarativeCandlestickSeries::onDoubleClicked(QCandlestickSet *set)
{
    emit doubleClicked(qobject_cast<DeclarativeCandlestickSet *>(set));
}

void DeclarativeCandlestickSeries::handleBrushChanged()
{
    // If the texture image of the brush has changed along the brush
    // the brush file name needs to be cleared.
    if (!m_brushFilename.isEmpty() && QCandlestickSeries::brush().textureImage() != m_brushImage) {
        m_brushFilename.clear();
        emit brushFilenameChanged(QString());
    }
}

#include "moc_declarativecandlestickseries.cpp"

QT_CHARTS_END_NAMESPACE
