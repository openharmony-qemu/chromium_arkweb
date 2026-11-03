// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Keep large object/element literal boilerplate shapes alive for heap dump
// smoke. This follows V8's large-object-literal tests but uses smaller counts
// so the rawheap -> translate runner stays fast.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function makeNamedLiteralFactory(count, nullPrototype) {
    const properties = [];
    for (let i = 0; i < count; ++i) {
      properties.push('p' + i + ':' + i);
    }
    const proto = nullPrototype ? '__proto__: null, ' : '';
    return new Function('return {' + proto + properties.join(',') + '};');
  }

  function makeElementLiteralFactory(count) {
    const properties = [];
    for (let i = 0; i < count; ++i) {
      properties.push(i + ':"element-' + i + '"');
    }
    return new Function('return {' + properties.join(',') + '};');
  }

  function makeMixedLiteralFactory(count) {
    const properties = [];
    for (let i = 0; i < count; ++i) {
      properties.push('p' + i + ':{slot:' + i + '}');
      properties.push(i + ':"index-' + i + '"');
    }
    return new Function('return {' + properties.join(',') + '};');
  }

  const namedFactory = keep('namedFactory', makeNamedLiteralFactory(256, false));
  const nullProtoFactory =
      keep('nullProtoFactory', makeNamedLiteralFactory(96, true));
  const elementFactory = keep('elementFactory', makeElementLiteralFactory(512));
  const mixedFactory = keep('mixedFactory', makeMixedLiteralFactory(96));

  const namedObjects = keep('namedObjects', []);
  const nullProtoObjects = keep('nullProtoObjects', []);
  const elementObjects = keep('elementObjects', []);
  const mixedObjects = keep('mixedObjects', []);
  for (let i = 0; i < 8; ++i) {
    namedObjects.push(namedFactory());
    nullProtoObjects.push(nullProtoFactory());
    elementObjects.push(elementFactory());
    mixedObjects.push(mixedFactory());
  }

  assertFalse(%HasFastProperties(namedObjects[0]));
  assertFalse(%HasFastProperties(nullProtoObjects[0]));
  assertEquals(null, Object.getPrototypeOf(nullProtoObjects[0]));
  assertEquals(255, namedObjects[0].p255);
  assertEquals(95, nullProtoObjects[0].p95);
  assertEquals('element-511', elementObjects[0][511]);
  assertEquals('index-95', mixedObjects[0][95]);
  assertEquals(95, mixedObjects[0].p95.slot);

  namedObjects[0].addedAfterClone = {kind: 'named-added'};
  nullProtoObjects[0].addedAfterClone = {kind: 'null-proto-added'};
  elementObjects[0][1024] = {kind: 'element-added'};
  mixedObjects[0].extra = {kind: 'mixed-extra'};

  keep('keySnapshots', {
    named: Object.keys(namedObjects[1]).slice(0, 16),
    nullProto: Object.keys(nullProtoObjects[1]).slice(0, 16),
    element: Object.keys(elementObjects[1]).slice(0, 16),
    mixed: Object.keys(mixedObjects[1]).slice(0, 16),
  });
  keep('valueChecks', [
    namedObjects[0].addedAfterClone,
    nullProtoObjects[0].addedAfterClone,
    elementObjects[0][1024],
    mixedObjects[0].extra,
  ]);

  globalThis.heapDumpLargeLiteralsDeepSample = catalog;
})();
