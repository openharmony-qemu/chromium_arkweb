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

#include "../include/mock_napi_api.h"
#include <cstdio>

namespace OHOS {
namespace NWeb {
MockNapiFunctions* g_MockNapiFunctions = nullptr;
MockNapiFunctions* GetMockNapiFunctions() {
    return g_MockNapiFunctions;
}

void SetMockNapiFunctions(MockNapiFunctions* mock) {
    g_MockNapiFunctions = mock;
}

extern "C" {
    napi_status napi_create_ark_runtime(napi_env* env) {
        printf("Mock napi_create_ark_runtime called\n");
        if (env == nullptr) {
            return napi_invalid_arg;
        }
        if (g_MockNapiFunctions != nullptr) {
            return g_MockNapiFunctions->napi_create_ark_runtime(env);
        }
        return napi_generic_failure;
    }
    
    napi_status napi_load_module_with_info(napi_env env, const char* path, 
                                           const char* module_info, napi_value* result) {
        if (path == nullptr || result == nullptr) {
            return napi_invalid_arg;
        }
        if (g_MockNapiFunctions != nullptr) {
            return g_MockNapiFunctions->napi_load_module_with_info(env, path, module_info, result);
        }
        return napi_generic_failure;
    }
    
    napi_status napi_create_object(napi_env env, napi_value* result) {
        if (result == nullptr) {
            return napi_invalid_arg;
        }
        if (g_MockNapiFunctions != nullptr) {
            return g_MockNapiFunctions->napi_create_object(env, result);
        }
        return napi_generic_failure;
    }
    
    napi_status napi_get_named_property(napi_env env, napi_value object, 
                                        const char* utf8name, napi_value* result) {
        if (utf8name == nullptr || result == nullptr) {
            return napi_invalid_arg;
        }
        if (g_MockNapiFunctions != nullptr) {
            return g_MockNapiFunctions->napi_get_named_property(env, object, utf8name, result);
        }
        return napi_generic_failure;
    }
    
    napi_status napi_create_uint32(napi_env env, uint32_t value, napi_value* result) {
        if (result == nullptr) {
            return napi_invalid_arg;
        }
        if (g_MockNapiFunctions != nullptr) {
            return g_MockNapiFunctions->napi_create_uint32(env, value, result);
        }
        return napi_generic_failure;
    }
    
    napi_status napi_set_named_property(napi_env env, napi_value object, 
                                        const char* utf8name, napi_value value) {
        if (utf8name == nullptr) {
            return napi_invalid_arg;
        }
        if (g_MockNapiFunctions != nullptr) {
            return g_MockNapiFunctions->napi_set_named_property(env, object, utf8name, value);
        }
        return napi_generic_failure;
    }
    
    napi_status napi_create_string_utf8(napi_env env, const char* str, 
                                        size_t length, napi_value* result) {
        if (str == nullptr || result == nullptr) {
            return napi_invalid_arg;
        }
        if (g_MockNapiFunctions != nullptr) {
            return g_MockNapiFunctions->napi_create_string_utf8(env, str, length, result);
        }
        return napi_generic_failure;
    }
    
    napi_status napi_call_function(napi_env env, napi_value recv, napi_value func, 
                                   size_t argc, const napi_value* argv, napi_value* result) {
        if (g_MockNapiFunctions != nullptr) {
            return g_MockNapiFunctions->napi_call_function(env, recv, func, argc, argv, result);
        }
        return napi_generic_failure;
    }

    napi_status napi_open_handle_scope(napi_env env, napi_handle_scope* result) {
        if (result == nullptr) {
            return napi_invalid_arg;
        }
        if (g_MockNapiFunctions != nullptr) {
            return g_MockNapiFunctions->napi_open_handle_scope(env, result);
        }
        return napi_generic_failure;
    }

    napi_status napi_close_handle_scope(napi_env env, napi_handle_scope scope) {
        if (g_MockNapiFunctions != nullptr) {
            return g_MockNapiFunctions->napi_close_handle_scope(env, scope);
        }
        return napi_generic_failure;
    }

    napi_status napi_create_int32(napi_env env, int32_t value, napi_value* result) {
        if (result == nullptr) {
            return napi_invalid_arg;
        }
        if (g_MockNapiFunctions != nullptr) {
            return g_MockNapiFunctions->napi_create_int32(env, value, result);
        }
        return napi_generic_failure;
    }

    napi_status napi_destroy_ark_runtime(napi_env* env) {
        printf("Mock napi_destroy_ark_runtime called\n");
        if (env == nullptr) {
            return napi_invalid_arg;
        }
        if (g_MockNapiFunctions != nullptr) {
            return g_MockNapiFunctions->napi_destroy_ark_runtime(env);
        }
        return napi_generic_failure;
    }

    napi_status napi_create_double(napi_env env, double value, napi_value* result) {
        if (result == nullptr) {
            return napi_invalid_arg;
        }
        if (g_MockNapiFunctions != nullptr) {
            return g_MockNapiFunctions->napi_create_double(env, value, result);
        }
        return napi_generic_failure;
    }
    napi_status napi_define_properties(napi_env env,
                                       napi_value object,
                                       size_t length,
                                       const napi_property_descriptor* desc) {
      if (env == nullptr) {
        return napi_invalid_arg;
      }
      if (g_MockNapiFunctions != nullptr) {
        return g_MockNapiFunctions->napi_define_properties(env, object, length,
                                                           desc);
      }
      return napi_generic_failure;
    }
    napi_status napi_typeof(napi_env env,
                            napi_value value,
                            napi_valuetype* result) {
      if (env == nullptr) {
        return napi_invalid_arg;
      }
      if (g_MockNapiFunctions != nullptr) {
        return g_MockNapiFunctions->napi_typeof(env, value, result);
      }
      return napi_generic_failure;
    }
    napi_status napi_get_cb_info(napi_env env,
                                 napi_callback_info cbinfo,
                                 size_t* argc,
                                 napi_value* argv,
                                 napi_value* this_arg,
                                 void** data) {
      if (env == nullptr) {
        return napi_invalid_arg;
      }
      if (g_MockNapiFunctions != nullptr) {
        return g_MockNapiFunctions->napi_get_cb_info(env, cbinfo, argc, argv,
                                                     this_arg, data);
      }
      return napi_generic_failure;
    }
    napi_status napi_create_function(napi_env env,
                                     const char* utf8name,
                                     size_t length,
                                     napi_callback cb,
                                     void* data,
                                     napi_value* result) {
      if (env == nullptr) {
        return napi_invalid_arg;
      }
      if (g_MockNapiFunctions != nullptr) {
        return g_MockNapiFunctions->napi_create_function(env, utf8name, length,
                                                         cb, data, result);
      }
      return napi_generic_failure;
    }
    napi_status napi_create_reference(napi_env env,
                                      napi_value value,
                                      uint32_t initial_refcount,
                                      napi_ref* result) {
      if (env == nullptr) {
        return napi_invalid_arg;
      }
      if (g_MockNapiFunctions != nullptr) {
        return g_MockNapiFunctions->napi_create_reference(
            env, value, initial_refcount, result);
      }
      return napi_generic_failure;
    }
    napi_status napi_delete_reference(napi_env env, napi_ref ref) {
      if (env == nullptr) {
        return napi_invalid_arg;
      }
      if (g_MockNapiFunctions != nullptr) {
        return g_MockNapiFunctions->napi_delete_reference(env, ref);
      }
      return napi_generic_failure;
    }
    napi_status napi_get_reference_value(napi_env env,
                                         napi_ref ref,
                                         napi_value* result) {
      if (env == nullptr) {
        return napi_invalid_arg;
      }
      if (g_MockNapiFunctions != nullptr) {
        return g_MockNapiFunctions->napi_get_reference_value(env, ref, result);
      }
      return napi_generic_failure;
    }
    napi_status napi_wrap(napi_env env,
                          napi_value js_object,
                          void* native_object,
                          napi_finalize finalize_cb,
                          void* finalize_hint,
                          napi_ref* result) {
      if (env == nullptr) {
        return napi_invalid_arg;
      }
      if (g_MockNapiFunctions != nullptr) {
        return g_MockNapiFunctions->napi_wrap(
            env, js_object, native_object, finalize_cb, finalize_hint, result);
      }
      return napi_generic_failure;
    }
    napi_status napi_is_array(napi_env env, napi_value value, bool* result) {
      if (env == nullptr) {
        return napi_invalid_arg;
      }
      if (g_MockNapiFunctions != nullptr) {
        return g_MockNapiFunctions->napi_is_array(env, value, result);
      }
      return napi_generic_failure;
    }
    napi_status napi_get_array_length(napi_env env,
                                      napi_value value,
                                      uint32_t* result) {
      if (env == nullptr) {
        return napi_invalid_arg;
      }
      if (g_MockNapiFunctions != nullptr) {
        return g_MockNapiFunctions->napi_get_array_length(env, value, result);
      }
      return napi_generic_failure;
    }
    napi_status napi_get_element(napi_env env,
                                 napi_value object,
                                 uint32_t index,
                                 napi_value* result) {
      if (env == nullptr) {
        return napi_invalid_arg;
      }
      if (g_MockNapiFunctions != nullptr) {
        return g_MockNapiFunctions->napi_get_element(env, object, index,
                                                     result);
      }
      return napi_generic_failure;
    }
    napi_status napi_get_value_int32(napi_env env,
                                     napi_value value,
                                     int32_t* result) {
      if (env == nullptr) {
        return napi_invalid_arg;
      }
      if (g_MockNapiFunctions != nullptr) {
        return g_MockNapiFunctions->napi_get_value_int32(env, value, result);
      }
      return napi_generic_failure;
    }
    napi_status napi_get_value_string_utf8(napi_env env,
                                           napi_value value,
                                           char* buf,
                                           size_t bufsize,
                                           size_t* result) {
      if (env == nullptr) {
        return napi_invalid_arg;
      }
      if (g_MockNapiFunctions != nullptr) {
        return g_MockNapiFunctions->napi_get_value_string_utf8(env, value, buf,
                                                               bufsize, result);
      }
      return napi_generic_failure;
    }
    napi_status napi_run_event_loop(napi_env env, napi_event_mode mode) {
      if (env == nullptr) {
        return napi_invalid_arg;
      }
      if (g_MockNapiFunctions != nullptr) {
        return g_MockNapiFunctions->napi_run_event_loop(env, mode);
      }
      return napi_generic_failure;
    }
}
}   // namespace NWeb
}   // namespace OHOS