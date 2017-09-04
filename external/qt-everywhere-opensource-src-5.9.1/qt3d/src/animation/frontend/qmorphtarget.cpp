/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmorphtarget.h"
#include "Qt3DAnimation/private/qmorphtarget_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {

/*!
    \class Qt3DAnimation::QMorphTarget
    \brief A class providing morph targets to blend-shape animation
    \inmodule Qt3DAnimation
    \since 5.9
    \inherits QObject

    A Qt3DAnimation::QMorphTarget class is a convenience class, which provides a list
    of \l {Qt3DRender::QAttribute} {QAttributes}, which the QMorphingAnimation uses
    to animate geometry. A QMorphTarget can also be created based on existing
    \l Qt3DRender::QGeometry.

*/
/*!
    \qmltype MorphTarget
    \brief A type providing morph targets to blend-shape animation
    \inqmlmodule Qt3D.Animation
    \since 5.9
    \inherits QtObject
    \instantiates Qt3DAnimation::QMorphTarget

    A MorphTarget type is a convenience type, which provides a list
    of \l {Qt3D.Render::Attribute} {Attributes}, which the MorphingAnimation uses
    to animate geometry. A MorphTarget can also be created based on existing
    \l {Qt3D.Render::Geometry}{Geometry}.

*/

/*!
    \property Qt3DAnimation::QMorphTarget::attributeNames
    Holds a list of attribute names contained in the morph target.
    \readonly
*/

/*!
    \qmlproperty list<string> MorphTarget::attributeNames
    Holds a list of attribute names contained in the morph target.
    \readonly
*/
/*!
    \qmlproperty list<Attribute> MorphTarget::attributes
    Holds the list of attributes in the morph target.
*/
/*!
    \qmlmethod MorphTarget Qt3D.Animation::MorphTarget::fromGeometry(geometry, stringList)
    Returns a morph target based on the attributes defined by the given stringList from
    the given geometry.
*/

QMorphTargetPrivate::QMorphTargetPrivate()
    : QObjectPrivate()
{

}

void QMorphTargetPrivate::updateAttributeNames()
{
    m_attributeNames.clear();
    for (const Qt3DRender::QAttribute *attr : qAsConst(m_targetAttributes))
        m_attributeNames.push_back(attr->name());
}

/*!
    Constructs a QMorphTarget with given \a parent.
*/
QMorphTarget::QMorphTarget(QObject *parent)
    : QObject(*new QMorphTargetPrivate, parent)
{

}

/*!
    Returns a list of attributes contained in the morph target.
*/
QVector<Qt3DRender::QAttribute *> QMorphTarget::attributeList() const
{
    Q_D(const QMorphTarget);
    return d->m_targetAttributes;
}

QStringList QMorphTarget::attributeNames() const
{
    Q_D(const QMorphTarget);
    return d->m_attributeNames;
}

/*!
    Sets \a attributes to the morph target. Old attributes are cleared.
*/
void QMorphTarget::setAttributes(const QVector<Qt3DRender::QAttribute *> &attributes)
{
    Q_D(QMorphTarget);
    d->m_targetAttributes = attributes;
    d->m_attributeNames.clear();
    for (const Qt3DRender::QAttribute *attr : attributes)
        d->m_attributeNames.push_back(attr->name());

    emit attributeNamesChanged(d->m_attributeNames);
}

/*!
    Adds an \a attribute the morph target. An attribute with the same
    name must not have been added previously to the morph target.
*/
void QMorphTarget::addAttribute(Qt3DRender::QAttribute *attribute)
{
    Q_D(QMorphTarget);
    for (const Qt3DRender::QAttribute *attr : qAsConst(d->m_targetAttributes)) {
        if (attr->name() == attribute->name())
            return;
    }
    d->m_targetAttributes.push_back(attribute);
    d->m_attributeNames.push_back(attribute->name());
    emit attributeNamesChanged(d->m_attributeNames);
}

/*!
    Removes an \a attribute from the morph target.
*/
void QMorphTarget::removeAttribute(Qt3DRender::QAttribute *attribute)
{
    Q_D(QMorphTarget);
    if (d->m_targetAttributes.contains(attribute)) {
        d->m_targetAttributes.removeAll(attribute);
        d->updateAttributeNames();
        emit attributeNamesChanged(d->m_attributeNames);
    }
}

/*!
    Returns a morph target based on the \a attributes in the given \a geometry.
*/
QMorphTarget *QMorphTarget::fromGeometry(Qt3DRender::QGeometry *geometry, const QStringList &attributes)
{
    QMorphTarget *target = new QMorphTarget();
    const auto geometryAttributes = geometry->attributes();
    for (Qt3DRender::QAttribute *attr : geometryAttributes) {
        if (attributes.contains(attr->name()))
            target->addAttribute(attr);
    }
    return target;
}

} // Qt3DAnimation

QT_END_NAMESPACE
