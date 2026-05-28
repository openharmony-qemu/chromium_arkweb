// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation --allow-natives-syntax

// Keep copy-on-write array literal elements alive for heap dump smoke. This
// follows V8's object-in-array mjsunit warmup pattern so `%HasCowElements`
// proves the intended FixedArray backing-store shape before dumping.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function warmup(f) {
    %PrepareFunctionForOptimization(f);
    for (let i = 0; i < 10; ++i) f();
  }

  function createPackedSmiNestedArray() {
    return [[1, 2, 3, 4]];
  }

  function createHoleySmiNestedArray() {
    return [[1, 2, , 4]];
  }

  function createPackedTaggedNestedArray() {
    return [[1, 2, 3, 'force PACKED_ELEMENTS']];
  }

  function createHoleyTaggedNestedArray() {
    return [[1, , 3, 'force HOLEY_ELEMENTS']];
  }

  function createArrayInObjectInArray() {
    return [{payload: [1, 2, 3, 'nested-cow']}];
  }

  const creators = keep('creators', [
    createPackedSmiNestedArray,
    createHoleySmiNestedArray,
    createPackedTaggedNestedArray,
    createHoleyTaggedNestedArray,
    createArrayInObjectInArray,
  ]);
  for (const creator of creators) warmup(creator);

  const packedSmi = keep('packedSmi', createPackedSmiNestedArray());
  const holeySmi = keep('holeySmi', createHoleySmiNestedArray());
  const packedTagged = keep('packedTagged', createPackedTaggedNestedArray());
  const holeyTagged = keep('holeyTagged', createHoleyTaggedNestedArray());
  const arrayInObject = keep('arrayInObject', createArrayInObjectInArray());

  assertTrue(%HasCowElements(packedSmi[0]));
  assertTrue(%HasCowElements(holeySmi[0]));
  assertTrue(%HasCowElements(packedTagged[0]));
  assertTrue(%HasCowElements(holeyTagged[0]));
  assertTrue(%HasCowElements(arrayInObject[0].payload));
  assertFalse(2 in holeySmi[0]);
  assertFalse(1 in holeyTagged[0]);

  const copiedAfterWrite = keep('copiedAfterWrite', createPackedTaggedNestedArray());
  assertTrue(%HasCowElements(copiedAfterWrite[0]));
  copiedAfterWrite[0][0] = {slot: 'detached-from-cow'};
  assertFalse(%HasCowElements(copiedAfterWrite[0]));
  assertEquals('detached-from-cow', copiedAfterWrite[0][0].slot);

  keep('referenceChecks', [
    packedTagged[0][3],
    holeyTagged[0][3],
    arrayInObject[0].payload,
    copiedAfterWrite[0],
  ]);

  globalThis.heapDumpCowElementsDeepSample = catalog;
})();
