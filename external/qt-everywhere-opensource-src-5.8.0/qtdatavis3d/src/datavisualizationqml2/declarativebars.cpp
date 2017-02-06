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

#include "declarativebars_p.h"
#include <QtCore/QMutexLocker>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

DeclarativeBars::DeclarativeBars(QQuickItem *parent)
    : AbstractDeclarative(parent),
      m_barsController(0)
{
    setAcceptedMouseButtons(Qt::AllButtons);

    // Create the shared component on the main GUI thread.
    m_barsController = new Bars3DController(boundingRect().toRect(), new Declarative3DScene);
    AbstractDeclarative::setSharedController(m_barsController);

    QObject::connect(m_barsController, &Bars3DController::primarySeriesChanged,
                     this, &DeclarativeBars::primarySeriesChanged);
    QObject::connect(m_barsController, &Bars3DController::selectedSeriesChanged,
                     this, &DeclarativeBars::selectedSeriesChanged);
}

DeclarativeBars::~DeclarativeBars()
{
    QMutexLocker locker(m_nodeMutex.data());
    delete m_barsController;
}

QCategory3DAxis *DeclarativeBars::rowAxis() const
{
    return static_cast<QCategory3DAxis *>(m_barsController->axisZ());
}

void DeclarativeBars::setRowAxis(QCategory3DAxis *axis)
{
    m_barsController->setAxisZ(axis);
}

QValue3DAxis *DeclarativeBars::valueAxis() const
{
    return static_cast<QValue3DAxis *>(m_barsController->axisY());
}

void DeclarativeBars::setValueAxis(QValue3DAxis *axis)
{
    m_barsController->setAxisY(axis);
}

QCategory3DAxis *DeclarativeBars::columnAxis() const
{
    return static_cast<QCategory3DAxis *>(m_barsController->axisX());
}

void DeclarativeBars::setColumnAxis(QCategory3DAxis *axis)
{
    m_barsController->setAxisX(axis);
}

void DeclarativeBars::setMultiSeriesUniform(bool uniform)
{
    if (uniform != isMultiSeriesUniform()) {
        m_barsController->setMultiSeriesScaling(uniform);
        emit multiSeriesUniformChanged(uniform);
    }
}

bool DeclarativeBars::isMultiSeriesUniform() const
{
    return m_barsController->multiSeriesScaling();
}

void DeclarativeBars::setBarThickness(float thicknessRatio)
{
    if (thicknessRatio != barThickness()) {
        m_barsController->setBarSpecs(GLfloat(thicknessRatio), barSpacing(),
                                      isBarSpacingRelative());
        emit barThicknessChanged(thicknessRatio);
    }
}

float DeclarativeBars::barThickness() const
{
    return m_barsController->barThickness();
}

void DeclarativeBars::setBarSpacing(QSizeF spacing)
{
    if (spacing != barSpacing()) {
        m_barsController->setBarSpecs(GLfloat(barThickness()), spacing, isBarSpacingRelative());
        emit barSpacingChanged(spacing);
    }
}

QSizeF DeclarativeBars::barSpacing() const
{
    return m_barsController->barSpacing();
}

void DeclarativeBars::setBarSpacingRelative(bool relative)
{
    if (relative != isBarSpacingRelative()) {
        m_barsController->setBarSpecs(GLfloat(barThickness()), barSpacing(), relative);
        emit barSpacingRelativeChanged(relative);
    }
}

bool DeclarativeBars::isBarSpacingRelative() const
{
    return m_barsController->isBarSpecRelative();
}

QBar3DSeries *DeclarativeBars::selectedSeries() const
{
    return m_barsController->selectedSeries();
}

void DeclarativeBars::setFloorLevel(float level)
{
    if (level != floorLevel()) {
        m_barsController->setFloorLevel(level);
        emit floorLevelChanged(level);
    }
}

float DeclarativeBars::floorLevel() const
{
    return m_barsController->floorLevel();
}

QQmlListProperty<QBar3DSeries> DeclarativeBars::seriesList()
{
    return QQmlListProperty<QBar3DSeries>(this, this,
                                          &DeclarativeBars::appendSeriesFunc,
                                          &DeclarativeBars::countSeriesFunc,
                                          &DeclarativeBars::atSeriesFunc,
                                          &DeclarativeBars::clearSeriesFunc);
}

void DeclarativeBars::appendSeriesFunc(QQmlListProperty<QBar3DSeries> *list, QBar3DSeries *series)
{
    reinterpret_cast<DeclarativeBars *>(list->data)->addSeries(series);
}

int DeclarativeBars::countSeriesFunc(QQmlListProperty<QBar3DSeries> *list)
{
    return reinterpret_cast<DeclarativeBars *>(list->data)->m_barsController->barSeriesList().size();
}

QBar3DSeries *DeclarativeBars::atSeriesFunc(QQmlListProperty<QBar3DSeries> *list, int index)
{
    return reinterpret_cast<DeclarativeBars *>(list->data)->m_barsController->barSeriesList().at(index);
}

void DeclarativeBars::clearSeriesFunc(QQmlListProperty<QBar3DSeries> *list)
{
    DeclarativeBars *declBars = reinterpret_cast<DeclarativeBars *>(list->data);
    QList<QBar3DSeries *> realList = declBars->m_barsController->barSeriesList();
    int count = realList.size();
    for (int i = 0; i < count; i++)
        declBars->removeSeries(realList.at(i));
}

void DeclarativeBars::addSeries(QBar3DSeries *series)
{
    m_barsController->addSeries(series);
}

void DeclarativeBars::removeSeries(QBar3DSeries *series)
{
    m_barsController->removeSeries(series);
    series->setParent(this); // Reparent as removing will leave series parentless
}

void DeclarativeBars::insertSeries(int index, QBar3DSeries *series)
{
    m_barsController->insertSeries(index, series);
}

void DeclarativeBars::setPrimarySeries(QBar3DSeries *series)
{
    m_barsController->setPrimarySeries(series);
}

QBar3DSeries *DeclarativeBars::primarySeries() const
{
    return m_barsController->primarySeries();
}

void DeclarativeBars::handleAxisXChanged(QAbstract3DAxis *axis)
{
    emit columnAxisChanged(static_cast<QCategory3DAxis *>(axis));
}

void DeclarativeBars::handleAxisYChanged(QAbstract3DAxis *axis)
{
    emit valueAxisChanged(static_cast<QValue3DAxis *>(axis));
}

void DeclarativeBars::handleAxisZChanged(QAbstract3DAxis *axis)
{
    emit rowAxisChanged(static_cast<QCategory3DAxis *>(axis));
}

QT_END_NAMESPACE_DATAVISUALIZATION
