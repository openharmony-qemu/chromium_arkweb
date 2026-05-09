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

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "components/metrics/metrics_log.h"
#include "arkweb_histogram_filter.h"
#include "base/logging.h"

namespace metrics {

const char* const ArkWebHistogramFilter::kUseCounterPrefixes[] = {
    "Blink.UseCounter.Features",
    "Blink.UseCounter.MainFrame.Features",
    "Blink.UseCounter.WebDXFeatures",
    "Blink.UseCounter.CSSProperties",
    "Blink.UseCounter.AnimatedCSSProperties",
    "Blink.UseCounter.Extensions.Features",
};

ArkWebHistogramFilter::ArkWebHistogramFilter(MetricsLog* log)
    : log_(log) {
  DCHECK(log_);
}

void ArkWebHistogramFilter::RecordDelta(
    const base::HistogramBase& histogram,
    const base::HistogramSamples& snapshot) {
  if (!IsUseCounterHistogram(histogram.histogram_name())) {
    return;
  }

  CHECK(histogram.HasFlags(base::HistogramBase::kUmaTargetedHistogramFlag));

  log_->RecordHistogramDelta(histogram.histogram_name(), snapshot);
}

bool ArkWebHistogramFilter::IsUseCounterHistogram(
    const std::string_view& name) const {
  for (const char* prefix : kUseCounterPrefixes) {
    if (name.find(prefix) == 0) {
      return true;
    }
  }
  return false;
}

} // namespace metrics