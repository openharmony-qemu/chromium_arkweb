// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep typed-array builtin result and iterator shapes alive for heap dump
// smoke. Basic buffer/view states are covered by buffers-typedarrays-sample.js;
// this sample stresses method-created typed arrays, species/subclass paths,
// BigInt typed arrays, DataView reads/writes, and retained error objects.

(function() {
  'use strict';

  const catalog = {};

  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, factory) {
    try {
      factory();
    } catch (e) {
      keep(name, e);
      return e;
    }
    return null;
  }

  const numberConstructors = keep('numberConstructors', [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array,
  ]);
  if (typeof Float16Array === 'function') {
    numberConstructors.push(Float16Array);
  }

  const numericResults = keep('numericResults', []);
  for (const Ctor of numberConstructors) {
    const values = Ctor === Uint8ClampedArray ?
        [-100, 0, 1, 2, 255, 300] :
        [-3, -1, 0, 1, 2, 4];
    const base = new Ctor(values);
    const copied = new Ctor(base.length + 2);
    copied.set(base, 1);
    base.copyWithin(1, 3, 5);
    base.reverse();
    base.sort();

    const mapped = base.map((value, index) => value + index);
    const filtered = base.filter((value) => value >= 0);
    const sliced = base.slice(1, Math.min(5, base.length));
    const subarray = base.subarray(1, Math.min(5, base.length));
    const entries = base.entries();
    const keys = base.keys();
    const valuesIterator = base.values();

    const result = {
      ctorName: Ctor.name,
      base,
      copied,
      mapped,
      filtered,
      sliced,
      subarray,
      entries,
      keys,
      valuesIterator,
      firstEntry: entries.next(),
      firstKey: keys.next(),
      firstValue: valuesIterator.next(),
      find: base.find((value) => value >= 0),
      findIndex: base.findIndex((value) => value >= 0),
      reduce: base.reduce((sum, value) => sum + Number(value), 0),
      includesZero: base.includes(0),
      joined: base.join('|'),
    };
    if (typeof base.toReversed === 'function') {
      result.toReversed = base.toReversed();
    }
    if (typeof base.toSorted === 'function') {
      result.toSorted = base.toSorted();
    }
    if (typeof base.with === 'function') {
      result.withValue = base.with(0, 7);
    }
    numericResults.push(result);
  }

  const bigintResults = keep('bigintResults', []);
  for (const Ctor of [BigInt64Array, BigUint64Array]) {
    const base = new Ctor([1n, 2n, 3n, 4n, 5n]);
    const copied = new Ctor(base.length + 1);
    copied.set(base, 1);
    base.copyWithin(0, 2, 4);
    const mapped = base.map((value) => value + 1n);
    const filtered = base.filter((value) => value > 2n);
    const sliced = base.slice(1, 4);
    const subarray = base.subarray(1, 4);
    const valuesIterator = base.values();
    bigintResults.push({
      ctorName: Ctor.name,
      base,
      copied,
      mapped,
      filtered,
      sliced,
      subarray,
      valuesIterator,
      firstValue: valuesIterator.next(),
      reduce: base.reduce((sum, value) => sum + value, 0n),
      includesThree: base.includes(3n),
      joined: base.join(':'),
    });
  }

  class HeapDumpUint16Array extends Uint16Array {
    static get [Symbol.species]() {
      return Uint16Array;
    }
  }
  class HeapDumpBigInt64Array extends BigInt64Array {
    static get [Symbol.species]() {
      return BigInt64Array;
    }
  }

  const numberSubclass = keep(
      'numberSubclass',
      new HeapDumpUint16Array([9, 8, 7, 6, 5]));
  keep('numberSubclassResults', {
    constructor: HeapDumpUint16Array,
    mapped: numberSubclass.map((value) => value + 1),
    filtered: numberSubclass.filter((value) => value > 6),
    sliced: numberSubclass.slice(1, 4),
  });

  const bigintSubclass = keep(
      'bigintSubclass',
      new HeapDumpBigInt64Array([9n, 8n, 7n, 6n, 5n]));
  keep('bigintSubclassResults', {
    constructor: HeapDumpBigInt64Array,
    mapped: bigintSubclass.map((value) => value - 1n),
    filtered: bigintSubclass.filter((value) => value < 8n),
    sliced: bigintSubclass.slice(1, 4),
  });

  const dataBuffer = keep('dataBuffer', new ArrayBuffer(64));
  const dataView = keep('dataView', new DataView(dataBuffer));
  dataView.setInt8(0, -1);
  dataView.setUint16(2, 0xabcd, true);
  dataView.setInt32(4, -123456, false);
  dataView.setFloat32(8, Math.PI, true);
  dataView.setFloat64(16, Math.E, false);
  dataView.setBigInt64(24, -123456789n, true);
  dataView.setBigUint64(32, 123456789n, false);
  keep('dataViewReads', {
    int8: dataView.getInt8(0),
    uint16: dataView.getUint16(2, true),
    int32: dataView.getInt32(4, false),
    float32: dataView.getFloat32(8, true),
    float64: dataView.getFloat64(16, false),
    bigint64: dataView.getBigInt64(24, true),
    biguint64: dataView.getBigUint64(32, false),
  });

  keepError('typedArrayWrongValueError',
            () => new BigInt64Array([1n]).set([1], 0));
  keepError('typedArrayOffsetError',
            () => new Uint8Array(new ArrayBuffer(4), 8));
  keepError('dataViewOffsetError',
            () => new DataView(new ArrayBuffer(4), 8));

  assertTrue(numericResults[0].mapped instanceof numberConstructors[0]);
  assertEquals(3n, bigintResults[0].firstValue.value);
  assertTrue(catalog.numberSubclassResults.mapped instanceof Uint16Array);
  assertFalse(catalog.numberSubclassResults.mapped instanceof HeapDumpUint16Array);
  assertEquals(0xabcd, catalog.dataViewReads.uint16);

  globalThis.heapDumpTypedArrayMethodsDeepSample = catalog;
})();
