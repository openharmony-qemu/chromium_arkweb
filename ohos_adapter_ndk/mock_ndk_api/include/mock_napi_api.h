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

#ifndef MOCK_NAPI_ADPI_H
#define MOCK_NAPI_ADPI_H

#include <gmock/gmock.h>
#include "napi/native_api.h"
#include "js_native_api.h"

namespace OHOS {
namespace NWeb {
class MockNapiFunctions {
public:
    virtual ~MockNapiFunctions() = default;
    
    MOCK_METHOD(napi_status, napi_create_ark_runtime, (napi_env* env));
    MOCK_METHOD(napi_status, napi_load_module_with_info, (napi_env env, const char* path,
                                                        const char* module_info, napi_value* result));
    MOCK_METHOD(napi_status, napi_create_object, (napi_env env, napi_value* result));
    MOCK_METHOD(napi_status, napi_get_named_property, (napi_env env, napi_value object, 
                                                    const char* utf8name, napi_value* result));
    MOCK_METHOD(napi_status, napi_create_uint32, (napi_env env, uint32_t value, napi_value* result));
    MOCK_METHOD(napi_status, napi_set_named_property, (napi_env env, napi_value object, 
                                                    const char* utf8name, napi_value value));
    MOCK_METHOD(napi_status, napi_create_string_utf8, (napi_env env, const char* str, 
                                                    size_t length, napi_value* result));
    MOCK_METHOD(napi_status, napi_call_function, (napi_env env, napi_value recv, napi_value func,
                                                size_t argc, const napi_value* argv, napi_value* result));
    MOCK_METHOD(napi_status, napi_open_handle_scope, (napi_env env, napi_handle_scope* result));
    MOCK_METHOD(napi_status, napi_close_handle_scope, (napi_env env, napi_handle_scope scope));
    MOCK_METHOD(napi_status, napi_create_int32, (napi_env env, int32_t value, napi_value* result));
    MOCK_METHOD(napi_status, napi_destroy_ark_runtime, (napi_env* env));
    MOCK_METHOD(napi_status, napi_create_double, (napi_env env, double value, napi_value* result));
    MOCK_METHOD(
        napi_status, napi_define_properties,
        (napi_env, napi_value, size_t, const napi_property_descriptor*));
    MOCK_METHOD(napi_status, napi_typeof, (napi_env, napi_value, napi_valuetype*));
    MOCK_METHOD(napi_status, napi_get_cb_info, 
                (napi_env, napi_callback_info, size_t*, napi_value*, napi_value*, void**));
    MOCK_METHOD(napi_status, napi_create_function, 
                (napi_env, const char*, size_t, napi_callback, void*, napi_value*));
    MOCK_METHOD(napi_status, napi_create_reference, (napi_env, napi_value, uint32_t, napi_ref*));
    MOCK_METHOD(napi_status, napi_delete_reference, (napi_env, napi_ref));
    MOCK_METHOD(napi_status, napi_get_reference_value, (napi_env, napi_ref, napi_value*));
    MOCK_METHOD(napi_status, napi_wrap, 
                (napi_env, napi_value, void*, napi_finalize, void*, napi_ref*));
    MOCK_METHOD(napi_status, napi_is_array, (napi_env, napi_value, bool*));
    MOCK_METHOD(napi_status, napi_get_array_length, (napi_env, napi_value, uint32_t*));
    MOCK_METHOD(napi_status, napi_get_element, (napi_env, napi_value, uint32_t, napi_value*));
    MOCK_METHOD(napi_status, napi_get_value_int32, (napi_env, napi_value, int32_t*));
    MOCK_METHOD(napi_status, napi_get_value_string_utf8, 
                (napi_env, napi_value, char*, size_t, size_t*));
    MOCK_METHOD(napi_status, napi_run_event_loop, (napi_env, napi_event_mode));
};

extern MockNapiFunctions* g_MockNapiFunctions;
extern MockNapiFunctions* GetMockNapiFunctions();
extern void SetMockNapiFunctions(MockNapiFunctions* mock);

extern "C" {
    napi_status napi_create_ark_runtime(napi_env* env);
    napi_status napi_load_module_with_info(napi_env env, const char* path, 
 	                                       const char* module_info, napi_value* result);
    napi_status napi_create_object(napi_env env, napi_value* result);
    napi_status napi_get_named_property(napi_env env, napi_value object, 
 	                                    const char* utf8name, napi_value* result);
    napi_status napi_create_uint32(napi_env env, uint32_t value, napi_value* result);
    napi_status napi_set_named_property(napi_env env, napi_value object, 
 	                                    const char* utf8name, napi_value value);
    napi_status napi_create_string_utf8(napi_env env, const char* str, 
 	                                    size_t length, napi_value* result);
    napi_status napi_call_function(napi_env env, napi_value recv, napi_value func, 
 	                               size_t argc, const napi_value* argv, napi_value* result);
    napi_status napi_open_handle_scope(napi_env env, napi_handle_scope* result);
    napi_status napi_close_handle_scope(napi_env env, napi_handle_scope scope);
    napi_status napi_create_int32(napi_env env, int32_t value, napi_value* result);
    napi_status napi_destroy_ark_runtime(napi_env* env);
    napi_status napi_create_double(napi_env env, double value, napi_value* result);
    napi_status napi_define_properties(napi_env env,
                                       napi_value object,
                                       size_t length,
                                       const napi_property_descriptor* desc);
    napi_status napi_typeof(napi_env env,
                            napi_value value,
                            napi_valuetype* result);
    napi_status napi_get_cb_info(napi_env env,
                                 napi_callback_info cbinfo,
                                 size_t* argc,
                                 napi_value* argv,
                                 napi_value* this_arg,
                                 void** data);
    napi_status napi_create_function(napi_env env,
                                     const char* utf8name,
                                     size_t length,
                                     napi_callback cb,
                                     void* data,
                                     napi_value* result);
    napi_status napi_create_reference(napi_env env,
                                      napi_value value,
                                      uint32_t initial_refcount,
                                      napi_ref* result);
    napi_status napi_delete_reference(napi_env env, napi_ref ref);
    napi_status napi_get_reference_value(napi_env env,
                                         napi_ref ref,
                                         napi_value* result);
    napi_status napi_wrap(napi_env env,
                          napi_value js_object,
                          void* native_object,
                          napi_finalize finalize_cb,
                          void* finalize_hint,
                          napi_ref* result);
    napi_status napi_is_array(napi_env env, napi_value value, bool* result);
    napi_status napi_get_array_length(napi_env env,
                                      napi_value value,
                                      uint32_t* result);
    napi_status napi_get_element(napi_env env,
                                 napi_value object,
                                 uint32_t index,
                                 napi_value* result);
    napi_status napi_get_value_int32(napi_env env,
                                     napi_value value,
                                     int32_t* result);
    napi_status napi_get_value_string_utf8(napi_env env,
                                           napi_value value,
                                           char* buf,
                                           size_t bufsize,
                                           size_t* result);
    napi_status napi_run_event_loop(napi_env env, napi_event_mode mode);
    }
}   // namespace NWeb
}   // namespace OHOS

#endif  // MOCK_NAPI_ADPI_H