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

#include "q3dobject_p.h"
#include "q3dscene_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
   \class Q3DObject
   \inmodule QtDataVisualization
   \brief The Q3DObject class is a simple base class for all the objects in a
   3D scene.
   \since QtDataVisualization 1.0

    Contains position information for an object in a 3D scene.
    The object is considered to be a single point in the coordinate space without dimensions.
*/

/*!
   \qmltype Object3D
   \inqmlmodule QtDataVisualization
   \since QtDataVisualization 1.0
   \ingroup datavisualization_qml
   \instantiates Q3DObject
   \brief A base type for all the objects in a 3D scene.

    An uncreatable base type that contains position information for an object in
    a 3D scene. The object is considered to be a single point in the coordinate space without
    dimensions.
*/

/*!
 * \qmlproperty vector3d Object3D::position
 *
 * The 3D position of the object.
 *
 * \note Currently setting this property has no effect for Camera3D, as the position is handled
 * internally.
 */

/*!
 * Constructs a new 3D object with the position set to origin by default. An
 * optional \a parent parameter can be given and is then passed to the QObject
 * constructor.
 */
Q3DObject::Q3DObject(QObject *parent) :
    QObject(parent),
    d_ptr(new Q3DObjectPrivate(this))
{
}

/*!
 *  Destroys the 3D object.
 */
Q3DObject::~Q3DObject()
{
}

/*!
 * Copies the 3D object position from the given \a source 3D object to this 3D object instance.
 */
void Q3DObject::copyValuesFrom(const Q3DObject &source)
{
    d_ptr->m_position = source.d_ptr->m_position;
    setDirty(true);
}

/*!
 * \property Q3DObject::parentScene
 *
 * \brief The parent scene as a read only value.
 *
 * If the object has no parent scene, the value is 0.
 */
Q3DScene *Q3DObject::parentScene()
{
    return qobject_cast<Q3DScene *>(parent());
}

/*!
 * \property Q3DObject::position
 *
 * \brief The 3D position of the object.
 *
 * \note Currently setting this property has no effect for Q3DCamera, as the position is handled
 * internally.
 */
QVector3D Q3DObject::position() const
{
    return d_ptr->m_position;
}

void Q3DObject::setPosition(const QVector3D &position)
{
    if (d_ptr->m_position != position) {
        d_ptr->m_position = position;
        setDirty(true);
        emit positionChanged(d_ptr->m_position);
    }
}

/*!
 * Sets \a dirty to \c true if the 3D object has changed since the last update.
 */
void Q3DObject::setDirty(bool dirty)
{
    d_ptr->m_isDirty = dirty;
    if (parentScene())
        parentScene()->d_ptr->markDirty();
}

/*!
 * Returns whether the 3D object has changed.
 */
bool Q3DObject::isDirty() const
{
    return d_ptr->m_isDirty;
}

Q3DObjectPrivate::Q3DObjectPrivate(Q3DObject *q) :
    q_ptr(q),
    m_isDirty(true)
{
}

Q3DObjectPrivate::~Q3DObjectPrivate()
{

}

QT_END_NAMESPACE_DATAVISUALIZATION
