// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FGAS_CRT_FGAS_UTILS_H_
#define XFA_FGAS_CRT_FGAS_UTILS_H_

#include "core/fxcrt/fx_coordinates.h"
#include "xfa/fgas/crt/fgas_memory.h"

class FX_BASEARRAYDATA;

class CFX_BaseArray : public CFX_Target {
 protected:
  CFX_BaseArray(int32_t iGrowSize, int32_t iBlockSize);
  ~CFX_BaseArray() override;

  int32_t GetSize() const;
  int32_t GetBlockSize() const;
  uint8_t* AddSpaceTo(int32_t index);
  uint8_t* GetAt(int32_t index) const;
  uint8_t* GetBuffer() const;
  int32_t Append(const CFX_BaseArray& src, int32_t iStart, int32_t iCount);
  int32_t Copy(const CFX_BaseArray& src, int32_t iStart, int32_t iCount);
  int32_t RemoveLast(int32_t iCount);
  void RemoveAll(bool bLeaveMemory);

  FX_BASEARRAYDATA* m_pData;
};

template <class baseType>
class CFX_BaseArrayTemplate : public CFX_BaseArray {
 public:
  CFX_BaseArrayTemplate(int32_t iGrowSize)
      : CFX_BaseArray(iGrowSize, sizeof(baseType)) {}
  CFX_BaseArrayTemplate(int32_t iGrowSize, int32_t iBlockSize)
      : CFX_BaseArray(iGrowSize, iBlockSize) {}

  int32_t GetSize() const { return CFX_BaseArray::GetSize(); }
  int32_t GetBlockSize() const { return CFX_BaseArray::GetBlockSize(); }
  baseType* AddSpace() {
    return (baseType*)CFX_BaseArray::AddSpaceTo(CFX_BaseArray::GetSize());
  }
  int32_t Add(const baseType& element) {
    int32_t index = CFX_BaseArray::GetSize();
    *(baseType*)CFX_BaseArray::AddSpaceTo(index) = element;
    return index;
  }
  baseType* GetBuffer() const { return (baseType*)CFX_BaseArray::GetBuffer(); }
  baseType& GetAt(int32_t index) const {
    return *(baseType*)CFX_BaseArray::GetAt(index);
  }
  baseType* GetPtrAt(int32_t index) const {
    return (baseType*)CFX_BaseArray::GetAt(index);
  }
  void SetAt(int32_t index, const baseType& element) {
    *(baseType*)CFX_BaseArray::GetAt(index) = element;
  }
  void SetAtGrow(int32_t index, const baseType& element) {
    *(baseType*)CFX_BaseArray::AddSpaceTo(index) = element;
  }
  int32_t Append(const CFX_BaseArrayTemplate& src,
                 int32_t iStart,
                 int32_t iCount) {
    return CFX_BaseArray::Append(src, iStart, iCount);
  }
  int32_t Copy(const CFX_BaseArrayTemplate& src,
               int32_t iStart,
               int32_t iCount) {
    return CFX_BaseArray::Copy(src, iStart, iCount);
  }
  int32_t RemoveLast(int32_t iCount) {
    return CFX_BaseArray::RemoveLast(iCount);
  }
  void RemoveAll(bool bLeaveMemory) { CFX_BaseArray::RemoveAll(bLeaveMemory); }
};

class CFX_BaseMassArrayImp : public CFX_Target {
 public:
  CFX_BaseMassArrayImp(int32_t iChunkSize, int32_t iBlockSize);
  ~CFX_BaseMassArrayImp() override;

  uint8_t* AddSpace() { return AddSpaceTo(m_iBlockCount); }
  uint8_t* AddSpaceTo(int32_t index);
  uint8_t* GetAt(int32_t index) const;
  int32_t Append(const CFX_BaseMassArrayImp& src,
                 int32_t iStart,
                 int32_t iCount);
  int32_t Copy(const CFX_BaseMassArrayImp& src, int32_t iStart, int32_t iCount);
  int32_t RemoveLast(int32_t iCount);
  void RemoveAll(bool bLeaveMemory);

  int32_t m_iChunkSize;
  int32_t m_iBlockSize;
  int32_t m_iChunkCount;
  int32_t m_iBlockCount;
  CFX_ArrayTemplate<void*>* m_pData;

 protected:
  void Append(int32_t iDstStart,
              const CFX_BaseMassArrayImp& src,
              int32_t iSrcStart,
              int32_t iSrcCount);
};

class CFX_BaseMassArray : public CFX_Target {
 protected:
  CFX_BaseMassArray(int32_t iChunkSize, int32_t iBlockSize);
  ~CFX_BaseMassArray() override;

  int32_t GetSize() const;
  uint8_t* AddSpaceTo(int32_t index);
  uint8_t* GetAt(int32_t index) const;
  int32_t Append(const CFX_BaseMassArray& src, int32_t iStart, int32_t iCount);
  int32_t Copy(const CFX_BaseMassArray& src, int32_t iStart, int32_t iCount);
  int32_t RemoveLast(int32_t iCount);
  void RemoveAll(bool bLeaveMemory);
  CFX_BaseMassArrayImp* m_pData;
};

template <class baseType>
class CFX_MassArrayTemplate : public CFX_BaseMassArray {
 public:
  CFX_MassArrayTemplate(int32_t iChunkSize)
      : CFX_BaseMassArray(iChunkSize, sizeof(baseType)) {}
  CFX_MassArrayTemplate(int32_t iChunkSize, int32_t iBlockSize)
      : CFX_BaseMassArray(iChunkSize, iBlockSize) {}

  int32_t GetSize() const { return CFX_BaseMassArray::GetSize(); }
  baseType* AddSpace() {
    return (baseType*)CFX_BaseMassArray::AddSpaceTo(
        CFX_BaseMassArray::GetSize());
  }
  int32_t Add(const baseType& element) {
    int32_t index = CFX_BaseMassArray::GetSize();
    *(baseType*)CFX_BaseMassArray::AddSpaceTo(index) = element;
    return index;
  }
  baseType& GetAt(int32_t index) const {
    return *(baseType*)CFX_BaseMassArray::GetAt(index);
  }
  baseType* GetPtrAt(int32_t index) const {
    return (baseType*)CFX_BaseMassArray::GetAt(index);
  }
  void SetAt(int32_t index, const baseType& element) {
    *(baseType*)CFX_BaseMassArray::GetAt(index) = element;
  }
  void SetAtGrow(int32_t index, const baseType& element) {
    *(baseType*)CFX_BaseMassArray::AddSpaceTo(index) = element;
  }
  int32_t Append(const CFX_MassArrayTemplate& src,
                 int32_t iStart,
                 int32_t iCount) {
    return CFX_BaseMassArray::Append(src, iStart, iCount);
  }
  int32_t Copy(const CFX_MassArrayTemplate& src,
               int32_t iStart,
               int32_t iCount) {
    return CFX_BaseMassArray::Copy(src, iStart, iCount);
  }
  int32_t RemoveLast(int32_t iCount) {
    return CFX_BaseMassArray::RemoveLast(iCount);
  }
  void RemoveAll(bool bLeaveMemory) {
    CFX_BaseMassArray::RemoveAll(bLeaveMemory);
  }
};

template <class baseType>
class CFX_ObjectMassArrayTemplate : public CFX_BaseMassArray {
 public:
  CFX_ObjectMassArrayTemplate(int32_t iChunkSize)
      : CFX_BaseMassArray(iChunkSize, sizeof(baseType)) {}
  ~CFX_ObjectMassArrayTemplate() { RemoveAll(false); }

  int32_t GetSize() const { return CFX_BaseMassArray::GetSize(); }
  int32_t Add(const baseType& element) {
    int32_t index = CFX_BaseMassArray::GetSize();
    baseType* p = (baseType*)CFX_BaseMassArray::AddSpaceTo(index);
    new ((void*)p) baseType(element);
    return index;
  }
  baseType& GetAt(int32_t index) const {
    return *(baseType*)CFX_BaseMassArray::GetAt(index);
  }
  baseType* GetPtrAt(int32_t index) const {
    return (baseType*)CFX_BaseMassArray::GetAt(index);
  }
  int32_t Append(const CFX_ObjectMassArrayTemplate& src,
                 int32_t iStart,
                 int32_t iCount) {
    if (iCount == 0) {
      return CFX_BaseMassArray::GetSize();
    }
    int32_t iSize = src.GetSize();
    ASSERT(iStart > -1 && iStart < iSize);
    if (iCount < 0) {
      iCount = iSize;
    }
    int32_t iEnd = iStart + iCount;
    if (iEnd > iSize) {
      iEnd = iSize;
    }
    for (int32_t i = iStart; i < iEnd; i++) {
      Add(src.GetAt(i));
    }
    return CFX_BaseMassArray::GetSize();
  }
  int32_t Copy(const CFX_ObjectMassArrayTemplate& src,
               int32_t iStart,
               int32_t iCount) {
    if (iCount == 0) {
      return CFX_BaseMassArray::GetSize();
    }
    int32_t iSize = src.GetSize();
    ASSERT(iStart > -1 && iStart < iSize);
    if (iCount < 0) {
      iCount = iSize;
    }
    int32_t iEnd = iStart + iCount;
    if (iEnd > iSize) {
      iEnd = iSize;
    }
    RemoveAll(true);
    for (int32_t i = iStart; i < iEnd; i++) {
      Add(src.GetAt(i));
    }
    return CFX_BaseMassArray::GetSize();
  }
  int32_t RemoveLast(int32_t iCount) {
    int32_t iSize = CFX_BaseMassArray::GetSize();
    if (iCount < 0 || iCount > iSize) {
      iCount = iSize;
    }
    if (iCount == 0) {
      return iSize;
    }
    for (int32_t i = iSize - iCount; i < iSize; i++) {
      ((baseType*)GetPtrAt(i))->~baseType();
    }
    return CFX_BaseMassArray::RemoveLast(iCount);
  }
  void RemoveAll(bool bLeaveMemory) {
    int32_t iSize = CFX_BaseMassArray::GetSize();
    for (int32_t i = 0; i < iSize; i++) {
      ((baseType*)GetPtrAt(i))->~baseType();
    }
    CFX_BaseMassArray::RemoveAll(bLeaveMemory);
  }
};

class CFX_BaseDiscreteArray : public CFX_Target {
 protected:
  CFX_BaseDiscreteArray(int32_t iChunkSize, int32_t iBlockSize);
  ~CFX_BaseDiscreteArray() override;

  uint8_t* AddSpaceTo(int32_t index);
  uint8_t* GetAt(int32_t index) const;
  void RemoveAll();
  void* m_pData;
};

template <class baseType>
class CFX_DiscreteArrayTemplate : public CFX_BaseDiscreteArray {
 public:
  CFX_DiscreteArrayTemplate(int32_t iChunkSize)
      : CFX_BaseDiscreteArray(iChunkSize, sizeof(baseType)) {}

  baseType& GetAt(int32_t index, const baseType& defValue) const {
    baseType* p = (baseType*)CFX_BaseDiscreteArray::GetAt(index);
    return p ? *p : (baseType&)defValue;
  }
  baseType* GetPtrAt(int32_t index) const {
    return (baseType*)CFX_BaseDiscreteArray::GetAt(index);
  }
  void SetAtGrow(int32_t index, const baseType& element) {
    *(baseType*)CFX_BaseDiscreteArray::AddSpaceTo(index) = element;
  }
  void RemoveAll() { CFX_BaseDiscreteArray::RemoveAll(); }
};

class CFX_BaseStack : public CFX_Target {
 protected:
  CFX_BaseStack(int32_t iChunkSize, int32_t iBlockSize);
  ~CFX_BaseStack() override;

  uint8_t* Push();
  void Pop();
  uint8_t* GetTopElement() const;
  int32_t GetSize() const;
  uint8_t* GetAt(int32_t index) const;
  void RemoveAll(bool bLeaveMemory);
  CFX_BaseMassArrayImp* m_pData;
};

template <class baseType>
class CFX_StackTemplate : public CFX_BaseStack {
 public:
  CFX_StackTemplate(int32_t iChunkSize)
      : CFX_BaseStack(iChunkSize, sizeof(baseType)) {}

  int32_t Push(const baseType& element) {
    int32_t index = CFX_BaseStack::GetSize();
    *(baseType*)CFX_BaseStack::Push() = element;
    return index;
  }
  void Pop() { CFX_BaseStack::Pop(); }
  baseType* GetTopElement() const {
    return (baseType*)CFX_BaseStack::GetTopElement();
  }
  int32_t GetSize() const { return CFX_BaseStack::GetSize(); }
  baseType* GetAt(int32_t index) const {
    return (baseType*)CFX_BaseStack::GetAt(index);
  }
  void RemoveAll(bool bLeaveMemory) { CFX_BaseStack::RemoveAll(bLeaveMemory); }
};

template <class baseType>
class CFX_ObjectStackTemplate : public CFX_BaseStack {
 public:
  CFX_ObjectStackTemplate(int32_t iChunkSize)
      : CFX_BaseStack(iChunkSize, sizeof(baseType)) {}
  ~CFX_ObjectStackTemplate() { RemoveAll(false); }

  int32_t Push(const baseType& element) {
    int32_t index = CFX_BaseStack::GetSize();
    baseType* p = (baseType*)CFX_BaseStack::Push();
    new ((void*)p) baseType(element);
    return index;
  }
  void Pop() {
    baseType* p = (baseType*)CFX_BaseStack::GetTopElement();
    if (p) {
      p->~baseType();
    }
    CFX_BaseStack::Pop();
  }
  baseType* GetTopElement() const {
    return (baseType*)CFX_BaseStack::GetTopElement();
  }
  int32_t GetSize() const { return CFX_BaseStack::GetSize(); }
  baseType* GetAt(int32_t index) const {
    return (baseType*)CFX_BaseStack::GetAt(index);
  }
  void RemoveAll(bool bLeaveMemory) {
    int32_t iSize = CFX_BaseStack::GetSize();
    for (int32_t i = 0; i < iSize; i++) {
      ((baseType*)CFX_BaseStack::GetAt(i))->~baseType();
    }
    CFX_BaseStack::RemoveAll(bLeaveMemory);
  }
  int32_t Copy(const CFX_ObjectStackTemplate& src,
               int32_t iStart,
               int32_t iCount) {
    if (iCount == 0) {
      return CFX_BaseStack::GetSize();
    }
    int32_t iSize = src.GetSize();
    ASSERT(iStart > -1 && iStart < iSize);
    if (iCount < 0) {
      iCount = iSize;
    }
    int32_t iEnd = iStart + iCount;
    if (iEnd > iSize) {
      iEnd = iSize;
    }
    RemoveAll(true);
    for (int32_t i = iStart; i < iEnd; i++) {
      Push(*src.GetAt(i));
    }
    return CFX_BaseStack::GetSize();
  }
};

#endif  // XFA_FGAS_CRT_FGAS_UTILS_H_
