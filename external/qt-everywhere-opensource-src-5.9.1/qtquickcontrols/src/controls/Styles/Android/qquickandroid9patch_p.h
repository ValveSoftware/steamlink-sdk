/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#ifndef QQUICKANDROID9PATCH_P_H
#define QQUICKANDROID9PATCH_P_H

#include <QtQuick/qquickitem.h>
#include <QtQuick/qsggeometry.h>
#include <QtQuick/qsgtexturematerial.h>

QT_BEGIN_NAMESPACE

struct QQuickAndroid9PatchDivs1
{
    QVector<qreal> coordsForSize(qreal size) const;

    void fill(const QVariantList &divs, qreal size);
    void clear();

    bool inverted;
    QVector<qreal> data;
};

class QQuickAndroid9Patch1 : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged FINAL)
    Q_PROPERTY(QVariantList xDivs READ xDivs WRITE setXDivs NOTIFY xDivsChanged FINAL)
    Q_PROPERTY(QVariantList yDivs READ yDivs WRITE setYDivs NOTIFY yDivsChanged FINAL)
    Q_PROPERTY(QSize sourceSize READ sourceSize NOTIFY sourceSizeChanged FINAL)

public:
    explicit QQuickAndroid9Patch1(QQuickItem *parent = 0);
    ~QQuickAndroid9Patch1();

    QUrl source() const;
    QVariantList xDivs() const;
    QVariantList yDivs() const;
    QSize sourceSize() const;

Q_SIGNALS:
    void sourceChanged(const QUrl &source);
    void xDivsChanged(const QVariantList &divs);
    void yDivsChanged(const QVariantList &divs);
    void sourceSizeChanged(const QSize &size);

public Q_SLOTS:
    void setSource(const QUrl &source);
    void setXDivs(const QVariantList &divs);
    void setYDivs(const QVariantList &divs);

protected:
    void classBegin();
    void componentComplete();
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data);

private Q_SLOTS:
    void loadImage();
    void updateDivs();

private:
    QImage m_image;
    QUrl m_source;
    QSize m_sourceSize;
    QVariantList m_xVars;
    QVariantList m_yVars;
    QQuickAndroid9PatchDivs1 m_xDivs;
    QQuickAndroid9PatchDivs1 m_yDivs;
};

QT_END_NAMESPACE

#endif // QQUICKANDROID9PATCH_P_H
