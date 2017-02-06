/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Extras module of the Qt Toolkit.
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

#ifndef QQUICKTEXTHANDLE_H
#define QQUICKTEXTHANDLE_H

#include <QPainter>
#include <QQuickPaintedItem>

class QQuickTextHandle : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(TextHandleType type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(qreal scaleFactor READ scaleFactor WRITE setScaleFactor NOTIFY scaleFactorChanged)
    Q_ENUMS(TextHandleType)
public:
    enum TextHandleType {
        CursorHandle = 0,
        SelectionHandle = 1
    };

    QQuickTextHandle(QQuickItem *parent = 0);
    ~QQuickTextHandle();

    void paint(QPainter *painter) Q_DECL_OVERRIDE;

    TextHandleType type() const;
    void setType(TextHandleType type);

    QColor color() const;
    void setColor(const QColor &color);

    qreal scaleFactor() const;
    void setScaleFactor(qreal scaleFactor);

Q_SIGNALS:
    void typeChanged();
    void colorChanged();
    void scaleFactorChanged();

private:
    void paintBulb(QPainter *painter, const QColor &color, bool isShadow);

    TextHandleType mType;
    QColor mColor;
    qreal mScaleFactor;
};

#endif // QQUICKTEXTHANDLE_H
