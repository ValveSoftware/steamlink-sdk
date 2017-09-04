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

#include "parameter_p.h"
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/private/qparameter_p.h>
#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qshaderdata.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/private/buffer_p.h>

#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/stringtoint_p.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

Parameter::Parameter()
    : BackendNode()
    , m_nameId(-1)
{
}

void Parameter::cleanup()
{
    QBackendNode::setEnabled(false);
    m_nameId = -1;
    m_name.clear();
    m_uniformValue = UniformValue();
}

void Parameter::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QParameterData>>(change);
    const auto &data = typedChange->data;
    m_name = data.name;
    m_nameId = StringToInt::lookupId(m_name);
    m_uniformValue = UniformValue::fromVariant(data.backendValue);
}

void Parameter::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<QPropertyUpdatedChange>(e);

    if (e->type() == PropertyUpdated) {
        if (propertyChange->propertyName() == QByteArrayLiteral("name")) {
            m_name = propertyChange->value().toString();
            m_nameId = StringToInt::lookupId(m_name);
        } else if (propertyChange->propertyName() == QByteArrayLiteral("value")) {
            m_uniformValue = UniformValue::fromVariant(propertyChange->value());
        }
        markDirty(AbstractRenderer::AllDirty);
    }

    BackendNode::sceneChangeEvent(e);
}

QString Parameter::name() const
{
    return m_name;
}

int Parameter::nameId() const Q_DECL_NOTHROW
{
    return m_nameId;
}

UniformValue Parameter::uniformValue() const
{
    return m_uniformValue;
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
