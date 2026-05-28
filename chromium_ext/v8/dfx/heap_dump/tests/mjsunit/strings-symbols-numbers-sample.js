// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep string/name/primitive-adjacent heap shapes alive for heap dump smoke.
// Exact string instance types are pinned by cctest; this mjsunit sample keeps
// natural JS inputs around for CLI dump/translate regression detection.

(function () {
  function makeRope(prefix, count) {
    let value = prefix;
    const pieces = [value];
    for (let i = 0; i < count; ++i) {
      value = value + '|chunk-' + i;
      pieces.push(value);
    }
    return {value, pieces};
  }

  const oneByteBase = 'heap-dump-string-shapes-' + 'abcdefghijklmnopqrstuvwxyz';
  const twoByteBase =
      'heap-dump-two-byte-\u0100-\u2603-\u{1f680}-\u{1f9ea}';
  const oneByteRope = makeRope('rope-one-byte', 32);
  const twoByteRope = makeRope('rope-two-byte-\u0100', 24);
  const slicedOneByte = oneByteRope.value.slice(5, 80);
  const slicedTwoByte = twoByteRope.value.substring(3, 70);
  const splitPieces = oneByteRope.value.split('|').slice(0, 12);
  const replacedTwoByte = twoByteBase.replace(/\u0100/, '\u0101');
  const normalized = 'e\u0301-\u212b'.normalize('NFC');
  const padded = 'pad'.padStart(40, 'x').padEnd(64, 'y');
  const rawTemplate = String.raw`heap\n${'dump'}\u0100`;

  const dynamicKey = ('dynamic-' + 'property-key').slice(0);
  const dynamicAccessorKey = ('dynamic-' + 'accessor-key').substring(0);
  const internalizedKeyCarrier = {
    [dynamicKey]: {kind: 'data-property'},
    get [dynamicAccessorKey]() {
      return {kind: 'getter-result'};
    },
  };
  Object.defineProperty(internalizedKeyCarrier, 'defined-' + 'property', {
    value: {kind: 'defined-property'},
    enumerable: true,
  });

  class ComputedNameCarrier {
    [dynamicKey]() {
      return dynamicKey;
    }

    static [('static-' + 'computed-name').slice(0)]() {
      return 'static-computed-name';
    }
  }

  const localSymbol = Symbol('heap-dump-local-symbol');
  const registrySymbol = Symbol.for('heap-dump-registry-symbol');
  const symbolCarrier = {
    [localSymbol]: {kind: 'local-symbol-property'},
    [registrySymbol]: {kind: 'registry-symbol-property'},
    [Symbol.iterator]: function* symbolCarrierIterator() {
      yield localSymbol;
      yield registrySymbol;
    },
  };
  const symbolWrapper = Object(Symbol('heap-dump-wrapped-symbol'));

  const largeBigInt = (1n << 100n) + 20260525n;
  const negativeBigInt = -((1n << 80n) + 17n);
  const bigintWrapper = Object(largeBigInt);
  const heapNumbers = [
    Math.PI,
    Number.MIN_VALUE,
    Number.MAX_SAFE_INTEGER + 0.5,
    NaN,
    Infinity,
    -Infinity,
    -0,
  ];
  const numberWrapper = Object(42.125);

  const holeyArray = [oneByteBase, , twoByteBase, , largeBigInt];
  holeyArray.length = 16;
  holeyArray[12] = slicedTwoByte;
  const sparseArray = [];
  sparseArray[1024] = {kind: 'sparse-high-index'};
  sparseArray[2048] = normalized;
  delete sparseArray[1024];

  const stringObject = new String(slicedOneByte);
  stringObject.extra = slicedTwoByte;
  const stringIterator = twoByteBase[Symbol.iterator]();
  const regexpMatch = /(\w+)-(\w+)/.exec(oneByteBase);

  assertEquals('chunk-0', splitPieces[1]);
  assertTrue(slicedOneByte.length > 10);
  assertTrue(slicedTwoByte.includes('\u0100'));
  assertEquals('data-property', internalizedKeyCarrier[dynamicKey].kind);
  assertEquals('getter-result', internalizedKeyCarrier[dynamicAccessorKey].kind);
  assertEquals(dynamicKey, new ComputedNameCarrier()[dynamicKey]());
  assertEquals('static-computed-name', ComputedNameCarrier['static-computed-name']());
  assertEquals(localSymbol, Array.from(symbolCarrier)[0]);
  assertEquals('heap-dump-registry-symbol', Symbol.keyFor(registrySymbol));
  assertTrue(largeBigInt > 0n);
  assertTrue(negativeBigInt < 0n);
  assertTrue(Number.isNaN(heapNumbers[3]));
  assertTrue(Object.is(heapNumbers[6], -0));
  assertEquals(16, holeyArray.length);
  assertEquals(2049, sparseArray.length);
  assertEquals('String', stringObject.constructor.name);
  assertEquals('heap', regexpMatch[1]);

  globalThis.heapDumpStringsSymbolsNumbersSample = {
    oneByteBase,
    twoByteBase,
    oneByteRope,
    twoByteRope,
    slicedOneByte,
    slicedTwoByte,
    splitPieces,
    replacedTwoByte,
    normalized,
    padded,
    rawTemplate,
    dynamicKey,
    dynamicAccessorKey,
    internalizedKeyCarrier,
    ComputedNameCarrier,
    symbolCarrier,
    localSymbol,
    registrySymbol,
    symbolWrapper,
    largeBigInt,
    negativeBigInt,
    bigintWrapper,
    heapNumbers,
    numberWrapper,
    holeyArray,
    sparseArray,
    stringObject,
    stringIterator,
    regexpMatch,
  };
})();
