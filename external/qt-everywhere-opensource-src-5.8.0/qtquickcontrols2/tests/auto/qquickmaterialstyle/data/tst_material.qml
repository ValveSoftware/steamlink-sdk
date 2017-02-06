/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2
import QtQuick.Window 2.2
import QtTest 1.0
import QtQuick.Templates 2.1 as T
import QtQuick.Controls 2.1
import QtQuick.Controls.Material 2.1

TestCase {
    id: testCase
    width: 200
    height: 200
    visible: true
    when: windowShown
    name: "Material"

    Component {
        id: button
        Button { }
    }

    Component {
        id: styledButton
        Button {
            Material.theme: Material.Dark
            Material.primary: Material.DeepOrange
            Material.accent: Material.DeepPurple
            Material.background: Material.Green
            Material.foreground: Material.Blue
        }
    }

    Component {
        id: window
        Window { }
    }

    Component {
        id: applicationWindow
        ApplicationWindow { }
    }

    Component {
        id: styledWindow
        Window {
            Material.theme: Material.Dark
            Material.primary: Material.Brown
            Material.accent: Material.Green
            Material.background: Material.Yellow
            Material.foreground: Material.Grey
        }
    }

    Component {
        id: loader
        Loader {
            active: false
            sourceComponent: Button { }
        }
    }

    Component {
        id: swipeView
        SwipeView {
            Material.theme: Material.Dark
            Button { }
        }
    }

    Component {
        id: menu
        ApplicationWindow {
            Material.primary: Material.Blue
            Material.accent: Material.Red
            property alias menu: popup
            Menu {
                id: popup
                Material.theme: Material.Dark
                MenuItem { }
            }
        }
    }

    Component {
        id: popupComponent
        ApplicationWindow {
            Material.primary: Material.Blue
            Material.accent: Material.Red
            visible: true
            property alias popup: popupInstance
            property alias label: labelInstance
            property alias label2: labelInstance2
            Popup {
                id: popupInstance
                Label {
                    id: labelInstance
                    text: "test"
                    color: popupInstance.Material.textSelectionColor
                }
                Component.onCompleted: open()
            }
            T.Popup {
                contentItem: Label {
                    id: labelInstance2
                    text: "test"
                    color: Material.textSelectionColor
                }
                Component.onCompleted: open()
            }
        }
    }

    Component {
        id: comboBox
        ApplicationWindow {
            width: 200
            height: 200
            visible: true
            Material.primary: Material.Blue
            Material.accent: Material.Red
            property alias combo: box
            ComboBox {
                id: box
                Material.theme: Material.Dark
                model: 1
            }
        }
    }

    Component {
        id: windowPane
        ApplicationWindow {
            width: 200
            height: 200
            visible: true
            property alias pane: pane
            Pane { id: pane }
        }
    }

    // need to be synced with QQuickMaterialStyle::themeShade()
    function themeshade(theme) {
        if (theme === Material.Light)
            return Material.Shade500
        else
            return Material.Shade200
    }

    function test_defaults() {
        var control = button.createObject(testCase)
        verify(control)
        verify(control.Material)
        compare(control.Material.primary, Material.color(Material.Indigo))
        compare(control.Material.accent, Material.color(Material.Pink))
        compare(control.Material.foreground, "#dd000000")
        compare(control.Material.background, "#fafafa")
        compare(control.Material.theme, Material.Light)
        control.destroy()
    }

    function test_set() {
        var control = button.createObject(testCase)
        verify(control)
        control.Material.primary = Material.Green
        control.Material.accent = Material.Brown
        control.Material.background = Material.Red
        control.Material.foreground = Material.Blue
        control.Material.theme = Material.Dark
        compare(control.Material.primary, Material.color(Material.Green))
        compare(control.Material.accent, Material.color(Material.Brown, themeshade(control.Material.theme)))
        compare(control.Material.background, Material.color(Material.Red, themeshade(control.Material.theme)))
        compare(control.Material.foreground, Material.color(Material.Blue))
        compare(control.Material.theme, Material.Dark)
        control.destroy()
    }

    function test_reset() {
        var control = styledButton.createObject(testCase)
        verify(control)
        compare(control.Material.primary, Material.color(Material.DeepOrange))
        compare(control.Material.accent, Material.color(Material.DeepPurple, themeshade(control.Material.theme)))
        compare(control.Material.background, Material.color(Material.Green, themeshade(control.Material.theme)))
        compare(control.Material.foreground, Material.color(Material.Blue))
        compare(control.Material.theme, Material.Dark)
        control.Material.primary = undefined
        control.Material.accent = undefined
        control.Material.background = undefined
        control.Material.foreground = undefined
        control.Material.theme = undefined
        compare(control.Material.primary, testCase.Material.primary)
        compare(control.Material.accent, testCase.Material.accent)
        compare(control.Material.background, testCase.Material.background)
        compare(control.Material.foreground, testCase.Material.foreground)
        compare(control.Material.theme, testCase.Material.theme)
        control.destroy()
    }

    function test_inheritance_data() {
        return [
            { tag: "primary", value1: Material.color(Material.Amber), value2: Material.color(Material.Indigo) },
            { tag: "accent", value1: Material.color(Material.Amber), value2: Material.color(Material.Indigo) },
            { tag: "background", value1: Material.color(Material.Amber), value2: Material.color(Material.Indigo) },
            { tag: "foreground", value1: Material.color(Material.Amber), value2: Material.color(Material.Indigo) },
            { tag: "theme", value1: Material.Dark, value2: Material.Light },
        ]
    }

    function test_inheritance(data) {
        var prop = data.tag
        var parent = button.createObject(testCase)
        parent.Material[prop] = data.value1
        compare(parent.Material[prop], data.value1)

        var child1 = button.createObject(parent)
        compare(child1.Material[prop], data.value1)

        parent.Material[prop] = data.value2
        compare(parent.Material[prop], data.value2)
        compare(child1.Material[prop], data.value2)

        var child2 = button.createObject(parent)
        compare(child2.Material[prop], data.value2)

        child2.Material[prop] = data.value1
        compare(child2.Material[prop], data.value1)
        compare(child1.Material[prop], data.value2)
        compare(parent.Material[prop], data.value2)

        parent.Material[prop] = undefined
        verify(parent.Material[prop] !== data.value1)
        verify(parent.Material[prop] !== undefined)
        compare(child1.Material[prop], parent.Material[prop])
        verify(child2.Material[prop] !== parent.Material[prop])

        var grandChild1 = button.createObject(child1)
        var grandChild2 = button.createObject(child2)
        compare(grandChild1.Material[prop], child1.Material[prop])
        compare(grandChild2.Material[prop], child2.Material[prop])

        var themelessGrandGrandChild = button.createObject(grandChild1)
        var grandGrandGrandChild1 = button.createObject(themelessGrandGrandChild)
        compare(grandGrandGrandChild1.Material[prop], parent.Material[prop])

        child1.Material[prop] = data.value2
        compare(child1.Material[prop], data.value2)
        compare(grandChild1.Material[prop], data.value2)
        compare(grandGrandGrandChild1.Material[prop], data.value2)

        parent.destroy()
    }

    function test_inheritance_popup_data() {
        return [
            { tag: "primary", value1: Material.color(Material.Amber), value2: Material.color(Material.Indigo) },
            { tag: "accent", value1: Material.color(Material.Amber), value2: Material.color(Material.Indigo) },
            { tag: "theme", value1: Material.Dark, value2: Material.Light },
        ]
    }

    function test_inheritance_popup(data) {
        var prop = data.tag
        var popupObject = popupComponent.createObject(testCase)
        compare(popupObject.popup.Material.textSelectionColor.toString(), popupObject.Material.textSelectionColor.toString())
        compare(popupObject.label.color.toString(), popupObject.Material.textSelectionColor.toString())
        compare(popupObject.label2.color.toString(), popupObject.Material.textSelectionColor.toString())

        popupObject.Material[prop] = data.value1
        compare(popupObject.Material[prop], data.value1)
        compare(popupObject.popup.Material.textSelectionColor.toString(), popupObject.Material.textSelectionColor.toString())
        compare(popupObject.label.color.toString(), popupObject.Material.textSelectionColor.toString())
        compare(popupObject.label2.color.toString(), popupObject.Material.textSelectionColor.toString())

        popupObject.Material[prop] = data.value2
        compare(popupObject.Material[prop], data.value2)
        compare(popupObject.popup.Material.textSelectionColor.toString(), popupObject.Material.textSelectionColor.toString())
        compare(popupObject.label.color.toString(), popupObject.Material.textSelectionColor.toString())
        compare(popupObject.label2.color.toString(), popupObject.Material.textSelectionColor.toString())

        popupObject.destroy()
    }

    function test_window() {
        var parent = window.createObject()

        var control = button.createObject(parent.contentItem)
        compare(control.Material.primary, parent.Material.primary)
        compare(control.Material.accent, parent.Material.accent)
        compare(control.Material.background, parent.Material.background)
        compare(control.Material.foreground, parent.Material.foreground)
        compare(control.Material.theme, parent.Material.theme)

        var styledChild = styledWindow.createObject(window)
        verify(styledChild.Material.primary !== parent.Material.primary)
        verify(styledChild.Material.accent !== parent.Material.accent)
        verify(styledChild.Material.background !== parent.Material.background)
        verify(styledChild.Material.foreground !== parent.Material.foreground)
        verify(styledChild.Material.theme !== parent.Material.theme)

        var unstyledChild = window.createObject(window)
        compare(unstyledChild.Material.primary, parent.Material.primary)
        compare(unstyledChild.Material.accent, parent.Material.accent)
        compare(unstyledChild.Material.background, parent.Material.background)
        compare(unstyledChild.Material.foreground, parent.Material.foreground)
        compare(unstyledChild.Material.theme, parent.Material.theme)

        parent.Material.primary = Material.Lime
        compare(control.Material.primary, Material.color(Material.Lime))
        verify(styledChild.Material.primary !== Material.color(Material.Lime))
        // ### TODO: compare(unstyledChild.Material.primary, Material.color(Material.Lime))

        parent.Material.accent = Material.Cyan
        compare(control.Material.accent, Material.color(Material.Cyan))
        verify(styledChild.Material.accent !== Material.color(Material.Cyan))
        // ### TODO: compare(unstyledChild.Material.accent, Material.color(Material.Cyan))

        parent.Material.background = Material.Indigo
        compare(control.Material.background, Material.color(Material.Indigo))
        verify(styledChild.Material.background !== Material.color(Material.Indigo))
        // ### TODO: compare(unstyledChild.Material.background, Material.color(Material.Indigo))

        parent.Material.foreground = Material.Pink
        compare(control.Material.foreground, Material.color(Material.Pink))
        verify(styledChild.Material.foreground !== Material.color(Material.Pink))
        // ### TODO: compare(unstyledChild.Material.foreground, Material.color(Material.Pink))

        parent.destroy()
    }

    function test_loader() {
        var control = loader.createObject(testCase)
        control.Material.primary = Material.Yellow
        control.Material.accent = Material.Lime
        control.Material.background = Material.LightGreen
        control.Material.foreground = Material.LightBlue
        control.active = true
        compare(control.item.Material.primary, Material.color(Material.Yellow))
        compare(control.item.Material.accent, Material.color(Material.Lime))
        compare(control.item.Material.background, Material.color(Material.LightGreen))
        compare(control.item.Material.foreground, Material.color(Material.LightBlue))
        control.Material.primary = Material.Red
        control.Material.accent = Material.Pink
        control.Material.background = Material.Blue
        control.Material.foreground = Material.Green
        compare(control.item.Material.primary, Material.color(Material.Red))
        compare(control.item.Material.accent, Material.color(Material.Pink))
        compare(control.item.Material.background, Material.color(Material.Blue))
        compare(control.item.Material.foreground, Material.color(Material.Green))
        control.active = false
        control.Material.primary = Material.Orange
        control.Material.accent = Material.Brown
        control.Material.background = Material.Red
        control.Material.foreground = Material.Pink
        control.active = true
        compare(control.item.Material.primary, Material.color(Material.Orange))
        compare(control.item.Material.accent, Material.color(Material.Brown))
        compare(control.item.Material.background, Material.color(Material.Red))
        compare(control.item.Material.foreground, Material.color(Material.Pink))
        control.destroy()
    }

    function test_swipeView() {
        var control = swipeView.createObject(testCase)
        verify(control)
        var child = control.itemAt(0)
        verify(child)
        compare(control.Material.theme, Material.Dark)
        compare(child.Material.theme, Material.Dark)
        control.destroy()
    }

    function test_menu() {
        var container = menu.createObject(testCase)
        verify(container)
        verify(container.menu)
        container.menu.open()
        verify(container.menu.visible)
        var child = container.menu.itemAt(0)
        verify(child)
        compare(container.Material.theme, Material.Light)
        compare(container.menu.Material.theme, Material.Dark)
        compare(child.Material.theme, Material.Dark)
        compare(container.Material.primary, Material.color(Material.Blue))
        compare(container.menu.Material.primary, Material.color(Material.Blue))
        compare(child.Material.primary, Material.color(Material.Blue))
        compare(container.Material.accent, Material.color(Material.Red))
        compare(container.menu.Material.accent, Material.color(Material.Red, themeshade(container.menu.Material.theme)))
        compare(child.Material.accent, Material.color(Material.Red, themeshade(child.Material.theme)))
        container.destroy()
    }

    function test_comboBox() {
        var window = comboBox.createObject(testCase)
        verify(window)
        verify(window.combo)
        waitForRendering(window.combo)
        window.combo.forceActiveFocus()
        verify(window.combo.activeFocus)
        keyClick(Qt.Key_Space)
        verify(window.combo.popup.visible)
        var listView = window.combo.popup.contentItem
        verify(listView)
        var child = listView.contentItem.children[0]
        verify(child)
        compare(window.Material.theme, Material.Light)
        compare(window.combo.Material.theme, Material.Dark)
        compare(child.Material.theme, Material.Dark)
        compare(window.Material.primary, Material.color(Material.Blue))
        compare(window.combo.Material.primary, Material.color(Material.Blue))
        compare(child.Material.primary, Material.color(Material.Blue))
        compare(window.Material.accent, Material.color(Material.Red))
        compare(window.combo.Material.accent, Material.color(Material.Red, themeshade(window.combo.Material.theme)))
        compare(child.Material.accent, Material.color(Material.Red, themeshade(child.Material.theme)))
        window.destroy()
    }

    function test_windowChange() {
        var ldr = loader.createObject()
        verify(ldr)

        var wnd = window.createObject()
        verify(wnd)

        wnd.Material.theme = Material.Dark
        compare(wnd.Material.theme, Material.Dark)

        ldr.active = true
        verify(ldr.item)
        compare(ldr.item.Material.theme, Material.Light)

        ldr.parent = wnd.contentItem
        compare(ldr.item.Material.theme, Material.Dark)

        wnd.destroy()
    }

    function test_colors_data() {
        return [
            { tag: "primary" }, { tag: "accent" }, { tag: "background" }, { tag: "foreground" }
        ]
    }

    function test_colors(data) {
        var control = button.createObject(testCase)
        verify(control)

        var prop = data.tag

        // Material.Color - enum
        control.Material[prop] = Material.Red
        compare(control.Material[prop], "#f44336")

        // Material.Color - string
        control.Material[prop] = "BlueGrey"
        compare(control.Material[prop], "#607d8b")

        // SVG named color
        control.Material[prop] = "tomato"
        compare(control.Material[prop], "#ff6347")

        // #rrggbb
        control.Material[prop] = "#123456"
        compare(control.Material[prop], "#123456")

        // #aarrggbb
        control.Material[prop] = "#12345678"
        compare(control.Material[prop], "#12345678")

        // Qt.rgba() - no alpha
        control.Material[prop] = Qt.rgba(0.5, 0.5, 0.5)
        compare(control.Material[prop], "#808080")

        // Qt.rgba() - with alpha
        control.Material[prop] = Qt.rgba(0.5, 0.5, 0.5, 0.5)
        compare(control.Material[prop], "#80808080")

        // unknown
        ignoreWarning(Qt.resolvedUrl("tst_material.qml") + ":58:9: QML Button: unknown Material." + prop + " value: 123")
        control.Material[prop] = 123
        ignoreWarning(Qt.resolvedUrl("tst_material.qml") + ":58:9: QML Button: unknown Material." + prop + " value: foo")
        control.Material[prop] = "foo"
        ignoreWarning(Qt.resolvedUrl("tst_material.qml") + ":58:9: QML Button: unknown Material." + prop + " value: #1")
        control.Material[prop] = "#1"

        control.destroy()
    }

    function test_font_data() {
        return [
            {tag: "Button:pixelSize", type: "Button", attribute: "pixelSize", value: 14, window: 20, pane: 10},
            {tag: "Button:weight", type: "Button", attribute: "weight", value: Font.Medium, window: Font.Black, pane: Font.Bold},
            {tag: "Button:capitalization", type: "Button", attribute: "capitalization", value: Font.AllUppercase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "TabButton:pixelSize", type: "TabButton", attribute: "pixelSize", value: 14, window: 20, pane: 10},
            {tag: "TabButton:weight", type: "TabButton", attribute: "weight", value: Font.Medium, window: Font.Black, pane: Font.Bold},
            {tag: "TabButton:capitalization", type: "TabButton", attribute: "capitalization", value: Font.AllUppercase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "ToolButton:pixelSize", type: "ToolButton", attribute: "pixelSize", value: 14, window: 20, pane: 10},
            {tag: "ToolButton:weight", type: "ToolButton", attribute: "weight", value: Font.Medium, window: Font.Black, pane: Font.Bold},
            {tag: "ToolButton:capitalization", type: "ToolButton", attribute: "capitalization", value: Font.AllUppercase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "ItemDelegate:pixelSize", type: "ItemDelegate", attribute: "pixelSize", value: 14, window: 20, pane: 10},
            {tag: "ItemDelegate:weight", type: "ItemDelegate", attribute: "weight", value: Font.Medium, window: Font.Black, pane: Font.Bold},
            {tag: "ItemDelegate:capitalization", type: "ItemDelegate", attribute: "capitalization", value: Font.MixedCase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "CheckDelegate:pixelSize", type: "CheckDelegate", attribute: "pixelSize", value: 16, window: 20, pane: 10},
            {tag: "CheckDelegate:weight", type: "CheckDelegate", attribute: "weight", value: Font.Normal, window: Font.Black, pane: Font.Bold},
            {tag: "CheckDelegate:capitalization", type: "CheckDelegate", attribute: "capitalization", value: Font.MixedCase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "RadioDelegate:pixelSize", type: "RadioDelegate", attribute: "pixelSize", value: 16, window: 20, pane: 10},
            {tag: "RadioDelegate:weight", type: "RadioDelegate", attribute: "weight", value: Font.Normal, window: Font.Black, pane: Font.Bold},
            {tag: "RadioDelegate:capitalization", type: "RadioDelegate", attribute: "capitalization", value: Font.MixedCase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "SwitchDelegate:pixelSize", type: "SwitchDelegate", attribute: "pixelSize", value: 16, window: 20, pane: 10},
            {tag: "SwitchDelegate:weight", type: "SwitchDelegate", attribute: "weight", value: Font.Normal, window: Font.Black, pane: Font.Bold},
            {tag: "SwitchDelegate:capitalization", type: "SwitchDelegate", attribute: "capitalization", value: Font.MixedCase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "Label:pixelSize", type: "Label", attribute: "pixelSize", value: 14, window: 20, pane: 10},
            {tag: "Label:weight", type: "Label", attribute: "weight", value: Font.Normal, window: Font.Black, pane: Font.Bold},
            {tag: "Label:capitalization", type: "Label", attribute: "capitalization", value: Font.MixedCase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "CheckBox:pixelSize", type: "CheckBox", attribute: "pixelSize", value: 14, window: 20, pane: 10},
            {tag: "CheckBox:weight", type: "CheckBox", attribute: "weight", value: Font.Normal, window: Font.Black, pane: Font.Bold},
            {tag: "CheckBox:capitalization", type: "CheckBox", attribute: "capitalization", value: Font.MixedCase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "RadioButton:pixelSize", type: "RadioButton", attribute: "pixelSize", value: 14, window: 20, pane: 10},
            {tag: "RadioButton:weight", type: "RadioButton", attribute: "weight", value: Font.Normal, window: Font.Black, pane: Font.Bold},
            {tag: "RadioButton:capitalization", type: "RadioButton", attribute: "capitalization", value: Font.MixedCase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "Switch:pixelSize", type: "Switch", attribute: "pixelSize", value: 14, window: 20, pane: 10},
            {tag: "Switch:weight", type: "Switch", attribute: "weight", value: Font.Normal, window: Font.Black, pane: Font.Bold},
            {tag: "Switch:capitalization", type: "Switch", attribute: "capitalization", value: Font.MixedCase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "MenuItem:pixelSize", type: "MenuItem", attribute: "pixelSize", value: 16, window: 20, pane: 10},
            {tag: "MenuItem:weight", type: "MenuItem", attribute: "weight", value: Font.Normal, window: Font.Black, pane: Font.Bold},
            {tag: "MenuItem:capitalization", type: "MenuItem", attribute: "capitalization", value: Font.MixedCase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "ComboBox:pixelSize", type: "ComboBox", attribute: "pixelSize", value: 16, window: 20, pane: 10},
            {tag: "ComboBox:weight", type: "ComboBox", attribute: "weight", value: Font.Normal, window: Font.Black, pane: Font.Bold},
            {tag: "ComboBox:capitalization", type: "ComboBox", attribute: "capitalization", value: Font.MixedCase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "TextField:pixelSize", type: "TextField", attribute: "pixelSize", value: 16, window: 20, pane: 10},
            {tag: "TextField:weight", type: "TextField", attribute: "weight", value: Font.Normal, window: Font.Black, pane: Font.Bold},
            {tag: "TextField:capitalization", type: "TextField", attribute: "capitalization", value: Font.MixedCase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "TextArea:pixelSize", type: "TextArea", attribute: "pixelSize", value: 16, window: 20, pane: 10},
            {tag: "TextArea:weight", type: "TextArea", attribute: "weight", value: Font.Normal, window: Font.Black, pane: Font.Bold},
            {tag: "TextArea:capitalization", type: "TextArea", attribute: "capitalization", value: Font.MixedCase, window: Font.Capitalize, pane: Font.AllLowercase},

            {tag: "SpinBox:pixelSize", type: "SpinBox", attribute: "pixelSize", value: 16, window: 20, pane: 10},
            {tag: "SpinBox:weight", type: "SpinBox", attribute: "weight", value: Font.Normal, window: Font.Black, pane: Font.Bold},
            {tag: "SpinBox:capitalization", type: "SpinBox", attribute: "capitalization", value: Font.MixedCase, window: Font.Capitalize, pane: Font.AllLowercase}
        ]
    }

    function test_font(data) {
        var window = windowPane.createObject(testCase)
        verify(window)
        verify(window.pane)

        var control = Qt.createQmlObject("import QtQuick.Controls 2.1; " + data.type + " { }", window.pane)
        verify(control)

        compare(control.font[data.attribute], data.value)

        window.font[data.attribute] = data.window
        compare(window.font[data.attribute], data.window)
        compare(window.pane.font[data.attribute], data.window)
        compare(control.font[data.attribute], data.window)

        window.pane.font[data.attribute] = data.pane
        compare(window.font[data.attribute], data.window)
        compare(window.pane.font[data.attribute], data.pane)
        compare(control.font[data.attribute], data.pane)

        window.pane.font = undefined
        compare(window.font[data.attribute], data.window)
        compare(window.pane.font[data.attribute], data.window)
        compare(control.font[data.attribute], data.window)

        window.destroy()
    }

    Component {
        id: backgroundControls
        ApplicationWindow {
            id: window
            property Button button: Button { }
            property ComboBox combobox: ComboBox { }
            property Drawer drawer: Drawer { }
            property GroupBox groupbox: GroupBox { Material.elevation: 10 }
            property Frame frame: Frame { Material.elevation: 10 }
            property Menu menu: Menu { }
            property Page page: Page { }
            property Pane pane: Pane { }
            property Popup popup: Popup { }
            property TabBar tabbar: TabBar { }
            property ToolBar toolbar: ToolBar { }
            property ToolTip tooltip: ToolTip { }
        }
    }

    function test_background_data() {
        return [
            { tag: "button", inherit: false, wait: 400 },
            { tag: "combobox", inherit: false, wait: 400 },
            { tag: "drawer", inherit: true },
            { tag: "groupbox", inherit: true },
            { tag: "frame", inherit: true },
            { tag: "menu", inherit: true },
            { tag: "page", inherit: true },
            { tag: "pane", inherit: true },
            { tag: "popup", inherit: true },
            { tag: "tabbar", inherit: true },
            { tag: "toolbar", inherit: false },
            { tag: "tooltip", inherit: false }
        ]
    }

    function test_background(data) {
        var window = backgroundControls.createObject(testCase)
        verify(window)

        var control = window[data.tag]
        verify(control)

        control.parent = window.contentItem
        control.visible = true

        var defaultBackground = control.background.color

        window.Material.background = "#ff0000"
        compare(window.color, "#ff0000")

        // For controls that have an animated background color, we wait the length
        // of the color animation to be sure that the color hasn't actually changed.
        if (data.wait)
            wait(data.wait)

        // We want the control's background color to be equal to the window's background
        // color, because we want the color to propagate to items that might actually use
        // it... Button, ComboBox, ToolBar and ToolTip have a special background color,
        // so they don't use the generic background color unless explicitly set, so we
        // compare the actual background rect color instead.
        if (data.inherit)
            compare(control.background.color, "#ff0000")
        else
            compare(control.background.color, defaultBackground)

        control.Material.background = "#0000ff"
        tryCompare(control.background, "color", "#0000ff")

        window.destroy()
    }

    Component {
        id: busyIndicator
        BusyIndicator { }
    }

    function test_shade() {
        var control = busyIndicator.createObject(testCase)

        compare(control.contentItem.color.toString(), Material.color(Material.Pink, Material.Shade500))
        control.Material.theme = Material.Dark
        compare(control.contentItem.color.toString(), Material.color(Material.Pink, Material.Shade200))

        control.destroy()
    }
}
