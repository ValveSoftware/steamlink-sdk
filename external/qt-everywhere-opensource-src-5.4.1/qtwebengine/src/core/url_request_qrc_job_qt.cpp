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

#include "url_request_qrc_job_qt.h"

#include "type_conversion.h"

#include "net/base/net_errors.h"
#include "net/base/io_buffer.h"

#include <QUrl>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

using namespace net;

URLRequestQrcJobQt::URLRequestQrcJobQt(URLRequest *request, NetworkDelegate *networkDelegate)
    : URLRequestJob(request, networkDelegate)
    , m_remainingBytes(0)
    , m_weakFactory(this)
{
}

URLRequestQrcJobQt::~URLRequestQrcJobQt()
{
    if (m_file.isOpen())
        m_file.close();
}

void URLRequestQrcJobQt::Start()
{
    base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(&URLRequestQrcJobQt::startGetHead, m_weakFactory.GetWeakPtr()));
}

void URLRequestQrcJobQt::Kill()
{
    if (m_file.isOpen())
        m_file.close();
    m_weakFactory.InvalidateWeakPtrs();

    URLRequestJob::Kill();
}

bool URLRequestQrcJobQt::GetMimeType(std::string *mimeType) const
{
    DCHECK(request_);
    if (m_mimeType.size() > 0) {
        *mimeType = m_mimeType;
        return true;
    }
    return false;
}

bool URLRequestQrcJobQt::ReadRawData(IOBuffer *buf, int bufSize, int *bytesRead)
{
    DCHECK(bytesRead);
    DCHECK_GE(m_remainingBytes, 0);
    // File has been read finished.
    if (!m_remainingBytes || !bufSize) {
        *bytesRead = 0;
        return true;
    }
    if (m_remainingBytes < bufSize)
        bufSize = static_cast<int>(m_remainingBytes);
    qint64 rv = m_file.read(buf->data(), bufSize);
    if (rv >= 0) {
        *bytesRead = static_cast<int>(rv);
        m_remainingBytes -= rv;
        DCHECK_GE(m_remainingBytes, 0);
        return true;
    } else {
        NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, ERR_FAILED));
    }
    return false;
}

void URLRequestQrcJobQt::startGetHead()
{
    // Get qrc file path.
    QString qrcFilePath = ':' + toQt(request_->url()).path(QUrl::RemovePath | QUrl::RemoveQuery);
    m_file.setFileName(qrcFilePath);
    QFileInfo qrcFileInfo(m_file);
    // Get qrc file mime type.
    QMimeDatabase mimeDatabase;
    QMimeType mimeType = mimeDatabase.mimeTypeForFile(qrcFileInfo);
    m_mimeType = mimeType.name().toStdString();
    // Open file
    if (m_file.open(QIODevice::ReadOnly)) {
        m_remainingBytes = m_file.size();
        // Notify that the headers are complete
        NotifyHeadersComplete();
    } else {
        NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, ERR_INVALID_URL));
    }
}
