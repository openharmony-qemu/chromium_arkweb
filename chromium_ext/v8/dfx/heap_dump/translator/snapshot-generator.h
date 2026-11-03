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

#ifndef SNAPSHOT_GENERATOR_H
#define SNAPSHOT_GENERATOR_H
#if defined(OH_ENABLE_HEAP_TRANSLATE)
#include <set>

#include "include/v8-profiler.h"
#include "src/objects/visitors.h"

// see HeapSnapshotJSONSerializer::SerializeSnapshot in
// heap-snapshot-generator.cc
namespace v8 {
namespace dfx {
class SnapshotGenerator;
class Node;

// copy from heap-snapshot-generator.h
// edge_fields
class Edge {
 public:
  enum class Type : uint8_t {
    kContextVariable = v8::HeapGraphEdge::kContextVariable,
    kElement = v8::HeapGraphEdge::kElement,
    kProperty = v8::HeapGraphEdge::kProperty,
    kInternal = v8::HeapGraphEdge::kInternal,
    kHidden = v8::HeapGraphEdge::kHidden,
    kShortcut = v8::HeapGraphEdge::kShortcut,
    kWeak = v8::HeapGraphEdge::kWeak,
    kNumTypes
  };
  Edge(Type type, uint32_t from, uint32_t name_or_index, Node* to);
  uint32_t type_ : 8;
  uint32_t from_index_ : 24;
  uint32_t name_or_index_;  // index to string
  Node* to_node_;
};

// node_fields
class Node {
 public:
  enum class Type : uint8_t {
    kHidden = v8::HeapGraphNode::kHidden,
    kArray = v8::HeapGraphNode::kArray,
    kString = v8::HeapGraphNode::kString,
    kObject = v8::HeapGraphNode::kObject,
    kCode = v8::HeapGraphNode::kCode,
    kClosure = v8::HeapGraphNode::kClosure,
    kRegExp = v8::HeapGraphNode::kRegExp,
    kHeapNumber = v8::HeapGraphNode::kHeapNumber,
    kNative = v8::HeapGraphNode::kNative,
    kSynthetic = v8::HeapGraphNode::kSynthetic,
    kConsString = v8::HeapGraphNode::kConsString,
    kSlicedString = v8::HeapGraphNode::kSlicedString,
    kSymbol = v8::HeapGraphNode::kSymbol,
    kBigInt = v8::HeapGraphNode::kBigInt,
    kObjectShape = v8::HeapGraphNode::kObjectShape,
    kNumTypes,
    kNotInit,
  };
  Node() = default;
  void SetReference(Edge::Type type, Node* child_node, uint32_t name_or_index,
                    SnapshotGenerator* generator);
  uint32_t type_ : 8 {0};
  uint32_t name_ : 24 {0};  // index to string
  uint32_t id_;
  uint32_t self_size_{0};
  uint8_t detachedness_{0};
  // a node will visited only once if use heap iterator
  std::vector<Edge*> childs_;
};

class SnapshotGenerator {
 public:
  explicit SnapshotGenerator();
  ~SnapshotGenerator();
  struct RetagNodeStatus {
    bool node_found{false};
    bool name_updated{false};
    bool name_suppressed{false};
    bool type_updated{false};

    bool Applied() const { return name_updated || type_updated; }
  };
  // get node index from address; get address from
  Node* GetNode(i::Address obj_addr);
  Node* AddNode(i::Address obj_addr, Node::Type type, std::string name,
                uint32_t self_size);
  RetagNodeStatus RetagNodeIfExisting(i::Address obj_addr,
                                      const std::string& name, Node::Type type,
                                      bool update_type,
                                      bool overwrite_system_name);
  void AddSyntheticRootNodes();

  std::vector<Node*>& nodes();

  void AdvanceEdgeCount();
  uint32_t EdgeCount();
  std::unordered_map<std::string, uint32_t>& strings();

  uint32_t GetStringId(std::string str);

  // reference
  bool SetContextReference(i::Address parent, std::string reference_name,
                           i::Address child, int field_offset = -1);
  bool SetShortcutReference(i::Address parent, std::string reference_name,
                            i::Address child);
  bool SetElementReference(i::Address parent, uint32_t index, i::Address child);
  bool SetInternalReference(i::Address parent, std::string reference_name,
                            i::Address child, int field_offset);
  bool SetHiddenReference(i::Address parent, uint32_t index, i::Address child);
  bool SetWeakReference(i::Address parent, uint32_t index, i::Address child,
                        std::optional<int> field_offset);
  bool SetWeakReference(i::Address parent, std::string reference_name,
                        i::Address child, std::optional<int> field_offset);
  // unresolved_name indicates the property name is synthetic (for example
  // "(property #N)") instead of a real JavaScript Name/String.
  // V8 native snapshots use actual Name/String payload when available, while
  // rawheap snapshots may only know descriptor/dictionary entry indexes and
  // synthesize fallback names. Keep these edges for reachability, but classify
  // them as kInternal so synthetic placeholders are not shown as JS property
  // names.
  bool SetPropertyReference(i::Address parent, std::string reference_name,
                            i::Address child, int field_offset = -1,
                            bool unresolved_name = false);
  bool SetDataOrAccessorPropertyReference(i::Address parent,
                                          std::string reference_name,
                                          i::Address child,
                                          int field_offset = -1);

  void SetUserGlobalReference(i::Address child);
  void SetGcSubrootReference(i::Root root, std::string description,
                             bool is_weak, i::Address root_address);

 private:
  Node* CreateNode(Node::Type type, std::string name, uint32_t self_size);
  void AddRootNode();
  void AddGcRootNode();

  bool SetReference(Edge::Type type, i::Address parent, uint32_t name_or_index,
                    i::Address child);
  void MarkVisitedField(int offset);

  std::vector<Node*> nodes_;
  Node* root_{nullptr};
  Node* gc_root_{nullptr};
  std::vector<Node*> gc_subroot_nodes_;
  uint32_t edge_count_{0};
  std::unordered_map<i::Address, uint32_t> node_by_addr_;

  std::unordered_map<std::string, uint32_t> strings_;

  std::vector<bool> visited_fields_;
  size_t max_pointers_;
  friend class ObjectTranslator;
  friend class ObjectDetailVisitor;
};
}  // namespace dfx
}  // namespace v8
#endif
#endif
