/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SVGListPropertyHelper_h
#define SVGListPropertyHelper_h

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/ExceptionCode.h"
#include "core/svg/properties/SVGProperty.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

// This is an implementation of the SVG*List property spec:
// http://www.w3.org/TR/SVG/single-page.html#types-InterfaceSVGLengthList
template<typename Derived, typename ItemProperty>
class SVGListPropertyHelper : public SVGPropertyBase {
public:
    typedef ItemProperty ItemPropertyType;

    SVGListPropertyHelper()
        : SVGPropertyBase(Derived::classType())
    {
    }

    ~SVGListPropertyHelper()
    {
        clear();
    }

    // used from Blink C++ code:

    ItemPropertyType* at(size_t index)
    {
        ASSERT(index < m_values.size());
        ASSERT(m_values.at(index)->ownerList() == this);
        return m_values.at(index).get();
    }

    const ItemPropertyType* at(size_t index) const
    {
        return const_cast<SVGListPropertyHelper<Derived, ItemProperty>*>(this)->at(index);
    }

    class ConstIterator {
    private:
        typedef typename Vector<RefPtr<ItemPropertyType> >::const_iterator WrappedType;

    public:
        ConstIterator(WrappedType it)
            : m_it(it)
        {
        }

        ConstIterator& operator++() { ++m_it; return *this; }

        bool operator==(const ConstIterator& o) const { return m_it == o.m_it; }
        bool operator!=(const ConstIterator& o) const { return m_it != o.m_it; }

        PassRefPtr<ItemPropertyType> operator*() { return *m_it; }
        PassRefPtr<ItemPropertyType> operator->() { return *m_it; }

    private:
        WrappedType m_it;
    };

    ConstIterator begin() const
    {
        return ConstIterator(m_values.begin());
    }

    ConstIterator lastAppended() const
    {
        return ConstIterator(m_values.begin() + m_values.size() - 1);
    }

    ConstIterator end() const
    {
        return ConstIterator(m_values.end());
    }

    void append(PassRefPtr<ItemPropertyType> passNewItem)
    {
        RefPtr<ItemPropertyType> newItem = passNewItem;

        ASSERT(newItem);
        m_values.append(newItem);
        newItem->setOwnerList(this);
    }

    bool operator==(const Derived&) const;
    bool operator!=(const Derived& other) const
    {
        return !(*this == other);
    }

    bool isEmpty() const
    {
        return !length();
    }

    // SVGList*Property DOM spec:

    size_t length() const
    {
        return m_values.size();
    }

    void clear();

    PassRefPtr<ItemPropertyType> initialize(PassRefPtr<ItemPropertyType>);
    PassRefPtr<ItemPropertyType> getItem(size_t, ExceptionState&);
    PassRefPtr<ItemPropertyType> insertItemBefore(PassRefPtr<ItemPropertyType>, size_t);
    PassRefPtr<ItemPropertyType> removeItem(size_t, ExceptionState&);
    PassRefPtr<ItemPropertyType> appendItem(PassRefPtr<ItemPropertyType>);
    PassRefPtr<ItemPropertyType> replaceItem(PassRefPtr<ItemPropertyType>, size_t, ExceptionState&);

protected:
    void deepCopy(PassRefPtr<Derived>);

private:
    inline bool checkIndexBound(size_t, ExceptionState&);
    bool removeFromOldOwnerListAndAdjustIndex(PassRefPtr<ItemPropertyType>, size_t* indexToModify);
    size_t findItem(PassRefPtr<ItemPropertyType>);

    Vector<RefPtr<ItemPropertyType> > m_values;

    static PassRefPtr<Derived> toDerived(PassRefPtr<SVGPropertyBase> passBase)
    {
        if (!passBase)
            return nullptr;

        RefPtr<SVGPropertyBase> base = passBase;
        ASSERT(base->type() == Derived::classType());
        return static_pointer_cast<Derived>(base);
    }
};

template<typename Derived, typename ItemProperty>
bool SVGListPropertyHelper<Derived, ItemProperty>::operator==(const Derived& other) const
{
    if (length() != other.length())
        return false;

    size_t size = length();
    for (size_t i = 0; i < size; ++i) {
        if (*at(i) != *other.at(i))
            return false;
    }

    return true;
}

template<typename Derived, typename ItemProperty>
void SVGListPropertyHelper<Derived, ItemProperty>::clear()
{
    // detach all list items as they are no longer part of this list
    typename Vector<RefPtr<ItemPropertyType> >::const_iterator it = m_values.begin();
    typename Vector<RefPtr<ItemPropertyType> >::const_iterator itEnd = m_values.end();
    for (; it != itEnd; ++it) {
        ASSERT((*it)->ownerList() == this);
        (*it)->setOwnerList(0);
    }

    m_values.clear();
}

template<typename Derived, typename ItemProperty>
PassRefPtr<ItemProperty> SVGListPropertyHelper<Derived, ItemProperty>::initialize(PassRefPtr<ItemProperty> passNewItem)
{
    RefPtr<ItemPropertyType> newItem = passNewItem;

    // Spec: If the inserted item is already in a list, it is removed from its previous list before it is inserted into this list.
    removeFromOldOwnerListAndAdjustIndex(newItem, 0);

    // Spec: Clears all existing current items from the list and re-initializes the list to hold the single item specified by the parameter.
    clear();
    append(newItem);
    return newItem.release();
}

template<typename Derived, typename ItemProperty>
PassRefPtr<ItemProperty> SVGListPropertyHelper<Derived, ItemProperty>::getItem(size_t index, ExceptionState& exceptionState)
{
    if (!checkIndexBound(index, exceptionState))
        return nullptr;

    ASSERT(index < m_values.size());
    ASSERT(m_values.at(index)->ownerList() == this);
    return m_values.at(index);
}

template<typename Derived, typename ItemProperty>
PassRefPtr<ItemProperty> SVGListPropertyHelper<Derived, ItemProperty>::insertItemBefore(PassRefPtr<ItemProperty> passNewItem, size_t index)
{
    // Spec: If the index is greater than or equal to length, then the new item is appended to the end of the list.
    if (index > m_values.size())
        index = m_values.size();

    RefPtr<ItemPropertyType> newItem = passNewItem;

    // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
    if (!removeFromOldOwnerListAndAdjustIndex(newItem, &index)) {
        // Inserting the item before itself is a no-op.
        return newItem.release();
    }

    // Spec: Inserts a new item into the list at the specified position. The index of the item before which the new item is to be
    // inserted. The first item is number 0. If the index is equal to 0, then the new item is inserted at the front of the list.
    m_values.insert(index, newItem);
    newItem->setOwnerList(this);

    return newItem.release();
}

template<typename Derived, typename ItemProperty>
PassRefPtr<ItemProperty> SVGListPropertyHelper<Derived, ItemProperty>::removeItem(size_t index, ExceptionState& exceptionState)
{
    if (index >= m_values.size()) {
        exceptionState.throwDOMException(IndexSizeError, ExceptionMessages::indexExceedsMaximumBound("index", index, m_values.size()));
        return nullptr;
    }
    ASSERT(m_values.at(index)->ownerList() == this);
    RefPtr<ItemPropertyType> oldItem = m_values.at(index);
    m_values.remove(index);
    oldItem->setOwnerList(0);
    return oldItem.release();
}

template<typename Derived, typename ItemProperty>
PassRefPtr<ItemProperty> SVGListPropertyHelper<Derived, ItemProperty>::appendItem(PassRefPtr<ItemProperty> passNewItem)
{
    RefPtr<ItemPropertyType> newItem = passNewItem;

    // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
    removeFromOldOwnerListAndAdjustIndex(newItem, 0);

    // Append the value and wrapper at the end of the list.
    append(newItem);

    return newItem.release();
}

template<typename Derived, typename ItemProperty>
PassRefPtr<ItemProperty> SVGListPropertyHelper<Derived, ItemProperty>::replaceItem(PassRefPtr<ItemProperty> passNewItem, size_t index, ExceptionState& exceptionState)
{
    if (!checkIndexBound(index, exceptionState))
        return nullptr;

    RefPtr<ItemPropertyType> newItem = passNewItem;

    // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
    // Spec: If the item is already in this list, note that the index of the item to replace is before the removal of the item.
    if (!removeFromOldOwnerListAndAdjustIndex(newItem, &index)) {
        // Replacing the item with itself is a no-op.
        return newItem.release();
    }

    if (m_values.isEmpty()) {
        // 'newItem' already lived in our list, we removed it, and now we're empty, which means there's nothing to replace.
        exceptionState.throwDOMException(IndexSizeError, String::format("Failed to replace the provided item at index %zu.", index));
        return nullptr;
    }

    // Update the value at the desired position 'index'.
    RefPtr<ItemPropertyType>& position = m_values[index];
    ASSERT(position->ownerList() == this);
    position->setOwnerList(0);
    position = newItem;
    newItem->setOwnerList(this);

    return newItem.release();
}

template<typename Derived, typename ItemProperty>
bool SVGListPropertyHelper<Derived, ItemProperty>::checkIndexBound(size_t index, ExceptionState& exceptionState)
{
    if (index >= m_values.size()) {
        exceptionState.throwDOMException(IndexSizeError, ExceptionMessages::indexExceedsMaximumBound("index", index, m_values.size()));
        return false;
    }

    return true;
}

template<typename Derived, typename ItemProperty>
bool SVGListPropertyHelper<Derived, ItemProperty>::removeFromOldOwnerListAndAdjustIndex(PassRefPtr<ItemPropertyType> passItem, size_t* indexToModify)
{
    RefPtr<ItemPropertyType> item = passItem;
    ASSERT(item);
    RefPtr<Derived> ownerList = toDerived(item->ownerList());
    if (!ownerList)
        return true;

    // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
    // 'newItem' is already living in another list. If it's not our list, synchronize the other lists wrappers after the removal.
    bool livesInOtherList = ownerList.get() != this;
    size_t indexToRemove = ownerList->findItem(item);
    ASSERT(indexToRemove != WTF::kNotFound);

    // Do not remove newItem if already in this list at the target index.
    if (!livesInOtherList && indexToModify && indexToRemove == *indexToModify)
        return false;

    ownerList->removeItem(indexToRemove, ASSERT_NO_EXCEPTION);

    if (!indexToModify)
        return true;

    // If the item lived in our list, adjust the insertion index.
    if (!livesInOtherList) {
        size_t& index = *indexToModify;
        // Spec: If the item is already in this list, note that the index of the item to (replace|insert before) is before the removal of the item.
        if (static_cast<size_t>(indexToRemove) < index)
            --index;
    }

    return true;
}

template<typename Derived, typename ItemProperty>
size_t SVGListPropertyHelper<Derived, ItemProperty>::findItem(PassRefPtr<ItemPropertyType> item)
{
    return m_values.find(item);
}

template<typename Derived, typename ItemProperty>
void SVGListPropertyHelper<Derived, ItemProperty>::deepCopy(PassRefPtr<Derived> passFrom)
{
    RefPtr<Derived> from = passFrom;

    clear();
    typename Vector<RefPtr<ItemPropertyType> >::const_iterator it = from->m_values.begin();
    typename Vector<RefPtr<ItemPropertyType> >::const_iterator itEnd = from->m_values.end();
    for (; it != itEnd; ++it) {
        append((*it)->clone());
    }
}

}

#endif // SVGListPropertyHelper_h
