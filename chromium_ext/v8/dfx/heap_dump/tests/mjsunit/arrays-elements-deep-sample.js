// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep JSArray element backing and arguments-object shapes alive for heap dump
// smoke. Exact backing-store edge semantics are pinned by cctest; this sample
// supplies natural JS inputs for dump/translate regression detection.

(function () {
  function makeSloppyArguments(first, second) {
    first.marker = 'mapped-argument-updated';
    arguments.extra = {kind: 'sloppy-arguments-extra'};
    return arguments;
  }

  function makeStrictArguments(first, second) {
    'use strict';
    first.marker = 'strict-argument-updated';
    arguments.extra = {kind: 'strict-arguments-extra'};
    return arguments;
  }

  function makeRestArguments(...rest) {
    rest.extra = {kind: 'rest-array-extra'};
    return rest;
  }

  const objectElements = [{id: 0}, {id: 1}, {id: 2}, {id: 3}];
  const packedSmi = [1, 2, 3, 4, 5, 6];
  const packedDouble = [0.5, 1.5, 2.5, 3.5];
  const packedObject = objectElements.slice();
  const holeySmi = [1, , 3, , 5];
  const holeyDouble = [1.25, , 3.25, , 5.25];
  const holeyObject = [{kind: 'zero'}, , {kind: 'two'}, , {kind: 'four'}];

  const mixedElements = [1, 2, 3];
  mixedElements.push({kind: 'object-transition'});
  mixedElements[8] = {kind: 'holey-transition'};

  const sparseDictionary = [];
  sparseDictionary[0] = {kind: 'first'};
  sparseDictionary[4096] = {kind: 'far'};
  sparseDictionary[8192] = 8192;
  delete sparseDictionary[4096];

  const sealedArray = [1, {kind: 'sealed'}, 3];
  Object.seal(sealedArray);
  const frozenArray = [{kind: 'frozen'}, 2, 3];
  Object.freeze(frozenArray);
  const nonExtensibleArray = [1, 2, {kind: 'non-extensible'}];
  Object.preventExtensions(nonExtensibleArray);

  class ArraySubclass extends Array {
    marker() {
      return this.length;
    }
  }
  const subclassArray = new ArraySubclass(1, {kind: 'subclass'}, 3);
  const subclassMapped = subclassArray.map((value) => value);
  const subclassSliced = subclassArray.slice(1);

  class SpeciesArray extends Array {
    static get [Symbol.species]() {
      return Array;
    }
  }
  const speciesArray = new SpeciesArray(1, 2, {kind: 'species'});
  const speciesMapped = speciesArray.map((value) => value);

  const arrayFromSet = Array.from(new Set([{kind: 'set-a'}, {kind: 'set-b'}]));
  const arrayOfMixed = Array.of(1, 2.5, {kind: 'array-of'}, 'text');
  const spreadArray = [...objectElements, {kind: 'spread-tail'}];
  const concatArray = packedObject.concat(holeyObject, [{kind: 'concat-tail'}]);
  const flatArray = [[{kind: 'nested-a'}], [{kind: 'nested-b'}]].flat();
  const flatMapArray = [1, 2, 3].flatMap((value) => [
    {value},
    value + 0.5,
  ]);

  const mappedArgumentObject = {marker: 'mapped-argument'};
  const strictArgumentObject = {marker: 'strict-argument'};
  const sloppyArguments =
      makeSloppyArguments(mappedArgumentObject, {kind: 'second'});
  const strictArguments =
      makeStrictArguments(strictArgumentObject, {kind: 'second'});
  const restArguments =
      makeRestArguments({kind: 'rest-a'}, {kind: 'rest-b'}, 3.5);

  const arrayIterators = {
    keys: packedObject.keys(),
    values: packedObject.values(),
    entries: packedObject.entries(),
    sparseValues: sparseDictionary.values(),
  };

  assertEquals(6, packedSmi.length);
  assertEquals(3.5, packedDouble[3]);
  assertEquals(5, holeySmi.length);
  assertFalse(1 in holeySmi);
  assertFalse(1 in holeyDouble);
  assertEquals('holey-transition', mixedElements[8].kind);
  assertEquals(8193, sparseDictionary.length);
  assertFalse(4096 in sparseDictionary);
  assertTrue(Object.isSealed(sealedArray));
  assertTrue(Object.isFrozen(frozenArray));
  assertFalse(Object.isExtensible(nonExtensibleArray));
  assertEquals(3, subclassArray.marker());
  assertTrue(subclassMapped instanceof ArraySubclass);
  assertTrue(subclassSliced instanceof ArraySubclass);
  assertFalse(speciesMapped instanceof SpeciesArray);
  assertEquals('mapped-argument-updated', mappedArgumentObject.marker);
  assertEquals('strict-argument-updated', strictArgumentObject.marker);
  assertEquals('sloppy-arguments-extra', sloppyArguments.extra.kind);
  assertEquals('strict-arguments-extra', strictArguments.extra.kind);
  assertEquals('rest-array-extra', restArguments.extra.kind);
  assertEquals('set-a', arrayFromSet[0].kind);
  assertEquals('array-of', arrayOfMixed[2].kind);
  assertEquals('spread-tail', spreadArray[4].kind);
  assertEquals('concat-tail', concatArray[concatArray.length - 1].kind);
  assertEquals('nested-a', flatArray[0].kind);
  assertEquals(6, flatMapArray.length);

  globalThis.heapDumpArraysElementsDeepSample = {
    objectElements,
    packedSmi,
    packedDouble,
    packedObject,
    holeySmi,
    holeyDouble,
    holeyObject,
    mixedElements,
    sparseDictionary,
    sealedArray,
    frozenArray,
    nonExtensibleArray,
    subclassArray,
    subclassMapped,
    subclassSliced,
    speciesArray,
    speciesMapped,
    arrayFromSet,
    arrayOfMixed,
    spreadArray,
    concatArray,
    flatArray,
    flatMapArray,
    sloppyArguments,
    strictArguments,
    restArguments,
    arrayIterators,
  };
})();
