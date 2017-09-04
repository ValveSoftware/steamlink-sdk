/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <Qt3DRender/qtexture.h>

#include <Qt3DQuickRender/private/quick3dmaterial_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

namespace Quick {

Quick3DMaterial::Quick3DMaterial(QObject *parent)
    : QObject(parent)
{
}

QQmlListProperty<QParameter> Quick3DMaterial::qmlParameters()
{
    return QQmlListProperty<QParameter>(this, 0,
                                       &Quick3DMaterial::appendParameter,
                                       &Quick3DMaterial::parameterCount,
                                       &Quick3DMaterial::parameterAt,
                                       &Quick3DMaterial::clearParameters);
}

void Quick3DMaterial::appendParameter(QQmlListProperty<QParameter> *list, QParameter *param)
{
    Quick3DMaterial *mat = qobject_cast<Quick3DMaterial *>(list->object);
    if (mat) {
        param->setParent(mat->parentMaterial());
        mat->parentMaterial()->addParameter(param);
    }
}

QParameter *Quick3DMaterial::parameterAt(QQmlListProperty<QParameter> *list, int index)
{
    Quick3DMaterial *mat = qobject_cast<Quick3DMaterial *>(list->object);
    if (mat)
        return mat->parentMaterial()->parameters().at(index);
    return 0;
}

int Quick3DMaterial::parameterCount(QQmlListProperty<QParameter> *list)
{
    Quick3DMaterial *mat = qobject_cast<Quick3DMaterial *>(list->object);
    if (mat)
        return mat->parentMaterial()->parameters().count();
    return 0;
}

void Quick3DMaterial::clearParameters(QQmlListProperty<QParameter> *list)
{
    Quick3DMaterial *mat = qobject_cast<Quick3DMaterial *>(list->object);
    if (mat) {
        const auto parameters = mat->parentMaterial()->parameters();
        for (QParameter *p : parameters)
            mat->parentMaterial()->removeParameter(p);
    }
}

} // Quick

} // namespace Render

} // Qt3D

QT_END_NAMESPACE
