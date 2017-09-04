/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "QtMultimedia/private/qtmultimediaglobal_p.h"
#include "camerabinresourcepolicy.h"
//#define DEBUG_RESOURCE_POLICY
#include <QtCore/qdebug.h>
#include <QtCore/qset.h>

#if QT_CONFIG(resourcepolicy)
#include <policy/resource.h>
#include <policy/resources.h>
#include <policy/resource-set.h>
#endif

QT_BEGIN_NAMESPACE

CamerabinResourcePolicy::CamerabinResourcePolicy(QObject *parent) :
    QObject(parent),
    m_resourceSet(NoResources),
    m_releasingResources(false),
    m_canCapture(false)
{
#if QT_CONFIG(resourcepolicy)
    //loaded resource set is also kept requested for image and video capture sets
    m_resource = new ResourcePolicy::ResourceSet("camera");
    m_resource->setAlwaysReply();
    m_resource->initAndConnect();

    connect(m_resource, SIGNAL(resourcesGranted(QList<ResourcePolicy::ResourceType>)),
            SLOT(handleResourcesGranted()));
    connect(m_resource, SIGNAL(resourcesDenied()), SIGNAL(resourcesDenied()));
    connect(m_resource, SIGNAL(lostResources()), SLOT(handleResourcesLost()));
    connect(m_resource, SIGNAL(resourcesReleased()), SLOT(handleResourcesReleased()));
    connect(m_resource, SIGNAL(resourcesBecameAvailable(QList<ResourcePolicy::ResourceType>)),
            this, SLOT(resourcesAvailable()));
    connect(m_resource, SIGNAL(updateOK()), this, SLOT(updateCanCapture()));
#endif
}

CamerabinResourcePolicy::~CamerabinResourcePolicy()
{
#if QT_CONFIG(resourcepolicy)
    //ensure the resources are released
    if (m_resourceSet != NoResources)
        setResourceSet(NoResources);

    //don't delete the resource set until resources are released
    if (m_releasingResources) {
        m_resource->connect(m_resource, SIGNAL(resourcesReleased()),
                            SLOT(deleteLater()));
    } else {
        delete m_resource;
        m_resource = 0;
    }
#endif
}

CamerabinResourcePolicy::ResourceSet CamerabinResourcePolicy::resourceSet() const
{
    return m_resourceSet;
}

void CamerabinResourcePolicy::setResourceSet(CamerabinResourcePolicy::ResourceSet set)
{
    CamerabinResourcePolicy::ResourceSet oldSet = m_resourceSet;
    m_resourceSet = set;

#ifdef DEBUG_RESOURCE_POLICY
    qDebug() << Q_FUNC_INFO << set;
#endif

#if QT_CONFIG(resourcepolicy)
    QSet<ResourcePolicy::ResourceType> requestedTypes;

    switch (set) {
    case NoResources:
        break;
    case LoadedResources:
        requestedTypes << ResourcePolicy::LensCoverType //to detect lens cover is opened/closed
                       << ResourcePolicy::VideoRecorderType; //to open camera device
        break;
    case ImageCaptureResources:
        requestedTypes << ResourcePolicy::LensCoverType
                       << ResourcePolicy::VideoPlaybackType
                       << ResourcePolicy::VideoRecorderType
                       << ResourcePolicy::LedsType;
        break;
    case VideoCaptureResources:
        requestedTypes << ResourcePolicy::LensCoverType
                       << ResourcePolicy::VideoPlaybackType
                       << ResourcePolicy::VideoRecorderType
                       << ResourcePolicy::AudioPlaybackType
                       << ResourcePolicy::AudioRecorderType
                       << ResourcePolicy::LedsType;
        break;
    }

    QSet<ResourcePolicy::ResourceType> currentTypes;
    const auto resources = m_resource->resources();
    currentTypes.reserve(resources.size());
    for (ResourcePolicy::Resource *resource : resources)
        currentTypes << resource->type();

    const auto diffCurrentWithRequested = currentTypes - requestedTypes;
    for (ResourcePolicy::ResourceType resourceType : diffCurrentWithRequested)
        m_resource->deleteResource(resourceType);

    const auto diffRequestedWithCurrent = requestedTypes - currentTypes;
    for (ResourcePolicy::ResourceType resourceType : diffRequestedWithCurrent) {
        if (resourceType == ResourcePolicy::LensCoverType) {
            ResourcePolicy::LensCoverResource *lensCoverResource = new ResourcePolicy::LensCoverResource;
            lensCoverResource->setOptional(true);
            m_resource->addResourceObject(lensCoverResource);
        } else if (resourceType == ResourcePolicy::AudioPlaybackType) {
            ResourcePolicy::Resource *resource = new ResourcePolicy::AudioResource;
            resource->setOptional(true);
            m_resource->addResourceObject(resource);
        } else if (resourceType == ResourcePolicy::AudioRecorderType) {
            ResourcePolicy::Resource *resource = new ResourcePolicy::AudioRecorderResource;
            resource->setOptional(true);
            m_resource->addResourceObject(resource);
        } else {
            m_resource->addResource(resourceType);
        }
    }

    m_resource->update();
    if (set != NoResources) {
        m_resource->acquire();
    } else {
        if (oldSet != NoResources) {
            m_releasingResources = true;
            m_resource->release();
        }
    }
#else
    Q_UNUSED(oldSet);
    updateCanCapture();
#endif
}

bool CamerabinResourcePolicy::isResourcesGranted() const
{
#if QT_CONFIG(resourcepolicy)
    const auto resources = m_resource->resources();
    for (ResourcePolicy::Resource *resource : resources)
        if (!resource->isOptional() && !resource->isGranted())
            return false;
#endif
    return true;
}

void CamerabinResourcePolicy::handleResourcesLost()
{
    updateCanCapture();
    emit resourcesLost();
}

void CamerabinResourcePolicy::handleResourcesGranted()
{
    updateCanCapture();
    emit resourcesGranted();
}

void CamerabinResourcePolicy::handleResourcesReleased()
{
#if QT_CONFIG(resourcepolicy)
#ifdef DEBUG_RESOURCE_POLICY
    qDebug() << Q_FUNC_INFO;
#endif
    m_releasingResources = false;
#endif
    updateCanCapture();
}

void CamerabinResourcePolicy::resourcesAvailable()
{
#if QT_CONFIG(resourcepolicy)
    if (m_resourceSet != NoResources) {
        m_resource->acquire();
    }
#endif
}

bool CamerabinResourcePolicy::canCapture() const
{
    return m_canCapture;
}

void CamerabinResourcePolicy::updateCanCapture()
{
    const bool wasAbleToRecord = m_canCapture;
    m_canCapture = (m_resourceSet == VideoCaptureResources) || (m_resourceSet == ImageCaptureResources);
#if QT_CONFIG(resourcepolicy)
    const auto resources = m_resource->resources();
    for (ResourcePolicy::Resource *resource : resources) {
        if (resource->type() != ResourcePolicy::LensCoverType)
            m_canCapture = m_canCapture && resource->isGranted();
    }
#endif
    if (wasAbleToRecord != m_canCapture)
        emit canCaptureChanged();
}

QT_END_NAMESPACE
