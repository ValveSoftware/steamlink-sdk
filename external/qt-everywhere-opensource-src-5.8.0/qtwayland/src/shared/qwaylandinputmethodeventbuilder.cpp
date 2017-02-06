/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qwaylandinputmethodeventbuilder_p.h"

#include <QInputMethod>
#include <QTextCharFormat>

#ifdef QT_BUILD_WAYLANDCOMPOSITOR_LIB
#include <QtWaylandCompositor/private/qwayland-server-text-input-unstable-v2.h>
#else
#include <QtWaylandClient/private/qwayland-text-input-unstable-v2.h>
#endif

QT_BEGIN_NAMESPACE

QWaylandInputMethodEventBuilder::QWaylandInputMethodEventBuilder()
    : m_anchor(0)
    , m_cursor(0)
    , m_deleteBefore(0)
    , m_deleteAfter(0)
    , m_preeditCursor(0)
    , m_preeditStyles()
{
}

QWaylandInputMethodEventBuilder::~QWaylandInputMethodEventBuilder()
{
}

void QWaylandInputMethodEventBuilder::reset()
{
    m_anchor = 0;
    m_cursor = 0;
    m_deleteBefore = 0;
    m_deleteAfter = 0;
    m_preeditCursor = 0;
    m_preeditStyles.clear();
}

void QWaylandInputMethodEventBuilder::setCursorPosition(int32_t index, int32_t anchor)
{
    m_cursor = index;
    m_anchor = anchor;
}

void QWaylandInputMethodEventBuilder::setDeleteSurroundingText(uint32_t beforeLength, uint32_t afterLength)
{
    m_deleteBefore = beforeLength;
    m_deleteAfter = afterLength;
}

void QWaylandInputMethodEventBuilder::addPreeditStyling(uint32_t index, uint32_t length, uint32_t style)
{
    QTextCharFormat format;

    switch (style) {
    case 0:
    case 1:
        format.setFontUnderline(true);
        format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        m_preeditStyles.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, index, length, format));
        break;
    case 2:
    case 3:
        format.setFontWeight(QFont::Bold);
        format.setFontUnderline(true);
        format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        m_preeditStyles.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, index, length, format));
        break;
    case 4:
        format.setFontUnderline(true);
        format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        m_preeditStyles.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, index, length, format));
    case 5:
        format.setFontUnderline(true);
        format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        format.setUnderlineColor(QColor(Qt::red));
        m_preeditStyles.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, index, length, format));
        break;
//    case QtWayland::wl_text_input::preedit_style_selection:
//    case QtWayland::wl_text_input::preedit_style_none:
    default:
        break;
    }
}

void QWaylandInputMethodEventBuilder::setPreeditCursor(int32_t index)
{
    m_preeditCursor = index;
}

QInputMethodEvent QWaylandInputMethodEventBuilder::buildCommit(const QString &text)
{
    QList<QInputMethodEvent::Attribute> attributes;

    const QPair<int, int> replacement = replacementForDeleteSurrounding();

    if (m_cursor != 0 || m_anchor != 0) {
        QString surrounding = QInputMethod::queryFocusObject(Qt::ImSurroundingText, QVariant()).toString();
        const int cursor = QInputMethod::queryFocusObject(Qt::ImCursorPosition, QVariant()).toInt();
        const int anchor = QInputMethod::queryFocusObject(Qt::ImAnchorPosition, QVariant()).toInt();
        const int absoluteCursor = QInputMethod::queryFocusObject(Qt::ImAbsolutePosition, QVariant()).toInt();

        const int absoluteOffset = absoluteCursor - cursor;

        const int cursorAfterCommit = qMin(anchor, cursor) + replacement.first + text.length();
        surrounding.replace(qMin(anchor, cursor) + replacement.first,
                            qAbs(anchor - cursor) + replacement.second, text);

        attributes.push_back(QInputMethodEvent::Attribute(QInputMethodEvent::Selection,
                                                          indexFromWayland(surrounding, m_cursor, cursorAfterCommit) + absoluteOffset,
                                                          indexFromWayland(surrounding, m_anchor, cursorAfterCommit) + absoluteOffset,
                                                          QVariant()));
    }

    QInputMethodEvent event(QString(), attributes);
    event.setCommitString(text, replacement.first, replacement.second);

    return event;
}

QInputMethodEvent QWaylandInputMethodEventBuilder::buildPreedit(const QString &text)
{
    QList<QInputMethodEvent::Attribute> attributes;

    if (m_preeditCursor < 0) {
        attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, 0, QVariant()));
    } else if (m_preeditCursor > 0) {
        attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, indexFromWayland(text, m_preeditCursor), 1, QVariant()));
    }

    Q_FOREACH (const QInputMethodEvent::Attribute &attr, m_preeditStyles) {
        int start = indexFromWayland(text, attr.start);
        int length = indexFromWayland(text, attr.start + attr.length) - start;
        attributes.append(QInputMethodEvent::Attribute(attr.type, start, length, attr.value));
    }

    QInputMethodEvent event(text, attributes);

    const QPair<int, int> replacement = replacementForDeleteSurrounding();
    event.setCommitString(QString(), replacement.first, replacement.second);

    return event;
}

QPair<int, int> QWaylandInputMethodEventBuilder::replacementForDeleteSurrounding()
{
    if (m_deleteBefore == 0 && m_deleteAfter == 0)
        return QPair<int, int>(0, 0);

    const QString &surrounding = QInputMethod::queryFocusObject(Qt::ImSurroundingText, QVariant()).toString();
    const int cursor = QInputMethod::queryFocusObject(Qt::ImCursorPosition, QVariant()).toInt();
    const int anchor = QInputMethod::queryFocusObject(Qt::ImAnchorPosition, QVariant()).toInt();

    const int selectionStart = qMin(cursor, anchor);
    const int selectionEnd = qMax(cursor, anchor);

    const int deleteBefore = selectionStart - indexFromWayland(surrounding, -m_deleteBefore, selectionStart);
    const int deleteAfter = indexFromWayland(surrounding, m_deleteAfter, selectionEnd) - selectionEnd;

    return QPair<int, int>(-deleteBefore, deleteBefore + deleteAfter);
}

QWaylandInputMethodContentType QWaylandInputMethodContentType::convert(Qt::InputMethodHints hints)
{
    uint32_t hint = ZWP_TEXT_INPUT_V2_CONTENT_HINT_NONE;
    uint32_t purpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_NORMAL;

    if (hints & Qt::ImhHiddenText) {
        hint |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_HIDDEN_TEXT;
    }
    if (hints & Qt::ImhSensitiveData) {
        hint |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_SENSITIVE_DATA;
    }
    if ((hints & Qt::ImhNoAutoUppercase) == 0) {
        hint |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_AUTO_CAPITALIZATION;
    }
    if (hints & Qt::ImhPreferNumbers) {
        // Nothing yet
    }
    if (hints & Qt::ImhPreferUppercase) {
        hint |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_UPPERCASE;
    }
    if (hints & Qt::ImhPreferLowercase) {
        hint |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_LOWERCASE;
    }
    if ((hints & Qt::ImhNoPredictiveText) == 0) {
        hint |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_AUTO_COMPLETION | ZWP_TEXT_INPUT_V2_CONTENT_HINT_AUTO_CORRECTION;
    }

    if ((hints & Qt::ImhDate) && (hints & Qt::ImhTime) == 0) {
        purpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_DATE;
    } else if ((hints & Qt::ImhDate) && (hints & Qt::ImhTime)) {
        purpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_DATETIME;
    } else if ((hints & Qt::ImhDate) == 0 && (hints & Qt::ImhTime)) {
        purpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_TIME;
    }

    if (hints & Qt::ImhPreferLatin) {
        hint |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_LATIN;
    }

    if (hints & Qt::ImhMultiLine) {
        hint |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_MULTILINE;
    }

    if (hints & Qt::ImhDigitsOnly) {
        purpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_DIGITS;
    }
    if (hints & Qt::ImhFormattedNumbersOnly) {
        purpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_NUMBER;
    }
    if (hints & Qt::ImhUppercaseOnly) {
        hint |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_UPPERCASE;
    }
    if (hints & Qt::ImhLowercaseOnly) {
        hint |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_LOWERCASE;
    }
    if (hints & Qt::ImhDialableCharactersOnly) {
        purpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_PHONE;
    }
    if (hints & Qt::ImhEmailCharactersOnly) {
       purpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_EMAIL;
    }
    if (hints & Qt::ImhUrlCharactersOnly) {
       purpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_URL;
    }
    if (hints & Qt::ImhLatinOnly) {
        hint |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_LATIN;
    }
    return QWaylandInputMethodContentType{hint, purpose};
}

int QWaylandInputMethodEventBuilder::indexFromWayland(const QString &text, int length, int base)
{
    if (length == 0)
        return base;

    if (length < 0) {
        const QByteArray &utf8 = text.leftRef(base).toUtf8();
        return QString::fromUtf8(utf8.left(qMax(utf8.length() + length, 0))).length();
    } else {
        const QByteArray &utf8 = text.midRef(base).toUtf8();
        return QString::fromUtf8(utf8.left(length)).length() + base;
    }
}

int QWaylandInputMethodEventBuilder::indexToWayland(const QString &text, int length, int base)
{
    return text.midRef(base, length).toUtf8().size();
}

QT_END_NAMESPACE

