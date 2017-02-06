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

#include "filterkey_p.h"
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DRender/private/qfilterkey_p.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

FilterKey::FilterKey()
    : BackendNode()
{
}

FilterKey::~FilterKey()
{
    cleanup();
}

void FilterKey::cleanup()
{
    QBackendNode::setEnabled(false);
    m_name.clear();
    m_value.clear();
}

void FilterKey::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QFilterKeyData>>(change);
    const auto &data = typedChange->data;
    m_name = data.name;
    m_value = data.value;
}

QVariant FilterKey::value() const
{
    return m_value;
}

QString FilterKey::name() const
{
    return m_name;
}

void FilterKey::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    switch (e->type()) {
    case PropertyUpdated: {
        QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<QPropertyUpdatedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("value"))
            m_value = propertyChange->value();
        else if (propertyChange->propertyName() == QByteArrayLiteral("name"))
            m_name = propertyChange->value().toString();

        markDirty(AbstractRenderer::AllDirty);
    }

    default:
        break;
    }
    BackendNode::sceneChangeEvent(e);
}

bool FilterKey::operator ==(const FilterKey &other)
{
    if (&other == this)
        return true;
    return ((other.name() == name()) &&
            (other.value() == value()));
}

bool FilterKey::operator !=(const FilterKey &other)
{
    return !operator ==(other);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
