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

#ifndef URL_REQUEST_QRC_JOB_QT_H_
#define URL_REQUEST_QRC_JOB_QT_H_

#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

#include <QtCore/qcompilerdetection.h> // Needed for Q_DECL_OVERRIDE
#include <QFile>

// A request job that handles reading qrc file URLs
class URLRequestQrcJobQt : public net::URLRequestJob {

public:
    URLRequestQrcJobQt(net::URLRequest *request, net::NetworkDelegate *networkDelegate);
    virtual void Start() Q_DECL_OVERRIDE;
    virtual void Kill() Q_DECL_OVERRIDE;
    virtual bool ReadRawData(net::IOBuffer *buf, int bufSize, int *bytesRead) Q_DECL_OVERRIDE;
    virtual bool GetMimeType(std::string *mimeType) const Q_DECL_OVERRIDE;

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

#endif // URL_REQUEST_QRC_JOB_QT_H_
