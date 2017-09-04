/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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

#ifndef StyleResolver_h
#define StyleResolver_h

#include "core/CoreExport.h"
#include "core/animation/PropertyHandle.h"
#include "core/css/ElementRuleCollector.h"
#include "core/css/PseudoStyleRequest.h"
#include "core/css/RuleFeature.h"
#include "core/css/RuleSet.h"
#include "core/css/SelectorChecker.h"
#include "core/css/SelectorFilter.h"
#include "core/css/resolver/CSSPropertyPriority.h"
#include "core/css/resolver/MatchedPropertiesCache.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/style/CachedUAStyle.h"
#include "platform/heap/Handle.h"
#include "wtf/Deque.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/ListHashSet.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class AnimatableValue;
class CSSRuleList;
class CSSStyleSheet;
class CSSValue;
class Document;
class Element;
class Interpolation;
class MatchResult;
class MediaQueryEvaluator;
class StylePropertySet;
class StyleRule;
class StyleRuleUsageTracker;

enum StyleSharingBehavior {
  AllowStyleSharing,
  DisallowStyleSharing,
};

enum RuleMatchingBehavior { MatchAllRules, MatchAllRulesExcludingSMIL };

const unsigned styleSharingListSize = 15;
const unsigned styleSharingMaxDepth = 32;
using StyleSharingList = HeapDeque<Member<Element>, styleSharingListSize>;
using ActiveInterpolationsMap =
    HashMap<PropertyHandle, Vector<RefPtr<Interpolation>, 1>>;

// This class selects a ComputedStyle for a given element based on a collection
// of stylesheets.
class CORE_EXPORT StyleResolver final
    : public GarbageCollectedFinalized<StyleResolver> {
  WTF_MAKE_NONCOPYABLE(StyleResolver);

 public:
  static StyleResolver* create(Document& document) {
    return new StyleResolver(document);
  }
  ~StyleResolver();
  void dispose();

  PassRefPtr<ComputedStyle> styleForElement(
      Element*,
      const ComputedStyle* parentStyle = 0,
      StyleSharingBehavior = AllowStyleSharing,
      RuleMatchingBehavior = MatchAllRules);

  static PassRefPtr<AnimatableValue> createAnimatableValueSnapshot(
      Element&,
      const ComputedStyle& baseStyle,
      const ComputedStyle* parentStyle,
      CSSPropertyID,
      const CSSValue*);

  PassRefPtr<ComputedStyle> pseudoStyleForElement(
      Element*,
      const PseudoStyleRequest&,
      const ComputedStyle* parentStyle);

  PassRefPtr<ComputedStyle> styleForPage(int pageIndex);
  PassRefPtr<ComputedStyle> styleForText(Text*);

  static PassRefPtr<ComputedStyle> styleForDocument(Document&);

  // FIXME: It could be better to call appendAuthorStyleSheets() directly after
  // we factor StyleResolver further.
  // https://bugs.webkit.org/show_bug.cgi?id=108890
  void appendAuthorStyleSheets(const HeapVector<Member<CSSStyleSheet>>&);
  void lazyAppendAuthorStyleSheets(unsigned firstNew,
                                   const HeapVector<Member<CSSStyleSheet>>&);
  void removePendingAuthorStyleSheets(const HeapVector<Member<CSSStyleSheet>>&);
  void appendPendingAuthorStyleSheets();
  bool hasPendingAuthorStyleSheets() const {
    return m_pendingStyleSheets.size() > 0;
  }

  // TODO(esprehn): StyleResolver should probably not contain tree walking
  // state, instead we should pass a context object during recalcStyle.
  SelectorFilter& selectorFilter() { return m_selectorFilter; }

  StyleRuleKeyframes* findKeyframesRule(const Element*,
                                        const AtomicString& animationName);

  // These methods will give back the set of rules that matched for a given
  // element (or a pseudo-element).
  enum CSSRuleFilter {
    UAAndUserCSSRules = 1 << 1,
    AuthorCSSRules = 1 << 2,
    EmptyCSSRules = 1 << 3,
    CrossOriginCSSRules = 1 << 4,
    AllButEmptyCSSRules =
        UAAndUserCSSRules | AuthorCSSRules | CrossOriginCSSRules,
    AllCSSRules = AllButEmptyCSSRules | EmptyCSSRules,
  };
  CSSRuleList* cssRulesForElement(
      Element*,
      unsigned rulesToInclude = AllButEmptyCSSRules);
  CSSRuleList* pseudoCSSRulesForElement(
      Element*,
      PseudoId,
      unsigned rulesToInclude = AllButEmptyCSSRules);
  StyleRuleList* styleRulesForElement(Element*, unsigned rulesToInclude);

  void computeFont(ComputedStyle*, const StylePropertySet&);

  void addViewportDependentMediaQueries(const MediaQueryResultList&);
  bool hasViewportDependentMediaQueries() const {
    return !m_viewportDependentMediaQueryResults.isEmpty();
  }
  bool mediaQueryAffectedByViewportChange() const;
  void addDeviceDependentMediaQueries(const MediaQueryResultList&);
  bool mediaQueryAffectedByDeviceChange() const;

  // FIXME: Rename to reflect the purpose, like didChangeFontSize or something.
  void invalidateMatchedPropertiesCache();

  void notifyResizeForViewportUnits();

  // Exposed for ComputedStyle::isStyleAvilable().
  static ComputedStyle* styleNotYetAvailable() {
    return s_styleNotYetAvailable;
  }

  StyleSharingList& styleSharingList();

  void addToStyleSharingList(Element&);
  void clearStyleSharingList();

  void increaseStyleSharingDepth() { ++m_styleSharingDepth; }
  void decreaseStyleSharingDepth() { --m_styleSharingDepth; }

  PseudoElement* createPseudoElementIfNeeded(Element& parent, PseudoId);

  DECLARE_TRACE();

  void setRuleUsageTracker(StyleRuleUsageTracker*);

 private:
  explicit StyleResolver(Document&);

  PassRefPtr<ComputedStyle> initialStyleForElement();

  // FIXME: This should probably go away, folded into FontBuilder.
  void updateFont(StyleResolverState&);

  void addMatchedRulesToTracker(const ElementRuleCollector&);

  void loadPendingResources(StyleResolverState&);
  void adjustComputedStyle(StyleResolverState&, Element*);

  void appendCSSStyleSheet(CSSStyleSheet&);

  void collectPseudoRulesForElement(const Element&,
                                    ElementRuleCollector&,
                                    PseudoId,
                                    unsigned rulesToInclude);
  void matchRuleSet(ElementRuleCollector&, RuleSet*);
  void matchUARules(ElementRuleCollector&);
  void matchScopedRules(const Element&, ElementRuleCollector&);
  void matchAuthorRules(const Element&, ElementRuleCollector&);
  void matchAuthorRulesV0(const Element&, ElementRuleCollector&);
  void matchAllRules(StyleResolverState&,
                     ElementRuleCollector&,
                     bool includeSMILProperties);
  void collectTreeBoundaryCrossingRulesV0CascadeOrder(const Element&,
                                                      ElementRuleCollector&);

  void applyMatchedProperties(StyleResolverState&, const MatchResult&);
  bool applyAnimatedProperties(StyleResolverState&,
                               const Element* animatingElement);
  void applyCallbackSelectors(StyleResolverState&);

  // These flags indicate whether an apply pass for a given CSSPropertyPriority
  // and isImportant is required.
  class NeedsApplyPass {
   public:
    bool get(CSSPropertyPriority priority, bool isImportant) const {
      return m_flags[getIndex(priority, isImportant)];
    }
    void set(CSSPropertyPriority priority, bool isImportant) {
      m_flags[getIndex(priority, isImportant)] = true;
    }

   private:
    static size_t getIndex(CSSPropertyPriority priority, bool isImportant) {
      DCHECK(priority >= 0 && priority < PropertyPriorityCount);
      return priority * 2 + isImportant;
    }
    bool m_flags[PropertyPriorityCount * 2] = {0};
  };

  enum ShouldUpdateNeedsApplyPass {
    CheckNeedsApplyPass = false,
    UpdateNeedsApplyPass = true,
  };

  template <CSSPropertyPriority priority, ShouldUpdateNeedsApplyPass>
  void applyMatchedProperties(StyleResolverState&,
                              const MatchedPropertiesRange&,
                              bool important,
                              bool inheritedOnly,
                              NeedsApplyPass&);
  template <CSSPropertyPriority priority, ShouldUpdateNeedsApplyPass>
  void applyProperties(StyleResolverState&,
                       const StylePropertySet* properties,
                       bool isImportant,
                       bool inheritedOnly,
                       NeedsApplyPass&,
                       PropertyWhitelistType = PropertyWhitelistNone);
  template <CSSPropertyPriority priority>
  void applyAnimatedProperties(StyleResolverState&,
                               const ActiveInterpolationsMap&);
  template <CSSPropertyPriority priority>
  void applyAllProperty(StyleResolverState&,
                        const CSSValue&,
                        bool inheritedOnly,
                        PropertyWhitelistType);
  template <CSSPropertyPriority priority, ShouldUpdateNeedsApplyPass>
  void applyPropertiesForApplyAtRule(StyleResolverState&,
                                     const CSSValue&,
                                     bool isImportant,
                                     NeedsApplyPass&,
                                     PropertyWhitelistType);

  bool pseudoStyleForElementInternal(Element&,
                                     const PseudoStyleRequest&,
                                     const ComputedStyle* parentStyle,
                                     StyleResolverState&);
  bool hasAuthorBackground(const StyleResolverState&);
  bool hasAuthorBorder(const StyleResolverState&);

  PseudoElement* createPseudoElement(Element* parent, PseudoId);

  Document& document() { return *m_document; }

  static ComputedStyle* s_styleNotYetAvailable;

  MatchedPropertiesCache m_matchedPropertiesCache;

  Member<MediaQueryEvaluator> m_medium;
  MediaQueryResultList m_viewportDependentMediaQueryResults;
  MediaQueryResultList m_deviceDependentMediaQueryResults;

  Member<Document> m_document;
  SelectorFilter m_selectorFilter;

  HeapListHashSet<Member<CSSStyleSheet>, 16> m_pendingStyleSheets;

  Member<StyleRuleUsageTracker> m_tracker;

  bool m_printMediaType;

  unsigned m_styleSharingDepth;
  HeapVector<Member<StyleSharingList>, styleSharingMaxDepth>
      m_styleSharingLists;
};

}  // namespace blink

#endif  // StyleResolver_h
