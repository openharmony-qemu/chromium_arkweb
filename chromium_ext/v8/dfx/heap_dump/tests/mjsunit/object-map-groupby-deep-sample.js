// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  const records = [
    {marker: 'groupby-even-1', bucket: 'even'},
    {marker: 'groupby-even-2', bucket: 'even'},
    {marker: 'groupby-odd', bucket: 'odd'},
  ];

  const evenKey = {marker: 'groupby-map-key-even'};
  const oddKey = {marker: 'groupby-map-key-odd'};

  let groupedMap;
  let groupedObject;
  let groupedMapEven;
  let groupedMapOdd;
  let groupedObjectEven;
  let groupedObjectOdd;

  if (typeof Map.groupBy === 'function') {
    groupedMap = Map.groupBy(records, (record) => {
      return record.bucket === 'even' ? evenKey : oddKey;
    });
    groupedMapEven = groupedMap.get(evenKey);
    groupedMapOdd = groupedMap.get(oddKey);
    assertTrue(groupedMap instanceof Map);
    assertEquals(2, groupedMap.size);
    assertEquals(2, groupedMapEven.length);
    assertEquals(1, groupedMapOdd.length);
    assertThrows(
        () => Map.groupBy(records, () => {
          throw new Error('heap-dump-map-groupby-error');
        }),
        Error,
        'heap-dump-map-groupby-error');
  }

  if (typeof Object.groupBy === 'function') {
    groupedObject = Object.groupBy(records, (record) => record.bucket);
    groupedObjectEven = groupedObject.even;
    groupedObjectOdd = groupedObject.odd;
    assertTrue(groupedObjectEven !== undefined);
    assertTrue(groupedObjectOdd !== undefined);
    assertEquals(2, groupedObjectEven.length);
    assertEquals(1, groupedObjectOdd.length);
    assertTrue(Array.isArray(groupedObjectEven));
    assertTrue(Array.isArray(groupedObjectOdd));
    assertEquals(records[0], groupedObjectEven[0]);
    assertEquals(records[1], groupedObjectEven[1]);
    assertEquals(records[2], groupedObjectOdd[0]);
    assertThrows(
        () => Object.groupBy(records, () => {
          throw new Error('heap-dump-object-groupby-error');
        }),
        Error,
        'heap-dump-object-groupby-error');
  }

  if (typeof Map.groupBy !== 'function' &&
      typeof Object.groupBy !== 'function') {
    globalThis.__heap_dump_object_map_groupby_deep_sample = {
      skipped: 'Map.groupBy and Object.groupBy are not available',
      records,
      evenKey,
      oddKey,
    };
    return;
  }

  globalThis.__heap_dump_object_map_groupby_deep_sample = {
    records,
    evenKey,
    oddKey,
    groupedMap,
    groupedObject,
    groupedMapEven,
    groupedMapOdd,
    groupedObjectEven,
    groupedObjectOdd,
  };
})();
