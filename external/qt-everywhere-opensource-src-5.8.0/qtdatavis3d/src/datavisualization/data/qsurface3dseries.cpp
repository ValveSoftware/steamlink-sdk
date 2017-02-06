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

#include "qsurface3dseries_p.h"
#include "surface3dcontroller_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QSurface3DSeries
 * \inmodule QtDataVisualization
 * \brief Base series class for Q3DSurface.
 * \since QtDataVisualization 1.0
 *
 * QSurface3DSeries manages the series specific visual elements, as well as series data
 * (via data proxy).
 *
 * If no data proxy is set explicitly for the series, the series creates a default
 * proxy. Setting another proxy will destroy the existing proxy and all data added to it.
 *
 * The object mesh set via QAbstract3DSeries::mesh property defines the selection
 * pointer shape in surface series.
 *
 * QSurface3DSeries supports the following format tags for QAbstract3DSeries::setItemLabelFormat():
 * \table
 *   \row
 *     \li @xTitle    \li Title from X axis
 *   \row
 *     \li @yTitle    \li Title from Y axis
 *   \row
 *     \li @zTitle    \li Title from Z axis
 *   \row
 *     \li @xLabel    \li Item value formatted using the same format as the X axis attached to the graph uses,
 *                            see \l{QValue3DAxis::setLabelFormat()} for more information.
 *   \row
 *     \li @yLabel    \li Item value formatted using the same format as the Y axis attached to the graph uses,
 *                            see \l{QValue3DAxis::setLabelFormat()} for more information.
 *   \row
 *     \li @zLabel    \li Item value formatted using the same format as the Z axis attached to the graph uses,
 *                            see \l{QValue3DAxis::setLabelFormat()} for more information.
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
 * \qmltype Surface3DSeries
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates QSurface3DSeries
 * \inherits Abstract3DSeries
 * \brief Base series type for Surfaces3D.
 *
 * This type  manages the series specific visual elements, as well as series data
 * (via data proxy).
 *
 * For Surface3DSeries enums, see \l{QSurface3DSeries::DrawFlag}.
 *
 * For more complete description, see QSurface3DSeries.
 *
 * \sa {Qt Data Visualization Data Handling}
 */

/*!
 * \qmlproperty SurfaceDataProxy Surface3DSeries::dataProxy
 *
 * This property holds the active data \a proxy. The series assumes ownership of any proxy set to
 * it and deletes any previously set proxy when a new one is added. The \a proxy cannot be null or
 * set to another series.
 */

/*!
 * \qmlproperty point Surface3DSeries::selectedPoint
 *
 * Selects a surface grid point in a \a position. The position is the (row, column) position in
 * the data array of the series.
 * Only one point can be selected at a time.
 * To clear selection from this series, set invalidSelectionPosition as the \a position.
 * If this series is added to a graph, the graph can adjust the selection according to user
 * interaction or if it becomes invalid.
 * Removing rows from or inserting rows to the series before the row of the selected point
 * will adjust the selection so that the same point will stay selected.
 *
 * \sa AbstractGraph3D::clearSelection()
 */

/*!
 * \qmlproperty point Surface3DSeries::invalidSelectionPosition
 * A constant property providing an invalid selection position.
 * Set this to selectedPoint property to clear the selection from this series.
 *
 * \sa AbstractGraph3D::clearSelection()
 */

/*!
 * \qmlproperty bool Surface3DSeries::flatShadingEnabled
 *
 * Sets surface flat shading to \a enabled. It is preset to \c true by default.
 * When disabled, the normals on the surface are interpolated making edges looking round.
 * When enabled, the normals are kept same on a triangle making the color of the triangle solid.
 * This makes the data more readable from the model.
 * \note Flat shaded surfaces require at least GLSL version 1.2 with GL_EXT_gpu_shader4 extension.
 * The value of flatShadingSupported property indicates if flat shading is supported at runtime.
 */

/*!
 * \qmlproperty bool Surface3DSeries::flatShadingSupported
 *
 * Flat shading for surfaces requires at least GLSL version 1.2 with GL_EXT_gpu_shader4 extension.
 * If true, flat shading for surfaces is supported by current system.
 * \note This read-only property is set to its correct value after first render pass.
 * Before then it is always true.
 */

/*!
 * \qmlproperty DrawFlag Surface3DSeries::drawMode
 *
 * Sets the drawing \a mode to one of \l{QSurface3DSeries::DrawFlag}{Surface3DSeries.DrawFlag}.
 * Clearing all flags is not allowed.
 */

/*!
 * \qmlproperty string Surface3DSeries::textureFile
 *
 * Holds the texture file name for the surface texture. To clear the texture, set empty
 * file name.
 */


/*!
 * \enum QSurface3DSeries::DrawFlag
 *
 * Drawing mode of the surface. Values of this enumeration can be combined with OR operator.
 *
 * \value DrawWireframe
 *        Only the grid is drawn.
 * \value DrawSurface
 *        Only the surface is drawn.
 * \value DrawSurfaceAndWireframe
 *        Both the surface and grid are drawn.
 */

/*!
 * Constructs QSurface3DSeries with the given \a parent.
 */
QSurface3DSeries::QSurface3DSeries(QObject *parent) :
    QAbstract3DSeries(new QSurface3DSeriesPrivate(this), parent)
{
    // Default proxy
    dptr()->setDataProxy(new QSurfaceDataProxy);
}

/*!
 * Constructs QSurface3DSeries with the given \a dataProxy and the \a parent.
 */
QSurface3DSeries::QSurface3DSeries(QSurfaceDataProxy *dataProxy, QObject *parent) :
    QAbstract3DSeries(new QSurface3DSeriesPrivate(this), parent)
{
    dptr()->setDataProxy(dataProxy);
}

/*!
 * \internal
 */
QSurface3DSeries::QSurface3DSeries(QSurface3DSeriesPrivate *d, QObject *parent) :
    QAbstract3DSeries(d, parent)
{
}

/*!
 * Destroys QSurface3DSeries.
 */
QSurface3DSeries::~QSurface3DSeries()
{
}

/*!
 * \property QSurface3DSeries::dataProxy
 *
 * This property holds the active data \a proxy. The series assumes ownership of any proxy set to
 * it and deletes any previously set proxy when a new one is added. The \a proxy cannot be null or
 * set to another series.
 */
void QSurface3DSeries::setDataProxy(QSurfaceDataProxy *proxy)
{
    d_ptr->setDataProxy(proxy);
}

QSurfaceDataProxy *QSurface3DSeries::dataProxy() const
{
    return static_cast<QSurfaceDataProxy *>(d_ptr->dataProxy());
}

/*!
 * \property QSurface3DSeries::selectedPoint
 *
 * Selects a surface grid point in a \a position. The position is the (row, column) position in
 * the data array of the series.
 * Only one point can be selected at a time.
 * To clear selection from this series, set invalidSelectionPosition() as the \a position.
 * If this series is added to a graph, the graph can adjust the selection according to user
 * interaction or if it becomes invalid.
 * Removing rows from or inserting rows to the series before the row of the selected point
 * will adjust the selection so that the same point will stay selected.
 *
 * \sa QAbstract3DGraph::clearSelection()
 */
void QSurface3DSeries::setSelectedPoint(const QPoint &position)
{
    // Don't do this in private to avoid loops, as that is used for callback from controller.
    if (d_ptr->m_controller)
        static_cast<Surface3DController *>(d_ptr->m_controller)->setSelectedPoint(position, this, true);
    else
        dptr()->setSelectedPoint(position);
}

QPoint QSurface3DSeries::selectedPoint() const
{
    return dptrc()->m_selectedPoint;
}

/*!
 * \return a QPoint signifying an invalid selection position. Set this to selectedPoint property
 * to clear the selection from this series.
 *
 * \sa QAbstract3DGraph::clearSelection()
 */
QPoint QSurface3DSeries::invalidSelectionPosition()
{
    return Surface3DController::invalidSelectionPosition();
}

/*!
 * \property QSurface3DSeries::flatShadingEnabled
 *
 * Sets surface flat shading to \a enabled. It is preset to \c true by default.
 * When disabled, the normals on the surface are interpolated making edges looking round.
 * When enabled, the normals are kept same on a triangle making the color of the triangle solid.
 * This makes the data more readable from the model.
 * \note Flat shaded surfaces require at least GLSL version 1.2 with GL_EXT_gpu_shader4 extension.
 * The value of flatShadingSupported property indicates if flat shading is supported at runtime.
 */
void QSurface3DSeries::setFlatShadingEnabled(bool enabled)
{
    if (dptr()->m_flatShadingEnabled != enabled) {
        dptr()->setFlatShadingEnabled(enabled);
        emit flatShadingEnabledChanged(enabled);
    }
}

bool QSurface3DSeries::isFlatShadingEnabled() const
{
    return dptrc()->m_flatShadingEnabled;
}

/*!
 * \property QSurface3DSeries::flatShadingSupported
 * Flat shading for surfaces requires at least GLSL version 1.2 with GL_EXT_gpu_shader4 extension.
 * If true, flat shading for surfaces is supported by current system.
 * \note This read-only property is set to its correct value after first render pass.
 * Before then it is always true.
 */
bool QSurface3DSeries::isFlatShadingSupported() const
{
    if (d_ptr->m_controller)
        return static_cast<Surface3DController *>(d_ptr->m_controller)->isFlatShadingSupported();
    else
        return true;
}

/*!
 * \property QSurface3DSeries::drawMode
 *
 * Sets the drawing \a mode to one of DrawFlag. Clearing all flags is not allowed.
 */
void QSurface3DSeries::setDrawMode(DrawFlags mode)
{
    if (dptr()->m_drawMode != mode) {
        dptr()->setDrawMode(mode);
        emit drawModeChanged(mode);
    }
}

QSurface3DSeries::DrawFlags QSurface3DSeries::drawMode() const
{
    return dptrc()->m_drawMode;
}

/*!
 * \property QSurface3DSeries::texture
 *
 * Set the \a texture as a QImage for the surface. To clear the texture, set empty
 * QImage as texture.
 */
void QSurface3DSeries::setTexture(const QImage &texture)
{
    if (dptr()->m_texture != texture) {
        dptr()->setTexture(texture);

        emit textureChanged(texture);
        dptr()->m_textureFile.clear();
    }
}

QImage QSurface3DSeries::texture() const
{
    return dptrc()->m_texture;
}

/*!
 * \property QSurface3DSeries::textureFile
 *
 * Holds the texture file name for the surface texture. To clear the texture, set empty
 * file name.
 */
void QSurface3DSeries::setTextureFile(const QString &filename)
{
    if (dptr()->m_textureFile != filename) {
        if (filename.isEmpty()) {
            setTexture(QImage());
        } else {
            QImage image(filename);
            if (image.isNull()) {
                qWarning() << "Warning: Tried to set invalid image file as surface texture.";
                return;
            }
            setTexture(image);
        }

        dptr()->m_textureFile = filename;
        emit textureFileChanged(filename);
    }
}

QString QSurface3DSeries::textureFile() const
{
    return dptrc()->m_textureFile;
}

/*!
 * \internal
 */
QSurface3DSeriesPrivate *QSurface3DSeries::dptr()
{
    return static_cast<QSurface3DSeriesPrivate *>(d_ptr.data());
}

/*!
 * \internal
 */
const QSurface3DSeriesPrivate *QSurface3DSeries::dptrc() const
{
    return static_cast<const QSurface3DSeriesPrivate *>(d_ptr.data());
}

// QSurface3DSeriesPrivate

QSurface3DSeriesPrivate::QSurface3DSeriesPrivate(QSurface3DSeries *q)
    : QAbstract3DSeriesPrivate(q, QAbstract3DSeries::SeriesTypeSurface),
      m_selectedPoint(Surface3DController::invalidSelectionPosition()),
      m_flatShadingEnabled(true),
      m_drawMode(QSurface3DSeries::DrawSurfaceAndWireframe)
{
    m_itemLabelFormat = QStringLiteral("@xLabel, @yLabel, @zLabel");
    m_mesh = QAbstract3DSeries::MeshSphere;
}

QSurface3DSeriesPrivate::~QSurface3DSeriesPrivate()
{
}

QSurface3DSeries *QSurface3DSeriesPrivate::qptr()
{
    return static_cast<QSurface3DSeries *>(q_ptr);
}

void QSurface3DSeriesPrivate::setDataProxy(QAbstractDataProxy *proxy)
{
    Q_ASSERT(proxy->type() == QAbstractDataProxy::DataTypeSurface);

    QAbstract3DSeriesPrivate::setDataProxy(proxy);

    emit qptr()->dataProxyChanged(static_cast<QSurfaceDataProxy *>(proxy));
}

void QSurface3DSeriesPrivate::connectControllerAndProxy(Abstract3DController *newController)
{
    QSurfaceDataProxy *surfaceDataProxy = static_cast<QSurfaceDataProxy *>(m_dataProxy);

    if (m_controller && surfaceDataProxy) {
        //Disconnect old controller/old proxy
        QObject::disconnect(surfaceDataProxy, 0, m_controller, 0);
        QObject::disconnect(q_ptr, 0, m_controller, 0);
    }

    if (newController && surfaceDataProxy) {
        Surface3DController *controller = static_cast<Surface3DController *>(newController);

        QObject::connect(surfaceDataProxy, &QSurfaceDataProxy::arrayReset, controller,
                         &Surface3DController::handleArrayReset);
        QObject::connect(surfaceDataProxy, &QSurfaceDataProxy::rowsAdded, controller,
                         &Surface3DController::handleRowsAdded);
        QObject::connect(surfaceDataProxy, &QSurfaceDataProxy::rowsChanged, controller,
                         &Surface3DController::handleRowsChanged);
        QObject::connect(surfaceDataProxy, &QSurfaceDataProxy::rowsRemoved, controller,
                         &Surface3DController::handleRowsRemoved);
        QObject::connect(surfaceDataProxy, &QSurfaceDataProxy::rowsInserted, controller,
                         &Surface3DController::handleRowsInserted);
        QObject::connect(surfaceDataProxy, &QSurfaceDataProxy::itemChanged, controller,
                         &Surface3DController::handleItemChanged);
    }
}

void QSurface3DSeriesPrivate::createItemLabel()
{
    static const QString xTitleTag(QStringLiteral("@xTitle"));
    static const QString yTitleTag(QStringLiteral("@yTitle"));
    static const QString zTitleTag(QStringLiteral("@zTitle"));
    static const QString xLabelTag(QStringLiteral("@xLabel"));
    static const QString yLabelTag(QStringLiteral("@yLabel"));
    static const QString zLabelTag(QStringLiteral("@zLabel"));
    static const QString seriesNameTag(QStringLiteral("@seriesName"));

    if (m_selectedPoint == QSurface3DSeries::invalidSelectionPosition()) {
        m_itemLabel = QString();
        return;
    }

    QValue3DAxis *axisX = static_cast<QValue3DAxis *>(m_controller->axisX());
    QValue3DAxis *axisY = static_cast<QValue3DAxis *>(m_controller->axisY());
    QValue3DAxis *axisZ = static_cast<QValue3DAxis *>(m_controller->axisZ());
    QVector3D selectedPosition = qptr()->dataProxy()->itemAt(m_selectedPoint)->position();

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

void QSurface3DSeriesPrivate::setSelectedPoint(const QPoint &position)
{
    if (position != m_selectedPoint) {
        markItemLabelDirty();
        m_selectedPoint = position;
        emit qptr()->selectedPointChanged(m_selectedPoint);
    }
}

void QSurface3DSeriesPrivate::setFlatShadingEnabled(bool enabled)
{
    m_flatShadingEnabled = enabled;
    if (m_controller)
        m_controller->markSeriesVisualsDirty();
}

void QSurface3DSeriesPrivate::setDrawMode(QSurface3DSeries::DrawFlags mode)
{
    if (mode.testFlag(QSurface3DSeries::DrawWireframe)
            || mode.testFlag(QSurface3DSeries::DrawSurface)) {
        m_drawMode = mode;
        if (m_controller)
            m_controller->markSeriesVisualsDirty();
    } else {
        qWarning("You may not clear all draw flags. Mode not changed.");
    }
}

void QSurface3DSeriesPrivate::setTexture(const QImage &texture)
{
    m_texture = texture;
    if (static_cast<Surface3DController *>(m_controller))
        static_cast<Surface3DController *>(m_controller)->updateSurfaceTexture(qptr());
}

QT_END_NAMESPACE_DATAVISUALIZATION
