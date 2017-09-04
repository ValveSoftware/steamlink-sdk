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

#ifndef CLIPBOARD_QT_H
#define CLIPBOARD_QT_H

#include "ui/base/clipboard/clipboard.h"

#include <QClipboard>
#include <QMap>
#include <QObject>

namespace QtWebEngineCore {

class ClipboardChangeObserver : public QObject {
    Q_OBJECT
public:
    ClipboardChangeObserver();
    quint64 getSequenceNumber(QClipboard::Mode mode) {
        return sequenceNumber.value(mode);
    }

private Q_SLOTS:
    void trackChange(QClipboard::Mode mode);

private:
    QMap<QClipboard::Mode, quint64> sequenceNumber;
};

class ClipboardQt : public ui::Clipboard {
public:
    uint64_t GetSequenceNumber(ui::ClipboardType type) const override;
    bool IsFormatAvailable(const FormatType& format, ui::ClipboardType type) const override;
    void Clear(ui::ClipboardType type) override;
    void ReadAvailableTypes(ui::ClipboardType type, std::vector<base::string16>* types, bool* contains_filenames) const override;
    void ReadText(ui::ClipboardType type, base::string16* result) const override;
    void ReadAsciiText(ui::ClipboardType type, std::string* result) const override;
    void ReadHTML(ui::ClipboardType type,
                        base::string16* markup,
                        std::string* src_url,
                        uint32_t* fragment_start,
                        uint32_t* fragment_end) const override;
    void ReadRTF(ui::ClipboardType type, std::string* result) const override;
    SkBitmap ReadImage(ui::ClipboardType type) const override;
    void ReadCustomData(ui::ClipboardType clipboard_type, const base::string16& type, base::string16* result) const override;
    void ReadBookmark(base::string16* title, std::string* url) const override;
    void ReadData(const FormatType& format, std::string* result) const override;

protected:
    void WriteObjects(ui::ClipboardType type, const ObjectMap& objects) override;
    void WriteText(const char* text_data, size_t text_len) override;
    void WriteHTML(const char* markup_data, size_t markup_len, const char* url_data, size_t url_len) override;
    void WriteRTF(const char* rtf_data, size_t data_len) override;
    void WriteBookmark(const char* title_data, size_t title_len, const char* url_data, size_t url_len) override;
    void WriteWebSmartPaste() override;
    void WriteBitmap(const SkBitmap& bitmap) override;
    void WriteData(const FormatType& format, const char* data_data, size_t data_len) override;
};

} // namespace QtWebEngineCore

#endif
