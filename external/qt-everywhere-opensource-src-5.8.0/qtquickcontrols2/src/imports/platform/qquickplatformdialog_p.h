/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Labs Platform module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKPLATFORMDIALOG_P_H
#define QQUICKPLATFORMDIALOG_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qobject.h>
#include <QtGui/qpa/qplatformtheme.h>
#include <QtGui/qpa/qplatformdialoghelper.h>
#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qqmllist.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QWindow;
class QPlatformDialogHelper;

class QQuickPlatformDialog : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QQmlListProperty<QObject> data READ data FINAL)
    Q_PROPERTY(QWindow *parentWindow READ parentWindow WRITE setParentWindow NOTIFY parentWindowChanged FINAL)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged FINAL)
    Q_PROPERTY(Qt::WindowFlags flags READ flags WRITE setFlags NOTIFY flagsChanged FINAL)
    Q_PROPERTY(Qt::WindowModality modality READ modality WRITE setModality NOTIFY modalityChanged FINAL)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)
    Q_PROPERTY(int result READ result WRITE setResult NOTIFY resultChanged FINAL)
    Q_CLASSINFO("DefaultProperty", "data")
    Q_ENUMS(StandardCode)

public:
    explicit QQuickPlatformDialog(QPlatformTheme::DialogType type, QObject *parent = nullptr);
    ~QQuickPlatformDialog();

    QPlatformDialogHelper *handle() const;

    QQmlListProperty<QObject> data();

    QWindow *parentWindow() const;
    void setParentWindow(QWindow *window);

    QString title() const;
    void setTitle(const QString &title);

    Qt::WindowFlags flags() const;
    void setFlags(Qt::WindowFlags flags);

    Qt::WindowModality modality() const;
    void setModality(Qt::WindowModality modality);

    bool isVisible() const;
    void setVisible(bool visible);

    enum StandardCode { Rejected, Accepted };

    int result() const;
    void setResult(int result);

public Q_SLOTS:
    void open();
    void close();
    virtual void accept();
    virtual void reject();
    virtual void done(int result);

Q_SIGNALS:
    void accepted();
    void rejected();
    void parentWindowChanged();
    void titleChanged();
    void flagsChanged();
    void modalityChanged();
    void visibleChanged();
    void resultChanged();

protected:
    void classBegin() override;
    void componentComplete() override;

    bool create();
    void destroy();

    virtual bool useNativeDialog() const;
    virtual void onCreate(QPlatformDialogHelper *dialog);
    virtual void onShow(QPlatformDialogHelper *dialog);
    virtual void onHide(QPlatformDialogHelper *dialog);

    QWindow *findParentWindow() const;

private:
    bool m_visible;
    bool m_complete;
    int m_result;
    QWindow *m_parentWindow;
    QString m_title;
    Qt::WindowFlags m_flags;
    Qt::WindowModality m_modality;
    QPlatformTheme::DialogType m_type;
    QList<QObject *> m_data;
    QPlatformDialogHelper *m_handle;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickPlatformDialog)

#endif // QQUICKPLATFORMDIALOG_P_H
