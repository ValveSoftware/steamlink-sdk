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

#ifndef ANDROIDSURFACEVIEW_H
#define ANDROIDSURFACEVIEW_H

#include <QtCore/private/qjni_p.h>
#include <qrect.h>
#include <QtCore/qrunnable.h>

QT_BEGIN_NAMESPACE

class QWindow;

class AndroidSurfaceHolder : public QObject
{
    Q_OBJECT
public:
    ~AndroidSurfaceHolder();

    jobject surfaceHolder() const;
    bool isSurfaceCreated() const;

    static bool initJNI(JNIEnv *env);

Q_SIGNALS:
    void surfaceCreated();

private:
    AndroidSurfaceHolder(QJNIObjectPrivate object);

    static void handleSurfaceCreated(JNIEnv*, jobject, jlong id);
    static void handleSurfaceDestroyed(JNIEnv*, jobject, jlong id);

    QJNIObjectPrivate m_surfaceHolder;
    bool m_surfaceCreated;

    friend class AndroidSurfaceView;
};

class AndroidSurfaceView : public QObject
{
    Q_OBJECT
public:
    AndroidSurfaceView();
    ~AndroidSurfaceView();

    AndroidSurfaceHolder *holder() const;

    void setVisible(bool v);
    void setGeometry(int x, int y, int width, int height);

Q_SIGNALS:
    void surfaceCreated();

private:
    QJNIObjectPrivate m_surfaceView;
    QWindow *m_window;
    AndroidSurfaceHolder *m_surfaceHolder;
    int m_pendingVisible;
    QRect m_pendingGeometry;
};

QT_END_NAMESPACE

#endif // ANDROIDSURFACEVIEW_H
