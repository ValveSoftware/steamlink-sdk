// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/base_feature_provider.h"

#include <stddef.h>

#include <stack>
#include <utility>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/complex_feature.h"
#include "extensions/common/features/simple_feature.h"

namespace extensions {

namespace {

bool IsNocompile(const base::Value& value) {
  bool nocompile = false;
  const base::DictionaryValue* as_dict = nullptr;
  if (value.GetAsDictionary(&as_dict)) {
    as_dict->GetBoolean("nocompile", &nocompile);
  } else {
    // "nocompile" is not supported for any other feature type.
  }
  return nocompile;
}

bool ParseFeature(const base::DictionaryValue* value,
                  const std::string& name,
                  SimpleFeature* feature) {
  feature->set_name(name);
  std::string error = feature->Parse(value);
  if (!error.empty())
    LOG(ERROR) << error;
  return error.empty();
}

}  // namespace

BaseFeatureProvider::BaseFeatureProvider(const base::DictionaryValue& root,
                                         FeatureFactory factory)
    : factory_(factory) {
  for (base::DictionaryValue::Iterator iter(root); !iter.IsAtEnd();
       iter.Advance()) {
    if (IsNocompile(iter.value())) {
      continue;
    }

    if (iter.value().GetType() == base::Value::TYPE_DICTIONARY) {
      std::unique_ptr<SimpleFeature> feature((*factory_)());

      std::vector<std::string> split = base::SplitString(
          iter.key(), ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

      // Push parent features on the stack, starting with the current feature.
      // If one of the features has "noparent" set, stop pushing features on
      // the stack. The features will then be parsed in order, starting with
      // the farthest parent that is either top level or has "noparent" set.
      std::stack<std::pair<std::string, const base::DictionaryValue*> >
          parse_stack;
      while (!split.empty()) {
        std::string parent_name = base::JoinString(split, ".");
        split.pop_back();
        if (root.HasKey(parent_name)) {
          const base::DictionaryValue* parent = nullptr;
          if (!root.GetDictionaryWithoutPathExpansion(parent_name, &parent)) {
            // If the parent is a complex feature, find the parent with the
            // 'default_parent' flag.
            const base::ListValue* parent_list = nullptr;
            CHECK(root.GetListWithoutPathExpansion(parent_name, &parent_list));
            for (size_t i = 0; i < parent_list->GetSize(); ++i) {
              CHECK(parent_list->GetDictionary(i, &parent));
              if (parent->HasKey("default_parent"))
                break;
              parent = nullptr;
            }
            CHECK(parent) << parent_name << " must declare one of its features"
                << " the default parent, with {\"default_parent\": true}.";
          }
          parse_stack.push(std::make_pair(parent_name, parent));
          bool no_parent = false;
          parent->GetBoolean("noparent", &no_parent);
          if (no_parent)
            break;
        }
      }

      CHECK(!parse_stack.empty());
      // Parse all parent features.
      bool parse_error = false;
      while (!parse_stack.empty()) {
        if (!ParseFeature(parse_stack.top().second,
                          parse_stack.top().first,
                          feature.get())) {
          parse_error = true;
          break;
        }
        parse_stack.pop();
      }

      if (parse_error)
        continue;

      features_[iter.key()] = std::move(feature);
    } else if (iter.value().GetType() == base::Value::TYPE_LIST) {
      // This is a complex feature.
      const base::ListValue* list =
          static_cast<const base::ListValue*>(&iter.value());
      CHECK_GT(list->GetSize(), 0UL);

      std::unique_ptr<ComplexFeature::FeatureList> features(
          new ComplexFeature::FeatureList());

      // Parse and add all SimpleFeatures from the list.
      for (const auto& entry : *list) {
        base::DictionaryValue* dict;
        if (!entry->GetAsDictionary(&dict)) {
          LOG(ERROR) << iter.key() << ": Feature rules must be dictionaries.";
          continue;
        }

        std::unique_ptr<SimpleFeature> feature((*factory_)());
        if (!ParseFeature(dict, iter.key(), feature.get()))
          continue;

        features->push_back(std::move(feature));
      }

      std::unique_ptr<ComplexFeature> feature(
          new ComplexFeature(std::move(features)));
      feature->set_name(iter.key());

      features_[iter.key()] = std::move(feature);
    } else {
      LOG(ERROR) << iter.key() << ": Feature description must be dictionary or"
                 << " list of dictionaries.";
    }
  }
}

BaseFeatureProvider::~BaseFeatureProvider() {
}

const FeatureMap& BaseFeatureProvider::GetAllFeatures() const {
  return features_;
}

Feature* BaseFeatureProvider::GetFeature(const std::string& name) const {
  FeatureMap::const_iterator iter = features_.find(name);
  if (iter != features_.end())
    return iter->second.get();
  else
    return nullptr;
}

Feature* BaseFeatureProvider::GetParent(Feature* feature) const {
  CHECK(feature);
  if (feature->no_parent())
    return nullptr;

  std::vector<std::string> split = base::SplitString(
      feature->name(), ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (split.size() < 2)
    return nullptr;
  split.pop_back();
  return GetFeature(base::JoinString(split, "."));
}

// Children of a given API are named starting with parent.name()+".", which
// means they'll be contiguous in the features_ std::map.
std::vector<Feature*> BaseFeatureProvider::GetChildren(const Feature& parent)
    const {
  std::string prefix = parent.name() + ".";
  const FeatureMap::const_iterator first_child = features_.lower_bound(prefix);

  // All children have names before (parent.name() + ('.'+1)).
  ++prefix[prefix.size() - 1];
  const FeatureMap::const_iterator after_children =
      features_.lower_bound(prefix);

  std::vector<Feature*> result;
  result.reserve(std::distance(first_child, after_children));
  for (FeatureMap::const_iterator it = first_child; it != after_children;
       ++it) {
    result.push_back(it->second.get());
  }
  return result;
}

}  // namespace extensions
