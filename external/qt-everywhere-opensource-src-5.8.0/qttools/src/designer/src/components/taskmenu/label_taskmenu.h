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

#ifndef LABEL_TASKMENU_H
#define LABEL_TASKMENU_H

#include <QtWidgets/QLabel>
#include <QtCore/QPointer>

#include <qdesigner_taskmenu_p.h>
#include <extensionfactory_p.h>

QT_BEGIN_NAMESPACE

class QDesignerFormWindowInterface;

namespace qdesigner_internal {

class LabelTaskMenu: public QDesignerTaskMenu
{
    Q_OBJECT
public:
    explicit LabelTaskMenu(QLabel *button, QObject *parent = 0);

    QAction *preferredEditAction() const Q_DECL_OVERRIDE;
    QList<QAction*> taskActions() const Q_DECL_OVERRIDE;

private slots:
    void editRichText();

private:
    QLabel *m_label;
    QList<QAction*> m_taskActions;
    QAction *m_editRichTextAction;
    QAction *m_editPlainTextAction;
};

typedef ExtensionFactory<QDesignerTaskMenuExtension, QLabel, LabelTaskMenu>  LabelTaskMenuFactory;
}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // LABEL_TASKMENU_H
