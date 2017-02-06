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

#include "q3dsurface.h"
#include "q3dsurface_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class Q3DSurface
 * \inmodule QtDataVisualization
 * \brief The Q3DSurface class provides methods for rendering 3D surface plots.
 * \since QtDataVisualization 1.0
 *
 * This class enables developers to render 3D surface plots and to view them by rotating the scene
 * freely. The visual properties of the surface such as draw mode and shading can be controlled
 * via QSurface3DSeries.
 *
 * The Q3DSurface supports selection by showing a highlighted ball on the data point where the user has clicked
 * with left mouse button (when default input handler is in use) or selected via QSurface3DSeries.
 * The selection pointer is accompanied with a label which in default case shows the value of the
 * data point and the coordinates of the point.
 *
 * The value range and the label format shown on the axis can be controlled through QValue3DAxis.
 *
 * To rotate the graph, hold down the right mouse button and move the mouse. Zooming is done using mouse
 * wheel. Both assume the default input handler is in use.
 *
 * If no axes are set explicitly to Q3DSurface, temporary default axes with no labels are created.
 * These default axes can be modified via axis accessors, but as soon any axis is set explicitly
 * for the orientation, the default axis for that orientation is destroyed.
 *
 * \section1 How to construct a minimal Q3DSurface graph
 *
 * First, construct Q3DSurface. Since we are running the graph as top level window
 * in this example, we need to clear the \c Qt::FramelessWindowHint flag, which gets set by
 * default:
 *
 * \snippet doc_src_q3dsurface_construction.cpp 0
 *
 * Now Q3DSurface is ready to receive data to be rendered. Create data elements to receive values:
 *
 * \snippet doc_src_q3dsurface_construction.cpp 1
 *
 * First feed the data to the row elements and then add their pointers to the data element:
 *
 * \snippet doc_src_q3dsurface_construction.cpp 2
 *
 * Create a new series and set data to it:
 *
 * \snippet doc_src_q3dsurface_construction.cpp 3
 *
 * Finally you will need to set it visible:
 *
 * \snippet doc_src_q3dsurface_construction.cpp 4
 *
 * The complete code needed to create and display this graph is:
 *
 * \snippet doc_src_q3dsurface_construction.cpp 5
 *
 * And this is what those few lines of code produce:
 *
 * \image q3dsurface-minimal.png
 *
 * The scene can be rotated, zoomed into, and a surface point can be selected to view its position,
 * but no other interaction is included in this minimal code example.
 * You can learn more by familiarizing yourself with the examples provided,
 * like the \l{Surface Example}.
 *
 *
 * \sa Q3DBars, Q3DScatter, {Qt Data Visualization C++ Classes}
 */

/*!
 * Constructs a new 3D surface graph with optional \a parent window
 * and surface \a format.
 */
Q3DSurface::Q3DSurface(const QSurfaceFormat *format, QWindow *parent)
    : QAbstract3DGraph(new Q3DSurfacePrivate(this), format, parent)
{
    if (!dptr()->m_initialized)
        return;

    dptr()->m_shared = new Surface3DController(geometry());
    d_ptr->setVisualController(dptr()->m_shared);
    dptr()->m_shared->initializeOpenGL();
    QObject::connect(dptr()->m_shared, &Surface3DController::selectedSeriesChanged,
                     this, &Q3DSurface::selectedSeriesChanged);
    QObject::connect(dptr()->m_shared, &Surface3DController::flipHorizontalGridChanged,
                     this, &Q3DSurface::flipHorizontalGridChanged);
}

/*!
 *  Destroys the 3D surface graph.
 */
Q3DSurface::~Q3DSurface()
{
}

/*!
 * Adds the \a series to the graph.  A graph can contain multiple series, but has only one set of
 * axes. If the newly added series has specified a selected item, it will be highlighted and
 * any existing selection will be cleared. Only one added series can have an active selection.
 */
void Q3DSurface::addSeries(QSurface3DSeries *series)
{
    dptr()->m_shared->addSeries(series);
}

/*!
 * Removes the \a series from the graph.
 */
void Q3DSurface::removeSeries(QSurface3DSeries *series)
{
    dptr()->m_shared->removeSeries(series);
}

/*!
 * \return list of series added to this graph.
 */
QList<QSurface3DSeries *> Q3DSurface::seriesList() const
{
    return dptrc()->m_shared->surfaceSeriesList();
}

Q3DSurfacePrivate *Q3DSurface::dptr()
{
    return static_cast<Q3DSurfacePrivate *>(d_ptr.data());
}

const Q3DSurfacePrivate *Q3DSurface::dptrc() const
{
    return static_cast<const Q3DSurfacePrivate *>(d_ptr.data());
}

/*!
 * \property Q3DSurface::axisX
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
void Q3DSurface::setAxisX(QValue3DAxis *axis)
{
    dptr()->m_shared->setAxisX(axis);
}

/*!
 * \return used X-axis.
 */
QValue3DAxis *Q3DSurface::axisX() const
{
    return static_cast<QValue3DAxis *>(dptrc()->m_shared->axisX());
}

/*!
 * \property Q3DSurface::axisY
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
void Q3DSurface::setAxisY(QValue3DAxis *axis)
{
    dptr()->m_shared->setAxisY(axis);
}

/*!
 * \return used Y-axis.
 */
QValue3DAxis *Q3DSurface::axisY() const
{
    return static_cast<QValue3DAxis *>(dptrc()->m_shared->axisY());
}

/*!
 * \property Q3DSurface::axisZ
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
void Q3DSurface::setAxisZ(QValue3DAxis *axis)
{
    dptr()->m_shared->setAxisZ(axis);
}

/*!
 * \return used Z-axis.
 */
QValue3DAxis *Q3DSurface::axisZ() const
{
    return static_cast<QValue3DAxis *>(dptrc()->m_shared->axisZ());
}

/*!
 * \property Q3DSurface::selectedSeries
 *
 * The selected series or \c null. If selectionMode has \c SelectionMultiSeries flag set, this
 * property holds the series which owns the selected point.
 */
QSurface3DSeries *Q3DSurface::selectedSeries() const
{
    return dptrc()->m_shared->selectedSeries();
}

/*!
 * \property Q3DSurface::flipHorizontalGrid
 * \since QtDataVisualization 1.2
 *
 * In some use cases the horizontal axis grid is mostly covered by the surface, so it can be more
 * useful to display the horizontal axis grid on top of the graph rather than on the bottom.
 * A typical use case for this is showing 2D spectrograms using orthoGraphic projection with
 * a top-down viewpoint.
 *
 * If \c{false}, the horizontal axis grid and labels are drawn on the horizontal background
 * of the graph.
 * If \c{true}, the horizontal axis grid and labels are drawn on the opposite side of the graph
 * from the horizontal background.
 * Defaults to \c{false}.
 */
void Q3DSurface::setFlipHorizontalGrid(bool flip)
{
    dptr()->m_shared->setFlipHorizontalGrid(flip);
}

bool Q3DSurface::flipHorizontalGrid() const
{
    return dptrc()->m_shared->flipHorizontalGrid();
}

/*!
 * Adds \a axis to the graph. The axes added via addAxis are not yet taken to use,
 * addAxis is simply used to give the ownership of the \a axis to the graph.
 * The \a axis must not be null or added to another graph.
 *
 * \sa releaseAxis(), setAxisX(), setAxisY(), setAxisZ()
 */
void Q3DSurface::addAxis(QValue3DAxis *axis)
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
void Q3DSurface::releaseAxis(QValue3DAxis *axis)
{
    dptr()->m_shared->releaseAxis(axis);
}

/*!
 * \return list of all added axes.
 *
 * \sa addAxis()
 */
QList<QValue3DAxis *> Q3DSurface::axes() const
{
    QList<QAbstract3DAxis *> abstractAxes = dptrc()->m_shared->axes();
    QList<QValue3DAxis *> retList;
    foreach (QAbstract3DAxis *axis, abstractAxes)
        retList.append(static_cast<QValue3DAxis *>(axis));

    return retList;
}

// Q3DSurfacePrivate

Q3DSurfacePrivate::Q3DSurfacePrivate(Q3DSurface *q)
    : QAbstract3DGraphPrivate(q),
      m_shared(0)
{
}

Q3DSurfacePrivate::~Q3DSurfacePrivate()
{
}

void Q3DSurfacePrivate::handleAxisXChanged(QAbstract3DAxis *axis)
{
    emit qptr()->axisXChanged(static_cast<QValue3DAxis *>(axis));
}

void Q3DSurfacePrivate::handleAxisYChanged(QAbstract3DAxis *axis)
{
    emit qptr()->axisYChanged(static_cast<QValue3DAxis *>(axis));
}

void Q3DSurfacePrivate::handleAxisZChanged(QAbstract3DAxis *axis)
{
    emit qptr()->axisZChanged(static_cast<QValue3DAxis *>(axis));
}

Q3DSurface *Q3DSurfacePrivate::qptr()
{
    return static_cast<Q3DSurface *>(q_ptr);
}

QT_END_NAMESPACE_DATAVISUALIZATION
