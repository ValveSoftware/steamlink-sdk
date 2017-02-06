/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef INPUTCONTEXT_H
#define INPUTCONTEXT_H

#include <QObject>
#include <QRectF>
#include <QLocale>
#include <QInputMethodEvent>
#include <QInputMethod>

namespace QtVirtualKeyboard {

class PlatformInputContext;
class InputEngine;
class ShiftHandler;
class InputContextPrivate;

class InputContext : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(InputContext)
    Q_DECLARE_PRIVATE(InputContext)
    Q_PROPERTY(bool focus READ focus NOTIFY focusChanged)
    Q_PROPERTY(bool shift READ shift WRITE setShift NOTIFY shiftChanged)
    Q_PROPERTY(bool capsLock READ capsLock WRITE setCapsLock NOTIFY capsLockChanged)
    Q_PROPERTY(int cursorPosition READ cursorPosition NOTIFY cursorPositionChanged)
    Q_PROPERTY(Qt::InputMethodHints inputMethodHints READ inputMethodHints NOTIFY inputMethodHintsChanged)
    Q_PROPERTY(QString preeditText READ preeditText WRITE setPreeditText NOTIFY preeditTextChanged)
    Q_PROPERTY(QString surroundingText READ surroundingText NOTIFY surroundingTextChanged)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY selectedTextChanged)
    Q_PROPERTY(QRectF anchorRectangle READ anchorRectangle NOTIFY anchorRectangleChanged)
    Q_PROPERTY(QRectF cursorRectangle READ cursorRectangle NOTIFY cursorRectangleChanged)
    Q_PROPERTY(QRectF keyboardRectangle READ keyboardRectangle WRITE setKeyboardRectangle NOTIFY keyboardRectangleChanged)
    Q_PROPERTY(QRectF previewRectangle READ previewRectangle WRITE setPreviewRectangle NOTIFY previewRectangleChanged)
    Q_PROPERTY(bool previewVisible READ previewVisible WRITE setPreviewVisible NOTIFY previewVisibleChanged)
    Q_PROPERTY(bool animating READ animating WRITE setAnimating NOTIFY animatingChanged)
    Q_PROPERTY(QString locale READ locale WRITE setLocale NOTIFY localeChanged)
    Q_PROPERTY(QObject *inputItem READ inputItem NOTIFY inputItemChanged)
    Q_PROPERTY(QtVirtualKeyboard::ShiftHandler *shiftHandler READ shiftHandler CONSTANT)
    Q_PROPERTY(QtVirtualKeyboard::InputEngine *inputEngine READ inputEngine CONSTANT)
    Q_PROPERTY(bool selectionControlVisible READ selectionControlVisible NOTIFY selectionControlVisibleChanged)
    Q_PROPERTY(bool anchorRectIntersectsClipRect READ anchorRectIntersectsClipRect NOTIFY anchorRectIntersectsClipRectChanged)
    Q_PROPERTY(bool cursorRectIntersectsClipRect READ cursorRectIntersectsClipRect NOTIFY cursorRectIntersectsClipRectChanged)

public:
    explicit InputContext(PlatformInputContext *parent = 0);
    ~InputContext();

    bool focus() const;
    bool shift() const;
    void setShift(bool enable);
    bool capsLock() const;
    void setCapsLock(bool enable);
    int cursorPosition() const;
    Qt::InputMethodHints inputMethodHints() const;
    QString preeditText() const;
    void setPreeditText(const QString &text, QList<QInputMethodEvent::Attribute> attributes = QList<QInputMethodEvent::Attribute>(), int replaceFrom = 0, int replaceLength = 0);
    QString surroundingText() const;
    QString selectedText() const;
    QRectF anchorRectangle() const;
    QRectF cursorRectangle() const;
    QRectF keyboardRectangle() const;
    void setKeyboardRectangle(QRectF rectangle);
    QRectF previewRectangle() const;
    void setPreviewRectangle(QRectF rectangle);
    bool previewVisible() const;
    void setPreviewVisible(bool visible);
    bool animating() const;
    void setAnimating(bool animating);
    QString locale() const;
    void setLocale(const QString &locale);
    Q_INVOKABLE void updateAvailableLocales(const QStringList &availableLocales);
    QObject *inputItem() const;
    ShiftHandler *shiftHandler() const;
    InputEngine *inputEngine() const;
    bool selectionControlVisible() const;
    bool anchorRectIntersectsClipRect() const;
    bool cursorRectIntersectsClipRect() const;

    Q_INVOKABLE void hideInputPanel();
    Q_INVOKABLE void sendKeyClick(int key, const QString &text, int modifiers = 0);
    Q_INVOKABLE void commit();
    Q_INVOKABLE void commit(const QString &text, int replaceFrom = 0, int replaceLength = 0);
    Q_INVOKABLE void clear();

    // Helper functions
    Q_INVOKABLE bool fileExists(const QUrl &fileUrl);
    Q_INVOKABLE bool hasEnterKeyAction(QObject *item) const;

    // For selection handles
    Q_INVOKABLE void setSelectionOnFocusObject(const QPointF &anchorPos, const QPointF &cursorPos);

signals:
    void focusChanged();
    void focusEditorChanged();
    void preeditTextChanged();
    void inputMethodHintsChanged();
    void surroundingTextChanged();
    void selectedTextChanged();
    void cursorPositionChanged();
    void anchorRectangleChanged();
    void cursorRectangleChanged();
    void shiftChanged();
    void capsLockChanged();
    void keyboardRectangleChanged();
    void previewRectangleChanged();
    void previewVisibleChanged();
    void animatingChanged();
    void localeChanged();
    void inputItemChanged();
    void selectionControlVisibleChanged();
    void navigationKeyPressed(int key, bool isAutoRepeat);
    void navigationKeyReleased(int key, bool isAutoRepeat);
    void anchorRectIntersectsClipRectChanged();
    void cursorRectIntersectsClipRectChanged();

private slots:
    void onInputItemChanged();

private:
    void setFocus(bool enable);
    void sendPreedit(const QString &text, const QList<QInputMethodEvent::Attribute> &attributes, int replaceFrom, int replaceLength);
    void reset();
    void externalCommit();
    void update(Qt::InputMethodQueries queries);
    void invokeAction(QInputMethod::Action action, int cursorPosition);
    bool filterEvent(const QEvent *event);

private:
    friend class PlatformInputContext;
};

} // namespace QtVirtualKeyboard

#endif
