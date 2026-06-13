/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef NWEB_ERRORS_H
#define NWEB_ERRORS_H

namespace OHOS::NWeb {
enum NWebErrNo {
  NWEB_ERR = -1,
  NWEB_OK = 0,

  NWEB_INVALID_URL = 17100002,
  NWEB_INVALID_RESOURCE = 17100003,
  NWEB_FUNCTION_NOT_ENABLE = 17100004,
  NWEB_INVALID_COOKIE_VALUE = 17100005,
  NWEB_INVALID_ORIGIN = 17100011,
  NWEB_TYPE_NOT_MATCH_WITCH_VALUE = 17100014,

};

enum NWebNavigationEntryUpdateResult {
  UPDATE_OK = 0,
  ERR_WRONG_OFFSET = -1,
  ERR_OTHER = -2,
};

// Error codes for DOM command action.
// Values 10-132 are aligned with WebCommandResult in ace_engine.
// Values 160-199 are common Blink/NWeb execution errors.
// Values 200-249 are input command errors.
// Values 250-299 are select command errors.
enum class WebCommandErrorCode {
  // success
  SUCCESS = 10,
  // environment error
  FAILED = 11,
  DELEGATE_NULL = 102,
  CONTEXT_NULL = 103,
  TASK_EXECUTOR_NULL = 104,
  // json error
  JSON_IS_INVALID = 110,
  JSON_MISSING_EVENT_TYPE = 111,
  JSON_INVALID_EVENT_TYPE = 112,
  JSON_VALUE_ERROR_EVENT_TYPE = 113,
  JSON_MISSING_XPATH = 114,
  JSON_INVALID_XPATH = 115,
  JSON_MISSING_DURATION = 116,
  JSON_INVALID_DURATION = 117,
  JSON_MISSING_ALIGN = 118,
  JSON_INVALID_ALIGN = 119,
  JSON_VALUE_ERROR_ALIGN = 120,
  JSON_MISSING_OFFSET = 121,
  JSON_INVALID_OFFSET = 122,
  // runtime error
  WEB_EXECUTE_TIMEOUT = 130,
  ELEMENT_NOT_FOUND = 131,
  WEB_NWEB_NULL = 132,
  // common execution error (160-199)
  PAGE_NOT_READY = 160,
  ELEMENT_TYPE_MISMATCH = 161,
  // input command error (200-249)
  JSON_INVALID_INPUT_XPATH = 200,
  JSON_INVALID_INPUT_VALUE = 201,
  INPUT_TYPE_INVALID = 202,
  INPUT_VALUE_FORMAT_INVALID = 203,
  INPUT_EVENT_TYPE_MISMATCH = 204,
  // select command error (250-299)
  JSON_INVALID_SELECT_XPATH = 250,
  JSON_INVALID_SELECT_OPTIONS = 251,
  SELECT_INDEX_OUT_OF_RANGE = 252,
  SELECT_VALUE_NOT_FOUND = 253,
  SELECT_NOT_MULTIPLE = 254,
  SELECT_OPTION_DISABLED = 255,
  SELECT_EMPTY_OPTIONS = 256,
  // gesture command error (300-349)
  JSON_INVALID_GESTURE_X = 300,
  JSON_INVALID_GESTURE_Y = 301,
  JSON_INVALID_GESTURE_DISTANCE = 302,
  JSON_INVALID_GESTURE_SCALE = 303,
  JSON_INVALID_GESTURE_DURATION = 304,
  JSON_INVALID_GESTURE_TAP_COUNT = 305,
  JSON_INVALID_GESTURE_SPEED = 306,
  JSON_INVALID_GESTURE_COORDINATES = 307,
  // Input method command error (400-479)
  // Common error (400-419)
  INPUT_METHOD_NOT_FOUND = 400,
  INPUT_METHOD_UNKNOWN = 401,
  INPUT_METHOD_NOT_BOUND = 402,
  // Click/Focus command error (420-439)
  INPUT_CLICK_PARAMS_MISSING = 420,
  INPUT_CLICK_NODEID_MISSING = 421,
  INPUT_CLICK_NODEID_EMPTY = 422,
  INPUT_CLICK_DELEGATE_NULL = 423,
  INPUT_CLICK_ELEMENT_NOT_FOUND = 424,
  // Type command error (440-459)
  INPUT_TYPE_PARAMS_MISSING = 440,
  INPUT_TYPE_NODEID_MISSING = 441,
  INPUT_TYPE_TEXT_MISSING = 442,
  INPUT_TYPE_XPATH_EMPTY = 443,
  INPUT_TYPE_DELEGATE_NULL = 444,
  INPUT_TYPE_ELEMENT_NOT_FOUND = 445,
  // SendKeys command error (460-479)
  INPUT_SEND_KEYS_PARAMS_MISSING = 460,
  INPUT_SEND_KEYS_KEY_MISSING = 461,
  INPUT_SEND_KEYS_NO_VALID_KEYS = 462,
  INPUT_SEND_KEYS_UNKNOWN_KEY = 463,
};

}  // namespace OHOS::NWeb

#endif  // NWEB_ERRORS_H
