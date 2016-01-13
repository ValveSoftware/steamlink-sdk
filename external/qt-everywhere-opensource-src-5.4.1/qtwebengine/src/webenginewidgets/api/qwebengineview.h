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
    Q_PROPERTY(QUrl iconUrl READ iconUrl)
    Q_PROPERTY(QString selectedText READ selectedText)
    Q_PROPERTY(bool hasSelection READ hasSelection)
    Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor)

public:
    explicit QWebEngineView(QWidget* parent = 0);
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

    bool hasSelection() const;
    QString selectedText() const;

#ifndef QT_NO_ACTION
    QAction* pageAction(QWebEnginePage::WebAction action) const;
#endif
    void triggerPageAction(QWebEnginePage::WebAction action, bool checked = false);

    qreal zoomFactor() const;
    void setZoomFactor(qreal factor);

#ifdef Q_QDOC
    void findText(const QString &subString, QWebEnginePage::FindFlags options = 0);
    void findText(const QString &subString, QWebEnginePage::FindFlags options, FunctorOrLambda resultCallback);
#else
    void findText(const QString &subString, QWebEnginePage::FindFlags options = 0, const QWebEngineCallback<bool> &resultCallback = QWebEngineCallback<bool>());
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

protected:
    virtual QWebEngineView *createWindow(QWebEnginePage::WebWindowType type);
    virtual void contextMenuEvent(QContextMenuEvent*) Q_DECL_OVERRIDE;
    virtual bool event(QEvent*) Q_DECL_OVERRIDE;

private:
    Q_DECLARE_PRIVATE(QWebEngineView);
    QScopedPointer<QWebEngineViewPrivate> d_ptr;

    friend class QWebEnginePage;
    friend class QWebEnginePagePrivate;
};

QT_END_NAMESPACE

#endif // QWEBENGINEVIEW_H
