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

//! [simpleSwitch_rep]
class SimpleSwitch
{
    PROP(bool currState=false);
    SLOT(server_slot(bool clientState));
};
//! [simpleSwitch_rep]

//! [simpleSwitch_repsource_example1]
REPC_SOURCE = simpleswitch.rep
//! [simpleSwitch_repsource_example1]

//! [simpleSwitch_remoteobjectsadd_example1]
QT       += remoteobjects
//! [simpleSwitch_remoteobjectsadd_example1]

//! [simpleSwitch_serverheader_example1]
#ifndef SIMPLESWITCH_H
#define SIMPLESWITCH_H

#include "rep_SimpleSwitch_source.h"

class SimpleSwitch : public SimpleSwitchSimpleSource
{
    Q_OBJECT
public:
    SimpleSwitch(QObject *parent = nullptr);
    ~SimpleSwitch();
    virtual void server_slot(bool clientState);
public Q_SLOTS:
    void timeout_slot();
private:
    QTimer *stateChangeTimer;
};

#endif
//! [simpleSwitch_serverheader_example1]

//! [simpleSwitch_serversource_example1]
#include "simpleswitch.h"

// constructor
SimpleSwitch::SimpleSwitch(QObject *parent) : SimpleSwitchSimpleSource(parent)
{
    stateChangeTimer = new QTimer(this); // Initialize timer
    QObject::connect(stateChangeTimer, SIGNAL(timeout()), this, SLOT(timeout_slot())); // connect timeout() signal from stateChangeTimer to timeout_slot() of simpleSwitch
    stateChangeTimer->start(2000); // Start timer and set timout to 2 seconds
    qDebug() << "Source Node Started";
}

//destructor
SimpleSwitch::~SimpleSwitch()
{
    stateChangeTimer->stop();
}

void SimpleSwitch::server_slot(bool clientState)
{
    qDebug() << "Replica state is " << clientState; // print switch state echoed back by client
}

void SimpleSwitch::timeout_slot()
{
    // slot called on timer timeout
    if (currState()) // check if current state is true, currState() is defined in repc generated rep_SimpleSwitch_source.h
        setCurrState(false); // set state to false
    else
        setCurrState(true); // set state to true
    qDebug() << "Source State is "<<currState();

}
//! [simpleSwitch_serversource_example1]

//! [simpleSwitch_serverhostnode_example1]
QRemoteObjectNode srcNode = QRemoteObjectNode::createHostNode();
//! [simpleSwitch_serverhostnode_example1]

//! [simpleSwitch_enableremoting_example1]
SimpleSwitch srcSwitch; // create simple switch
srcNode.enableRemoting(&srcSwitch); // enable remoting
//! [simpleSwitch_enableremoting_example1]

//! [simpleSwitch_servermaincpp_example1]
#include <QCoreApplication>
#include "simpleswitch.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    SimpleSwitch srcSwitch; // create simple switch

    QRemoteObjectNode srcNode = QRemoteObjectNode::createHostNode(); // create host node without Regsitry
    //The static node creation routines take one or two URLs as input parameters, but they have default values to help people getting started.
    //It is recommended use your own URLs in any production environment to avoid name conflicts.
    srcNode.enableRemoting(&srcSwitch); // enable remoting/sharing

    return a.exec();
}
//! [simpleSwitch_servermaincpp_example1]

//! [simpleSwitch_clientrep_example1]
REPC_REPLICA = simpleswitch.rep
//! [simpleSwitch_clientrep_example1]

//! [simpleSwitch_clientremotenode_example1]
QRemoteObjectNode repNode; // create remote object node
repNode.connect(); // connect with remote host node
 //! [simpleSwitch_clientremotenode_example1]

 //! [simpleSwitch_clientacquirereplica_example1]
QSharedPointer<SimpleSwitchReplica> ptr;
ptr.reset(repNode.acquire<SimpleSwitchReplica>()); // acquire replica of source from host node
//! [simpleSwitch_clientacquirereplica_example1]

//! [simpleSwitch_clientheader_example1]
#ifndef _CLIENT_H
#define _CLIENT_H

#include <QObject>
#include <QSharedPointer>

#include "rep_SimpleSwitch_replica.h"

class Client : public QObject
{
    Q_OBJECT
public:
    Client(QSharedPointer<SimpleSwitchReplica> ptr);
    ~Client();
    void initConnections();// Function to connect signals and slots of source and client

Q_SIGNALS:
    void echoSwitchState(bool switchState);// this signal is connected with server_slot(..) on the source object and echoes back switch state received from source

public Q_SLOTS:
    void recSwitchState_slot(); // slot to receive source state
private:
    bool clientSwitchState; // holds received server switch state
    QSharedPointer<SimpleSwitchReplica> reptr;// holds reference to replica

 };

#endif
//! [simpleSwitch_clientheader_example1]

//! [simpleSwitch_clientcpp_example1]
#include "client.h"

// constructor
Client::Client(QSharedPointer<SimpleSwitchReplica> ptr) :
    QObject(nullptr),reptr(ptr)
{
    initConnections();
    //We can connect to SimpleSwitchReplica Signals/Slots
    //directly because our Replica was generated by repc.
}

//destructor
Client::~Client()
{
}

void Client::initConnections()
{
        // initialize connections between signals and slots

       // connect source replica signal currStateChanged() with client's recSwitchState() slot to receive source's current state
        QObject::connect(reptr.data(), SIGNAL(currStateChanged()), this, SLOT(recSwitchState_slot()));
       // connect client's echoSwitchState(..) signal with replica's server_slot(..) to echo back received state
        QObject::connect(this, SIGNAL(echoSwitchState(bool)),reptr.data(), SLOT(server_slot(bool)));
}

void Client::recSwitchState_slot()
{
    qDebug() << "Received source state "<<reptr.data()->currState();
    clientSwitchState = reptr.data()->currState();
    Q_EMIT echoSwitchState(clientSwitchState); // Emit signal to echo received state back to server
}
//! [simpleSwitch_clientcpp_example1]

//! [simpleSwitch_clientmain_example1]
#include <QCoreApplication>
#include "client.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);


    QSharedPointer<SimpleSwitchReplica> ptr; // shared pointer to hold source replica

    QRemoteObjectNode repNode; // create remote object node
    repNode.connect(); // connect with remote host node

    ptr.reset(repNode.acquire<SimpleSwitchReplica>()); // acquire replica of source from host node

    Client rswitch(ptr); // create client switch object and pass reference of replica to it

    return a.exec();
}
//! [simpleSwitch_clientmain_example1]

//! [simpleSwitch_dynamicclientnode_example2]
QRemoteObjectNode repNode; // create remote object node
repNode.connect(); // connect with remote host node
//! [simpleSwitch_dynamicclientnode_example2]

//! [simpleSwitch_dynamicclientacquirereplica_example2]
QSharedPointer<QRemoteObjectDynamicReplica> ptr; // shared pointer to hold replica
ptr.reset(repNode.acquire("SimpleSwitch")); // acquire replica of source from host node
//! [simpleSwitch_dynamicclientacquirereplica_example2]

//! [simpleSwitch_dynamicclientheader_example2]
#ifndef _DYNAMICCLIENT_H
#define _DYNAMICCLIENT_H

#include <QObject>
#include <QSharedPointer>

#include <QRemoteObjectNode>
#include <qremoteobjectdynamicreplica.h>

class DynamicClient : public QObject
{
    Q_OBJECT
public:
    DynamicClient(QSharedPointer<QRemoteObjectDynamicReplica> ptr);
    ~DynamicClient();

Q_SIGNALS:
    void echoSwitchState(bool switchState);// this signal is connected with server_slot(..) slot of source object and echoes back switch state received from source

public Q_SLOTS:
    void recSwitchState_slot(); // Slot to receive source state
    void initConnection_slot(); //Slot to connect signals/slot on replica initialization

private:
    bool clientSwitchState; // holds received server switch state
    QSharedPointer<QRemoteObjectDynamicReplica> reptr;// holds reference to replica
 };

#endif
//! [simpleSwitch_dynamicclientheader_example2]

//! [simpleSwitch_dynamicclientcpp_example2]
#include "dynamicclient.h"

// constructor
DynamicClient::DynamicClient(QSharedPointer<QRemoteObjectDynamicReplica> ptr) :
    QObject(nullptr), reptr(ptr)
{

    //connect signal for replica valid changed with signal slot initialization
    QObject::connect(reptr.data(), SIGNAL(initialized()), this, SLOT(initConnection_slot()));
}

//destructor
DynamicClient::~DynamicClient()
{
}

// Function to initialize connections between slots and signals
void DynamicClient::initConnection_slot()
{

    // connect source replica signal currStateChanged() with client's recSwitchState() slot to receive source's current state
   QObject::connect(reptr.data(), SIGNAL(currStateChanged()), this, SLOT(recSwitchState_slot()));
   // connect client's echoSwitchState(..) signal with replica's server_slot(..) to echo back received state
   QObject::connect(this, SIGNAL(echoSwitchState(bool)),reptr.data(), SLOT(server_slot(bool)));
}

void DynamicClient::recSwitchState_slot()
{
   clientSwitchState = reptr->property("currState").toBool(); // use replica property to get currState from source
   qDebug() << "Received source state " << clientSwitchState;
   Q_EMIT echoSwitchState(clientSwitchState); // Emit signal to echo received state back to server
}

//! [simpleSwitch_dynamicclientcpp_example2]

//! [simpleSwitch_dynamicclientmaincpp_example2]
#include <QCoreApplication>

#include "dynamicclient.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QSharedPointer<QRemoteObjectDynamicReplica> ptr; // shared pointer to hold replica

    QRemoteObjectNode repNode; // create remote object node
    repNode.connect(); // connect with remote host node

    ptr.reset(repNode.acquire("SimpleSwitch")); // acquire replica of source from host node

    dynamicclient rswitch(ptr); // create client switch object and pass replica reference to it

    return a.exec();
}
//! [simpleSwitch_dynamicclientmaincpp_example2]

//! [simpleSwitch_registrymaincpp_example3]
#include <QCoreApplication>
#include "simpleswitch.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    SimpleSwitch srcSwitch; // create SimpleSwitch

    QRemoteObjectNode regNode = QRemoteObjectNode::createRegistryHostNode(); // create node that hosts registy
    QRemoteObjectNode srcNode = QRemoteObjectNode::createHostNodeConnectedToRegistry(); // create node that will host source and connect to registry
    //Note, you can add srcSwitch directly to regNode if desired.
    //We use two Nodes here, as the regNode could easily be in a third process.

    srcNode.enableRemoting(&srcSwitch); // enable remoting of source object

    return a.exec();
}
//! [simpleSwitch_registrymaincpp_example3]

//! [simpleSwitch_registrydynamicclientmaincpp_example3]
    QRemoteObjectNode repNode = QRemoteObjectNode::createNodeConnectedToRegistry();
//! [simpleSwitch_registrydynamicclientmaincpp_example3]
