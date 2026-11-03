/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Create by lin on 9/3/2024.
 */

#ifndef OH_WEB_PERFORMANCE_TIMING_H_
#define OH_WEB_PERFORMANCE_TIMING_H_

#include <cstdint>

#include "net/base/network_change_notifier.h"
#include "net/nqe/effective_connection_type.h"

struct OhWebPerformanceTiming {
  OhWebPerformanceTiming() = default;
  void Reset();

  int64_t navigation_id = -1;
  int64_t navigation_start = -1;
  // Updated on every navigation including reloads and renderer-initiated
  // navigations. Unlike navigation_start (which is read from a static variable
  // only updated in LoadURLWithParams), this field always reflects the
  // actual start time of the current navigation.
  int64_t actual_navigation_start = -1;
  uint32_t redirect_count = 0;
  int64_t redirect_start = -1;
  int64_t redirect_end = -1;
  int64_t fetch_start = -1;
  int64_t worker_start = -1;
  int64_t domain_lookup_start = -1;
  int64_t domain_lookup_end = -1;
  int64_t connect_start = -1;
  int64_t secure_connect_start = -1;
  int64_t connect_end = -1;
  int64_t request_start = -1;
  int64_t response_start = -1;
  int64_t response_end = -1;
  int64_t dom_interactive = -1;
  int64_t dom_content_loaded_event_start = -1;
  int64_t dom_content_loaded_event_end = -1;
  int64_t load_event_start = -1;
  int64_t load_event_end = -1;
  int64_t first_paint = -1;
  int64_t first_contentful_paint = -1;
  int64_t largest_contentful_paint = -1;
  int64_t render_init_block = -1;
  int64_t input_time = -1;
  bool is_paint_done = false;
  int64_t first_meaningful_paint = -1;
  int64_t first_input_delay = -1;
  int64_t total_web_contents = -1;
  bool is_ssl_cert_error = false;
  net::NetworkChangeNotifier::ConnectionType current_connection_type =
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN;
  net::EffectiveConnectionType effective_connection_type =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
};

#endif  // OH_WEB_PERFORMANCE_TIMING_H_
