/*
 * Copyright 2011 Intel Corporation.
 * Copyright © 2012, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "components.h"

#include <networkservice.h>
#include <clockmodel.h>
#include "networkingmodel.h"
#include "technologymodel.h"
#include "savedservicemodel.h"
#include "useragent.h"
#include "networksession.h"
#include "counter.h"

void Components::registerTypes(const char *uri)
{
    // @uri MeeGo.Connman

    qmlRegisterType<NetworkService>(uri,0,2,"NetworkService");
    qmlRegisterType<NetworkingModel>(uri,0,2,"NetworkingModel");
    qmlRegisterType<TechnologyModel>(uri,0,2,"TechnologyModel");
    qmlRegisterType<SavedServiceModel>(uri,0,2,"SavedServiceModel");
    qmlRegisterType<UserAgent>(uri,0,2,"UserAgent");
    qmlRegisterType<ClockModel>(uri,0,2,"ClockModel");
    qmlRegisterType<NetworkSession>(uri,0,2,"NetworkSession");
    qmlRegisterType<NetworkManager>(uri,0,2,"NetworkManager");
    qmlRegisterType<NetworkManagerFactory>(uri,0,2,"NetworkManagerFactory");
    qmlRegisterType<NetworkTechnology>(uri,0,2,"NetworkTechnology");
    qmlRegisterType<Counter>(uri,0,2,"NetworkCounter");
}

void Components::initializeEngine(QDeclarativeEngine *engine, const char *uri)
{
    Q_UNUSED(uri);
    Q_UNUSED(engine);
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_EXPORT_PLUGIN(Components)
#endif
