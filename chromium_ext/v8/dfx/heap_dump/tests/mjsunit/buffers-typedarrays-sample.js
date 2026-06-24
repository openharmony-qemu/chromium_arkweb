// Heap-dump smoke sample for ArrayBuffer, DataView, and TypedArray families.
//
// Exact buffer/view edges are covered by cctests. This sample keeps many view
// shapes alive as dump/translate input, including guarded RAB/GSAB cases.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function tryKeep(name, factory) {
    try {
      const value = factory();
      keep(name, value);
      return value;
    } catch (e) {
      keep(name + 'Error', e);
      return null;
    }
  }

  const fixedBuffer = keep('fixedBuffer', new ArrayBuffer(256));
  const fixedDataViews = keep('fixedDataViews', [
    new DataView(fixedBuffer),
    new DataView(fixedBuffer, 8),
    new DataView(fixedBuffer, 16, 64),
    new DataView(fixedBuffer, 128, 0),
  ]);
  fixedDataViews[0].setUint32(0, 0x12345678, true);
  fixedDataViews[1].setFloat64(0, Math.PI, true);
  fixedDataViews[2].setBigUint64(0, 0x1234n, true);

  const typedArrayConstructors = [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array,
    BigInt64Array,
    BigUint64Array,
  ];
  if (typeof Float16Array === 'function') {
    typedArrayConstructors.push(Float16Array);
  }

  const typedViews = {};
  const typedViewSlices = {};
  for (const Ctor of typedArrayConstructors) {
    const name = Ctor.name;
    const length = Math.min(8, Math.floor(fixedBuffer.byteLength / Ctor.BYTES_PER_ELEMENT));
    const view = new Ctor(fixedBuffer, 0, length);
    if (Ctor === BigInt64Array || Ctor === BigUint64Array) {
      for (let i = 0; i < view.length; ++i) view[i] = BigInt(i + 1);
    } else {
      for (let i = 0; i < view.length; ++i) view[i] = i + 1;
    }
    typedViews[name] = view;
    typedViewSlices[name] = {
      subarray: view.subarray(1, Math.min(4, view.length)),
      slice: view.slice(0, Math.min(4, view.length)),
    };
  }
  keep('typedViews', typedViews);
  keep('typedViewSlices', typedViewSlices);

  class HeapDumpUint8ArraySubclass extends Uint8Array {}
  class HeapDumpDataViewSubclass extends DataView {}
  keep('typedArraySubclass', new HeapDumpUint8ArraySubclass(fixedBuffer, 32, 16));
  keep('dataViewSubclass', new HeapDumpDataViewSubclass(fixedBuffer, 48, 32));

  const standaloneTypedArrays = keep('standaloneTypedArrays', {
    uint8FromArray: Uint8Array.from([1, 2, 3, 4]),
    int16Of: Int16Array.of(-1, 0, 1, 2),
    float64Array: new Float64Array([1.25, 2.5, 5.0]),
    bigint64Array: new BigInt64Array([1n, 2n, 3n]),
  });
  keep('standaloneArrayBuffers',
       Object.fromEntries(Object.entries(standaloneTypedArrays).map(
           ([name, view]) => [name, view.buffer])));

  const sharedBuffer = tryKeep('sharedBuffer', () => {
    if (typeof SharedArrayBuffer !== 'function') return null;
    return new SharedArrayBuffer(128);
  });
  if (sharedBuffer !== null) {
    const sharedViews = {
      int32: new Int32Array(sharedBuffer),
      uint8: new Uint8Array(sharedBuffer),
      dataView: new DataView(sharedBuffer, 16, 32),
    };
    sharedViews.int32[0] = 42;
    if (typeof Atomics === 'object') {
      Atomics.store(sharedViews.int32, 1, 7);
      keep('sharedAtomicsLoad', Atomics.load(sharedViews.int32, 1));
    }
    keep('sharedViews', sharedViews);
  }

  const resizable = tryKeep('resizableArrayBuffer', () => {
    const buffer = new ArrayBuffer(64, {maxByteLength: 256});
    const fixedLength = new Uint8Array(buffer, 0, 16);
    const lengthTracking = new Uint8Array(buffer);
    const dataView = new DataView(buffer, 8);
    fixedLength[0] = 11;
    lengthTracking[20] = 22;
    if (typeof buffer.resize === 'function') {
      buffer.resize(128);
      lengthTracking[80] = 33;
      buffer.resize(96);
    }
    return {buffer, fixedLength, lengthTracking, dataView};
  });

  const growableShared = tryKeep('growableSharedArrayBuffer', () => {
    if (typeof SharedArrayBuffer !== 'function') return null;
    const buffer = new SharedArrayBuffer(64, {maxByteLength: 256});
    const fixedLength = new Uint8Array(buffer, 0, 16);
    const lengthTracking = new Uint8Array(buffer);
    const dataView = new DataView(buffer, 8);
    fixedLength[0] = 44;
    lengthTracking[20] = 55;
    if (typeof buffer.grow === 'function') {
      buffer.grow(128);
      lengthTracking[80] = 66;
    }
    return {buffer, fixedLength, lengthTracking, dataView};
  });

  const transferSource = tryKeep('transferSource', () => {
    const buffer = new ArrayBuffer(32);
    new Uint8Array(buffer).set([1, 2, 3, 4]);
    if (typeof buffer.transfer === 'function') {
      return {original: buffer, transferred: buffer.transfer(64)};
    }
    return {original: buffer};
  });

  keep('bufferMeta', {
    fixedByteLength: fixedBuffer.byteLength,
    resizableByteLength: resizable && resizable.buffer ?
        resizable.buffer.byteLength : null,
    growableSharedByteLength: growableShared && growableShared.buffer ?
        growableShared.buffer.byteLength : null,
    transferSourceDetached: transferSource && transferSource.original ?
        transferSource.original.byteLength : null,
  });

  assertEquals(fixedBuffer, fixedDataViews[0].buffer);
  assertEquals(256, fixedDataViews[0].byteLength);
  assertTrue(typedViews.Uint8Array instanceof Uint8Array);
  assertTrue(typedViewSlices.Uint8Array.subarray instanceof Uint8Array);

  globalThis.heapDumpBuffersTypedArraysSample = catalog;
})();
