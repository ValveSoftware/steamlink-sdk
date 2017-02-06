HEADERS += \
    $$PWD/qquickcalendarmodel_p.h \
    $$PWD/qquicktooltip_p.h \
    $$PWD/qquickspinboxvalidator_p.h \
    $$PWD/qquickrangemodel_p.h \
    $$PWD/qquickrangemodel_p_p.h \
    $$PWD/qquickrangeddate_p.h \
    $$PWD/qquickcontrolsettings_p.h \
    $$PWD/qquickwheelarea_p.h \
    $$PWD/qquickabstractstyle_p.h \
    $$PWD/qquickpadding_p.h \
    $$PWD/qquickcontrolsprivate_p.h \
    $$PWD/qquicktreemodeladaptor_p.h \
    $$PWD/qquicksceneposlistener_p.h

SOURCES += \
    $$PWD/qquickcalendarmodel.cpp \
    $$PWD/qquicktooltip.cpp \
    $$PWD/qquickspinboxvalidator.cpp \
    $$PWD/qquickrangemodel.cpp \
    $$PWD/qquickrangeddate.cpp \
    $$PWD/qquickcontrolsettings.cpp \
    $$PWD/qquickwheelarea.cpp \
    $$PWD/qquickabstractstyle.cpp \
    $$PWD/qquicktreemodeladaptor.cpp \
    $$PWD/qquickcontrolsprivate.cpp \
    $$PWD/qquicksceneposlistener.cpp

!no_desktop {
    QT += widgets
    HEADERS += $$PWD/qquickstyleitem_p.h
    SOURCES += $$PWD/qquickstyleitem.cpp
}

# private qml files
PRIVATE_QML_FILES += \
    $$PWD/AbstractCheckable.qml \
    $$PWD/TabBar.qml \
    $$PWD/BasicButton.qml \
    $$PWD/Control.qml \
    $$PWD/CalendarHeaderModel.qml \
    $$PWD/CalendarUtils.js \
    $$PWD/FastGlow.qml \
    $$PWD/SourceProxy.qml\
    $$PWD/Style.qml \
    $$PWD/style.js \
    $$PWD/MenuItemSubControls.qml \
    $$PWD/ModalPopupBehavior.qml \
    $$PWD/StackViewSlideDelegate.qml \
    $$PWD/StackView.js \
    $$PWD/ScrollViewHelper.qml \
    $$PWD/ScrollBar.qml \
    $$PWD/SystemPaletteSingleton.qml \
    $$PWD/TableViewSelection.qml \
    $$PWD/TextHandle.qml \
    $$PWD/TextSingleton.qml \
    $$PWD/FocusFrame.qml \
    $$PWD/ColumnMenuContent.qml \
    $$PWD/MenuContentItem.qml \
    $$PWD/MenuContentScroller.qml \
    $$PWD/ContentItem.qml \
    $$PWD/HoverButton.qml \
    $$PWD/TextInputWithHandles.qml \
    $$PWD/EditMenu.qml \
    $$PWD/EditMenu_base.qml \
    $$PWD/ToolMenuButton.qml \
    $$PWD/BasicTableView.qml \
    $$PWD/TableViewItemDelegateLoader.qml \
    $$PWD/TreeViewItemDelegateLoader.qml \
    $$PWD/qmldir

!qtquickcompiler: QML_FILES += $$PRIVATE_QML_FILES
