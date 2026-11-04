// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-stress-flush-code

// Keep optimized/deoptimized function, feedback-vector, bytecode, and code
// support objects alive for heap dump smoke. The sample follows common V8
// optimization mjsunit patterns but records status and results instead of
// asserting an exact optimization tier, which can vary by build configuration.

(function() {
  'use strict';

  const catalog = {};

  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function numericAdd(a, b) {
    return (a + b) | 0;
  }

  function loadAndCall(object, delta) {
    return object.payload.value + object.method(delta);
  }

  function keyedLoad(object, key) {
    return object[key] + object.fixed;
  }

  function arrayReduce(values) {
    return values.reduce((sum, value) => sum + value, 0);
  }

  function closureFactory(seed) {
    let cell = {seed};
    return function optimizedClosure(delta) {
      cell = {seed: cell.seed + delta};
      return cell.seed;
    };
  }

  function Constructed(seed) {
    this.seed = seed;
    this.payload = {seed};
  }

  function constructAndRead(seed) {
    const object = new Constructed(seed);
    return object.payload.seed + object.seed;
  }

  function makeStableObject(seed) {
    return {
      fixed: seed,
      payload: {value: seed * 2},
      method(delta) {
        return this.fixed + delta;
      },
    };
  }

  function makeDifferentShape(seed) {
    return {
      fixed: seed,
      extra: seed + 1,
      payload: {value: seed * 3},
      method(delta) {
        return this.fixed + this.extra + delta;
      },
    };
  }

  const stableObjects = keep('stableObjects', []);
  const differentObjects = keep('differentObjects', []);
  const arrays = keep('arrays', []);
  for (let i = 0; i < 32; ++i) {
    stableObjects.push(makeStableObject(i));
    differentObjects.push(makeDifferentShape(i));
    arrays.push([i, i + 1, i + 2, i + 3]);
  }

  const closure = keep('closure', closureFactory(10));

  const functions = keep('functions', {
    numericAdd,
    loadAndCall,
    keyedLoad,
    arrayReduce,
    closureFactory,
    closure,
    Constructed,
    constructAndRead,
  });

  %PrepareFunctionForOptimization(numericAdd);
  %PrepareFunctionForOptimization(loadAndCall);
  %PrepareFunctionForOptimization(keyedLoad);
  %PrepareFunctionForOptimization(arrayReduce);
  %PrepareFunctionForOptimization(closure);
  %PrepareFunctionForOptimization(constructAndRead);

  const warmupResults = keep('warmupResults', []);
  for (let i = 0; i < 24; ++i) {
    warmupResults.push(numericAdd(i, i + 1));
    warmupResults.push(loadAndCall(stableObjects[i], 1));
    warmupResults.push(keyedLoad(stableObjects[i], 'fixed'));
    warmupResults.push(arrayReduce(arrays[i]));
    warmupResults.push(closure(1));
    warmupResults.push(constructAndRead(i));
  }

  %OptimizeFunctionOnNextCall(numericAdd);
  %OptimizeFunctionOnNextCall(loadAndCall);
  %OptimizeFunctionOnNextCall(keyedLoad);
  %OptimizeFunctionOnNextCall(arrayReduce);
  %OptimizeFunctionOnNextCall(closure);
  %OptimizeFunctionOnNextCall(constructAndRead);

  const optimizedResults = keep('optimizedResults', {
    numericAdd: numericAdd(40, 2),
    loadAndCall: loadAndCall(stableObjects[3], 5),
    keyedLoad: keyedLoad(stableObjects[4], 'fixed'),
    arrayReduce: arrayReduce([1, 2, 3, 4, 5]),
    closure: closure(2),
    constructAndRead: constructAndRead(21),
  });

  const optimizationStatusBeforeDeopt = keep('optimizationStatusBeforeDeopt', {
    numericAdd: %GetOptimizationStatus(numericAdd),
    loadAndCall: %GetOptimizationStatus(loadAndCall),
    keyedLoad: %GetOptimizationStatus(keyedLoad),
    arrayReduce: %GetOptimizationStatus(arrayReduce),
    closure: %GetOptimizationStatus(closure),
    constructAndRead: %GetOptimizationStatus(constructAndRead),
  });

  const deoptResults = keep('deoptResults', {
    loadDifferentShape: loadAndCall(differentObjects[3], 5),
    keyedStringValue: keyedLoad({fixed: 2, dynamic: 7}, 'dynamic'),
  });

  %DeoptimizeFunction(loadAndCall);
  %DeoptimizeFunction(keyedLoad);
  %DeoptimizeFunction(arrayReduce);

  const optimizationStatusAfterDeopt = keep('optimizationStatusAfterDeopt', {
    loadAndCall: %GetOptimizationStatus(loadAndCall),
    keyedLoad: %GetOptimizationStatus(keyedLoad),
    arrayReduce: %GetOptimizationStatus(arrayReduce),
  });

  keep('postDeoptResults', {
    loadAndCall: loadAndCall(stableObjects[5], 6),
    keyedLoad: keyedLoad(stableObjects[6], 'fixed'),
    arrayReduce: arrayReduce([5, 4, 3, 2, 1]),
  });

  keep('metadata', {
    warmupCount: warmupResults.length,
    stableObjectCount: stableObjects.length,
    differentObjectCount: differentObjects.length,
    statusBeforeKeys: Object.keys(optimizationStatusBeforeDeopt),
    statusAfterKeys: Object.keys(optimizationStatusAfterDeopt),
  });

  assertEquals(42, optimizedResults.numericAdd);
  assertEquals(14, optimizedResults.loadAndCall);
  assertEquals(8, optimizedResults.keyedLoad);
  assertEquals(15, optimizedResults.arrayReduce);
  assertEquals(36, optimizedResults.closure);
  assertEquals(42, optimizedResults.constructAndRead);
  assertEquals(21, deoptResults.loadDifferentShape);
  assertEquals(9, deoptResults.keyedStringValue);
  assertEquals(21, catalog.postDeoptResults.loadAndCall);
  assertEquals(12, catalog.postDeoptResults.keyedLoad);
  assertEquals(15, catalog.postDeoptResults.arrayReduce);

  globalThis.heapDumpOptimizedDeoptFeedbackDeepSample = catalog;
})();
