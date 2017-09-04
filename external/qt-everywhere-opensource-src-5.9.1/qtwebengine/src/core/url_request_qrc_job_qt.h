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

#ifndef URL_REQUEST_QRC_JOB_QT_H_
#define URL_REQUEST_QRC_JOB_QT_H_

#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

#include <QFile>

namespace QtWebEngineCore {

// A request job that handles reading qrc file URLs
class URLRequestQrcJobQt : public net::URLRequestJob {

public:
    URLRequestQrcJobQt(net::URLRequest *request, net::NetworkDelegate *networkDelegate);
    void Start() override;
    void Kill() override;
    int ReadRawData(net::IOBuffer* buf, int buf_size)  override;;
    bool GetMimeType(std::string *mimeType) const override;

protected:
    virtual ~URLRequestQrcJobQt();
    // Get file mime type and try open file on a background thread.
    void startGetHead();

private:
    qint64 m_remainingBytes;
    QFile m_file;
    std::string m_mimeType;
    base::WeakPtrFactory<URLRequestQrcJobQt> m_weakFactory;

    DISALLOW_COPY_AND_ASSIGN(URLRequestQrcJobQt);
};

} // namespace QtWebEngineCore

#endif // URL_REQUEST_QRC_JOB_QT_H_
