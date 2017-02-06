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

#include "q3dscatter.h"
#include "q3dscatter_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class Q3DScatter
 * \inmodule QtDataVisualization
 * \brief The Q3DScatter class provides methods for rendering 3D scatter graphs.
 * \since QtDataVisualization 1.0
 *
 * This class enables developers to render scatter graphs in 3D and to view them by rotating the scene
 * freely. Rotation is done by holding down the right mouse button and moving the mouse. Zooming
 * is done by mouse wheel. Selection, if enabled, is done by left mouse button. The scene can be
 * reset to default camera view by clicking mouse wheel. In touch devices rotation is done
 * by tap-and-move, selection by tap-and-hold and zoom by pinch.
 *
 * If no axes are set explicitly to Q3DScatter, temporary default axes with no labels are created.
 * These default axes can be modified via axis accessors, but as soon any axis is set explicitly
 * for the orientation, the default axis for that orientation is destroyed.
 *
 * Q3DScatter supports more than one series visible at the same time.
 *
 * \section1 How to construct a minimal Q3DScatter graph
 *
 * First, construct Q3DScatter. Since we are running the graph as top level window
 * in this example, we need to clear the \c Qt::FramelessWindowHint flag, which gets set by
 * default:
 *
 * \snippet doc_src_q3dscatter_construction.cpp 0
 *
 * Now Q3DScatter is ready to receive data to be rendered. Add one series of 3 QVector3D items:
 *
 * \snippet doc_src_q3dscatter_construction.cpp 1
 *
 * Finally you will need to set it visible:
 *
 * \snippet doc_src_q3dscatter_construction.cpp 2
 *
 * The complete code needed to create and display this graph is:
 *
 * \snippet doc_src_q3dscatter_construction.cpp 3
 *
 * And this is what those few lines of code produce:
 *
 * \image q3dscatter-minimal.png
 *
 * The scene can be rotated, zoomed into, and an item can be selected to view its position,
 * but no other interaction is included in this minimal code example.
 * You can learn more by familiarizing yourself with the examples provided, like
 * the \l{Scatter Example}.
 *
 * \sa Q3DBars, Q3DSurface, {Qt Data Visualization C++ Classes}
 */

/*!
 * Constructs a new 3D scatter graph with optional \a parent window
 * and surface \a format.
 */
Q3DScatter::Q3DScatter(const QSurfaceFormat *format, QWindow *parent)
    : QAbstract3DGraph(new Q3DScatterPrivate(this), format, parent)
{
    if (!dptr()->m_initialized)
        return;

    dptr()->m_shared = new Scatter3DController(geometry());
    d_ptr->setVisualController(dptr()->m_shared);
    dptr()->m_shared->initializeOpenGL();
    QObject::connect(dptr()->m_shared, &Scatter3DController::selectedSeriesChanged,
                     this, &Q3DScatter::selectedSeriesChanged);
}

/*!
 * Destroys the 3D scatter graph.
 */
Q3DScatter::~Q3DScatter()
{
}

/*!
 * Adds the \a series to the graph. A graph can contain multiple series, but has only one set of
 * axes. If the newly added series has specified a selected item, it will be highlighted and
 * any existing selection will be cleared. Only one added series can have an active selection.
 */
void Q3DScatter::addSeries(QScatter3DSeries *series)
{
    dptr()->m_shared->addSeries(series);
}

/*!
 * Removes the \a series from the graph.
 */
void Q3DScatter::removeSeries(QScatter3DSeries *series)
{
    dptr()->m_shared->removeSeries(series);
}

/*!
 * \return list of series added to this graph.
 */
QList<QScatter3DSeries *> Q3DScatter::seriesList() const
{
    return dptrc()->m_shared->scatterSeriesList();
}

Q3DScatterPrivate *Q3DScatter::dptr()
{
    return static_cast<Q3DScatterPrivate *>(d_ptr.data());
}

const Q3DScatterPrivate *Q3DScatter::dptrc() const
{
    return static_cast<const Q3DScatterPrivate *>(d_ptr.data());
}

/*!
 * \property Q3DScatter::axisX
 *
 * The active X-axis. Implicitly calls addAxis() to transfer ownership
 * of the \a axis to this graph.
 *
 * If the \a axis is null, a temporary default axis with no labels and automatically adjusting
 * range is created.
 * This temporary axis is destroyed if another \a axis is set explicitly to the same orientation.
 *
 * \sa addAxis(), releaseAxis()
 */
void Q3DScatter::setAxisX(QValue3DAxis *axis)
{
    dptr()->m_shared->setAxisX(axis);
}

QValue3DAxis *Q3DScatter::axisX() const
{
    return static_cast<QValue3DAxis *>(dptrc()->m_shared->axisX());
}

/*!
 * \property Q3DScatter::axisY
 *
 * The active Y-axis. Implicitly calls addAxis() to transfer ownership
 * of the \a axis to this graph.
 *
 * If the \a axis is null, a temporary default axis with no labels and automatically adjusting
 * range is created.
 * This temporary axis is destroyed if another \a axis is set explicitly to the same orientation.
 *
 * \sa addAxis(), releaseAxis()
 */
void Q3DScatter::setAxisY(QValue3DAxis *axis)
{
    dptr()->m_shared->setAxisY(axis);
}

QValue3DAxis *Q3DScatter::axisY() const
{
    return static_cast<QValue3DAxis *>(dptrc()->m_shared->axisY());
}

/*!
 * \property Q3DScatter::axisZ
 *
 * The active Z-axis. Implicitly calls addAxis() to transfer ownership
 * of the \a axis to this graph.
 *
 * If the \a axis is null, a temporary default axis with no labels and automatically adjusting
 * range is created.
 * This temporary axis is destroyed if another \a axis is set explicitly to the same orientation.
 *
 * \sa addAxis(), releaseAxis()
 */
void Q3DScatter::setAxisZ(QValue3DAxis *axis)
{
    dptr()->m_shared->setAxisZ(axis);
}

/*!
 * \return used Z-axis.
 */
QValue3DAxis *Q3DScatter::axisZ() const
{
    return static_cast<QValue3DAxis *>(dptrc()->m_shared->axisZ());
}

/*!
 * \property Q3DScatter::selectedSeries
 *
 * The selected series or \c null.
 */
QScatter3DSeries *Q3DScatter::selectedSeries() const
{
    return dptrc()->m_shared->selectedSeries();
}

/*!
 * Adds \a axis to the graph. The axes added via addAxis are not yet taken to use,
 * addAxis is simply used to give the ownership of the \a axis to the graph.
 * The \a axis must not be null or added to another graph.
 *
 * \sa releaseAxis(), setAxisX(), setAxisY(), setAxisZ()
 */
void Q3DScatter::addAxis(QValue3DAxis *axis)
{
    dptr()->m_shared->addAxis(axis);
}

/*!
 * Releases the ownership of the \a axis back to the caller, if it is added to this graph.
 * If the released \a axis is in use, a new default axis will be created and set active.
 *
 * If the default axis is released and added back later, it behaves as any other axis would.
 *
 * \sa addAxis(), setAxisX(), setAxisY(), setAxisZ()
 */
void Q3DScatter::releaseAxis(QValue3DAxis *axis)
{
    dptr()->m_shared->releaseAxis(axis);
}

/*!
 * \return list of all added axes.
 *
 * \sa addAxis()
 */
QList<QValue3DAxis *> Q3DScatter::axes() const
{
    QList<QAbstract3DAxis *> abstractAxes = dptrc()->m_shared->axes();
    QList<QValue3DAxis *> retList;
    foreach (QAbstract3DAxis *axis, abstractAxes)
        retList.append(static_cast<QValue3DAxis *>(axis));

    return retList;
}

Q3DScatterPrivate::Q3DScatterPrivate(Q3DScatter *q)
    : QAbstract3DGraphPrivate(q),
    m_shared(0)
{
}

Q3DScatterPrivate::~Q3DScatterPrivate()
{
}

void Q3DScatterPrivate::handleAxisXChanged(QAbstract3DAxis *axis)
{
    emit qptr()->axisXChanged(static_cast<QValue3DAxis *>(axis));
}

void Q3DScatterPrivate::handleAxisYChanged(QAbstract3DAxis *axis)
{
    emit qptr()->axisYChanged(static_cast<QValue3DAxis *>(axis));
}

void Q3DScatterPrivate::handleAxisZChanged(QAbstract3DAxis *axis)
{
    emit qptr()->axisZChanged(static_cast<QValue3DAxis *>(axis));
}

Q3DScatter *Q3DScatterPrivate::qptr()
{
    return static_cast<Q3DScatter *>(q_ptr);
}

QT_END_NAMESPACE_DATAVISUALIZATION

