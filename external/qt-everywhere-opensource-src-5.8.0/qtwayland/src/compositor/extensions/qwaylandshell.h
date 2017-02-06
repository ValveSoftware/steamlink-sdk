/****************************************************************************
**
** Copyright (C) 2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#ifndef QWAYLANDSHELL_H
#define QWAYLANDSHELL_H

#include <QtWaylandCompositor/QWaylandCompositorExtension>

QT_BEGIN_NAMESPACE

class QWaylandShellPrivate;

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandShell : public QWaylandCompositorExtension
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandShell)
    Q_PROPERTY(FocusPolicy focusPolicy READ focusPolicy WRITE setFocusPolicy NOTIFY focusPolicyChanged)
public:
    enum FocusPolicy {
        AutomaticFocus,
        ManualFocus
    };
    Q_ENUM(FocusPolicy)

    QWaylandShell();
    QWaylandShell(QWaylandObject *waylandObject);

    FocusPolicy focusPolicy() const;
    void setFocusPolicy(FocusPolicy focusPolicy);

Q_SIGNALS:
    void focusPolicyChanged();

protected:
    QWaylandShell(QWaylandCompositorExtensionPrivate &dd) : QWaylandCompositorExtension(dd) {}
    QWaylandShell(QWaylandObject *container, QWaylandCompositorExtensionPrivate &dd) : QWaylandCompositorExtension(container, dd) {}
};

template <typename T>
class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandShellTemplate : public QWaylandShell
{
public:
    QWaylandShellTemplate()
        : QWaylandShell()
    { }

    QWaylandShellTemplate(QWaylandObject *container)
        : QWaylandShell(container)
    { }

    const struct wl_interface *extensionInterface() const Q_DECL_OVERRIDE
    {
        return T::interface();
    }

    static T *findIn(QWaylandObject *container)
    {
        if (!container) return nullptr;
        return qobject_cast<T *>(container->extension(T::interfaceName()));
    }

protected:
    QWaylandShellTemplate(QWaylandCompositorExtensionPrivate &dd)
        : QWaylandShell(dd)
    { }

    QWaylandShellTemplate(QWaylandObject *container, QWaylandCompositorExtensionPrivate &dd)
        : QWaylandShell(container,dd)
    { }
};

QT_END_NAMESPACE

#endif // QWAYLANDSHELL_H
