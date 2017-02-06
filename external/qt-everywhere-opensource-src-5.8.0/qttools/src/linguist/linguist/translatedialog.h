/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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

#ifndef TRANSLATEDIALOG_H
#define TRANSLATEDIALOG_H

#include "ui_translatedialog.h"
#include <QDialog>

QT_BEGIN_NAMESPACE

class TranslateDialog : public QDialog
{
    Q_OBJECT

public:
    enum {
        Skip,
        Translate,
        TranslateAll
    };

    TranslateDialog(QWidget *parent = 0);

    bool markFinished() const { return m_ui.ckMarkFinished->isChecked(); }
    Qt::CaseSensitivity caseSensitivity() const
        { return m_ui.ckMatchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive; }
    QString findText() const { return m_ui.ledFindWhat->text(); }
    QString replaceText() const { return m_ui.ledTranslateTo->text(); }

signals:
    void requestMatchUpdate(bool &hit);
    void activated(int mode);

protected:
    virtual void showEvent(QShowEvent *event);

private slots:
    void emitFindNext();
    void emitTranslateAndFindNext();
    void emitTranslateAll();
    void verifyText();

private:
    Ui::TranslateDialog m_ui;
};


QT_END_NAMESPACE
#endif  //TRANSLATEDIALOG_H

