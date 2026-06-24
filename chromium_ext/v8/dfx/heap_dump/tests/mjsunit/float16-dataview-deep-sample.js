// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Float16Array and DataView float16 accessor shapes alive for heap dump
// smoke. This follows V8's Float16Array/DataView mjsunit cases without relying
// on optimization tier state.

(function() {
  'use strict';

  const catalog = {};
  globalThis.__heapDumpFloat16DataView = catalog;

  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, factory) {
    try {
      factory();
    } catch (e) {
      return keep(name, e);
    }
    return keep(name, null);
  }

  if (typeof Float16Array !== 'function' ||
      typeof DataView.prototype.getFloat16 !== 'function' ||
      typeof DataView.prototype.setFloat16 !== 'function') {
    keep('unavailable', true);
    return;
  }

  const cases = keep('float16Cases', [
    {input: 1.1, expected: Math.f16round(1.1)},
    {input: -1.1, expected: Math.f16round(-1.1)},
    {input: 2049, expected: 2048},
    {input: 2051, expected: 2052},
    {input: 0.00006103515625, expected: 0.00006103515625},
    {input: 0.00006097555160522461, expected: 0.00006097555160522461},
    {input: 5.960464477539063e-8, expected: 5.960464477539063e-8},
    {input: 2.9802322387695312e-8, expected: 0},
    {input: 65504, expected: 65504},
    {input: 65520, expected: Infinity},
    {input: -65520, expected: -Infinity},
    {input: NaN, expected: NaN},
    {input: Infinity, expected: Infinity},
    {input: -Infinity, expected: -Infinity},
  ]);

  const buffer = keep('buffer', new ArrayBuffer(96));
  const dataView = keep('dataView', new DataView(buffer, 8, 64));
  const littleEndianResults = keep('littleEndianResults', []);
  const bigEndianResults = keep('bigEndianResults', []);

  for (let i = 0; i < cases.length; ++i) {
    const offset = i * 2;
    dataView.setFloat16(offset, cases[i].input, true);
    littleEndianResults.push({
      offset,
      input: cases[i].input,
      value: dataView.getFloat16(offset, true),
    });
    dataView.setFloat16(offset, cases[i].input, false);
    bigEndianResults.push({
      offset,
      input: cases[i].input,
      value: dataView.getFloat16(offset, false),
    });
  }

  const float16 = keep('float16', new Float16Array([
    0,
    0.1,
    0.01,
    12.12,
    2049,
    2051,
    65504,
    65520,
    -65520,
    NaN,
    Infinity,
    -Infinity,
  ]));
  const copiedFloat32 = keep('copiedFloat32', new Float32Array(float16));
  const copiedFloat16 = keep('copiedFloat16', new Float16Array(float16));
  const fromArray = keep('fromArray', Float16Array.from([1.25, 2.5, 3.75]));
  const ofArray = keep('ofArray', Float16Array.of(4.5, 5.5, 6.5));
  const subarray = keep('subarray', float16.subarray(2, 8));
  const sliced = keep('sliced', float16.slice(1, 6));
  const mapped = keep('mapped', float16.map((value) => value + 1));
  const filtered = keep('filtered', float16.filter((value) => Number.isFinite(value)));
  const entries = keep('entries', float16.entries());
  keep('firstEntry', entries.next());

  keepError('dataViewGetOutOfBoundsError', () => dataView.getFloat16(63, true));
  keepError('dataViewSetOutOfBoundsError',
            () => dataView.setFloat16(63, 1.5, true));
  keepError('float16ConstructorAlignmentError',
            () => new Float16Array(buffer, 1, 1));

  for (let i = 0; i < cases.length; ++i) {
    assertEquals(cases[i].expected, littleEndianResults[i].value);
    assertEquals(cases[i].expected, bigEndianResults[i].value);
  }
  assertEquals(float16.length, copiedFloat32.length);
  assertEquals(float16.length, copiedFloat16.length);
  assertEquals(3, fromArray.length);
  assertEquals(3, ofArray.length);
  assertTrue(subarray instanceof Float16Array);
  assertTrue(sliced instanceof Float16Array);
  assertTrue(mapped instanceof Float16Array);
  assertTrue(filtered instanceof Float16Array);
  assertEquals(0, catalog.firstEntry.value[0]);
  assertTrue(catalog.dataViewGetOutOfBoundsError instanceof RangeError);
  assertTrue(catalog.dataViewSetOutOfBoundsError instanceof RangeError);
  assertTrue(catalog.float16ConstructorAlignmentError instanceof RangeError);
})();
