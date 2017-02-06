/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
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

#ifndef WLDATAOFFER_H
#define WLDATAOFFER_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QPointer>
#include <QtWaylandCompositor/private/qwayland-server-wayland.h>

QT_BEGIN_NAMESPACE

namespace QtWayland
{

class DataSource;

class DataOffer : public QtWaylandServer::wl_data_offer
{
public:
    DataOffer(DataSource *data_source, QtWaylandServer::wl_data_device::Resource *target);
    ~DataOffer();

protected:
    void data_offer_accept(Resource *resource, uint32_t serial, const QString &mime_type) Q_DECL_OVERRIDE;
    void data_offer_receive(Resource *resource, const QString &mime_type, int32_t fd) Q_DECL_OVERRIDE;
    void data_offer_destroy(Resource *resource) Q_DECL_OVERRIDE;
    void data_offer_destroy_resource(Resource *resource) Q_DECL_OVERRIDE;

private:
    QPointer<DataSource> m_dataSource;
};

}

QT_END_NAMESPACE

#endif // WLDATAOFFER_H
