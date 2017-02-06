/****************************************************************************
**
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

#ifndef QWAYLANDEXTENSION_H
#define QWAYLANDEXTENSION_H

#include <QtWaylandCompositor/qtwaylandcompositorglobal.h>

#include <QtCore/QObject>
#include <QtCore/QVector>

struct wl_interface;

QT_BEGIN_NAMESPACE

class QWaylandCompositor;
class QWaylandCompositorExtension;
class QWaylandCompositorExtensionPrivate;

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandObject : public QObject
{
    Q_OBJECT
public:
    virtual ~QWaylandObject();

    QWaylandCompositorExtension *extension(const QByteArray &name);
    QWaylandCompositorExtension *extension(const wl_interface *interface);
    QList<QWaylandCompositorExtension *> extensions() const;
    void addExtension(QWaylandCompositorExtension *extension);
    void removeExtension(QWaylandCompositorExtension *extension);

protected:
    QWaylandObject(QObject *parent = nullptr);
    QWaylandObject(QObjectPrivate &d, QObject *parent = nullptr);
    QList<QWaylandCompositorExtension *> extension_vector;
};

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandCompositorExtension : public QWaylandObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandCompositorExtension)
public:
    QWaylandCompositorExtension();
    QWaylandCompositorExtension(QWaylandObject *container);
    virtual ~QWaylandCompositorExtension();

    QWaylandObject *extensionContainer() const;
    void setExtensionContainer(QWaylandObject *container);

    virtual void initialize();
    bool isInitialized() const;

    virtual const struct wl_interface *extensionInterface() const = 0;

protected:
    QWaylandCompositorExtension(QWaylandCompositorExtensionPrivate &dd);
    QWaylandCompositorExtension(QWaylandObject *container, QWaylandCompositorExtensionPrivate &dd);

    bool event(QEvent *event) Q_DECL_OVERRIDE;
};

template <typename T>
class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandCompositorExtensionTemplate : public QWaylandCompositorExtension
{
public:
    QWaylandCompositorExtensionTemplate()
        : QWaylandCompositorExtension()
    { }

    QWaylandCompositorExtensionTemplate(QWaylandObject *container)
        : QWaylandCompositorExtension(container)
    { }

    const struct wl_interface *extensionInterface() const Q_DECL_OVERRIDE
    {
        return T::interface();
    }

    static T *findIn(QWaylandObject *container)
    {
        if (!container) return Q_NULLPTR;
        return qobject_cast<T *>(container->extension(T::interfaceName()));
    }

protected:
    QWaylandCompositorExtensionTemplate(QWaylandCompositorExtensionPrivate &dd)
        : QWaylandCompositorExtension(dd)
    { }

    QWaylandCompositorExtensionTemplate(QWaylandObject *container, QWaylandCompositorExtensionPrivate &dd)
        : QWaylandCompositorExtension(container,dd)
    { }
};

QT_END_NAMESPACE

#endif
