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

#ifndef BUDDYEDITOR_H
#define BUDDYEDITOR_H

#include "buddyeditor_global.h"

#include <connectionedit_p.h>
#include <QtCore/QPointer>
#include <QtCore/QSet>

QT_BEGIN_NAMESPACE

class QDesignerFormWindowInterface;

class QLabel;

namespace qdesigner_internal {

class QT_BUDDYEDITOR_EXPORT BuddyEditor : public ConnectionEdit
{
    Q_OBJECT

public:
    BuddyEditor(QDesignerFormWindowInterface *form, QWidget *parent);

    QDesignerFormWindowInterface *formWindow() const;
    void setBackground(QWidget *background) Q_DECL_OVERRIDE;
    void deleteSelected() Q_DECL_OVERRIDE;

public slots:
    void updateBackground() Q_DECL_OVERRIDE;
    void widgetRemoved(QWidget *w) Q_DECL_OVERRIDE;
    void autoBuddy();

protected:
    QWidget *widgetAt(const QPoint &pos) const Q_DECL_OVERRIDE;
    Connection *createConnection(QWidget *source, QWidget *destination) Q_DECL_OVERRIDE;
    void endConnection(QWidget *target, const QPoint &pos) Q_DECL_OVERRIDE;
    void createContextMenu(QMenu &menu) Q_DECL_OVERRIDE;

private:
    QWidget *findBuddy(QLabel *l, const QWidgetList &existingBuddies) const;

    QPointer<QDesignerFormWindowInterface> m_formWindow;
    bool m_updating;
};

}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif
