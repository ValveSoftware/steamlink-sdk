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

#ifndef DEFAULTINPUTMETHOD_H
#define DEFAULTINPUTMETHOD_H

#include "abstractinputmethod.h"

namespace QtVirtualKeyboard {

class DefaultInputMethod : public AbstractInputMethod
{
    Q_OBJECT

public:
    explicit DefaultInputMethod(QObject *parent = 0);

    QList<InputEngine::InputMode> inputModes(const QString &locale);
    bool setInputMode(const QString &locale, InputEngine::InputMode inputMode);
    bool setTextCase(InputEngine::TextCase textCase);

    bool keyEvent(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers);

    void reset();
    void update();
};

} // namespace QtVirtualKeyboard

#endif // DEFAULTINPUTMETHOD_H
