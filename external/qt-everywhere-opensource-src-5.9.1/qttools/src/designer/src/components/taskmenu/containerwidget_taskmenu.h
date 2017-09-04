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

#ifndef CONTAINERWIDGER_TASKMENU_H
#define CONTAINERWIDGER_TASKMENU_H

#include <qdesigner_taskmenu_p.h>
#include <shared_enums_p.h>

#include <extensionfactory_p.h>

#include <QtCore/QPointer>

QT_BEGIN_NAMESPACE

class QDesignerFormWindowInterface;
class QDesignerFormEditorInterface;
class QDesignerContainerExtension;
class QAction;
class QMdiArea;
class QMenu;
class QWizard;

namespace qdesigner_internal {

class PromotionTaskMenu;

// ContainerWidgetTaskMenu: Task menu for containers with extension

class ContainerWidgetTaskMenu: public QDesignerTaskMenu
{
    Q_OBJECT
public:
    explicit ContainerWidgetTaskMenu(QWidget *widget, ContainerType type, QObject *parent = 0);
    virtual ~ContainerWidgetTaskMenu();

    QAction *preferredEditAction() const Q_DECL_OVERRIDE;
    QList<QAction*> taskActions() const Q_DECL_OVERRIDE;

private slots:
    void removeCurrentPage();
    void addPage();
    void addPageAfter();

protected:
    QDesignerContainerExtension *containerExtension() const;
    QList<QAction*> &containerActions() { return m_taskActions; }
    int pageCount() const;

private:
    QDesignerFormWindowInterface *formWindow() const;

private:
    static QString pageMenuText(ContainerType ct, int index, int count);
    bool canDeletePage() const;

    const ContainerType m_type;
    QWidget *m_containerWidget;
    QDesignerFormEditorInterface *m_core;
    PromotionTaskMenu *m_pagePromotionTaskMenu;
    QAction *m_pageMenuAction;
    QMenu *m_pageMenu;
    QList<QAction*> m_taskActions;
    QAction *m_actionInsertPageAfter;
    QAction *m_actionInsertPage;
    QAction *m_actionDeletePage;
};

// WizardContainerWidgetTaskMenu: Provide next/back since QWizard
// has modes in which the "Back" button is not visible.

class WizardContainerWidgetTaskMenu : public ContainerWidgetTaskMenu {
    Q_OBJECT
public:
    explicit WizardContainerWidgetTaskMenu(QWizard *w, QObject *parent = 0);

    QList<QAction*> taskActions() const Q_DECL_OVERRIDE;

private:
    QAction *m_nextAction;
    QAction *m_previousAction;
};


// MdiContainerWidgetTaskMenu: Provide tile/cascade for MDI containers in addition

class MdiContainerWidgetTaskMenu : public ContainerWidgetTaskMenu {
    Q_OBJECT
public:
    explicit MdiContainerWidgetTaskMenu(QMdiArea *m, QObject *parent = 0);

    QList<QAction*> taskActions() const Q_DECL_OVERRIDE;
private:
    void initializeActions();

    QAction *m_nextAction;
    QAction *m_previousAction;
    QAction *m_tileAction;
    QAction *m_cascadeAction;
};

class ContainerWidgetTaskMenuFactory: public QExtensionFactory
{
    Q_OBJECT
public:
    explicit ContainerWidgetTaskMenuFactory(QDesignerFormEditorInterface *core, QExtensionManager *extensionManager = 0);

protected:
    QObject *createExtension(QObject *object, const QString &iid, QObject *parent) const Q_DECL_OVERRIDE;

private:
    QDesignerFormEditorInterface *m_core;
};

}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // CONTAINERWIDGER_TASKMENU_H
