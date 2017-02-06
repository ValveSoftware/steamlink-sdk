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

#include "loadproxydevicejob_p.h"
#include <Qt3DInput/private/inputhandler_p.h>
#include <Qt3DInput/private/inputmanagers_p.h>
#include "job_common_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DInput {
namespace Input {

LoadProxyDeviceJob::LoadProxyDeviceJob()
    : Qt3DCore::QAspectJob()
    , m_inputHandler(nullptr)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::DeviceProxyLoading, 0);

}

LoadProxyDeviceJob::~LoadProxyDeviceJob()
{
}

void LoadProxyDeviceJob::setProxiesToLoad(const QVector<Qt3DCore::QNodeId> &wrappers)
{
    m_proxies = wrappers;
}

void LoadProxyDeviceJob::setInputHandler(InputHandler *handler)
{
    m_inputHandler = handler;
}

InputHandler *LoadProxyDeviceJob::inputHandler() const
{
    return m_inputHandler;
}

QVector<Qt3DCore::QNodeId> LoadProxyDeviceJob::proxies() const
{
    return m_proxies;
}

void LoadProxyDeviceJob::run()
{
    Q_ASSERT(m_inputHandler);
    for (const Qt3DCore::QNodeId id : qAsConst(m_proxies)) {
        PhysicalDeviceProxy *proxy = m_inputHandler->physicalDeviceProxyManager()->lookupResource(id);
        QAbstractPhysicalDevice *device = m_inputHandler->createPhysicalDevice(proxy->deviceName());
        if (device != nullptr)
            proxy->setDevice(device);
    }
    m_proxies.clear();
}

} // namespace Input
} // namespace Qt3DInput

QT_END_NAMESPACE
