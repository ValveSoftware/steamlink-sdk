/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef QQUICKVIEWTESTUTIL_H
#define QQUICKVIEWTESTUTIL_H

#include <QtQuick/QQuickItem>
#include <QtQml/QQmlExpression>
#include <QtCore/QAbstractListModel>

QT_FORWARD_DECLARE_CLASS(QQuickView)
QT_FORWARD_DECLARE_CLASS(QQuickItemViewPrivate)
QT_FORWARD_DECLARE_CLASS(FxViewItem)

namespace QQuickViewTestUtil
{
    QQuickView *createView();

    void flick(QQuickView *window, const QPoint &from, const QPoint &to, int duration);
    void centerOnScreen(QQuickView *window, const QSize &size);
    void centerOnScreen(QQuickView *window);
    void moveMouseAway(QQuickView *window);
    void moveAndPress(QQuickView *window, const QPoint &position);
    void moveAndRelease(QQuickView *window, const QPoint &position);

    QList<int> adjustIndexesForAddDisplaced(const QList<int> &indexes, int index, int count);
    QList<int> adjustIndexesForMove(const QList<int> &indexes, int from, int to, int count);
    QList<int> adjustIndexesForRemoveDisplaced(const QList<int> &indexes, int index, int count);

    struct ListChange {
        enum { Inserted, Removed, Moved, SetCurrent, SetContentY, Polish } type;
        int index;
        int count;
        int to;     // Move
        qreal pos;  // setContentY

        static ListChange insert(int index, int count = 1) { ListChange c = { Inserted, index, count, -1, 0.0 }; return c; }
        static ListChange remove(int index, int count = 1) { ListChange c = { Removed, index, count, -1, 0.0 }; return c; }
        static ListChange move(int index, int to, int count) { ListChange c = { Moved, index, count, to, 0.0 }; return c; }
        static ListChange setCurrent(int index) { ListChange c = { SetCurrent, index, -1, -1, 0.0 }; return c; }
        static ListChange setContentY(qreal pos) { ListChange c = { SetContentY, -1, -1, -1, pos }; return c; }
        static ListChange polish() { ListChange c = { Polish, -1, -1, -1, 0.0 }; return c; }
    };

    class QaimModel : public QAbstractListModel
    {
        Q_OBJECT
    public:
        enum Roles { Name = Qt::UserRole+1, Number = Qt::UserRole+2 };

        QaimModel(QObject *parent=0);

        int rowCount(const QModelIndex &parent=QModelIndex()) const;
        QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;
        QHash<int,QByteArray> roleNames() const;

        int count() const;
        QString name(int index) const;
        QString number(int index) const;

        Q_INVOKABLE void addItem(const QString &name, const QString &number);
        void addItems(const QList<QPair<QString, QString> > &items);
        void insertItem(int index, const QString &name, const QString &number);
        void insertItems(int index, const QList<QPair<QString, QString> > &items);

        Q_INVOKABLE void removeItem(int index);
        void removeItems(int index, int count);

        void moveItem(int from, int to);
        void moveItems(int from, int to, int count);

        void modifyItem(int idx, const QString &name, const QString &number);

        void clear();
        void reset();
        void resetItems(const QList<QPair<QString, QString> > &items);

        void matchAgainst(const QList<QPair<QString, QString> > &other, const QString &error1, const QString &error2);

        using QAbstractListModel::dataChanged;

    private:
        QList<QPair<QString,QString> > list;
    };

    class ListRange
    {
    public:
        ListRange();
        ListRange(const ListRange &other);
        ListRange(int start, int end);

        ~ListRange();

        ListRange operator+(const ListRange &other) const;
        bool operator==(const ListRange &other) const;
        bool operator!=(const ListRange &other) const;

        bool isValid() const;
        int count() const;

        QList<QPair<QString,QString> > getModelDataValues(const QaimModel &model);

        QList<int> indexes;
        bool valid;
    };

    template<typename T>
    static void qquickmodelviewstestutil_move(int from, int to, int n, T *items)
    {
        if (from > to) {
            // Only move forwards - flip if backwards moving
            int tfrom = from;
            int tto = to;
            from = tto;
            to = tto+n;
            n = tfrom-tto;
        }

        T replaced;
        int i=0;
        typename T::ConstIterator it=items->begin(); it += from+n;
        for (; i<to-from; ++i,++it)
            replaced.append(*it);
        i=0;
        it=items->begin(); it += from;
        for (; i<n; ++i,++it)
            replaced.append(*it);
        typename T::ConstIterator f=replaced.begin();
        typename T::Iterator t=items->begin(); t += from;
        for (; f != replaced.end(); ++f, ++t)
            *t = *f;
    }

    class StressTestModel : public QAbstractListModel
    {
        Q_OBJECT

    public:

        StressTestModel();

        int rowCount(const QModelIndex &) const;
        QVariant data(const QModelIndex &, int) const;

    public Q_SLOTS:
        void updateModel();

    private:
        int m_rowCount;
    };

    bool testVisibleItems(const QQuickItemViewPrivate *priv, bool *nonUnique, FxViewItem **failItem, int *expectedIdx);
}

namespace QQuickTouchUtils {
    void flush(QQuickWindow *window);
}

Q_DECLARE_METATYPE(QQuickViewTestUtil::QaimModel*)
Q_DECLARE_METATYPE(QQuickViewTestUtil::ListChange)
Q_DECLARE_METATYPE(QList<QQuickViewTestUtil::ListChange>)
Q_DECLARE_METATYPE(QQuickViewTestUtil::ListRange)


#endif // QQUICKVIEWTESTUTIL_H
