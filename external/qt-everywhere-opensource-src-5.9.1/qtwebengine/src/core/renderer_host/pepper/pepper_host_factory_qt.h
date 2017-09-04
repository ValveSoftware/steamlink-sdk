/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#ifndef PEPPER_HOST_FACTORY_QT_H
#define PEPPER_HOST_FACTORY_QT_H

#include "base/compiler_specific.h"
#include "ppapi/host/host_factory.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/host/ppapi_host.h"

namespace content {
class BrowserPpapiHost;
}  // namespace content

namespace QtWebEngineCore {

class PepperHostFactoryQt final : public ppapi::host::HostFactory {
public:
    // Non-owning pointer to the filter must outlive this class.
    explicit PepperHostFactoryQt(content::BrowserPpapiHost* host);
    ~PepperHostFactoryQt() override;

    virtual std::unique_ptr<ppapi::host::ResourceHost> CreateResourceHost(
            ppapi::host::PpapiHost* host,
            PP_Resource resource,
            PP_Instance instance,
            const IPC::Message& message) override;
private:
    // Non-owning pointer.
    content::BrowserPpapiHost* host_;

    DISALLOW_COPY_AND_ASSIGN(PepperHostFactoryQt);
};
}  // namespace QtWebEngineCore

#endif  // PEPPER_HOST_FACTORY_QT_H
