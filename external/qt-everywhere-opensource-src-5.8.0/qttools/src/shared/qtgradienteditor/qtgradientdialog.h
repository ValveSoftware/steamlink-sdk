/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QTGRADIENTDIALOG_H
#define QTGRADIENTDIALOG_H

#include <QtWidgets/QDialog>

QT_BEGIN_NAMESPACE

class QtGradientDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(QGradient gradient READ gradient WRITE setGradient)
    Q_PROPERTY(bool backgroundCheckered READ isBackgroundCheckered WRITE setBackgroundCheckered)
    Q_PROPERTY(bool detailsVisible READ detailsVisible WRITE setDetailsVisible)
    Q_PROPERTY(bool detailsButtonVisible READ isDetailsButtonVisible WRITE setDetailsButtonVisible)
public:
    QtGradientDialog(QWidget *parent = 0);
    ~QtGradientDialog();

    void setGradient(const QGradient &gradient);
    QGradient gradient() const;

    bool isBackgroundCheckered() const;
    void setBackgroundCheckered(bool checkered);

    bool detailsVisible() const;
    void setDetailsVisible(bool visible);

    bool isDetailsButtonVisible() const;
    void setDetailsButtonVisible(bool visible);

    QColor::Spec spec() const;
    void setSpec(QColor::Spec spec);

    static QGradient getGradient(bool *ok, const QGradient &initial, QWidget *parent = 0, const QString &caption = QString());
    static QGradient getGradient(bool *ok, QWidget *parent = 0, const QString &caption = QString());

private:
    QScopedPointer<class QtGradientDialogPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QtGradientDialog)
    Q_DISABLE_COPY(QtGradientDialog)
    Q_PRIVATE_SLOT(d_func(), void slotAboutToShowDetails(bool details, int extensionWidthHint))
};

QT_END_NAMESPACE

#endif
