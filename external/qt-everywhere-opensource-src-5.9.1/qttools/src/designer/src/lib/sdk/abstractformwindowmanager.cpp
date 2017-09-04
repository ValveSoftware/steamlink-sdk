/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "abstractformwindowmanager.h"

#include <QtCore/QMap>

QT_BEGIN_NAMESPACE

/*!
    \class QDesignerFormWindowManagerInterface

    \brief The QDesignerFormWindowManagerInterface class allows you to
    manipulate the collection of form windows in Qt Designer, and
    control Qt Designer's form editing actions.

    \inmodule QtDesigner

    QDesignerFormWindowManagerInterface is not intended to be
    instantiated directly. \QD uses the form window manager to
    control the various form windows in its workspace. You can
    retrieve an interface to \QD's form window manager using
    the QDesignerFormEditorInterface::formWindowManager()
    function. For example:

    \snippet lib/tools_designer_src_lib_sdk_abstractformwindowmanager.cpp 0

    When implementing a custom widget plugin, a pointer to \QD's
    current QDesignerFormEditorInterface object (\c formEditor in the
    example above) is provided by the
    QDesignerCustomWidgetInterface::initialize() function's parameter.
    You must subclass the QDesignerCustomWidgetInterface to expose
    your plugin to Qt Designer.

    The form window manager interface provides the createFormWindow()
    function that enables you to create a new form window which you
    can add to the collection of form windows that the manager
    maintains, using the addFormWindow() slot. It also provides the
    formWindowCount() function returning the number of form windows
    currently under the manager's control, the formWindow() function
    returning the form window associated with a given index, and the
    activeFormWindow() function returning the currently selected form
    window. The removeFormWindow() slot allows you to reduce the
    number of form windows the manager must maintain, and the
    setActiveFormWindow() slot allows you to change the form window
    focus in \QD's workspace.

    In addition, QDesignerFormWindowManagerInterface contains a
    collection of functions that enables you to intervene and control
    \QD's form editing actions. All these functions return the
    original action, making it possible to propagate the function
    further after intervention.

    Finally, the interface provides three signals which are emitted
    when a form window is added, when the currently selected form
    window changes, or when a form window is removed, respectively. All
    the signals carry the form window in question as their parameter.

    \sa QDesignerFormEditorInterface, QDesignerFormWindowInterface
*/

/*!
    \enum QDesignerFormWindowManagerInterface::Action

    Specifies an action of \QD.

    \sa action()

    \since 5.0
    \value CutAction         Clipboard Cut
    \value CopyAction        Clipboard Copy
    \value PasteAction       Clipboard Paste
    \value DeleteAction      Clipboard Delete
    \value SelectAllAction   Select All
    \value LowerAction       Lower current widget
    \value RaiseAction       Raise current widget
    \value UndoAction        Undo
    \value RedoAction        Redo
    \value HorizontalLayoutAction Lay out using QHBoxLayout
    \value VerticalLayoutAction   Lay out using QVBoxLayout
    \value SplitHorizontalAction  Lay out in horizontal QSplitter
    \value SplitVerticalAction    Lay out in vertical QSplitter
    \value GridLayoutAction       Lay out using QGridLayout
    \value FormLayoutAction       Lay out using QFormLayout
    \value BreakLayoutAction      Break existing layout
    \value AdjustSizeAction       Adjust size
    \value SimplifyLayoutAction   Simplify QGridLayout or QFormLayout
    \value DefaultPreviewAction   Create a preview in default style
    \value FormWindowSettingsDialogAction Show dialog with form settings
*/

/*!
    \enum QDesignerFormWindowManagerInterface::ActionGroup

    Specifies an action group of \QD.

    \sa actionGroup()
    \since 5.0
    \value StyledPreviewActionGroup Action group containing styled preview actions
*/

/*!
    Constructs an interface with the given \a parent for the form window
    manager.
*/
QDesignerFormWindowManagerInterface::QDesignerFormWindowManagerInterface(QObject *parent)
    : QObject(parent)
{
}

/*!
    Destroys the interface for the form window manager.
*/
QDesignerFormWindowManagerInterface::~QDesignerFormWindowManagerInterface()
{
}

/*!
    Allows you to intervene and control \QD's "cut" action. The function
    returns the original action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionCut() const
{
    return action(CutAction);
}

/*!
    Allows you to intervene and control \QD's "copy" action. The
    function returns the original action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionCopy() const
{
    return action(CopyAction);
}

/*!
    Allows you to intervene and control \QD's "paste" action. The
    function returns the original action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionPaste() const
{
    return action(PasteAction);
}

/*!
    Allows you to intervene and control \QD's "delete" action. The function
    returns the original action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionDelete() const
{
    return action(DeleteAction);
}

/*!
    Allows you to intervene and control \QD's "select all" action. The
    function returns the original action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionSelectAll() const
{
    return action(SelectAllAction);
}

/*!
    Allows you to intervene and control the action of lowering a form
    window in \QD's workspace. The function returns the original
    action.

    \sa QAction
    \obsolete

    Use action() instead.
*/

QAction *QDesignerFormWindowManagerInterface::actionLower() const
{
    return action(LowerAction);
}

/*!
    Allows you to intervene and control the action of raising of a
    form window in \QD's workspace. The function returns the original
    action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionRaise() const
{
    return action(RaiseAction);
}

/*!
    Allows you to intervene and control a request for horizontal
    layout for a form window in \QD's workspace. The function returns
    the original action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionHorizontalLayout() const
{
    return action(HorizontalLayoutAction);
}

/*!
    Allows you to intervene and control a request for vertical layout
    for a form window in \QD's workspace. The function returns the
    original action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionVerticalLayout() const
{
    return action(VerticalLayoutAction);
}

/*!
    Allows you to intervene and control \QD's "split horizontal"
    action. The function returns the original action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionSplitHorizontal() const
{
    return action(SplitHorizontalAction);
}

/*!
    Allows you to intervene and control \QD's "split vertical"
    action. The function returns the original action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionSplitVertical() const
{
    return action(SplitVerticalAction);
}

/*!
    Allows you to intervene and control a request for grid layout for
    a form window in \QD's workspace. The function returns the
    original action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionGridLayout() const
{
    return action(GridLayoutAction);
}

/*!
    Allows you to intervene and control \QD's "form layout" action. The
    function returns the original action.

    \sa QAction
    \since 4.4
    \obsolete

    Use action() instead.
*/

QAction *QDesignerFormWindowManagerInterface::actionFormLayout() const
{
    return action(FormLayoutAction);
}

/*!
    Allows you to intervene and control \QD's "break layout" action. The
    function returns the original action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionBreakLayout() const
{
    return action(BreakLayoutAction);
}

/*!
    Allows you to intervene and control \QD's "adjust size" action. The
    function returns the original action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionAdjustSize() const
{
    return action(AdjustSizeAction);
}

/*!
    Allows you to intervene and control \QD's "simplify layout" action. The
    function returns the original action.

    \sa QAction
    \since 4.4
    \obsolete

    Use action() instead.
*/

QAction *QDesignerFormWindowManagerInterface::actionSimplifyLayout() const
{
    return action(SimplifyLayoutAction);
}

/*!
  \fn virtual QDesignerFormWindowInterface *QDesignerFormWindowManagerInterface::activeFormWindow() const
   Returns the currently active form window in \QD's workspace.

   \sa setActiveFormWindow(), removeFormWindow()
*/

/*!
    \fn virtual QDesignerFormEditorInterface *QDesignerFormWindowManagerInterface::core() const
    Returns a pointer to \QD's current QDesignerFormEditorInterface
    object.
*/

/*!
   \fn virtual void QDesignerFormWindowManagerInterface::addFormWindow(QDesignerFormWindowInterface *formWindow)
   Adds the given \a formWindow to the collection of windows that
   \QD's form window manager maintains.

   \sa formWindowAdded()
*/

/*!
   \fn virtual void QDesignerFormWindowManagerInterface::removeFormWindow(QDesignerFormWindowInterface *formWindow)
   Removes the given \a formWindow from the collection of windows that
   \QD's form window manager maintains.

   \sa formWindow(), formWindowRemoved()
*/

/*!
   \fn virtual void QDesignerFormWindowManagerInterface::setActiveFormWindow(QDesignerFormWindowInterface *formWindow)
   Sets the given \a formWindow to be the currently active form window in
   \QD's workspace.

   \sa activeFormWindow(), activeFormWindowChanged()
*/

/*!
   \fn int QDesignerFormWindowManagerInterface::formWindowCount() const
   Returns the number of form windows maintained by \QD's form window
   manager.
*/

/*!
   \fn QDesignerFormWindowInterface *QDesignerFormWindowManagerInterface::formWindow(int index) const
   Returns the form window at the given \a index.

   \sa setActiveFormWindow(), removeFormWindow()
*/

/*!
  \fn QDesignerFormWindowInterface *QDesignerFormWindowManagerInterface::createFormWindow(QWidget *parent, Qt::WindowFlags flags)

   Creates a form window with the given \a parent and the given window
   \a flags.

   \sa addFormWindow()
*/

/*!
    Allows you to intervene and control \QD's "undo" action. The
    function returns the original action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionUndo() const
{
    return action(UndoAction);
}

/*!
    Allows you to intervene and control \QD's "redo" action. The
    function returns the original action.

    \sa QAction
    \obsolete

    Use action() instead.
*/
QAction *QDesignerFormWindowManagerInterface::actionRedo() const
{
    return action(RedoAction);
}

/*!
    \fn void QDesignerFormWindowManagerInterface::formWindowAdded(QDesignerFormWindowInterface *formWindow)

    This signal is emitted when a new form window is added to the
    collection of windows that \QD's form window manager maintains. A
    pointer to the new \a formWindow is passed as an argument.

    \sa addFormWindow(), setActiveFormWindow()
*/

/*!
    \fn void QDesignerFormWindowManagerInterface::formWindowSettingsChanged(QDesignerFormWindowInterface *formWindow)

    This signal is emitted when the settings of the form window change. It can be used to update
    window titles, etc. accordingly. A pointer to the \a formWindow is passed as an argument.

    \sa FormWindowSettingsDialogAction
*/

/*!
    \fn void QDesignerFormWindowManagerInterface::formWindowRemoved(QDesignerFormWindowInterface *formWindow)

    This signal is emitted when a form window is removed from the
    collection of windows that \QD's form window manager maintains. A
    pointer to the removed \a formWindow is passed as an argument.

    \sa removeFormWindow()
*/

/*!
    \fn void QDesignerFormWindowManagerInterface::activeFormWindowChanged(QDesignerFormWindowInterface *formWindow)

    This signal is emitted when the contents of the currently active
    form window in \QD's workspace changed. A pointer to the currently
    active \a formWindow is passed as an argument.

    \sa activeFormWindow()
*/

/*!
    \fn void QDesignerFormWindowManagerInterface::dragItems(const QList<QDesignerDnDItemInterface*> &item_list)

    \internal
*/

/*!
    \fn virtual QAction QDesignerFormWindowManagerInterface::action(Action action) const

    Returns the action specified by the enumeration value \a action.

    Obsoletes the action accessors of Qt 4.X.

    \since 5.0
*/

/*!
    \fn virtual QActionGroup *QDesignerFormWindowManagerInterface::actionGroup(ActionGroup actionGroup) const

    Returns the action group specified by the enumeration value \a actionGroup.

    \since 5.0
*/

/*!
    \fn virtual void QDesignerFormWindowManagerInterface::showPreview()

    Show a preview of the current form using the default parameters.

    \since 5.0
    \sa closeAllPreviews()
*/

/*!
    \fn virtual void QDesignerFormWindowManagerInterface::closeAllPreviews()

    Close all currently open previews.

    \since 5.0
    \sa showPreview()
*/

/*!
    \fn virtual void QDesignerFormWindowManagerInterface::showPluginDialog()

    Opens a dialog showing the plugins loaded by \QD's and its plugin load failures.

    \since 5.0
*/

QT_END_NAMESPACE
