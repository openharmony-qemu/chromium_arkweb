// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_NWEB_SRC_CAPI_COMMON_COMMON_H_
#define OHOS_NWEB_SRC_CAPI_COMMON_COMMON_H_

#include <type_traits>

template <class T, typename MethodPointer>
bool CheckValid(T* obj, MethodPointer* pointer) {
  if (!obj || !pointer) {
    return false;
  }
  typename T::Base* base = static_cast<typename T::Base*>(obj);
  uintptr_t start_pos = reinterpret_cast<intptr_t>(base);
  uintptr_t method_pos = reinterpret_cast<intptr_t>(pointer);
  if (start_pos + obj->struct_size < method_pos + sizeof(MethodPointer)) {
    return false;
  }
  if (!(*pointer)) {
    return false;
  }
  return true;
}

// Non-void return: caller supplies a default value used when the check fails.
template <class T, typename MethodPointer, typename Default, typename... Args,
          typename R = std::invoke_result_t<MethodPointer, T*, Args...>,
          std::enable_if_t<!std::is_void_v<R>, int> = 0>
R CheckAndCall(T* obj, MethodPointer* slot, Default&& default_value, Args&&... args) {
  if (!CheckValid(obj, slot)) {
    return static_cast<R>(std::forward<Default>(default_value));
  }
  return (obj->*(*slot))(std::forward<Args>(args)...);
}

// Void return: no default value needed.
template <class T, typename MethodPointer, typename... Args,
          typename R = std::invoke_result_t<MethodPointer, T*, Args...>,
          std::enable_if_t<std::is_void_v<R>, int> = 0>
void CheckAndCall(T* obj, MethodPointer* slot, Args&&... args) {
  if (!CheckValid(obj, slot)) {
    return;
  }
  (obj->*(*slot))(std::forward<Args>(args)...);
}

#endif // OHOS_NWEB_SRC_CAPI_COMMON_COMMON_H_
