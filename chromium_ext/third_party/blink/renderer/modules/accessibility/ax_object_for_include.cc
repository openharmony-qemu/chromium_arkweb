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

// This file is included inline by ax_object.cc via #include.
// It contains ArkWeb-specific CSS property serialization for OHOS
// accessibility consumption. Decoupled from the main Serialize() method
// to keep ax_object.cc clean and maintainable.

#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_ACCESSIBILITY)

#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "url/gurl.h"

namespace blink {
// ArkWeb: Serialize kValue for non-atomic text fields (contenteditable).
// For non-atomic text fields, we compute the value from DOM inner text
// (which skips placeholder <br>) and serialize it explicitly, even when
// empty. Without kValue present, the browser falls back to computing from
// AX child nodes, which includes a kLineBreak for the placeholder <br>,
// producing a phantom "\n".
// Truncation logic is inlined here because TruncateAndAddStringAttribute
// lives in an anonymous namespace inside ax_object.cc that is not visible
// after the closing of namespace blink.
void AXObject::SerializeArkWebValueAttribute(
    ui::AXNodeData* node_data) const {
  if (IsNonAtomicTextField()) {
    String value = SlowGetValueForControlIncludingContentEditable();
    if (value.empty()) {
      node_data->AddStringAttribute(
          ax::mojom::blink::StringAttribute::kValue, std::string());
    } else {
      std::string value_utf8 = value.Utf8(Utf8ConversionMode::kStrict);
      constexpr uint32_t kMaxLen = 10000;
      if (value_utf8.size() > kMaxLen) {
        std::string truncated;
        base::TruncateUTF8ToByteSize(value_utf8, kMaxLen, &truncated);
        value_utf8 = truncated;
      }
      if (!value_utf8.empty()) {
        node_data->AddStringAttribute(
            ax::mojom::blink::StringAttribute::kValue, value_utf8);
      }
    }
  } else {
    String value = GetValueForControl();
    if (!value.empty()) {
      std::string value_utf8 = value.Utf8(Utf8ConversionMode::kStrict);
      constexpr uint32_t kMaxLen = 10000;
      if (value_utf8.size() > kMaxLen) {
        std::string truncated;
        base::TruncateUTF8ToByteSize(value_utf8, kMaxLen, &truncated);
        value_utf8 = truncated;
      }
      if (!value_utf8.empty()) {
        node_data->AddStringAttribute(
            ax::mojom::blink::StringAttribute::kValue, value_utf8);
      }
    }
  }
}

bool AXObject::OnArkWebReplaceSelectedTextAction(const String& text) {
  if (!IsTextField()) {
    return false;
  }

  Document* document = GetDocument();
  if (!document) {
    return false;
  }

  LocalFrame* frame = document->GetFrame();
  if (!frame) {
    return false;
  }

  // Ensure the element has focus so the frame selection is within it.
  // ReplaceSelectionWithText operates on the frame selection, not on |element|,
  // so the caret must be inside the target element first.
  Element* element = GetElement();
  if (!element) {
    // Non-Element text field: cannot position the caret, abort to avoid
    // inserting text at an unrelated frame-selection location.
    return false;
  }
  if (!element->IsFocusedElementInDocument()) {
    element->Focus(FocusParams(SelectionBehaviorOnFocus::kNone,
                               mojom::blink::FocusType::kNone, nullptr));
    // element->Focus() may run synchronous script (focus event) that can
    // detach the document/frame. Re-fetch and re-validate before continuing.
    if (document != GetDocument()) {
      return false;
    }
    frame = document->GetFrame();
    if (!frame) {
      return false;
    }
    // kNone leaves the frame selection untouched; explicitly move the caret
    // to the end of the target element so ReplaceSelectionWithText inserts
    // here.
    frame->Selection().SetSelection(
        SelectionInDOMTree::Builder()
            .Collapse(Position::LastPositionInOrAfterNode(*element))
            .Build(),
        SetSelectionOptions::Builder()
            .SetShouldCloseTyping(true)
            .SetShouldClearTypingStyle(true)
            .SetDoNotSetFocus(true)
            .Build());
  }

  Editor& editor = frame->GetEditor();
  editor.ReplaceSelectionWithText(
      text, /*select_replacement=*/false, /*smart_replace=*/false,
      InputEvent::InputType::kInsertText);
  // ReplaceSelectionWithText may run synchronous script (beforeinput/input
  // events). Do not touch |this| members after this point; just return.
  return true;
}

static std::string FillLayerToBgSizeString(
    const FillLayer& bg_layers, float effective_zoom) {
  EFillSizeType size_type = bg_layers.SizeType();
  switch (size_type) {
    case EFillSizeType::kContain:
      return "contain";
    case EFillSizeType::kCover:
      return "cover";
    case EFillSizeType::kSizeLength: {
      const LengthSize& sz = bg_layers.SizeLength();
      auto length_to_str = [effective_zoom](const Length& l) -> std::string {
        if (l.IsAuto()) {
          return "auto";
        }
        if (l.IsFixed()) {
          float px = (effective_zoom > 0.01f)
                         ? (l.Pixels() / effective_zoom)
                         : l.Pixels();
          return base::NumberToString(px) + "px";
        }
        if (l.IsPercent()) {
          return base::NumberToString(l.Percent()) + "%";
        }
        return "auto";
      };
      std::string w = length_to_str(sz.Width());
      std::string h = length_to_str(sz.Height());
      if (w == "auto" && h == "auto") {
        return "auto";
      }
      return w + " " + h;
    }
    default:
      return "auto";
  }
}

static void SerializeBgImgAttr(ui::AXNodeData* node_data,
                                const std::string& bg_img_url) {
  GURL bg_gurl(bg_img_url);
  if (!bg_gurl.is_valid()) {
    return;
  }
  if (bg_gurl.SchemeIs(url::kHttpsScheme) ||
      bg_gurl.SchemeIs(url::kHttpScheme)) {
    GURL origin = bg_gurl.DeprecatedGetOriginAsURL();
    std::string sanitized = origin.is_valid() ? origin.spec() : bg_img_url;
    if (!sanitized.empty() && sanitized.back() == '/') {
      sanitized.pop_back();
    }
    sanitized += bg_gurl.GetPath();
    node_data->html_attributes.emplace_back("data-css-bg-img", sanitized);
  } else if (bg_gurl.SchemeIs(url::kDataScheme)) {
    std::string mime_prefix = "data:";
    size_t comma_pos = bg_img_url.find(',');
    if (comma_pos != std::string::npos) {
      mime_prefix = bg_img_url.substr(0, comma_pos);
    }
    node_data->html_attributes.emplace_back("data-css-bg-img", mime_prefix);
  }
}

static std::string ExtractBgImageUrl(const FillLayer& bg_layers) {
  if (!bg_layers.GetImage()) {
    return "";
  }
  CSSValue* css_val = bg_layers.GetImage()->CssValue();
  if (!css_val || !css_val->IsImageValue()) {
    return "";
  }
  auto* img_val = DynamicTo<CSSImageValue>(css_val);
  if (!img_val) {
    return "";
  }
  return img_val->UrlData().ResolvedUrl().Utf8();
}

static bool IsHiddenByFilter(const ComputedStyle* css_style) {
  if (!css_style->HasFilter()) {
    return false;
  }
  const FilterOperations& ops = css_style->Filter();
  for (size_t i = 0; i < ops.Operations().size(); ++i) {
    if (ops.Operations()[i]->GetType() !=
        FilterOperation::OperationType::kOpacity) {
      continue;
    }
    auto* op = DynamicTo<BasicComponentTransferFilterOperation>(
        ops.Operations()[i].Get());
    if (op && op->Amount() <= 0.0f) {
      return true;
    }
  }
  return false;
}

static bool IsInsetFullyClipped(const BasicShapeInset* inset) {
  if (!inset) {
    return false;
  }
  auto is_half_or_more = [](const Length& l) -> bool {
    if (l.IsPercent()) {
      return l.Percent() >= 50.0f;
    }
    return false;
  };
  return is_half_or_more(inset->Top()) && is_half_or_more(inset->Right()) &&
         is_half_or_more(inset->Bottom()) && is_half_or_more(inset->Left());
}

static bool IsCircleZeroRadius(const BasicShapeCircle* circle) {
  if (!circle) {
    return false;
  }
  const BasicShapeRadius& radius = circle->Radius();
  if (radius.GetType() != BasicShapeRadius::kValue) {
    return false;
  }
  const Length& r = radius.Value();
  if (r.IsPercent() && r.Percent() <= 0.0f) {
    return true;
  }
  if (r.IsFixed() && r.Pixels() <= 0.0f) {
    return true;
  }
  return false;
}

static bool IsHiddenByClipPath(const ComputedStyle* css_style) {
  if (!css_style->HasClipPath()) {
    return false;
  }
  const ClipPathOperation* clip = css_style->ClipPath();
  if (!clip || clip->GetType() != ClipPathOperation::kShape) {
    return false;
  }
  const auto* shape = DynamicTo<ShapeClipPathOperation>(clip);
  if (!shape) {
    return false;
  }
  const BasicShape* basic_shape = shape->GetBasicShape();
  if (!basic_shape) {
    return false;
  }
  if (basic_shape->GetType() == BasicShape::kBasicShapeInsetType) {
    return IsInsetFullyClipped(DynamicTo<BasicShapeInset>(basic_shape));
  }
  if (basic_shape->GetType() == BasicShape::kBasicShapeCircleType) {
    return IsCircleZeroRadius(DynamicTo<BasicShapeCircle>(basic_shape));
  }
  return false;
}

static bool ComputeCssVisible(const ComputedStyle* css_style) {
  if (css_style->Visibility() != EVisibility::kVisible) {
    return false;
  }
  if (css_style->Display() == EDisplay::kNone) {
    return false;
  }
  if (css_style->Opacity() <= 0.0f) {
    return false;
  }
  if (IsHiddenByFilter(css_style)) {
    return false;
  }
  if (IsHiddenByClipPath(css_style)) {
    return false;
  }
  if (css_style->EffectiveZoom() == 0.0f) {
    return false;
  }
  return true;
}

void AXObject::SerializeArkWebCSSAttributes(
    ui::AXNodeData* node_data) const {
  const ComputedStyle* css_style = GetComputedStyle();
  if (!css_style) {
    return;
  }

  auto add_attr = [&node_data](const char* key, const std::string& value) {
    node_data->html_attributes.emplace_back(key, value);
  };

  // Direct-serialize: z-index, font-size, image-rendering,
  // background-size-type, background-color.
  add_attr("data-css-zindex",
           base::NumberToString(css_style->EffectiveZIndex()));
  add_attr("data-css-font-size",
           base::NumberToString(static_cast<int>(css_style->SpecifiedFontSize())));
  add_attr("data-css-image-rendering",
           base::NumberToString(
               static_cast<int>(css_style->ImageRendering())));
  add_attr("data-css-bg-size-type",
           base::NumberToString(
               static_cast<int>(css_style->BackgroundLayers().SizeType())));
  add_attr("data-css-bg-color",
           base::NumberToString(BackgroundColor()));

  // Background image URL.
  const FillLayer& bg_layers = css_style->BackgroundLayers();
  std::string bg_img_url = ExtractBgImageUrl(bg_layers);
  if (!bg_img_url.empty()) {
    SerializeBgImgAttr(node_data, bg_img_url);
  }

  // Background size as string.
  float effective_zoom = css_style->EffectiveZoom();
  std::string bg_size_str = FillLayerToBgSizeString(bg_layers, effective_zoom);
  add_attr("data-css-bg-size", bg_size_str);

  // cssVisible composite.
  bool css_visible = ComputeCssVisible(css_style);
  add_attr("data-css-visible", css_visible ? "true" : "false");
}

}  // namespace blink

#endif  // BUILDFLAG(ARKWEB_ACCESSIBILITY)
