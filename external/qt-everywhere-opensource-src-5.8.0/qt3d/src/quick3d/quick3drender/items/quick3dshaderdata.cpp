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

#include <Qt3DQuickRender/private/quick3dshaderdataarray_p.h>
#include <QMetaProperty>

#include "quick3dshaderdata_p.h"
#include <private/qshaderdata_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {
namespace Quick {

namespace {

const int qjsValueTypeId = qMetaTypeId<QJSValue>();
const int quick3DShaderDataArrayTypeId = qMetaTypeId<Quick3DShaderDataArray*>();
const int quick3DShaderDataTypeId = qMetaTypeId<Quick3DShaderData*>();

}

class Quick3DShaderDataPropertyReader : public PropertyReaderInterface
{
public:
    Quick3DShaderDataPropertyReader()
    {
    }

    QVariant readProperty(const QVariant &v) Q_DECL_OVERRIDE
    {
        // qjsValueTypeId are not compile time constant (no switch)
        if (v.userType() == qjsValueTypeId) {
            QJSValue jsValue = v.value<QJSValue>();
            if (jsValue.isArray())
                return v.value<QVariantList>();
            else if (jsValue.isVariant())
                return jsValue.toVariant();
        } else if (v.userType() == quick3DShaderDataArrayTypeId) {
            Quick3DShaderDataArray *array = v.value<Quick3DShaderDataArray *>();
            QVariantList innerValues;
            if (array) {
                const auto values = array->values();
                for (QShaderData *d : values) {
                    if (d)
                        innerValues.append(QVariant::fromValue(d->id()));
                }
            }
            return innerValues;
        } else if (v.userType() == quick3DShaderDataTypeId) {
            Qt3DCore::QNodeId id;
            QShaderData *shaderData = v.value<Quick3DShaderData *>();
            if (shaderData)
                id = shaderData->id();
            return QVariant::fromValue(id);
        }
        return v;
    }
};

Quick3DShaderData::Quick3DShaderData(QNode *parent)
    : QShaderData(*new QShaderDataPrivate(PropertyReaderInterfacePtr(new Quick3DShaderDataPropertyReader()))
                  , parent)
{
}

} // namespace Quick
} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
