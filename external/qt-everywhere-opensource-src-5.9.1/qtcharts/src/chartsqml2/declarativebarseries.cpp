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

#include "declarativebarseries.h"
#include <QtCharts/QBarSet>
#include <QtCharts/QVBarModelMapper>
#include <QtCharts/QHBarModelMapper>

QT_CHARTS_BEGIN_NAMESPACE

DeclarativeBarSet::DeclarativeBarSet(QObject *parent)
    : QBarSet("", parent)
{
    connect(this, SIGNAL(valuesAdded(int,int)), this, SLOT(handleCountChanged(int,int)));
    connect(this, SIGNAL(valuesRemoved(int,int)), this, SLOT(handleCountChanged(int,int)));
    connect(this, SIGNAL(brushChanged()), this, SLOT(handleBrushChanged()));
}

void DeclarativeBarSet::handleCountChanged(int index, int count)
{
    Q_UNUSED(index)
    Q_UNUSED(count)
    emit countChanged(QBarSet::count());
}

qreal DeclarativeBarSet::borderWidth() const
{
    return pen().widthF();
}

void DeclarativeBarSet::setBorderWidth(qreal width)
{
    if (width != pen().widthF()) {
        QPen p = pen();
        p.setWidthF(width);
        setPen(p);
        emit borderWidthChanged(width);
    }
}

QVariantList DeclarativeBarSet::values()
{
    QVariantList values;
    for (int i(0); i < count(); i++)
        values.append(QVariant(QBarSet::at(i)));
    return values;
}

void DeclarativeBarSet::setValues(QVariantList values)
{
    while (count())
        remove(count() - 1);

    if (values.count() > 0 && values.at(0).canConvert(QVariant::Point)) {
        // Create list of values for appending if the first item is Qt.point
        int maxValue = 0;
        for (int i = 0; i < values.count(); i++) {
            if (values.at(i).canConvert(QVariant::Point) &&
                    values.at(i).toPoint().x() > maxValue) {
                maxValue = values.at(i).toPoint().x();
            }
        }

        QVector<qreal> indexValueList;
        indexValueList.resize(maxValue + 1);

        for (int i = 0; i < values.count(); i++) {
            if (values.at(i).canConvert(QVariant::Point)) {
                indexValueList.replace(values.at(i).toPoint().x(), values.at(i).toPointF().y());
            }
        }

        for (int i = 0; i < indexValueList.count(); i++)
            QBarSet::append(indexValueList.at(i));

    } else {
        for (int i(0); i < values.count(); i++) {
            if (values.at(i).canConvert(QVariant::Double))
                QBarSet::append(values[i].toDouble());
        }
    }
}

QString DeclarativeBarSet::brushFilename() const
{
    return m_brushFilename;
}

void DeclarativeBarSet::setBrushFilename(const QString &brushFilename)
{
    QImage brushImage(brushFilename);
    if (QBarSet::brush().textureImage() != brushImage) {
        QBrush brush = QBarSet::brush();
        brush.setTextureImage(brushImage);
        QBarSet::setBrush(brush);
        m_brushFilename = brushFilename;
        m_brushImage = brushImage;
        emit brushFilenameChanged(brushFilename);
    }
}

void DeclarativeBarSet::handleBrushChanged()
{
    // If the texture image of the brush has changed along the brush
    // the brush file name needs to be cleared.
    if (!m_brushFilename.isEmpty() && QBarSet::brush().textureImage() != m_brushImage) {
        m_brushFilename.clear();
        emit brushFilenameChanged(QString(""));
    }
}

// Declarative bar series ======================================================================================
DeclarativeBarSeries::DeclarativeBarSeries(QQuickItem *parent) :
    QBarSeries(parent),
    m_axes(new DeclarativeAxes(this))
{
    connect(m_axes, SIGNAL(axisXChanged(QAbstractAxis*)), this, SIGNAL(axisXChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYChanged(QAbstractAxis*)), this, SIGNAL(axisYChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisXTopChanged(QAbstractAxis*)), this, SIGNAL(axisXTopChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYRightChanged(QAbstractAxis*)), this, SIGNAL(axisYRightChanged(QAbstractAxis*)));
}

void DeclarativeBarSeries::classBegin()
{
}

void DeclarativeBarSeries::componentComplete()
{
    foreach (QObject *child, children()) {
        if (qobject_cast<DeclarativeBarSet *>(child)) {
            QAbstractBarSeries::append(qobject_cast<DeclarativeBarSet *>(child));
        } else if (qobject_cast<QVBarModelMapper *>(child)) {
            QVBarModelMapper *mapper = qobject_cast<QVBarModelMapper *>(child);
            mapper->setSeries(this);
        } else if (qobject_cast<QHBarModelMapper *>(child)) {
            QHBarModelMapper *mapper = qobject_cast<QHBarModelMapper *>(child);
            mapper->setSeries(this);
        }
    }
}

QQmlListProperty<QObject> DeclarativeBarSeries::seriesChildren()
{
    return QQmlListProperty<QObject>(this, 0, &DeclarativeBarSeries::appendSeriesChildren ,0,0,0);
}

void DeclarativeBarSeries::appendSeriesChildren(QQmlListProperty<QObject> *list, QObject *element)
{
    // Empty implementation; the children are parsed in componentComplete instead
    Q_UNUSED(list);
    Q_UNUSED(element);
}

DeclarativeBarSet *DeclarativeBarSeries::at(int index)
{
    QList<QBarSet *> setList = barSets();
    if (index >= 0 && index < setList.count())
        return qobject_cast<DeclarativeBarSet *>(setList[index]);

    return 0;
}

DeclarativeBarSet *DeclarativeBarSeries::insert(int index, QString label, QVariantList values)
{
    DeclarativeBarSet *barset = new DeclarativeBarSet(this);
    barset->setLabel(label);
    barset->setValues(values);
    if (QBarSeries::insert(index, barset))
        return barset;
    delete barset;
    return 0;
}

// Declarative stacked bar series ==============================================================================
DeclarativeStackedBarSeries::DeclarativeStackedBarSeries(QQuickItem *parent) :
    QStackedBarSeries(parent),
    m_axes(0)
{
    m_axes = new DeclarativeAxes(this);
    connect(m_axes, SIGNAL(axisXChanged(QAbstractAxis*)), this, SIGNAL(axisXChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYChanged(QAbstractAxis*)), this, SIGNAL(axisYChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisXTopChanged(QAbstractAxis*)), this, SIGNAL(axisXTopChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYRightChanged(QAbstractAxis*)), this, SIGNAL(axisYRightChanged(QAbstractAxis*)));
}

void DeclarativeStackedBarSeries::classBegin()
{
}

void DeclarativeStackedBarSeries::componentComplete()
{
    foreach (QObject *child, children()) {
        if (qobject_cast<DeclarativeBarSet *>(child)) {
            QAbstractBarSeries::append(qobject_cast<DeclarativeBarSet *>(child));
        } else if (qobject_cast<QVBarModelMapper *>(child)) {
            QVBarModelMapper *mapper = qobject_cast<QVBarModelMapper *>(child);
            mapper->setSeries(this);
        } else if (qobject_cast<QHBarModelMapper *>(child)) {
            QHBarModelMapper *mapper = qobject_cast<QHBarModelMapper *>(child);
            mapper->setSeries(this);
        }
    }
}


QQmlListProperty<QObject> DeclarativeStackedBarSeries::seriesChildren()
{
    return QQmlListProperty<QObject>(this, 0, &DeclarativeBarSeries::appendSeriesChildren ,0,0,0);
}

void DeclarativeStackedBarSeries::appendSeriesChildren(QQmlListProperty<QObject> * list, QObject *element)
{
    // Empty implementation; the children are parsed in componentComplete instead
    Q_UNUSED(list);
    Q_UNUSED(element);
}

DeclarativeBarSet *DeclarativeStackedBarSeries::at(int index)
{
    QList<QBarSet *> setList = barSets();
    if (index >= 0 && index < setList.count())
        return qobject_cast<DeclarativeBarSet *>(setList[index]);

    return 0;
}

DeclarativeBarSet *DeclarativeStackedBarSeries::insert(int index, QString label, QVariantList values)
{
    DeclarativeBarSet *barset = new DeclarativeBarSet(this);
    barset->setLabel(label);
    barset->setValues(values);
    if (QStackedBarSeries::insert(index, barset))
        return barset;
    delete barset;
    return 0;
}

// Declarative percent bar series ==============================================================================
DeclarativePercentBarSeries::DeclarativePercentBarSeries(QQuickItem *parent) :
    QPercentBarSeries(parent),
    m_axes(0)
{
    m_axes = new DeclarativeAxes(this);
    connect(m_axes, SIGNAL(axisXChanged(QAbstractAxis*)), this, SIGNAL(axisXChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYChanged(QAbstractAxis*)), this, SIGNAL(axisYChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisXTopChanged(QAbstractAxis*)), this, SIGNAL(axisXTopChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYRightChanged(QAbstractAxis*)), this, SIGNAL(axisYRightChanged(QAbstractAxis*)));
}

void DeclarativePercentBarSeries::classBegin()
{
}

void DeclarativePercentBarSeries::componentComplete()
{
    foreach (QObject *child, children()) {
        if (qobject_cast<DeclarativeBarSet *>(child)) {
            QAbstractBarSeries::append(qobject_cast<DeclarativeBarSet *>(child));
        } else if (qobject_cast<QVBarModelMapper *>(child)) {
            QVBarModelMapper *mapper = qobject_cast<QVBarModelMapper *>(child);
            mapper->setSeries(this);
        } else if (qobject_cast<QHBarModelMapper *>(child)) {
            QHBarModelMapper *mapper = qobject_cast<QHBarModelMapper *>(child);
            mapper->setSeries(this);
        }
    }
}

QQmlListProperty<QObject> DeclarativePercentBarSeries::seriesChildren()
{
    return QQmlListProperty<QObject>(this, 0, &DeclarativeBarSeries::appendSeriesChildren ,0,0,0);
}

void DeclarativePercentBarSeries::appendSeriesChildren(QQmlListProperty<QObject> * list, QObject *element)
{
    // Empty implementation; the children are parsed in componentComplete instead
    Q_UNUSED(list);
    Q_UNUSED(element);
}

DeclarativeBarSet *DeclarativePercentBarSeries::at(int index)
{
    QList<QBarSet *> setList = barSets();
    if (index >= 0 && index < setList.count())
        return qobject_cast<DeclarativeBarSet *>(setList[index]);

    return 0;
}

DeclarativeBarSet *DeclarativePercentBarSeries::insert(int index, QString label, QVariantList values)
{
    DeclarativeBarSet *barset = new DeclarativeBarSet(this);
    barset->setLabel(label);
    barset->setValues(values);
    if (QPercentBarSeries::insert(index, barset))
        return barset;
    delete barset;
    return 0;
}

// Declarative horizontal bar series ===========================================================================
DeclarativeHorizontalBarSeries::DeclarativeHorizontalBarSeries(QQuickItem *parent) :
    QHorizontalBarSeries(parent),
    m_axes(0)
{
    m_axes = new DeclarativeAxes(this);
    connect(m_axes, SIGNAL(axisXChanged(QAbstractAxis*)), this, SIGNAL(axisXChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYChanged(QAbstractAxis*)), this, SIGNAL(axisYChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisXTopChanged(QAbstractAxis*)), this, SIGNAL(axisXTopChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYRightChanged(QAbstractAxis*)), this, SIGNAL(axisYRightChanged(QAbstractAxis*)));
}

void DeclarativeHorizontalBarSeries::classBegin()
{
}

void DeclarativeHorizontalBarSeries::componentComplete()
{
    foreach (QObject *child, children()) {
        if (qobject_cast<DeclarativeBarSet *>(child)) {
            QAbstractBarSeries::append(qobject_cast<DeclarativeBarSet *>(child));
        } else if (qobject_cast<QVBarModelMapper *>(child)) {
            QVBarModelMapper *mapper = qobject_cast<QVBarModelMapper *>(child);
            mapper->setSeries(this);
        } else if (qobject_cast<QHBarModelMapper *>(child)) {
            QHBarModelMapper *mapper = qobject_cast<QHBarModelMapper *>(child);
            mapper->setSeries(this);
        }
    }
}

QQmlListProperty<QObject> DeclarativeHorizontalBarSeries::seriesChildren()
{
    return QQmlListProperty<QObject>(this, 0, &DeclarativeHorizontalBarSeries::appendSeriesChildren ,0,0,0);
}

void DeclarativeHorizontalBarSeries::appendSeriesChildren(QQmlListProperty<QObject> * list, QObject *element)
{
    // Empty implementation; the children are parsed in componentComplete instead
    Q_UNUSED(list);
    Q_UNUSED(element);
}

DeclarativeBarSet *DeclarativeHorizontalBarSeries::at(int index)
{
    QList<QBarSet *> setList = barSets();
    if (index >= 0 && index < setList.count())
        return qobject_cast<DeclarativeBarSet *>(setList[index]);

    return 0;
}

DeclarativeBarSet *DeclarativeHorizontalBarSeries::insert(int index, QString label, QVariantList values)
{
    DeclarativeBarSet *barset = new DeclarativeBarSet(this);
    barset->setLabel(label);
    barset->setValues(values);
    if (QHorizontalBarSeries::insert(index, barset))
        return barset;
    delete barset;
    return 0;
}

// Declarative horizontal stacked bar series ===================================================================
DeclarativeHorizontalStackedBarSeries::DeclarativeHorizontalStackedBarSeries(QQuickItem *parent) :
    QHorizontalStackedBarSeries(parent),
    m_axes(0)
{
    m_axes = new DeclarativeAxes(this);
    connect(m_axes, SIGNAL(axisXChanged(QAbstractAxis*)), this, SIGNAL(axisXChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYChanged(QAbstractAxis*)), this, SIGNAL(axisYChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisXTopChanged(QAbstractAxis*)), this, SIGNAL(axisXTopChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYRightChanged(QAbstractAxis*)), this, SIGNAL(axisYRightChanged(QAbstractAxis*)));
}

void DeclarativeHorizontalStackedBarSeries::classBegin()
{
}

void DeclarativeHorizontalStackedBarSeries::componentComplete()
{
    foreach (QObject *child, children()) {
        if (qobject_cast<DeclarativeBarSet *>(child)) {
            QAbstractBarSeries::append(qobject_cast<DeclarativeBarSet *>(child));
        } else if (qobject_cast<QVBarModelMapper *>(child)) {
            QVBarModelMapper *mapper = qobject_cast<QVBarModelMapper *>(child);
            mapper->setSeries(this);
        } else if (qobject_cast<QHBarModelMapper *>(child)) {
            QHBarModelMapper *mapper = qobject_cast<QHBarModelMapper *>(child);
            mapper->setSeries(this);
        }
    }
}

QQmlListProperty<QObject> DeclarativeHorizontalStackedBarSeries::seriesChildren()
{
    return QQmlListProperty<QObject>(this, 0, &DeclarativeHorizontalStackedBarSeries::appendSeriesChildren ,0,0,0);
}

void DeclarativeHorizontalStackedBarSeries::appendSeriesChildren(QQmlListProperty<QObject> * list, QObject *element)
{
    // Empty implementation; the children are parsed in componentComplete instead
    Q_UNUSED(list);
    Q_UNUSED(element);
}

DeclarativeBarSet *DeclarativeHorizontalStackedBarSeries::at(int index)
{
    QList<QBarSet *> setList = barSets();
    if (index >= 0 && index < setList.count())
        return qobject_cast<DeclarativeBarSet *>(setList[index]);

    return 0;
}

DeclarativeBarSet *DeclarativeHorizontalStackedBarSeries::insert(int index, QString label, QVariantList values)
{
    DeclarativeBarSet *barset = new DeclarativeBarSet(this);
    barset->setLabel(label);
    barset->setValues(values);
    if (QHorizontalStackedBarSeries::insert(index, barset))
        return barset;
    delete barset;
    return 0;
}

// Declarative horizontal percent bar series ===================================================================
DeclarativeHorizontalPercentBarSeries::DeclarativeHorizontalPercentBarSeries(QQuickItem *parent) :
    QHorizontalPercentBarSeries(parent),
    m_axes(0)
{
    m_axes = new DeclarativeAxes(this);
    connect(m_axes, SIGNAL(axisXChanged(QAbstractAxis*)), this, SIGNAL(axisXChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYChanged(QAbstractAxis*)), this, SIGNAL(axisYChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisXTopChanged(QAbstractAxis*)), this, SIGNAL(axisXTopChanged(QAbstractAxis*)));
    connect(m_axes, SIGNAL(axisYRightChanged(QAbstractAxis*)), this, SIGNAL(axisYRightChanged(QAbstractAxis*)));
}

void DeclarativeHorizontalPercentBarSeries::classBegin()
{
}

void DeclarativeHorizontalPercentBarSeries::componentComplete()
{
    foreach (QObject *child, children()) {
        if (qobject_cast<DeclarativeBarSet *>(child)) {
            QAbstractBarSeries::append(qobject_cast<DeclarativeBarSet *>(child));
        } else if (qobject_cast<QVBarModelMapper *>(child)) {
            QVBarModelMapper *mapper = qobject_cast<QVBarModelMapper *>(child);
            mapper->setSeries(this);
        } else if (qobject_cast<QHBarModelMapper *>(child)) {
            QHBarModelMapper *mapper = qobject_cast<QHBarModelMapper *>(child);
            mapper->setSeries(this);
        }
    }
}

QQmlListProperty<QObject> DeclarativeHorizontalPercentBarSeries::seriesChildren()
{
    return QQmlListProperty<QObject>(this, 0, &DeclarativeHorizontalPercentBarSeries::appendSeriesChildren ,0,0,0);
}

void DeclarativeHorizontalPercentBarSeries::appendSeriesChildren(QQmlListProperty<QObject> * list, QObject *element)
{
    // Empty implementation; the children are parsed in componentComplete instead
    Q_UNUSED(list);
    Q_UNUSED(element);
}

DeclarativeBarSet *DeclarativeHorizontalPercentBarSeries::at(int index)
{
    QList<QBarSet *> setList = barSets();
    if (index >= 0 && index < setList.count())
        return qobject_cast<DeclarativeBarSet *>(setList[index]);

    return 0;
}

DeclarativeBarSet *DeclarativeHorizontalPercentBarSeries::insert(int index, QString label, QVariantList values)
{
    DeclarativeBarSet *barset = new DeclarativeBarSet(this);
    barset->setLabel(label);
    barset->setValues(values);
    if (QHorizontalPercentBarSeries::insert(index, barset))
        return barset;
    delete barset;
    return 0;
}

#include "moc_declarativebarseries.cpp"

QT_CHARTS_END_NAMESPACE
