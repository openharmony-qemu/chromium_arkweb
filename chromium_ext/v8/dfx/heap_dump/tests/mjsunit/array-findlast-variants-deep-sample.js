// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Array.prototype.findLast/findLastIndex variants alive for heap dump
// smoke. This follows V8's harmony array findLast/findLastIndex mjsunit paths
// while retaining callback logs, prototype-hole hits, generic receivers,
// mutation results, and representative TypeError objects.

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
    } catch (error) {
      keep(name, error);
      return error;
    }
    return null;
  }

  const packed = keep('packed', [
    {name: 'first', score: 1},
    {name: 'middle', score: -1},
    {name: 'last', score: 5},
  ]);
  const packedLog = keep('packedLog', []);
  const foundObject = keep('foundObject', packed.findLast((value, index) => {
    packedLog.push({index, value});
    return value.score > 0;
  }));
  const foundIndex = keep('foundIndex',
                          packed.findLastIndex((value) => value.score > 0));

  const holey = keep('holey', [, , {name: 'own-last', score: -1}]);
  const oldPrototype = holey.__proto__;
  const prototypeCarrier = keep('prototypeCarrier',
                                Object.create(Array.prototype));
  prototypeCarrier[0] = {name: 'proto-zero', score: 6};
  holey.__proto__ = prototypeCarrier;
  const prototypeResult = keep('prototypeResult', {
    value: holey.findLast((value) => value && value.score > 0),
    index: holey.findLastIndex((value) => value && value.score > 0),
  });
  holey.__proto__ = oldPrototype;

  const getterLog = keep('getterLog', []);
  const getterArray = keep('getterArray', [0, 1, 2, 3]);
  Object.defineProperty(getterArray, 2, {
    configurable: true,
    get() {
      getterLog.push({kind: 'getter', index: 2});
      return {name: 'getter-two', score: 42};
    },
  });
  const getterResult = keep('getterResult', {
    value: getterArray.findLast((value, index) => {
      getterLog.push({kind: 'callback', index, value});
      return value && value.score === 42;
    }),
    index: getterArray.findLastIndex((value) => value && value.score === 42),
    descriptors: Object.getOwnPropertyDescriptors(getterArray),
  });

  const genericReceiver = keep('genericReceiver', {
    length: 4,
    0: {name: 'zero', score: 0},
    2: {name: 'two', score: 2},
    3: {name: 'three', score: 3},
  });
  const genericResult = keep('genericResult', {
    value: Array.prototype.findLast.call(
        genericReceiver, (value) => value && value.score < 3),
    index: Array.prototype.findLastIndex.call(
        genericReceiver, (value) => value && value.score < 3),
  });

  const mutatingArray = keep('mutatingArray', [
    {name: 'a', score: 1},
    {name: 'b', score: 2},
    {name: 'c', score: 3},
  ]);
  const mutationLog = keep('mutationLog', []);
  const mutationResult = keep('mutationResult', {
    value: mutatingArray.findLast((value, index, array) => {
      mutationLog.push({index, value});
      if (index === 2) {
        array[0] = {name: 'mutated-zero', score: 100};
        array.push({name: 'pushed-after-start', score: 200});
      }
      return value.score === 100;
    }),
    index: mutatingArray.findLastIndex((value) => value.score === 100),
    finalArray: mutatingArray,
  });

  const boxedStringResult = keep('boxedStringResult', {
    value: Array.prototype.findLast.call('abc', (value) => value === 'b'),
    index: Array.prototype.findLastIndex.call('abc', (value) => value === 'b'),
  });

  keepError('findLastNullReceiverError',
            () => Array.prototype.findLast.call(null, () => true));
  keepError('findLastIndexUndefinedReceiverError',
            () => Array.prototype.findLastIndex.call(undefined, () => true));
  keepError('findLastNonCallableError',
            () => [1].findLast({notCallable: true}));
  keepError('findLastIndexCallbackError',
            () => [1, 2, 3].findLastIndex(() => {
              throw new RangeError('heap-dump findLastIndex callback');
            }));

  assertEquals('last', foundObject.name);
  assertEquals(2, foundIndex);
  assertEquals('proto-zero', prototypeResult.value.name);
  assertEquals(0, prototypeResult.index);
  assertEquals('getter-two', getterResult.value.name);
  assertEquals(2, getterResult.index);
  assertEquals('two', genericResult.value.name);
  assertEquals(2, genericResult.index);
  assertEquals('mutated-zero', mutationResult.value.name);
  assertEquals(0, mutationResult.index);
  assertEquals(4, mutatingArray.length);
  assertEquals('b', boxedStringResult.value);
  assertEquals(1, boxedStringResult.index);
  assertTrue(catalog.findLastNullReceiverError instanceof TypeError);
  assertTrue(catalog.findLastIndexUndefinedReceiverError instanceof TypeError);
  assertTrue(catalog.findLastNonCallableError instanceof TypeError);
  assertTrue(catalog.findLastIndexCallbackError instanceof RangeError);

  globalThis.heapDumpArrayFindLastVariantsDeepSample = catalog;
})();
