// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep JSArray elements-kind transition and backing-store shapes alive for heap
// dump smoke. Exact JSArray/FixedArray/FixedDoubleArray edges are pinned by
// cctests; this sample supplies naturally-created transition histories.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const smiToDoubleToObject = keep('smiToDoubleToObject', []);
  for (let i = 0; i < 24; ++i) {
    const array = [i, i + 1, i + 2, i + 3];
    array.push(i + 0.5);
    array[1] = {kind: 'object-transition', i};
    smiToDoubleToObject.push(array);
  }

  const packedToHoley = keep('packedToHoley', []);
  for (let i = 0; i < 24; ++i) {
    const array = [i, i + 1, i + 2, i + 3, i + 4];
    delete array[2];
    array[9] = {kind: 'far-hole', i};
    packedToHoley.push(array);
  }

  const doubleToHoleyObject = keep('doubleToHoleyObject', []);
  for (let i = 0; i < 24; ++i) {
    const array = [i + 0.25, i + 1.25, i + 2.25, i + 3.25];
    array.length = 8;
    array[6] = {kind: 'object-after-double', i};
    doubleToHoleyObject.push(array);
  }

  const dictionaryArrays = keep('dictionaryArrays', []);
  for (let i = 0; i < 10; ++i) {
    const array = [];
    array[0] = {i, index: 0};
    array[1 << 12] = {i, index: 1 << 12};
    array[1 << 16] = {i, index: 1 << 16};
    delete array[1 << 12];
    array.extra = {kind: 'dictionary-extra', i};
    dictionaryArrays.push(array);
  }

  function literalFactory(seed) {
    return [seed, seed + 1, {seed}, seed + 0.5];
  }
  const literalCopies = keep('literalCopies', []);
  for (let i = 0; i < 32; ++i) {
    const literal = literalFactory(i);
    literalCopies.push({
      literal,
      slice: literal.slice(),
      concat: literal.concat([{tail: i}]),
      spread: [...literal, {spreadTail: i}],
    });
  }

  const builtinTransitionArrays = keep('builtinTransitionArrays', []);
  for (let i = 0; i < 16; ++i) {
    const array = [i, i + 1, i + 2, i + 3, i + 4, i + 5];
    array.fill({filled: i}, 1, 3);
    array.copyWithin(3, 0, 2);
    array.splice(2, 1, {spliced: i}, i + 0.75);
    array.reverse();
    array.sort((left, right) => String(left).localeCompare(String(right)));
    builtinTransitionArrays.push(array);
  }

  const constructorArrays = keep('constructorArrays', [
    new Array(8).fill({filled: true}),
    Array.from({length: 12}, (_, i) => i),
    Array.of(1, 2.5, {kind: 'array-of-object'}, 'text'),
  ]);
  constructorArrays[0][11] = {expanded: true};
  constructorArrays[1][20] = 20.5;
  constructorArrays[2][7] = {holeExpansion: true};

  const nestedElementStores = keep('nestedElementStores', []);
  for (let i = 0; i < 20; ++i) {
    const nested = [
      [{i, a: true}],
      [{i, b: true}, i + 0.125],
      ,
      [{i, c: true}],
    ];
    nested[8] = [{i, far: true}];
    nestedElementStores.push(nested);
  }

  const iteratorKeepalive = keep('iteratorKeepalive', {
    smiValues: smiToDoubleToObject[0].values(),
    holeyKeys: packedToHoley[0].keys(),
    dictionaryEntries: dictionaryArrays[0].entries(),
  });

  assertEquals('object-transition', smiToDoubleToObject[0][1].kind);
  assertFalse(2 in packedToHoley[0]);
  assertEquals('far-hole', packedToHoley[0][9].kind);
  assertEquals('object-after-double', doubleToHoleyObject[0][6].kind);
  assertFalse((1 << 12) in dictionaryArrays[0]);
  assertEquals('dictionary-extra', dictionaryArrays[0].extra.kind);
  assertEquals(4, literalCopies[0].literal.length);
  assertEquals(5, literalCopies[0].concat.length);
  assertEquals(7, builtinTransitionArrays[0].length);
  assertEquals(12, constructorArrays[0].length);
  assertEquals(21, constructorArrays[1].length);
  assertEquals(9, nestedElementStores[0].length);
  assertEquals('function', typeof iteratorKeepalive.smiValues.next);

  globalThis.heapDumpArrayElementsTransitionDeepSample = catalog;
})();
