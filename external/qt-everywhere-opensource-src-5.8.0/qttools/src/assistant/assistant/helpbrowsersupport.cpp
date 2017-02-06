/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "helpbrowsersupport.h"
#include "helpenginewrapper.h"
#include "helpviewer.h"
#include "tracer.h"

#include <QtHelp/QHelpEngineCore>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <QtCore/QTimer>
#include <QtCore/QCoreApplication>
#include <QtCore/QUrl>
#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

// -- messages

static const char g_htmlPage[] = "<html><head><meta http-equiv=\"content-type\" content=\"text/html; "
    "charset=UTF-8\"><title>%1</title><style>body{padding: 3em 0em;background: #eeeeee;}"
    "hr{color: lightgray;width: 100%;}img{float: left;opacity: .8;}#box{background: white;border: 1px solid "
    "lightgray;width: 600px;padding: 60px;margin: auto;}h1{font-size: 130%;font-weight: bold;border-bottom: "
    "1px solid lightgray;margin-left: 48px;}h2{font-size: 100%;font-weight: normal;border-bottom: 1px solid "
    "lightgray;margin-left: 48px;}ul{font-size: 80%;padding-left: 48px;margin: 0;}#reloadButton{padding-left:"
    "48px;}</style></head><body><div id=\"box\"><h1>%2</h1><h2>%3</h2><h2><b>%4</b></h2></div></body></html>";

QString HelpBrowserSupport::msgError404()
{
    return QCoreApplication::translate("HelpViewer", "Error 404...");
}

QString HelpBrowserSupport::msgPageNotFound()
{
    return QCoreApplication::translate("HelpViewer", "The page could not be found!");
}

QString HelpBrowserSupport::msgAllDocumentationSets()
{
     return QCoreApplication::translate("HelpViewer",
                                        "Please make sure that you have all "
                                        "documentation sets installed.");
}

QString HelpBrowserSupport::msgLoadError(const QUrl &url)
{
    return HelpViewer::tr("Error loading: %1").arg(url.toString());
}

QString HelpBrowserSupport::msgHtmlErrorPage(const QUrl &url)
{
    return QString::fromLatin1(g_htmlPage)
        .arg(HelpBrowserSupport::msgError404(), HelpBrowserSupport::msgPageNotFound(),
             HelpBrowserSupport::msgLoadError(url), HelpBrowserSupport::msgAllDocumentationSets());
}

// A QNetworkAccessManager implementing unhandled URL schema handling for WebKit-type browsers.

// -- HelpNetworkReply

class HelpNetworkReply : public QNetworkReply
{
public:
    HelpNetworkReply(const QNetworkRequest &request, const QByteArray &fileData,
        const QString &mimeType);

    virtual void abort();

    virtual qint64 bytesAvailable() const
        { return data.length() + QNetworkReply::bytesAvailable(); }

protected:
    virtual qint64 readData(char *data, qint64 maxlen);

private:
    QByteArray data;
    const qint64 origLen;
};

HelpNetworkReply::HelpNetworkReply(const QNetworkRequest &request,
        const QByteArray &fileData, const QString& mimeType)
    : data(fileData), origLen(fileData.length())
{
    TRACE_OBJ
    setRequest(request);
    setUrl(request.url());
    setOpenMode(QIODevice::ReadOnly);

    setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
    setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(origLen));
    QTimer::singleShot(0, this, &QNetworkReply::metaDataChanged);
    QTimer::singleShot(0, this, &QNetworkReply::readyRead);
    QTimer::singleShot(0, this, &QNetworkReply::finished);
}

void HelpNetworkReply::abort()
{
    TRACE_OBJ
}

qint64 HelpNetworkReply::readData(char *buffer, qint64 maxlen)
{
    TRACE_OBJ
    qint64 len = qMin(qint64(data.length()), maxlen);
    if (len) {
        memcpy(buffer, data.constData(), len);
        data.remove(0, len);
    }
    if (!data.length())
        QTimer::singleShot(0, this, &QNetworkReply::finished);
    return len;
}

// -- HelpRedirectNetworkReply

class HelpRedirectNetworkReply : public QNetworkReply
{
public:
    HelpRedirectNetworkReply(const QNetworkRequest &request, const QUrl &newUrl)
    {
        setRequest(request);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 301);
        setAttribute(QNetworkRequest::RedirectionTargetAttribute, newUrl);

        QTimer::singleShot(0, this, &QNetworkReply::finished);
    }

protected:
    void abort() { TRACE_OBJ }
    qint64 readData(char*, qint64) { TRACE_OBJ return qint64(-1); }
};

// -- HelpNetworkAccessManager

class HelpNetworkAccessManager : public QNetworkAccessManager
{
public:
    HelpNetworkAccessManager(QObject *parent);

protected:
    virtual QNetworkReply *createRequest(Operation op,
        const QNetworkRequest &request, QIODevice *outgoingData = 0);
};

HelpNetworkAccessManager::HelpNetworkAccessManager(QObject *parent)
    : QNetworkAccessManager(parent)
{
    TRACE_OBJ
}

QNetworkReply *HelpNetworkAccessManager::createRequest(Operation, const QNetworkRequest &request, QIODevice*)
{
    TRACE_OBJ

    QByteArray data;
    const QUrl url = request.url();
    QUrl redirectedUrl;
    switch (HelpBrowserSupport::resolveUrl(url, &redirectedUrl, &data)) {
    case HelpBrowserSupport::UrlRedirect:
        return new HelpRedirectNetworkReply(request, redirectedUrl);
    case HelpBrowserSupport::UrlLocalData: {
        const QString mimeType = HelpViewer::mimeFromUrl(url);
        return new HelpNetworkReply(request, data, mimeType);
    }
    case HelpBrowserSupport::UrlResolveError:
        break;
    }
    return new HelpNetworkReply(request, HelpBrowserSupport::msgHtmlErrorPage(request.url()).toUtf8(),
                                QStringLiteral("text/html"));
}

QByteArray HelpBrowserSupport::fileDataForLocalUrl(const QUrl &url)
{
    return HelpEngineWrapper::instance().fileData(url);
}

HelpBrowserSupport::ResolveUrlResult HelpBrowserSupport::resolveUrl(const QUrl &url,
                                                                    QUrl *targetUrlP,
                                                                    QByteArray *dataP)
{
    const HelpEngineWrapper &engine = HelpEngineWrapper::instance();

    const QUrl targetUrl = engine.findFile(url);
    if (!targetUrl.isValid())
        return UrlResolveError;

    if (targetUrl != url) {
        if (targetUrlP)
            *targetUrlP = targetUrl;
        return UrlRedirect;
    }

    if (dataP)
        *dataP = HelpBrowserSupport::fileDataForLocalUrl(targetUrl);
    return UrlLocalData;
}

QNetworkAccessManager *HelpBrowserSupport::createNetworkAccessManager(QObject *parent)
{
    return new HelpNetworkAccessManager(parent);
}

QT_END_NAMESPACE
