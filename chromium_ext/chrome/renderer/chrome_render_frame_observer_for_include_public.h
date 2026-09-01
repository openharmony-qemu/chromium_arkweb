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

#ifndef CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_FOR_INCLUDE_PUBLIC_H_
#define CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_FOR_INCLUDE_PUBLIC_H_

#ifndef CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_
#error \
    "must be in include form CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_"
#endif

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
  // Re-run CapturePageText for translate language detection when requested by
  // the browser process (GetWebLanguageAsync cache miss).
  void CapturePageTextForTranslate() override;
#endif  // BUILDFLAG(ARKWEB_COMPOSITE_RENDER)

#endif  // CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_FOR_INCLUDE_PUBLIC_H_
