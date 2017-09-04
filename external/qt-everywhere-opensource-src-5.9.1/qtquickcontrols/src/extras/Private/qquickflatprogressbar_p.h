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

#ifndef FLATPROGRESSBAR_H
#define FLATPROGRESSBAR_H

#include <QtCore/QPropertyAnimation>
#include <QtGui/QPainterPath>
#include <QtQuick/QQuickPaintedItem>

class QQuickFlatProgressBar : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(qreal stripeOffset READ stripeOffset WRITE setStripeOffset NOTIFY stripeOffsetChanged)
    Q_PROPERTY(qreal progress READ progress WRITE setProgress NOTIFY progressChanged)
    Q_PROPERTY(bool indeterminate READ isIndeterminate WRITE setIndeterminate NOTIFY indeterminateChanged)
public:
    explicit QQuickFlatProgressBar(QQuickItem *parent = 0);

    void paint(QPainter *painter);

    qreal stripeOffset() const;
    void setStripeOffset(qreal stripeOffset);

    qreal progress() const;
    void setProgress(qreal progress);

    bool isIndeterminate();
    void setIndeterminate(bool indeterminate);

public slots:
    void repaint();
    void restartAnimation();
    void onVisibleChanged();
    void onWidthChanged();
    void onHeightChanged();

signals:
    void stripeOffsetChanged(qreal stripeOffset);
    void progressChanged(qreal progress);
    void indeterminateChanged(bool indeterminate);

private:
    int barWidth();

private:
    qreal mStripeOffset;
    qreal mProgress;
    qreal mRadius;
    bool mIndeterminate;
    QPainterPath mClipPath;
    QPropertyAnimation mAnimation;
};

#endif // FLATPROGRESSBAR_H
