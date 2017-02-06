/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#ifndef QWAYLANDTEXTINPUT_P_H
#define QWAYLANDTEXTINPUT_P_H

#include <QtWaylandCompositor/private/qwaylandcompositorextension_p.h>
#include <QtWaylandCompositor/private/qwayland-server-text-input-unstable-v2.h>
#include <QtWaylandCompositor/QWaylandDestroyListener>

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QRect>
#include <QtCore/QVector>
#include <QtGui/QInputMethod>
#include <QtWaylandCompositor/QWaylandSurface>

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

QT_BEGIN_NAMESPACE

class QInputMethodEvent;
class QKeyEvent;
class QWaylandCompositor;
class QWaylandView;

class QWaylandTextInputClientState {
public:
    QWaylandTextInputClientState();

    Qt::InputMethodQueries updatedQueries(const QWaylandTextInputClientState &other) const;
    Qt::InputMethodQueries mergeChanged(const QWaylandTextInputClientState &other);

    Qt::InputMethodHints hints;
    QRect cursorRectangle;
    QString surroundingText;
    int cursorPosition;
    int anchorPosition;
    QString preferredLanguage;

    Qt::InputMethodQueries changedState;
};

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandTextInputPrivate : public QWaylandCompositorExtensionPrivate, public QtWaylandServer::zwp_text_input_v2
{
    Q_DECLARE_PUBLIC(QWaylandTextInput)
public:
    explicit QWaylandTextInputPrivate(QWaylandCompositor *compositor);

    void sendInputMethodEvent(QInputMethodEvent *event);
    void sendKeyEvent(QKeyEvent *event);
    void sendInputPanelState();
    void sendTextDirection();
    void sendLocale();

    QVariant inputMethodQuery(Qt::InputMethodQuery property, QVariant argument) const;

    void setFocus(QWaylandSurface *surface);

    QWaylandCompositor *compositor;

    QWaylandSurface *focus;
    Resource *focusResource;
    QWaylandDestroyListener focusDestroyListener;

    bool inputPanelVisible;

    QScopedPointer<QWaylandTextInputClientState> currentState;
    QScopedPointer<QWaylandTextInputClientState> pendingState;

    uint32_t serial;

    QHash<Resource *, QWaylandSurface*> enabledSurfaces;

protected:
    void zwp_text_input_v2_bind_resource(Resource *resource) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_destroy_resource(Resource *resource) Q_DECL_OVERRIDE;

    void zwp_text_input_v2_destroy(Resource *resource) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_enable(Resource *resource, wl_resource *surface) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_disable(Resource *resource, wl_resource *surface) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_show_input_panel(Resource *resource) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_hide_input_panel(Resource *resource) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_set_surrounding_text(Resource *resource, const QString &text, int32_t cursor, int32_t anchor) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_set_content_type(Resource *resource, uint32_t hint, uint32_t purpose) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_set_cursor_rectangle(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_set_preferred_language(Resource *resource, const QString &language) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_update_state(Resource *resource, uint32_t serial, uint32_t flags) Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE

#endif // QWAYLANDTEXTINPUT_P_H
