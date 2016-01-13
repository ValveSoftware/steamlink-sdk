/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#ifndef QQUICKANDROID9PATCH_P_H
#define QQUICKANDROID9PATCH_P_H

#include <QtQuick/qquickitem.h>
#include <QtQuick/qsggeometry.h>
#include <QtQuick/qsgtexturematerial.h>

QT_BEGIN_NAMESPACE

struct QQuickAndroid9PatchDivs
{
    QVector<qreal> coordsForSize(qreal size) const;

    void fill(const QVariantList &divs, qreal size);
    void clear();

    bool inverted;
    QVector<qreal> data;
};

class QQuickAndroid9Patch : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged FINAL)
    Q_PROPERTY(QVariantList xDivs READ xDivs WRITE setXDivs NOTIFY xDivsChanged FINAL)
    Q_PROPERTY(QVariantList yDivs READ yDivs WRITE setYDivs NOTIFY yDivsChanged FINAL)
    Q_PROPERTY(QSize sourceSize READ sourceSize NOTIFY sourceSizeChanged FINAL)

public:
    explicit QQuickAndroid9Patch(QQuickItem *parent = 0);
    ~QQuickAndroid9Patch();

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
    QQuickAndroid9PatchDivs m_xDivs;
    QQuickAndroid9PatchDivs m_yDivs;
};

QT_END_NAMESPACE

#endif // QQUICKANDROID9PATCH_P_H
