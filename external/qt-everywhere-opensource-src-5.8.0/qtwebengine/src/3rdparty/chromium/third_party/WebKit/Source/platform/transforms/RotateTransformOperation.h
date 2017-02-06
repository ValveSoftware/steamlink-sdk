/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RotateTransformOperation_h
#define RotateTransformOperation_h

#include "platform/geometry/FloatPoint3D.h"
#include "platform/transforms/Rotation.h"
#include "platform/transforms/TransformOperation.h"

namespace blink {

class PLATFORM_EXPORT RotateTransformOperation final : public TransformOperation {
public:
    static PassRefPtr<RotateTransformOperation> create(double angle, OperationType type)
    {
        return create(Rotation(FloatPoint3D(0, 0, 1), angle), type);
    }

    static PassRefPtr<RotateTransformOperation> create(double x, double y, double z, double angle, OperationType type)
    {
        return create(Rotation(FloatPoint3D(x, y, z), angle), type);
    }

    static PassRefPtr<RotateTransformOperation> create(const Rotation& rotation, OperationType type)
    {
        return adoptRef(new RotateTransformOperation(rotation, type));
    }

    double x() const { return m_rotation.axis.x(); }
    double y() const { return m_rotation.axis.y(); }
    double z() const { return m_rotation.axis.z(); }
    double angle() const { return m_rotation.angle; }
    const FloatPoint3D& axis() const { return m_rotation.axis; }

    static bool getCommonAxis(const RotateTransformOperation*, const RotateTransformOperation*, FloatPoint3D& resultAxis, double& resultAngleA, double& resultAngleB);

    virtual bool canBlendWith(const TransformOperation& other) const;
    OperationType type() const override { return m_type; }

    void apply(TransformationMatrix& transform, const FloatSize& /*borderBoxSize*/) const override
    {
        transform.rotate3d(m_rotation);
    }

    static bool isMatchingOperationType(OperationType type) { return type == Rotate || type == RotateX || type == RotateY || type == RotateZ || type == Rotate3D; }

private:
    bool operator==(const TransformOperation&) const override;

    PassRefPtr<TransformOperation> blend(const TransformOperation* from, double progress, bool blendToIdentity = false) override;
    PassRefPtr<TransformOperation> zoom(double factor) final { return this; }

    RotateTransformOperation(const Rotation& rotation, OperationType type)
        : m_rotation(rotation)
        , m_type(type)
    {
        ASSERT(isMatchingOperationType(type));
    }

    const Rotation m_rotation;
    const OperationType m_type;
};

DEFINE_TRANSFORM_TYPE_CASTS(RotateTransformOperation);

} // namespace blink

#endif // RotateTransformOperation_h
