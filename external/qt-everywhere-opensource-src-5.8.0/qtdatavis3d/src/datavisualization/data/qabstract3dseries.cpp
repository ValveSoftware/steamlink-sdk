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

#include "qabstract3dseries_p.h"
#include "qabstractdataproxy_p.h"
#include "abstract3dcontroller_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QAbstract3DSeries
 * \inmodule QtDataVisualization
 * \brief Base class for all QtDataVisualization series.
 * \since QtDataVisualization 1.0
 *
 * You use the visualization type specific inherited classes instead of the base class.
 * \sa QBar3DSeries, QScatter3DSeries, QSurface3DSeries, {Qt Data Visualization Data Handling}
 */

/*!
 * \class QAbstract3DSeriesChangeBitField
 * \internal
 */

/*!
 * \class QAbstract3DSeriesThemeOverrideBitField
 * \internal
 */

/*!
 * \qmltype Abstract3DSeries
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates QAbstract3DSeries
 * \brief Base type for all QtDataVisualization series.
 *
 * This type is uncreatable, but contains properties that are exposed via subtypes.
 *
 * For Abstract3DSeries enums, see \l QAbstract3DSeries::SeriesType and \l{QAbstract3DSeries::Mesh}.
 *
 * \sa Bar3DSeries, Scatter3DSeries, Surface3DSeries, {Qt Data Visualization Data Handling}
 */

/*!
 * \enum QAbstract3DSeries::SeriesType
 *
 * Type of the series.
 *
 * \value SeriesTypeNone
 *        No series type.
 * \value SeriesTypeBar
 *        Series type for Q3DBars.
 * \value SeriesTypeScatter
 *        Series type for Q3DScatter.
 * \value SeriesTypeSurface
 *        Series type for Q3DSurface.
 */

/*!
 *  \enum QAbstract3DSeries::Mesh
 *
 *  Predefined mesh types. All styles are not usable with all visualization types.
 *
 *  \value MeshUserDefined
 *         User defined mesh, set via QAbstract3DSeries::userDefinedMesh property.
 *  \value MeshBar
 *         Basic rectangular bar.
 *  \value MeshCube
 *         Basic cube.
 *  \value MeshPyramid
 *         Four-sided pyramid.
 *  \value MeshCone
 *         Basic cone.
 *  \value MeshCylinder
 *         Basic cylinder.
 *  \value MeshBevelBar
 *         Slightly beveled (rounded) rectangular bar.
 *  \value MeshBevelCube
 *         Slightly beveled (rounded) cube.
 *  \value MeshSphere
 *         Sphere.
 *  \value MeshMinimal
 *         The minimal 3D mesh: a triangular pyramid. Usable only with Q3DScatter.
 *  \value MeshArrow
 *         Arrow pointing upwards.
 *  \value MeshPoint
 *         2D point. Usable only with Q3DScatter.
 *         \b Note: Shadows do not affect this style. Color style Q3DTheme::ColorStyleObjectGradient
 *         is not supported by this style.
 */

/*!
 * \qmlproperty Abstract3DSeries.SeriesType Abstract3DSeries::type
 * The type of the series.
 */

/*!
 * \qmlproperty string Abstract3DSeries::itemLabelFormat
 *
 * Label format for data items in this series. This format is used for single item labels,
 * for example, when an item is selected. How the format is interpreted depends on series type. See
 * each series class documentation for more information.
 */

/*!
 * \qmlproperty bool Abstract3DSeries::visible
 * Sets the visibility of the series. If false, the series is not rendered.
 */

/*!
 * \qmlproperty Abstract3DSeries.Mesh Abstract3DSeries::mesh
 *
 * Sets the \a mesh of the items in the series, or the selection pointer in case of
 * Surface3DSeries. If the \a mesh is \l{QAbstract3DSeries::MeshUserDefined}{Abstract3DSeries.MeshUserDefined},
 * then the userDefinedMesh property must also be set for items to render properly.
 * The default value depends on the graph type.
 */

/*!
 * \qmlproperty bool Abstract3DSeries::meshSmooth
 *
 * If \a enable is \c true, smooth versions of predefined meshes set via mesh property are used.
 * This property doesn't affect custom meshes used when mesh is
 * \l{QAbstract3DSeries::MeshUserDefined}{Abstract3DSeries.MeshUserDefined}.
 * Defaults to \c{false}.
 */

/*!
 * \qmlproperty quaternion Abstract3DSeries::meshRotation
 *
 * Sets the mesh \a rotation that is applied to all items of the series.
 * The \a rotation should be a normalized quaternion.
 * For those series types that support item specific rotation, the rotations are
 * multiplied together.
 * Bar3DSeries ignores any rotation that is not around Y-axis.
 * Surface3DSeries applies the rotation only to the selection pointer.
 * Defaults to no rotation.
 */

/*!
 * \qmlproperty string Abstract3DSeries::userDefinedMesh
 *
 * Sets the \a fileName for user defined custom mesh for objects that is used when mesh
 * is \l{QAbstract3DSeries::MeshUserDefined}{Abstract3DSeries.MeshUserDefined}.
 * \note The file specified by \a fileName needs to be in Wavefront obj format and include
 * vertices, normals and UVs. It also needs to be in triangles.
 */

/*!
 * \qmlproperty Theme3D.ColorStyle Abstract3DSeries::colorStyle
 *
 * Sets the color \a style for the series.
 * See \l{Theme3D::colorStyle}{Theme3D.colorStyle}
 * documentation for more information.
 */

/*!
 * \qmlproperty Color Abstract3DSeries::baseColor
 *
 * Sets the base \a color of the series.
 * See \l{Theme3D::baseColors}{Theme3D.baseColors}
 * documentation for more information.
 *
 * \sa colorStyle
 */

/*!
 * \qmlproperty ColorGradient Abstract3DSeries::baseGradient
 *
 * Sets the base \a gradient of the series.
 * See \l{Theme3D::baseGradients}{Theme3D.baseGradients}
 * documentation for more information.
 *
 * \sa colorStyle
 */

/*!
 * \qmlproperty Color Abstract3DSeries::singleHighlightColor
 *
 * Sets the single item highlight \a color of the series.
 * See \l{Theme3D::singleHighlightColor}{Theme3D.singleHighlightColor}
 * documentation for more information.
 *
 * \sa colorStyle
 */

/*!
 * \qmlproperty ColorGradient Abstract3DSeries::singleHighlightGradient
 *
 * Sets the single item highlight \a gradient of the series.
 * See \l{Theme3D::singleHighlightGradient}{Theme3D.singleHighlightGradient}
 * documentation for more information.
 *
 * \sa colorStyle
 */

/*!
 * \qmlproperty Color Abstract3DSeries::multiHighlightColor
 *
 * Sets the multiple item highlight \a color of the series.
 * See \l{Theme3D::multiHighlightColor}{Theme3D.multiHighlightColor}
 * documentation for more information.
 *
 * \sa colorStyle
 */

/*!
 * \qmlproperty ColorGradient Abstract3DSeries::multiHighlightGradient
 *
 * Sets the multiple item highlight \a gradient of the series.
 * See \l{Theme3D::multiHighlightGradient}{Theme3D.multiHighlightGradient}
 * documentation for more information.
 *
 * \sa colorStyle
 */

/*!
 * \qmlproperty string Abstract3DSeries::name
 *
 * Sets the series name.
 * Series name can be used in item label format with tag \c{@seriesName}.
 *
 * \sa itemLabelFormat
 */

/*!
 * \qmlproperty string Abstract3DSeries::itemLabel
 * \since QtDataVisualization 1.1
 *
 * Contains the formatted item label. If there is no selected item or the selected item is not
 * visible, returns an empty string.
 *
 * \sa itemLabelFormat
 */

/*!
 * \qmlproperty bool Abstract3DSeries::itemLabelVisible
 * \since QtDataVisualization 1.1
 *
 * If \c true, item labels are drawn as floating labels in the graph. Otherwise item labels are not
 * drawn. If you prefer to show the item label in an external control, set this property to
 * \c false. Defaults to \c true.
 *
 * \sa itemLabelFormat, itemLabel
 */

/*!
 * \qmlmethod void Abstract3DSeries::setMeshAxisAndAngle(vector3d axis, real angle)
 *
 * A convenience function to construct mesh rotation quaternion from axis and angle.
 *
 * \sa meshRotation
 */

/*!
 * \internal
 */
QAbstract3DSeries::QAbstract3DSeries(QAbstract3DSeriesPrivate *d, QObject *parent) :
    QObject(parent),
    d_ptr(d)
{
}

/*!
 * Destroys QAbstract3DSeries.
 */
QAbstract3DSeries::~QAbstract3DSeries()
{
}

/*!
 * \property QAbstract3DSeries::type
 *
 * The type of the series.
 */
QAbstract3DSeries::SeriesType QAbstract3DSeries::type() const
{
    return d_ptr->m_type;
}

/*!
 * \property QAbstract3DSeries::itemLabelFormat
 *
 * Sets label \a format for data items in this series. This format is used for single item labels,
 * for example, when an item is selected. How the format is interpreted depends on series type. See
 * each series class documentation for more information.
 *
 * \sa QBar3DSeries, QScatter3DSeries, QSurface3DSeries
 */
void QAbstract3DSeries::setItemLabelFormat(const QString &format)
{
    if (d_ptr->m_itemLabelFormat != format) {
        d_ptr->setItemLabelFormat(format);
        emit itemLabelFormatChanged(format);
    }
}

QString QAbstract3DSeries::itemLabelFormat() const
{
    return d_ptr->m_itemLabelFormat;
}

/*!
 * \property QAbstract3DSeries::visible
 *
 * Sets the visibility of the series. If \a visible is false, the series is not rendered.
 * Defaults to \c{true}.
 */
void QAbstract3DSeries::setVisible(bool visible)
{
    if (d_ptr->m_visible != visible) {
        d_ptr->setVisible(visible);
        emit visibilityChanged(visible);
    }
}

bool QAbstract3DSeries::isVisible() const
{
    return d_ptr->m_visible;
}

/*!
 * \property QAbstract3DSeries::mesh
 *
 * Sets the \a mesh of the items in the series, or the selection pointer in case of
 * QSurface3DSeries. If the \a mesh is MeshUserDefined, then the userDefinedMesh property
 * must also be set for items to render properly. The default value depends on the graph type.
 */
void QAbstract3DSeries::setMesh(QAbstract3DSeries::Mesh mesh)
{
    if ((mesh == QAbstract3DSeries::MeshPoint || mesh == QAbstract3DSeries::MeshMinimal
         || mesh == QAbstract3DSeries::MeshArrow)
            && type() != QAbstract3DSeries::SeriesTypeScatter) {
        qWarning() << "Specified style is only supported for QScatter3DSeries.";
    } else if (d_ptr->m_mesh != mesh) {
        d_ptr->setMesh(mesh);
        emit meshChanged(mesh);
    }
}

QAbstract3DSeries::Mesh QAbstract3DSeries::mesh() const
{
    return d_ptr->m_mesh;
}

/*!
 * \property QAbstract3DSeries::meshSmooth
 *
 * If \a enable is \c true, smooth versions of predefined meshes set via mesh property are used.
 * This property doesn't affect custom meshes used when mesh is MeshUserDefined.
 * Defaults to \c{false}.
 */
void QAbstract3DSeries::setMeshSmooth(bool enable)
{
    if (d_ptr->m_meshSmooth != enable) {
        d_ptr->setMeshSmooth(enable);
        emit meshSmoothChanged(enable);
    }
}

bool QAbstract3DSeries::isMeshSmooth() const
{
    return d_ptr->m_meshSmooth;
}

/*!
 * \property QAbstract3DSeries::meshRotation
 *
 * Sets the mesh \a rotation that is applied to all items of the series.
 * The \a rotation should be a normalized QQuaternion.
 * For those series types that support item specific rotation, the rotations are
 * multiplied together.
 * QBar3DSeries ignores any rotation that is not around Y-axis.
 * QSurface3DSeries applies the rotation only to the selection pointer.
 * Defaults to no rotation.
 */
void QAbstract3DSeries::setMeshRotation(const QQuaternion &rotation)
{
    if (d_ptr->m_meshRotation != rotation) {
        d_ptr->setMeshRotation(rotation);
        emit meshRotationChanged(rotation);
    }
}

QQuaternion QAbstract3DSeries::meshRotation() const
{
    return d_ptr->m_meshRotation;
}

/*!
 * A convenience function to construct mesh rotation quaternion from \a axis and \a angle.
 *
 * \sa meshRotation
 */
void QAbstract3DSeries::setMeshAxisAndAngle(const QVector3D &axis, float angle)
{
    setMeshRotation(QQuaternion::fromAxisAndAngle(axis, angle));
}

/*!
 * \property QAbstract3DSeries::userDefinedMesh
 *
 * Sets the \a fileName for user defined custom mesh for objects that is used when mesh
 * is MeshUserDefined.
 * \note The file specified by \a fileName needs to be in Wavefront obj format and include
 * vertices, normals and UVs. It also needs to be in triangles.
 */
void QAbstract3DSeries::setUserDefinedMesh(const QString &fileName)
{
    if (d_ptr->m_userDefinedMesh != fileName) {
        d_ptr->setUserDefinedMesh(fileName);
        emit userDefinedMeshChanged(fileName);
    }
}

QString QAbstract3DSeries::userDefinedMesh() const
{
    return d_ptr->m_userDefinedMesh;
}

/*!
 * \property QAbstract3DSeries::colorStyle
 *
 * Sets the color \a style for the series.
 * See Q3DTheme::ColorStyle documentation for more information.
 */
void QAbstract3DSeries::setColorStyle(Q3DTheme::ColorStyle style)
{
    if (d_ptr->m_colorStyle != style) {
        d_ptr->setColorStyle(style);
        emit colorStyleChanged(style);
    }
    d_ptr->m_themeTracker.colorStyleOverride = true;
}

Q3DTheme::ColorStyle QAbstract3DSeries::colorStyle() const
{
    return d_ptr->m_colorStyle;
}

/*!
 * \property QAbstract3DSeries::baseColor
 *
 * Sets the base \a color of the series.
 * See Q3DTheme::baseColors documentation for more information.
 *
 * \sa colorStyle
 */
void QAbstract3DSeries::setBaseColor(const QColor &color)
{
    if (d_ptr->m_baseColor != color) {
        d_ptr->setBaseColor(color);
        emit baseColorChanged(color);
    }
    d_ptr->m_themeTracker.baseColorOverride = true;
}

QColor QAbstract3DSeries::baseColor() const
{
    return d_ptr->m_baseColor;
}

/*!
 * \property QAbstract3DSeries::baseGradient
 *
 * Sets the base \a gradient of the series.
 * See Q3DTheme::baseGradients documentation for more information.
 *
 * \sa colorStyle
 */
void QAbstract3DSeries::setBaseGradient(const QLinearGradient &gradient)
{
    if (d_ptr->m_baseGradient != gradient) {
        d_ptr->setBaseGradient(gradient);
        emit baseGradientChanged(gradient);
    }
    d_ptr->m_themeTracker.baseGradientOverride = true;
}

QLinearGradient QAbstract3DSeries::baseGradient() const
{
    return d_ptr->m_baseGradient;
}

/*!
 * \property QAbstract3DSeries::singleHighlightColor
 *
 * Sets the single item highlight \a color of the series.
 * See Q3DTheme::singleHighlightColor documentation for more information.
 *
 * \sa colorStyle
 */
void QAbstract3DSeries::setSingleHighlightColor(const QColor &color)
{
    if (d_ptr->m_singleHighlightColor != color) {
        d_ptr->setSingleHighlightColor(color);
        emit singleHighlightColorChanged(color);
    }
    d_ptr->m_themeTracker.singleHighlightColorOverride = true;
}

QColor QAbstract3DSeries::singleHighlightColor() const
{
    return d_ptr->m_singleHighlightColor;
}

/*!
 * \property QAbstract3DSeries::singleHighlightGradient
 *
 * Sets the single item highlight \a gradient of the series.
 * See Q3DTheme::singleHighlightGradient documentation for more information.
 *
 * \sa colorStyle
 */
void QAbstract3DSeries::setSingleHighlightGradient(const QLinearGradient &gradient)
{
    if (d_ptr->m_singleHighlightGradient != gradient) {
        d_ptr->setSingleHighlightGradient(gradient);
        emit singleHighlightGradientChanged(gradient);
    }
    d_ptr->m_themeTracker.singleHighlightGradientOverride = true;
}

QLinearGradient QAbstract3DSeries::singleHighlightGradient() const
{
    return d_ptr->m_singleHighlightGradient;
}

/*!
 * \property QAbstract3DSeries::multiHighlightColor
 *
 * Sets the multiple item highlight \a color of the series.
 * See Q3DTheme::multiHighlightColor documentation for more information.
 *
 * \sa colorStyle
 */
void QAbstract3DSeries::setMultiHighlightColor(const QColor &color)
{
    if (d_ptr->m_multiHighlightColor != color) {
        d_ptr->setMultiHighlightColor(color);
        emit multiHighlightColorChanged(color);
    }
    d_ptr->m_themeTracker.multiHighlightColorOverride = true;
}

QColor QAbstract3DSeries::multiHighlightColor() const
{
    return d_ptr->m_multiHighlightColor;
}

/*!
 * \property QAbstract3DSeries::multiHighlightGradient
 *
 * Sets the multiple item highlight \a gradient of the series.
 * See Q3DTheme::multiHighlightGradient documentation for more information.
 *
 * \sa colorStyle
 */
void QAbstract3DSeries::setMultiHighlightGradient(const QLinearGradient &gradient)
{
    if (d_ptr->m_multiHighlightGradient != gradient) {
        d_ptr->setMultiHighlightGradient(gradient);
        emit multiHighlightGradientChanged(gradient);
    }
    d_ptr->m_themeTracker.multiHighlightGradientOverride = true;
}

QLinearGradient QAbstract3DSeries::multiHighlightGradient() const
{
    return d_ptr->m_multiHighlightGradient;
}

/*!
 * \property QAbstract3DSeries::name
 *
 * Sets the series \a name.
 * Series \a name can be used in item label format with tag \c{@seriesName}.
 *
 * \sa itemLabelFormat
 */
void QAbstract3DSeries::setName(const QString &name)
{
    if (d_ptr->m_name != name) {
        d_ptr->setName(name);
        emit nameChanged(name);
    }
}

QString QAbstract3DSeries::name() const
{
    return d_ptr->m_name;
}

/*!
 * \property QAbstract3DSeries::itemLabel
 * \since QtDataVisualization 1.1
 *
 * Contains the formatted item label. If there is no selected item or the selected item is not
 * visible, returns an empty string.
 *
 * \sa itemLabelFormat
 */
QString QAbstract3DSeries::itemLabel() const
{
    return d_ptr->itemLabel();
}

/*!
 * \property QAbstract3DSeries::itemLabelVisible
 * \since QtDataVisualization 1.1
 *
 * If \c true, item labels are drawn as floating labels in the graph. Otherwise item labels are not
 * drawn. If you prefer to show the item label in an external control, set this property to
 * \c false. Defaults to \c true.
 *
 * \sa itemLabelFormat, itemLabel
 */
void QAbstract3DSeries::setItemLabelVisible(bool visible)
{
    if (d_ptr->m_itemLabelVisible != visible) {
        d_ptr->setItemLabelVisible(visible);
        emit itemLabelVisibilityChanged(visible);
    }
}

bool QAbstract3DSeries::isItemLabelVisible() const
{
    return d_ptr->m_itemLabelVisible;
}

// QAbstract3DSeriesPrivate

QAbstract3DSeriesPrivate::QAbstract3DSeriesPrivate(QAbstract3DSeries *q,
                                                   QAbstract3DSeries::SeriesType type)
    : QObject(0),
      q_ptr(q),
      m_type(type),
      m_dataProxy(0),
      m_visible(true),
      m_controller(0),
      m_mesh(QAbstract3DSeries::MeshCube),
      m_meshSmooth(false),
      m_colorStyle(Q3DTheme::ColorStyleUniform),
      m_baseColor(Qt::black),
      m_singleHighlightColor(Qt::black),
      m_multiHighlightColor(Qt::black),
      m_itemLabelDirty(true),
      m_itemLabelVisible(true)
{
}

QAbstract3DSeriesPrivate::~QAbstract3DSeriesPrivate()
{
}

QAbstractDataProxy *QAbstract3DSeriesPrivate::dataProxy() const
{
    return m_dataProxy;
}

void QAbstract3DSeriesPrivate::setDataProxy(QAbstractDataProxy *proxy)
{
    Q_ASSERT(proxy && proxy != m_dataProxy && !proxy->d_ptr->series());

    delete m_dataProxy;
    m_dataProxy = proxy;

    proxy->d_ptr->setSeries(q_ptr); // also sets parent

    if (m_controller) {
        connectControllerAndProxy(m_controller);
        m_controller->markDataDirty();
    }
}

void QAbstract3DSeriesPrivate::setController(Abstract3DController *controller)
{
    connectControllerAndProxy(controller);
    m_controller = controller;
    q_ptr->setParent(controller);
    markItemLabelDirty();
}

void QAbstract3DSeriesPrivate::setItemLabelFormat(const QString &format)
{
    m_itemLabelFormat = format;
    markItemLabelDirty();
}

void QAbstract3DSeriesPrivate::setVisible(bool visible)
{
    m_visible = visible;
    markItemLabelDirty();
}

void QAbstract3DSeriesPrivate::setMesh(QAbstract3DSeries::Mesh mesh)
{
    m_mesh = mesh;
    m_changeTracker.meshChanged = true;
    if (m_controller) {
        m_controller->markSeriesVisualsDirty();

        if (m_controller->optimizationHints().testFlag(QAbstract3DGraph::OptimizationStatic))
            m_controller->markDataDirty();
    }
}

void QAbstract3DSeriesPrivate::setMeshSmooth(bool enable)
{
    m_meshSmooth = enable;
    m_changeTracker.meshSmoothChanged = true;
    if (m_controller) {
        m_controller->markSeriesVisualsDirty();

        if (m_controller->optimizationHints().testFlag(QAbstract3DGraph::OptimizationStatic))
            m_controller->markDataDirty();
    }
}

void QAbstract3DSeriesPrivate::setMeshRotation(const QQuaternion &rotation)
{
    m_meshRotation = rotation;
    m_changeTracker.meshRotationChanged = true;
    if (m_controller) {
        m_controller->markSeriesVisualsDirty();

        if (m_controller->optimizationHints().testFlag(QAbstract3DGraph::OptimizationStatic))
            m_controller->markDataDirty();
    }
}

void QAbstract3DSeriesPrivate::setUserDefinedMesh(const QString &meshFile)
{
    m_userDefinedMesh = meshFile;
    m_changeTracker.userDefinedMeshChanged = true;
    if (m_controller) {
        m_controller->markSeriesVisualsDirty();

        if (m_controller->optimizationHints().testFlag(QAbstract3DGraph::OptimizationStatic))
            m_controller->markDataDirty();
    }
}

void QAbstract3DSeriesPrivate::setColorStyle(Q3DTheme::ColorStyle style)
{
    m_colorStyle = style;
    m_changeTracker.colorStyleChanged = true;
    if (m_controller)
        m_controller->markSeriesVisualsDirty();
}

void QAbstract3DSeriesPrivate::setBaseColor(const QColor &color)
{
    m_baseColor = color;
    m_changeTracker.baseColorChanged = true;
    if (m_controller)
        m_controller->markSeriesVisualsDirty();
}

void QAbstract3DSeriesPrivate::setBaseGradient(const QLinearGradient &gradient)
{
    m_baseGradient = gradient;
    m_changeTracker.baseGradientChanged = true;
    if (m_controller)
        m_controller->markSeriesVisualsDirty();
}

void QAbstract3DSeriesPrivate::setSingleHighlightColor(const QColor &color)
{
    m_singleHighlightColor = color;
    m_changeTracker.singleHighlightColorChanged = true;
    if (m_controller)
        m_controller->markSeriesVisualsDirty();
}

void QAbstract3DSeriesPrivate::setSingleHighlightGradient(const QLinearGradient &gradient)
{
    m_singleHighlightGradient = gradient;
    m_changeTracker.singleHighlightGradientChanged = true;
    if (m_controller)
        m_controller->markSeriesVisualsDirty();
}

void QAbstract3DSeriesPrivate::setMultiHighlightColor(const QColor &color)
{
    m_multiHighlightColor = color;
    m_changeTracker.multiHighlightColorChanged = true;
    if (m_controller)
        m_controller->markSeriesVisualsDirty();
}

void QAbstract3DSeriesPrivate::setMultiHighlightGradient(const QLinearGradient &gradient)
{
    m_multiHighlightGradient = gradient;
    m_changeTracker.multiHighlightGradientChanged = true;
    if (m_controller)
        m_controller->markSeriesVisualsDirty();
}

void QAbstract3DSeriesPrivate::setName(const QString &name)
{
    m_name = name;
    markItemLabelDirty();
    m_changeTracker.nameChanged = true;
}

void QAbstract3DSeriesPrivate::resetToTheme(const Q3DTheme &theme, int seriesIndex, bool force)
{
    int themeIndex = seriesIndex;
    if (force || !m_themeTracker.colorStyleOverride) {
        q_ptr->setColorStyle(theme.colorStyle());
        m_themeTracker.colorStyleOverride = false;
    }
    if (force || !m_themeTracker.baseColorOverride) {
        if (theme.baseColors().size() <= seriesIndex)
            themeIndex = seriesIndex % theme.baseColors().size();
        q_ptr->setBaseColor(theme.baseColors().at(themeIndex));
        m_themeTracker.baseColorOverride = false;
    }
    if (force || !m_themeTracker.baseGradientOverride) {
        if (theme.baseGradients().size() <= seriesIndex)
            themeIndex = seriesIndex % theme.baseGradients().size();
        q_ptr->setBaseGradient(theme.baseGradients().at(themeIndex));
        m_themeTracker.baseGradientOverride = false;
    }
    if (force || !m_themeTracker.singleHighlightColorOverride) {
        q_ptr->setSingleHighlightColor(theme.singleHighlightColor());
        m_themeTracker.singleHighlightColorOverride = false;
    }
    if (force || !m_themeTracker.singleHighlightGradientOverride) {
        q_ptr->setSingleHighlightGradient(theme.singleHighlightGradient());
        m_themeTracker.singleHighlightGradientOverride = false;
    }
    if (force || !m_themeTracker.multiHighlightColorOverride) {
        q_ptr->setMultiHighlightColor(theme.multiHighlightColor());
        m_themeTracker.multiHighlightColorOverride = false;
    }
    if (force || !m_themeTracker.multiHighlightGradientOverride) {
        q_ptr->setMultiHighlightGradient(theme.multiHighlightGradient());
        m_themeTracker.multiHighlightGradientOverride = false;
    }
}

QString QAbstract3DSeriesPrivate::itemLabel()
{
    if (m_itemLabelDirty) {
        QString oldLabel = m_itemLabel;
        if (m_controller && m_visible)
            createItemLabel();
        else
            m_itemLabel = QString();
        m_itemLabelDirty = false;

        if (oldLabel != m_itemLabel)
            emit q_ptr->itemLabelChanged(m_itemLabel);
    }

    return m_itemLabel;
}

void QAbstract3DSeriesPrivate::markItemLabelDirty()
{
    m_itemLabelDirty = true;
    m_changeTracker.itemLabelChanged = true;
    if (m_controller)
        m_controller->markSeriesVisualsDirty();
}

void QAbstract3DSeriesPrivate::setItemLabelVisible(bool visible)
{
    m_itemLabelVisible = visible;
    markItemLabelDirty();
    m_changeTracker.itemLabelVisibilityChanged = true;
}

QT_END_NAMESPACE_DATAVISUALIZATION
