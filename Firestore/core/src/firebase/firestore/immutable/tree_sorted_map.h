/*
 * Copyright 2018 Google
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIRESTORE_CORE_SRC_FIREBASE_FIRESTORE_IMMUTABLE_TREE_SORTED_MAP_H_
#define FIRESTORE_CORE_SRC_FIREBASE_FIRESTORE_IMMUTABLE_TREE_SORTED_MAP_H_

#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>
#include <utility>

#include "Firestore/core/src/firebase/firestore/immutable/llrb_node.h"
#include "Firestore/core/src/firebase/firestore/immutable/map_entry.h"
#include "Firestore/core/src/firebase/firestore/immutable/sorted_map_base.h"
#include "Firestore/core/src/firebase/firestore/util/comparator_holder.h"
#include "Firestore/core/src/firebase/firestore/util/comparison.h"
#include "Firestore/core/src/firebase/firestore/util/iterator_adaptors.h"

namespace firebase {
namespace firestore {
namespace immutable {
namespace impl {

/**
 * TreeSortedMap is a value type containing a map. It is immutable, but has
 * methods to efficiently create new maps that are mutations of it.
 */
template <typename K, typename V, typename C = util::Comparator<K>>
class TreeSortedMap : public SortedMapBase, private util::ComparatorHolder<C> {
 public:
  /**
   * The type of the entries stored in the map.
   */
  using value_type = std::pair<K, V>;

  /**
   * The type of the node containing entries of value_type.
   */
  using node_type = LlrbNode<K, V>;
  using const_iterator = typename node_type::const_iterator;

  /**
   * Creates an empty TreeSortedMap.
   */
  explicit TreeSortedMap(const C& comparator = {})
      : util::ComparatorHolder<C>{comparator} {
  }

  /**
   * Creates a TreeSortedMap from a range of pairs to insert.
   */
  template <typename Range>
  static TreeSortedMap Create(const Range& range, const C& comparator) {
    node_type node;
    for (auto&& element : range) {
      node = node.insert(element.first, element.second, comparator);
    }
    return TreeSortedMap{std::move(node), comparator};
  }

  /** Returns true if the map contains no elements. */
  bool empty() const {
    return root_.empty();
  }

  /** Returns the number of items in this map. */
  size_type size() const {
    return root_.size();
  }

  const node_type& root() const {
    return root_;
  }

  /**
   * Creates a new map identical to this one, but with a key-value pair added or
   * updated.
   *
   * @param key The key to insert/update.
   * @param value The value to associate with the key.
   * @return A new dictionary with the added/updated value.
   */
  TreeSortedMap insert(const K& key, const V& value) const {
    const C& comparator = this->comparator();
    return TreeSortedMap{root_.insert(key, value, comparator), comparator};
  }

  /**
   * Creates a new map identical to this one, but with a key removed from it.
   *
   * @param key The key to remove.
   * @return A new map without that value.
   */
  TreeSortedMap erase(const K& key) const {
    // TODO(wilhuff): Actually implement erase
    (void)key;
    return TreeSortedMap{this->comparator()};
  }

  bool contains(const K& key) const {
    // Inline the tree traversal here to avoid building up the stack required
    // to construct a full iterator.
    const C& comparator = this->comparator();
    const node_type* node = &root();
    while (!node->empty()) {
      util::ComparisonResult cmp = util::Compare(key, node->key(), comparator);
      if (cmp == util::ComparisonResult::Same) {
        return true;
      } else if (cmp == util::ComparisonResult::Ascending) {
        node = &node->left();
      } else {
        node = &node->right();
      }
    }
    return false;
  }

  /**
   * Finds a value in the map.
   *
   * @param key The key to look up.
   * @return An iterator pointing to the entry containing the key, or end() if
   *     not found.
   */
  const_iterator find(const K& key) const {
    const_iterator found = LowerBound(key);
    if (!found.is_end() && !this->comparator()(key, found->first)) {
      return found;
    } else {
      return end();
    }
  }

  /**
   * Finds the index of the given key in the map.
   *
   * @param key The key to look up.
   * @return The index of the entry containing the key, or npos if not found.
   */
  size_type find_index(const K& key) const {
    const C& comparator = this->comparator();

    size_type pruned_nodes = 0;
    const node_type* node = &root_;
    while (!node->empty()) {
      util::ComparisonResult cmp = util::Compare(key, node->key(), comparator);
      if (cmp == util::ComparisonResult::Same) {
        return pruned_nodes + node->left().size();

      } else if (cmp == util::ComparisonResult::Ascending) {
        node = &node->left();

      } else if (cmp == util::ComparisonResult::Descending) {
        pruned_nodes += node->left().size() + 1;
        node = &node->right();
      }
    }
    return npos;
  }

  /**
   * Returns a forward iterator pointing to the first entry in the map. If there
   * are no entries in the map, begin() == end().
   *
   * See LlrbNodeIterator for details
   */
  const_iterator begin() const {
    return const_iterator::Begin(&root_);
  }

  /**
   * Returns an iterator pointing past the last entry in the map.
   */
  const_iterator end() const {
    return const_iterator::End();
  }

 private:
  TreeSortedMap(node_type&& root, const C& comparator) noexcept
      : util::ComparatorHolder<C>{comparator}, root_{std::move(root)} {
  }

  const_iterator LowerBound(const K& key) const noexcept {
    return const_iterator::LowerBound(&root_, key, this->comparator());
  }

  TreeSortedMap Wrap(node_type&& root) noexcept {
    return TreeSortedMap{std::move(root), this->comparator()};
  }

  node_type root_;
};

}  // namespace impl
}  // namespace immutable
}  // namespace firestore
}  // namespace firebase

#endif  // FIRESTORE_CORE_SRC_FIREBASE_FIRESTORE_IMMUTABLE_TREE_SORTED_MAP_H_
