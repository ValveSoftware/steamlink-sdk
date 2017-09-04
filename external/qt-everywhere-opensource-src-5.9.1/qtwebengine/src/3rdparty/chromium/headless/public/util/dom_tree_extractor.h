// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_UTIL_DOM_TREE_EXTRACTOR_H_
#define HEADLESS_PUBLIC_UTIL_DOM_TREE_EXTRACTOR_H_

#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "headless/public/devtools/domains/css.h"
#include "headless/public/devtools/domains/dom.h"

namespace headless {
class HeadlessDevToolsClient;

// A utility class for extracting information from the DOM via DevTools. In
// addition, it also extracts details of bounding boxes and layout text (NB the
// exact layout should not be regarded as stable, it's subject to change without
// notice).
class DomTreeExtractor {
 public:
  explicit DomTreeExtractor(HeadlessDevToolsClient* devtools_client);
  ~DomTreeExtractor();

  using NodeId = int;
  using Index = size_t;

  class DomTree {
   public:
    DomTree();
    DomTree(DomTree&& other);
    ~DomTree();

    // Flattened dom tree. The root node is always the first entry.
    std::vector<const dom::Node*> dom_nodes_;

    // Map of node IDs to indexes into |dom_nodes_|.
    std::unordered_map<NodeId, Index> node_id_to_index_;

    std::vector<const css::LayoutTreeNode*> layout_tree_nodes_;

    std::vector<const css::ComputedStyle*> computed_styles_;

   private:
    friend class DomTreeExtractor;

    // Owns the raw pointers in |dom_nodes_|.
    std::unique_ptr<dom::GetDocumentResult> document_result_;

    // Owns the raw pointers in |layout_tree_nodes_|.
    std::unique_ptr<css::GetLayoutTreeAndStylesResult>
        layout_tree_and_styles_result_;

    DISALLOW_COPY_AND_ASSIGN(DomTree);
  };

  using DomResultCB = base::Callback<void(DomTree)>;

  // Extracts all nodes from the DOM.  This is an asynchronous operation and
  // it's an error to call ExtractDom while a previous operation is in flight.
  void ExtractDomTree(const std::vector<std::string>& css_style_whitelist,
                      DomResultCB callback);

 private:
  void OnDocumentFetched(std::unique_ptr<dom::GetDocumentResult> result);

  void OnLayoutTreeAndStylesFetched(
      std::unique_ptr<css::GetLayoutTreeAndStylesResult> result);

  void MaybeExtractDomTree();
  void EnumerateNodes(const dom::Node* node);
  void ExtractLayoutTreeNodes();
  void ExtractComputedStyles();

  DomResultCB callback_;
  DomTree dom_tree_;
  bool work_in_progress_;
  HeadlessDevToolsClient* devtools_client_;  // NOT OWNED
  base::WeakPtrFactory<DomTreeExtractor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DomTreeExtractor);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_UTIL_DOM_TREE_EXTRACTOR_H_
