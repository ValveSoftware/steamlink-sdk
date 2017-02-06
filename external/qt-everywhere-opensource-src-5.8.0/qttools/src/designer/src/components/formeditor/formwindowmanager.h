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

#ifndef FORMWINDOWMANAGER_H
#define FORMWINDOWMANAGER_H

#include "formeditor_global.h"

#include <QtDesigner/private/qdesigner_formwindowmanager_p.h>

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/QMap>

QT_BEGIN_NAMESPACE

class QAction;
class QActionGroup;
class QUndoGroup;
class QDesignerFormEditorInterface;
class QDesignerWidgetBoxInterface;

namespace qdesigner_internal {

class FormWindow;
class PreviewManager;
class PreviewActionGroup;

class QT_FORMEDITOR_EXPORT FormWindowManager
    : public QDesignerFormWindowManager
{
    Q_OBJECT
public:
    explicit FormWindowManager(QDesignerFormEditorInterface *core, QObject *parent = 0);
    virtual ~FormWindowManager();

    QDesignerFormEditorInterface *core() const Q_DECL_OVERRIDE;

    QAction *action(Action action) const Q_DECL_OVERRIDE;
    QActionGroup *actionGroup(ActionGroup actionGroup) const Q_DECL_OVERRIDE;

    QDesignerFormWindowInterface *activeFormWindow() const Q_DECL_OVERRIDE;

    int formWindowCount() const Q_DECL_OVERRIDE;
    QDesignerFormWindowInterface *formWindow(int index) const Q_DECL_OVERRIDE;

    QDesignerFormWindowInterface *createFormWindow(QWidget *parentWidget = 0, Qt::WindowFlags flags = 0) Q_DECL_OVERRIDE;

    QPixmap createPreviewPixmap() const Q_DECL_OVERRIDE;

    bool eventFilter(QObject *o, QEvent *e) Q_DECL_OVERRIDE;

    void dragItems(const QList<QDesignerDnDItemInterface*> &item_list) Q_DECL_OVERRIDE;

    QUndoGroup *undoGroup() const;

    PreviewManager *previewManager() const Q_DECL_OVERRIDE { return m_previewManager; }

public slots:
    void addFormWindow(QDesignerFormWindowInterface *formWindow) Q_DECL_OVERRIDE;
    void removeFormWindow(QDesignerFormWindowInterface *formWindow) Q_DECL_OVERRIDE;
    void setActiveFormWindow(QDesignerFormWindowInterface *formWindow) Q_DECL_OVERRIDE;
    void closeAllPreviews() Q_DECL_OVERRIDE;
    void deviceProfilesChanged();

private slots:
#ifndef QT_NO_CLIPBOARD
    void slotActionCutActivated();
    void slotActionCopyActivated();
    void slotActionPasteActivated();
#endif
    void slotActionDeleteActivated();
    void slotActionSelectAllActivated();
    void slotActionLowerActivated();
    void slotActionRaiseActivated();
    void createLayout();
    void slotActionBreakLayoutActivated();
    void slotActionAdjustSizeActivated();
    void slotActionSimplifyLayoutActivated();
    void showPreview() Q_DECL_OVERRIDE;
    void slotActionGroupPreviewInStyle(const QString &style, int deviceProfileIndex);
    void slotActionShowFormWindowSettingsDialog();

    void slotUpdateActions();

private:
    void setupActions();
    FormWindow *findFormWindow(QWidget *w);
    QWidget *findManagedWidget(FormWindow *fw, QWidget *w);

    void setCurrentUndoStack(QUndoStack *stack);

private:
    enum CreateLayoutContext { LayoutContainer, LayoutSelection, MorphLayout };

    QDesignerFormEditorInterface *m_core;
    FormWindow *m_activeFormWindow;
    QList<FormWindow*> m_formWindows;

    PreviewManager *m_previewManager;

    /* Context of the layout actions and base for morphing layouts. Determined
     * in slotUpdateActions() and used later on in the action slots. */
    CreateLayoutContext m_createLayoutContext;
    QWidget *m_morphLayoutContainer;

    // edit actions
#ifndef QT_NO_CLIPBOARD
    QAction *m_actionCut;
    QAction *m_actionCopy;
    QAction *m_actionPaste;
#endif
    QAction *m_actionSelectAll;
    QAction *m_actionDelete;
    QAction *m_actionLower;
    QAction *m_actionRaise;
    // layout actions
    QAction *m_actionHorizontalLayout;
    QAction *m_actionVerticalLayout;
    QAction *m_actionFormLayout;
    QAction *m_actionSplitHorizontal;
    QAction *m_actionSplitVertical;
    QAction *m_actionGridLayout;
    QAction *m_actionBreakLayout;
    QAction *m_actionSimplifyLayout;
    QAction *m_actionAdjustSize;
    // preview actions
    QAction *m_actionDefaultPreview;
    mutable PreviewActionGroup *m_actionGroupPreviewInStyle;
    QAction *m_actionShowFormWindowSettingsDialog;

    QAction *m_actionUndo;
    QAction *m_actionRedo;

    QMap<QWidget *,bool> getUnsortedLayoutsToBeBroken(bool firstOnly) const;
    bool hasLayoutsToBeBroken() const;
    QWidgetList layoutsToBeBroken(QWidget *w) const;
    QWidgetList layoutsToBeBroken() const;

    QUndoGroup *m_undoGroup;

};

}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // FORMWINDOWMANAGER_H
