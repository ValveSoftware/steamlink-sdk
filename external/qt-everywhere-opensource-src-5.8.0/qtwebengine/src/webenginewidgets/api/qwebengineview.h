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

#ifndef QWEBENGINEVIEW_H
#define QWEBENGINEVIEW_H

#include <QtGui/qpainter.h>
#include <QtNetwork/qnetworkaccessmanager.h>
#include <QtWidgets/qwidget.h>

#include <QtWebEngineWidgets/qtwebenginewidgetsglobal.h>
#include <QtWebEngineWidgets/qwebenginepage.h>

QT_BEGIN_NAMESPACE
class QContextMenuEvent;
class QUrl;
class QWebEnginePage;
class QWebEngineSettings;
class QWebEngineViewPrivate;

class QWEBENGINEWIDGETS_EXPORT QWebEngineView : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString title READ title)
    Q_PROPERTY(QUrl url READ url WRITE setUrl)
    Q_PROPERTY(QUrl iconUrl READ iconUrl NOTIFY iconUrlChanged)
    Q_PROPERTY(QIcon icon READ icon NOTIFY iconChanged)
    Q_PROPERTY(QString selectedText READ selectedText)
    Q_PROPERTY(bool hasSelection READ hasSelection)
    Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor)

public:
    explicit QWebEngineView(QWidget* parent = Q_NULLPTR);
    virtual ~QWebEngineView();

    QWebEnginePage* page() const;
    void setPage(QWebEnginePage* page);

    void load(const QUrl& url);
    void setHtml(const QString& html, const QUrl& baseUrl = QUrl());
    void setContent(const QByteArray& data, const QString& mimeType = QString(), const QUrl& baseUrl = QUrl());

    QWebEngineHistory* history() const;

    QString title() const;
    void setUrl(const QUrl &url);
    QUrl url() const;
    QUrl iconUrl() const;
    QIcon icon() const;

    bool hasSelection() const;
    QString selectedText() const;

#ifndef QT_NO_ACTION
    QAction* pageAction(QWebEnginePage::WebAction action) const;
#endif
    void triggerPageAction(QWebEnginePage::WebAction action, bool checked = false);

    qreal zoomFactor() const;
    void setZoomFactor(qreal factor);

#ifdef Q_QDOC
    void findText(const QString &subString, QWebEnginePage::FindFlags options = QWebEnginePage::FindFlags());
    void findText(const QString &subString, QWebEnginePage::FindFlags options, FunctorOrLambda resultCallback);
#else
    void findText(const QString &subString, QWebEnginePage::FindFlags options = QWebEnginePage::FindFlags(), const QWebEngineCallback<bool> &resultCallback = QWebEngineCallback<bool>());
#endif

    virtual QSize sizeHint() const Q_DECL_OVERRIDE;
    QWebEngineSettings *settings() const;

public Q_SLOTS:
    void stop();
    void back();
    void forward();
    void reload();

Q_SIGNALS:
    void loadStarted();
    void loadProgress(int progress);
    void loadFinished(bool);
    void titleChanged(const QString& title);
    void selectionChanged();
    void urlChanged(const QUrl&);
    void iconUrlChanged(const QUrl&);
    void iconChanged(const QIcon&);
    void renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus,
                             int exitCode);

protected:
    virtual QWebEngineView *createWindow(QWebEnginePage::WebWindowType type);
    virtual void contextMenuEvent(QContextMenuEvent*) Q_DECL_OVERRIDE;
    virtual bool event(QEvent*) Q_DECL_OVERRIDE;
    virtual void showEvent(QShowEvent *) Q_DECL_OVERRIDE;
    virtual void hideEvent(QHideEvent *) Q_DECL_OVERRIDE;
    void dragEnterEvent(QDragEnterEvent *e) Q_DECL_OVERRIDE;
    void dragLeaveEvent(QDragLeaveEvent *e) Q_DECL_OVERRIDE;
    void dragMoveEvent(QDragMoveEvent *e) Q_DECL_OVERRIDE;
    void dropEvent(QDropEvent *e) Q_DECL_OVERRIDE;

private:
    Q_DISABLE_COPY(QWebEngineView)
    Q_DECLARE_PRIVATE(QWebEngineView)
    QScopedPointer<QWebEngineViewPrivate> d_ptr;

    friend class QWebEnginePage;
    friend class QWebEnginePagePrivate;
};

QT_END_NAMESPACE

#endif // QWEBENGINEVIEW_H
