/****************************************************************************
**
** Copyright (C) 2016 LG Electronics Ltd., author: <mikko.levonmaa@lge.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mockseat.h"

MockSeat::MockSeat(wl_seat *seat)
    : m_seat(seat)
{
    // Bind to the keyboard interface so that the compositor has
    // the right resource associations
    m_keyboard = wl_seat_get_keyboard(seat);
}

MockSeat::~MockSeat()
{
    wl_keyboard_destroy(m_keyboard);
    wl_seat_destroy(m_seat);
}
