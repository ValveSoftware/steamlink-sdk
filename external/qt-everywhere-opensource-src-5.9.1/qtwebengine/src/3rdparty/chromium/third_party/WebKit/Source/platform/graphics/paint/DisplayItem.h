// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItem_h
#define DisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/graphics/ContiguousContainer.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/Noncopyable.h"

#ifndef NDEBUG
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"
#endif

class SkPictureGpuAnalyzer;

namespace blink {

class GraphicsContext;
class IntRect;
class WebDisplayItemList;

class PLATFORM_EXPORT DisplayItem {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  enum {
    // Must be kept in sync with core/paint/PaintPhase.h.
    kPaintPhaseMax = 11,
  };

  // A display item type uniquely identifies a display item of a client.
  // Some display item types can be categorized using the following directives:
  // - In enum Type:
  //   - enum value <Category>First;
  //   - enum values of the category, first of which should equal
  //     <Category>First (for ease of maintenance, the values should be in
  //     alphabetic order);
  //   - enum value <Category>Last which should be equal to the last of the enum
  //     values of the category
  // - DEFINE_CATEGORY_METHODS(<Category>) to define is<Category>Type(Type) and
  //   is<Category>() methods.
  //
  // A category or subset of a category can contain types each of which
  // corresponds to a PaintPhase:
  // - In enum Type:
  //   - enum value <Category>[<Subset>]PaintPhaseFirst;
  //   - enum value <Category>[<Subset>]PaintPhaseLast =
  //     <Category>[<Subset>]PaintPhaseFirst + PaintPhaseMax;
  // - DEFINE_PAINT_PHASE_CONVERSION_METHOD(<Category>[<Subset>]) to define
  //   paintPhaseTo<Category>[<Subset>]Type(PaintPhase) method.
  //
  // A category can be derived from another category, containing types each of
  // which corresponds to a value of the latter category:
  // - In enum Type:
  //   - enum value <Category>First;
  //   - enum value <Category>Last =
  //     <Category>First + <BaseCategory>Last - <BaseCategory>First;
  // - DEFINE_CONVERSION_METHODS(<Category>,
  //                             <category>,
  //                             <BaseCategory>,
  //                             <baseCategory>)
  //   to define methods to convert types between the categories.
  enum Type {
    kDrawingFirst,
    kDrawingPaintPhaseFirst = kDrawingFirst,
    kDrawingPaintPhaseLast = kDrawingFirst + kPaintPhaseMax,
    kBoxDecorationBackground,
    kCaret,
    kColumnRules,
    kDebugDrawing,
    kDocumentBackground,
    kDragImage,
    kDragCaret,
    kSVGImage,
    kLinkHighlight,
    kImageAreaFocusRing,
    kPageOverlay,
    kPageWidgetDelegateBackgroundFallback,
    kPopupContainerBorder,
    kPopupListBoxBackground,
    kPopupListBoxRow,
    kPrintedContentBackground,
    kPrintedContentDestinationLocations,
    kPrintedContentLineBoundary,
    kPrintedContentPDFURLRect,
    kResizer,
    kSVGClip,
    kSVGFilter,
    kSVGMask,
    kScrollbarBackButtonEnd,
    kScrollbarBackButtonStart,
    kScrollbarBackground,
    kScrollbarBackTrack,
    kScrollbarCorner,
    kScrollbarForwardButtonEnd,
    kScrollbarForwardButtonStart,
    kScrollbarForwardTrack,
    kScrollbarThumb,
    kScrollbarTickmarks,
    kScrollbarTrackBackground,
    kScrollbarCompositedScrollbar,
    kSelectionTint,
    kTableCellBackgroundFromColumnGroup,
    kTableCellBackgroundFromColumn,
    kTableCellBackgroundFromSection,
    kTableCellBackgroundFromRow,
    // Table collapsed borders can be painted together (e.g., left & top) but
    // there are at most 4 phases of collapsed border painting for a single
    // cell. To disambiguate these phases of collapsed border painting, a mask
    // is used. TableCollapsedBorderBase can be larger than
    // TableCollapsedBorderUnalignedBase to ensure the base lower bits are 0's.
    kTableCollapsedBorderUnalignedBase,
    kTableCollapsedBorderBase =
        (((kTableCollapsedBorderUnalignedBase - 1) >> 4) + 1) << 4,
    kTableCollapsedBorderLast = kTableCollapsedBorderBase + 0x0f,
    kTableSectionBoxShadowInset,
    kTableSectionBoxShadowNormal,
    kTableRowBoxShadowInset,
    kTableRowBoxShadowNormal,
    kVideoBitmap,
    kWebPlugin,
    kWebFont,
    kReflectionMask,
    kDrawingLast = kReflectionMask,

    kForeignLayerFirst,
    kForeignLayerCanvas = kForeignLayerFirst,
    kForeignLayerPlugin,
    kForeignLayerVideo,
    kForeignLayerLast = kForeignLayerVideo,

    kClipFirst,
    kClipBoxPaintPhaseFirst = kClipFirst,
    kClipBoxPaintPhaseLast = kClipBoxPaintPhaseFirst + kPaintPhaseMax,
    kClipColumnBoundsPaintPhaseFirst,
    kClipColumnBoundsPaintPhaseLast =
        kClipColumnBoundsPaintPhaseFirst + kPaintPhaseMax,
    kClipLayerFragmentPaintPhaseFirst,
    kClipLayerFragmentPaintPhaseLast =
        kClipLayerFragmentPaintPhaseFirst + kPaintPhaseMax,
    kClipFileUploadControlRect,
    kClipFrameToVisibleContentRect,
    kClipFrameScrollbars,
    kClipLayerBackground,
    kClipLayerColumnBounds,
    kClipLayerFilter,
    kClipLayerForeground,
    kClipLayerParent,
    kClipLayerOverflowControls,
    kClipNodeImage,
    kClipPopupListBoxFrame,
    kClipScrollbarsToBoxBounds,
    kClipSelectionImage,
    kPageWidgetDelegateClip,
    kClipPrintedPage,
    kClipLast = kClipPrintedPage,

    kEndClipFirst,
    kEndClipLast = kEndClipFirst + kClipLast - kClipFirst,

    kFloatClipFirst,
    kFloatClipPaintPhaseFirst = kFloatClipFirst,
    kFloatClipPaintPhaseLast = kFloatClipFirst + kPaintPhaseMax,
    kFloatClipLast = kFloatClipPaintPhaseLast,
    kEndFloatClipFirst,
    kEndFloatClipLast = kEndFloatClipFirst + kFloatClipLast - kFloatClipFirst,

    kScrollFirst,
    kScrollPaintPhaseFirst = kScrollFirst,
    kScrollPaintPhaseLast = kScrollPaintPhaseFirst + kPaintPhaseMax,
    kScrollOverflowControls,
    kScrollLast = kScrollOverflowControls,
    kEndScrollFirst,
    kEndScrollLast = kEndScrollFirst + kScrollLast - kScrollFirst,

    kTransform3DFirst,
    kTransform3DElementTransform = kTransform3DFirst,
    kTransform3DLast = kTransform3DElementTransform,
    kEndTransform3DFirst,
    kEndTransform3DLast =
        kEndTransform3DFirst + kTransform3DLast - kTransform3DFirst,

    kBeginFilter,
    kEndFilter,
    kBeginCompositing,
    kEndCompositing,
    kBeginTransform,
    kEndTransform,
    kBeginClipPath,
    kEndClipPath,

    kSubsequence,
    kEndSubsequence,

    kUninitializedType,
    kTypeLast = kUninitializedType
  };

  static_assert(kTableCollapsedBorderBase >= kTableCollapsedBorderUnalignedBase,
                "TableCollapsedBorder types overlap with other types");
  static_assert((kTableCollapsedBorderBase & 0xf) == 0,
                "The lowest 4 bits of TableCollapsedBorderBase should be zero");
  // Bits or'ed onto TableCollapsedBorderBase to generate a real table collapsed
  // border type.
  enum TableCollapsedBorderSides {
    TableCollapsedBorderTop = 1 << 0,
    TableCollapsedBorderRight = 1 << 1,
    TableCollapsedBorderBottom = 1 << 2,
    TableCollapsedBorderLeft = 1 << 3,
  };

  DisplayItem(const DisplayItemClient& client, Type type, size_t derivedSize)
      : m_client(&client),
        m_type(type),
        m_derivedSize(derivedSize),
        m_skippedCache(false)
#ifndef NDEBUG
        ,
        m_clientDebugString(client.debugName())
#endif
  {
    // derivedSize must fit in m_derivedSize.
    // If it doesn't, enlarge m_derivedSize and fix this assert.
    SECURITY_DCHECK(derivedSize < (1 << 8));
    SECURITY_DCHECK(derivedSize >= sizeof(*this));
  }

  virtual ~DisplayItem() {}

  // Ids are for matching new DisplayItems with existing DisplayItems.
  struct Id {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    Id(const DisplayItemClient& client, const Type type)
        : client(client), type(type) {}

    const DisplayItemClient& client;
    const Type type;
  };

  Id getId() const { return Id(*m_client, m_type); }

  virtual void replay(GraphicsContext&) const {}

  const DisplayItemClient& client() const {
    ASSERT(m_client);
    return *m_client;
  }
  Type getType() const { return m_type; }

  // Size of this object in memory, used to move it with memcpy.
  // This is not sizeof(*this), because it needs to account for the size of
  // the derived class (i.e. runtime type). Derived classes are expected to
  // supply this to the DisplayItem constructor.
  size_t derivedSize() const { return m_derivedSize; }

  // For PaintController only. Painters should use DisplayItemCacheSkipper
  // instead.
  void setSkippedCache() { m_skippedCache = true; }
  bool skippedCache() const { return m_skippedCache; }

  // TODO(wkorman): Only DrawingDisplayItem needs the visual rect argument.
  // Consider refactoring class hierarchy to make this more explicit.
  virtual void appendToWebDisplayItemList(const IntRect&,
                                          WebDisplayItemList*) const {}

// See comments of enum Type for usage of the following macros.
#define DEFINE_CATEGORY_METHODS(Category)                           \
  static bool is##Category##Type(Type type) {                       \
    return type >= k##Category##First && type <= k##Category##Last; \
  }                                                                 \
  bool is##Category() const { return is##Category##Type(getType()); }

#define DEFINE_CONVERSION_METHODS(Category1, category1, Category2, category2) \
  static Type category1##TypeTo##Category2##Type(Type type) {                 \
    static_assert(k##Category1##Last - k##Category1##First ==                 \
                      k##Category2##Last - k##Category2##First,               \
                  "Categories " #Category1 " and " #Category2                 \
                  " should have same number of enum values. See comments of " \
                  "DisplayItem::Type");                                       \
    ASSERT(is##Category1##Type(type));                                        \
    return static_cast<Type>(type - k##Category1##First +                     \
                             k##Category2##First);                            \
  }                                                                           \
  static Type category2##TypeTo##Category1##Type(Type type) {                 \
    ASSERT(is##Category2##Type(type));                                        \
    return static_cast<Type>(type - k##Category2##First +                     \
                             k##Category1##First);                            \
  }

#define DEFINE_PAIRED_CATEGORY_METHODS(Category, category) \
  DEFINE_CATEGORY_METHODS(Category)                        \
  DEFINE_CATEGORY_METHODS(End##Category)                   \
  DEFINE_CONVERSION_METHODS(Category, category, End##Category, end##Category)

#define DEFINE_PAINT_PHASE_CONVERSION_METHOD(Category)                   \
  static Type paintPhaseTo##Category##Type(int paintPhase) {             \
    static_assert(                                                       \
        k##Category##PaintPhaseLast - k##Category##PaintPhaseFirst ==    \
            k##PaintPhaseMax,                                            \
        "Invalid paint-phase-based category " #Category                  \
        ". See comments of DisplayItem::Type");                          \
    return static_cast<Type>(paintPhase + k##Category##PaintPhaseFirst); \
  }

  DEFINE_CATEGORY_METHODS(Drawing)
  DEFINE_PAINT_PHASE_CONVERSION_METHOD(Drawing)

  DEFINE_CATEGORY_METHODS(ForeignLayer)

  DEFINE_PAIRED_CATEGORY_METHODS(Clip, clip)
  DEFINE_PAINT_PHASE_CONVERSION_METHOD(ClipLayerFragment)
  DEFINE_PAINT_PHASE_CONVERSION_METHOD(ClipBox)
  DEFINE_PAINT_PHASE_CONVERSION_METHOD(ClipColumnBounds)

  DEFINE_PAIRED_CATEGORY_METHODS(FloatClip, floatClip)
  DEFINE_PAINT_PHASE_CONVERSION_METHOD(FloatClip)

  DEFINE_PAIRED_CATEGORY_METHODS(Scroll, scroll)
  DEFINE_PAINT_PHASE_CONVERSION_METHOD(Scroll)

  DEFINE_PAIRED_CATEGORY_METHODS(Transform3D, transform3D)

  static bool isCacheableType(Type type) {
    return isDrawingType(type) || type == kSubsequence;
  }
  bool isCacheable() const {
    return !skippedCache() && isCacheableType(m_type);
  }

  virtual bool isBegin() const { return false; }
  virtual bool isEnd() const { return false; }

#if DCHECK_IS_ON()
  virtual bool isEndAndPairedWith(DisplayItem::Type otherType) const {
    return false;
  }
#endif

  virtual bool equals(const DisplayItem& other) const {
    return m_client == other.m_client && m_type == other.m_type &&
           m_derivedSize == other.m_derivedSize &&
           m_skippedCache == other.m_skippedCache;
  }

  // True if the client is non-null. Because m_client is const, this should
  // never be false except when we explicitly create a tombstone/"dead display
  // item" as part of moving an item from one list to another (see:
  // DisplayItemList::appendByMoving).
  bool hasValidClient() const { return m_client; }

  virtual bool drawsContent() const { return false; }

  // Override to implement specific analysis strategies.
  virtual void analyzeForGpuRasterization(SkPictureGpuAnalyzer&) const {}

#ifndef NDEBUG
  static WTF::String typeAsDebugString(DisplayItem::Type);
  const WTF::String clientDebugString() const { return m_clientDebugString; }
  void setClientDebugString(const WTF::String& s) { m_clientDebugString = s; }
  WTF::String asDebugString() const;
  virtual void dumpPropertiesAsDebugString(WTF::StringBuilder&) const;
#endif

 private:
  // The default DisplayItem constructor is only used by
  // ContiguousContainer::appendByMoving where an invalid DisplaItem is
  // constructed at the source location.
  template <typename T, unsigned alignment>
  friend class ContiguousContainer;

  DisplayItem()
      : m_client(nullptr),
        m_type(kUninitializedType),
        m_derivedSize(sizeof(*this)),
        m_skippedCache(false) {}

  const DisplayItemClient* m_client;
  static_assert(kTypeLast < (1 << 16),
                "DisplayItem::Type should fit in 16 bits");
  const Type m_type : 16;
  const unsigned m_derivedSize : 8;  // size of the actual derived class
  unsigned m_skippedCache : 1;

#ifndef NDEBUG
  WTF::String m_clientDebugString;
#endif
};

inline bool operator==(const DisplayItem::Id& a, const DisplayItem::Id& b) {
  return a.client == b.client && a.type == b.type;
}

inline bool operator!=(const DisplayItem::Id& a, const DisplayItem::Id& b) {
  return !(a == b);
}

class PLATFORM_EXPORT PairedBeginDisplayItem : public DisplayItem {
 protected:
  PairedBeginDisplayItem(const DisplayItemClient& client,
                         Type type,
                         size_t derivedSize)
      : DisplayItem(client, type, derivedSize) {}

 private:
  bool isBegin() const final { return true; }
};

class PLATFORM_EXPORT PairedEndDisplayItem : public DisplayItem {
 protected:
  PairedEndDisplayItem(const DisplayItemClient& client,
                       Type type,
                       size_t derivedSize)
      : DisplayItem(client, type, derivedSize) {}

#if ENABLE(ASSERT)
  bool isEndAndPairedWith(DisplayItem::Type otherType) const override = 0;
#endif

 private:
  bool isEnd() const final { return true; }
};

}  // namespace blink

#endif  // DisplayItem_h
