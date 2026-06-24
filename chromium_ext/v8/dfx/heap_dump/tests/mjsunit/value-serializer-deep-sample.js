// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --js-staging

// Keep d8 value-serializer and structured-clone-like object graphs alive for
// heap dump smoke. This follows V8's value-serializer mjsunit coverage while
// retaining both serialized byte buffers and deserialized graphs.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const sparseArray = keep('sparseArray', []);
  sparseArray[0] = {first: true};
  sparseArray[0xffff] = {last: true};

  const enumProto = keep('enumProto', {
    fn() {
      return 1;
    },
  });
  const enumCacheObject = keep('enumCacheObject', {
    __proto__: enumProto,
    a: 1,
    b: 2,
    c: 'heap-dump-enum-cache',
  });
  for (const key in enumCacheObject) {
    if (key === 'a') enumCacheObject.a += enumCacheObject.fn();
  }

  const circularError = keep('circularError', new Error(undefined));
  delete circularError.stack;
  circularError.cause = circularError;

  const arrayBuffer = keep('arrayBuffer', new ArrayBuffer(32));
  const uint8 = keep('uint8', new Uint8Array(arrayBuffer, 4, 12));
  for (let i = 0; i < uint8.length; ++i) uint8[i] = i * 7;
  const dataView = keep('dataView', new DataView(arrayBuffer, 8, 8));
  dataView.setUint32(0, 0x12345678, true);

  const objectGraph = keep('objectGraph', {
    numbers: [1, -1, -0, NaN, Infinity, -Infinity, 9007199254740993],
    strings: [
      'heap-dump-serializer-one-byte',
      'heap-dump-serializer-two-byte-\u2028',
      'heap-dump-serializer-slice'.slice(5),
    ],
    bigint: 123456789012345678901234567890n,
    date: new Date('2026-05-25T12:34:56.789Z'),
    regexp: /heap-(dump)(?<kind>serializer)/giu,
    map: new Map([
      ['object', {nested: true}],
      [{key: 'object-key'}, new Set([{setValue: 1}, {setValue: 2}])],
    ]),
    set: new Set([sparseArray, enumCacheObject, circularError]),
    arrayBuffer,
    uint8,
    dataView,
    sparseArray,
    enumCacheObject,
    circularError,
  });
  objectGraph.self = objectGraph;

  const serializedGraph = keep(
      'serializedGraph', d8.serializer.serialize(objectGraph));
  const serializedError = keep(
      'serializedError', d8.serializer.serialize(circularError));
  const deserializedGraph = keep(
      'deserializedGraph', d8.serializer.deserialize(serializedGraph));
  const deserializedError = keep(
      'deserializedError', d8.serializer.deserialize(serializedError));

  const malformedInputs = keep('malformedInputs', [
    new Uint8Array([255, 15, 66, 4, 3, 5, 7, 11, 86, 66, 1, 2, 1]),
    new Uint8Array([255, 15, 66, 4, 3, 5, 7, 11, 86, 66, 1, 2, 2]),
  ]);
  const deserializeErrors = keep('deserializeErrors', []);
  for (const input of malformedInputs) {
    try {
      d8.serializer.deserialize(input.buffer);
    } catch (error) {
      deserializeErrors.push(error);
    }
  }

  const byteCopies = keep('byteCopies', [
    new Uint8Array(serializedGraph).slice(0, 64),
    new Uint8Array(serializedError).slice(0, 64),
  ]);

  assertEquals('object', typeof deserializedGraph);
  assertEquals('object', typeof deserializedGraph.self);
  assertSame(deserializedGraph, deserializedGraph.self);
  assertTrue(deserializedGraph.map instanceof Map);
  assertTrue(deserializedGraph.set instanceof Set);
  assertTrue(deserializedGraph.uint8 instanceof Uint8Array);
  assertTrue(deserializedGraph.dataView instanceof DataView);
  assertEquals(12, deserializedGraph.uint8.length);
  assertEquals(0x12345678, deserializedGraph.dataView.getUint32(0, true));
  assertInstanceof(deserializedError, Error);
  assertSame(deserializedError, deserializedError.cause);
  assertEquals(2, deserializeErrors.length);
  assertTrue(deserializeErrors.every((error) => error instanceof Error));
  assertTrue(byteCopies.every((copy) => copy instanceof Uint8Array));

  globalThis.heapDumpValueSerializerDeepSample = catalog;
})();
