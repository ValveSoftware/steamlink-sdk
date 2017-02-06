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

#include "q3dbars_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class Q3DBars
 * \inmodule QtDataVisualization
 * \brief The Q3DBars class provides methods for rendering 3D bar graphs.
 * \since QtDataVisualization 1.0
 *
 * This class enables developers to render bar graphs in 3D and to view them by rotating the scene
 * freely. Rotation is done by holding down the right mouse button and moving the mouse. Zooming
 * is done by mouse wheel. Selection, if enabled, is done by left mouse button. The scene can be
 * reset to default camera view by clicking mouse wheel. In touch devices rotation is done
 * by tap-and-move, selection by tap-and-hold and zoom by pinch.
 *
 * If no axes are set explicitly to Q3DBars, temporary default axes with no labels are created.
 * These default axes can be modified via axis accessors, but as soon any axis is set explicitly
 * for the orientation, the default axis for that orientation is destroyed.
 *
 * Q3DBars supports more than one series visible at the same time. It is not necessary for all series
 * to have the same amount of rows and columns.
 * Row and column labels are taken from the first added series, unless explicitly defined to
 * row and column axes.
 *
 * \section1 How to construct a minimal Q3DBars graph
 *
 * First, construct an instance of Q3DBars. Since we are running the graph as top level window
 * in this example, we need to clear the \c Qt::FramelessWindowHint flag, which gets set by
 * default:
 *
 * \snippet doc_src_q3dbars_construction.cpp 4
 *
 * After constructing Q3DBars, you can set the data window by changing the range on the row and
 * column axes. It is not mandatory, as data window will default to showing all of the data in
 * the series. If the amount of data is large, it is usually preferable to show just a
 * portion of it. For the example, let's set the data window to show first five rows and columns:
 *
 * \snippet doc_src_q3dbars_construction.cpp 0
 *
 * Now Q3DBars is ready to receive data to be rendered. Create a series with one row of 5 values:
 *
 * \snippet doc_src_q3dbars_construction.cpp 1
 *
 * \note We set the data window to 5 x 5, but we are adding only one row of data. This is ok,
 * the rest of the rows will just be blank.
 *
 * Finally you will need to set it visible:
 *
 * \snippet doc_src_q3dbars_construction.cpp 2
 *
 * The complete code needed to create and display this graph is:
 *
 * \snippet doc_src_q3dbars_construction.cpp 3
 *
 * And this is what those few lines of code produce:
 *
 * \image q3dbars-minimal.png
 *
 * The scene can be rotated, zoomed into, and a bar can be selected to view its value,
 * but no other interaction is included in this minimal code example. You can learn more by
 * familiarizing yourself with the examples provided, like the \l{Bars Example}.
 *
 * \sa Q3DScatter, Q3DSurface, {Qt Data Visualization C++ Classes}
 */

/*!
 * Constructs a new 3D bar graph with optional \a parent window
 * and surface \a format.
 */
Q3DBars::Q3DBars(const QSurfaceFormat *format, QWindow *parent)
    : QAbstract3DGraph(new Q3DBarsPrivate(this), format, parent)
{
    if (!dptr()->m_initialized)
        return;

    dptr()->m_shared = new Bars3DController(geometry());
    d_ptr->setVisualController(dptr()->m_shared);
    dptr()->m_shared->initializeOpenGL();
    QObject::connect(dptr()->m_shared, &Bars3DController::primarySeriesChanged,
                     this, &Q3DBars::primarySeriesChanged);
    QObject::connect(dptr()->m_shared, &Bars3DController::selectedSeriesChanged,
                     this, &Q3DBars::selectedSeriesChanged);
}

/*!
 * Destroys the 3D bar graph.
 */
Q3DBars::~Q3DBars()
{
}

/*!
 * \property Q3DBars::primarySeries
 *
 * Specifies the \a series that is the primary series of the graph. The primary series
 * is used to determine the row and column axis labels when the labels are not explicitly
 * set to the axes.
 * If the specified \a series is not already added to the graph, setting it as the
 * primary series will also implicitly add it to the graph.
 * If the primary series itself is removed from the graph, this property
 * resets to default.
 * If \a series is null, this property resets to default.
 * Defaults to the first added series or zero if no series are added to the graph.
 */
void Q3DBars::setPrimarySeries(QBar3DSeries *series)
{
    dptr()->m_shared->setPrimarySeries(series);
}

QBar3DSeries *Q3DBars::primarySeries() const
{
    return dptrc()->m_shared->primarySeries();
}

/*!
 * Adds the \a series to the graph. A graph can contain multiple series, but only one set of axes,
 * so the rows and columns of all series must match for the visualized data to be meaningful.
 * If the graph has multiple visible series, only the primary series will
 * generate the row or column labels on the axes in cases where the labels are not explicitly set
 * to the axes. If the newly added series has specified a selected bar, it will be highlighted and
 * any existing selection will be cleared. Only one added series can have an active selection.
 *
 * /sa seriesList(), primarySeries
 */
void Q3DBars::addSeries(QBar3DSeries *series)
{
    dptr()->m_shared->addSeries(series);
}

/*!
 * Removes the \a series from the graph.
 */
void Q3DBars::removeSeries(QBar3DSeries *series)
{
    dptr()->m_shared->removeSeries(series);
}

/*!
 * Inserts the \a series into the position \a index in the series list.
 * If the \a series has already been added to the list, it is moved to the
 * new \a index.
 * \note When moving a series to a new \a index that is after its old index,
 * the new position in list is calculated as if the series was still in its old
 * index, so the final index is actually the \a index decremented by one.
 *
 * \sa addSeries(), seriesList()
 */
void Q3DBars::insertSeries(int index, QBar3DSeries *series)
{
    dptr()->m_shared->insertSeries(index, series);
}

/*!
 * \return list of series added to this graph.
 */
QList<QBar3DSeries *> Q3DBars::seriesList() const
{
    return dptrc()->m_shared->barSeriesList();
}

/*!
 * \property Q3DBars::multiSeriesUniform
 *
 * This property controls if bars are to be scaled with proportions set to a single series bar even
 * if there are multiple series displayed. If set to \c {true}, \l{barSpacing}{bar spacing} will
 * affect only X-axis correctly. It is preset to \c false by default.
 */
void Q3DBars::setMultiSeriesUniform(bool uniform)
{
    if (uniform != isMultiSeriesUniform()) {
        dptr()->m_shared->setMultiSeriesScaling(uniform);
        emit multiSeriesUniformChanged(uniform);
    }
}

bool Q3DBars::isMultiSeriesUniform() const
{
    return dptrc()->m_shared->multiSeriesScaling();
}

/*!
 * \property Q3DBars::barThickness
 *
 * Bar thickness ratio between X and Z dimensions. 1.0 means bars are as wide as they are deep, 0.5
 * makes them twice as deep as they are wide. It is preset to \c 1.0 by default.
 */
void Q3DBars::setBarThickness(float thicknessRatio)
{
    if (thicknessRatio != barThickness()) {
        dptr()->m_shared->setBarSpecs(GLfloat(thicknessRatio), barSpacing(),
                                      isBarSpacingRelative());
        emit barThicknessChanged(thicknessRatio);
    }
}

float Q3DBars::barThickness() const
{
    return dptrc()->m_shared->barThickness();
}

/*!
 * \property Q3DBars::barSpacing
 *
 * Bar spacing, which is the empty space between bars, in X and Z dimensions. It is preset to
 * \c {(1.0, 1.0)} by default. Spacing is affected by barSpacingRelative -property.
 *
 * \sa barSpacingRelative, multiSeriesUniform
 */
void Q3DBars::setBarSpacing(const QSizeF &spacing)
{
    if (spacing != barSpacing()) {
        dptr()->m_shared->setBarSpecs(GLfloat(barThickness()), spacing, isBarSpacingRelative());
        emit barSpacingChanged(spacing);
    }
}

QSizeF Q3DBars::barSpacing() const
{
    return dptrc()->m_shared->barSpacing();
}

/*!
 * \property Q3DBars::barSpacingRelative
 *
 * This is used to indicate if spacing is meant to be absolute or relative to bar thickness.
 * If it is true, value of 0.0 means the bars are side-to-side and for example 1.0 means
 * there is one thickness in between the bars. It is preset to \c true.
 */
void Q3DBars::setBarSpacingRelative(bool relative)
{
    if (relative != isBarSpacingRelative()) {
        dptr()->m_shared->setBarSpecs(GLfloat(barThickness()), barSpacing(), relative);
        emit barSpacingRelativeChanged(relative);
    }
}

bool Q3DBars::isBarSpacingRelative() const
{
    return dptrc()->m_shared->isBarSpecRelative();
}

/*!
 * \property Q3DBars::rowAxis
 *
 * The active row \a axis. Implicitly calls addAxis() to transfer ownership of
 * the \a axis to this graph.
 *
 * If the \a axis is null, a temporary default axis with no labels is created.
 * This temporary axis is destroyed if another \a axis is set explicitly to the same orientation.
 *
 * \sa addAxis(), releaseAxis()
 */
void Q3DBars::setRowAxis(QCategory3DAxis *axis)
{
    dptr()->m_shared->setAxisZ(axis);
}

QCategory3DAxis *Q3DBars::rowAxis() const
{
    return static_cast<QCategory3DAxis *>(dptrc()->m_shared->axisZ());
}

/*!
 * \property Q3DBars::columnAxis
 *
 * The active column \a axis. Implicitly calls addAxis() to transfer ownership of
 * the \a axis to this graph.
 *
 * If the \a axis is null, a temporary default axis with no labels is created.
 * This temporary axis is destroyed if another \a axis is set explicitly to the same orientation.
 *
 * \sa addAxis(), releaseAxis()
 */
void Q3DBars::setColumnAxis(QCategory3DAxis *axis)
{
    dptr()->m_shared->setAxisX(axis);
}

QCategory3DAxis *Q3DBars::columnAxis() const
{
    return static_cast<QCategory3DAxis *>(dptrc()->m_shared->axisX());
}

/*!
 * \property Q3DBars::valueAxis
 *
 * The active value \a axis (the Y-axis). Implicitly calls addAxis() to transfer ownership
 * of the \a axis to this graph.
 *
 * If the \a axis is null, a temporary default axis with no labels and automatically adjusting
 * range is created.
 * This temporary axis is destroyed if another \a axis is set explicitly to the same orientation.
 *
 * \sa addAxis(), releaseAxis()
 */
void Q3DBars::setValueAxis(QValue3DAxis *axis)
{
    dptr()->m_shared->setAxisY(axis);
}

QValue3DAxis *Q3DBars::valueAxis() const
{
    return static_cast<QValue3DAxis *>(dptrc()->m_shared->axisY());
}

/*!
 * \property Q3DBars::selectedSeries
 *
 * The selected series or \c null. If selectionMode has \c SelectionMultiSeries flag set, this
 * property holds the series which owns the selected bar.
 */
QBar3DSeries *Q3DBars::selectedSeries() const
{
    return dptrc()->m_shared->selectedSeries();
}

/*!
 * \property Q3DBars::floorLevel
 *
 * The desired floor level for the bar graph in Y-axis data coordinates.
 * The actual floor level cannot go below Y-axis minimum or above Y-axis maximum.
 * Defaults to zero.
 */
void Q3DBars::setFloorLevel(float level)
{
    if (level != floorLevel()) {
        dptr()->m_shared->setFloorLevel(level);
        emit floorLevelChanged(level);
    }
}

float Q3DBars::floorLevel() const
{
    return dptrc()->m_shared->floorLevel();
}

/*!
 * Adds \a axis to the graph. The axes added via addAxis are not yet taken to use,
 * addAxis is simply used to give the ownership of the \a axis to the graph.
 * The \a axis must not be null or added to another graph.
 *
 * \sa releaseAxis(), setValueAxis(), setRowAxis(), setColumnAxis()
 */
void Q3DBars::addAxis(QAbstract3DAxis *axis)
{
    dptr()->m_shared->addAxis(axis);
}

/*!
 * Releases the ownership of the \a axis back to the caller, if it is added to this graph.
 * If the released \a axis is in use, a new default axis will be created and set active.
 *
 * If the default axis is released and added back later, it behaves as any other axis would.
 *
 * \sa addAxis(), setValueAxis(), setRowAxis(), setColumnAxis()
 */
void Q3DBars::releaseAxis(QAbstract3DAxis *axis)
{
    dptr()->m_shared->releaseAxis(axis);
}

/*!
 * \return list of all added axes.
 *
 * \sa addAxis()
 */
QList<QAbstract3DAxis *> Q3DBars::axes() const
{
    return dptrc()->m_shared->axes();
}

Q3DBarsPrivate *Q3DBars::dptr()
{
    return static_cast<Q3DBarsPrivate *>(d_ptr.data());
}

const Q3DBarsPrivate *Q3DBars::dptrc() const
{
    return static_cast<const Q3DBarsPrivate *>(d_ptr.data());
}

Q3DBarsPrivate::Q3DBarsPrivate(Q3DBars *q)
    : QAbstract3DGraphPrivate(q),
      m_shared(0)
{
}

Q3DBarsPrivate::~Q3DBarsPrivate()
{
}

void Q3DBarsPrivate::handleAxisXChanged(QAbstract3DAxis *axis)
{
    emit qptr()->columnAxisChanged(static_cast<QCategory3DAxis *>(axis));
}

void Q3DBarsPrivate::handleAxisYChanged(QAbstract3DAxis *axis)
{
    emit qptr()->valueAxisChanged(static_cast<QValue3DAxis *>(axis));
}

void Q3DBarsPrivate::handleAxisZChanged(QAbstract3DAxis *axis)
{
    emit qptr()->rowAxisChanged(static_cast<QCategory3DAxis *>(axis));
}

Q3DBars *Q3DBarsPrivate::qptr()
{
    return static_cast<Q3DBars *>(q_ptr);
}

QT_END_NAMESPACE_DATAVISUALIZATION
