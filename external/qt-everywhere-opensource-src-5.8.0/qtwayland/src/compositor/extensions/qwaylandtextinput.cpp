/****************************************************************************
**
** Copyright (C) 2013-2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "qwaylandtextinput.h"
#include "qwaylandtextinput_p.h"

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/private/qwaylandseat_p.h>

#include "qwaylandsurface.h"
#include "qwaylandview.h"
#include "qwaylandxkb_p.h"
#include "qwaylandinputmethodeventbuilder_p.h"

#include <QGuiApplication>
#include <QInputMethodEvent>

QT_BEGIN_NAMESPACE

QWaylandTextInputClientState::QWaylandTextInputClientState()
    : hints(0)
    , cursorRectangle()
    , surroundingText()
    , cursorPosition(0)
    , anchorPosition(0)
    , preferredLanguage()
    , changedState()
{
}

Qt::InputMethodQueries QWaylandTextInputClientState::updatedQueries(const QWaylandTextInputClientState &other) const
{
    Qt::InputMethodQueries queries;

    if (hints != other.hints)
        queries |= Qt::ImHints;
    if (cursorRectangle != other.cursorRectangle)
        queries |= Qt::ImCursorRectangle;
    if (surroundingText != other.surroundingText)
        queries |= Qt::ImSurroundingText | Qt::ImCurrentSelection;
    if (cursorPosition != other.cursorPosition)
        queries |= Qt::ImCursorPosition | Qt::ImCurrentSelection;
    if (anchorPosition != other.anchorPosition)
        queries |= Qt::ImAnchorPosition | Qt::ImCurrentSelection;
    if (preferredLanguage != other.preferredLanguage)
        queries |= Qt::ImPreferredLanguage;

    return queries;
}

Qt::InputMethodQueries QWaylandTextInputClientState::mergeChanged(const QWaylandTextInputClientState &other) {
    Qt::InputMethodQueries queries;

    if ((other.changedState & Qt::ImHints) && hints != other.hints) {
        hints = other.hints;
        queries |= Qt::ImHints;
    }

    if ((other.changedState & Qt::ImCursorRectangle) && cursorRectangle != other.cursorRectangle) {
        cursorRectangle = other.cursorRectangle;
        queries |= Qt::ImCursorRectangle;
    }

    if ((other.changedState & Qt::ImSurroundingText) && surroundingText != other.surroundingText) {
        surroundingText = other.surroundingText;
        queries |= Qt::ImSurroundingText | Qt::ImCurrentSelection;
    }

    if ((other.changedState & Qt::ImCursorPosition) && cursorPosition != other.cursorPosition) {
        cursorPosition = other.cursorPosition;
        queries |= Qt::ImCursorPosition | Qt::ImCurrentSelection;
    }

    if ((other.changedState & Qt::ImAnchorPosition) && anchorPosition != other.anchorPosition) {
        anchorPosition = other.anchorPosition;
        queries |= Qt::ImAnchorPosition | Qt::ImCurrentSelection;
    }

    if ((other.changedState & Qt::ImPreferredLanguage) && preferredLanguage != other.preferredLanguage) {
        preferredLanguage = other.preferredLanguage;
        queries |= Qt::ImPreferredLanguage;
    }

    return queries;
}

QWaylandTextInputPrivate::QWaylandTextInputPrivate(QWaylandCompositor *compositor)
    : QWaylandCompositorExtensionPrivate()
    , QtWaylandServer::zwp_text_input_v2()
    , compositor(compositor)
    , focus(nullptr)
    , focusResource(nullptr)
    , focusDestroyListener()
    , inputPanelVisible(false)
    , currentState(new QWaylandTextInputClientState)
    , pendingState(new QWaylandTextInputClientState)
    , serial(0)
    , enabledSurfaces()
{
}

void QWaylandTextInputPrivate::sendInputMethodEvent(QInputMethodEvent *event)
{
    Q_Q(QWaylandTextInput);

    if (!focusResource || !focusResource->handle)
        return;

    QWaylandTextInputClientState afterCommit;

    afterCommit.surroundingText = currentState->surroundingText;
    afterCommit.cursorPosition = qMin(currentState->cursorPosition, currentState->anchorPosition);

    // Remove selection
    afterCommit.surroundingText.remove(afterCommit.cursorPosition, qAbs(currentState->cursorPosition - currentState->anchorPosition));

    if (event->replacementLength() > 0 || event->replacementStart() != 0) {
        // Remove replacement
        afterCommit.cursorPosition = qBound(0, afterCommit.cursorPosition + event->replacementStart(), afterCommit.surroundingText.length());
        afterCommit.surroundingText.remove(afterCommit.cursorPosition,
                                           qMin(event->replacementLength(),
                                                afterCommit.surroundingText.length() - afterCommit.cursorPosition));

        if (event->replacementStart() <= 0 && (event->replacementLength() >= -event->replacementStart())) {
            const int selectionStart = qMin(currentState->cursorPosition, currentState->anchorPosition);
            const int selectionEnd = qMax(currentState->cursorPosition, currentState->anchorPosition);
            const int before = QWaylandInputMethodEventBuilder::indexToWayland(currentState->surroundingText, -event->replacementStart(), selectionStart + event->replacementStart());
            const int after = QWaylandInputMethodEventBuilder::indexToWayland(currentState->surroundingText, event->replacementLength() + event->replacementStart(), selectionEnd);
            send_delete_surrounding_text(focusResource->handle, before, after);
        } else {
            // TODO: Implement this case
            qWarning() << "Not yet supported case of replacement. Start:" << event->replacementStart() << "length:" << event->replacementLength();
        }
    }

    // Insert commit string
    afterCommit.surroundingText.insert(afterCommit.cursorPosition, event->commitString());
    afterCommit.cursorPosition += event->commitString().length();
    afterCommit.anchorPosition = afterCommit.cursorPosition;

    foreach (const QInputMethodEvent::Attribute &attribute, event->attributes()) {
        if (attribute.type == QInputMethodEvent::Selection) {
            afterCommit.cursorPosition = attribute.start;
            afterCommit.anchorPosition = attribute.length;
            int cursor = QWaylandInputMethodEventBuilder::indexToWayland(afterCommit.surroundingText, qAbs(attribute.start - afterCommit.cursorPosition), qMin(attribute.start, afterCommit.cursorPosition));
            int anchor = QWaylandInputMethodEventBuilder::indexToWayland(afterCommit.surroundingText, qAbs(attribute.length - afterCommit.cursorPosition), qMin(attribute.length, afterCommit.cursorPosition));
            send_cursor_position(focusResource->handle,
                                 attribute.start < afterCommit.cursorPosition ? -cursor : cursor,
                                 attribute.length < afterCommit.cursorPosition ? -anchor : anchor);
        }
    }
    send_commit_string(focusResource->handle, event->commitString());
    foreach (const QInputMethodEvent::Attribute &attribute, event->attributes()) {
        if (attribute.type == QInputMethodEvent::Cursor) {
            int index = QWaylandInputMethodEventBuilder::indexToWayland(event->preeditString(), attribute.start);
            send_preedit_cursor(focusResource->handle, index);
        } else if (attribute.type == QInputMethodEvent::TextFormat) {
            int start = QWaylandInputMethodEventBuilder::indexToWayland(event->preeditString(), attribute.start);
            int length = QWaylandInputMethodEventBuilder::indexToWayland(event->preeditString(), attribute.length, attribute.start);
            // TODO add support for different stylesQWaylandTextInput
            send_preedit_styling(focusResource->handle, start, length, preedit_style_default);
        }
    }
    send_preedit_string(focusResource->handle, event->preeditString(), event->preeditString());

    Qt::InputMethodQueries queries = currentState->updatedQueries(afterCommit);
    currentState->surroundingText = afterCommit.surroundingText;
    currentState->cursorPosition = afterCommit.cursorPosition;
    currentState->anchorPosition = afterCommit.anchorPosition;

    if (queries) {
        qCDebug(qLcCompositorInputMethods) << "QInputMethod::update() after QInputMethodEvent" << queries;

        emit q->updateInputMethod(queries);
    }
}

void QWaylandTextInputPrivate::sendKeyEvent(QKeyEvent *event)
{
    if (!focusResource || !focusResource->handle)
        return;

    // TODO add support for modifiers

    foreach (xkb_keysym_t keysym, QWaylandXkb::toKeysym(event)) {
        send_keysym(focusResource->handle, event->timestamp(), keysym,
                    event->type() == QEvent::KeyPress ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED,
                    0);
    }
}

void QWaylandTextInputPrivate::sendInputPanelState()
{
    if (!focusResource || !focusResource->handle)
        return;

    QInputMethod *inputMethod = qApp->inputMethod();
    const QRectF& keyboardRect = inputMethod->keyboardRectangle();
    const QRectF& sceneInputRect = inputMethod->inputItemTransform().mapRect(inputMethod->inputItemRectangle());
    const QRectF& localRect = sceneInputRect.intersected(keyboardRect).translated(-sceneInputRect.topLeft());

    send_input_panel_state(focusResource->handle,
                           inputMethod->isVisible() ? input_panel_visibility_visible : input_panel_visibility_hidden,
                           localRect.x(), localRect.y(), localRect.width(), localRect.height());
}

void QWaylandTextInputPrivate::sendTextDirection()
{
    if (!focusResource || !focusResource->handle)
        return;

    const Qt::LayoutDirection direction = qApp->inputMethod()->inputDirection();
    send_text_direction(focusResource->handle,
                        (direction == Qt::LeftToRight) ? text_direction_ltr :
                                                         (direction == Qt::RightToLeft) ? text_direction_rtl : text_direction_auto);
}

void QWaylandTextInputPrivate::sendLocale()
{
    if (!focusResource || !focusResource->handle)
        return;

    const QLocale locale = qApp->inputMethod()->locale();
    send_language(focusResource->handle, locale.bcp47Name());
}

QVariant QWaylandTextInputPrivate::inputMethodQuery(Qt::InputMethodQuery property, QVariant argument) const
{
    switch (property) {
    case Qt::ImHints:
        return QVariant(static_cast<int>(currentState->hints));
    case Qt::ImCursorRectangle:
        return currentState->cursorRectangle;
    case Qt::ImFont:
        // Not supported
        return QVariant();
    case Qt::ImCursorPosition:
        return currentState->cursorPosition;
    case Qt::ImSurroundingText:
        return currentState->surroundingText;
    case Qt::ImCurrentSelection:
        return currentState->surroundingText.mid(qMin(currentState->cursorPosition, currentState->anchorPosition),
                                                 qAbs(currentState->anchorPosition - currentState->cursorPosition));
    case Qt::ImMaximumTextLength:
        // Not supported
        return QVariant();
    case Qt::ImAnchorPosition:
        return currentState->anchorPosition;
    case Qt::ImAbsolutePosition:
        // We assume the surrounding text is our whole document for now
        return currentState->cursorPosition;
    case Qt::ImTextAfterCursor:
        if (argument.isValid())
            return currentState->surroundingText.mid(currentState->cursorPosition, argument.toInt());
        return currentState->surroundingText.mid(currentState->cursorPosition);
    case Qt::ImTextBeforeCursor:
        if (argument.isValid())
            return currentState->surroundingText.left(currentState->cursorPosition).right(argument.toInt());
        return currentState->surroundingText.left(currentState->cursorPosition);
    case Qt::ImPreferredLanguage:
        return currentState->preferredLanguage;

    default:
        return QVariant();
    }
}

void QWaylandTextInputPrivate::setFocus(QWaylandSurface *surface)
{
    Q_Q(QWaylandTextInput);

    if (focusResource && focus != surface) {
        uint32_t serial = compositor->nextSerial();
        send_leave(focusResource->handle, serial, focus->resource());
        focusDestroyListener.reset();
    }

    Resource *resource = surface ? resourceMap().value(surface->waylandClient()) : 0;

    if (resource && (focus != surface || focusResource != resource)) {
        uint32_t serial = compositor->nextSerial();
        currentState.reset(new QWaylandTextInputClientState);
        pendingState.reset(new QWaylandTextInputClientState);
        send_enter(resource->handle, serial, surface->resource());
        focusResource = resource;
        sendInputPanelState();
        sendLocale();
        sendTextDirection();
        focusDestroyListener.listenForDestruction(surface->resource());
        if (inputPanelVisible && q->isSurfaceEnabled(surface))
            qApp->inputMethod()->show();
    }

    focusResource = resource;
    focus = surface;
}

void QWaylandTextInputPrivate::zwp_text_input_v2_bind_resource(Resource *resource)
{
    send_modifiers_map(resource->handle, QByteArray(""));
}

void QWaylandTextInputPrivate::zwp_text_input_v2_destroy_resource(Resource *resource)
{
    if (focusResource == resource)
        focusResource = 0;
}

void QWaylandTextInputPrivate::zwp_text_input_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void QWaylandTextInputPrivate::zwp_text_input_v2_enable(Resource *resource, wl_resource *surface)
{
    Q_Q(QWaylandTextInput);

    QWaylandSurface *s = QWaylandSurface::fromResource(surface);
    enabledSurfaces.insert(resource, s);
    emit q->surfaceEnabled(s);
}

void QWaylandTextInputPrivate::zwp_text_input_v2_disable(QtWaylandServer::zwp_text_input_v2::Resource *resource, wl_resource *)
{
    Q_Q(QWaylandTextInput);

    QWaylandSurface *s = enabledSurfaces.take(resource);
    emit q->surfaceDisabled(s);
}

void QWaylandTextInputPrivate::zwp_text_input_v2_show_input_panel(Resource *)
{
    inputPanelVisible = true;

    qApp->inputMethod()->show();
}

void QWaylandTextInputPrivate::zwp_text_input_v2_hide_input_panel(Resource *)
{
    inputPanelVisible = false;

    qApp->inputMethod()->hide();
}

void QWaylandTextInputPrivate::zwp_text_input_v2_set_cursor_rectangle(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    if (resource != focusResource)
        return;

    pendingState->cursorRectangle = QRect(x, y, width, height);

    pendingState->changedState |= Qt::ImCursorRectangle;
}

void QWaylandTextInputPrivate::zwp_text_input_v2_update_state(Resource *resource, uint32_t serial, uint32_t flags)
{
    Q_Q(QWaylandTextInput);

    qCDebug(qLcCompositorInputMethods) << "update_state" << serial << flags;

    if (resource != focusResource)
        return;

    if (flags == update_state_reset || flags == update_state_enter) {
        qCDebug(qLcCompositorInputMethods) << "QInputMethod::reset()";
        qApp->inputMethod()->reset();
    }

    this->serial = serial;

    Qt::InputMethodQueries queries;
    if (flags == update_state_change) {
        queries = currentState->mergeChanged(*pendingState.data());
    } else {
        queries = pendingState->updatedQueries(*currentState.data());
        currentState.swap(pendingState);
    }

    pendingState.reset(new QWaylandTextInputClientState);

    if (queries) {
        qCDebug(qLcCompositorInputMethods) << "QInputMethod::update()" << queries;

        emit q->updateInputMethod(queries);
    }
}

void QWaylandTextInputPrivate::zwp_text_input_v2_set_content_type(Resource *resource, uint32_t hint, uint32_t purpose)
{
    if (resource != focusResource)
        return;

    pendingState->hints = 0;

    if ((hint & content_hint_auto_completion) == 0
        && (hint & content_hint_auto_correction) == 0)
        pendingState->hints |= Qt::ImhNoPredictiveText;
    if ((hint & content_hint_auto_capitalization) == 0)
        pendingState->hints |= Qt::ImhNoAutoUppercase;
    if ((hint & content_hint_lowercase) != 0)
        pendingState->hints |= Qt::ImhPreferLowercase;
    if ((hint & content_hint_uppercase) != 0)
        pendingState->hints |= Qt::ImhPreferUppercase;
    if ((hint & content_hint_hidden_text) != 0)
        pendingState->hints |= Qt::ImhHiddenText;
    if ((hint & content_hint_sensitive_data) != 0)
        pendingState->hints |= Qt::ImhSensitiveData;
    if ((hint & content_hint_latin) != 0)
        pendingState->hints |= Qt::ImhLatinOnly;
    if ((hint & content_hint_multiline) != 0)
        pendingState->hints |= Qt::ImhMultiLine;

    switch (purpose) {
    case content_purpose_normal:
        break;
    case content_purpose_alpha:
        pendingState->hints |= Qt::ImhUppercaseOnly | Qt::ImhLowercaseOnly;
        break;
    case content_purpose_digits:
        pendingState->hints |= Qt::ImhDigitsOnly;
        break;
    case content_purpose_number:
        pendingState->hints |= Qt::ImhFormattedNumbersOnly;
        break;
    case content_purpose_phone:
        pendingState->hints |= Qt::ImhDialableCharactersOnly;
        break;
    case content_purpose_url:
        pendingState->hints |= Qt::ImhUrlCharactersOnly;
        break;
    case content_purpose_email:
        pendingState->hints |= Qt::ImhEmailCharactersOnly;
        break;
    case content_purpose_name:
    case content_purpose_password:
        break;
    case content_purpose_date:
        pendingState->hints |= Qt::ImhDate;
        break;
    case content_purpose_time:
        pendingState->hints |= Qt::ImhTime;
        break;
    case content_purpose_datetime:
        pendingState->hints |= Qt::ImhDate | Qt::ImhTime;
        break;
    case content_purpose_terminal:
    default:
        break;
    }

    pendingState->changedState |= Qt::ImHints;
}

void QWaylandTextInputPrivate::zwp_text_input_v2_set_preferred_language(Resource *resource, const QString &language)
{
    if (resource != focusResource)
        return;

    pendingState->preferredLanguage = language;

    pendingState->changedState |= Qt::ImPreferredLanguage;
}

void QWaylandTextInputPrivate::zwp_text_input_v2_set_surrounding_text(Resource *resource, const QString &text, int32_t cursor, int32_t anchor)
{
    if (resource != focusResource)
        return;

    pendingState->surroundingText = text;
    pendingState->cursorPosition = QWaylandInputMethodEventBuilder::indexFromWayland(text, cursor);
    pendingState->anchorPosition = QWaylandInputMethodEventBuilder::indexFromWayland(text, anchor);

    pendingState->changedState |= Qt::ImSurroundingText | Qt::ImCursorPosition | Qt::ImAnchorPosition;
}

QWaylandTextInput::QWaylandTextInput(QWaylandObject *container, QWaylandCompositor *compositor)
    : QWaylandCompositorExtensionTemplate(container, *new QWaylandTextInputPrivate(compositor))
{
    connect(&d_func()->focusDestroyListener, &QWaylandDestroyListener::fired,
            this, &QWaylandTextInput::focusSurfaceDestroyed);

    connect(qApp->inputMethod(), &QInputMethod::visibleChanged,
            this, &QWaylandTextInput::sendInputPanelState);
    connect(qApp->inputMethod(), &QInputMethod::keyboardRectangleChanged,
            this, &QWaylandTextInput::sendInputPanelState);
    connect(qApp->inputMethod(), &QInputMethod::inputDirectionChanged,
            this, &QWaylandTextInput::sendTextDirection);
    connect(qApp->inputMethod(), &QInputMethod::localeChanged,
            this, &QWaylandTextInput::sendLocale);
}

QWaylandTextInput::~QWaylandTextInput()
{
}

void QWaylandTextInput::sendInputMethodEvent(QInputMethodEvent *event)
{
    Q_D(QWaylandTextInput);

    d->sendInputMethodEvent(event);
}

void QWaylandTextInput::sendKeyEvent(QKeyEvent *event)
{
    Q_D(QWaylandTextInput);

    d->sendKeyEvent(event);
}

void QWaylandTextInput::sendInputPanelState()
{
    Q_D(QWaylandTextInput);

    d->sendInputPanelState();
}

void QWaylandTextInput::sendTextDirection()
{
    Q_D(QWaylandTextInput);

    d->sendTextDirection();
}

void QWaylandTextInput::sendLocale()
{
    Q_D(QWaylandTextInput);

    d->sendLocale();
}

QVariant QWaylandTextInput::inputMethodQuery(Qt::InputMethodQuery property, QVariant argument) const
{
    const Q_D(QWaylandTextInput);

    return d->inputMethodQuery(property, argument);
}

QWaylandSurface *QWaylandTextInput::focus() const
{
    const Q_D(QWaylandTextInput);

    return d->focus;
}

void QWaylandTextInput::setFocus(QWaylandSurface *surface)
{
    Q_D(QWaylandTextInput);

    d->setFocus(surface);
}

void QWaylandTextInput::focusSurfaceDestroyed(void *)
{
    Q_D(QWaylandTextInput);

    d->focusDestroyListener.reset();

    d->focus = nullptr;
    d->focusResource = nullptr;
}

bool QWaylandTextInput::isSurfaceEnabled(QWaylandSurface *surface) const
{
    const Q_D(QWaylandTextInput);

    return d->enabledSurfaces.values().contains(surface);
}

void QWaylandTextInput::add(::wl_client *client, uint32_t id, int version)
{
    Q_D(QWaylandTextInput);

    d->add(client, id, version);
}

const wl_interface *QWaylandTextInput::interface()
{
    return QWaylandTextInputPrivate::interface();
}

QByteArray QWaylandTextInput::interfaceName()
{
    return QWaylandTextInputPrivate::interfaceName();
}

QT_END_NAMESPACE
