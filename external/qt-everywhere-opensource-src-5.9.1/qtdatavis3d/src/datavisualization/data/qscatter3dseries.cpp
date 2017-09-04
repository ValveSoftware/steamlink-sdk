/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "qscatter3dseries_p.h"
#include "scatter3dcontroller_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QScatter3DSeries
 * \inmodule QtDataVisualization
 * \brief The QScatter3DSeries class represents a data series in a 3D scatter
 * graph.
 * \since QtDataVisualization 1.0
 *
 * This class manages the series specific visual elements, as well as the series
 * data (via a data proxy).
 *
 * If no data proxy is set explicitly for the series, the series creates a default
 * proxy. Setting another proxy will destroy the existing proxy and all data added to it.
 *
 * QScatter3DSeries supports the following format tags for QAbstract3DSeries::setItemLabelFormat():
 * \table
 *   \row
 *     \li @xTitle    \li Title from x-axis
 *   \row
 *     \li @yTitle    \li Title from y-axis
 *   \row
 *     \li @zTitle    \li Title from z-axis
 *   \row
 *     \li @xLabel    \li Item value formatted using the format of the x-axis.
 *                        For more information, see
 *                        \l{QValue3DAxis::setLabelFormat()}.
 *   \row
 *     \li @yLabel    \li Item value formatted using the format of the y-axis.
 *                        For more information, see
 *                        \l{QValue3DAxis::setLabelFormat()}.
 *   \row
 *     \li @zLabel    \li Item value formatted using the format of the z-axis.
 *                        For more information, see
 *                        \l{QValue3DAxis::setLabelFormat()}.
 *   \row
 *     \li @seriesName \li Name of the series
 * \endtable
 *
 * For example:
 * \snippet doc_src_qtdatavisualization.cpp 1
 *
 * \sa {Qt Data Visualization Data Handling}
 */

/*!
 * \qmltype Scatter3DSeries
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates QScatter3DSeries
 * \inherits Abstract3DSeries
 * \brief Represents a data series in a 3D scatter graph.
 *
 * This type manages the series specific visual elements, as well as the series
 * data (via a data proxy).
 *
 * For a more complete description, see QScatter3DSeries.
 *
 * \sa {Qt Data Visualization Data Handling}
 */

/*!
 * \qmlproperty ScatterDataProxy Scatter3DSeries::dataProxy
 *
 * Sets the active data proxy. The series assumes ownership of any proxy set to
 * it and deletes any previously set proxy when a new one is added. The proxy
 * cannot be null or set to another series.
 */

/*!
 * \qmlproperty int Scatter3DSeries::selectedItem
 *
 * The item that is selected at the index in the data array of the series.
 * Only one item can be selected at a time.
 * To clear selection from this series, invalidSelectionIndex is set as the index.
 * If this series is added to a graph, the graph can adjust the selection according to user
 * interaction or if it becomes invalid. Selecting an item on another added series will also
 * clear the selection.
 * Removing items from or inserting items to the series before the selected item
 * will adjust the selection so that the same item will stay selected.
 *
 * \sa AbstractGraph3D::clearSelection()
 */

/*!
 * \qmlproperty float Scatter3DSeries::itemSize
 *
 * Sets the item size for the series. The size must be between \c 0.0 and
 * \c 1.0. Setting the size to \c 0.0 causes the item size to be automatically
 * scaled based on the total number of items in all the series for the graph.
 * The preset default is \c 0.0.
 */

/*!
 * \qmlproperty int Scatter3DSeries::invalidSelectionIndex
 * A constant property providing an invalid index for selection. This index is
 * set to the selectedItem property to clear the selection from this series.
 *
 * \sa AbstractGraph3D::clearSelection()
 */

/*!
 * Constructs a scatter 3D series with the parent \a parent.
 */
QScatter3DSeries::QScatter3DSeries(QObject *parent) :
    QAbstract3DSeries(new QScatter3DSeriesPrivate(this), parent)
{
    // Default proxy
    dptr()->setDataProxy(new QScatterDataProxy);
}

/*!
 * Constructs a scatter 3D series with the data proxy \a dataProxy and the
 * parent \a parent.
 */
QScatter3DSeries::QScatter3DSeries(QScatterDataProxy *dataProxy, QObject *parent) :
    QAbstract3DSeries(new QScatter3DSeriesPrivate(this), parent)
{
    dptr()->setDataProxy(dataProxy);
}

/*!
 * \internal
 */
QScatter3DSeries::QScatter3DSeries(QScatter3DSeriesPrivate *d, QObject *parent) :
    QAbstract3DSeries(d, parent)
{
}

/*!
 * Deletes the scatter 3D series.
 */
QScatter3DSeries::~QScatter3DSeries()
{
}

/*!
 * \property QScatter3DSeries::dataProxy
 *
 * \brief The active data proxy.
 */

/*!
 * Sets the active data proxy for the series to \a proxy. The series assumes
 * ownership of any proxy set to it and deletes any previously set proxy when
 * a new one is added. The \a proxy argument cannot be null or set to another
 * series.
 */
void QScatter3DSeries::setDataProxy(QScatterDataProxy *proxy)
{
    d_ptr->setDataProxy(proxy);
}

QScatterDataProxy *QScatter3DSeries::dataProxy() const
{
    return static_cast<QScatterDataProxy *>(d_ptr->dataProxy());
}

/*!
 * \property QScatter3DSeries::selectedItem
 *
 * \brief The item that is selected in the series.
 */

/*!
 * Selects the item at the index \a index in the data array of the series.
 * Only one item can be selected at a time.
 *
 * To clear selection from this series, invalidSelectionIndex() is set as \a index.
 * If this series is added to a graph, the graph can adjust the selection according to user
 * interaction or if it becomes invalid. Selecting an item on another added series will also
 * clear the selection.
 *
 * Removing items from or inserting items to the series before the selected item
 * will adjust the selection so that the same item will stay selected.
 *
 * \sa QAbstract3DGraph::clearSelection()
 */
void QScatter3DSeries::setSelectedItem(int index)
{
    // Don't do this in private to avoid loops, as that is used for callback from controller.
    if (d_ptr->m_controller)
        static_cast<Scatter3DController *>(d_ptr->m_controller)->setSelectedItem(index, this);
    else
        dptr()->setSelectedItem(index);
}

int QScatter3DSeries::selectedItem() const
{
    return dptrc()->m_selectedItem;
}

/*!
 * \property QScatter3DSeries::itemSize
 *
 * \brief Item size for the series.
 *
 * The size must be between \c 0.0f and \c 1.0f. Setting the size to \c 0.0f
 * causes the item size to be automatically scaled based on the total number of
 * items in all the series for the graph.
 *
 * The preset default is \c 0.0f.
 */
void QScatter3DSeries::setItemSize(float size)
{
    if (size < 0.0f || size > 1.0f) {
        qWarning("Invalid size. Valid range for itemSize is 0.0f...1.0f");
    } else if (size != dptr()->m_itemSize) {
        dptr()->setItemSize(size);
        emit itemSizeChanged(size);
    }
}

float QScatter3DSeries::itemSize() const
{
    return dptrc()->m_itemSize;
}

/*!
 * Returns an invalid index for selection. This index is set to the selectedItem
 * property to clear the selection from this series.
 *
 * \sa QAbstract3DGraph::clearSelection()
 */
int QScatter3DSeries::invalidSelectionIndex()
{
    return Scatter3DController::invalidSelectionIndex();
}

/*!
 * \internal
 */
QScatter3DSeriesPrivate *QScatter3DSeries::dptr()
{
    return static_cast<QScatter3DSeriesPrivate *>(d_ptr.data());
}

/*!
 * \internal
 */
const QScatter3DSeriesPrivate *QScatter3DSeries::dptrc() const
{
    return static_cast<const QScatter3DSeriesPrivate *>(d_ptr.data());
}

// QScatter3DSeriesPrivate

QScatter3DSeriesPrivate::QScatter3DSeriesPrivate(QScatter3DSeries *q)
    : QAbstract3DSeriesPrivate(q, QAbstract3DSeries::SeriesTypeScatter),
      m_selectedItem(Scatter3DController::invalidSelectionIndex()),
      m_itemSize(0.0f)
{
    m_itemLabelFormat = QStringLiteral("@xLabel, @yLabel, @zLabel");
    m_mesh = QAbstract3DSeries::MeshSphere;
}

QScatter3DSeriesPrivate::~QScatter3DSeriesPrivate()
{
}

QScatter3DSeries *QScatter3DSeriesPrivate::qptr()
{
    return static_cast<QScatter3DSeries *>(q_ptr);
}

void QScatter3DSeriesPrivate::setDataProxy(QAbstractDataProxy *proxy)
{
    Q_ASSERT(proxy->type() == QAbstractDataProxy::DataTypeScatter);

    QAbstract3DSeriesPrivate::setDataProxy(proxy);

    emit qptr()->dataProxyChanged(static_cast<QScatterDataProxy *>(proxy));
}

void QScatter3DSeriesPrivate::connectControllerAndProxy(Abstract3DController *newController)
{
    QScatterDataProxy *scatterDataProxy = static_cast<QScatterDataProxy *>(m_dataProxy);

    if (m_controller && scatterDataProxy) {
        //Disconnect old controller/old proxy
        QObject::disconnect(scatterDataProxy, 0, m_controller, 0);
        QObject::disconnect(q_ptr, 0, m_controller, 0);
    }

    if (newController && scatterDataProxy) {
        Scatter3DController *controller = static_cast<Scatter3DController *>(newController);
        QObject::connect(scatterDataProxy, &QScatterDataProxy::arrayReset,
                         controller, &Scatter3DController::handleArrayReset);
        QObject::connect(scatterDataProxy, &QScatterDataProxy::itemsAdded,
                         controller, &Scatter3DController::handleItemsAdded);
        QObject::connect(scatterDataProxy, &QScatterDataProxy::itemsChanged,
                         controller, &Scatter3DController::handleItemsChanged);
        QObject::connect(scatterDataProxy, &QScatterDataProxy::itemsRemoved,
                         controller, &Scatter3DController::handleItemsRemoved);
        QObject::connect(scatterDataProxy, &QScatterDataProxy::itemsInserted,
                         controller, &Scatter3DController::handleItemsInserted);
        QObject::connect(qptr(), &QScatter3DSeries::dataProxyChanged,
                         controller, &Scatter3DController::handleArrayReset);
    }
}

void QScatter3DSeriesPrivate::createItemLabel()
{
    static const QString xTitleTag(QStringLiteral("@xTitle"));
    static const QString yTitleTag(QStringLiteral("@yTitle"));
    static const QString zTitleTag(QStringLiteral("@zTitle"));
    static const QString xLabelTag(QStringLiteral("@xLabel"));
    static const QString yLabelTag(QStringLiteral("@yLabel"));
    static const QString zLabelTag(QStringLiteral("@zLabel"));
    static const QString seriesNameTag(QStringLiteral("@seriesName"));

    if (m_selectedItem == QScatter3DSeries::invalidSelectionIndex()) {
        m_itemLabel = QString();
        return;
    }

    QValue3DAxis *axisX = static_cast<QValue3DAxis *>(m_controller->axisX());
    QValue3DAxis *axisY = static_cast<QValue3DAxis *>(m_controller->axisY());
    QValue3DAxis *axisZ = static_cast<QValue3DAxis *>(m_controller->axisZ());
    QVector3D selectedPosition = qptr()->dataProxy()->itemAt(m_selectedItem)->position();

    m_itemLabel = m_itemLabelFormat;

    m_itemLabel.replace(xTitleTag, axisX->title());
    m_itemLabel.replace(yTitleTag, axisY->title());
    m_itemLabel.replace(zTitleTag, axisZ->title());

    if (m_itemLabel.contains(xLabelTag)) {
        QString valueLabelText = axisX->formatter()->stringForValue(
                    qreal(selectedPosition.x()), axisX->labelFormat());
        m_itemLabel.replace(xLabelTag, valueLabelText);
    }
    if (m_itemLabel.contains(yLabelTag)) {
        QString valueLabelText = axisY->formatter()->stringForValue(
                    qreal(selectedPosition.y()), axisY->labelFormat());
        m_itemLabel.replace(yLabelTag, valueLabelText);
    }
    if (m_itemLabel.contains(zLabelTag)) {
        QString valueLabelText = axisZ->formatter()->stringForValue(
                    qreal(selectedPosition.z()), axisZ->labelFormat());
        m_itemLabel.replace(zLabelTag, valueLabelText);
    }
    m_itemLabel.replace(seriesNameTag, m_name);
}

void QScatter3DSeriesPrivate::setSelectedItem(int index)
{
    if (index != m_selectedItem) {
        markItemLabelDirty();
        m_selectedItem = index;
        emit qptr()->selectedItemChanged(m_selectedItem);
    }
}

void QScatter3DSeriesPrivate::setItemSize(float size)
{
    m_itemSize = size;
    if (m_controller)
        m_controller->markSeriesVisualsDirty();
}

QT_END_NAMESPACE_DATAVISUALIZATION
