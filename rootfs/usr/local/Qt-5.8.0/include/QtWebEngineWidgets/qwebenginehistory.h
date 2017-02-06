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

#ifndef QWEBENGINEHISTORY_H
#define QWEBENGINEHISTORY_H

#include <QtCore/qurl.h>
#include <QtCore/qstring.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qshareddata.h>
#include <QtGui/qicon.h>
#include <QtWebEngineWidgets/qtwebenginewidgetsglobal.h>

QT_BEGIN_NAMESPACE

class QWebEngineHistory;
class QWebEngineHistoryItemPrivate;
class QWebEnginePage;
class QWebEnginePagePrivate;

class QWEBENGINEWIDGETS_EXPORT QWebEngineHistoryItem {
public:
    QWebEngineHistoryItem(const QWebEngineHistoryItem &other);
    QWebEngineHistoryItem &operator=(const QWebEngineHistoryItem &other);
    ~QWebEngineHistoryItem();

    QUrl originalUrl() const;
    QUrl url() const;

    QString title() const;
    QDateTime lastVisited() const;
    QUrl iconUrl() const;

    bool isValid() const;

    void swap(QWebEngineHistoryItem &other) Q_DECL_NOTHROW { qSwap(d, other.d); }

private:
    QWebEngineHistoryItem(QWebEngineHistoryItemPrivate *priv);
    Q_DECLARE_PRIVATE_D(d.data(), QWebEngineHistoryItem)
    QExplicitlySharedDataPointer<QWebEngineHistoryItemPrivate> d;
    friend class QWebEngineHistory;
    friend class QWebEngineHistoryPrivate;
};

#if QT_VERSION >= QT_VERSION_CHECK(5,6,0)
Q_DECLARE_SHARED_NOT_MOVABLE_UNTIL_QT6(QWebEngineHistoryItem)
#endif

class QWebEngineHistoryPrivate;
class QWEBENGINEWIDGETS_EXPORT QWebEngineHistory {
public:
    void clear();

    QList<QWebEngineHistoryItem> items() const;
    QList<QWebEngineHistoryItem> backItems(int maxItems) const;
    QList<QWebEngineHistoryItem> forwardItems(int maxItems) const;

    bool canGoBack() const;
    bool canGoForward() const;

    void back();
    void forward();
    void goToItem(const QWebEngineHistoryItem &item);

    QWebEngineHistoryItem backItem() const;
    QWebEngineHistoryItem currentItem() const;
    QWebEngineHistoryItem forwardItem() const;
    QWebEngineHistoryItem itemAt(int i) const;

    int currentItemIndex() const;

    int count() const;

private:
    QWebEngineHistory(QWebEngineHistoryPrivate *d);
    ~QWebEngineHistory();

    Q_DISABLE_COPY(QWebEngineHistory)
    Q_DECLARE_PRIVATE(QWebEngineHistory)
    QScopedPointer<QWebEngineHistoryPrivate> d_ptr;

    friend QWEBENGINEWIDGETS_EXPORT QDataStream& operator>>(QDataStream&, QWebEngineHistory&);
    friend QWEBENGINEWIDGETS_EXPORT QDataStream& operator<<(QDataStream&, const QWebEngineHistory&);
    friend class QWebEnginePage;
    friend class QWebEnginePagePrivate;
};

QWEBENGINEWIDGETS_EXPORT QDataStream& operator<<(QDataStream& stream, const QWebEngineHistory& history);
QWEBENGINEWIDGETS_EXPORT QDataStream& operator>>(QDataStream& stream, QWebEngineHistory& history);

QT_END_NAMESPACE

#endif // QWEBENGINEHISTORY_H
