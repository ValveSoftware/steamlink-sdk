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

#ifndef QWEBENGINEPAGE_H
#define QWEBENGINEPAGE_H

#include <QtWebEngineWidgets/qtwebenginewidgetsglobal.h>
#include <QtWebEngineWidgets/qwebenginecertificateerror.h>

#include <QtCore/qobject.h>
#include <QtCore/qurl.h>
#include <QtCore/qvariant.h>
#include <QtNetwork/qnetworkaccessmanager.h>
#include <QtWidgets/qwidget.h>

QT_BEGIN_NAMESPACE
class QMenu;
class QWebEngineHistory;
class QWebEnginePage;
class QWebEngineSettings;
class QWebEnginePagePrivate;

namespace QtWebEnginePrivate {

template <typename T>
class QWebEngineCallbackPrivateBase : public QSharedData {
public:
    virtual ~QWebEngineCallbackPrivateBase() {}
    virtual void operator()(T) = 0;
};

template <typename T, typename F>
class QWebEngineCallbackPrivate : public QWebEngineCallbackPrivateBase<T> {
public:
    QWebEngineCallbackPrivate(F callable) : m_callable(callable) {}
    virtual void operator()(T value) Q_DECL_OVERRIDE { m_callable(value); }
private:
    F m_callable;
};

} // namespace QtWebEnginePrivate

template <typename T>
class QWebEngineCallback {
public:
    template <typename F>
    QWebEngineCallback(F f) : d(new QtWebEnginePrivate::QWebEngineCallbackPrivate<T, F>(f)) { }
    QWebEngineCallback() { }
private:
    QExplicitlySharedDataPointer<QtWebEnginePrivate::QWebEngineCallbackPrivateBase<T> > d;
    friend class QWebEnginePage;
};

class QWEBENGINEWIDGETS_EXPORT QWebEnginePage : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString selectedText READ selectedText)
    Q_PROPERTY(bool hasSelection READ hasSelection)

    // Ex-QWebFrame properties
    Q_PROPERTY(QUrl requestedUrl READ requestedUrl)
    Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor)
    Q_PROPERTY(QString title READ title)
    Q_PROPERTY(QUrl url READ url WRITE setUrl)
    Q_PROPERTY(QUrl iconUrl READ iconUrl)

public:
    enum WebAction {
        NoWebAction = - 1,
        Back,
        Forward,
        Stop,
        Reload,

        Cut,
        Copy,
        Paste,

        Undo,
        Redo,
        SelectAll,
        ReloadAndBypassCache,

        PasteAndMatchStyle,

        WebActionCount
    };

    enum FindFlag {
        FindBackward = 1,
        FindCaseSensitively = 2,
    };
    Q_DECLARE_FLAGS(FindFlags, FindFlag);

    enum WebWindowType {
        WebBrowserWindow,
        WebBrowserTab,
        WebDialog
    };

    enum PermissionPolicy {
        PermissionUnknown,
        PermissionGrantedByUser,
        PermissionDeniedByUser
    };

    enum Feature {
#ifndef Q_QDOC
        Notifications = 0,
        Geolocation = 1,
#endif
        MediaAudioCapture = 2,
        MediaVideoCapture,
        MediaAudioVideoCapture
    };

    // Ex-QWebFrame enum

    enum FileSelectionMode {
        FileSelectOpen,
        FileSelectOpenMultiple,
    };

    // must match WebContentsAdapterClient::JavaScriptConsoleMessageLevel
    enum JavaScriptConsoleMessageLevel {
        InfoMessageLevel = 0,
        WarningMessageLevel,
        ErrorMessageLevel
    };

    explicit QWebEnginePage(QObject *parent = 0);
    ~QWebEnginePage();
    QWebEngineHistory *history() const;

    void setView(QWidget *view);
    QWidget *view() const;

    bool hasSelection() const;
    QString selectedText() const;

#ifndef QT_NO_ACTION
    QAction *action(WebAction action) const;
#endif
    virtual void triggerAction(WebAction action, bool checked = false);

    virtual bool event(QEvent*);
#ifdef Q_QDOC
    void findText(const QString &subString, FindFlags options = 0);
    void findText(const QString &subString, FindFlags options, FunctorOrLambda resultCallback);
#else
    void findText(const QString &subString, FindFlags options = 0, const QWebEngineCallback<bool> &resultCallback = QWebEngineCallback<bool>());
#endif
    QMenu *createStandardContextMenu();

    void setFeaturePermission(const QUrl &securityOrigin, Feature feature, PermissionPolicy policy);

    // Ex-QWebFrame methods
    void load(const QUrl &url);
    void setHtml(const QString &html, const QUrl &baseUrl = QUrl());
    void setContent(const QByteArray &data, const QString &mimeType = QString(), const QUrl &baseUrl = QUrl());

#ifdef Q_QDOC
    void toHtml(FunctorOrLambda resultCallback) const;
    void toPlainText(FunctorOrLambda resultCallback) const;
#else
    void toHtml(const QWebEngineCallback<const QString &> &resultCallback) const;
    void toPlainText(const QWebEngineCallback<const QString &> &resultCallback) const;
#endif

    QString title() const;
    void setUrl(const QUrl &url);
    QUrl url() const;
    QUrl requestedUrl() const;
    QUrl iconUrl() const;

    qreal zoomFactor() const;
    void setZoomFactor(qreal factor);

    void runJavaScript(const QString& scriptSource);
#ifdef Q_QDOC
    void runJavaScript(const QString& scriptSource, FunctorOrLambda resultCallback);
#else
    void runJavaScript(const QString& scriptSource, const QWebEngineCallback<const QVariant &> &resultCallback);
#endif

    QWebEngineSettings *settings() const;

Q_SIGNALS:
    void loadStarted();
    void loadProgress(int progress);
    void loadFinished(bool ok);

    void linkHovered(const QString &url);
    void selectionChanged();
    void geometryChangeRequested(const QRect& geom);
    void windowCloseRequested();

    void featurePermissionRequested(const QUrl &securityOrigin, QWebEnginePage::Feature feature);
    void featurePermissionRequestCanceled(const QUrl &securityOrigin, QWebEnginePage::Feature feature);

    void authenticationRequired(const QUrl &requestUrl, QAuthenticator *authenticator);
    void proxyAuthenticationRequired(const QUrl &requestUrl, QAuthenticator *authenticator, const QString &proxyHost);

    // Ex-QWebFrame signals
    void titleChanged(const QString &title);
    void urlChanged(const QUrl &url);
    // Was iconChanged() in QWebFrame
    void iconUrlChanged(const QUrl &url);

protected:
    virtual QWebEnginePage *createWindow(WebWindowType type);

    virtual QStringList chooseFiles(FileSelectionMode mode, const QStringList &oldFiles, const QStringList &acceptedMimeTypes);
    virtual void javaScriptAlert(const QUrl &securityOrigin, const QString& msg);
    virtual bool javaScriptConfirm(const QUrl &securityOrigin, const QString& msg);
    virtual bool javaScriptPrompt(const QUrl &securityOrigin, const QString& msg, const QString& defaultValue, QString* result);
    virtual void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber, const QString& sourceID);
    virtual bool certificateError(const QWebEngineCertificateError &certificateError);

private:
    Q_DECLARE_PRIVATE(QWebEnginePage);
    QScopedPointer<QWebEnginePagePrivate> d_ptr;
#ifndef QT_NO_ACTION
    Q_PRIVATE_SLOT(d_func(), void _q_webActionTriggered(bool checked))
#endif

    friend class QWebEngineView;
    friend class QWebEngineViewPrivate;
    friend class QWebEngineViewAccessible;
};


QT_END_NAMESPACE

#endif // QWEBENGINEPAGE_H
