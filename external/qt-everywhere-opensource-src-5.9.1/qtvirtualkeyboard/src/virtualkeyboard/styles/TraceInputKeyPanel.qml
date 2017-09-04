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

import QtQuick 2.0

/*!
    \qmltype TraceInputKeyPanel
    \inqmlmodule QtQuick.VirtualKeyboard.Styles
    \brief A base type of the trace input key.
    \ingroup qtvirtualkeyboard-styles-qml
    \since QtQuick.VirtualKeyboard 2.0

    This type provides panel for decorating TraceInputKey
    items in the keyboard layout.
*/

Item {
    /*! Provides access to properties in TraceInputKey.

        A list of available properties in control:
        \list
            \li \c patternRecognitionMode Pattern recognition mode of this input area
            \li \c horizontalRulers A list of horizontal rulers
            \li \c verticalRulers A list of vertical rulers
            \li \c boundingBox Bounding box for the trace input
        \endlist
    */
    property Item control

    /*! Sets margins of the trace input area.

        The margins affect to the bounding box of the trace input area.
    */
    property real traceMargins
}
