/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#ifndef QQUICKWEBENGINDIALOGREQUESTS_H
#define QQUICKWEBENGINDIALOGREQUESTS_H

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

#include <private/qtwebengineglobal_p.h>
#include <QtCore/QUrl>
#include <QtCore/QWeakPointer>
#include <QtCore/QRect>
#include <QtGui/QColor>

namespace QtWebEngineCore {
    class AuthenticationDialogController;
    class ColorChooserController;
    class FilePickerController;
    class JavaScriptDialogController;
}

QT_BEGIN_NAMESPACE

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineAuthenticationDialogRequest : public QObject {
    Q_OBJECT
public:

    enum AuthenticationType {
        AuthenticationTypeHTTP,
        AuthenticationTypeProxy
    };

    Q_ENUM(AuthenticationType)

    Q_PROPERTY(QUrl url READ url CONSTANT FINAL)
    Q_PROPERTY(QString realm READ realm CONSTANT FINAL)
    Q_PROPERTY(QString proxyHost READ proxyHost CONSTANT FINAL)
    Q_PROPERTY(AuthenticationType type READ type CONSTANT FINAL)
    Q_PROPERTY(bool accepted READ isAccepted WRITE setAccepted FINAL)

    ~QQuickWebEngineAuthenticationDialogRequest();

    QUrl url() const;
    QString realm() const;
    QString proxyHost() const;
    AuthenticationType type() const;
    bool isAccepted() const;
    void setAccepted(bool accepted);

public slots:
    void dialogAccept(const QString &user, const QString &password);
    void dialogReject();

private:
    QQuickWebEngineAuthenticationDialogRequest(QSharedPointer<QtWebEngineCore::AuthenticationDialogController> controller,
                                    QObject *parent = nullptr);
    QWeakPointer<QtWebEngineCore::AuthenticationDialogController> m_controller;
    QUrl m_url;
    QString m_realm;
    AuthenticationType m_type;
    QString m_host;
    bool m_accepted;
    friend class QQuickWebEngineViewPrivate;
    Q_DISABLE_COPY(QQuickWebEngineAuthenticationDialogRequest)
};

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineJavaScriptDialogRequest : public QObject {
    Q_OBJECT
public:

    enum DialogType {
        DialogTypeAlert,
        DialogTypeConfirm,
        DialogTypePrompt,
        DialogTypeBeforeUnload,
    };
    Q_ENUM(DialogType)

    Q_PROPERTY(QString message READ message CONSTANT FINAL)
    Q_PROPERTY(QString defaultText READ defaultText CONSTANT FINAL)
    Q_PROPERTY(QString title READ title CONSTANT FINAL)
    Q_PROPERTY(DialogType type READ type CONSTANT FINAL)
    Q_PROPERTY(QUrl securityOrigin READ securityOrigin CONSTANT FINAL)
    Q_PROPERTY(bool accepted READ isAccepted WRITE setAccepted FINAL)

    ~QQuickWebEngineJavaScriptDialogRequest();

    QString message() const;
    QString defaultText() const;
    QString title() const;
    DialogType type() const;
    QUrl securityOrigin() const;
    bool isAccepted() const;
    void setAccepted(bool accepted);

public slots:
    void dialogAccept(const QString& text = QString());
    void dialogReject();

private:
    QQuickWebEngineJavaScriptDialogRequest(QSharedPointer<QtWebEngineCore::JavaScriptDialogController> controller,
                                    QObject *parent = nullptr);
    QWeakPointer<QtWebEngineCore::JavaScriptDialogController> m_controller;
    QString m_message;
    QString m_defaultPrompt;
    QString m_title;
    DialogType m_type;
    QUrl m_securityOrigin;
    bool m_accepted;
    friend class QQuickWebEngineViewPrivate;
    Q_DISABLE_COPY(QQuickWebEngineJavaScriptDialogRequest)
};

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineColorDialogRequest : public QObject {
    Q_OBJECT
public:

    Q_PROPERTY(QColor color READ color CONSTANT FINAL)
    Q_PROPERTY(bool accepted READ isAccepted WRITE setAccepted FINAL)

    ~QQuickWebEngineColorDialogRequest();

    QColor color() const;
    bool isAccepted() const;
    void setAccepted(bool accepted);

public slots:
    void dialogAccept(const QColor &color);
    void dialogReject();

private:
    QQuickWebEngineColorDialogRequest(QSharedPointer<QtWebEngineCore::ColorChooserController> controller,
                                    QObject *parent = nullptr);
    QWeakPointer<QtWebEngineCore::ColorChooserController> m_controller;
    QColor m_color;
    bool m_accepted;
    friend class QQuickWebEngineViewPrivate;
    Q_DISABLE_COPY(QQuickWebEngineColorDialogRequest)
};

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineFileDialogRequest : public QObject {
    Q_OBJECT
public:

    enum FileMode {
        FileModeOpen,
        FileModeOpenMultiple,
        FileModeUploadFolder,
        FileModeSave
    };
    Q_ENUM(FileMode)

    Q_PROPERTY(QString defaultFileName READ defaultFileName CONSTANT FINAL)
    Q_PROPERTY(QStringList acceptedMimeTypes READ acceptedMimeTypes CONSTANT FINAL)
    Q_PROPERTY(FileMode mode READ mode CONSTANT FINAL)
    Q_PROPERTY(bool accepted READ isAccepted WRITE setAccepted FINAL)

    ~QQuickWebEngineFileDialogRequest();

    QStringList acceptedMimeTypes() const;
    QString defaultFileName() const;
    FileMode mode() const;
    bool isAccepted() const;
    void setAccepted(bool accepted);

public slots:
    void dialogAccept(const QStringList &files);
    void dialogReject();

private:
    QQuickWebEngineFileDialogRequest(QSharedPointer<QtWebEngineCore::FilePickerController> controller,
                                    QObject *parent = nullptr);
    QWeakPointer<QtWebEngineCore::FilePickerController> m_controller;
    QString m_filename;
    QStringList m_acceptedMimeTypes;
    FileMode m_mode;
    bool m_accepted;
    friend class QQuickWebEngineViewPrivate;
    Q_DISABLE_COPY(QQuickWebEngineFileDialogRequest)
};

class Q_WEBENGINE_PRIVATE_EXPORT QQuickWebEngineFormValidationMessageRequest : public QObject {
    Q_OBJECT
public:
    enum RequestType {
        Show,
        Hide,
        Move,
    };
    Q_ENUM(RequestType)
    Q_PROPERTY(QRect anchor READ anchor CONSTANT FINAL)
    Q_PROPERTY(QString text READ text CONSTANT FINAL)
    Q_PROPERTY(QString subText READ subText CONSTANT FINAL)
    Q_PROPERTY(RequestType type READ type CONSTANT FINAL)
    Q_PROPERTY(bool accepted READ isAccepted WRITE setAccepted FINAL)

    ~QQuickWebEngineFormValidationMessageRequest();
    QRect anchor() const;
    QString text() const;
    QString subText() const;
    RequestType type() const;
    bool isAccepted() const;
    void setAccepted(bool accepted);

private:
    QQuickWebEngineFormValidationMessageRequest(RequestType type, const QRect &anchor = QRect(),
                                      const QString &mainText = QString(),
                                      const QString &subText = QString(),
                                      QObject *parent = nullptr);
    QRect m_anchor;
    QString m_mainText;
    QString m_subText;
    RequestType m_type;
    bool m_accepted;
    friend class QQuickWebEngineViewPrivate;
};

QT_END_NAMESPACE

#endif // QQUICKWEBENGINDIALOGREQUESTS_H
