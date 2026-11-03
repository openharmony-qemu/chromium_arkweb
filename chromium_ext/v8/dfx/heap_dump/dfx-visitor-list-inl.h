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

#ifndef DFX_VISITOR_LIST_INL_H
#define DFX_VISITOR_LIST_INL_H

// ========== VisitorId Coverage Check Functions ==========
// These functions check how many VisitorIds are covered by DFX macros
// ========== VisitorId Coverage Check Functions ==========
// These functions check how many VisitorIds are covered by DFX macros
// and compare with V8 native VisitorIds

#include <cstdio>
#include <cstring>
#include <set>

#include "arkweb/chromium_ext/v8/dfx/heap_dump/dfx-visitor-list.h"

// Count DFX VisitorIds - constexpr version
static constexpr int kDFXVisitorIdCount =
#define DFX_V(id) +1
    0 DFX_DATA_ONLY_VISITOR_ID_LIST(DFX_V)
        DFX_TORQUE_DATA_ONLY_VISITOR_ID_LIST(DFX_V)
            DFX_TYPED_VISITOR_ID_LIST(DFX_V)
                DFX_TYPED_VISITOR_WITH_SLACK_ID_LIST(DFX_V)
                    DFX_TORQUE_VISITOR_ID_LIST(DFX_V)
                        DFX_TRUSTED_VISITOR_ID_LIST(DFX_V);
#undef DFX_V

// Count V8 native VisitorIds - constexpr version
static constexpr int kV8NativeVisitorIdCount =
#define DFX_V(id) +1
    0 DATA_ONLY_VISITOR_ID_LIST(DFX_V) TORQUE_DATA_ONLY_VISITOR_ID_LIST(DFX_V)
        POINTER_VISITOR_ID_LIST(DFX_V) TORQUE_POINTER_VISITOR_ID_LIST(DFX_V)
            TRUSTED_VISITOR_ID_LIST(DFX_V);
#undef DFX_V

// Collect DFX IDs as string array
#define DFX_V(id) #id,
static const char* kDFXVisitorIdNames[] = {
    DFX_DATA_ONLY_VISITOR_ID_LIST(DFX_V)
        DFX_TORQUE_DATA_ONLY_VISITOR_ID_LIST(DFX_V)
            DFX_TYPED_VISITOR_ID_LIST(DFX_V)
                DFX_TYPED_VISITOR_WITH_SLACK_ID_LIST(DFX_V)
                    DFX_TORQUE_VISITOR_ID_LIST(DFX_V)
                        DFX_TRUSTED_VISITOR_ID_LIST(DFX_V)};
#undef DFX_V

// Collect V8 native IDs as string array
#define DFX_V(id) #id,
static const char* kV8NativeVisitorIdNames[] = {
    DATA_ONLY_VISITOR_ID_LIST(DFX_V) TORQUE_DATA_ONLY_VISITOR_ID_LIST(DFX_V)
        POINTER_VISITOR_ID_LIST(DFX_V) TORQUE_POINTER_VISITOR_ID_LIST(DFX_V)
            TRUSTED_VISITOR_ID_LIST(DFX_V)};
#undef DFX_V

// Convert string array to set
inline std::set<std::string> ArrayToSet(const char* arr[], int size) {
  return std::set<std::string>(arr, arr + size);
}

// Check and report differences between DFX and V8 native VisitorIds
inline void CheckVisitorIdCoverage() {
  std::set<std::string> dfx_set =
      ArrayToSet(kDFXVisitorIdNames, kDFXVisitorIdCount);
  std::set<std::string> v8_set =
      ArrayToSet(kV8NativeVisitorIdNames, kV8NativeVisitorIdCount);

  printf("\n========== VisitorId Coverage Report ==========\n");
  printf("DFX VisitorId count: %d\n", kDFXVisitorIdCount);
  printf("V8 Native VisitorId count: %d\n", kV8NativeVisitorIdCount);

  // Find missing (in V8 but not in DFX)
  std::set<std::string> missing;
  std::set_difference(v8_set.begin(), v8_set.end(), dfx_set.begin(),
                      dfx_set.end(), std::inserter(missing, missing.begin()));

  // Find extra (in DFX but not in V8)
  std::set<std::string> extra;
  std::set_difference(dfx_set.begin(), dfx_set.end(), v8_set.begin(),
                      v8_set.end(), std::inserter(extra, extra.begin()));

  if (!missing.empty()) {
    printf("\n--- Missing in DFX (present in V8) ---\n");
    for (const auto& id : missing) {
      printf("  %s\n", id.c_str());
    }
  }

  if (!extra.empty()) {
    printf("\n--- Extra in DFX (not in V8) ---\n");
    for (const auto& id : extra) {
      printf("  %s\n", id.c_str());
    }
  }

  if (missing.empty() && extra.empty()) {
    printf("\n*** DFX VisitorId coverage is COMPLETE! ***\n");
  }

  printf("================================================\n\n");
}

#endif
