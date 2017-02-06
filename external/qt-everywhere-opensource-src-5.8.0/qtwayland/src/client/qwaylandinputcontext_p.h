/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWaylandClient module of the Qt Toolkit.
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


#ifndef QWAYLANDINPUTCONTEXT_H
#define QWAYLANDINPUTCONTEXT_H

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

#include <qpa/qplatforminputcontext.h>

#include <QLoggingCategory>
#include <QPointer>
#include <QRectF>
#include <QVector>

#include <QtWaylandClient/private/qwayland-text-input-unstable-v2.h>
#include <qwaylandinputmethodeventbuilder_p.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(qLcQpaInputMethods)

namespace QtWaylandClient {

class QWaylandDisplay;

class QWaylandTextInput : public QtWayland::zwp_text_input_v2
{
public:
    QWaylandTextInput(QWaylandDisplay *display, struct ::zwp_text_input_v2 *text_input);
    ~QWaylandTextInput();

    void reset();
    void commit();
    void updateState(Qt::InputMethodQueries queries, uint32_t flags);

    void setCursorInsidePreedit(int cursor);

    bool isInputPanelVisible() const;
    QRectF keyboardRect() const;

    QLocale locale() const;
    Qt::LayoutDirection inputDirection() const;

protected:
    void zwp_text_input_v2_enter(uint32_t serial, struct ::wl_surface *surface) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_leave(uint32_t serial, struct ::wl_surface *surface) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_modifiers_map(wl_array *map) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_input_panel_state(uint32_t state, int32_t x, int32_t y, int32_t width, int32_t height) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_preedit_string(const QString &text, const QString &commit) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_preedit_styling(uint32_t index, uint32_t length, uint32_t style) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_preedit_cursor(int32_t index) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_commit_string(const QString &text) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_cursor_position(int32_t index, int32_t anchor) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_delete_surrounding_text(uint32_t before_length, uint32_t after_length) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_keysym(uint32_t time, uint32_t sym, uint32_t state, uint32_t modifiers) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_language(const QString &language) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_text_direction(uint32_t direction) Q_DECL_OVERRIDE;
    void zwp_text_input_v2_input_method_changed(uint32_t serial, uint32_t flags) Q_DECL_OVERRIDE;

private:
    Qt::KeyboardModifiers modifiersToQtModifiers(uint32_t modifiers);

    QWaylandDisplay *m_display;
    QWaylandInputMethodEventBuilder m_builder;

    QVector<Qt::KeyboardModifier> m_modifiersMap;

    uint32_t m_serial;
    struct ::wl_surface *m_surface;

    QString m_preeditCommit;

    bool m_inputPanelVisible;
    QRectF m_keyboardRectangle;
    QLocale m_locale;
    Qt::LayoutDirection m_inputDirection;

    struct ::wl_callback *m_resetCallback;
    static const wl_callback_listener callbackListener;
    static void resetCallback(void *data, struct wl_callback *wl_callback, uint32_t time);
};

class QWaylandInputContext : public QPlatformInputContext
{
    Q_OBJECT
public:
    explicit QWaylandInputContext(QWaylandDisplay *display);
    ~QWaylandInputContext();

    bool isValid() const Q_DECL_OVERRIDE;

    void reset() Q_DECL_OVERRIDE;
    void commit() Q_DECL_OVERRIDE;
    void update(Qt::InputMethodQueries) Q_DECL_OVERRIDE;

    void invokeAction(QInputMethod::Action, int cursorPosition) Q_DECL_OVERRIDE;

    void showInputPanel() Q_DECL_OVERRIDE;
    void hideInputPanel() Q_DECL_OVERRIDE;
    bool isInputPanelVisible() const Q_DECL_OVERRIDE;
    QRectF keyboardRect() const Q_DECL_OVERRIDE;

    QLocale locale() const Q_DECL_OVERRIDE;
    Qt::LayoutDirection inputDirection() const Q_DECL_OVERRIDE;

    void setFocusObject(QObject *object) Q_DECL_OVERRIDE;

private:
    QWaylandTextInput *textInput() const;

    QWaylandDisplay *mDisplay;
    QPointer<QWindow> mCurrentWindow;
};

}

QT_END_NAMESPACE

#endif // QWAYLANDINPUTCONTEXT_H
