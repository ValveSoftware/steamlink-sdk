/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
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

#include "declarativeseries_p.h"
#include <QtCore/QMetaMethod>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

static void setSeriesGradient(QAbstract3DSeries *series, const ColorGradient &gradient,
                              GradientType type)
{
    QLinearGradient newGradient;
    QGradientStops stops;
    QList<ColorGradientStop *> qmlstops = gradient.m_stops;

    // Get sorted gradient stops
    for (int i = 0; i < qmlstops.size(); i++) {
        int j = 0;
        while (j < stops.size() && stops.at(j).first < qmlstops[i]->position())
            j++;
        stops.insert(j, QGradientStop(qmlstops.at(i)->position(), qmlstops.at(i)->color()));
    }

    newGradient.setStops(stops);
    switch (type) {
    case GradientTypeBase:
        series->setBaseGradient(newGradient);
        break;
    case GradientTypeSingle:
        series->setSingleHighlightGradient(newGradient);
        break;
    case GradientTypeMulti:
        series->setMultiHighlightGradient(newGradient);
        break;
    default: // Never goes here
        break;
    }
}

static void connectSeriesGradient(QAbstract3DSeries *series, ColorGradient *newGradient,
                                  GradientType type, ColorGradient **memberGradient)
{
    // connect new / disconnect old
    if (newGradient != *memberGradient) {
        if (*memberGradient)
            QObject::disconnect(*memberGradient, 0, series, 0);

        *memberGradient = newGradient;

        int updatedIndex = newGradient->metaObject()->indexOfSignal("updated()");
        QMetaMethod updateFunction = newGradient->metaObject()->method(updatedIndex);
        int handleIndex = -1;
        switch (type) {
        case GradientTypeBase:
            handleIndex = series->metaObject()->indexOfSlot("handleBaseGradientUpdate()");
            break;
        case GradientTypeSingle:
            handleIndex = series->metaObject()->indexOfSlot("handleSingleHighlightGradientUpdate()");
            break;
        case GradientTypeMulti:
            handleIndex = series->metaObject()->indexOfSlot("handleMultiHighlightGradientUpdate()");
            break;
        default: // Never goes here
            break;
        }
        QMetaMethod handleFunction = series->metaObject()->method(handleIndex);

        if (*memberGradient)
            QObject::connect(*memberGradient, updateFunction, series, handleFunction);
    }

    if (*memberGradient)
        setSeriesGradient(series, **memberGradient, type);
}

DeclarativeBar3DSeries::DeclarativeBar3DSeries(QObject *parent)
    : QBar3DSeries(parent),
      m_baseGradient(0),
      m_singleHighlightGradient(0),
      m_multiHighlightGradient(0)
{
    QObject::connect(this, &QBar3DSeries::selectedBarChanged, this,
                     &DeclarativeBar3DSeries::selectedBarChanged);
}

DeclarativeBar3DSeries::~DeclarativeBar3DSeries()
{
}

QQmlListProperty<QObject> DeclarativeBar3DSeries::seriesChildren()
{
    return QQmlListProperty<QObject>(this, this, &DeclarativeBar3DSeries::appendSeriesChildren
                                     , 0, 0, 0);
}

void DeclarativeBar3DSeries::appendSeriesChildren(QQmlListProperty<QObject> *list, QObject *element)
{
    QBarDataProxy *proxy = qobject_cast<QBarDataProxy *>(element);
    if (proxy)
        reinterpret_cast<DeclarativeBar3DSeries *>(list->data)->setDataProxy(proxy);
}

void DeclarativeBar3DSeries::setSelectedBar(const QPointF &position)
{
    QBar3DSeries::setSelectedBar(position.toPoint());
}

QPointF DeclarativeBar3DSeries::selectedBar() const
{
    return QPointF(QBar3DSeries::selectedBar());
}

QPointF DeclarativeBar3DSeries::invalidSelectionPosition() const
{
    return QPointF(QBar3DSeries::invalidSelectionPosition());
}

void DeclarativeBar3DSeries::setBaseGradient(ColorGradient *gradient)
{
    connectSeriesGradient(this, gradient, GradientTypeBase, &m_baseGradient);
}

ColorGradient *DeclarativeBar3DSeries::baseGradient() const
{
    return m_baseGradient;
}

void DeclarativeBar3DSeries::setSingleHighlightGradient(ColorGradient *gradient)
{
    connectSeriesGradient(this, gradient, GradientTypeSingle, &m_singleHighlightGradient);
}

ColorGradient *DeclarativeBar3DSeries::singleHighlightGradient() const
{
    return m_singleHighlightGradient;
}

void DeclarativeBar3DSeries::setMultiHighlightGradient(ColorGradient *gradient)
{
    connectSeriesGradient(this, gradient, GradientTypeMulti, &m_multiHighlightGradient);
}

ColorGradient *DeclarativeBar3DSeries::multiHighlightGradient() const
{
    return m_multiHighlightGradient;
}

void DeclarativeBar3DSeries::handleBaseGradientUpdate()
{
    if (m_baseGradient)
        setSeriesGradient(this, *m_baseGradient, GradientTypeBase);
}

void DeclarativeBar3DSeries::handleSingleHighlightGradientUpdate()
{
    if (m_singleHighlightGradient)
        setSeriesGradient(this, *m_singleHighlightGradient, GradientTypeSingle);
}

void DeclarativeBar3DSeries::handleMultiHighlightGradientUpdate()
{
    if (m_multiHighlightGradient)
        setSeriesGradient(this, *m_multiHighlightGradient, GradientTypeMulti);
}

DeclarativeScatter3DSeries::DeclarativeScatter3DSeries(QObject *parent)
    : QScatter3DSeries(parent),
      m_baseGradient(0),
      m_singleHighlightGradient(0),
      m_multiHighlightGradient(0)
{
}

DeclarativeScatter3DSeries::~DeclarativeScatter3DSeries()
{
}

QQmlListProperty<QObject> DeclarativeScatter3DSeries::seriesChildren()
{
    return QQmlListProperty<QObject>(this, this, &DeclarativeScatter3DSeries::appendSeriesChildren
                                     , 0, 0, 0);
}

void DeclarativeScatter3DSeries::appendSeriesChildren(QQmlListProperty<QObject> *list,
                                                      QObject *element)
{
    QScatterDataProxy *proxy = qobject_cast<QScatterDataProxy *>(element);
    if (proxy)
        reinterpret_cast<DeclarativeScatter3DSeries *>(list->data)->setDataProxy(proxy);
}

void DeclarativeScatter3DSeries::setBaseGradient(ColorGradient *gradient)
{
    connectSeriesGradient(this, gradient, GradientTypeBase, &m_baseGradient);
}

ColorGradient *DeclarativeScatter3DSeries::baseGradient() const
{
    return m_baseGradient;
}

void DeclarativeScatter3DSeries::setSingleHighlightGradient(ColorGradient *gradient)
{
    connectSeriesGradient(this, gradient, GradientTypeSingle, &m_singleHighlightGradient);
}

ColorGradient *DeclarativeScatter3DSeries::singleHighlightGradient() const
{
    return m_singleHighlightGradient;
}

void DeclarativeScatter3DSeries::setMultiHighlightGradient(ColorGradient *gradient)
{
    connectSeriesGradient(this, gradient, GradientTypeMulti, &m_multiHighlightGradient);
}

ColorGradient *DeclarativeScatter3DSeries::multiHighlightGradient() const
{
    return m_multiHighlightGradient;
}

int DeclarativeScatter3DSeries::invalidSelectionIndex() const
{
    return QScatter3DSeries::invalidSelectionIndex();
}

void DeclarativeScatter3DSeries::handleBaseGradientUpdate()
{
    if (m_baseGradient)
        setSeriesGradient(this, *m_baseGradient, GradientTypeBase);
}

void DeclarativeScatter3DSeries::handleSingleHighlightGradientUpdate()
{
    if (m_singleHighlightGradient)
        setSeriesGradient(this, *m_singleHighlightGradient, GradientTypeSingle);
}

void DeclarativeScatter3DSeries::handleMultiHighlightGradientUpdate()
{
    if (m_multiHighlightGradient)
        setSeriesGradient(this, *m_multiHighlightGradient, GradientTypeMulti);
}

DeclarativeSurface3DSeries::DeclarativeSurface3DSeries(QObject *parent)
    : QSurface3DSeries(parent),
      m_baseGradient(0),
      m_singleHighlightGradient(0),
      m_multiHighlightGradient(0)
{
    QObject::connect(this, &QSurface3DSeries::selectedPointChanged, this,
                     &DeclarativeSurface3DSeries::selectedPointChanged);
}

DeclarativeSurface3DSeries::~DeclarativeSurface3DSeries()
{
}

void DeclarativeSurface3DSeries::setSelectedPoint(const QPointF &position)
{
    QSurface3DSeries::setSelectedPoint(position.toPoint());
}

QPointF DeclarativeSurface3DSeries::selectedPoint() const
{
    return QPointF(QSurface3DSeries::selectedPoint());
}

QPointF DeclarativeSurface3DSeries::invalidSelectionPosition() const
{
    return QPointF(QSurface3DSeries::invalidSelectionPosition());
}

QQmlListProperty<QObject> DeclarativeSurface3DSeries::seriesChildren()
{
    return QQmlListProperty<QObject>(this, this, &DeclarativeSurface3DSeries::appendSeriesChildren
                                     , 0, 0, 0);
}

void DeclarativeSurface3DSeries::appendSeriesChildren(QQmlListProperty<QObject> *list,
                                                      QObject *element)
{
    QSurfaceDataProxy *proxy = qobject_cast<QSurfaceDataProxy *>(element);
    if (proxy)
        reinterpret_cast<DeclarativeSurface3DSeries *>(list->data)->setDataProxy(proxy);
}

void DeclarativeSurface3DSeries::setBaseGradient(ColorGradient *gradient)
{
    connectSeriesGradient(this, gradient, GradientTypeBase, &m_baseGradient);
}

ColorGradient *DeclarativeSurface3DSeries::baseGradient() const
{
    return m_baseGradient;
}

void DeclarativeSurface3DSeries::setSingleHighlightGradient(ColorGradient *gradient)
{
    connectSeriesGradient(this, gradient, GradientTypeSingle, &m_singleHighlightGradient);
}

ColorGradient *DeclarativeSurface3DSeries::singleHighlightGradient() const
{
    return m_singleHighlightGradient;
}

void DeclarativeSurface3DSeries::setMultiHighlightGradient(ColorGradient *gradient)
{
    connectSeriesGradient(this, gradient, GradientTypeMulti, &m_multiHighlightGradient);
}

ColorGradient *DeclarativeSurface3DSeries::multiHighlightGradient() const
{
    return m_multiHighlightGradient;
}

void DeclarativeSurface3DSeries::handleBaseGradientUpdate()
{
    if (m_baseGradient)
        setSeriesGradient(this, *m_baseGradient, GradientTypeBase);
}

void DeclarativeSurface3DSeries::handleSingleHighlightGradientUpdate()
{
    if (m_singleHighlightGradient)
        setSeriesGradient(this, *m_singleHighlightGradient, GradientTypeSingle);
}

void DeclarativeSurface3DSeries::handleMultiHighlightGradientUpdate()
{
    if (m_multiHighlightGradient)
        setSeriesGradient(this, *m_multiHighlightGradient, GradientTypeMulti);
}

QT_END_NAMESPACE_DATAVISUALIZATION
