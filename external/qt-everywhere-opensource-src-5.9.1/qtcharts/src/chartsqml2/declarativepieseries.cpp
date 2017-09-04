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

#include "declarativepieseries.h"
#include <QtCharts/QPieSlice>
#include <QtCharts/QVPieModelMapper>
#include <QtCharts/QHPieModelMapper>

QT_CHARTS_BEGIN_NAMESPACE

DeclarativePieSlice::DeclarativePieSlice(QObject *parent)
    : QPieSlice(parent)
{
    connect(this, SIGNAL(brushChanged()), this, SLOT(handleBrushChanged()));
}

QString DeclarativePieSlice::brushFilename() const
{
    return m_brushFilename;
}

void DeclarativePieSlice::setBrushFilename(const QString &brushFilename)
{
    QImage brushImage(brushFilename);
    if (QPieSlice::brush().textureImage() != brushImage) {
        QBrush brush = QPieSlice::brush();
        brush.setTextureImage(brushImage);
        QPieSlice::setBrush(brush);
        m_brushFilename = brushFilename;
        m_brushImage = brushImage;
        emit brushFilenameChanged(brushFilename);
    }
}

void DeclarativePieSlice::handleBrushChanged()
{
    // If the texture image of the brush has changed along the brush
    // the brush file name needs to be cleared.
    if (!m_brushFilename.isEmpty() && QPieSlice::brush().textureImage() != m_brushImage) {
        m_brushFilename.clear();
        emit brushFilenameChanged(QString(""));
    }
}

// Declarative pie series =========================================================================
DeclarativePieSeries::DeclarativePieSeries(QQuickItem *parent) :
    QPieSeries(parent)
{
    connect(this, SIGNAL(added(QList<QPieSlice*>)), this, SLOT(handleAdded(QList<QPieSlice*>)));
    connect(this, SIGNAL(removed(QList<QPieSlice*>)), this, SLOT(handleRemoved(QList<QPieSlice*>)));
}

void DeclarativePieSeries::classBegin()
{
}

void DeclarativePieSeries::componentComplete()
{
    foreach (QObject *child, children()) {
        if (qobject_cast<QPieSlice *>(child)) {
            QPieSeries::append(qobject_cast<QPieSlice *>(child));
        } else if (qobject_cast<QVPieModelMapper *>(child)) {
            QVPieModelMapper *mapper = qobject_cast<QVPieModelMapper *>(child);
            mapper->setSeries(this);
        } else if (qobject_cast<QHPieModelMapper *>(child)) {
            QHPieModelMapper *mapper = qobject_cast<QHPieModelMapper *>(child);
            mapper->setSeries(this);
        }
    }
}

QQmlListProperty<QObject> DeclarativePieSeries::seriesChildren()
{
    return QQmlListProperty<QObject>(this, 0, &DeclarativePieSeries::appendSeriesChildren ,0,0,0);
}

void DeclarativePieSeries::appendSeriesChildren(QQmlListProperty<QObject> * list, QObject *element)
{
    // Empty implementation; the children are parsed in componentComplete instead
    Q_UNUSED(list);
    Q_UNUSED(element);
}

QPieSlice *DeclarativePieSeries::at(int index)
{
    QList<QPieSlice *> sliceList = slices();
    if (index >= 0 && index < sliceList.count())
        return sliceList[index];

    return 0;
}

QPieSlice *DeclarativePieSeries::find(QString label)
{
    foreach (QPieSlice *slice, slices()) {
        if (slice->label() == label)
            return slice;
    }
    return 0;
}

DeclarativePieSlice *DeclarativePieSeries::append(QString label, qreal value)
{
    DeclarativePieSlice *slice = new DeclarativePieSlice(this);
    slice->setLabel(label);
    slice->setValue(value);
    if (QPieSeries::append(slice))
        return slice;
    delete slice;
    return 0;
}

bool DeclarativePieSeries::remove(QPieSlice *slice)
{
    return QPieSeries::remove(slice);
}

void DeclarativePieSeries::clear()
{
    QPieSeries::clear();
}

void DeclarativePieSeries::handleAdded(QList<QPieSlice *> slices)
{
    foreach (QPieSlice *slice, slices)
        emit sliceAdded(slice);
}

void DeclarativePieSeries::handleRemoved(QList<QPieSlice *> slices)
{
    foreach (QPieSlice *slice, slices)
        emit sliceRemoved(slice);
}

#include "moc_declarativepieseries.cpp"

QT_CHARTS_END_NAMESPACE
