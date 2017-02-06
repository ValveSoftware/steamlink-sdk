'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
''
'' Copyright (C) 2016 The Qt Company Ltd.
'' Contact: https://www.qt.io/licensing/
''
'' This file is part of the ActiveQt module of the Qt Toolkit.
''
'' $QT_BEGIN_LICENSE:GPL-EXCEPT$
'' Commercial License Usage
'' Licensees holding valid commercial Qt licenses may use this file in
'' accordance with the commercial license agreement provided with the
'' Software or, alternatively, in accordance with the terms contained in
'' a written agreement between you and The Qt Company. For licensing terms
'' and conditions see https://www.qt.io/terms-conditions. For further
'' information use the contact form at https://www.qt.io/contact-us.
''
'' GNU General Public License Usage
'' Alternatively, this file may be used under the terms of the GNU
'' General Public License version 3 as published by the Free Software
'' Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
'' included in the packaging of this file. Please review the following
'' information to ensure the GNU General Public License requirements will
'' be met: https://www.gnu.org/licenses/gpl-3.0.html.
''
'' $QT_END_LICENSE$
''
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

' This script can be loaded in Qt TestCon, and used to script
' the hierarchyax project.
'
' Instructions: Open testcon, insert the QParentWidget class,
'               load this script, run "Main()" macro.

Sub Main
    ' Create new widget object
    QParentWidget.createSubWidget("ABC")

    ' Retrieve widget
    Set widget = QParentWidget.subWidget("ABC")

    ' Read label property
    label = widget.label
    MainWindow.logMacro 0, "Old widget label: "&label, 0, ""

    ' Write label property
    widget.label = "renamed "&label
    label = widget.label
    MainWindow.logMacro 0, "New widget label: "&label, 0, ""
End Sub
