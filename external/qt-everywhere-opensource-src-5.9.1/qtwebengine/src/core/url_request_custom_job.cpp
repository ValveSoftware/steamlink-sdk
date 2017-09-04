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

#include "api/qwebengineurlrequestjob.h"
#include "api/qwebengineurlschemehandler.h"
#include "browser_context_adapter.h"
#include "type_conversion.h"

#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/base/io_buffer.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QUrl>

using namespace net;

namespace QtWebEngineCore {

URLRequestCustomJob::URLRequestCustomJob(URLRequest *request, NetworkDelegate *networkDelegate,
                                         const std::string &scheme, QWeakPointer<const BrowserContextAdapter> adapter)
    : URLRequestJob(request, networkDelegate)
    , m_scheme(scheme)
    , m_adapter(adapter)
    , m_shared(new URLRequestCustomJobShared(this))
{
}

URLRequestCustomJob::~URLRequestCustomJob()
{
    if (m_shared)
        m_shared->killJob();
}

static void startAsync(URLRequestCustomJobShared *shared)
{
    shared->startAsync();
}

void URLRequestCustomJob::Start()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE, base::Bind(&startAsync, m_shared));
}

void URLRequestCustomJob::Kill()
{
    if (m_shared)
        m_shared->killJob();
    m_shared = 0;

    URLRequestJob::Kill();
}

bool URLRequestCustomJob::GetMimeType(std::string *mimeType) const
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if (!m_shared)
        return false;
    QMutexLocker lock(&m_shared->m_mutex);
    if (m_shared->m_mimeType.size() > 0) {
        *mimeType = m_shared->m_mimeType;
        return true;
    }
    return false;
}

bool URLRequestCustomJob::GetCharset(std::string* charset)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if (!m_shared)
        return false;
    QMutexLocker lock(&m_shared->m_mutex);
    if (m_shared->m_charset.size() > 0) {
        *charset = m_shared->m_charset;
        return true;
    }
    return false;
}

bool URLRequestCustomJob::IsRedirectResponse(GURL* location, int* http_status_code)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if (!m_shared)
        return false;
    QMutexLocker lock(&m_shared->m_mutex);
    if (m_shared->m_redirect.is_valid()) {
        *location = m_shared->m_redirect;
        *http_status_code = 303;
        return true;
    }
    return false;
}

int URLRequestCustomJob::ReadRawData(IOBuffer *buf, int bufSize)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    Q_ASSERT(m_shared);
    QMutexLocker lock(&m_shared->m_mutex);
    if (m_shared->m_error)
        return m_shared->m_error;
    qint64 rv = m_shared->m_device ? m_shared->m_device->read(buf->data(), bufSize) : -1;
    if (rv >= 0)
        return static_cast<int>(rv);
    else {
        // QIODevice::read might have called fail on us.
        if (m_shared->m_error)
            return m_shared->m_error;
        return ERR_FAILED;
    }
}


URLRequestCustomJobShared::URLRequestCustomJobShared(URLRequestCustomJob *job)
    : m_mutex(QMutex::Recursive)
    , m_job(job)
    , m_delegate(0)
    , m_error(0)
    , m_started(false)
    , m_asyncInitialized(false)
    , m_weakFactory(this)
{
}

URLRequestCustomJobShared::~URLRequestCustomJobShared()
{
    Q_ASSERT(!m_job);
    Q_ASSERT(!m_delegate);
}

void URLRequestCustomJobShared::killJob()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    QMutexLocker lock(&m_mutex);
    m_job = 0;
    bool doDelete = false;
    if (m_delegate) {
        m_delegate->deleteLater();
    } else {
        // Do not delete yet if startAsync has not yet run.
        doDelete = m_asyncInitialized;
    }
    if (m_device && m_device->isOpen())
        m_device->close();
    m_device = 0;
    m_weakFactory.InvalidateWeakPtrs();
    lock.unlock();
    if (doDelete)
        delete this;
}

void URLRequestCustomJobShared::unsetJobDelegate()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    QMutexLocker lock(&m_mutex);
    m_delegate = 0;
    bool doDelete = false;
    if (m_job)
        abort();
    else
        doDelete = true;
    lock.unlock();
    if (doDelete)
        delete this;
}

void URLRequestCustomJobShared::setReplyMimeType(const std::string &mimeType)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    QMutexLocker lock(&m_mutex);
    m_mimeType = mimeType;
}

void URLRequestCustomJobShared::setReplyCharset(const std::string &charset)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    QMutexLocker lock(&m_mutex);
    m_charset = charset;
}

void URLRequestCustomJobShared::setReplyDevice(QIODevice *device)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    QMutexLocker lock(&m_mutex);
    if (!m_job)
        return;
    m_device = device;
    if (m_device && !m_device->isReadable())
        m_device->open(QIODevice::ReadOnly);

    qint64 size = m_device ? m_device->size() : -1;
    if (size > 0)
        m_job->set_expected_content_size(size);
    if (m_device && m_device->isReadable())
        content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE, base::Bind(&URLRequestCustomJobShared::notifyStarted, m_weakFactory.GetWeakPtr()));
    else
        fail(ERR_INVALID_URL);
}

void URLRequestCustomJobShared::redirect(const GURL &url)
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    QMutexLocker lock(&m_mutex);
    if (m_device || m_error)
        return;
    if (!m_job)
        return;
    m_redirect = url;
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE, base::Bind(&URLRequestCustomJobShared::notifyStarted, m_weakFactory.GetWeakPtr()));
}

void URLRequestCustomJobShared::abort()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    QMutexLocker lock(&m_mutex);
    if (m_device && m_device->isOpen())
        m_device->close();
    m_device = 0;
    if (!m_job)
        return;
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE, base::Bind(&URLRequestCustomJobShared::notifyCanceled, m_weakFactory.GetWeakPtr()));
}

void URLRequestCustomJobShared::notifyCanceled()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    QMutexLocker lock(&m_mutex);
    if (!m_job)
        return;
    if (m_started)
        m_job->NotifyCanceled();
    else
        m_job->NotifyStartError(URLRequestStatus(URLRequestStatus::CANCELED, ERR_ABORTED));
}

void URLRequestCustomJobShared::notifyStarted()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    QMutexLocker lock(&m_mutex);
    if (!m_job)
        return;
    Q_ASSERT(!m_started);
    m_started = true;
    m_job->NotifyHeadersComplete();
}

void URLRequestCustomJobShared::fail(int error)
{
    QMutexLocker lock(&m_mutex);
    m_error = error;
    if (content::BrowserThread::CurrentlyOn(content::BrowserThread::IO))
        return;
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!m_job)
        return;
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE, base::Bind(&URLRequestCustomJobShared::notifyFailure, m_weakFactory.GetWeakPtr()));
}

void URLRequestCustomJobShared::notifyFailure()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    QMutexLocker lock(&m_mutex);
    if (!m_job)
        return;
    if (m_device)
        m_device->close();
    if (!m_started)
        m_job->NotifyStartError(URLRequestStatus::FromError(m_error));
    // else we fail on the next read, or the read that might already be in progress
}

GURL URLRequestCustomJobShared::requestUrl()
{
    QMutexLocker lock(&m_mutex);
    if (!m_job)
        return GURL();
    return m_job->request()->url();
}

std::string URLRequestCustomJobShared::requestMethod()
{
    QMutexLocker lock(&m_mutex);
    if (!m_job)
        return std::string();
    return m_job->request()->method();
}

void URLRequestCustomJobShared::startAsync()
{
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    Q_ASSERT(!m_started);
    Q_ASSERT(!m_delegate);
    QMutexLocker lock(&m_mutex);
    if (!m_job) {
        lock.unlock();
        delete this;
        return;
    }

    QWebEngineUrlSchemeHandler *schemeHandler = 0;
    QSharedPointer<const BrowserContextAdapter> browserContext = m_job->m_adapter.toStrongRef();
    if (browserContext)
        schemeHandler = browserContext->customUrlSchemeHandlers()[toQByteArray(m_job->m_scheme)];
    if (schemeHandler) {
        m_delegate = new URLRequestCustomJobDelegate(this);
        m_asyncInitialized = true;
        QWebEngineUrlRequestJob *requestJob = new QWebEngineUrlRequestJob(m_delegate);
        schemeHandler->requestStarted(requestJob);
    } else {
        lock.unlock();
        abort();
        delete this;
        return;
    }
}

} // namespace
