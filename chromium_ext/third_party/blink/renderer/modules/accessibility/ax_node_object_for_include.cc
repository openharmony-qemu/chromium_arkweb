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

// This file is included from ax_node_object.cc via #include. It wraps its
// content in namespace blink so it can be placed in the host file's top
// #include block alongside other headers. It must not be compiled standalone.
#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_NODE_OBJECT_H_
#error "must be in include form THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_NODE_OBJECT_H_"
#endif

#if BUILDFLAG(ARKWEB_ACCESSIBILITY)

namespace blink {

// Three-step algorithm to remove phantom newlines produced by placeholder
// <br> in contenteditable elements:
//   Step 1: original = innerText (contains phantom \n from placeholder BR,
//           plus block-boundary \n and user-newline \n)
//   Step 2: dirtiest = traverse DOM collecting real text, marking placeholder
//           BR positions with '\n' and stripping all other \n. A placeholder
//           BR is a BR with no siblings (the only child of its parent block).
//   Step 3: two-pointer walk over original and dirtiest. Where both have '\n',
//           it's a placeholder position — drop the '\n' from original; where
//           only original has '\n', it's a block boundary — keep it.
//
// Resource limits guard against pathological contenteditable trees; on
// overflow, fall back to original innerText (accepting phantom newlines) to
// preserve content completeness. Tuned to cover typical rich-text editors
// (nested div/p/span structures) while bounding traversal cost.
constexpr int kMaxCollectTextChildren = 200;
constexpr int kMaxCollectTextDepth = 20;

// Append text from a visible Text node, stripping user '\n' (which come from
// <br> or block boundaries in contenteditable, not from Text nodes).
static void AppendTextWithoutUserNewlines(const LayoutText& layout_text,
                                          StringBuilder& builder) {
  if (layout_text.HasEmptyText()) {
    return;
  }
  String text = layout_text.PlainText();
  unsigned pos = text.find('\n');
  if (pos == kNotFound) {
    builder.Append(text);
    return;
  }
  // Strip \n from user text — innerText handles block boundaries, and user
  // newlines in contenteditable come from <br> or block boundaries, not
  // from \n inside Text nodes.
  for (unsigned i = 0; i < text.length(); ++i) {
    if (text[i] != '\n') {
      builder.Append(text[i]);
    }
  }
}

// True if |br| is a placeholder BR: the only child of its parent (no
// siblings). User BRs always have sibling nodes.
static bool IsPlaceholderBR(const Node& br) {
  return !br.previousSibling() && !br.nextSibling();
}

// Step 2: Collect the dirtiest string in parallel with innerText.
// Produces '\n' only at placeholder-BR positions; all other \n (from user
// text or non-placeholder BRs) are stripped. Block-level boundaries produce
// no \n — innerText already handles those.
void AXNodeObject::CollectTextWithPlaceholderNewlines(
    const Node& node,
    StringBuilder& builder,
    bool& truncated,
    int depth) const {
  if (truncated || depth > kMaxCollectTextDepth ||
      NodeTraversal::CountChildren(node) > kMaxCollectTextChildren) {
    truncated = true;
    return;
  }
  for (const Node& child : NodeTraversal::ChildrenOf(node)) {
    const LayoutObject* layout_object = child.GetLayoutObject();
    if (!layout_object || layout_object->Style()->Visibility() !=
                              EVisibility::kVisible) {
      continue;
    }

    if (DynamicTo<Text>(child)) {
      const auto* layout_text = DynamicTo<LayoutText>(layout_object);
      if (layout_text) {
        AppendTextWithoutUserNewlines(*layout_text, builder);
      }
      continue;
    }

    if (IsA<HTMLBRElement>(child)) {
      // Placeholder BR emits '\n'; non-placeholder BR is skipped (innerText
      // block boundary handles it).
      if (IsPlaceholderBR(child)) {
        builder.Append('\n');
      }
      continue;
    }

    // For inline and block elements, recurse to collect child content.
    // No \n at boundaries — innerText handles block-level line breaks.
    CollectTextWithPlaceholderNewlines(child, builder, truncated, depth + 1);
    if (truncated) {
      return;
    }
  }
}

// Step 3: Two-pointer walk. Drop original[i] where dirtiest[j] is '\n'
// (placeholder position); keep everything else. |original| is the innerText
// result; |dirtiest| is the Step 2 result. |dirtiest| is always a subsequence
// of |original| with placeholder \n markers interleaved.
static String RemovePlaceholderNewlines(const String& original,
                                        const String& dirtiest) {
  StringBuilder result;
  unsigned i = 0;  // index into original
  unsigned j = 0;  // index into dirtiest
  unsigned orig_len = original.length();
  unsigned dirt_len = dirtiest.length();

  while (i < orig_len && j < dirt_len) {
    UChar oc = original[i];
    UChar dc = dirtiest[j];

    if (oc == '\n' && dc == '\n') {
      // Placeholder \n — drop from original, advance both.
      ++i;
      ++j;
    } else if (oc == dc) {
      // Same non-\n character — keep, advance both.
      result.Append(oc);
      ++i;
      ++j;
    } else if (oc == '\n') {
      // original has \n where dirtiest doesn't — block boundary or user
      // newline. Keep it, advance i only.
      result.Append(oc);
      ++i;
    } else if (dc == '\n') {
      // dirtiest has \n but original doesn't — misaligned, fall back.
      return original;
    } else {
      // Characters differ and neither is \n — misaligned. dirtiest is
      // supposed to be a subsequence of original (minus non-placeholder
      // \n); a character mismatch means our traversal and innerText
      // diverge, so the result is unreliable. Fall back to original.
      return original;
    }
  }

  // Append remaining original characters (trailing block-boundary \n etc.).
  // Note: if dirtiest was empty (e.g. all content visibility:hidden), the
  // main loop never executed and the full |original| is appended here —
  // this preserves innerText verbatim (including phantom newlines) as a
  // safe fallback. The result is identical to the truncated/unaligned
  // fallback path, so no separate signal is needed.
  while (i < orig_len) {
    result.Append(original[i]);
    ++i;
  }

  return result.ToString();
}

String AXNodeObject::GetContentEditableValueWithoutPlaceholderBR(
    Element& element) const {
  // Step 1: original = innerText (with phantom \n).
  String original = element.GetInnerTextWithoutUpdate();
  if (original.empty()) {
    return original;
  }

  // Step 2: collect dirtiest (placeholder positions marked with '\n').
  StringBuilder dirtiest_builder;
  bool truncated = false;
  CollectTextWithPlaceholderNewlines(element, dirtiest_builder, truncated, 0);
  String dirtiest = dirtiest_builder.ToString();

  // Fall back to innerText when content is too large or too deep, accepting
  // phantom newlines in exchange for content completeness.
  if (truncated) {
    return original;
  }

  // Step 3: two-pointer removal.
  String result = RemovePlaceholderNewlines(original, dirtiest);
  return result;
}

}  // namespace blink

#endif  // BUILDFLAG(ARKWEB_ACCESSIBILITY)
