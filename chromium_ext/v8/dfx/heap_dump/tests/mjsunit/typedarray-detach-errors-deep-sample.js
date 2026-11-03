// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --js-staging

// Keep detached ArrayBuffer, TypedArray, DataView, and related error states
// alive for heap dump smoke. This follows V8 mjsunit detach patterns while
// avoiding worker timing or GC-dependent behavior.

(function() {
  'use strict';

  const catalog = {};
  globalThis.__heapDumpTypedArrayDetachErrors = catalog;

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

  const numberConstructors = [
    Uint8Array,
    Int8Array,
    Uint8ClampedArray,
    Uint16Array,
    Int16Array,
    Uint32Array,
    Int32Array,
    Float32Array,
    Float64Array,
  ];
  if (typeof Float16Array === 'function') {
    numberConstructors.push(Float16Array);
  }
  keep('numberConstructors', numberConstructors);

  const detachedStates = keep('detachedStates', []);
  for (const Ctor of numberConstructors) {
    const typedArray = new Ctor([1, 2, 3, 4, 5, 6]);
    const iterator = typedArray.values();
    const subarray = typedArray.subarray(1, 4);
    %ArrayBufferDetach(typedArray.buffer);

    detachedStates.push({
      ctorName: Ctor.name,
      typedArray,
      subarray,
      iterator,
      iteratorNextError: keepError(
          Ctor.name + 'IteratorNextError', () => iterator.next()),
      lengthAfterDetach: typedArray.length,
      byteLengthAfterDetach: typedArray.byteLength,
      findError: keepError(
          Ctor.name + 'FindError', () => typedArray.find(() => true)),
      mapError: keepError(
          Ctor.name + 'MapError', () => typedArray.map((value) => value)),
      setError: keepError(
          Ctor.name + 'SetError', () => typedArray.set([9], 0)),
      dataError: keepError(
          Ctor.name + 'ConstructFromDetachedError',
          () => new Uint8Array(typedArray.buffer)),
    });
  }

  const bigintStates = keep('bigintStates', []);
  for (const Ctor of [BigInt64Array, BigUint64Array]) {
    const typedArray = new Ctor([1n, 2n, 3n, 4n]);
    const entries = typedArray.entries();
    %ArrayBufferDetach(typedArray.buffer);

    bigintStates.push({
      ctorName: Ctor.name,
      typedArray,
      entries,
      entriesNextError: keepError(
          Ctor.name + 'EntriesNextError', () => entries.next()),
      reduceError: keepError(
          Ctor.name + 'ReduceError',
          () => typedArray.reduce((sum, value) => sum + value, 0n)),
      copyWithinError: keepError(
          Ctor.name + 'CopyWithinError', () => typedArray.copyWithin(0, 1)),
    });
  }

  const dataViewBuffer = keep('dataViewBuffer', new ArrayBuffer(32));
  const dataView = keep('dataView', new DataView(dataViewBuffer, 4, 16));
  dataView.setUint32(0, 0x12345678, true);
  %ArrayBufferDetach(dataViewBuffer);
  keep('dataViewDetachedState', {
    buffer: dataViewBuffer,
    dataView,
    byteLengthError: keepError(
        'DataViewByteLengthError', () => dataView.byteLength),
    getUint8Error: keepError('DataViewGetUint8Error', () => dataView.getUint8(0)),
    setUint16Error: keepError(
        'DataViewSetUint16Error', () => dataView.setUint16(0, 0xabcd, true)),
  });

  const midIteration = keep('midIteration', {});
  const arrayForFindLast = new Uint8Array([1, 2, 3, 4, 5, 6]);
  const visited = [];
  const findLastResult = arrayForFindLast.findLast((value) => {
    visited.push(value);
    if (value === 4) {
      %ArrayBufferDetach(arrayForFindLast.buffer);
    }
    return false;
  });
  midIteration.arrayForFindLast = arrayForFindLast;
  midIteration.visited = visited;
  midIteration.findLastResult = findLastResult;

  const detachedParameterOrder = keep('detachedParameterOrder', {});
  const arrayForParameterOrder = new Uint8Array([7, 8, 9]);
  const shouldNotCoerce = {
    [Symbol.toPrimitive]() {
      detachedParameterOrder.coerced = true;
      return 0;
    },
  };
  %ArrayBufferDetach(arrayForParameterOrder.buffer);
  detachedParameterOrder.array = arrayForParameterOrder;
  detachedParameterOrder.error = keepError(
      'DetachedParameterOrderError',
      () => arrayForParameterOrder.with(shouldNotCoerce, 1));
  detachedParameterOrder.coerced = detachedParameterOrder.coerced === true;

  if (typeof ArrayBuffer.prototype.transfer === 'function') {
    const transferSource = keep('transferSource', new ArrayBuffer(48));
    const sourceView = keep('transferDetachedSourceView',
                            new Uint16Array(transferSource));
    sourceView[0] = 0x1234;
    const transferred = keep('transferTarget', transferSource.transfer(96));
    keep('transferDetachedState', {
      transferSource,
      sourceView,
      transferred,
      sourceByteLength: transferSource.byteLength,
      sourceViewLength: sourceView.length,
      targetByteLength: transferred.byteLength,
    });
  }

  assertTrue(detachedStates.length >= 9);
  assertEquals(0, detachedStates[0].lengthAfterDetach);
  assertTrue(catalog.DataViewByteLengthError instanceof TypeError);
  assertTrue(catalog.DetachedParameterOrderError instanceof TypeError);
  assertFalse(detachedParameterOrder.coerced);
})();
