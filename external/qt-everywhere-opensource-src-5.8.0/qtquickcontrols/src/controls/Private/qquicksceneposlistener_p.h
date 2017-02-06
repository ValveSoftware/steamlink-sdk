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

#ifndef QQUICKSCENEPOSLISTENER_P_H
#define QQUICKSCENEPOSLISTENER_P_H

#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qset.h>
#include <QtQuick/private/qquickitemchangelistener_p.h>

QT_BEGIN_NAMESPACE

class QQuickItem;

class QQuickScenePosListener1 : public QObject, public QQuickItemChangeListener
{
    Q_OBJECT
    Q_PROPERTY(QQuickItem *item READ item WRITE setItem FINAL)
    Q_PROPERTY(QPointF scenePos READ scenePos NOTIFY scenePosChanged FINAL)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged FINAL)

public:
    explicit QQuickScenePosListener1(QObject *parent = 0);
    ~QQuickScenePosListener1();

    QQuickItem *item() const;
    void setItem(QQuickItem *item);

    QPointF scenePos() const;

    bool isEnabled() const;
    void setEnabled(bool enabled);

Q_SIGNALS:
    void scenePosChanged();
    void enabledChanged();

protected:
    void itemGeometryChanged(QQuickItem *, QQuickGeometryChange, const QRectF &) override;
    void itemParentChanged(QQuickItem *, QQuickItem *parent) override;
    void itemChildRemoved(QQuickItem *, QQuickItem *child) override;
    void itemDestroyed(QQuickItem *item) override;

private:
    void updateScenePos();

    void removeAncestorListeners(QQuickItem *item);
    void addAncestorListeners(QQuickItem *item);

    bool isAncestor(QQuickItem *item) const;

    bool m_enabled;
    QPointF m_scenePos;
    QQuickItem *m_item;
};

QT_END_NAMESPACE

#endif // QQUICKSCENEPOSLISTENER_P_H
