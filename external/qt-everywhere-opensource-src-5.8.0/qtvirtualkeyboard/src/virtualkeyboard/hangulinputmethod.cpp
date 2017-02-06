/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "hangulinputmethod.h"
#include "inputcontext.h"
#include "hangul.h"

namespace QtVirtualKeyboard {

/*!
    \class QtVirtualKeyboard::HangulInputMethod
    \internal
*/

HangulInputMethod::HangulInputMethod(QObject *parent) :
    AbstractInputMethod(*new AbstractInputMethodPrivate(), parent)
{
}

HangulInputMethod::~HangulInputMethod()
{
}

QList<InputEngine::InputMode> HangulInputMethod::inputModes(const QString &locale)
{
    Q_UNUSED(locale)
    return QList<InputEngine::InputMode>() << InputEngine::Hangul;
}

bool HangulInputMethod::setInputMode(const QString &locale, InputEngine::InputMode inputMode)
{
    Q_UNUSED(locale)
    Q_UNUSED(inputMode)
    return true;
}

bool HangulInputMethod::setTextCase(InputEngine::TextCase textCase)
{
    Q_UNUSED(textCase)
    return true;
}

bool HangulInputMethod::keyEvent(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    InputContext *ic = inputContext();
    bool accept = false;
    int cursorPosition = ic->cursorPosition();
    if (ic->cursorPosition() > 0) {
        if (key == Qt::Key_Backspace) {
            int contextLength = cursorPosition > 1 ? 2 : 1;
            QString hangul = Hangul::decompose(ic->surroundingText().mid(cursorPosition - contextLength, contextLength));
            int length = hangul.length();
            if (hangul.length() > 1) {
                ic->commit(Hangul::compose(hangul.left(length - 1)), -contextLength, contextLength);
                accept = true;
            }
        } else if (!text.isEmpty() && Hangul::isJamo(text.at(0).unicode())) {
            QString hangul = Hangul::compose(ic->surroundingText().mid(cursorPosition - 1, 1) + text);
            ic->commit(hangul, -1, 1);
            accept = true;
        }
    }
    return accept;
}

void HangulInputMethod::reset()
{
}

void HangulInputMethod::update()
{
}

} // namespace QtVirtualKeyboard
