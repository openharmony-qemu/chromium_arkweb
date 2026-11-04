/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(OH_ENABLE_HEAP_TRANSLATE)
#include "snapshot-generator.h"

#include "arkweb/chromium_ext/v8/dfx/heap_dump/heap-dump-log.h"
#include "include/v8-internal.h"

namespace v8 {
namespace dfx {

////////// Edge
Edge::Edge(Type type, uint32_t from, uint32_t name_or_index, Node* to)
    : type_(static_cast<uint8_t>(type)),
      from_index_(from),
      name_or_index_(name_or_index),
      to_node_(to) {}

////////// Node
void Node::SetReference(Edge::Type type, Node* child_node,
                        uint32_t name_or_index, SnapshotGenerator* generator) {
  Edge* e = new Edge(type, this->id_, name_or_index, child_node);
  childs_.push_back(e);
  generator->AdvanceEdgeCount();
}

////////// SnapshotGenerator

SnapshotGenerator::SnapshotGenerator() {
  // see SerializeStrings, the first is <dummy>
  (void)GetStringId("<dummy>");
}
SnapshotGenerator::~SnapshotGenerator() {
  for (auto node : nodes_) {
    for (auto edge : node->childs_) {
      delete edge;
    }
    delete node;
  }
}

std::vector<Node*>& SnapshotGenerator::nodes() { return nodes_; }

void SnapshotGenerator::AdvanceEdgeCount() { edge_count_++; }

uint32_t SnapshotGenerator::EdgeCount() { return edge_count_; }

std::unordered_map<std::string, uint32_t>& SnapshotGenerator::strings() {
  return strings_;
}

Node* SnapshotGenerator::GetNode(internal::Address obj_addr) {
  auto it = node_by_addr_.find(obj_addr);
  if (it != node_by_addr_.end()) {
    return nodes_[it->second];
  }
  return nullptr;
}

Node* SnapshotGenerator::CreateNode(Node::Type type, std::string name,
                                    uint32_t self_size) {
  // Note: trace_node_id is not support now
  Node* n = new Node();
  n->type_ = static_cast<uint32_t>(type);
  n->name_ = GetStringId(name);
  // Note: id_ is used when compare two snapshot, get from dump
  n->id_ = static_cast<uint32_t>(nodes_.size());
  n->self_size_ = self_size;
  nodes_.push_back(n);
  return n;
}

Node* SnapshotGenerator::AddNode(internal::Address obj_addr, Node::Type type,
                                 std::string name, uint32_t self_size) {
  // Rawheap real addresses are graph identities only. They key the translated
  // node map like V8HeapExplorer's HeapObjectsMap, but are never dereferenced
  // by the offline translator.
  if (Node* n = GetNode(obj_addr); n != nullptr) {
    if (n->type_ == static_cast<uint32_t>(Node::Type::kNotInit)) {
      n->type_ = static_cast<uint32_t>(type);
      n->name_ = GetStringId(name);
      n->self_size_ = self_size;
    } else {
      CHECK(n->type_ == static_cast<uint32_t>(type) &&
            n->self_size_ == self_size);
    }
    return n;
  }
  Node* n = CreateNode(type, name, self_size);
  node_by_addr_[obj_addr] = n->id_;
  return n;
}

SnapshotGenerator::RetagNodeStatus SnapshotGenerator::RetagNodeIfExisting(
    internal::Address obj_addr, const std::string& name, Node::Type type,
    bool update_type, bool overwrite_system_name) {
  RetagNodeStatus status;
  Node* node = GetNode(obj_addr);
  if (node == nullptr ||
      node->type_ == static_cast<uint32_t>(Node::Type::kNotInit)) {
    return status;
  }
  status.node_found = true;

  // Native TagObject's default path only overwrites empty names. Some ArkWeb
  // system names are fallback names for native-empty nodes, so callers must
  // opt in before those placeholders can be replaced.
  bool should_update_name = false;
  auto empty_name = strings_.find("");
  if (empty_name != strings_.end() && node->name_ == empty_name->second) {
    should_update_name = true;
  } else if (overwrite_system_name) {
    for (const auto& entry : strings_) {
      if (entry.second == node->name_ &&
          entry.first.rfind("system / ", 0) == 0) {
        should_update_name = true;
        break;
      }
    }
  }
  if (should_update_name) {
    node->name_ = GetStringId(name);
    status.name_updated = true;
  } else if (!name.empty()) {
    status.name_suppressed = true;
  }
  if (update_type && node->type_ != static_cast<uint32_t>(type)) {
    node->type_ = static_cast<uint32_t>(type);
    status.type_updated = true;
  }
  return status;
}

void SnapshotGenerator::AddSyntheticRootNodes() {
  AddRootNode();
  AddGcRootNode();
  CHECK(nodes_.size() == 2 && root_ && gc_root_);
  for (int root = 0; root < static_cast<int>(i::Root::kNumberOfRoots); root++) {
    gc_subroot_nodes_.push_back(
        CreateNode(Node::Type::kSynthetic,
                   i::RootVisitor::RootName(static_cast<i::Root>(root)), 0));
    // maybe SetIndexedAutoIndexReference?
    gc_root_->SetReference(Edge::Type::kElement, gc_subroot_nodes_.back(),
                           static_cast<uint32_t>(gc_root_->childs_.size() + 1),
                           this);
  }
}
void SnapshotGenerator::AddRootNode() {
  CHECK(nodes_.empty());
  root_ = CreateNode(Node::Type::kSynthetic, "", 0);
}
void SnapshotGenerator::AddGcRootNode() {
  CHECK(nodes_.size() == 1 && root_ && !gc_root_);
  gc_root_ = CreateNode(Node::Type::kSynthetic, "(GC roots)", 0);
  // see SetRootGcRootsReference in heap-snapshot-generator.cc
  // SetIndexedAutoIndexReference
  root_->SetReference(Edge::Type::kElement, gc_root_,
                      static_cast<uint32_t>(root_->childs_.size()) + 1, this);
}

uint32_t SnapshotGenerator::GetStringId(std::string str) {
  auto it = strings_.find(str);
  if (it != strings_.end()) {
    return it->second;
  }
  uint32_t id = static_cast<uint32_t>(strings_.size());
  strings_.emplace(str, id);
  return id;
}

bool SnapshotGenerator::SetReference(Edge::Type type, internal::Address parent,
                                     uint32_t name_or_index,
                                     internal::Address child) {
  Node* parent_node = GetNode(parent);
  if (!parent_node) {
    LogInfo("parent_node addr " + std::to_string(parent));
    return false;
  }
  Node* child_node = GetNode(child);
  if (!child_node) {
    LogInfo("child_node addr " + std::to_string(child));
    return false;
  }
  parent_node->SetReference(type, child_node, name_or_index, this);
  return true;
}

bool SnapshotGenerator::SetShortcutReference(i::Address parent,
                                             std::string reference_name,
                                             i::Address child) {
  return SetReference(Edge::Type::kShortcut, parent, GetStringId(reference_name),
                      child);
}

bool SnapshotGenerator::SetContextReference(i::Address parent,
                                            std::string reference_name,
                                            i::Address child,
                                            int field_offset) {
  if (!SetReference(Edge::Type::kContextVariable, parent,
                    GetStringId(reference_name), child)) {
    return false;
  }
  MarkVisitedField(field_offset);
  return true;
}

bool SnapshotGenerator::SetElementReference(i::Address parent, uint32_t index,
                                            i::Address child) {
  return SetReference(Edge::Type::kElement, parent, index, child);
}

bool SnapshotGenerator::SetInternalReference(i::Address parent,
                                             std::string reference_name,
                                             i::Address child,
                                             int field_offset) {
  if (!SetReference(Edge::Type::kInternal, parent, GetStringId(reference_name),
                    child)) {
    return false;
  }
  MarkVisitedField(field_offset);
  return true;
}

// SetHiddenReference in heap-snapshot-generator.cc
bool SnapshotGenerator::SetHiddenReference(internal::Address parent,
                                           uint32_t index,
                                           internal::Address child) {
  // Note: Check IsEssentialObject and IsEssentialHiddenReference
  return SetReference(Edge::Type::kHidden, parent, index, child);
}

bool SnapshotGenerator::SetWeakReference(i::Address parent, uint32_t index,
                                         i::Address child,
                                         std::optional<int> field_offset) {
  // NIY:IsEssentialObject
  if (!SetReference(Edge::Type::kWeak, parent, GetStringId(std::to_string(index)),
                    child)) {
    return false;
  }
  if (field_offset.has_value()) {
    MarkVisitedField(*field_offset);
  }
  return true;
}

bool SnapshotGenerator::SetWeakReference(i::Address parent,
                                         std::string reference_name,
                                         i::Address child,
                                         std::optional<int> field_offset) {
  // NIY:IsEssentialObject
  if (!SetReference(Edge::Type::kWeak, parent, GetStringId(reference_name),
                    child)) {
    return false;
  }
  if (field_offset.has_value()) {
    MarkVisitedField(*field_offset);
  }
  return true;
}

bool SnapshotGenerator::SetPropertyReference(i::Address parent,
                                             std::string reference_name,
                                             i::Address child, int field_offset,
                                             bool unresolved_name) {
  // See SetPropertyReference in heap-snapshot-generator.cc.
  // For resolved names, classify as kProperty; for synthetic fallback names
  // generated from descriptor/dictionary indexes, classify as kInternal via
  // unresolved_name.
  Edge::Type type = (unresolved_name || reference_name.empty())
                        ? Edge::Type::kInternal
                        : Edge::Type::kProperty;
  if (!SetReference(type, parent, GetStringId(reference_name), child)) {
    return false;
  }
  MarkVisitedField(field_offset);
  return true;
}

bool SnapshotGenerator::SetDataOrAccessorPropertyReference(
    i::Address parent, std::string reference_name, i::Address child,
    int field_offset) {
  // see SetDataOrAccessorPropertyReference in heap-snapshot-generator.cc
  // Note: V8 version has PropertyKind parameter to handle accessor properties.
  // For accessor properties, it would call ExtractAccessorPairProperty.
  // Here we simplify by always treating as data property.
  // NIY: Handle accessor properties if needed
  return SetPropertyReference(parent, reference_name, child, field_offset);
}

void SnapshotGenerator::SetUserGlobalReference(i::Address child) {
  // see SetUserGlobalReference in heap-snapshot-generator.cc
  // Creates a shortcut reference from root to the child object
  Node* child_node = GetNode(child);
  if (!child_node) {
    // In V8, DCHECK_NOT_NULL is used, but we gracefully handle nullptr
    return;
  }
  CHECK(root_);
  // Use auto-indexing for the edge name (like SetNamedAutoIndexReference in V8)
  root_->SetReference(Edge::Type::kShortcut, child_node,
                      static_cast<uint32_t>(root_->childs_.size()) + 1, this);
}

void SnapshotGenerator::SetGcSubrootReference(i::Root root,
                                              std::string description,
                                              bool is_weak,
                                              i::Address root_address) {
  // !!!root_address must not be smi, check it in dump-roots.cc
  CHECK(root_ && gc_root_ &&
        static_cast<size_t>(root) < gc_subroot_nodes_.size());
  Node* root_node = gc_subroot_nodes_[static_cast<uint32_t>(root)];
  Node* root_obj_node = AddNode(root_address, Node::Type::kNotInit, "", 0);

  Edge::Type edge_type = is_weak ? Edge::Type::kWeak : Edge::Type::kInternal;
  // maybe need description? see SetGcSubrootReference in
  // heap-snapshot-generator.cc
  root_node->SetReference(edge_type, root_obj_node, GetStringId(description),
                          this);
}

void SnapshotGenerator::MarkVisitedField(int offset) {
  if (offset < 0) {
    return;
  }
  int index = offset / i::kTaggedSize;
  DCHECK_LT(index, max_pointers_);
  DCHECK(!visited_fields_[index]);
  visited_fields_[index] = true;
}

}  // namespace dfx
}  // namespace v8
#endif
