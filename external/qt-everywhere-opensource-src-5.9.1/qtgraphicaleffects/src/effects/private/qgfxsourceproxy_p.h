/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Graphical Effects module of the Qt Toolkit.
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

#ifndef QGFXSOURCEPROXY_P_H
#define QGFXSOURCEPROXY_P_H

#include <QtQuick/QQuickItem>

QT_BEGIN_NAMESPACE

class QQuickShaderEffectSource;

class QGfxSourceProxy : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QQuickItem *input READ input WRITE setInput NOTIFY inputChanged RESET resetInput)
    Q_PROPERTY(QQuickItem *output READ output NOTIFY outputChanged)
    Q_PROPERTY(QRectF sourceRect READ sourceRect WRITE setSourceRect NOTIFY sourceRectChanged)

    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(Interpolation interpolation READ interpolation WRITE setInterpolation NOTIFY interpolationChanged)

    Q_ENUMS(Interpolation)

public:
    enum Interpolation {
        AnyInterpolation,
        NearestInterpolation,
        LinearInterpolation
    };

    QGfxSourceProxy(QQuickItem *item = 0);
    ~QGfxSourceProxy();

    QQuickItem *input() const { return m_input; }
    void setInput(QQuickItem *input);
    void resetInput() { setInput(0); }

    QQuickItem *output() const { return m_output; }

    QRectF sourceRect() const { return m_sourceRect; }
    void setSourceRect(const QRectF &sourceRect);

    bool isActive() const { return m_output && m_output != m_input; }

    void setInterpolation(Interpolation i);
    Interpolation interpolation() const { return m_interpolation; }

protected:
    void updatePolish() Q_DECL_OVERRIDE;

signals:
    void inputChanged();
    void outputChanged();
    void sourceRectChanged();
    void activeChanged();
    void interpolationChanged();

private:
    void setOutput(QQuickItem *output);
    void useProxy();
    static QObject *findLayer(QQuickItem *);

    QRectF m_sourceRect;
    QQuickItem *m_input;
    QQuickItem *m_output;
    QQuickShaderEffectSource *m_proxy;

    Interpolation m_interpolation;
};

QT_END_NAMESPACE

#endif // QGFXSOURCEPROXY_P_H
