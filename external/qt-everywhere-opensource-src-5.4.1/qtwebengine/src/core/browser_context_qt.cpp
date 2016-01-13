/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "browser_context_qt.h"

#include "type_conversion.h"
#include "qtwebenginecoreglobal.h"
#include "resource_context_qt.h"
#include "url_request_context_getter_qt.h"

#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/proxy/proxy_config_service.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <QStringBuilder>

BrowserContextQt::BrowserContextQt()
{
    resourceContext.reset(new ResourceContextQt(this));
}

BrowserContextQt::~BrowserContextQt()
{
    if (resourceContext)
        content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE, resourceContext.release());
}

base::FilePath BrowserContextQt::GetPath() const
{
    QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    if (dataLocation.isEmpty())
        dataLocation = QDir::homePath() % QDir::separator() % QChar::fromLatin1('.') % QCoreApplication::applicationName();

    dataLocation.append(QDir::separator() % QLatin1String("QtWebEngine"));
    dataLocation.append(QDir::separator() % QLatin1String("Default"));
    return base::FilePath(toFilePathString(dataLocation));
}

base::FilePath BrowserContextQt::GetCachePath() const
{
    QString cacheLocation = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheLocation.isEmpty())
        cacheLocation = QDir::homePath() % QDir::separator() % QChar::fromLatin1('.') % QCoreApplication::applicationName();

    cacheLocation.append(QDir::separator() % QLatin1String("QtWebEngine"));
    cacheLocation.append(QDir::separator() % QLatin1String("Default"));
    return base::FilePath(toFilePathString(cacheLocation));
}

bool BrowserContextQt::IsOffTheRecord() const
{
    return false;
}

net::URLRequestContextGetter *BrowserContextQt::GetRequestContext()
{
    return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter *BrowserContextQt::GetRequestContextForRenderProcess(int)
{
    return GetRequestContext();
}

net::URLRequestContextGetter *BrowserContextQt::GetMediaRequestContext()
{
    return GetRequestContext();
}

net::URLRequestContextGetter *BrowserContextQt::GetMediaRequestContextForRenderProcess(int)
{
    return GetRequestContext();
}

net::URLRequestContextGetter *BrowserContextQt::GetMediaRequestContextForStoragePartition(const base::FilePath&, bool)
{
    return GetRequestContext();
}

content::ResourceContext *BrowserContextQt::GetResourceContext()
{
    return resourceContext.get();
}

content::DownloadManagerDelegate *BrowserContextQt::GetDownloadManagerDelegate()
{
    return downloadManagerDelegate.get();
}

content::BrowserPluginGuestManager *BrowserContextQt::GetGuestManager()
{
    return 0;
}

quota::SpecialStoragePolicy *BrowserContextQt::GetSpecialStoragePolicy()
{
    QT_NOT_YET_IMPLEMENTED
    return 0;
}

content::PushMessagingService *BrowserContextQt::GetPushMessagingService()
{
    return 0;
}

net::URLRequestContextGetter *BrowserContextQt::CreateRequestContext(content::ProtocolHandlerMap *protocol_handlers)
{
    url_request_getter_ = new URLRequestContextGetterQt(GetPath(), GetCachePath(), protocol_handlers);
    static_cast<ResourceContextQt*>(resourceContext.get())->set_url_request_context_getter(url_request_getter_.get());
    return url_request_getter_.get();
}
