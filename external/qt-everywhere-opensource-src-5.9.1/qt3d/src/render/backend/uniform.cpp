/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include "uniform_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

namespace {

const int qNodeIdTypeId = qMetaTypeId<Qt3DCore::QNodeId>();

// glUniform*fv/glUniform*iv/glUniform*uiv -> only handles sizeof(float)/sizeof(int)
int byteSizeForMetaType(int type)
{
    switch (type) {
    case QMetaType::Bool:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::ULongLong:
    case QMetaType::LongLong:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Char:
    case QMetaType::UChar:
        return sizeof(int);

    case QMetaType::Float:
    case QMetaType::Double: // Assumes conversion to float
        return sizeof(float);

    case QMetaType::QPoint:
    case QMetaType::QSize:
        return 2 * sizeof(int);

    case QMetaType::QRect:
        return 4 * sizeof(int);

    case QMetaType::QPointF:
    case QMetaType::QSizeF:
    case QMetaType::QVector2D:
        return 2 * sizeof(float);

    case QMetaType::QVector3D:
        return 3 * sizeof(float);

    case QMetaType::QRectF:
    case QMetaType::QVector4D:
    case QMetaType::QColor:
        return 4 * sizeof(float);

    case QMetaType::QMatrix4x4:
        return 16 * sizeof(float);
    default:
        Q_UNREACHABLE();
        return -1;
    }
}

} // anonymous

UniformValue UniformValue::fromVariant(const QVariant &variant)
{
    // Texture/Buffer case
    if (variant.userType() == qNodeIdTypeId)
        return UniformValue(variant.value<Qt3DCore::QNodeId>());

    UniformValue v;
    switch (variant.userType()) {
    case QMetaType::Bool:
        v.data<bool>()[0] = variant.toBool();
        break;
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::LongLong:
    case QMetaType::Short:
    case QMetaType::ULong:
    case QMetaType::ULongLong:
    case QMetaType::UShort:
    case QMetaType::Char:
    case QMetaType::UChar:
        v.data<int>()[0] = variant.toInt();
        v.m_storedType = Int;
        break;
    case QMetaType::Float:
    case QMetaType::Double: // Convert double to floats
        v.m_data[0] = variant.toFloat();
        break;
    case QMetaType::QPoint: {
        const QPoint p = variant.toPoint();
        v.data<int>()[0] = p.x();
        v.data<int>()[1] = p.y();
        break;
    }
    case QMetaType::QSize: {
        const QSize s = variant.toSize();
        v.data<int>()[0] = s.width();
        v.data<int>()[1] = s.height();
        break;
    }
    case QMetaType::QRect: {
        const QRect r = variant.toRect();
        v.data<int>()[0] = r.x();
        v.data<int>()[1] = r.y();
        v.data<int>()[2] = r.width();
        v.data<int>()[3] = r.height();
        break;
    }
    case QMetaType::QSizeF: {
        const QSizeF s = variant.toSize();
        v.m_data[0] = s.width();
        v.m_data[1] = s.height();
        break;
    }
    case QMetaType::QPointF: {
        const QPointF p = variant.toPointF();
        v.m_data[0] = p.x();
        v.m_data[1] = p.y();
        break;
    }
    case QMetaType::QRectF: {
        const QRectF r = variant.toRect();
        v.m_data[0] = r.x();
        v.m_data[1] = r.y();
        v.m_data[2] = r.width();
        v.m_data[3] = r.height();
        break;
    }
    case QMetaType::QVector2D: {
        const QVector2D vec2 = variant.value<QVector2D>();
        v.m_data[0] = vec2.x();
        v.m_data[1] = vec2.y();
        break;
    }
    case QMetaType::QVector3D: {
        const QVector3D vec3 = variant.value<QVector3D>();
        v.m_data[0] = vec3.x();
        v.m_data[1] = vec3.y();
        v.m_data[2] = vec3.z();
        break;
    }
    case QMetaType::QVector4D: {
        const QVector4D vec4 = variant.value<QVector4D>();
        v.m_data[0] = vec4.x();
        v.m_data[1] = vec4.y();
        v.m_data[2] = vec4.z();
        v.m_data[3] = vec4.w();
        break;
    }
    case QMetaType::QColor: {
        const QColor col = variant.value<QColor>();
        v.m_data[0] = col.redF();
        v.m_data[1] = col.greenF();
        v.m_data[2] = col.blueF();
        v.m_data[3] = col.alphaF();
        break;
    }
    case QMetaType::QMatrix4x4: {
        const QMatrix4x4 mat44 = variant.value<QMatrix4x4>();
        // Use constData because we want column-major layout
        v.m_data.resize(16);
        memcpy(v.data<float>(), mat44.constData(), 16 * sizeof(float));
        break;
    }
    case QMetaType::QVariantList: {
        const QVariantList variants = variant.toList();
        if (variants.size() < 1)
            break;

        const int listEntryType = variants.first().userType();
        const int stride = byteSizeForMetaType(listEntryType) / sizeof(float);
        // Resize v.m_data
        v.m_data.resize(stride * variants.size());

        int idx = 0;
        for (const QVariant &variant : variants) {
            Q_ASSERT_X(variant.userType() == listEntryType,
                       Q_FUNC_INFO,
                       "Uniform array doesn't contain elements of the same type");
            UniformValue vi = UniformValue::fromVariant(variant);
            memcpy(v.data<float>() + idx, vi.data<float>(), stride * sizeof(float));
            idx += stride;
        }
        break;
    }

    default:
        qWarning() << "Unknown uniform type or value:" << variant << "Please check your QParameters";
    }
    return v;
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
