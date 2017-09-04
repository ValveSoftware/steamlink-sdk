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

#ifndef QWEBENGINEHTTPREQUEST_H
#define QWEBENGINEHTTPREQUEST_H

#include <QtWebEngineCore/qtwebenginecoreglobal.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qvector.h>
#include <QtCore/qmap.h>
#include <QtCore/qstring.h>
#include <QtCore/qurl.h>

QT_BEGIN_NAMESPACE


class QWebEngineHttpRequestPrivate;

class QWEBENGINE_EXPORT QWebEngineHttpRequest
{
public:
    enum Method {
        Get,
        Post
    };

    explicit QWebEngineHttpRequest(const QUrl &url = QUrl(),
                          const QWebEngineHttpRequest::Method &method = QWebEngineHttpRequest::Get);
    QWebEngineHttpRequest(const QWebEngineHttpRequest &other);
    ~QWebEngineHttpRequest();
#ifdef Q_COMPILER_RVALUE_REFS
    QWebEngineHttpRequest &operator=(QWebEngineHttpRequest &&other) Q_DECL_NOTHROW { swap(other);
                                                                                     return *this; }
#endif
    QWebEngineHttpRequest &operator=(const QWebEngineHttpRequest &other);

    static QWebEngineHttpRequest postRequest(const QUrl &url,
                                             const QMap<QString, QString> &postData);
    void swap(QWebEngineHttpRequest &other) Q_DECL_NOTHROW { qSwap(d, other.d); }

    bool operator==(const QWebEngineHttpRequest &other) const;
    inline bool operator!=(const QWebEngineHttpRequest &other) const
    { return !operator==(other); }

    Method method() const;
    void setMethod(QWebEngineHttpRequest::Method method);

    QUrl url() const;
    void setUrl(const QUrl &url);

    QByteArray postData() const;
    void setPostData(const QByteArray &postData);

    bool hasHeader(const QByteArray &headerName) const;
    QVector<QByteArray> headers() const;
    QByteArray header(const QByteArray &headerName) const;
    void setHeader(const QByteArray &headerName, const QByteArray &value);
    void unsetHeader(const QByteArray &headerName);

private:
    QSharedDataPointer<QWebEngineHttpRequestPrivate> d;
    friend class QWebEngineHttpRequestPrivate;
};

Q_DECLARE_SHARED(QWebEngineHttpRequest)

QT_END_NAMESPACE

#endif
