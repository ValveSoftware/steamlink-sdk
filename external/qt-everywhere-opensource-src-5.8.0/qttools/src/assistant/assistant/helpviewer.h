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

#ifndef HELPVIEWER_H
#define HELPVIEWER_H

#include <QtCore/qglobal.h>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QVariant>

#include <QtWidgets/QAction>
#include <QtGui/QFont>

#if defined(BROWSER_QTWEBKIT)
#  include <QWebView>
#elif defined(BROWSER_QTEXTBROWSER)
#  include <QtWidgets/QTextBrowser>
#endif

QT_BEGIN_NAMESPACE

class HelpEngineWrapper;

#if defined(BROWSER_QTWEBKIT)
class HelpViewer : public QWebView
#elif defined(BROWSER_QTEXTBROWSER)
class HelpViewer : public QTextBrowser
#endif
{
    Q_OBJECT
    class HelpViewerPrivate;
    Q_DISABLE_COPY(HelpViewer)

public:
    enum FindFlag {
        FindBackward = 0x01,
        FindCaseSensitively = 0x02
    };
    Q_DECLARE_FLAGS(FindFlags, FindFlag)

    HelpViewer(qreal zoom, QWidget *parent = 0);
    ~HelpViewer();

    QFont viewerFont() const;
    void setViewerFont(const QFont &font);

    void scaleUp();
    void scaleDown();

    void resetScale();
    qreal scale() const;

    QString title() const;
    void setTitle(const QString &title);

    QUrl source() const;
    void setSource(const QUrl &url);

    QString selectedText() const;
    bool isForwardAvailable() const;
    bool isBackwardAvailable() const;

    bool findText(const QString &text, FindFlags flags, bool incremental,
        bool fromSearch);

    static const QString AboutBlank;
    static const QString LocalHelpFile;
    static const QString PageNotFoundMessage;

    static bool isLocalUrl(const QUrl &url);
    static bool canOpenPage(const QString &url);
    static QString mimeFromUrl(const QUrl &url);
    static bool launchWithExternalApp(const QUrl &url);

public slots:
#ifndef QT_NO_CLIPBOARD
    void copy();
#endif
    void home();

    void forward();
    void backward();

signals:
    void titleChanged();
#if !defined(BROWSER_QTEXTBROWSER)
    // Provide signals present in QTextBrowser, QTextEdit for browsers that do not inherit QTextBrowser
    void copyAvailable(bool yes);
    void sourceChanged(const QUrl &url);
    void forwardAvailable(bool enabled);
    void backwardAvailable(bool enabled);
    void highlighted(const QString &link);
    void printRequested();
#elif !defined(BROWSER_QTWEBKIT)
    // Provide signals present in QWebView for browsers that do not inherit QWebView
    void loadStarted();
    void loadFinished(bool finished);
#endif

protected:
    void keyPressEvent(QKeyEvent *e);
    void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private slots:
    void actionChanged();
    void setLoadStarted();
    void setLoadFinished(bool ok);

private:
    bool eventFilter(QObject *obj, QEvent *event);
    void contextMenuEvent(QContextMenuEvent *event);
    QVariant loadResource(int type, const QUrl &name);
    bool handleForwardBackwardMouseButtons(QMouseEvent *e);

private:
    HelpViewerPrivate *d;
};

QT_END_NAMESPACE
Q_DECLARE_METATYPE(HelpViewer*)

#endif  // HELPVIEWER_H
