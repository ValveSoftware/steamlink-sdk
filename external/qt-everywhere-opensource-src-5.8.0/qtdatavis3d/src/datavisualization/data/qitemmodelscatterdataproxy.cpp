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

#include "qitemmodelscatterdataproxy_p.h"
#include "scatteritemmodelhandler_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QItemModelScatterDataProxy
 * \inmodule QtDataVisualization
 * \brief Proxy class for presenting data in item models with Q3DScatter.
 * \since QtDataVisualization 1.0
 *
 * QItemModelScatterDataProxy allows you to use QAbstractItemModel derived models as a data source
 * for Q3DScatter. It maps roles of QAbstractItemModel to the XYZ-values of Q3DScatter points.
 *
 * The data is resolved asynchronously whenever the mapping or the model changes.
 * QScatterDataProxy::arrayReset() is emitted when the data has been resolved. However, inserts,
 * removes, and single data item changes after the model initialization are resolved synchronously,
 * unless the same frame also contains a change that causes the whole model to be resolved.
 *
 * Mapping ignores rows and columns of the QAbstractItemModel and treats
 * all items equally. It requires the model to provide roles for the data items
 * that can be mapped to X, Y, and Z-values for the scatter points.
 *
 * For example, assume that you have a custom QAbstractItemModel for storing various measurements
 * done on material samples, providing data for roles such as "density", "hardness", and
 * "conductivity". You could visualize these properties on a scatter graph using this proxy:
 *
 * \snippet doc_src_qtdatavisualization.cpp 4
 *
 * If the fields of the model do not contain the data in the exact format you need, you can specify
 * a search pattern regular expression and a replace rule for each role to get the value in a
 * format you need. For more information how the replace using regular expressions works, see
 * QString::replace(const QRegExp &rx, const QString &after) function documentation. Note that
 * using regular expressions has an impact on the performance, so it's more efficient to utilize
 * item models where doing search and replace is not necessary to get the desired values.
 *
 * For example about using the search patterns in conjunction with the roles, see
 * ItemModelBarDataProxy usage in \l{Qt Quick 2 Bars Example}.
 *
 * \sa {Qt Data Visualization Data Handling}
 */

/*!
 * \qmltype ItemModelScatterDataProxy
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates QItemModelScatterDataProxy
 * \inherits ScatterDataProxy
 * \brief Proxy class for presenting data in item models with Scatter3D.
 *
 * This type allows you to use AbstractItemModel derived models as a data source for Scatter3D.
 *
 * The data is resolved asynchronously whenever the mapping or the model changes.
 * QScatterDataProxy::arrayReset() is emitted when the data has been resolved.
 *
 * For more details, see QItemModelScatterDataProxy documentation.
 *
 * Usage example:
 *
 * \snippet doc_src_qmldatavisualization.cpp 8
 *
 * \sa ScatterDataProxy, {Qt Data Visualization Data Handling}
 */

/*!
 * \qmlproperty model ItemModelScatterDataProxy::itemModel
 * The item model.
 */

/*!
 * \qmlproperty string ItemModelScatterDataProxy::xPosRole
 * Defines the item model role to map into X position.
 */

/*!
 * \qmlproperty string ItemModelScatterDataProxy::yPosRole
 * Defines the item model role to map into Y position.
 */

/*!
 * \qmlproperty string ItemModelScatterDataProxy::zPosRole
 * Defines the item model role to map into Z position.
 */

/*!
 * \qmlproperty string ItemModelScatterDataProxy::rotationRole
 *
 * Defines the item model role to map into item rotation.
 * The model may supply the value for rotation as either variant that is directly convertible
 * to \l quaternion, or as one of the string representations: \c{"scalar,x,y,z"} or
 * \c{"@angle,x,y,z"}. The first format will construct the \l quaternion directly with given values,
 * and the second one will construct the \l quaternion using QQuaternion::fromAxisAndAngle() method.
 */

/*!
 * \qmlproperty regExp ItemModelScatterDataProxy::xPosRolePattern
 *
 * When set, a search and replace is done on the value mapped by xPos role before it is used as
 * a item position value. This property specifies the regular expression to find the portion of the
 * mapped value to replace and xPosRoleReplace property contains the replacement string.
 *
 * \sa xPosRole, xPosRoleReplace
 */

/*!
 * \qmlproperty regExp ItemModelScatterDataProxy::yPosRolePattern
 *
 * When set, a search and replace is done on the value mapped by yPos role before it is used as
 * a item position value. This property specifies the regular expression to find the portion of the
 * mapped value to replace and yPosRoleReplace property contains the replacement string.
 *
 * \sa yPosRole, yPosRoleReplace
 */

/*!
 * \qmlproperty regExp ItemModelScatterDataProxy::zPosRolePattern
 *
 * When set, a search and replace is done on the value mapped by zPos role before it is used as
 * a item position value. This property specifies the regular expression to find the portion of the
 * mapped value to replace and zPosRoleReplace property contains the replacement string.
 *
 * \sa zPosRole, zPosRoleReplace
 */

/*!
 * \qmlproperty regExp ItemModelScatterDataProxy::rotationRolePattern
 * When set, a search and replace is done on the value mapped by rotation role before it is used
 * as a item rotation. This property specifies the regular expression to find the portion
 * of the mapped value to replace and rotationRoleReplace property contains the replacement string.
 *
 * \sa rotationRole, rotationRoleReplace
 */

/*!
 * \qmlproperty string ItemModelScatterDataProxy::xPosRoleReplace
 *
 * This property defines the replace content to be used in conjunction with xPosRolePattern.
 * Defaults to empty string. For more information on how the search and replace using regular
 * expressions works, see QString::replace(const QRegExp &rx, const QString &after)
 * function documentation.
 *
 * \sa xPosRole, xPosRolePattern
 */

/*!
 * \qmlproperty string ItemModelScatterDataProxy::yPosRoleReplace
 *
 * This property defines the replace content to be used in conjunction with yPosRolePattern.
 * Defaults to empty string. For more information on how the search and replace using regular
 * expressions works, see QString::replace(const QRegExp &rx, const QString &after)
 * function documentation.
 *
 * \sa yPosRole, yPosRolePattern
 */

/*!
 * \qmlproperty string ItemModelScatterDataProxy::zPosRoleReplace
 *
 * This property defines the replace content to be used in conjunction with zPosRolePattern.
 * Defaults to empty string. For more information on how the search and replace using regular
 * expressions works, see QString::replace(const QRegExp &rx, const QString &after)
 * function documentation.
 *
 * \sa zPosRole, zPosRolePattern
 */

/*!
 * \qmlproperty string ItemModelScatterDataProxy::rotationRoleReplace
 * This property defines the replace content to be used in conjunction with rotationRolePattern.
 * Defaults to empty string. For more information on how the search and replace using regular
 * expressions works, see QString::replace(const QRegExp &rx, const QString &after)
 * function documentation.
 *
 * \sa rotationRole, rotationRolePattern
 */

/*!
 * Constructs QItemModelScatterDataProxy with optional \a parent.
 */
QItemModelScatterDataProxy::QItemModelScatterDataProxy(QObject *parent)
    : QScatterDataProxy(new QItemModelScatterDataProxyPrivate(this), parent)
{
    dptr()->connectItemModelHandler();
}

/*!
 * Constructs QItemModelScatterDataProxy with \a itemModel and optional \a parent. Proxy doesn't take
 * ownership of the \a itemModel, as typically item models are owned by other controls.
 */
QItemModelScatterDataProxy::QItemModelScatterDataProxy(QAbstractItemModel *itemModel,
                                                       QObject *parent)
    : QScatterDataProxy(new QItemModelScatterDataProxyPrivate(this), parent)
{
    dptr()->m_itemModelHandler->setItemModel(itemModel);
    dptr()->connectItemModelHandler();
}

/*!
 * Constructs QItemModelScatterDataProxy with \a itemModel and optional \a parent. Proxy doesn't take
 * ownership of the \a itemModel, as typically item models are owned by other controls.
 * The xPosRole property is set to \a xPosRole, yPosRole property to \a yPosRole, and zPosRole property
 * to \a zPosRole.
 */
QItemModelScatterDataProxy::QItemModelScatterDataProxy(QAbstractItemModel *itemModel,
                                                       const QString &xPosRole,
                                                       const QString &yPosRole,
                                                       const QString &zPosRole,
                                                       QObject *parent)
    : QScatterDataProxy(new QItemModelScatterDataProxyPrivate(this), parent)
{
    dptr()->m_itemModelHandler->setItemModel(itemModel);
    dptr()->m_xPosRole = xPosRole;
    dptr()->m_yPosRole = yPosRole;
    dptr()->m_zPosRole = zPosRole;
    dptr()->connectItemModelHandler();
}

/*!
 * Constructs QItemModelScatterDataProxy with \a itemModel and optional \a parent. Proxy doesn't take
 * ownership of the \a itemModel, as typically item models are owned by other controls.
 * The xPosRole property is set to \a xPosRole, yPosRole property to \a yPosRole, zPosRole property
 * to \a zPosRole, and rotationRole property to \a rotationRole.
 */
QItemModelScatterDataProxy::QItemModelScatterDataProxy(QAbstractItemModel *itemModel,
                                                       const QString &xPosRole,
                                                       const QString &yPosRole,
                                                       const QString &zPosRole,
                                                       const QString &rotationRole,
                                                       QObject *parent)
    : QScatterDataProxy(new QItemModelScatterDataProxyPrivate(this), parent)
{
    dptr()->m_itemModelHandler->setItemModel(itemModel);
    dptr()->m_xPosRole = xPosRole;
    dptr()->m_yPosRole = yPosRole;
    dptr()->m_zPosRole = zPosRole;
    dptr()->m_rotationRole = rotationRole;
    dptr()->connectItemModelHandler();
}

/*!
 * Destroys QItemModelScatterDataProxy.
 */
QItemModelScatterDataProxy::~QItemModelScatterDataProxy()
{
}

/*!
 * \property QItemModelScatterDataProxy::itemModel
 *
 * Defines the item model. Does not take ownership of the model, but does connect to it to listen for
 * changes.
 */
void QItemModelScatterDataProxy::setItemModel(QAbstractItemModel *itemModel)
{
    dptr()->m_itemModelHandler->setItemModel(itemModel);
}

QAbstractItemModel *QItemModelScatterDataProxy::itemModel() const
{
    return dptrc()->m_itemModelHandler->itemModel();
}

/*!
 * \property QItemModelScatterDataProxy::xPosRole
 *
 * Defines the item model role to map into X position.
 */
void QItemModelScatterDataProxy::setXPosRole(const QString &role)
{
    if (dptr()->m_xPosRole != role) {
        dptr()->m_xPosRole = role;
        emit xPosRoleChanged(role);
    }
}

QString QItemModelScatterDataProxy::xPosRole() const
{
    return dptrc()->m_xPosRole;
}

/*!
 * \property QItemModelScatterDataProxy::yPosRole
 *
 * Defines the item model role to map into Y position.
 */
void QItemModelScatterDataProxy::setYPosRole(const QString &role)
{
    if (dptr()->m_yPosRole != role) {
        dptr()->m_yPosRole = role;
        emit yPosRoleChanged(role);
    }
}

QString QItemModelScatterDataProxy::yPosRole() const
{
    return dptrc()->m_yPosRole;
}

/*!
 * \property QItemModelScatterDataProxy::zPosRole
 *
 * Defines the item model role to map into Z position.
 */
void QItemModelScatterDataProxy::setZPosRole(const QString &role)
{
    if (dptr()->m_zPosRole != role) {
        dptr()->m_zPosRole = role;
        emit zPosRoleChanged(role);
    }
}

QString QItemModelScatterDataProxy::zPosRole() const
{
    return dptrc()->m_zPosRole;
}

/*!
 * \property QItemModelScatterDataProxy::rotationRole
 *
 * Defines the item model role to map into item rotation.
 *
 * The model may supply the value for rotation as either variant that is directly convertible
 * to QQuaternion, or as one of the string representations: \c{"scalar,x,y,z"} or \c{"@angle,x,y,z"}.
 * The first will construct the quaternion directly with given values, and the second one will
 * construct the quaternion using QQuaternion::fromAxisAndAngle() method.
 */
void QItemModelScatterDataProxy::setRotationRole(const QString &role)
{
    if (dptr()->m_rotationRole != role) {
        dptr()->m_rotationRole = role;
        emit rotationRoleChanged(role);
    }
}

QString QItemModelScatterDataProxy::rotationRole() const
{
    return dptrc()->m_rotationRole;
}

/*!
 * \property QItemModelScatterDataProxy::xPosRolePattern
 *
 * When set, a search and replace is done on the value mapped by xPos role before it is used as
 * a item position value. This property specifies the regular expression to find the portion of the
 * mapped value to replace and xPosRoleReplace property contains the replacement string.
 *
 * \sa xPosRole, xPosRoleReplace
 */
void QItemModelScatterDataProxy::setXPosRolePattern(const QRegExp &pattern)
{
    if (dptr()->m_xPosRolePattern != pattern) {
        dptr()->m_xPosRolePattern = pattern;
        emit xPosRolePatternChanged(pattern);
    }
}

QRegExp QItemModelScatterDataProxy::xPosRolePattern() const
{
    return dptrc()->m_xPosRolePattern;
}

/*!
 * \property QItemModelScatterDataProxy::yPosRolePattern
 *
 * When set, a search and replace is done on the value mapped by yPos role before it is used as
 * a item position value. This property specifies the regular expression to find the portion of the
 * mapped value to replace and yPosRoleReplace property contains the replacement string.
 *
 * \sa yPosRole, yPosRoleReplace
 */
void QItemModelScatterDataProxy::setYPosRolePattern(const QRegExp &pattern)
{
    if (dptr()->m_yPosRolePattern != pattern) {
        dptr()->m_yPosRolePattern = pattern;
        emit yPosRolePatternChanged(pattern);
    }
}

QRegExp QItemModelScatterDataProxy::yPosRolePattern() const
{
    return dptrc()->m_yPosRolePattern;
}

/*!
 * \property QItemModelScatterDataProxy::zPosRolePattern
 *
 * When set, a search and replace is done on the value mapped by zPos role before it is used as
 * a item position value. This property specifies the regular expression to find the portion of the
 * mapped value to replace and zPosRoleReplace property contains the replacement string.
 *
 * \sa zPosRole, zPosRoleReplace
 */
void QItemModelScatterDataProxy::setZPosRolePattern(const QRegExp &pattern)
{
    if (dptr()->m_zPosRolePattern != pattern) {
        dptr()->m_zPosRolePattern = pattern;
        emit zPosRolePatternChanged(pattern);
    }
}

QRegExp QItemModelScatterDataProxy::zPosRolePattern() const
{
    return dptrc()->m_zPosRolePattern;
}

/*!
 * \property QItemModelScatterDataProxy::rotationRolePattern
 *
 * When set, a search and replace is done on the value mapped by rotation role before it is used
 * as a item rotation. This property specifies the regular expression to find the portion
 * of the mapped value to replace and rotationRoleReplace property contains the replacement string.
 *
 * \sa rotationRole, rotationRoleReplace
 */
void QItemModelScatterDataProxy::setRotationRolePattern(const QRegExp &pattern)
{
    if (dptr()->m_rotationRolePattern != pattern) {
        dptr()->m_rotationRolePattern = pattern;
        emit rotationRolePatternChanged(pattern);
    }
}

QRegExp QItemModelScatterDataProxy::rotationRolePattern() const
{
    return dptrc()->m_rotationRolePattern;
}

/*!
 * \property QItemModelScatterDataProxy::xPosRoleReplace
 *
 * This property defines the replace content to be used in conjunction with xPosRolePattern.
 * Defaults to empty string. For more information on how the search and replace using regular
 * expressions works, see QString::replace(const QRegExp &rx, const QString &after)
 * function documentation.
 *
 * \sa xPosRole, xPosRolePattern
 */
void QItemModelScatterDataProxy::setXPosRoleReplace(const QString &replace)
{
    if (dptr()->m_xPosRoleReplace != replace) {
        dptr()->m_xPosRoleReplace = replace;
        emit xPosRoleReplaceChanged(replace);
    }
}

QString QItemModelScatterDataProxy::xPosRoleReplace() const
{
    return dptrc()->m_xPosRoleReplace;
}

/*!
 * \property QItemModelScatterDataProxy::yPosRoleReplace
 *
 * This property defines the replace content to be used in conjunction with yPosRolePattern.
 * Defaults to empty string. For more information on how the search and replace using regular
 * expressions works, see QString::replace(const QRegExp &rx, const QString &after)
 * function documentation.
 *
 * \sa yPosRole, yPosRolePattern
 */
void QItemModelScatterDataProxy::setYPosRoleReplace(const QString &replace)
{
    if (dptr()->m_yPosRoleReplace != replace) {
        dptr()->m_yPosRoleReplace = replace;
        emit yPosRoleReplaceChanged(replace);
    }
}

QString QItemModelScatterDataProxy::yPosRoleReplace() const
{
    return dptrc()->m_yPosRoleReplace;
}

/*!
 * \property QItemModelScatterDataProxy::zPosRoleReplace
 *
 * This property defines the replace content to be used in conjunction with zPosRolePattern.
 * Defaults to empty string. For more information on how the search and replace using regular
 * expressions works, see QString::replace(const QRegExp &rx, const QString &after)
 * function documentation.
 *
 * \sa zPosRole, zPosRolePattern
 */
void QItemModelScatterDataProxy::setZPosRoleReplace(const QString &replace)
{
    if (dptr()->m_zPosRoleReplace != replace) {
        dptr()->m_zPosRoleReplace = replace;
        emit zPosRoleReplaceChanged(replace);
    }
}

QString QItemModelScatterDataProxy::zPosRoleReplace() const
{
    return dptrc()->m_zPosRoleReplace;
}

/*!
 * \property QItemModelScatterDataProxy::rotationRoleReplace
 *
 * This property defines the replace content to be used in conjunction with rotationRolePattern.
 * Defaults to empty string. For more information on how the search and replace using regular
 * expressions works, see QString::replace(const QRegExp &rx, const QString &after)
 * function documentation.
 *
 * \sa rotationRole, rotationRolePattern
 */
void QItemModelScatterDataProxy::setRotationRoleReplace(const QString &replace)
{
    if (dptr()->m_rotationRoleReplace != replace) {
        dptr()->m_rotationRoleReplace = replace;
        emit rotationRoleReplaceChanged(replace);
    }
}

QString QItemModelScatterDataProxy::rotationRoleReplace() const
{
    return dptrc()->m_rotationRoleReplace;
}

/*!
 * Changes \a xPosRole, \a yPosRole, \a zPosRole, and \a rotationRole mapping.
 */
void QItemModelScatterDataProxy::remap(const QString &xPosRole, const QString &yPosRole,
                                       const QString &zPosRole, const QString &rotationRole)
{
    setXPosRole(xPosRole);
    setYPosRole(yPosRole);
    setZPosRole(zPosRole);
    setRotationRole(rotationRole);
}

/*!
 * \internal
 */
QItemModelScatterDataProxyPrivate *QItemModelScatterDataProxy::dptr()
{
    return static_cast<QItemModelScatterDataProxyPrivate *>(d_ptr.data());
}

/*!
 * \internal
 */
const QItemModelScatterDataProxyPrivate *QItemModelScatterDataProxy::dptrc() const
{
    return static_cast<const QItemModelScatterDataProxyPrivate *>(d_ptr.data());
}

// QItemModelScatterDataProxyPrivate

QItemModelScatterDataProxyPrivate::QItemModelScatterDataProxyPrivate(QItemModelScatterDataProxy *q)
    : QScatterDataProxyPrivate(q),
      m_itemModelHandler(new ScatterItemModelHandler(q))
{
}

QItemModelScatterDataProxyPrivate::~QItemModelScatterDataProxyPrivate()
{
    delete m_itemModelHandler;
}

QItemModelScatterDataProxy *QItemModelScatterDataProxyPrivate::qptr()
{
    return static_cast<QItemModelScatterDataProxy *>(q_ptr);
}

void QItemModelScatterDataProxyPrivate::connectItemModelHandler()
{
    QObject::connect(m_itemModelHandler, &ScatterItemModelHandler::itemModelChanged,
                     qptr(), &QItemModelScatterDataProxy::itemModelChanged);
    QObject::connect(qptr(), &QItemModelScatterDataProxy::xPosRoleChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelScatterDataProxy::yPosRoleChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelScatterDataProxy::zPosRoleChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelScatterDataProxy::rotationRoleChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelScatterDataProxy::xPosRolePatternChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelScatterDataProxy::yPosRolePatternChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelScatterDataProxy::zPosRolePatternChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelScatterDataProxy::rotationRolePatternChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelScatterDataProxy::xPosRoleReplaceChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelScatterDataProxy::yPosRoleReplaceChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelScatterDataProxy::zPosRoleReplaceChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
    QObject::connect(qptr(), &QItemModelScatterDataProxy::rotationRoleReplaceChanged,
                     m_itemModelHandler, &AbstractItemModelHandler::handleMappingChanged);
}

QT_END_NAMESPACE_DATAVISUALIZATION
