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

#ifndef ACTIVEQT_EXTRAINFO_H
#define ACTIVEQT_EXTRAINFO_H

#include <QtDesigner/QDesignerExtraInfoExtension>
#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QExtensionFactory>

#include <QtCore/QPointer>

QT_BEGIN_NAMESPACE

class QDesignerAxWidget;

class QAxWidgetExtraInfo: public QObject, public QDesignerExtraInfoExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerExtraInfoExtension)
public:
    QAxWidgetExtraInfo(QDesignerAxWidget *widget, QDesignerFormEditorInterface *core, QObject *parent);

    QWidget *widget() const Q_DECL_OVERRIDE;
    QDesignerFormEditorInterface *core() const Q_DECL_OVERRIDE;

    bool saveUiExtraInfo(DomUI *ui) Q_DECL_OVERRIDE;
    bool loadUiExtraInfo(DomUI *ui) Q_DECL_OVERRIDE;

    bool saveWidgetExtraInfo(DomWidget *ui_widget) Q_DECL_OVERRIDE;
    bool loadWidgetExtraInfo(DomWidget *ui_widget) Q_DECL_OVERRIDE;

private:
    QPointer<QDesignerAxWidget> m_widget;
    QPointer<QDesignerFormEditorInterface> m_core;
};

class QAxWidgetExtraInfoFactory: public QExtensionFactory
{
    Q_OBJECT
public:
    explicit QAxWidgetExtraInfoFactory(QDesignerFormEditorInterface *core, QExtensionManager *parent = 0);

protected:
    QObject *createExtension(QObject *object, const QString &iid, QObject *parent) const Q_DECL_OVERRIDE;

private:
    QDesignerFormEditorInterface *m_core;
};

QT_END_NAMESPACE

#endif // ACTIVEQT_EXTRAINFO_H
