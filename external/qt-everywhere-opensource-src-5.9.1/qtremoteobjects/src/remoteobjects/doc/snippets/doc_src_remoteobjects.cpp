/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

//! [qremoteobjectnode_include]
#include <QtRemoteObjects/QRemoteObjectNode>
//! [qremoteobjectnode_include]

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

//! [create_registry]
QRemoteObjectNode registryNode = QRemoteObjectNode::createRegistryHostNode();
//! [create_registry]

//! [create_host_nodes]
QRemoteObjectNode hostNode = QRemoteObjectNode::createHostNode(QUrl("local://mynode"));
QRemoteObjectNode registeredHostNode = QRemoteObjectNode::createHostNodeConnectedToRegistry();
//! [create_host_nodes]

//! [create_connect_client_nodes]
QRemoteObjectNode clientNode = QRemoteObjectNode();
QRemoteObjectNode registryClientNode = QRemoteObjectNode::createNodeConnectedToRegistry();
clientNode.connect(QUrl("local://mynode"));
//! [create_connect_client_nodes]

//! [share_source]
MinuteTimer timer;
hostNode.enableRemoting(&timer);
//! [share_source]

//! [api_source]
#include "rep_TimeModel_source.h"
MinuteTimer timer;
hostNode.enableRemoting<MinuteTimerSourceAPI>(&timer);
//! [api_source]

//! [create_replica]
#include "rep_TimeModel_replica.h"
QScopedPointer<MinuteTimerReplica> d_ptr;
//! [create_replica]

//! [dynamic_replica]
QRemoteObjectDynamicReplica *dynamicRep;
//! [dynamic_replica]

//! [acquire_replica]
d_ptr->reset(clientNode.acquire("MinuteTimer"));
*dynamicRep = clientNode.acquire("MinuteTimer");
//! [acquire_replica]

    Q_UNUSED(timer);
    return app.exec();
}
