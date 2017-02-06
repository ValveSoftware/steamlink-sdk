/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Jolla Ltd
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

#include "qwaylandxkb_p.h"

#include <QKeyEvent>
#include <QString>

#include <xkbcommon/xkbcommon-keysyms.h>

QT_BEGIN_NAMESPACE

static const uint32_t KeyTbl[] = {
    XKB_KEY_Escape,                  Qt::Key_Escape,
    XKB_KEY_Tab,                     Qt::Key_Tab,
    XKB_KEY_ISO_Left_Tab,            Qt::Key_Backtab,
    XKB_KEY_BackSpace,               Qt::Key_Backspace,
    XKB_KEY_Return,                  Qt::Key_Return,
    XKB_KEY_Insert,                  Qt::Key_Insert,
    XKB_KEY_Delete,                  Qt::Key_Delete,
    XKB_KEY_Clear,                   Qt::Key_Delete,
    XKB_KEY_Pause,                   Qt::Key_Pause,
    XKB_KEY_Print,                   Qt::Key_Print,

    XKB_KEY_Home,                    Qt::Key_Home,
    XKB_KEY_End,                     Qt::Key_End,
    XKB_KEY_Left,                    Qt::Key_Left,
    XKB_KEY_Up,                      Qt::Key_Up,
    XKB_KEY_Right,                   Qt::Key_Right,
    XKB_KEY_Down,                    Qt::Key_Down,
    XKB_KEY_Prior,                   Qt::Key_PageUp,
    XKB_KEY_Next,                    Qt::Key_PageDown,

    XKB_KEY_Shift_L,                 Qt::Key_Shift,
    XKB_KEY_Shift_R,                 Qt::Key_Shift,
    XKB_KEY_Shift_Lock,              Qt::Key_Shift,
    XKB_KEY_Control_L,               Qt::Key_Control,
    XKB_KEY_Control_R,               Qt::Key_Control,
    XKB_KEY_Meta_L,                  Qt::Key_Meta,
    XKB_KEY_Meta_R,                  Qt::Key_Meta,
    XKB_KEY_Alt_L,                   Qt::Key_Alt,
    XKB_KEY_Alt_R,                   Qt::Key_Alt,
    XKB_KEY_Caps_Lock,               Qt::Key_CapsLock,
    XKB_KEY_Num_Lock,                Qt::Key_NumLock,
    XKB_KEY_Scroll_Lock,             Qt::Key_ScrollLock,
    XKB_KEY_Super_L,                 Qt::Key_Super_L,
    XKB_KEY_Super_R,                 Qt::Key_Super_R,
    XKB_KEY_Menu,                    Qt::Key_Menu,
    XKB_KEY_Hyper_L,                 Qt::Key_Hyper_L,
    XKB_KEY_Hyper_R,                 Qt::Key_Hyper_R,
    XKB_KEY_Help,                    Qt::Key_Help,

    XKB_KEY_KP_Space,                Qt::Key_Space,
    XKB_KEY_KP_Tab,                  Qt::Key_Tab,
    XKB_KEY_KP_Enter,                Qt::Key_Enter,
    XKB_KEY_KP_Home,                 Qt::Key_Home,
    XKB_KEY_KP_Left,                 Qt::Key_Left,
    XKB_KEY_KP_Up,                   Qt::Key_Up,
    XKB_KEY_KP_Right,                Qt::Key_Right,
    XKB_KEY_KP_Down,                 Qt::Key_Down,
    XKB_KEY_KP_Prior,                Qt::Key_PageUp,
    XKB_KEY_KP_Next,                 Qt::Key_PageDown,
    XKB_KEY_KP_End,                  Qt::Key_End,
    XKB_KEY_KP_Begin,                Qt::Key_Clear,
    XKB_KEY_KP_Insert,               Qt::Key_Insert,
    XKB_KEY_KP_Delete,               Qt::Key_Delete,
    XKB_KEY_KP_Equal,                Qt::Key_Equal,
    XKB_KEY_KP_Multiply,             Qt::Key_Asterisk,
    XKB_KEY_KP_Add,                  Qt::Key_Plus,
    XKB_KEY_KP_Separator,            Qt::Key_Comma,
    XKB_KEY_KP_Subtract,             Qt::Key_Minus,
    XKB_KEY_KP_Decimal,              Qt::Key_Period,
    XKB_KEY_KP_Divide,               Qt::Key_Slash,

    XKB_KEY_ISO_Level3_Shift,        Qt::Key_AltGr,
    XKB_KEY_Multi_key,               Qt::Key_Multi_key,
    XKB_KEY_Codeinput,               Qt::Key_Codeinput,
    XKB_KEY_SingleCandidate,         Qt::Key_SingleCandidate,
    XKB_KEY_MultipleCandidate,       Qt::Key_MultipleCandidate,
    XKB_KEY_PreviousCandidate,       Qt::Key_PreviousCandidate,

    XKB_KEY_Mode_switch,             Qt::Key_Mode_switch,
    XKB_KEY_script_switch,           Qt::Key_Mode_switch,

    XKB_KEY_XF86Back,                Qt::Key_Back,
    XKB_KEY_XF86Forward,             Qt::Key_Forward,
    XKB_KEY_XF86Stop,                Qt::Key_Stop,
    XKB_KEY_XF86Refresh,             Qt::Key_Refresh,
    XKB_KEY_XF86Favorites,           Qt::Key_Favorites,
    XKB_KEY_XF86AudioMedia,          Qt::Key_LaunchMedia,
    XKB_KEY_XF86OpenURL,             Qt::Key_OpenUrl,
    XKB_KEY_XF86HomePage,            Qt::Key_HomePage,
    XKB_KEY_XF86Search,              Qt::Key_Search,
    XKB_KEY_XF86AudioLowerVolume,    Qt::Key_VolumeDown,
    XKB_KEY_XF86AudioMute,           Qt::Key_VolumeMute,
    XKB_KEY_XF86AudioRaiseVolume,    Qt::Key_VolumeUp,
    XKB_KEY_XF86AudioPlay,           Qt::Key_MediaTogglePlayPause,
    XKB_KEY_XF86AudioStop,           Qt::Key_MediaStop,
    XKB_KEY_XF86AudioPrev,           Qt::Key_MediaPrevious,
    XKB_KEY_XF86AudioNext,           Qt::Key_MediaNext,
    XKB_KEY_XF86AudioRecord,         Qt::Key_MediaRecord,
    XKB_KEY_XF86AudioPause,          Qt::Key_MediaPause,
    XKB_KEY_XF86Mail,                Qt::Key_LaunchMail,
    XKB_KEY_XF86Calculator,          Qt::Key_Calculator,
    XKB_KEY_XF86Memo,                Qt::Key_Memo,
    XKB_KEY_XF86ToDoList,            Qt::Key_ToDoList,
    XKB_KEY_XF86Calendar,            Qt::Key_Calendar,
    XKB_KEY_XF86PowerDown,           Qt::Key_PowerDown,
    XKB_KEY_XF86ContrastAdjust,      Qt::Key_ContrastAdjust,
    XKB_KEY_XF86Standby,             Qt::Key_Standby,
    XKB_KEY_XF86MonBrightnessUp,     Qt::Key_MonBrightnessUp,
    XKB_KEY_XF86MonBrightnessDown,   Qt::Key_MonBrightnessDown,
    XKB_KEY_XF86KbdLightOnOff,       Qt::Key_KeyboardLightOnOff,
    XKB_KEY_XF86KbdBrightnessUp,     Qt::Key_KeyboardBrightnessUp,
    XKB_KEY_XF86KbdBrightnessDown,   Qt::Key_KeyboardBrightnessDown,
    XKB_KEY_XF86PowerOff,            Qt::Key_PowerOff,
    XKB_KEY_XF86WakeUp,              Qt::Key_WakeUp,
    XKB_KEY_XF86Eject,               Qt::Key_Eject,
    XKB_KEY_XF86ScreenSaver,         Qt::Key_ScreenSaver,
    XKB_KEY_XF86WWW,                 Qt::Key_WWW,
    XKB_KEY_XF86Sleep,               Qt::Key_Sleep,
    XKB_KEY_XF86LightBulb,           Qt::Key_LightBulb,
    XKB_KEY_XF86Shop,                Qt::Key_Shop,
    XKB_KEY_XF86History,             Qt::Key_History,
    XKB_KEY_XF86AddFavorite,         Qt::Key_AddFavorite,
    XKB_KEY_XF86HotLinks,            Qt::Key_HotLinks,
    XKB_KEY_XF86BrightnessAdjust,    Qt::Key_BrightnessAdjust,
    XKB_KEY_XF86Finance,             Qt::Key_Finance,
    XKB_KEY_XF86Community,           Qt::Key_Community,
    XKB_KEY_XF86AudioRewind,         Qt::Key_AudioRewind,
    XKB_KEY_XF86BackForward,         Qt::Key_BackForward,
    XKB_KEY_XF86ApplicationLeft,     Qt::Key_ApplicationLeft,
    XKB_KEY_XF86ApplicationRight,    Qt::Key_ApplicationRight,
    XKB_KEY_XF86Book,                Qt::Key_Book,
    XKB_KEY_XF86CD,                  Qt::Key_CD,
    XKB_KEY_XF86Calculater,          Qt::Key_Calculator,
    XKB_KEY_XF86Clear,               Qt::Key_Clear,
    XKB_KEY_XF86ClearGrab,           Qt::Key_ClearGrab,
    XKB_KEY_XF86Close,               Qt::Key_Close,
    XKB_KEY_XF86Copy,                Qt::Key_Copy,
    XKB_KEY_XF86Cut,                 Qt::Key_Cut,
    XKB_KEY_XF86Display,             Qt::Key_Display,
    XKB_KEY_XF86DOS,                 Qt::Key_DOS,
    XKB_KEY_XF86Documents,           Qt::Key_Documents,
    XKB_KEY_XF86Excel,               Qt::Key_Excel,
    XKB_KEY_XF86Explorer,            Qt::Key_Explorer,
    XKB_KEY_XF86Game,                Qt::Key_Game,
    XKB_KEY_XF86Go,                  Qt::Key_Go,
    XKB_KEY_XF86iTouch,              Qt::Key_iTouch,
    XKB_KEY_XF86LogOff,              Qt::Key_LogOff,
    XKB_KEY_XF86Market,              Qt::Key_Market,
    XKB_KEY_XF86Meeting,             Qt::Key_Meeting,
    XKB_KEY_XF86MenuKB,              Qt::Key_MenuKB,
    XKB_KEY_XF86MenuPB,              Qt::Key_MenuPB,
    XKB_KEY_XF86MySites,             Qt::Key_MySites,
    XKB_KEY_XF86New,                 Qt::Key_New,
    XKB_KEY_XF86News,                Qt::Key_News,
    XKB_KEY_XF86OfficeHome,          Qt::Key_OfficeHome,
    XKB_KEY_XF86Open,                Qt::Key_Open,
    XKB_KEY_XF86Option,              Qt::Key_Option,
    XKB_KEY_XF86Paste,               Qt::Key_Paste,
    XKB_KEY_XF86Phone,               Qt::Key_Phone,
    XKB_KEY_XF86Reply,               Qt::Key_Reply,
    XKB_KEY_XF86Reload,              Qt::Key_Reload,
    XKB_KEY_XF86RotateWindows,       Qt::Key_RotateWindows,
    XKB_KEY_XF86RotationPB,          Qt::Key_RotationPB,
    XKB_KEY_XF86RotationKB,          Qt::Key_RotationKB,
    XKB_KEY_XF86Save,                Qt::Key_Save,
    XKB_KEY_XF86Send,                Qt::Key_Send,
    XKB_KEY_XF86Spell,               Qt::Key_Spell,
    XKB_KEY_XF86SplitScreen,         Qt::Key_SplitScreen,
    XKB_KEY_XF86Support,             Qt::Key_Support,
    XKB_KEY_XF86TaskPane,            Qt::Key_TaskPane,
    XKB_KEY_XF86Terminal,            Qt::Key_Terminal,
    XKB_KEY_XF86Tools,               Qt::Key_Tools,
    XKB_KEY_XF86Travel,              Qt::Key_Travel,
    XKB_KEY_XF86Video,               Qt::Key_Video,
    XKB_KEY_XF86Word,                Qt::Key_Word,
    XKB_KEY_XF86Xfer,                Qt::Key_Xfer,
    XKB_KEY_XF86ZoomIn,              Qt::Key_ZoomIn,
    XKB_KEY_XF86ZoomOut,             Qt::Key_ZoomOut,
    XKB_KEY_XF86Away,                Qt::Key_Away,
    XKB_KEY_XF86Messenger,           Qt::Key_Messenger,
    XKB_KEY_XF86WebCam,              Qt::Key_WebCam,
    XKB_KEY_XF86MailForward,         Qt::Key_MailForward,
    XKB_KEY_XF86Pictures,            Qt::Key_Pictures,
    XKB_KEY_XF86Music,               Qt::Key_Music,
    XKB_KEY_XF86Battery,             Qt::Key_Battery,
    XKB_KEY_XF86Bluetooth,           Qt::Key_Bluetooth,
    XKB_KEY_XF86WLAN,                Qt::Key_WLAN,
    XKB_KEY_XF86UWB,                 Qt::Key_UWB,
    XKB_KEY_XF86AudioForward,        Qt::Key_AudioForward,
    XKB_KEY_XF86AudioRepeat,         Qt::Key_AudioRepeat,
    XKB_KEY_XF86AudioRandomPlay,     Qt::Key_AudioRandomPlay,
    XKB_KEY_XF86Subtitle,            Qt::Key_Subtitle,
    XKB_KEY_XF86AudioCycleTrack,     Qt::Key_AudioCycleTrack,
    XKB_KEY_XF86Time,                Qt::Key_Time,
    XKB_KEY_XF86Select,              Qt::Key_Select,
    XKB_KEY_XF86View,                Qt::Key_View,
    XKB_KEY_XF86TopMenu,             Qt::Key_TopMenu,
    XKB_KEY_XF86Red,                 Qt::Key_Red,
    XKB_KEY_XF86Green,               Qt::Key_Green,
    XKB_KEY_XF86Yellow,              Qt::Key_Yellow,
    XKB_KEY_XF86Blue,                Qt::Key_Blue,
    XKB_KEY_XF86Bluetooth,           Qt::Key_Bluetooth,
    XKB_KEY_XF86Suspend,             Qt::Key_Suspend,
    XKB_KEY_XF86Hibernate,           Qt::Key_Hibernate,
    XKB_KEY_XF86TouchpadToggle,      Qt::Key_TouchpadToggle,
    XKB_KEY_XF86TouchpadOn,          Qt::Key_TouchpadOn,
    XKB_KEY_XF86TouchpadOff,         Qt::Key_TouchpadOff,
    XKB_KEY_XF86AudioMicMute,        Qt::Key_MicMute,
    XKB_KEY_XF86Launch0,             Qt::Key_Launch0,
    XKB_KEY_XF86Launch1,             Qt::Key_Launch1,
    XKB_KEY_XF86Launch2,             Qt::Key_Launch2,
    XKB_KEY_XF86Launch3,             Qt::Key_Launch3,
    XKB_KEY_XF86Launch4,             Qt::Key_Launch4,
    XKB_KEY_XF86Launch5,             Qt::Key_Launch5,
    XKB_KEY_XF86Launch6,             Qt::Key_Launch6,
    XKB_KEY_XF86Launch7,             Qt::Key_Launch7,
    XKB_KEY_XF86Launch8,             Qt::Key_Launch8,
    XKB_KEY_XF86Launch9,             Qt::Key_Launch9,
    XKB_KEY_XF86LaunchA,             Qt::Key_LaunchA,
    XKB_KEY_XF86LaunchB,             Qt::Key_LaunchB,
    XKB_KEY_XF86LaunchC,             Qt::Key_LaunchC,
    XKB_KEY_XF86LaunchD,             Qt::Key_LaunchD,
    XKB_KEY_XF86LaunchE,             Qt::Key_LaunchE,
    XKB_KEY_XF86LaunchF,             Qt::Key_LaunchF,

    0,                          0
};

static int lookupKeysym(xkb_keysym_t key)
{
    int code = 0;
    int i = 0;
    while (KeyTbl[i]) {
        if (key == KeyTbl[i]) {
            code = (int)KeyTbl[i+1];
            break;
        }
        i += 2;
    }

    return code;
}

static xkb_keysym_t toKeysymFromTable(uint32_t key)
{
    for (int i = 0; KeyTbl[i]; i += 2) {
        if (key == KeyTbl[i + 1])
            return KeyTbl[i];
    }

    return 0;
}

std::pair<int, QString> QWaylandXkb::keysymToQtKey(xkb_keysym_t keysym, Qt::KeyboardModifiers &modifiers)
{
    QString text;
    uint utf32 = xkb_keysym_to_utf32(keysym);
    if (utf32)
        text = QString::fromUcs4(&utf32, 1);

    int code = 0;

    if (keysym >= XKB_KEY_F1 && keysym <= XKB_KEY_F35) {
        code =  Qt::Key_F1 + (int(keysym) - XKB_KEY_F1);
    } else if (keysym >= XKB_KEY_KP_Space && keysym <= XKB_KEY_KP_9) {
        if (keysym >= XKB_KEY_KP_0) {
            // numeric keypad keys
            code = Qt::Key_0 + ((int)keysym - XKB_KEY_KP_0);
        } else {
            code = lookupKeysym(keysym);
        }
        modifiers |= Qt::KeypadModifier;
    } else if (text.length() == 1 && text.unicode()->unicode() > 0x1f
        && text.unicode()->unicode() != 0x7f
        && !(keysym >= XKB_KEY_dead_grave && keysym <= XKB_KEY_dead_currency)) {
        code = text.unicode()->toUpper().unicode();
    } else {
        // any other keys
        code = lookupKeysym(keysym);
    }

    // Map control + letter to proper text
    if (utf32 >= 'A' && utf32 <= '~' && (modifiers & Qt::ControlModifier)) {
        utf32 &= ~0x60;
        text = QString::fromUcs4(&utf32, 1);
    }

    return { code, text };
}

Qt::KeyboardModifiers QWaylandXkb::modifiers(struct xkb_state *state)
{
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;

    xkb_state_component cstate = static_cast<xkb_state_component>(XKB_STATE_DEPRESSED | XKB_STATE_LATCHED | XKB_STATE_LOCKED);

    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT, cstate))
        modifiers |= Qt::ShiftModifier;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL, cstate))
        modifiers |= Qt::ControlModifier;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT, cstate))
        modifiers |= Qt::AltModifier;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_LOGO, cstate))
        modifiers |= Qt::MetaModifier;

    return modifiers;
}

QEvent::Type QWaylandXkb::toQtEventType(uint32_t state)
{
    return state != 0 ? QEvent::KeyPress : QEvent::KeyRelease;
}

QVector<xkb_keysym_t> QWaylandXkb::toKeysym(QKeyEvent *event)
{
    QVector<xkb_keysym_t> keysyms;
    if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F35) {
        keysyms.append(XKB_KEY_F1 + (event->key() - Qt::Key_F1));
    } else if (event->modifiers() & Qt::KeypadModifier) {
        if (event->key() >= Qt::Key_0 && event->key() <= Qt::Key_9)
            keysyms.append(XKB_KEY_KP_0 + (event->key() - Qt::Key_0));
        else
            keysyms.append(toKeysymFromTable(event->key()));
    } else if (!event->text().isEmpty()) {
        // From libxkbcommon keysym-utf.c:
        // "We allow to represent any UCS character in the range U-00000000 to
        // U-00FFFFFF by a keysym value in the range 0x01000000 to 0x01ffffff."
        foreach (uint utf32, event->text().toUcs4()) {
            keysyms.append(utf32 | 0x01000000);
        }
    } else {
        keysyms.append(toKeysymFromTable(event->key()));
    }
    return keysyms;
}

QT_END_NAMESPACE
