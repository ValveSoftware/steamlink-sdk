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

#include "url_request_custom_job.h"
#include "url_request_custom_job_delegate.h"

#include "type_conversion.h"
#include "net/base/net_errors.h"

#include <QByteArray>

namespace QtWebEngineCore {

URLRequestCustomJobDelegate::URLRequestCustomJobDelegate(URLRequestCustomJobShared *shared)
    : m_shared(shared)
{
}

URLRequestCustomJobDelegate::~URLRequestCustomJobDelegate()
{
    m_shared->unsetJobDelegate();
}

QUrl URLRequestCustomJobDelegate::url() const
{
    return toQt(m_shared->requestUrl());
}

QByteArray URLRequestCustomJobDelegate::method() const
{
    return QByteArray::fromStdString(m_shared->requestMethod());
}

void URLRequestCustomJobDelegate::setReply(const QByteArray &contentType, QIODevice *device)
{
    m_shared->setReplyMimeType(contentType.toStdString());
    m_shared->setReplyDevice(device);
}

void URLRequestCustomJobDelegate::abort()
{
    m_shared->abort();
}

void URLRequestCustomJobDelegate::redirect(const QUrl &url)
{
    m_shared->redirect(toGurl(url));
}

void URLRequestCustomJobDelegate::fail(Error error)
{
    int net_error =  0;
    switch (error) {
    case NoError:
        break;
    case UrlInvalid:
        net_error = net::ERR_INVALID_URL;
        break;
    case UrlNotFound:
        net_error = net::ERR_FILE_NOT_FOUND;
        break;
    case RequestAborted:
        net_error = net::ERR_ABORTED;
        break;
    case RequestDenied:
        net_error = net::ERR_ACCESS_DENIED;
        break;
    case RequestFailed:
        net_error = net::ERR_FAILED;
        break;
    }
    if (net_error)
        m_shared->fail(net_error);
}

} // namespace
