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

#include "declarativesurface_p.h"
#include <QtCore/QMutexLocker>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

DeclarativeSurface::DeclarativeSurface(QQuickItem *parent)
    : AbstractDeclarative(parent),
      m_surfaceController(0)
{
    setAcceptedMouseButtons(Qt::AllButtons);

    // Create the shared component on the main GUI thread.
    m_surfaceController = new Surface3DController(boundingRect().toRect(), new Declarative3DScene);
    setSharedController(m_surfaceController);

    QObject::connect(m_surfaceController, &Surface3DController::selectedSeriesChanged,
                     this, &DeclarativeSurface::selectedSeriesChanged);
    QObject::connect(m_surfaceController, &Surface3DController::flipHorizontalGridChanged,
                     this, &DeclarativeSurface::flipHorizontalGridChanged);
}

DeclarativeSurface::~DeclarativeSurface()
{
    QMutexLocker locker(m_nodeMutex.data());
    const QMutexLocker locker2(mutex());
    delete m_surfaceController;
}

QValue3DAxis *DeclarativeSurface::axisX() const
{
    return static_cast<QValue3DAxis *>(m_surfaceController->axisX());
}

void DeclarativeSurface::setAxisX(QValue3DAxis *axis)
{
    m_surfaceController->setAxisX(axis);
}

QValue3DAxis *DeclarativeSurface::axisY() const
{
    return static_cast<QValue3DAxis *>(m_surfaceController->axisY());
}

void DeclarativeSurface::setAxisY(QValue3DAxis *axis)
{
    m_surfaceController->setAxisY(axis);
}

QValue3DAxis *DeclarativeSurface::axisZ() const
{
    return static_cast<QValue3DAxis *>(m_surfaceController->axisZ());
}

void DeclarativeSurface::setAxisZ(QValue3DAxis *axis)
{
    m_surfaceController->setAxisZ(axis);
}

QSurface3DSeries *DeclarativeSurface::selectedSeries() const
{
    return m_surfaceController->selectedSeries();
}

void DeclarativeSurface::setFlipHorizontalGrid(bool flip)
{
   m_surfaceController->setFlipHorizontalGrid(flip);
}

bool DeclarativeSurface::flipHorizontalGrid() const
{
    return m_surfaceController->flipHorizontalGrid();
}

QQmlListProperty<QSurface3DSeries> DeclarativeSurface::seriesList()
{
    return QQmlListProperty<QSurface3DSeries>(this, this,
                                              &DeclarativeSurface::appendSeriesFunc,
                                              &DeclarativeSurface::countSeriesFunc,
                                              &DeclarativeSurface::atSeriesFunc,
                                              &DeclarativeSurface::clearSeriesFunc);
}

void DeclarativeSurface::appendSeriesFunc(QQmlListProperty<QSurface3DSeries> *list,
                                          QSurface3DSeries *series)
{
    reinterpret_cast<DeclarativeSurface *>(list->data)->addSeries(series);
}

int DeclarativeSurface::countSeriesFunc(QQmlListProperty<QSurface3DSeries> *list)
{
    return reinterpret_cast<DeclarativeSurface *>(list->data)->m_surfaceController->surfaceSeriesList().size();
}

QSurface3DSeries *DeclarativeSurface::atSeriesFunc(QQmlListProperty<QSurface3DSeries> *list, int index)
{
    return reinterpret_cast<DeclarativeSurface *>(list->data)->m_surfaceController->surfaceSeriesList().at(index);
}

void DeclarativeSurface::clearSeriesFunc(QQmlListProperty<QSurface3DSeries> *list)
{
    DeclarativeSurface *declSurface = reinterpret_cast<DeclarativeSurface *>(list->data);
    QList<QSurface3DSeries *> realList = declSurface->m_surfaceController->surfaceSeriesList();
    int count = realList.size();
    for (int i = 0; i < count; i++)
        declSurface->removeSeries(realList.at(i));
}

void DeclarativeSurface::addSeries(QSurface3DSeries *series)
{
    m_surfaceController->addSeries(series);
}

void DeclarativeSurface::removeSeries(QSurface3DSeries *series)
{
    m_surfaceController->removeSeries(series);
    series->setParent(this); // Reparent as removing will leave series parentless
}

void DeclarativeSurface::handleAxisXChanged(QAbstract3DAxis *axis)
{
    emit axisXChanged(static_cast<QValue3DAxis *>(axis));
}

void DeclarativeSurface::handleAxisYChanged(QAbstract3DAxis *axis)
{
    emit axisYChanged(static_cast<QValue3DAxis *>(axis));
}

void DeclarativeSurface::handleAxisZChanged(QAbstract3DAxis *axis)
{
    emit axisZChanged(static_cast<QValue3DAxis *>(axis));
}

QT_END_NAMESPACE_DATAVISUALIZATION
