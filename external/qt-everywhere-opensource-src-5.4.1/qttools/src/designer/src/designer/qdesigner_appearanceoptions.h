/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Designer of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QDESIGNER_APPEARANCEOPTIONS_H
#define QDESIGNER_APPEARANCEOPTIONS_H

#include "designer_enums.h"
#include "qdesigner_toolwindow.h"

#include <QtDesigner/QDesignerOptionsPageInterface>

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class QDesignerFormEditorInterface;
class QDesignerSettings;

namespace Ui {
    class AppearanceOptionsWidget;
}

/* AppearanceOptions data */
struct AppearanceOptions {
    AppearanceOptions();
    bool equals(const AppearanceOptions&) const;
    void toSettings(QDesignerSettings &) const;
    void fromSettings(const QDesignerSettings &);

    UIMode uiMode;
    ToolWindowFontSettings toolWindowFontSettings;
};

inline bool operator==(const AppearanceOptions &ao1, const AppearanceOptions &ao2)
{
    return ao1.equals(ao2);
}

inline bool operator!=(const AppearanceOptions &ao1, const AppearanceOptions &ao2)
{
    return !ao1.equals(ao2);
}

/* QDesignerAppearanceOptionsWidget: Let the user edit AppearanceOptions */
class QDesignerAppearanceOptionsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit QDesignerAppearanceOptionsWidget(QWidget *parent = 0);
    ~QDesignerAppearanceOptionsWidget();

    AppearanceOptions appearanceOptions() const;
    void setAppearanceOptions(const AppearanceOptions &ao);

signals:
    void uiModeChanged(bool modified);

private slots:
    void slotUiModeComboChanged();

private:
    UIMode uiMode() const;

    Ui::AppearanceOptionsWidget *m_ui;
    UIMode m_initialUIMode;
};

/* The options page for appearance options. */

class QDesignerAppearanceOptionsPage : public QObject, public QDesignerOptionsPageInterface
{
    Q_OBJECT

public:
    QDesignerAppearanceOptionsPage(QDesignerFormEditorInterface *core);

    QString name() const;
    QWidget *createPage(QWidget *parent);
    virtual void apply();
    virtual void finish();

signals:
    void settingsChanged();

private:
    QDesignerFormEditorInterface *m_core;
    QPointer<QDesignerAppearanceOptionsWidget> m_widget;
    AppearanceOptions m_initialOptions;
};

QT_END_NAMESPACE

#endif // QDESIGNER_APPEARANCEOPTIONS_H
