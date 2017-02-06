/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include "desktop_screen_qt.h"

#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"

namespace QtWebEngineCore {

gfx::Point DesktopScreenQt::GetCursorScreenPoint()
{
    Q_UNREACHABLE();
    return gfx::Point();
}

bool DesktopScreenQt::IsWindowUnderCursor(gfx::NativeWindow)
{
    Q_UNREACHABLE();
    return false;
}

gfx::NativeWindow DesktopScreenQt::GetWindowAtScreenPoint(const gfx::Point& point)
{
    Q_UNREACHABLE();
    return gfx::NativeWindow();
}

int DesktopScreenQt::GetNumDisplays() const
{
    Q_UNREACHABLE();
    return 0;
}

std::vector<display::Display> DesktopScreenQt::GetAllDisplays() const
{
    Q_UNREACHABLE();
    return std::vector<display::Display>();
}

display::Display DesktopScreenQt::GetDisplayNearestWindow(gfx::NativeView window) const
{
    // RenderViewHostImpl::OnStartDragging uses this to determine
    // the scale factor for the view.
    return display::Display();
}

display::Display DesktopScreenQt::GetDisplayNearestPoint(const gfx::Point& point) const
{
    Q_UNREACHABLE();
    return display::Display();
}

display::Display DesktopScreenQt::GetDisplayMatching(const gfx::Rect& match_rect) const
{
    Q_UNREACHABLE();
    return display::Display();
}

display::Display DesktopScreenQt::GetPrimaryDisplay() const
{
    return display::Display();
}

void DesktopScreenQt::AddObserver(display::DisplayObserver* observer)
{
    Q_UNREACHABLE();
}

void DesktopScreenQt::RemoveObserver(display::DisplayObserver* observer)
{
    Q_UNREACHABLE();
}

} // namespace QtWebEngineCore
