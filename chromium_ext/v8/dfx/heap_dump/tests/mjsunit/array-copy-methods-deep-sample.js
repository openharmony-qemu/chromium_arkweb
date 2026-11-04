// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep ordinary Array change-by-copy result shapes alive for heap dump smoke.
// This follows V8's toSorted/toReversed/toSpliced/with mjsunit coverage while
// retaining original arrays, copied results, generic receivers, species cases,
// and representative RangeError objects.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, callback) {
    try {
      callback();
    } catch (error) {
      return keep(name, error);
    }
    throw new Error(name + ' did not throw');
  }

  const packedSmi = keep('packedSmi', [1, 3, 2, 4]);
  const packedDouble = keep('packedDouble', [1.1, 3.3, 2.2, 4.4]);
  const packedObject = keep('packedObject', [
    {rank: 3}, {rank: 1}, {rank: 4}, {rank: 2},
  ]);
  const holey = keep('holey', [1, , 3, , 2, , 4]);
  const nested = keep('nested', [[3], [1], [2]]);

  keep('sortedSmi', packedSmi.toSorted());
  keep('sortedDouble', packedDouble.toSorted((a, b) => a - b));
  keep('sortedObjects', packedObject.toSorted((a, b) => a.rank - b.rank));
  keep('sortedHoley', holey.toSorted());
  keep('reversedSmi', packedSmi.toReversed());
  keep('reversedHoley', holey.toReversed());
  keep('splicedInsert', packedSmi.toSpliced(1, 2, {inserted: true}, 9));
  keep('splicedDeleteOnly', packedObject.toSpliced(-2, 1));
  keep('withPositive', packedSmi.with(1, {replacement: true}));
  keep('withNegative', packedDouble.with(-1, {replacement: 'last'}));
  keep('nestedReversed', nested.toReversed());

  const genericReceiver = keep('genericReceiver', {
    length: 4,
    get 0() {
      return 'hello';
    },
    get 1() {
      return 'cursed';
    },
    get 2() {
      return 'java';
    },
    get 3() {
      return 'script';
    },
  });
  keep('genericReversed', Array.prototype.toReversed.call(genericReceiver));
  keep('genericSorted', Array.prototype.toSorted.call(genericReceiver));
  keep('genericSpliced',
       Array.prototype.toSpliced.call(genericReceiver, 1, 2, 'heap', 'dump'));
  keep('genericWith', Array.prototype.with.call(genericReceiver, -1, 'done'));

  const readOrder = keep('readOrder', []);
  const orderReceiver = keep('orderReceiver', {
    length: 4,
    get 0() {
      readOrder.push('0');
      return 'zero';
    },
    get 1() {
      readOrder.push('1');
      return 'one';
    },
    get 2() {
      readOrder.push('2');
      return 'two';
    },
    get 3() {
      readOrder.push('3');
      return 'three';
    },
  });
  keep('orderReversed', Array.prototype.toReversed.call(orderReceiver));
  keep('orderSpliced', Array.prototype.toSpliced.call(orderReceiver));

  const mutatingSortSource = keep('mutatingSortSource', [1, 3, 4, 2]);
  keep('mutatingSorted', mutatingSortSource.toSorted((x, y) => {
    mutatingSortSource.pop();
    return x - y;
  }));

  const mutatingWithSource = keep('mutatingWithSource', [1, 2, 3, 4]);
  keep('mutatingWithResult',
       mutatingWithSource.with({
         valueOf() {
           mutatingWithSource.length = 2;
           return 1;
         },
       }, 42));

  class SpeciesArray extends Array {
    static get [Symbol.species]() {
      return SpeciesArray;
    }
  }
  const speciesArray = keep('speciesArray', new SpeciesArray(1, 2, 3, 4));
  keep('speciesToSorted', speciesArray.toSorted());
  keep('speciesToReversed', speciesArray.toReversed());
  keep('speciesToSpliced', speciesArray.toSpliced(1, 1, 9));
  keep('speciesWith', speciesArray.with(1, 9));

  const protoValue = keep('protoValue', {fromPrototype: true});
  Array.prototype[3] = protoValue;
  try {
    const prototypeHoles = keep('prototypeHoles', [,,,,]);
    keep('prototypeHolesSorted', prototypeHoles.toSorted());
    keep('prototypeHolesReversed', prototypeHoles.toReversed());
    keep('prototypeHolesSpliced', prototypeHoles.toSpliced());
    keep('prototypeHolesWith', prototypeHoles.with(1, {own: true}));
  } finally {
    delete Array.prototype[3];
  }

  keep('tooBigSortedError',
       keepError('tooBigSortedError',
                 () => Array.prototype.toSorted.call({length: 2 ** 32})));
  keep('tooBigReversedError',
       keepError('tooBigReversedError',
                 () => Array.prototype.toReversed.call({length: 2 ** 32})));
  keep('tooBigSplicedError',
       keepError('tooBigSplicedError',
                 () => Array.prototype.toSpliced.call({length: 2 ** 32})));
  keep('withHighIndexError',
       keepError('withHighIndexError', () => packedSmi.with(packedSmi.length, 0)));
  keep('withLowIndexError',
       keepError('withLowIndexError',
                 () => packedSmi.with(-packedSmi.length - 1, 0)));

  assertArrayEquals([1, 2, 3, 4], catalog.sortedSmi);
  assertArrayEquals([1.1, 2.2, 3.3, 4.4], catalog.sortedDouble);
  assertEquals(1, catalog.sortedObjects[0].rank);
  assertEquals(7, catalog.sortedHoley.length);
  assertTrue(catalog.sortedHoley.hasOwnProperty(6));
  assertArrayEquals([4, 2, 3, 1], catalog.reversedSmi);
  assertTrue(catalog.reversedHoley.hasOwnProperty(1));
  assertEquals(true, catalog.splicedInsert[1].inserted);
  assertEquals(true, catalog.withPositive[1].replacement);
  assertEquals('last', catalog.withNegative[3].replacement);
  assertArrayEquals(['script', 'java', 'cursed', 'hello'],
                    catalog.genericReversed);
  assertArrayEquals(['cursed', 'hello', 'java', 'script'],
                    catalog.genericSorted);
  assertArrayEquals(['hello', 'heap', 'dump', 'script'],
                    catalog.genericSpliced);
  assertArrayEquals(['hello', 'cursed', 'java', 'done'],
                    catalog.genericWith);
  assertArrayEquals(['3', '2', '1', '0', '0', '1', '2', '3'], readOrder);
  assertArrayEquals([1, 2, 3, 4], catalog.mutatingSorted);
  assertEquals(0, mutatingSortSource.length);
  assertArrayEquals([1, 42, undefined, undefined], catalog.mutatingWithResult);
  assertEquals(2, mutatingWithSource.length);
  assertEquals(Array, catalog.speciesToSorted.constructor);
  assertEquals(Array, catalog.speciesToReversed.constructor);
  assertEquals(Array, catalog.speciesToSpliced.constructor);
  assertEquals(Array, catalog.speciesWith.constructor);
  assertSame(protoValue, catalog.prototypeHolesSorted[0]);
  assertSame(protoValue, catalog.prototypeHolesReversed[0]);
  assertSame(protoValue, catalog.prototypeHolesSpliced[3]);
  assertTrue(catalog.tooBigSortedError instanceof RangeError);
  assertTrue(catalog.withHighIndexError instanceof RangeError);

  globalThis.heapDumpArrayCopyMethodsDeepSample = catalog;
})();
