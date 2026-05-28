// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep weak-reference and finalization-registration shapes alive for heap dump
// smoke without depending on GC cleanup timing.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const cleanupLog = keep('cleanupLog', []);
  const strongTargets = keep('strongTargets', []);
  const heldValues = keep('heldValues', []);
  const unregisterTokens = keep('unregisterTokens', []);

  for (let i = 0; i < 40; ++i) {
    strongTargets.push({
      index: i,
      payload: {kind: 'weak-target-' + i},
    });
    heldValues.push({
      heldIndex: i,
      nested: {kind: 'held-value-' + i},
    });
    unregisterTokens.push({
      tokenIndex: i,
      nested: {kind: 'token-' + i},
    });
  }

  const weakRefs = keep('weakRefs', []);
  if (typeof WeakRef === 'function') {
    for (let i = 0; i < strongTargets.length; ++i) {
      weakRefs.push(new WeakRef(strongTargets[i]));
    }
  }

  const finalizationRegistries = keep('finalizationRegistries', []);
  if (typeof FinalizationRegistry === 'function') {
    const primaryRegistry = new FinalizationRegistry((held) => {
      cleanupLog.push(['primary', held]);
    });
    const secondaryRegistry = new FinalizationRegistry((held) => {
      cleanupLog.push(['secondary', held]);
    });
    const proxyCleanup = new Proxy(function proxyCleanup(held) {
      cleanupLog.push(['proxy', held]);
    }, {
      apply(target, receiver, args) {
        cleanupLog.push(['proxy-apply', args.length]);
        return Reflect.apply(target, receiver, args);
      },
    });
    const proxyRegistry = new FinalizationRegistry(proxyCleanup);

    for (let i = 0; i < strongTargets.length; ++i) {
      primaryRegistry.register(
          strongTargets[i], heldValues[i], unregisterTokens[i]);
      if (i % 2 === 0) {
        secondaryRegistry.register(
            strongTargets[i], {secondaryHeld: i}, unregisterTokens[i]);
      }
      if (i % 5 === 0) {
        proxyRegistry.register(
            strongTargets[i], {proxyHeld: i}, unregisterTokens[i]);
      }
    }

    const unregisterResult = primaryRegistry.unregister(unregisterTokens[3]);
    keep('unregisterResult', unregisterResult);
    finalizationRegistries.push(primaryRegistry, secondaryRegistry, proxyRegistry);
  }

  const weakMap = keep('weakMap', new WeakMap());
  const weakSet = keep('weakSet', new WeakSet());
  const functionTargets = keep('functionTargets', []);
  const proxyTargets = keep('proxyTargets', []);

  for (let i = 0; i < 12; ++i) {
    function functionTarget() {
      return i;
    }
    functionTarget.meta = {kind: 'function-target-' + i};
    functionTargets.push(functionTarget);
    weakMap.set(functionTarget, {functionValue: i});
    weakSet.add(functionTarget);

    const proxyTarget = new Proxy({proxyIndex: i}, {
      get(target, key, receiver) {
        return Reflect.get(target, key, receiver);
      },
    });
    proxyTargets.push(proxyTarget);
    weakMap.set(proxyTarget, {proxyValue: i});
    weakSet.add(proxyTarget);
  }

  for (let i = 0; i < strongTargets.length; ++i) {
    weakMap.set(strongTargets[i], heldValues[i]);
    weakSet.add(strongTargets[i]);
  }

  const weakSymbolTargets = keep('weakSymbolTargets', []);
  if (typeof WeakRef === 'function' &&
      typeof FinalizationRegistry === 'function') {
    try {
      const symbolTarget = Symbol('heap-dump-weak-symbol-target');
      const symbolToken = {kind: 'symbol-token'};
      const symbolRegistry = new FinalizationRegistry((held) => {
        cleanupLog.push(['symbol', held]);
      });
      weakSymbolTargets.push(symbolTarget);
      weakRefs.push(new WeakRef(symbolTarget));
      symbolRegistry.register(
          symbolTarget, {symbolHeld: true}, symbolToken);
      finalizationRegistries.push(symbolRegistry);
      keep('symbolUnregisterToken', symbolToken);
    } catch (e) {
      keep('weakSymbolTargetError', e);
    }
  }

  const nestedWeakCollections = keep('nestedWeakCollections', {
    weakMap,
    weakSet,
    weakRefs,
    finalizationRegistries,
  });

  assertEquals(40, strongTargets.length);
  assertEquals(40, heldValues.length);
  assertEquals(40, unregisterTokens.length);
  assertTrue(weakMap.has(strongTargets[0]));
  assertTrue(weakSet.has(strongTargets[1]));
  assertEquals(0, weakMap.get(functionTargets[0]).functionValue);
  assertTrue(weakSet.has(proxyTargets[0]));
  if (weakRefs.length > 0 && typeof weakRefs[0].deref === 'function') {
    assertSame(strongTargets[0], weakRefs[0].deref());
  }
  if (finalizationRegistries.length > 0) {
    assertTrue(finalizationRegistries[0] instanceof FinalizationRegistry);
  }
  assertSame(weakMap, nestedWeakCollections.weakMap);

  globalThis.heapDumpWeakRefsFinalizationDeepSample = catalog;
})();
