/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#ifndef QTGRADIENTSTOPSMODEL_H
#define QTGRADIENTSTOPSMODEL_H

#include <QtCore/QObject>
#include <QtCore/QMap>

QT_BEGIN_NAMESPACE

class QColor;

class QtGradientStopsModel;

class QtGradientStop
{
public:
    qreal position() const;
    QColor color() const;
    QtGradientStopsModel *gradientModel() const;

private:
    void setColor(const QColor &color);
    void setPosition(qreal position);
    friend class QtGradientStopsModel;
    QtGradientStop(QtGradientStopsModel *model = 0);
    ~QtGradientStop();
    QScopedPointer<class QtGradientStopPrivate> d_ptr;
};

class QtGradientStopsModel : public QObject
{
    Q_OBJECT
public:
    typedef QMap<qreal, QtGradientStop *> PositionStopMap;

    QtGradientStopsModel(QObject *parent = 0);
    ~QtGradientStopsModel();

    PositionStopMap stops() const;
    QtGradientStop *at(qreal pos) const;
    QColor color(qreal pos) const; // calculated between points
    QList<QtGradientStop *> selectedStops() const;
    QtGradientStop *currentStop() const;
    bool isSelected(QtGradientStop *stop) const;
    QtGradientStop *firstSelected() const;
    QtGradientStop *lastSelected() const;
    QtGradientStopsModel *clone() const;

    QtGradientStop *addStop(qreal pos, const QColor &color);
    void removeStop(QtGradientStop *stop);
    void moveStop(QtGradientStop *stop, qreal newPos);
    void swapStops(QtGradientStop *stop1, QtGradientStop *stop2);
    void changeStop(QtGradientStop *stop, const QColor &newColor);
    void selectStop(QtGradientStop *stop, bool select);
    void setCurrentStop(QtGradientStop *stop);

    void moveStops(double newPosition); // moves current stop to newPos and all selected stops are moved accordingly
    void clear();
    void clearSelection();
    void flipAll();
    void selectAll();
    void deleteStops();

signals:
    void stopAdded(QtGradientStop *stop);
    void stopRemoved(QtGradientStop *stop);
    void stopMoved(QtGradientStop *stop, qreal newPos);
    void stopsSwapped(QtGradientStop *stop1, QtGradientStop *stop2);
    void stopChanged(QtGradientStop *stop, const QColor &newColor);
    void stopSelected(QtGradientStop *stop, bool selected);
    void currentStopChanged(QtGradientStop *stop);

private:
    QScopedPointer<class QtGradientStopsModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QtGradientStopsModel)
    Q_DISABLE_COPY(QtGradientStopsModel)
};

QT_END_NAMESPACE

#endif
