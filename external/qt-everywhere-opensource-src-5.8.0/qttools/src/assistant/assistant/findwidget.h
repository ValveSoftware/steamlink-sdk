/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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
#ifndef FINDWIDGET_H
#define FINDWIDGET_H

#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class QCheckBox;
class QLabel;
class QLineEdit;
class QToolButton;

class FindWidget : public QWidget
{
    Q_OBJECT
public:
    FindWidget(QWidget *parent = 0);
    ~FindWidget();

    void show();
    void showAndClear();

    QString text() const;
    bool caseSensitive() const;

    void setPalette(bool found);
    void setTextWrappedVisible(bool visible);

signals:
    void findNext();
    void findPrevious();
    void escapePressed();
    void find(const QString &text, bool forward, bool incremental);

protected:
    void hideEvent(QHideEvent* event);
    void showEvent(QShowEvent * event);

private slots:
    void updateButtons();
    void textChanged(const QString &text);

private:
    bool eventFilter(QObject *object, QEvent *e);
    QToolButton* setupToolButton(const QString &text, const QString &icon);

private:
    QPalette appPalette;

    QLineEdit *editFind;
    QCheckBox *checkCase;
    QLabel *labelWrapped;
    QToolButton *toolNext;
    QToolButton *toolClose;
    QToolButton *toolPrevious;
};

QT_END_NAMESPACE

#endif  // FINDWIDGET_H
