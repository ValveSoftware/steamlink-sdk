/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScopedStyleTree_h
#define ScopedStyleTree_h

#include "core/css/resolver/ScopedStyleResolver.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class ScopedStyleTree {
    WTF_MAKE_NONCOPYABLE(ScopedStyleTree); WTF_MAKE_FAST_ALLOCATED;
public:
    ScopedStyleTree() : m_scopedResolverForDocument(0), m_buildInDocumentOrder(true) { }

    ScopedStyleResolver* ensureScopedStyleResolver(ContainerNode& scopingNode);
    ScopedStyleResolver* lookupScopedStyleResolverFor(const ContainerNode* scopingNode)
    {
        HashMap<const ContainerNode*, OwnPtr<ScopedStyleResolver> >::iterator it = m_authorStyles.find(scopingNode);
        return it != m_authorStyles.end() ? it->value.get() : 0;
    }

    ScopedStyleResolver* scopedStyleResolverFor(const ContainerNode& scopingNode);
    ScopedStyleResolver* addScopedStyleResolver(ContainerNode& scopingNode, bool& isNewEntry);
    void clear();

    // for fast-path.
    bool hasOnlyScopedResolverForDocument() const { return m_scopedResolverForDocument && m_authorStyles.size() == 1; }
    ScopedStyleResolver* scopedStyleResolverForDocument() const { return m_scopedResolverForDocument; }

    void resolveScopedStyles(const Element*, Vector<ScopedStyleResolver*, 8>&);
    void collectScopedResolversForHostedShadowTrees(const Element*, Vector<ScopedStyleResolver*, 8>&);
    void resolveScopedKeyframesRules(const Element*, Vector<ScopedStyleResolver*, 8>&);
    ScopedStyleResolver* scopedResolverFor(const Element*);

    void remove(const ContainerNode* scopingNode);

    void pushStyleCache(const ContainerNode& scopingNode, const ContainerNode* parent);
    void popStyleCache(const ContainerNode& scopingNode);

    void collectFeaturesTo(RuleFeatureSet& features);
    void setBuildInDocumentOrder(bool enabled) { m_buildInDocumentOrder = enabled; }
    bool buildInDocumentOrder() const { return m_buildInDocumentOrder; }

private:
    void setupScopedStylesTree(ScopedStyleResolver* target);

    bool cacheIsValid(const ContainerNode* parent) const { return parent && parent == m_cache.nodeForScopedStyles; }
    void resolveStyleCache(const ContainerNode* scopingNode);
    ScopedStyleResolver* enclosingScopedStyleResolverFor(const ContainerNode* scopingNode);

    void reparentNodes(const ScopedStyleResolver* oldParent, ScopedStyleResolver* newParent);

private:
    HashMap<const ContainerNode*, OwnPtr<ScopedStyleResolver> > m_authorStyles;
    ScopedStyleResolver* m_scopedResolverForDocument;
    bool m_buildInDocumentOrder;

    struct ScopedStyleCache {
        ScopedStyleResolver* scopedResolver;
        const ContainerNode* nodeForScopedStyles;

        void clear()
        {
            scopedResolver = 0;
            nodeForScopedStyles = 0;
        }
    };
    ScopedStyleCache m_cache;
};

inline ScopedStyleResolver* ScopedStyleTree::scopedResolverFor(const Element* element)
{
    if (!cacheIsValid(element))
        resolveStyleCache(element);

    return m_cache.scopedResolver;
}

} // namespace WebCore

#endif // ScopedStyleTree_h
