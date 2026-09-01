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

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
void ChromeRenderFrameObserver::CapturePageTextForTranslate() {
  // Only extract page text and run language detection, without triggering
  // phishing detection or optimization guide. This avoids side effects on
  // other modules that CapturePageText would invoke.
  if (!translate_agent_) {
    return;
  }
  if (!render_frame()->IsMainFrame()) {
    return;
  }
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame) {
    return;
  }
  scoped_refptr<const base::RefCountedString16> contents =
      base::MakeRefCounted<const base::RefCountedString16>(
          blink::WebFrameContentDumper::DumpFrameTreeAsText(
              frame, kMaxIndexChars)
              .Utf16());
  translate_agent_->PageCaptured(contents);
}
#endif  // BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
