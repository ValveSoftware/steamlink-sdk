/****************************************************************************
**
** Copyright (C) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWAYLANDQUICKOUTPUT_H
#define QWAYLANDQUICKOUTPUT_H

#include <QtQuick/QQuickWindow>
#include <QtWaylandCompositor/qwaylandoutput.h>

QT_BEGIN_NAMESPACE

class QWaylandQuickCompositor;
class QQuickWindow;

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandQuickOutput : public QWaylandOutput
{
    Q_OBJECT
    Q_PROPERTY(bool automaticFrameCallback READ automaticFrameCallback WRITE setAutomaticFrameCallback NOTIFY automaticFrameCallbackChanged)
public:
    QWaylandQuickOutput();
    QWaylandQuickOutput(QWaylandCompositor *compositor, QWindow *window);

    void update() Q_DECL_OVERRIDE;

    bool automaticFrameCallback() const;
    void setAutomaticFrameCallback(bool automatic);

    QQuickItem *pickClickableItem(const QPointF &position);

public Q_SLOTS:
    void updateStarted();

Q_SIGNALS:
    void automaticFrameCallbackChanged();

protected:
    void initialize() Q_DECL_OVERRIDE;

private:
    void doFrameCallbacks();

    bool m_updateScheduled;
    bool m_automaticFrameCallback;
};

QT_END_NAMESPACE

#endif
