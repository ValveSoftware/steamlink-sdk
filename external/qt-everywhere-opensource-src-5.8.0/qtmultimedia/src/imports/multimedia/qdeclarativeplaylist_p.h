/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#ifndef QDECLARATIVEPLAYLIST_P_H
#define QDECLARATIVEPLAYLIST_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/QAbstractListModel>
#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qqml.h>

#include <qmediaplaylist.h>

QT_BEGIN_NAMESPACE

class QDeclarativePlaylistItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource)

public:
    QDeclarativePlaylistItem(QObject *parent = 0);

    QUrl source() const;
    void setSource(const QUrl &source);

private:
    QUrl m_source;
};

class QDeclarativePlaylist : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_PROPERTY(PlaybackMode playbackMode READ playbackMode WRITE setPlaybackMode NOTIFY playbackModeChanged)
    Q_PROPERTY(QUrl currentItemSource READ currentItemSource NOTIFY currentItemSourceChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY readOnlyChanged)
    Q_PROPERTY(Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(QQmlListProperty<QDeclarativePlaylistItem> items READ items DESIGNABLE false)
    Q_ENUMS(PlaybackMode)
    Q_ENUMS(Error)
    Q_INTERFACES(QQmlParserStatus)
    Q_CLASSINFO("DefaultProperty", "items")

public:
    enum PlaybackMode
    {
        CurrentItemOnce = QMediaPlaylist::CurrentItemOnce,
        CurrentItemInLoop = QMediaPlaylist::CurrentItemInLoop,
        Sequential = QMediaPlaylist::Sequential,
        Loop = QMediaPlaylist::Loop,
        Random = QMediaPlaylist::Random
    };
    enum Error
    {
        NoError = QMediaPlaylist::NoError,
        FormatError = QMediaPlaylist::FormatError,
        FormatNotSupportedError = QMediaPlaylist::FormatNotSupportedError,
        NetworkError = QMediaPlaylist::NetworkError,
        AccessDeniedError = QMediaPlaylist::AccessDeniedError
    };
    enum Roles
    {
        SourceRole = Qt::UserRole + 1
    };

    QDeclarativePlaylist(QObject *parent = 0);
    ~QDeclarativePlaylist();

    PlaybackMode playbackMode() const;
    void setPlaybackMode(PlaybackMode playbackMode);
    QUrl currentItemSource() const;
    int currentIndex() const;
    void setCurrentIndex(int currentIndex);
    int itemCount() const;
    bool readOnly() const;
    Error error() const;
    QString errorString() const;
    QMediaPlaylist *mediaPlaylist() const { return m_playlist; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QHash<int, QByteArray> roleNames() const;

    void classBegin();
    void componentComplete();

    QQmlListProperty<QDeclarativePlaylistItem> items() {
        return QQmlListProperty<QDeclarativePlaylistItem>(
            this, 0, item_append, item_count, 0, item_clear);
    }
    static void item_append(QQmlListProperty<QDeclarativePlaylistItem> *list,
                            QDeclarativePlaylistItem* item) {
        static_cast<QDeclarativePlaylist*>(list->object)->addItem(item->source());
    }
    static int item_count(QQmlListProperty<QDeclarativePlaylistItem> *list) {
        return static_cast<QDeclarativePlaylist*>(list->object)->itemCount();
    }
    static void item_clear(QQmlListProperty<QDeclarativePlaylistItem> *list) {
        static_cast<QDeclarativePlaylist*>(list->object)->clear();
    }

public Q_SLOTS:
    QUrl itemSource(int index);
    int nextIndex(int steps = 1);
    int previousIndex(int steps = 1);
    void next();
    void previous();
    void shuffle();
    void load(const QUrl &location, const QString &format = QString());
    bool save(const QUrl &location, const QString &format = QString());
    bool addItem(const QUrl &source);
    Q_REVISION(1) bool addItems(const QList<QUrl> &sources);
    bool insertItem(int index, const QUrl &source);
    Q_REVISION(1) bool insertItems(int index, const QList<QUrl> &sources);
    Q_REVISION(1) bool moveItem(int from, int to);
    bool removeItem(int index);
    Q_REVISION(1) bool removeItems(int start, int end);
    bool clear();

Q_SIGNALS:
    void playbackModeChanged();
    void currentItemSourceChanged();
    void currentIndexChanged();
    void itemCountChanged();
    void readOnlyChanged();
    void errorChanged();

    void itemAboutToBeInserted(int start, int end);
    void itemInserted(int start, int end);
    void itemAboutToBeRemoved(int start, int end);
    void itemRemoved(int start, int end);
    void itemChanged(int start, int end);
    void loaded();
    void loadFailed();

    void error(QDeclarativePlaylist::Error error, const QString &errorString);

private Q_SLOTS:
    void _q_mediaAboutToBeInserted(int start, int end);
    void _q_mediaInserted(int start, int end);
    void _q_mediaAboutToBeRemoved(int start, int end);
    void _q_mediaRemoved(int start, int end);
    void _q_mediaChanged(int start, int end);
    void _q_loadFailed();

private:
    Q_DISABLE_COPY(QDeclarativePlaylist)

    QMediaPlaylist *m_playlist;
    QString m_errorString;
    QMediaPlaylist::Error m_error;
    bool m_readOnly;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QT_PREPEND_NAMESPACE(QDeclarativePlaylistItem))
QML_DECLARE_TYPE(QT_PREPEND_NAMESPACE(QDeclarativePlaylist))

#endif
