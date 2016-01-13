/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "viewtestutil.h"

#include <QtQuick/QQuickView>
#include <QtQuick/QQuickView>
#include <QtGui/QScreen>

#include <QtTest/QTest>

#include <private/qquickwindow_p.h>


QQuickView *QQuickViewTestUtil::createView()
{
    QQuickView *window = new QQuickView(0);
    const QSize size(240, 320);
    window->resize(size);
    QQuickViewTestUtil::centerOnScreen(window, size);
    return window;
}

void QQuickViewTestUtil::centerOnScreen(QQuickView *window, const QSize &size)
{
    const QRect screenGeometry = window->screen()->availableGeometry();
    const QPoint offset = QPoint(size.width() / 2, size.height() / 2);
    window->setFramePosition(screenGeometry.center() - offset);
}

void QQuickViewTestUtil::centerOnScreen(QQuickView *window)
{
    QQuickViewTestUtil::centerOnScreen(window, window->size());
}

void QQuickViewTestUtil::moveMouseAway(QQuickView *window)
{
#ifndef QT_NO_CURSOR // Get the cursor out of the way.
    QCursor::setPos(window->geometry().topRight() + QPoint(100, 100));
#else
    Q_UNUSED(window)
#endif
}

void QQuickViewTestUtil::flick(QQuickView *window, const QPoint &from, const QPoint &to, int duration)
{
    const int pointCount = 5;
    QPoint diff = to - from;

    // send press, five equally spaced moves, and release.
    QTest::mousePress(window, Qt::LeftButton, 0, from);

    for (int i = 0; i < pointCount; ++i)
        QTest::mouseMove(window, from + (i+1)*diff/pointCount, duration / pointCount);

    QTest::mouseRelease(window, Qt::LeftButton, 0, to);
    QTest::qWait(50);
}

QList<int> QQuickViewTestUtil::adjustIndexesForAddDisplaced(const QList<int> &indexes, int index, int count)
{
    QList<int> result;
    for (int i=0; i<indexes.count(); i++) {
        int num = indexes[i];
        if (num >= index) {
            num += count;
        }
        result << num;
    }
    return result;
}

QList<int> QQuickViewTestUtil::adjustIndexesForMove(const QList<int> &indexes, int from, int to, int count)
{
    QList<int> result;
    for (int i=0; i<indexes.count(); i++) {
        int num = indexes[i];
        if (from < to) {
            if (num >= from && num < from + count)
                num += (to - from); // target
            else if (num >= from && num < to + count)
                num -= count;   // displaced
        } else if (from > to) {
            if (num >= from && num < from + count)
                num -= (from - to);  // target
            else if (num >= to && num < from + count)
                num += count;   // displaced
        }
        result << num;
    }
    return result;
}

QList<int> QQuickViewTestUtil::adjustIndexesForRemoveDisplaced(const QList<int> &indexes, int index, int count)
{
    QList<int> result;
    for (int i=0; i<indexes.count(); i++) {
        int num = indexes[i];
        if (num >= index)
            num -= count;
        result << num;
    }
    return result;
}

QQuickViewTestUtil::QaimModel::QaimModel(QObject *parent)
    : QAbstractListModel(parent)
{
    QHash<int, QByteArray> roles;
    roles[Name] = "name";
    roles[Number] = "number";
    setRoleNames(roles);
}

int QQuickViewTestUtil::QaimModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return list.count();
}

QVariant QQuickViewTestUtil::QaimModel::data(const QModelIndex &index, int role) const
{
    QVariant rv;
    if (role == Name)
        rv = list.at(index.row()).first;
    else if (role == Number)
        rv = list.at(index.row()).second;

    return rv;
}

int QQuickViewTestUtil::QaimModel::count() const
{
    return rowCount();
}

QString QQuickViewTestUtil::QaimModel::name(int index) const
{
    return list.at(index).first;
}

QString QQuickViewTestUtil::QaimModel::number(int index) const
{
    return list.at(index).second;
}

void QQuickViewTestUtil::QaimModel::addItem(const QString &name, const QString &number)
{
    emit beginInsertRows(QModelIndex(), list.count(), list.count());
    list.append(QPair<QString,QString>(name, number));
    emit endInsertRows();
}

void QQuickViewTestUtil::QaimModel::addItems(const QList<QPair<QString, QString> > &items)
{
    emit beginInsertRows(QModelIndex(), list.count(), list.count()+items.count()-1);
    for (int i=0; i<items.count(); i++)
        list.append(QPair<QString,QString>(items[i].first, items[i].second));
    emit endInsertRows();
}

void QQuickViewTestUtil::QaimModel::insertItem(int index, const QString &name, const QString &number)
{
    emit beginInsertRows(QModelIndex(), index, index);
    list.insert(index, QPair<QString,QString>(name, number));
    emit endInsertRows();
}

void QQuickViewTestUtil::QaimModel::insertItems(int index, const QList<QPair<QString, QString> > &items)
{
    emit beginInsertRows(QModelIndex(), index, index+items.count()-1);
    for (int i=0; i<items.count(); i++)
        list.insert(index + i, QPair<QString,QString>(items[i].first, items[i].second));
    emit endInsertRows();
}

void QQuickViewTestUtil::QaimModel::removeItem(int index)
{
    emit beginRemoveRows(QModelIndex(), index, index);
    list.removeAt(index);
    emit endRemoveRows();
}

void QQuickViewTestUtil::QaimModel::removeItems(int index, int count)
{
    emit beginRemoveRows(QModelIndex(), index, index+count-1);
    while (count--)
        list.removeAt(index);
    emit endRemoveRows();
}

void QQuickViewTestUtil::QaimModel::moveItem(int from, int to)
{
    emit beginMoveRows(QModelIndex(), from, from, QModelIndex(), to);
    list.move(from, to);
    emit endMoveRows();
}

void QQuickViewTestUtil::QaimModel::moveItems(int from, int to, int count)
{
    emit beginMoveRows(QModelIndex(), from, from+count-1, QModelIndex(), to > from ? to+count : to);
    qquickmodelviewstestutil_move(from, to, count, &list);
    emit endMoveRows();
}

void QQuickViewTestUtil::QaimModel::modifyItem(int idx, const QString &name, const QString &number)
{
    list[idx] = QPair<QString,QString>(name, number);
    emit dataChanged(index(idx,0), index(idx,0));
}

void QQuickViewTestUtil::QaimModel::clear()
{
    int count = list.count();
    if (count > 0) {
        beginRemoveRows(QModelIndex(), 0, count-1);
        list.clear();
        endRemoveRows();
    }
}

void QQuickViewTestUtil::QaimModel::reset()
{
    emit beginResetModel();
    emit endResetModel();
}

void QQuickViewTestUtil::QaimModel::resetItems(const QList<QPair<QString, QString> > &items)
{
    beginResetModel();
    list = items;
    endResetModel();
}

void QQuickViewTestUtil::QaimModel::matchAgainst(const QList<QPair<QString, QString> > &other, const QString &error1, const QString &error2) {
    for (int i=0; i<other.count(); i++) {
        QVERIFY2(list.contains(other[i]),
                 QTest::toString(other[i].first + " " + other[i].second + " " + error1));
    }
    for (int i=0; i<list.count(); i++) {
        QVERIFY2(other.contains(list[i]),
                 QTest::toString(list[i].first + " " + list[i].second + " " + error2));
    }
}



QQuickViewTestUtil::ListRange::ListRange()
    : valid(false)
{
}

QQuickViewTestUtil::ListRange::ListRange(const ListRange &other)
    : valid(other.valid)
{
    indexes = other.indexes;
}

QQuickViewTestUtil::ListRange::ListRange(int start, int end)
    : valid(true)
{
    for (int i=start; i<=end; i++)
        indexes << i;
}

QQuickViewTestUtil::ListRange::~ListRange()
{
}

QQuickViewTestUtil::ListRange QQuickViewTestUtil::ListRange::operator+(const ListRange &other) const
{
    if (other == *this)
        return *this;
    ListRange a(*this);
    a.indexes.append(other.indexes);
    return a;
}

bool QQuickViewTestUtil::ListRange::operator==(const ListRange &other) const
{
    return indexes.toSet() == other.indexes.toSet();
}

bool QQuickViewTestUtil::ListRange::operator!=(const ListRange &other) const
{
    return !(*this == other);
}

bool QQuickViewTestUtil::ListRange::isValid() const
{
    return valid;
}

int QQuickViewTestUtil::ListRange::count() const
{
    return indexes.count();
}

QList<QPair<QString,QString> > QQuickViewTestUtil::ListRange::getModelDataValues(const QaimModel &model)
{
    QList<QPair<QString,QString> > data;
    if (!valid)
        return data;
    for (int i=0; i<indexes.count(); i++)
        data.append(qMakePair(model.name(indexes[i]), model.number(indexes[i])));
    return data;
}

namespace QQuickTouchUtils {

    /* QQuickWindow does event compression and only delivers events just
     * before it is about to render the next frame. Since some tests
     * rely on events being delivered immediately AND that no other
     * event processing has occurred in the meanwhile, we flush the
     * event manually and immediately.
     */
    void flush(QQuickWindow *window) {
        if (!window)
            return;
        QQuickWindowPrivate *wd = QQuickWindowPrivate::get(window);
        if (!wd || !wd->delayedTouch)
            return;
        wd->reallyDeliverTouchEvent(wd->delayedTouch);
        delete wd->delayedTouch;
        wd->delayedTouch = 0;
    }

}
