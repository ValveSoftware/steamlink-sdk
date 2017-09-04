// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/dom_tree_extractor.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "headless/public/headless_devtools_client.h"

namespace headless {

DomTreeExtractor::DomTreeExtractor(HeadlessDevToolsClient* devtools_client)
    : work_in_progress_(false),
      devtools_client_(devtools_client),
      weak_factory_(this) {}

DomTreeExtractor::~DomTreeExtractor() {}

void DomTreeExtractor::ExtractDomTree(
    const std::vector<std::string>& css_style_whitelist,
    DomResultCB callback) {
  DCHECK(!work_in_progress_);
  work_in_progress_ = true;

  callback_ = std::move(callback);

  devtools_client_->GetDOM()->GetDocument(
      dom::GetDocumentParams::Builder().SetDepth(-1).SetPierce(true).Build(),
      base::Bind(&DomTreeExtractor::OnDocumentFetched,
                 weak_factory_.GetWeakPtr()));

  devtools_client_->GetCSS()->GetExperimental()->GetLayoutTreeAndStyles(
      css::GetLayoutTreeAndStylesParams::Builder()
          .SetComputedStyleWhitelist(css_style_whitelist)
          .Build(),
      base::Bind(&DomTreeExtractor::OnLayoutTreeAndStylesFetched,
                 weak_factory_.GetWeakPtr()));
}

void DomTreeExtractor::OnDocumentFetched(
    std::unique_ptr<dom::GetDocumentResult> result) {
  dom_tree_.document_result_ = std::move(result);
  MaybeExtractDomTree();
}

void DomTreeExtractor::OnLayoutTreeAndStylesFetched(
    std::unique_ptr<css::GetLayoutTreeAndStylesResult> result) {
  dom_tree_.layout_tree_and_styles_result_ = std::move(result);
  MaybeExtractDomTree();
}

void DomTreeExtractor::MaybeExtractDomTree() {
  if (dom_tree_.document_result_ && dom_tree_.layout_tree_and_styles_result_) {
    EnumerateNodes(dom_tree_.document_result_->GetRoot());
    ExtractLayoutTreeNodes();
    ExtractComputedStyles();

    work_in_progress_ = false;

    callback_.Run(std::move(dom_tree_));
  }
}

void DomTreeExtractor::EnumerateNodes(const dom::Node* node) {
  // Allocate an index and record the node pointer.
  size_t index = dom_tree_.node_id_to_index_.size();
  dom_tree_.node_id_to_index_[node->GetNodeId()] = index;
  dom_tree_.dom_nodes_.push_back(node);

  if (node->HasContentDocument())
    EnumerateNodes(node->GetContentDocument());

  if (node->HasChildren()) {
    for (const std::unique_ptr<dom::Node>& child : *node->GetChildren()) {
      EnumerateNodes(child.get());
    }
  }
}

void DomTreeExtractor::ExtractLayoutTreeNodes() {
  dom_tree_.layout_tree_nodes_.reserve(
      dom_tree_.layout_tree_and_styles_result_->GetLayoutTreeNodes()->size());

  for (const std::unique_ptr<css::LayoutTreeNode>& layout_node :
       *dom_tree_.layout_tree_and_styles_result_->GetLayoutTreeNodes()) {
    std::unordered_map<NodeId, size_t>::const_iterator it =
        dom_tree_.node_id_to_index_.find(layout_node->GetNodeId());
    DCHECK(it != dom_tree_.node_id_to_index_.end());
    dom_tree_.layout_tree_nodes_.push_back(layout_node.get());
  }
}

void DomTreeExtractor::ExtractComputedStyles() {
  dom_tree_.computed_styles_.reserve(
      dom_tree_.layout_tree_and_styles_result_->GetComputedStyles()->size());

  for (const std::unique_ptr<css::ComputedStyle>& computed_style :
       *dom_tree_.layout_tree_and_styles_result_->GetComputedStyles()) {
    dom_tree_.computed_styles_.push_back(computed_style.get());
  }
}

DomTreeExtractor::DomTree::DomTree() {}
DomTreeExtractor::DomTree::~DomTree() {}

DomTreeExtractor::DomTree::DomTree(DomTree&& other) = default;

}  // namespace headless
