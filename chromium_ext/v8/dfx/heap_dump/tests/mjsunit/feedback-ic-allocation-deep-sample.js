// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep feedback-vector, feedback-cell, allocation-site, data-handler, and
// call/load/store IC-adjacent shapes alive for heap dump smoke. This sample
// intentionally warms common JS operations without relying on native syntax or
// optimization state.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function makeFastObject(seed) {
    return {
      fixed: seed,
      payload: {seed},
      method(delta) {
        return this.fixed + delta;
      },
    };
  }

  function makeAlternateObject(seed) {
    return {
      fixed: seed,
      payload: {seed, alternate: true},
      extra: seed * 2,
      method(delta) {
        return this.fixed + this.extra + delta;
      },
    };
  }

  function loadNamed(object) {
    return object.payload.seed + object.fixed;
  }

  function storeNamed(object, value) {
    object.dynamicSlot = {value};
    return object.dynamicSlot.value;
  }

  function loadKeyed(object, key) {
    return object[key];
  }

  function storeKeyed(object, key, value) {
    object[key] = value;
    return object[key];
  }

  function callProperty(object, delta) {
    return object.method(delta);
  }

  function callTarget(fn, receiver, value) {
    return fn.call(receiver, value);
  }

  function Constructed(seed) {
    this.seed = seed;
    this.payload = {constructed: seed};
  }

  function constructTarget(Ctor, value) {
    return new Ctor(value);
  }

  function makeArrayLiteral(seed) {
    return [
      seed,
      seed + 1,
      {arrayPayload: seed + 2},
      [seed + 3, seed + 4],
    ];
  }

  function makeObjectLiteral(seed) {
    const key = 'literal_' + (seed % 8);
    return {
      seed,
      [key]: {key, seed},
      nested: {
        left: {seed: seed + 1},
        right: [seed + 2, seed + 3],
      },
    };
  }

  const monomorphicObjects = keep('monomorphicObjects', []);
  const polymorphicObjects = keep('polymorphicObjects', []);
  const megamorphicObjects = keep('megamorphicObjects', []);
  const keyedObjects = keep('keyedObjects', []);
  const constructedObjects = keep('constructedObjects', []);
  const arrayLiterals = keep('arrayLiterals', []);
  const objectLiterals = keep('objectLiterals', []);
  const feedbackResults = keep('feedbackResults', []);

  for (let i = 0; i < 96; ++i) {
    const object = makeFastObject(i);
    monomorphicObjects.push(object);
    feedbackResults.push(loadNamed(object));
    feedbackResults.push(storeNamed(object, i + 100));
    feedbackResults.push(callProperty(object, 1));
  }

  for (let i = 0; i < 96; ++i) {
    const object = (i & 1) === 0 ? makeFastObject(i) : makeAlternateObject(i);
    polymorphicObjects.push(object);
    feedbackResults.push(loadNamed(object));
    feedbackResults.push(callProperty(object, 2));
  }

  for (let i = 0; i < 48; ++i) {
    const object = {fixed: i, payload: {seed: i}};
    object['shape_' + i] = {i};
    object.method = function megamorphicMethod(delta) {
      return this.fixed + delta;
    };
    megamorphicObjects.push(object);
    feedbackResults.push(loadNamed(object));
    feedbackResults.push(storeNamed(object, i + 200));
    feedbackResults.push(callProperty(object, 3));
  }

  for (let i = 0; i < 80; ++i) {
    const key = 'key_' + (i % 16);
    const object = {};
    keyedObjects.push(object);
    const stored = storeKeyed(object, key, {i, key});
    feedbackResults.push(loadKeyed(object, key).i + stored.i);
  }

  for (let i = 0; i < 64; ++i) {
    constructedObjects.push(constructTarget(Constructed, i));
    arrayLiterals.push(makeArrayLiteral(i));
    objectLiterals.push(makeObjectLiteral(i));
  }

  const callReceivers = keep('callReceivers', [
    {fixed: 10},
    {fixed: 20, extra: true},
    Object.create({inherited: true}, {
      fixed: {value: 30, enumerable: true},
    }),
  ]);
  function receiverMethod(delta) {
    return this.fixed + delta;
  }
  keep('receiverMethod', receiverMethod);
  for (let i = 0; i < 96; ++i) {
    feedbackResults.push(callTarget(
        receiverMethod, callReceivers[i % callReceivers.length], i));
  }

  const closureCells = keep('closureCells', []);
  function makeClosure(seed) {
    let cell = {seed};
    return function feedbackClosure(delta) {
      cell = {seed: cell.seed + delta};
      return cell.seed;
    };
  }
  for (let i = 0; i < 32; ++i) {
    const closure = makeClosure(i);
    closureCells.push(closure);
    feedbackResults.push(closure(1));
    feedbackResults.push(closure(2));
  }

  assertEquals(0, feedbackResults[0]);
  assertEquals(100, feedbackResults[1]);
  assertEquals(1, feedbackResults[2]);
  assertEquals(0, constructedObjects[0].payload.constructed);
  assertEquals(2, arrayLiterals[0][2].arrayPayload);
  assertEquals(2, objectLiterals[1].nested.left.seed);
  assertEquals(11, callTarget(receiverMethod, callReceivers[0], 1));
  assertEquals(3, closureCells[0](0));

  globalThis.heapDumpFeedbackIcAllocationDeepSample = catalog;
})();
