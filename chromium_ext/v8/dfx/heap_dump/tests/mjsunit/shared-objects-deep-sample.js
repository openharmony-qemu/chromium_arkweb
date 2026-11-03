// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-struct --shared-string-table

// Keep JS shared objects and synchronization primitives alive for heap dump
// smoke. This mirrors V8 shared-memory mjsunit coverage without workers.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  if (typeof SharedArray !== 'function' ||
      typeof SharedStructType !== 'function') {
    keep('unavailable', true);
    globalThis.heapDumpSharedObjectsDeepSample = catalog;
    return;
  }

  const InnerStruct = keep(
      'InnerStruct',
      new SharedStructType(['payload', 'label']));
  const PayloadStruct = keep(
      'PayloadStruct',
      new SharedStructType(['kind', 'value']));
  const OuterStruct = keep(
      'OuterStruct',
      new SharedStructType(['inner', 'array', 'mutex', 'condition', 'string']));
  const inner = keep('inner', new InnerStruct());
  const payload = keep('payload', new PayloadStruct());
  const outer = keep('outer', new OuterStruct());
  const sharedArray = keep('sharedArray', new SharedArray(8));
  const nestedSharedArray = keep('nestedSharedArray', new SharedArray(2));

  Atomics.store(payload, 'kind', 'inner-payload');
  Atomics.store(payload, 'value', 2026);
  Atomics.store(inner, 'payload', payload);
  Atomics.store(inner, 'label', 'heap-dump-shared-label');
  Atomics.store(sharedArray, 0, inner);
  Atomics.store(sharedArray, 1, 'heap-dump-shared-string');
  Atomics.store(sharedArray, 2, 123);
  Atomics.store(sharedArray, 3, 4.5);
  Atomics.store(sharedArray, 4, true);
  Atomics.store(nestedSharedArray, 0, sharedArray);
  Atomics.store(nestedSharedArray, 1, inner);

  const synchronization = keep('synchronization', {});
  if (typeof Atomics.Mutex === 'function') {
    synchronization.mutex = new Atomics.Mutex();
    synchronization.lockResult = Atomics.Mutex.lock(
        synchronization.mutex, () => {
          synchronization.lockEntered = true;
          return {locked: true};
        });
    synchronization.tryLockResult = Atomics.Mutex.tryLock(
        synchronization.mutex, () => {
          synchronization.tryLockEntered = true;
          return {tryLocked: true};
        });
  }
  if (typeof Atomics.Condition === 'function') {
    synchronization.condition = new Atomics.Condition();
    if (typeof Atomics.Condition.notify === 'function') {
      synchronization.notifyNoWaiters =
          Atomics.Condition.notify(synchronization.condition, 1);
    }
  }

  Atomics.store(outer, 'inner', inner);
  Atomics.store(outer, 'array', sharedArray);
  if (synchronization.mutex) {
    Atomics.store(outer, 'mutex', synchronization.mutex);
  }
  if (synchronization.condition) {
    Atomics.store(outer, 'condition', synchronization.condition);
  }
  Atomics.store(outer, 'string', 'heap-dump-outer-shared-string');

  const brandChecks = keep('brandChecks', {
    sharedArray: SharedArray.isSharedArray(sharedArray),
    innerStruct: SharedStructType.isSharedStruct(inner),
    outerStruct: SharedStructType.isSharedStruct(outer),
    sharedArrayInstance: sharedArray instanceof SharedArray,
    innerInstance: inner instanceof InnerStruct,
    outerInstance: outer instanceof OuterStruct,
  });

  const atomicsResults = keep('atomicsResults', {
    array0: Atomics.load(sharedArray, 0),
    array1: Atomics.load(sharedArray, 1),
    array2: Atomics.load(sharedArray, 2),
    innerPayload: Atomics.load(inner, 'payload'),
    outerInner: Atomics.load(outer, 'inner'),
    outerArray: Atomics.load(outer, 'array'),
    compareExchangeArray: Atomics.compareExchange(
        sharedArray, 5, undefined, 'compare-exchanged'),
    exchangeArray: Atomics.exchange(sharedArray, 6, 'exchanged'),
  });

  assertTrue(brandChecks.sharedArray);
  assertTrue(brandChecks.innerStruct);
  assertTrue(brandChecks.outerStruct);
  assertTrue(brandChecks.sharedArrayInstance);
  assertTrue(brandChecks.innerInstance);
  assertTrue(brandChecks.outerInstance);
  assertSame(inner, atomicsResults.array0);
  assertEquals('heap-dump-shared-string', atomicsResults.array1);
  assertEquals(123, atomicsResults.array2);
  assertEquals('inner-payload', Atomics.load(atomicsResults.innerPayload, 'kind'));
  assertEquals(2026, Atomics.load(atomicsResults.innerPayload, 'value'));
  assertSame(inner, atomicsResults.outerInner);
  assertSame(sharedArray, atomicsResults.outerArray);
  assertEquals(undefined, atomicsResults.compareExchangeArray);
  assertEquals(undefined, atomicsResults.exchangeArray);
  assertEquals('compare-exchanged', Atomics.load(sharedArray, 5));
  assertEquals('exchanged', Atomics.load(sharedArray, 6));
  if (synchronization.mutex) {
    assertTrue(synchronization.lockEntered);
    assertTrue(synchronization.tryLockEntered);
    assertTrue(synchronization.lockResult.locked);
    assertTrue(synchronization.tryLockResult.success);
  }
  if (synchronization.condition) {
    assertEquals(0, synchronization.notifyNoWaiters);
  }

  globalThis.heapDumpSharedObjectsDeepSample = catalog;
})();
