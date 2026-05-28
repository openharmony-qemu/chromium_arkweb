#include "arkweb/chromium_ext/v8/dfx/heap_dump/tests/cctest/heap-dump-test-utils.h"

#include "include/v8-primitive-object.h"
#include "include/v8-primitive.h"
#include "src/objects/api-callbacks-inl.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/descriptor-array-inl.h"
#include "src/objects/feedback-vector-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/js-raw-json.h"
#include "src/objects/js-promise-inl.h"
#include "src/objects/js-proxy-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-regexp-string-iterator-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/name-inl.h"
#include "src/objects/oddball-inl.h"
#include "src/objects/property.h"
#include "src/objects/property-array-inl.h"
#include "src/objects/script-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/scope-info.h"
#include "src/objects/struct-inl.h"
#include "test/common/flag-utils.h"

namespace v8 {
namespace internal {
namespace heap {

#if defined(OH_ENABLE_HEAP_DUMP_TEST) && defined(OH_ENABLE_HEAP_DUMP) && \
    defined(OH_ENABLE_HEAP_TRANSLATE)

using namespace heap_dump_test;

void CheckExactTemporalInternalMapEdgeNoWrongKind(SnapshotGenerator* generator,
                                                  const Node* object,
                                                  const Node* map) {
  CHECK(NodeHasType(map, Node::Type::kObjectShape));
  CHECK(HasExactNamedEdge(generator, object, Edge::Type::kInternal, "map", map));
  CHECK_EQ(1, CountExactNamedEdges(generator, object, Edge::Type::kInternal,
                                   "map", map));
  CHECK_EQ(1, CountEdgesFromNodeToTarget(object, Edge::Type::kInternal, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kWeak, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kHidden, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kProperty, map));
  CHECK_EQ(0, CountEdgesFromNodeToTarget(object, Edge::Type::kShortcut, map));
}

UNINITIALIZED_TEST(HeapDumpJSTemporalDurationExactPrototypeEdge) {
  FlagScope<bool> harmony_temporal(&v8_flags.harmony_temporal, true);

  Address duration_address = 0;
  Address duration_map_address = 0;
  Address prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const duration =
                    new Temporal.Duration(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
                const prototype = Object.getPrototypeOf(duration);
                if (prototype !== Temporal.Duration.prototype ||
                    !(duration instanceof Temporal.Duration) ||
                    duration.years !== 1 ||
                    duration.months !== 2 ||
                    duration.weeks !== 3 ||
                    duration.days !== 4 ||
                    duration.hours !== 5 ||
                    duration.minutes !== 6 ||
                    duration.seconds !== 7 ||
                    duration.milliseconds !== 8 ||
                    duration.microseconds !== 9 ||
                    duration.nanoseconds !== 10 ||
                    duration.toString() !==
                        'P1Y2M3W4DT5H6M7.00800901S') {
                  throw new Error(
                      'unexpected Temporal.Duration fixture');
                }
                globalThis
                    .__heap_dump_js_temporal_duration_exact_prototype_edge = {
                  duration, prototype
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_temporal_duration_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> duration_value = get_property("duration");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK(duration_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK_EQ(JS_TEMPORAL_DURATION_TYPE,
                     LocalObjectInstanceType(duration_value));

            v8::Local<v8::Object> duration =
                v8::Local<v8::Object>::Cast(duration_value);
            CHECK(duration->GetPrototypeV2()->StrictEquals(prototype_value));

            auto check_int_property = [&](const char* name, int expected) {
              v8::Local<v8::Value> field_value =
                  duration->Get(context, NewApiString(v8_isolate, name))
                      .ToLocalChecked();
              CHECK(field_value->IsNumber());
              CHECK_EQ(expected, field_value->Int32Value(context).FromJust());
            };
            check_int_property("years", 1);
            check_int_property("months", 2);
            check_int_property("weeks", 3);
            check_int_property("days", 4);
            check_int_property("hours", 5);
            check_int_property("minutes", 6);
            check_int_property("seconds", 7);
            check_int_property("milliseconds", 8);
            check_int_property("microseconds", 9);
            check_int_property("nanoseconds", 10);

            DirectHandle<JSObject> js_duration =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*duration));
            CHECK_EQ(JS_TEMPORAL_DURATION_TYPE,
                     js_duration->map()->instance_type());
            duration_map_address = js_duration->map()->address();
            duration_address = js_duration->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* duration = GetSnapshotNode(generator.get(), duration_address);
  Node* duration_map = GetSnapshotNode(generator.get(), duration_map_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  CHECK_NOT_NULL(duration);
  CHECK_NOT_NULL(duration_map);
  CHECK_NOT_NULL(prototype);
  CHECK(NodeHasType(duration, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CheckExactTemporalInternalMapEdgeNoWrongKind(generator.get(), duration,
                                               duration_map);

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), duration, "__proto__", prototype);
  MarkCoveredInstanceType(JS_TEMPORAL_DURATION_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSTemporalInstantExactPrototypeEdge) {
  FlagScope<bool> harmony_temporal(&v8_flags.harmony_temporal, true);

  Address instant_address = 0;
  Address instant_map_address = 0;
  Address prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const instant =
                    Temporal.Instant.fromEpochMilliseconds(1234);
                const prototype = Object.getPrototypeOf(instant);
                if (prototype !== Temporal.Instant.prototype ||
                    !(instant instanceof Temporal.Instant) ||
                    instant.epochMilliseconds !== 1234 ||
                    instant.epochNanoseconds !== 1234000000n ||
                    instant.toString() !== '1970-01-01T00:00:01.234Z') {
                  throw new Error('unexpected Temporal.Instant fixture');
                }
                globalThis
                    .__heap_dump_js_temporal_instant_exact_prototype_edge = {
                  instant, prototype
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_temporal_instant_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> instant_value = get_property("instant");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK(instant_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK_EQ(JS_TEMPORAL_INSTANT_TYPE,
                     LocalObjectInstanceType(instant_value));

            v8::Local<v8::Object> instant =
                v8::Local<v8::Object>::Cast(instant_value);
            CHECK(instant->GetPrototypeV2()->StrictEquals(prototype_value));

            v8::Local<v8::Value> epoch_milliseconds =
                instant->Get(context,
                             NewApiString(v8_isolate, "epochMilliseconds"))
                    .ToLocalChecked();
            CHECK(epoch_milliseconds->IsNumber());
            CHECK_EQ(1234, epoch_milliseconds->Int32Value(context).FromJust());

            DirectHandle<JSObject> js_instant =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*instant));
            CHECK_EQ(JS_TEMPORAL_INSTANT_TYPE,
                     js_instant->map()->instance_type());
            instant_map_address = js_instant->map()->address();
            instant_address = js_instant->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* instant = GetSnapshotNode(generator.get(), instant_address);
  Node* instant_map = GetSnapshotNode(generator.get(), instant_map_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  CHECK_NOT_NULL(instant);
  CHECK_NOT_NULL(instant_map);
  CHECK_NOT_NULL(prototype);
  CHECK(NodeHasType(instant, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CheckExactTemporalInternalMapEdgeNoWrongKind(generator.get(), instant,
                                               instant_map);

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), instant, "__proto__", prototype);
  MarkCoveredInstanceType(JS_TEMPORAL_INSTANT_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSTemporalPlainDateExactPrototypeEdge) {
  FlagScope<bool> harmony_temporal(&v8_flags.harmony_temporal, true);

  Address plain_date_address = 0;
  Address plain_date_map_address = 0;
  Address prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const plainDate = new Temporal.PlainDate(2020, 5, 6);
                const prototype = Object.getPrototypeOf(plainDate);
                if (prototype !== Temporal.PlainDate.prototype ||
                    !(plainDate instanceof Temporal.PlainDate) ||
                    plainDate.year !== 2020 ||
                    plainDate.month !== 5 ||
                    plainDate.day !== 6 ||
                    plainDate.calendarId !== 'iso8601' ||
                    plainDate.toString() !== '2020-05-06') {
                  throw new Error(
                      'unexpected Temporal.PlainDate fixture');
                }
                globalThis
                    .__heap_dump_js_temporal_plain_date_exact_prototype_edge = {
                  plainDate, prototype
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_temporal_plain_date_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> plain_date_value =
                get_property("plainDate");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK(plain_date_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK_EQ(JS_TEMPORAL_PLAIN_DATE_TYPE,
                     LocalObjectInstanceType(plain_date_value));

            v8::Local<v8::Object> plain_date =
                v8::Local<v8::Object>::Cast(plain_date_value);
            CHECK(plain_date->GetPrototypeV2()->StrictEquals(
                prototype_value));

            auto check_int_property = [&](const char* name, int expected) {
              v8::Local<v8::Value> field_value =
                  plain_date->Get(context, NewApiString(v8_isolate, name))
                      .ToLocalChecked();
              CHECK(field_value->IsNumber());
              CHECK_EQ(expected, field_value->Int32Value(context).FromJust());
            };
            check_int_property("year", 2020);
            check_int_property("month", 5);
            check_int_property("day", 6);
            v8::Local<v8::Value> calendar_id =
                plain_date
                    ->Get(context,
                          NewApiString(v8_isolate, "calendarId"))
                    .ToLocalChecked();
            CHECK(calendar_id->Equals(context,
                                      NewApiString(v8_isolate, "iso8601"))
                      .FromJust());

            DirectHandle<JSObject> js_plain_date =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*plain_date));
            CHECK_EQ(JS_TEMPORAL_PLAIN_DATE_TYPE,
                     js_plain_date->map()->instance_type());
            plain_date_map_address = js_plain_date->map()->address();
            plain_date_address = js_plain_date->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* plain_date = GetSnapshotNode(generator.get(), plain_date_address);
  Node* plain_date_map = GetSnapshotNode(generator.get(),
                                         plain_date_map_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  CHECK_NOT_NULL(plain_date);
  CHECK_NOT_NULL(plain_date_map);
  CHECK_NOT_NULL(prototype);
  CHECK(NodeHasType(plain_date, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CheckExactTemporalInternalMapEdgeNoWrongKind(generator.get(), plain_date,
                                               plain_date_map);

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), plain_date, "__proto__", prototype);
  MarkCoveredInstanceType(JS_TEMPORAL_PLAIN_DATE_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSTemporalPlainMonthDayExactPrototypeEdge) {
  FlagScope<bool> harmony_temporal(&v8_flags.harmony_temporal, true);

  Address plain_month_day_address = 0;
  Address plain_month_day_map_address = 0;
  Address prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const plainMonthDay = new Temporal.PlainMonthDay(5, 6);
                const prototype = Object.getPrototypeOf(plainMonthDay);
                if (prototype !== Temporal.PlainMonthDay.prototype ||
                    !(plainMonthDay instanceof Temporal.PlainMonthDay) ||
                    plainMonthDay.monthCode !== 'M05' ||
                    plainMonthDay.day !== 6 ||
                    plainMonthDay.calendarId !== 'iso8601' ||
                    plainMonthDay.toString() !== '05-06' ||
                    ('month' in plainMonthDay) ||
                    plainMonthDay.month !== undefined) {
                  throw new Error(
                      'unexpected Temporal.PlainMonthDay fixture');
                }
                globalThis
                    .__heap_dump_js_temporal_plain_month_day_exact_prototype_edge = {
                  plainMonthDay, prototype
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_temporal_plain_month_day_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> plain_month_day_value =
                get_property("plainMonthDay");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK(plain_month_day_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK_EQ(JS_TEMPORAL_PLAIN_MONTH_DAY_TYPE,
                     LocalObjectInstanceType(plain_month_day_value));

            v8::Local<v8::Object> plain_month_day =
                v8::Local<v8::Object>::Cast(plain_month_day_value);
            CHECK(plain_month_day->GetPrototypeV2()->StrictEquals(
                prototype_value));

            v8::Local<v8::Value> month_code =
                plain_month_day
                    ->Get(context,
                          NewApiString(v8_isolate, "monthCode"))
                    .ToLocalChecked();
            CHECK(month_code->Equals(context,
                                     NewApiString(v8_isolate, "M05"))
                      .FromJust());
            v8::Local<v8::Value> day =
                plain_month_day
                    ->Get(context, NewApiString(v8_isolate, "day"))
                    .ToLocalChecked();
            CHECK(day->IsNumber());
            CHECK_EQ(6, day->Int32Value(context).FromJust());
            v8::Local<v8::Value> calendar_id =
                plain_month_day
                    ->Get(context,
                          NewApiString(v8_isolate, "calendarId"))
                    .ToLocalChecked();
            CHECK(calendar_id->Equals(context,
                                      NewApiString(v8_isolate, "iso8601"))
                      .FromJust());
            v8::Local<v8::Value> to_string_value =
                plain_month_day
                    ->Get(context, NewApiString(v8_isolate, "toString"))
                    .ToLocalChecked();
            CHECK(to_string_value->IsFunction());
            v8::Local<v8::Function> to_string =
                v8::Local<v8::Function>::Cast(to_string_value);
            v8::Local<v8::Value> to_string_result =
                to_string->Call(context, plain_month_day, 0, nullptr)
                    .ToLocalChecked();
            CHECK(to_string_result
                      ->Equals(context,
                               NewApiString(v8_isolate, "05-06"))
                      .FromJust());

            DirectHandle<JSObject> js_plain_month_day =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(
                    *plain_month_day));
            CHECK_EQ(JS_TEMPORAL_PLAIN_MONTH_DAY_TYPE,
                     js_plain_month_day->map()->instance_type());
            plain_month_day_map_address = js_plain_month_day->map()->address();
            plain_month_day_address = js_plain_month_day->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* plain_month_day =
      GetSnapshotNode(generator.get(), plain_month_day_address);
  Node* plain_month_day_map =
      GetSnapshotNode(generator.get(), plain_month_day_map_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  CHECK_NOT_NULL(plain_month_day);
  CHECK_NOT_NULL(plain_month_day_map);
  CHECK_NOT_NULL(prototype);
  CHECK(NodeHasType(plain_month_day, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CheckExactTemporalInternalMapEdgeNoWrongKind(generator.get(), plain_month_day,
                                              plain_month_day_map);

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), plain_month_day, "__proto__", prototype);
  MarkCoveredInstanceType(JS_TEMPORAL_PLAIN_MONTH_DAY_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSTemporalPlainYearMonthExactPrototypeEdge) {
  FlagScope<bool> harmony_temporal(&v8_flags.harmony_temporal, true);

  Address plain_year_month_address = 0;
  Address plain_year_month_map_address = 0;
  Address prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const plainYearMonth = new Temporal.PlainYearMonth(2024, 5);
                const prototype = Object.getPrototypeOf(plainYearMonth);
                if (prototype !== Temporal.PlainYearMonth.prototype ||
                    !(plainYearMonth instanceof Temporal.PlainYearMonth) ||
                    plainYearMonth.year !== 2024 ||
                    plainYearMonth.month !== 5 ||
                    plainYearMonth.monthCode !== 'M05' ||
                    plainYearMonth.calendarId !== 'iso8601' ||
                    plainYearMonth.toString() !== '2024-05') {
                  throw new Error(
                      'unexpected Temporal.PlainYearMonth fixture');
                }
                globalThis
                    .__heap_dump_js_temporal_plain_year_month_exact_prototype_edge = {
                  plainYearMonth, prototype
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_temporal_plain_year_month_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> plain_year_month_value =
                get_property("plainYearMonth");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK(plain_year_month_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK_EQ(JS_TEMPORAL_PLAIN_YEAR_MONTH_TYPE,
                     LocalObjectInstanceType(plain_year_month_value));

            v8::Local<v8::Object> plain_year_month =
                v8::Local<v8::Object>::Cast(plain_year_month_value);
            CHECK(plain_year_month->GetPrototypeV2()->StrictEquals(
                prototype_value));

            auto check_int_property = [&](const char* name, int expected) {
              v8::Local<v8::Value> field_value =
                  plain_year_month
                      ->Get(context, NewApiString(v8_isolate, name))
                      .ToLocalChecked();
              CHECK(field_value->IsNumber());
              CHECK_EQ(expected, field_value->Int32Value(context).FromJust());
            };
            check_int_property("year", 2024);
            check_int_property("month", 5);
            v8::Local<v8::Value> month_code =
                plain_year_month
                    ->Get(context,
                          NewApiString(v8_isolate, "monthCode"))
                    .ToLocalChecked();
            CHECK(month_code->Equals(context,
                                     NewApiString(v8_isolate, "M05"))
                      .FromJust());
            v8::Local<v8::Value> calendar_id =
                plain_year_month
                    ->Get(context,
                          NewApiString(v8_isolate, "calendarId"))
                    .ToLocalChecked();
            CHECK(calendar_id->Equals(context,
                                      NewApiString(v8_isolate, "iso8601"))
                      .FromJust());
            v8::Local<v8::Value> to_string_value =
                plain_year_month
                    ->Get(context, NewApiString(v8_isolate, "toString"))
                    .ToLocalChecked();
            CHECK(to_string_value->IsFunction());
            v8::Local<v8::Function> to_string =
                v8::Local<v8::Function>::Cast(to_string_value);
            v8::Local<v8::Value> to_string_result =
                to_string->Call(context, plain_year_month, 0, nullptr)
                    .ToLocalChecked();
            CHECK(to_string_result
                      ->Equals(context,
                               NewApiString(v8_isolate, "2024-05"))
                      .FromJust());

            DirectHandle<JSObject> js_plain_year_month =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(
                    *plain_year_month));
            CHECK_EQ(JS_TEMPORAL_PLAIN_YEAR_MONTH_TYPE,
                     js_plain_year_month->map()->instance_type());
            plain_year_month_map_address = js_plain_year_month->map()->address();
            plain_year_month_address = js_plain_year_month->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* plain_year_month =
      GetSnapshotNode(generator.get(), plain_year_month_address);
  Node* plain_year_month_map =
      GetSnapshotNode(generator.get(), plain_year_month_map_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  CHECK_NOT_NULL(plain_year_month);
  CHECK_NOT_NULL(plain_year_month_map);
  CHECK_NOT_NULL(prototype);
  CHECK(NodeHasType(plain_year_month, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CheckExactTemporalInternalMapEdgeNoWrongKind(generator.get(), plain_year_month,
                                              plain_year_month_map);

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), plain_year_month, "__proto__", prototype);
  MarkCoveredInstanceType(JS_TEMPORAL_PLAIN_YEAR_MONTH_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSTemporalPlainDateTimeExactPrototypeEdge) {
  FlagScope<bool> harmony_temporal(&v8_flags.harmony_temporal, true);

  Address plain_date_time_address = 0;
  Address plain_date_time_map_address = 0;
  Address prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const plainDateTime = new Temporal.PlainDateTime(
                    2024, 5, 6, 7, 8, 9, 123, 456, 789);
                const prototype = Object.getPrototypeOf(plainDateTime);
                if (prototype !== Temporal.PlainDateTime.prototype ||
                    !(plainDateTime instanceof Temporal.PlainDateTime) ||
                    plainDateTime.year !== 2024 ||
                    plainDateTime.month !== 5 ||
                    plainDateTime.day !== 6 ||
                    plainDateTime.hour !== 7 ||
                    plainDateTime.minute !== 8 ||
                    plainDateTime.second !== 9 ||
                    plainDateTime.millisecond !== 123 ||
                    plainDateTime.microsecond !== 456 ||
                    plainDateTime.nanosecond !== 789 ||
                    plainDateTime.calendarId !== 'iso8601' ||
                    plainDateTime.toString() !==
                        '2024-05-06T07:08:09.123456789') {
                  throw new Error(
                      'unexpected Temporal.PlainDateTime fixture');
                }
                globalThis
                    .__heap_dump_js_temporal_plain_date_time_exact_prototype_edge = {
                  plainDateTime, prototype
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_temporal_plain_date_time_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> plain_date_time_value =
                get_property("plainDateTime");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK(plain_date_time_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK_EQ(JS_TEMPORAL_PLAIN_DATE_TIME_TYPE,
                     LocalObjectInstanceType(plain_date_time_value));

            v8::Local<v8::Object> plain_date_time =
                v8::Local<v8::Object>::Cast(plain_date_time_value);
            CHECK(plain_date_time->GetPrototypeV2()->StrictEquals(
                prototype_value));

            auto check_int_property = [&](const char* name, int expected) {
              v8::Local<v8::Value> field_value =
                  plain_date_time
                      ->Get(context, NewApiString(v8_isolate, name))
                      .ToLocalChecked();
              CHECK(field_value->IsNumber());
              CHECK_EQ(expected, field_value->Int32Value(context).FromJust());
            };
            check_int_property("year", 2024);
            check_int_property("month", 5);
            check_int_property("day", 6);
            check_int_property("hour", 7);
            check_int_property("minute", 8);
            check_int_property("second", 9);
            check_int_property("millisecond", 123);
            check_int_property("microsecond", 456);
            check_int_property("nanosecond", 789);
            v8::Local<v8::Value> calendar_id =
                plain_date_time
                    ->Get(context,
                          NewApiString(v8_isolate, "calendarId"))
                    .ToLocalChecked();
            CHECK(calendar_id->Equals(context,
                                      NewApiString(v8_isolate, "iso8601"))
                      .FromJust());

            DirectHandle<JSObject> js_plain_date_time =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(
                    *plain_date_time));
            CHECK_EQ(JS_TEMPORAL_PLAIN_DATE_TIME_TYPE,
                     js_plain_date_time->map()->instance_type());
            plain_date_time_map_address = js_plain_date_time->map()->address();
            plain_date_time_address = js_plain_date_time->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* plain_date_time =
      GetSnapshotNode(generator.get(), plain_date_time_address);
  Node* plain_date_time_map =
      GetSnapshotNode(generator.get(), plain_date_time_map_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  CHECK_NOT_NULL(plain_date_time);
  CHECK_NOT_NULL(plain_date_time_map);
  CHECK_NOT_NULL(prototype);
  CHECK(NodeHasType(plain_date_time, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CheckExactTemporalInternalMapEdgeNoWrongKind(generator.get(), plain_date_time,
                                              plain_date_time_map);

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), plain_date_time, "__proto__", prototype);
  MarkCoveredInstanceType(JS_TEMPORAL_PLAIN_DATE_TIME_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSTemporalPlainTimeExactPrototypeEdge) {
  FlagScope<bool> harmony_temporal(&v8_flags.harmony_temporal, true);

  Address plain_time_address = 0;
  Address plain_time_map_address = 0;
  Address prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const plainTime = new Temporal.PlainTime(
                    7, 8, 9, 123, 456, 789);
                const prototype = Object.getPrototypeOf(plainTime);
                if (prototype !== Temporal.PlainTime.prototype ||
                    !(plainTime instanceof Temporal.PlainTime) ||
                    plainTime.hour !== 7 ||
                    plainTime.minute !== 8 ||
                    plainTime.second !== 9 ||
                    plainTime.millisecond !== 123 ||
                    plainTime.microsecond !== 456 ||
                    plainTime.nanosecond !== 789 ||
                    ('calendar' in plainTime) ||
                    ('calendarId' in plainTime) ||
                    plainTime.calendar !== undefined ||
                    plainTime.calendarId !== undefined ||
                    plainTime.toString() !== '07:08:09.123456789') {
                  throw new Error(
                      'unexpected Temporal.PlainTime fixture');
                }
                globalThis
                    .__heap_dump_js_temporal_plain_time_exact_prototype_edge = {
                  plainTime, prototype
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_temporal_plain_time_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> plain_time_value =
                get_property("plainTime");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK(plain_time_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK_EQ(JS_TEMPORAL_PLAIN_TIME_TYPE,
                     LocalObjectInstanceType(plain_time_value));

            v8::Local<v8::Object> plain_time =
                v8::Local<v8::Object>::Cast(plain_time_value);
            CHECK(plain_time->GetPrototypeV2()->StrictEquals(
                prototype_value));

            auto check_int_property = [&](const char* name, int expected) {
              v8::Local<v8::Value> field_value =
                  plain_time
                      ->Get(context, NewApiString(v8_isolate, name))
                      .ToLocalChecked();
              CHECK(field_value->IsNumber());
              CHECK_EQ(expected, field_value->Int32Value(context).FromJust());
            };
            check_int_property("hour", 7);
            check_int_property("minute", 8);
            check_int_property("second", 9);
            check_int_property("millisecond", 123);
            check_int_property("microsecond", 456);
            check_int_property("nanosecond", 789);
            CHECK(!plain_time
                       ->Has(context,
                             NewApiString(v8_isolate, "calendar"))
                       .FromJust());
            CHECK(!plain_time
                       ->Has(context,
                             NewApiString(v8_isolate, "calendarId"))
                       .FromJust());
            CHECK(plain_time
                      ->Get(context, NewApiString(v8_isolate, "calendar"))
                      .ToLocalChecked()
                      ->IsUndefined());
            CHECK(plain_time
                      ->Get(context, NewApiString(v8_isolate, "calendarId"))
                      .ToLocalChecked()
                      ->IsUndefined());

            DirectHandle<JSObject> js_plain_time =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*plain_time));
            CHECK_EQ(JS_TEMPORAL_PLAIN_TIME_TYPE,
                     js_plain_time->map()->instance_type());
            plain_time_map_address = js_plain_time->map()->address();
            plain_time_address = js_plain_time->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* plain_time = GetSnapshotNode(generator.get(), plain_time_address);
  Node* plain_time_map = GetSnapshotNode(generator.get(), plain_time_map_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  CHECK_NOT_NULL(plain_time);
  CHECK_NOT_NULL(plain_time_map);
  CHECK_NOT_NULL(prototype);
  CHECK(NodeHasType(plain_time, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CheckExactTemporalInternalMapEdgeNoWrongKind(generator.get(), plain_time,
                                              plain_time_map);

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), plain_time, "__proto__", prototype);
  MarkCoveredInstanceType(JS_TEMPORAL_PLAIN_TIME_TYPE);
}

UNINITIALIZED_TEST(HeapDumpJSTemporalZonedDateTimeExactPrototypeEdge) {
  FlagScope<bool> harmony_temporal(&v8_flags.harmony_temporal, true);

  Address zoned_date_time_address = 0;
  Address zoned_date_time_map_address = 0;
  Address prototype_address = 0;

  std::unique_ptr<SnapshotGenerator> generator =
      DumpAndTranslateWithIsolateTeardown(
          [&](Isolate* isolate, v8::Local<v8::Context> context) {
            RunScript(context, R"(
              (function() {
                const zonedDateTime = new Temporal.ZonedDateTime(
                    123456789000000000n, 'UTC', 'iso8601');
                const prototype = Object.getPrototypeOf(zonedDateTime);
                if (prototype !== Temporal.ZonedDateTime.prototype ||
                    !(zonedDateTime instanceof Temporal.ZonedDateTime) ||
                    zonedDateTime.epochNanoseconds !== 123456789000000000n ||
                    zonedDateTime.timeZoneId !== 'UTC' ||
                    zonedDateTime.calendarId !== 'iso8601' ||
                    !zonedDateTime.toString().includes('[UTC]')) {
                  throw new Error(
                      'unexpected Temporal.ZonedDateTime fixture');
                }
                globalThis
                    .__heap_dump_js_temporal_zoned_date_time_exact_prototype_edge = {
                  zonedDateTime, prototype
                };
              })();
            )");

            InvokeAtomicMajorGC(isolate->heap());

            v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
            v8::Local<v8::Object> fixture = v8::Local<v8::Object>::Cast(
                context->Global()
                    ->Get(context,
                          NewApiString(
                              v8_isolate,
                              "__heap_dump_js_temporal_zoned_date_time_exact_prototype_edge"))
                    .ToLocalChecked());
            auto get_property = [&](const char* name) {
              return fixture
                  ->Get(context, NewApiString(v8_isolate, name))
                  .ToLocalChecked();
            };
            v8::Local<v8::Value> zoned_date_time_value =
                get_property("zonedDateTime");
            v8::Local<v8::Value> prototype_value = get_property("prototype");
            CHECK(zoned_date_time_value->IsObject());
            CHECK(prototype_value->IsObject());
            CHECK_EQ(JS_TEMPORAL_ZONED_DATE_TIME_TYPE,
                     LocalObjectInstanceType(zoned_date_time_value));

            v8::Local<v8::Object> zoned_date_time =
                v8::Local<v8::Object>::Cast(zoned_date_time_value);
            CHECK(zoned_date_time->GetPrototypeV2()->StrictEquals(
                prototype_value));

            v8::Local<v8::Value> timezone_id =
                zoned_date_time
                    ->Get(context, NewApiString(v8_isolate, "timeZoneId"))
                    .ToLocalChecked();
            CHECK(timezone_id->Equals(context, NewApiString(v8_isolate, "UTC"))
                      .FromJust());

            DirectHandle<JSObject> js_zoned_date_time =
                Cast<JSObject>(v8::Utils::OpenDirectHandle(*zoned_date_time));
            CHECK_EQ(JS_TEMPORAL_ZONED_DATE_TIME_TYPE,
                     js_zoned_date_time->map()->instance_type());
            zoned_date_time_map_address = js_zoned_date_time->map()->address();
            zoned_date_time_address = js_zoned_date_time->address();
            prototype_address = LocalObjectAddress(prototype_value);

            return DumpArtifacts{};
          });

  Node* zoned_date_time =
      GetSnapshotNode(generator.get(), zoned_date_time_address);
  Node* zoned_date_time_map =
      GetSnapshotNode(generator.get(), zoned_date_time_map_address);
  Node* prototype = GetSnapshotNode(generator.get(), prototype_address);
  CHECK_NOT_NULL(zoned_date_time);
  CHECK_NOT_NULL(zoned_date_time_map);
  CHECK_NOT_NULL(prototype);
  CHECK(NodeHasType(zoned_date_time, Node::Type::kObject));
  CHECK(NodeHasType(prototype, Node::Type::kObject));
  CheckExactTemporalInternalMapEdgeNoWrongKind(generator.get(), zoned_date_time,
                                              zoned_date_time_map);

  CheckExactPropertyEdgeNoHiddenOrInternalDuplicate(
      generator.get(), zoned_date_time, "__proto__", prototype);
  MarkCoveredInstanceType(JS_TEMPORAL_ZONED_DATE_TIME_TYPE);
}


#else
UNINITIALIZED_TEST(HeapDumpJSTemporalDurationExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSTemporalInstantExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSTemporalPlainDateExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSTemporalPlainMonthDayExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSTemporalPlainYearMonthExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSTemporalPlainDateTimeExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSTemporalPlainTimeExactPrototypeEdge) {}
UNINITIALIZED_TEST(HeapDumpJSTemporalZonedDateTimeExactPrototypeEdge) {}
#endif

}  // namespace heap
}  // namespace internal
}  // namespace v8
