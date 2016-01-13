/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "clipboard_qt.h"
#include "ui/base/clipboard/clipboard.h"

#include "type_conversion.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/custom_data_helper.h"

#include <QGuiApplication>
#include <QImage>
#include <QMimeData>

Q_GLOBAL_STATIC(ClipboardChangeObserver, clipboardChangeObserver)

ClipboardChangeObserver::ClipboardChangeObserver()
{
    connect(QGuiApplication::clipboard(), SIGNAL(changed(QClipboard::Mode)), SLOT(trackChange(QClipboard::Mode)));
}

void ClipboardChangeObserver::trackChange(QClipboard::Mode mode)
{
    ++sequenceNumber[mode];
}

namespace ui {

namespace {

const char kMimeTypeBitmap[] = "image/bmp";
const char kMimeTypeMozillaURL[] = "text/x-moz-url";
const char kMimeTypeWebCustomDataCopy[] = "chromium/x-web-custom-data";
const char kMimeTypePepperCustomData[] = "chromium/x-pepper-custom-data";
const char kMimeTypeWebkitSmartPaste[] = "chromium/x-webkit-paste";

QScopedPointer<QMimeData> uncommittedData;
QMimeData *getUncommittedData()
{
    if (!uncommittedData)
        uncommittedData.reset(new QMimeData);
    return uncommittedData.data();
}

}  // namespace

Clipboard::FormatType::FormatType()
{
}

Clipboard::FormatType::FormatType(const std::string& format_string)
    : data_(format_string)
{
}

Clipboard::FormatType::~FormatType()
{
}

std::string Clipboard::FormatType::Serialize() const
{
  return data_;
}

Clipboard::FormatType Clipboard::FormatType::Deserialize(const std::string& serialization)
{
  return FormatType(serialization);
}

bool Clipboard::FormatType::Equals(const FormatType& other) const
{
  return data_ == other.data_;
}

Clipboard::Clipboard()
{
}

Clipboard::~Clipboard()
{
}

void Clipboard::WriteObjects(ClipboardType type, const ObjectMap& objects)
{
    DCHECK(CalledOnValidThread());
    DCHECK(IsSupportedClipboardType(type));

    for (ObjectMap::const_iterator iter = objects.begin(); iter != objects.end(); ++iter)
        DispatchObject(static_cast<ObjectType>(iter->first), iter->second);

    // Commit the accumulated data.
    if (uncommittedData)
        QGuiApplication::clipboard()->setMimeData(uncommittedData.take(), type == CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);

    if (type == CLIPBOARD_TYPE_COPY_PASTE) {
        ObjectMap::const_iterator text_iter = objects.find(CBF_TEXT);
        if (text_iter != objects.end()) {
            // Copy text and SourceTag to the selection clipboard.
            ObjectMap::const_iterator next_iter = text_iter;
            WriteObjects(CLIPBOARD_TYPE_SELECTION, ObjectMap(text_iter, ++next_iter));
        }
    }
}

void Clipboard::WriteText(const char* text_data, size_t text_len)
{
    getUncommittedData()->setText(QString::fromUtf8(text_data, text_len));
}

void Clipboard::WriteHTML(const char* markup_data, size_t markup_len, const char* url_data, size_t url_len)
{
    getUncommittedData()->setHtml(QString::fromUtf8(markup_data, markup_len));
}

void Clipboard::WriteRTF(const char* rtf_data, size_t data_len)
{
    getUncommittedData()->setData(QString::fromLatin1(kMimeTypeRTF), QByteArray(rtf_data, data_len));
}

void Clipboard::WriteWebSmartPaste()
{
    getUncommittedData()->setData(QString::fromLatin1(kMimeTypeWebkitSmartPaste), QByteArray());
}

void Clipboard::WriteBitmap(const SkBitmap& bitmap)
{
    QImage image(reinterpret_cast<const uchar *>(bitmap.getPixels()), bitmap.width(), bitmap.height(), QImage::Format_ARGB32);
    getUncommittedData()->setImageData(image.copy());
}

void Clipboard::WriteBookmark(const char* title_data, size_t title_len, const char* url_data, size_t url_len)
{
    // FIXME: Untested, seems to be used only for drag-n-drop.
    // Write as a mozilla url (UTF16: URL, newline, title).
    QString url = QString::fromUtf8(url_data, url_len);
    QString title = QString::fromUtf8(title_data, title_len);

    QByteArray data;
    data.append(reinterpret_cast<const char*>(url.utf16()), url.size() * 2);
    data.append('\n');
    data.append(reinterpret_cast<const char*>(title.utf16()), title.size() * 2);
    getUncommittedData()->setData(QString::fromLatin1(kMimeTypeMozillaURL), data);
}

void Clipboard::WriteData(const FormatType& format, const char* data_data, size_t data_len)
{
    getUncommittedData()->setData(QString::fromStdString(format.data_), QByteArray(data_data, data_len));
}

bool Clipboard::IsFormatAvailable(const Clipboard::FormatType& format, ClipboardType type) const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(type == CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    return mimeData->hasFormat(QString::fromStdString(format.data_));
}

void Clipboard::Clear(ClipboardType type)
{
    QGuiApplication::clipboard()->clear(type == CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
}

void Clipboard::ReadAvailableTypes(ui::ClipboardType type, std::vector<base::string16>* types, bool* contains_filenames) const
{
    if (!types || !contains_filenames) {
        NOTREACHED();
        return;
    }

    types->clear();
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(type == CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    Q_FOREACH (const QString &mimeType, mimeData->formats())
        types->push_back(toString16(mimeType));
    *contains_filenames = false;

    const QByteArray customData = mimeData->data(QString::fromLatin1(kMimeTypeWebCustomDataCopy));
    ReadCustomDataTypes(customData.constData(), customData.size(), types);
}


void Clipboard::ReadText(ClipboardType type, base::string16* result) const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(type == CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    *result = toString16(mimeData->text());
}

void Clipboard::ReadAsciiText(ClipboardType type, std::string* result) const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(type == CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    *result = mimeData->text().toStdString();
}

void Clipboard::ReadHTML(ClipboardType type, base::string16* markup, std::string* src_url, uint32* fragment_start, uint32* fragment_end) const
{
    markup->clear();
    if (src_url)
        src_url->clear();
    *fragment_start = 0;
    *fragment_end = 0;

    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(type == CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    *markup = toString16(mimeData->html());
    *fragment_end = static_cast<uint32>(markup->length());
}

void Clipboard::ReadRTF(ClipboardType type, std::string* result) const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(type == CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    const QByteArray byteArray = mimeData->data(QString::fromLatin1(kMimeTypeRTF));
    *result = std::string(byteArray.constData(), byteArray.length());
}

SkBitmap Clipboard::ReadImage(ClipboardType type) const
{
    // FIXME: Untested, pasting image data seems to only be supported through
    // FileReader.readAsDataURL in JavaScript and this isn't working down the pipe for some reason.
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(type == CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    QImage image = qvariant_cast<QImage>(mimeData->imageData());

    Q_ASSERT(image.format() == QImage::Format_ARGB32);
    SkBitmap bitmap;
    bitmap.setInfo(SkImageInfo::MakeN32(image.width(), image.height(), kOpaque_SkAlphaType));
    bitmap.setPixels(const_cast<uchar*>(image.constBits()));

    // Return a deep copy of the pixel data.
    SkBitmap copy;
    bitmap.copyTo(&copy, kN32_SkColorType);
    return copy;
}

void Clipboard::ReadCustomData(ClipboardType clipboard_type, const base::string16& type, base::string16* result) const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData(clipboard_type == CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
    const QByteArray customData = mimeData->data(QString::fromLatin1(kMimeTypeWebCustomDataCopy));
    ReadCustomDataForType(customData.constData(), customData.size(), type, result);
}

void Clipboard::ReadBookmark(base::string16* title, std::string* url) const
{
    NOTIMPLEMENTED();
}

void Clipboard::ReadData(const FormatType& format, std::string* result) const
{
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    const QByteArray byteArray = mimeData->data(QString::fromStdString(format.data_));
    *result = std::string(byteArray.constData(), byteArray.length());
}

uint64 Clipboard::GetSequenceNumber(ClipboardType type)
{
    return clipboardChangeObserver()->getSequenceNumber(type == CLIPBOARD_TYPE_COPY_PASTE ? QClipboard::Clipboard : QClipboard::Selection);
}

Clipboard::FormatType Clipboard::GetFormatType(const std::string& format_string)
{
    return FormatType::Deserialize(format_string);
}

const Clipboard::FormatType& Clipboard::GetPlainTextFormatType()
{
    CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeText));
    return type;
}

const Clipboard::FormatType& Clipboard::GetPlainTextWFormatType()
{
    return GetPlainTextFormatType();
}

const Clipboard::FormatType& Clipboard::GetUrlFormatType()
{
    return GetPlainTextFormatType();
}

const Clipboard::FormatType& Clipboard::GetUrlWFormatType()
{
    return GetPlainTextWFormatType();
}

const Clipboard::FormatType& Clipboard::GetHtmlFormatType()
{
    CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeHTML));
    return type;
}

const Clipboard::FormatType& Clipboard::GetRtfFormatType()
{
    CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeRTF));
    return type;
}

const Clipboard::FormatType& Clipboard::GetBitmapFormatType()
{
    CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeBitmap));
    return type;
}

const Clipboard::FormatType& Clipboard::GetWebKitSmartPasteFormatType()
{
    CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebkitSmartPaste));
    return type;
}

const Clipboard::FormatType& Clipboard::GetWebCustomDataFormatType()
{
    CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebCustomDataCopy));
    return type;
}

const Clipboard::FormatType& Clipboard::GetPepperCustomDataFormatType()
{
    CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypePepperCustomData));
    return type;
}

#if defined(OS_WIN) || defined(USE_AURA)
bool Clipboard::FormatType::operator<(const FormatType& other) const
{
    return data_.compare(other.data_) < 0;
}
#endif

}  // namespace ui
