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


#include "qwaylandinputcontext_p.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QTextCharFormat>
#include <QtGui/QWindow>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformintegration.h>

#include "qwaylanddisplay_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylandinputmethodeventbuilder_p.h"
#include "qwaylandwindow_p.h"
#include "qwaylandxkb_p.h"

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qLcQpaInputMethods, "qt.qpa.input.methods")

namespace QtWaylandClient {

namespace {
const Qt::InputMethodQueries supportedQueries = Qt::ImEnabled |
                                                Qt::ImSurroundingText |
                                                Qt::ImCursorPosition |
                                                Qt::ImAnchorPosition |
                                                Qt::ImHints |
                                                Qt::ImCursorRectangle |
                                                Qt::ImPreferredLanguage;
}

QWaylandTextInput::QWaylandTextInput(QWaylandDisplay *display, struct ::zwp_text_input_v2 *text_input)
    : QtWayland::zwp_text_input_v2(text_input)
    , m_display(display)
    , m_builder()
    , m_serial(0)
    , m_surface(nullptr)
    , m_preeditCommit()
    , m_inputPanelVisible(false)
    , m_keyboardRectangle()
    , m_locale()
    , m_inputDirection(Qt::LayoutDirectionAuto)
    , m_resetCallback(nullptr)
{
}

QWaylandTextInput::~QWaylandTextInput()
{
    if (m_resetCallback)
        wl_callback_destroy(m_resetCallback);
}

void QWaylandTextInput::reset()
{
    m_builder.reset();
    m_preeditCommit = QString();
    updateState(Qt::ImQueryAll, update_state_reset);
}

void QWaylandTextInput::commit()
{
    if (QObject *o = QGuiApplication::focusObject()) {
        QInputMethodEvent event;
        event.setCommitString(m_preeditCommit);
        QCoreApplication::sendEvent(o, &event);
    }

    reset();
}

const wl_callback_listener QWaylandTextInput::callbackListener = {
    QWaylandTextInput::resetCallback
};

void QWaylandTextInput::resetCallback(void *data, wl_callback *, uint32_t)
{
    QWaylandTextInput *self = static_cast<QWaylandTextInput*>(data);

    if (self->m_resetCallback) {
        wl_callback_destroy(self->m_resetCallback);
        self->m_resetCallback = nullptr;
    }
}

void QWaylandTextInput::updateState(Qt::InputMethodQueries queries, uint32_t flags)
{
    if (!QGuiApplication::focusObject())
        return;

    if (!QGuiApplication::focusWindow() || !QGuiApplication::focusWindow()->handle())
        return;

    struct ::wl_surface *surface = static_cast<QWaylandWindow *>(QGuiApplication::focusWindow()->handle())->object();
    if (!surface || (surface != m_surface))
        return;

    queries &= supportedQueries;

    // Surrounding text, cursor and anchor positions are transferred together
    if ((queries & Qt::ImSurroundingText) || (queries & Qt::ImCursorPosition) || (queries & Qt::ImAnchorPosition))
        queries |= Qt::ImSurroundingText | Qt::ImCursorPosition | Qt::ImAnchorPosition;

    QInputMethodQueryEvent event(queries);
    QCoreApplication::sendEvent(QGuiApplication::focusObject(), &event);

    if ((queries & Qt::ImSurroundingText) || (queries & Qt::ImCursorPosition) || (queries & Qt::ImAnchorPosition)) {
        QString text = event.value(Qt::ImSurroundingText).toString();
        int cursor = event.value(Qt::ImCursorPosition).toInt();
        int anchor = event.value(Qt::ImAnchorPosition).toInt();

        // Make sure text is not too big
        if (text.toUtf8().size() > 2048) {
            int c = qAbs(cursor - anchor) <= 512 ? qMin(cursor, anchor) + qAbs(cursor - anchor) / 2: cursor;

            const int offset = c - qBound(0, c, 512 - qMin(text.size() - c, 256));
            text = text.mid(offset + c - 256, 512);
            cursor -= offset;
            anchor -= offset;
        }

        set_surrounding_text(text, QWaylandInputMethodEventBuilder::indexToWayland(text, cursor), QWaylandInputMethodEventBuilder::indexToWayland(text, anchor));
    }

    if (queries & Qt::ImHints) {
        QWaylandInputMethodContentType contentType = QWaylandInputMethodContentType::convert(static_cast<Qt::InputMethodHints>(event.value(Qt::ImHints).toInt()));
        set_content_type(contentType.hint, contentType.purpose);
    }

    if (queries & Qt::ImCursorRectangle) {
        const QRect &cRect = event.value(Qt::ImCursorRectangle).toRect();
        const QRect &tRect = QGuiApplication::inputMethod()->inputItemTransform().mapRect(cRect);
        set_cursor_rectangle(tRect.x(), tRect.y(), tRect.width(), tRect.height());
    }

    if (queries & Qt::ImPreferredLanguage) {
        const QString &language = event.value(Qt::ImPreferredLanguage).toString();
        set_preferred_language(language);
    }

    update_state(m_serial, flags);
    if (flags != update_state_change) {
        if (m_resetCallback)
            wl_callback_destroy(m_resetCallback);
        m_resetCallback = wl_display_sync(m_display->wl_display());
        wl_callback_add_listener(m_resetCallback, &QWaylandTextInput::callbackListener, this);
    }
}

void QWaylandTextInput::setCursorInsidePreedit(int)
{
    // Not supported yet
}

bool QWaylandTextInput::isInputPanelVisible() const
{
    return m_inputPanelVisible;
}

QRectF QWaylandTextInput::keyboardRect() const
{
    return m_keyboardRectangle;
}

QLocale QWaylandTextInput::locale() const
{
    return m_locale;
}

Qt::LayoutDirection QWaylandTextInput::inputDirection() const
{
    return m_inputDirection;
}

void QWaylandTextInput::zwp_text_input_v2_enter(uint32_t serial, ::wl_surface *surface)
{
    m_serial = serial;
    m_surface = surface;

    updateState(Qt::ImQueryAll, update_state_enter);
}

void QWaylandTextInput::zwp_text_input_v2_leave(uint32_t serial, ::wl_surface *surface)
{
    m_serial = serial;

    if (m_surface != surface) {
        qCDebug(qLcQpaInputMethods()) << Q_FUNC_INFO << "Got leave event for surface" << surface << "focused surface" << m_surface;
    }

    m_surface = nullptr;
}

void QWaylandTextInput::zwp_text_input_v2_modifiers_map(wl_array *map)
{
    QList<QByteArray> modifiersMap = QByteArray::fromRawData(static_cast<const char*>(map->data), map->size).split('\0');

    m_modifiersMap.clear();

    Q_FOREACH (const QByteArray &modifier, modifiersMap) {
        if (modifier == "Shift")
            m_modifiersMap.append(Qt::ShiftModifier);
        else if (modifier == "Control")
            m_modifiersMap.append(Qt::ControlModifier);
        else if (modifier == "Alt")
            m_modifiersMap.append(Qt::AltModifier);
        else if (modifier == "Mod1")
            m_modifiersMap.append(Qt::AltModifier);
        else if (modifier == "Mod4")
            m_modifiersMap.append(Qt::MetaModifier);
        else
            m_modifiersMap.append(Qt::NoModifier);
    }
}

void QWaylandTextInput::zwp_text_input_v2_input_panel_state(uint32_t visible, int32_t x, int32_t y, int32_t width, int32_t height)
{
    const bool inputPanelVisible = (visible == input_panel_visibility_visible);
    if (m_inputPanelVisible != inputPanelVisible) {
        m_inputPanelVisible = inputPanelVisible;
        QGuiApplicationPrivate::platformIntegration()->inputContext()->emitInputPanelVisibleChanged();
    }
    const QRectF keyboardRectangle(x, y, width, height);
    if (m_keyboardRectangle != keyboardRectangle) {
        m_keyboardRectangle = keyboardRectangle;
        QGuiApplicationPrivate::platformIntegration()->inputContext()->emitKeyboardRectChanged();
    }
}

void QWaylandTextInput::zwp_text_input_v2_preedit_string(const QString &text, const QString &commit)
{
    if (m_resetCallback) {
        qCDebug(qLcQpaInputMethods()) << "discard preedit_string: reset not confirmed";
        m_builder.reset();
        return;
    }

    if (!QGuiApplication::focusObject())
        return;

    QInputMethodEvent event = m_builder.buildPreedit(text);

    m_builder.reset();
    m_preeditCommit = commit;

    QCoreApplication::sendEvent(QGuiApplication::focusObject(), &event);
}

void QWaylandTextInput::zwp_text_input_v2_preedit_styling(uint32_t index, uint32_t length, uint32_t style)
{
    m_builder.addPreeditStyling(index, length, style);
}

void QWaylandTextInput::zwp_text_input_v2_preedit_cursor(int32_t index)
{
    m_builder.setPreeditCursor(index);
}

void QWaylandTextInput::zwp_text_input_v2_commit_string(const QString &text)
{
    if (m_resetCallback) {
        qCDebug(qLcQpaInputMethods()) << "discard commit_string: reset not confirmed";
        m_builder.reset();
        return;
    }

    if (!QGuiApplication::focusObject())
        return;

    QInputMethodEvent event = m_builder.buildCommit(text);

    m_builder.reset();

    QCoreApplication::sendEvent(QGuiApplication::focusObject(), &event);
}

void QWaylandTextInput::zwp_text_input_v2_cursor_position(int32_t index, int32_t anchor)
{
    m_builder.setCursorPosition(index, anchor);
}

void QWaylandTextInput::zwp_text_input_v2_delete_surrounding_text(uint32_t before_length, uint32_t after_length)
{
    m_builder.setDeleteSurroundingText(before_length, after_length);
}

void QWaylandTextInput::zwp_text_input_v2_keysym(uint32_t time, uint32_t sym, uint32_t state, uint32_t modifiers)
{
    if (m_resetCallback) {
        qCDebug(qLcQpaInputMethods()) << "discard keysym: reset not confirmed";
        return;
    }

    if (!QGuiApplication::focusWindow())
        return;

    Qt::KeyboardModifiers qtModifiers = modifiersToQtModifiers(modifiers);

    QEvent::Type type = QWaylandXkb::toQtEventType(state);
    QString text;
    int qtkey;
    std::tie(qtkey, text) = QWaylandXkb::keysymToQtKey(sym, qtModifiers);

    QWindowSystemInterface::handleKeyEvent(QGuiApplication::focusWindow(),
                                           time, type, qtkey, qtModifiers, text);
}

void QWaylandTextInput::zwp_text_input_v2_language(const QString &language)
{
    if (m_resetCallback) {
        qCDebug(qLcQpaInputMethods()) << "discard language: reset not confirmed";
        return;
    }

    const QLocale locale(language);
    if (m_locale != locale) {
        m_locale = locale;
        QGuiApplicationPrivate::platformIntegration()->inputContext()->emitLocaleChanged();
    }
}

void QWaylandTextInput::zwp_text_input_v2_text_direction(uint32_t direction)
{
    if (m_resetCallback) {
        qCDebug(qLcQpaInputMethods()) << "discard text_direction: reset not confirmed";
        return;
    }

    const Qt::LayoutDirection inputDirection = (direction == text_direction_auto) ? Qt::LayoutDirectionAuto :
                                               (direction == text_direction_ltr) ? Qt::LeftToRight :
                                               (direction == text_direction_rtl) ? Qt::RightToLeft : Qt::LayoutDirectionAuto;
    if (m_inputDirection != inputDirection) {
        m_inputDirection = inputDirection;
        QGuiApplicationPrivate::platformIntegration()->inputContext()->emitInputDirectionChanged(m_inputDirection);
    }
}

void QWaylandTextInput::zwp_text_input_v2_input_method_changed(uint32_t serial, uint32_t flags)
{
    Q_UNUSED(flags);

    m_serial = serial;
    updateState(Qt::ImQueryAll, update_state_full);
}

Qt::KeyboardModifiers QWaylandTextInput::modifiersToQtModifiers(uint32_t modifiers)
{
    Qt::KeyboardModifiers ret = Qt::NoModifier;
    for (int i = 0; modifiers >>= 1; ++i) {
        ret |= m_modifiersMap[i];
    }
    return ret;
}

QWaylandInputContext::QWaylandInputContext(QWaylandDisplay *display)
    : QPlatformInputContext()
    , mDisplay(display)
    , mCurrentWindow()
{
}

QWaylandInputContext::~QWaylandInputContext()
{
}

bool QWaylandInputContext::isValid() const
{
    return mDisplay->textInputManager() != 0;
}

void QWaylandInputContext::reset()
{
    qCDebug(qLcQpaInputMethods) << Q_FUNC_INFO;

    QPlatformInputContext::reset();

    if (!textInput())
        return;

    textInput()->reset();
}

void QWaylandInputContext::commit()
{
    qCDebug(qLcQpaInputMethods) << Q_FUNC_INFO;

    if (!textInput())
        return;

    textInput()->commit();
}

void QWaylandInputContext::update(Qt::InputMethodQueries queries)
{
    qCDebug(qLcQpaInputMethods) << Q_FUNC_INFO << queries;

    if (!QGuiApplication::focusObject() || !textInput())
        return;

    if (mCurrentWindow && mCurrentWindow->handle() && !inputMethodAccepted()) {
        struct ::wl_surface *surface = static_cast<QWaylandWindow *>(mCurrentWindow->handle())->object();
        textInput()->disable(surface);
        mCurrentWindow.clear();
    } else if (!mCurrentWindow && inputMethodAccepted()) {
        QWindow *window = QGuiApplication::focusWindow();
        if (window && window->handle()) {
            struct ::wl_surface *surface = static_cast<QWaylandWindow *>(window->handle())->object();
            textInput()->enable(surface);
            mCurrentWindow = window;
        }
    }

    textInput()->updateState(queries, QtWayland::zwp_text_input_v2::update_state_change);
}

void QWaylandInputContext::invokeAction(QInputMethod::Action action, int cursorPostion)
{
    if (!textInput())
        return;

    if (action == QInputMethod::Click)
        textInput()->setCursorInsidePreedit(cursorPostion);
}

void QWaylandInputContext::showInputPanel()
{
    qCDebug(qLcQpaInputMethods) << Q_FUNC_INFO;

    if (!textInput())
        return;

    textInput()->show_input_panel();
}

void QWaylandInputContext::hideInputPanel()
{
    qCDebug(qLcQpaInputMethods) << Q_FUNC_INFO;

    if (!textInput())
        return;

    textInput()->hide_input_panel();
}

bool QWaylandInputContext::isInputPanelVisible() const
{
    qCDebug(qLcQpaInputMethods) << Q_FUNC_INFO;

    if (!textInput())
        return QPlatformInputContext::isInputPanelVisible();

    return textInput()->isInputPanelVisible();
}

QRectF QWaylandInputContext::keyboardRect() const
{
    qCDebug(qLcQpaInputMethods) << Q_FUNC_INFO;

    if (!textInput())
        return QPlatformInputContext::keyboardRect();

    return textInput()->keyboardRect();
}

QLocale QWaylandInputContext::locale() const
{
    qCDebug(qLcQpaInputMethods) << Q_FUNC_INFO;

    if (!textInput())
        return QPlatformInputContext::locale();

    return textInput()->locale();
}

Qt::LayoutDirection QWaylandInputContext::inputDirection() const
{
    qCDebug(qLcQpaInputMethods) << Q_FUNC_INFO;

    if (!textInput())
        return QPlatformInputContext::inputDirection();

    return textInput()->inputDirection();
}

void QWaylandInputContext::setFocusObject(QObject *)
{
    qCDebug(qLcQpaInputMethods) << Q_FUNC_INFO;

    if (!textInput())
        return;

    QWindow *window = QGuiApplication::focusWindow();

    if (mCurrentWindow && mCurrentWindow->handle()) {
        if (mCurrentWindow.data() != window || !inputMethodAccepted()) {
            struct ::wl_surface *surface = static_cast<QWaylandWindow *>(mCurrentWindow->handle())->object();
            if (surface)
                textInput()->disable(surface);
            mCurrentWindow.clear();
        }
    }

    if (window && window->handle() && inputMethodAccepted()) {
        if (mCurrentWindow.data() != window) {
            struct ::wl_surface *surface = static_cast<QWaylandWindow *>(window->handle())->object();
            textInput()->enable(surface);
            mCurrentWindow = window;
        }
        textInput()->updateState(Qt::ImQueryAll, QtWayland::zwp_text_input_v2::update_state_enter);
    }
}

QWaylandTextInput *QWaylandInputContext::textInput() const
{
    return mDisplay->defaultInputDevice()->textInput();
}

}

QT_END_NAMESPACE
