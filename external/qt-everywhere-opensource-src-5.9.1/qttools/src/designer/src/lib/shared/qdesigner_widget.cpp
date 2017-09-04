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

#include "qdesigner_widget_p.h"
#include "formwindowbase_p.h"
#include "grid_p.h"

#include <QtDesigner/QDesignerFormWindowInterface>
#include <QtGui/QPainter>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>
#include <QtGui/qevent.h>

QT_BEGIN_NAMESPACE

/* QDesignerDialog / QDesignerWidget are used to paint a grid on QDialog and QWidget main containers
 * and container extension pages.
 * The paint routines work as follows:
 * We need to clean the background here (to make the parent grid disappear in case we are a container page
 * and to make palette background settings take effect),
 * which would normally break style sheets with background settings.
 * So, we manually make the style paint after cleaning. On top comes the grid
 * In addition, this code works around
 * the QStyleSheetStyle setting Qt::WA_StyledBackground to false for subclasses of QWidget.
 */

QDesignerDialog::QDesignerDialog(QDesignerFormWindowInterface *fw, QWidget *parent) :
    QDialog(parent),
    m_formWindow(qobject_cast<qdesigner_internal::FormWindowBase*>(fw))
{
}

void QDesignerDialog::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    QStyleOption opt;
    opt.initFrom(this);
    p.fillRect(e->rect(), palette().brush(backgroundRole()));
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    if (m_formWindow && m_formWindow->gridVisible())
        m_formWindow->designerGrid().paint(p, this, e);
}

QDesignerWidget::QDesignerWidget(QDesignerFormWindowInterface* formWindow, QWidget *parent)  :
    QWidget(parent),
    m_formWindow(qobject_cast<qdesigner_internal::FormWindowBase*>(formWindow))
{
}

QDesignerWidget::~QDesignerWidget()
{
}

QDesignerFormWindowInterface* QDesignerWidget::formWindow() const
{
    return m_formWindow;
}

void QDesignerWidget::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    QStyleOption opt;
    opt.initFrom(this);
    p.fillRect(e->rect(), palette().brush(backgroundRole()));
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    if (m_formWindow && m_formWindow->gridVisible())
        m_formWindow->designerGrid().paint(p, this, e);
}

QT_END_NAMESPACE
