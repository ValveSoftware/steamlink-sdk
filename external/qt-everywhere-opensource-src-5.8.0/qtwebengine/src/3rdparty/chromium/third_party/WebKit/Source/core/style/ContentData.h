/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ContentData_h
#define ContentData_h

#include "core/style/CounterContent.h"
#include "core/style/StyleImage.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class Document;
class LayoutObject;
class ComputedStyle;

class ContentData : public GarbageCollectedFinalized<ContentData> {
public:
    static ContentData* create(StyleImage*);
    static ContentData* create(const String&);
    static ContentData* create(std::unique_ptr<CounterContent>);
    static ContentData* create(QuoteType);

    virtual ~ContentData() { }

    virtual bool isCounter() const { return false; }
    virtual bool isImage() const { return false; }
    virtual bool isQuote() const { return false; }
    virtual bool isText() const { return false; }

    virtual LayoutObject* createLayoutObject(Document&, ComputedStyle&) const = 0;

    virtual ContentData* clone() const;

    ContentData* next() const { return m_next.get(); }
    void setNext(ContentData* next) { m_next = next; }

    virtual bool equals(const ContentData&) const = 0;

    DECLARE_VIRTUAL_TRACE();

private:
    virtual ContentData* cloneInternal() const = 0;

    Member<ContentData> m_next;
};

#define DEFINE_CONTENT_DATA_TYPE_CASTS(typeName) \
    DEFINE_TYPE_CASTS(typeName##ContentData, ContentData, content, content->is##typeName(), content.is##typeName())

class ImageContentData final : public ContentData {
    friend class ContentData;
public:
    const StyleImage* image() const { return m_image.get(); }
    StyleImage* image() { return m_image.get(); }
    void setImage(StyleImage* image) { ASSERT(image); m_image = image; }

    bool isImage() const override { return true; }
    LayoutObject* createLayoutObject(Document&, ComputedStyle&) const override;

    bool equals(const ContentData& data) const override
    {
        if (!data.isImage())
            return false;
        return *static_cast<const ImageContentData&>(data).image() == *image();
    }

    DECLARE_VIRTUAL_TRACE();

private:
    ImageContentData(StyleImage* image)
        : m_image(image)
    {
        ASSERT(m_image);
    }

    ContentData* cloneInternal() const override
    {
        StyleImage* image = const_cast<StyleImage*>(this->image());
        return create(image);
    }

    Member<StyleImage> m_image;
};

DEFINE_CONTENT_DATA_TYPE_CASTS(Image);

class TextContentData final : public ContentData {
    friend class ContentData;
public:
    const String& text() const { return m_text; }
    void setText(const String& text) { m_text = text; }

    bool isText() const override { return true; }
    LayoutObject* createLayoutObject(Document&, ComputedStyle&) const override;

    bool equals(const ContentData& data) const override
    {
        if (!data.isText())
            return false;
        return static_cast<const TextContentData&>(data).text() == text();
    }

private:
    TextContentData(const String& text)
        : m_text(text)
    {
    }

    ContentData* cloneInternal() const override { return create(text()); }

    String m_text;
};

DEFINE_CONTENT_DATA_TYPE_CASTS(Text);

class CounterContentData final : public ContentData {
    friend class ContentData;
public:
    const CounterContent* counter() const { return m_counter.get(); }
    void setCounter(std::unique_ptr<CounterContent> counter) { m_counter = std::move(counter); }

    bool isCounter() const override { return true; }
    LayoutObject* createLayoutObject(Document&, ComputedStyle&) const override;

private:
    CounterContentData(std::unique_ptr<CounterContent> counter)
        : m_counter(std::move(counter))
    {
    }

    ContentData* cloneInternal() const override
    {
        std::unique_ptr<CounterContent> counterData = wrapUnique(new CounterContent(*counter()));
        return create(std::move(counterData));
    }

    bool equals(const ContentData& data) const override
    {
        if (!data.isCounter())
            return false;
        return *static_cast<const CounterContentData&>(data).counter() == *counter();
    }

    std::unique_ptr<CounterContent> m_counter;
};

DEFINE_CONTENT_DATA_TYPE_CASTS(Counter);

class QuoteContentData final : public ContentData {
    friend class ContentData;
public:
    QuoteType quote() const { return m_quote; }
    void setQuote(QuoteType quote) { m_quote = quote; }

    bool isQuote() const override { return true; }
    LayoutObject* createLayoutObject(Document&, ComputedStyle&) const override;

    bool equals(const ContentData& data) const override
    {
        if (!data.isQuote())
            return false;
        return static_cast<const QuoteContentData&>(data).quote() == quote();
    }

private:
    QuoteContentData(QuoteType quote)
        : m_quote(quote)
    {
    }

    ContentData* cloneInternal() const override { return create(quote()); }

    QuoteType m_quote;
};

DEFINE_CONTENT_DATA_TYPE_CASTS(Quote);

inline bool operator==(const ContentData& a, const ContentData& b)
{
    return a.equals(b);
}

inline bool operator!=(const ContentData& a, const ContentData& b)
{
    return !(a == b);
}

} // namespace blink

#endif // ContentData_h
