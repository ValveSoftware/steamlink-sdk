/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qservicelocator_p.h"
#include "qabstractserviceprovider_p.h"
#include "nullservices_p.h"
#include "qtickclockservice_p.h"
#include "qeventfilterservice_p.h"
#include <QHash>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

/* !\internal
    \class Qt3DCore::QAbstractServiceProvider
    \inmodule Qt3DCore
*/

QAbstractServiceProvider::QAbstractServiceProvider(int type, const QString &description)
    : d_ptr(new QAbstractServiceProviderPrivate(type, description))
{
    d_ptr->q_ptr = this;
}

/* \internal */
QAbstractServiceProvider::QAbstractServiceProvider(QAbstractServiceProviderPrivate &dd)
    : d_ptr(&dd)
{
    d_ptr->q_ptr = this;
}

QAbstractServiceProvider::~QAbstractServiceProvider()
{
}

int QAbstractServiceProvider::type() const
{
    Q_D(const QAbstractServiceProvider);
    return d->m_type;
}

QString QAbstractServiceProvider::description() const
{
    Q_D(const QAbstractServiceProvider);
    return d->m_description;
}


class QServiceLocatorPrivate
{
public:
    QServiceLocatorPrivate()
        : m_nonNullDefaultServices(0)
    {}

    QHash<int, QAbstractServiceProvider *> m_services;

    NullSystemInformationService m_nullSystemInfo;
    NullOpenGLInformationService m_nullOpenGLInfo;
    QTickClockService m_defaultFrameAdvanceService;
    QEventFilterService m_eventFilterService;
    int m_nonNullDefaultServices;
};


/* !\internal
    \class Qt3DCore::QServiceLocator
    \inmodule Qt3DCore
    \brief Service locator used by aspects to retrieve pointers to concrete service objects

    The Qt3DCore::QServiceLocator class can be used by aspects to obtain pointers to concrete
    providers of abstract service interfaces. A subclass of Qt3DCore::QAbstractServiceProvider
    encapsulates a service that can be provided by an aspect for other parts of the system.
    For example, an aspect may wish to know the current frame number, or how many CPU cores
    are available in the Qt3D tasking threadpool.

    Aspects or the Qt3DCore::QAspectEngine are able to register objects as providers of services.
    The service locator itself can be accessed via the Qt3DCore::QAbstractAspect::services()
    function.

    As a convenience, the service locator provides methods to access services provided by
    built in Qt3D aspects. Currently these are Qt3DCore::QSystemInformationService and
    Qt3DCore::QOpenGLInformationService. For such services, the service provider will never
    return a null pointer. The default implementations of these services are simple null or
    do nothing implementations.
*/

/*
    Creates an instance of QServiceLocator.
*/
QServiceLocator::QServiceLocator()
    : d_ptr(new QServiceLocatorPrivate)
{
}

/*
   Destroys a QServiceLocator object
*/
QServiceLocator::~QServiceLocator()
{
}

/*
    Registers \a provider service provider for the service \a serviceType. This replaces any
    existing provider for this service. The service provider does not take ownership
    of the provider.

    \sa unregisterServiceProvider(), serviceCount(), service()
*/
void QServiceLocator::registerServiceProvider(int serviceType, QAbstractServiceProvider *provider)
{
    Q_D(QServiceLocator);
    d->m_services.insert(serviceType, provider);
    if (serviceType < DefaultServiceCount)
        ++(d->m_nonNullDefaultServices);
}

/*
    Unregisters any existing provider for the \a serviceType.

    \sa registerServiceProvider()
 */
void QServiceLocator::unregisterServiceProvider(int serviceType)
{
    Q_D(QServiceLocator);
    int removedCount = d->m_services.remove(serviceType);
    if (serviceType < DefaultServiceCount)
        d->m_nonNullDefaultServices -= removedCount;
}

/*
    Returns the number of registered services.
 */
int QServiceLocator::serviceCount() const
{
    Q_D(const QServiceLocator);
    return DefaultServiceCount + d->m_services.size() - d->m_nonNullDefaultServices;
}

/*
    \fn T *Qt3DCore::QServiceLocator::service(int serviceType)

    Returns a pointer to the service provider for \a serviceType. If no provider
    has been explicitly registered, this returns a null pointer for non-Qt3D provided
    default services and a null pointer for non-default services.

    \sa registerServiceProvider()

*/

/*
    Returns a pointer to a provider for the system information service. If no provider
    has been explicitly registered for this service type, then a pointer to a null, do-
    nothing service is returned.
*/
QSystemInformationService *QServiceLocator::systemInformation()
{
    Q_D(QServiceLocator);
    return static_cast<QSystemInformationService *>(d->m_services.value(SystemInformation, &d->m_nullSystemInfo));
}

/*
    Returns a pointer to a provider for the OpenGL information service. If no provider
    has been explicitly registered for this service type, then a pointer to a null, do-
    nothing service is returned.
*/
QOpenGLInformationService *QServiceLocator::openGLInformation()
{
    Q_D(QServiceLocator);
    return static_cast<QOpenGLInformationService *>(d->m_services.value(OpenGLInformation, &d->m_nullOpenGLInfo));
}

/*
    Returns a pointer to a provider for the frame advance service. If no provider
    has been explicitly registered for this service type, then a pointer to a simple timer-based
    service is returned.
*/
QAbstractFrameAdvanceService *QServiceLocator::frameAdvanceService()
{
    Q_D(QServiceLocator);
    return static_cast<QAbstractFrameAdvanceService *>(d->m_services.value(FrameAdvanceService, &d->m_defaultFrameAdvanceService));
}

/*
    Returns a pointer to a provider for the event filter service. If no
    provider has been explicitly registered for this service type, then a
    pointer to the default event filter service is returned.
 */
QEventFilterService *QServiceLocator::eventFilterService()
{
    Q_D(QServiceLocator);
    return static_cast<QEventFilterService *>(d->m_services.value(EventFilterService, &d->m_eventFilterService));
}

/*
    \internal
*/
QAbstractServiceProvider *QServiceLocator::_q_getServiceHelper(int type)
{
    Q_D(QServiceLocator);
    switch (type) {
    case SystemInformation:
        return systemInformation();
    case OpenGLInformation:
        return openGLInformation();
    case FrameAdvanceService:
        return frameAdvanceService();
    case EventFilterService:
        return eventFilterService();
    default:
        return d->m_services.value(type, nullptr);
    }
}

}

QT_END_NAMESPACE
