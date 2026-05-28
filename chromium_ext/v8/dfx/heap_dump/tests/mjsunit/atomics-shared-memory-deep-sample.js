// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep SharedArrayBuffer, Atomics, and optional synchronization primitive
// shapes alive for heap dump smoke without workers or long blocking waits.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  if (typeof SharedArrayBuffer !== 'function' || typeof Atomics !== 'object') {
    keep('unavailable', true);
    globalThis.heapDumpAtomicsSharedMemoryDeepSample = catalog;
    return;
  }

  const sharedBuffer = keep('sharedBuffer', new SharedArrayBuffer(256));
  const int8 = keep('int8', new Int8Array(sharedBuffer, 0, 32));
  const uint8 = keep('uint8', new Uint8Array(sharedBuffer, 32, 32));
  const int16 = keep('int16', new Int16Array(sharedBuffer, 64, 16));
  const uint16 = keep('uint16', new Uint16Array(sharedBuffer, 96, 16));
  const int32 = keep('int32', new Int32Array(sharedBuffer, 128, 16));
  const bigInt64 = keep('bigInt64', new BigInt64Array(sharedBuffer, 192, 4));
  const bigUint64 = keep('bigUint64', new BigUint64Array(sharedBuffer, 224, 4));
  const dataView = keep('dataView', new DataView(sharedBuffer, 16, 96));

  int8[0] = -8;
  uint8[0] = 8;
  int16[0] = -16;
  uint16[0] = 16;
  int32[0] = 32;
  bigInt64[0] = -64n;
  bigUint64[0] = 64n;
  dataView.setUint32(0, 0x12345678, true);

  const atomicsResults = keep('atomicsResults', {
    store: Atomics.store(int32, 1, 10),
    load: Atomics.load(int32, 1),
    add: Atomics.add(int32, 1, 5),
    sub: Atomics.sub(int32, 1, 3),
    and: Atomics.and(int32, 1, 0xff),
    or: Atomics.or(int32, 1, 0x100),
    xor: Atomics.xor(int32, 1, 0x10),
    exchange: Atomics.exchange(int32, 2, 22),
    compareExchange: Atomics.compareExchange(int32, 2, 22, 33),
    isLockFree4: Atomics.isLockFree(4),
  });

  const bigAtomicsResults = keep('bigAtomicsResults', {
    store: Atomics.store(bigInt64, 1, 100n),
    add: Atomics.add(bigInt64, 1, 23n),
    compareExchange: Atomics.compareExchange(bigInt64, 1, 123n, 456n),
    load: Atomics.load(bigInt64, 1),
  });

  const waitResults = keep('waitResults', {});
  try {
    waitResults.notEqual = Atomics.wait(int32, 3, 1, 0);
  } catch (e) {
    waitResults.waitError = e;
  }
  try {
    waitResults.notifyNoWaiters = Atomics.notify(int32, 3, 1);
  } catch (e) {
    waitResults.notifyError = e;
  }
  try {
    if (typeof Atomics.waitAsync === 'function') {
      waitResults.waitAsyncNotEqual = Atomics.waitAsync(int32, 3, 1, 0);
      waitResults.waitAsyncTimeout = Atomics.waitAsync(int32, 4, 0, 0);
    }
  } catch (e) {
    waitResults.waitAsyncError = e;
  }

  const growableShared = keep('growableShared', (() => {
    try {
      const buffer = new SharedArrayBuffer(64, {maxByteLength: 256});
      const fixed = new Int32Array(buffer, 0, 4);
      const tracking = new Uint8Array(buffer);
      fixed[0] = 7;
      tracking[20] = 8;
      if (typeof buffer.grow === 'function') {
        buffer.grow(128);
        tracking[80] = 9;
      }
      return {buffer, fixed, tracking};
    } catch (e) {
      return {error: e};
    }
  })());

  const synchronization = keep('synchronization', {});
  try {
    if (typeof Atomics.Mutex === 'function') {
      const mutex = new Atomics.Mutex();
      synchronization.mutex = mutex;
      synchronization.lockResult = Atomics.Mutex.lock(mutex, () => {
        synchronization.lockEntered = true;
        return {locked: true};
      });
      synchronization.tryLockResult = Atomics.Mutex.tryLock(mutex, () => {
        synchronization.tryLockEntered = true;
        return {tryLocked: true};
      });
      if (typeof Atomics.Mutex.lockWithTimeout === 'function') {
        synchronization.lockWithTimeoutResult =
            Atomics.Mutex.lockWithTimeout(mutex, () => ({timeoutLocked: true}), 0);
      }
    }
  } catch (e) {
    synchronization.mutexError = e;
  }
  try {
    if (typeof Atomics.Condition === 'function') {
      const condition = new Atomics.Condition();
      synchronization.condition = condition;
      if (typeof Atomics.Condition.notify === 'function') {
        synchronization.notifyNoWaiters = Atomics.Condition.notify(condition, 1);
      }
    }
  } catch (e) {
    synchronization.conditionError = e;
  }

  assertEquals(10, atomicsResults.load);
  assertEquals(10, atomicsResults.add);
  assertEquals(15, atomicsResults.sub);
  assertEquals(12, atomicsResults.and);
  assertEquals(12, atomicsResults.or);
  assertEquals(268, atomicsResults.xor);
  assertEquals(22, atomicsResults.compareExchange);
  assertEquals(100n, bigAtomicsResults.add);
  assertEquals(123n, bigAtomicsResults.compareExchange);
  assertEquals(456n, bigAtomicsResults.load);
  assertEquals('not-equal', waitResults.notEqual);
  assertEquals(0, waitResults.notifyNoWaiters);
  if (waitResults.waitAsyncNotEqual) {
    assertEquals('not-equal', waitResults.waitAsyncNotEqual.value);
  }
  if (synchronization.mutex) {
    assertTrue(synchronization.lockEntered);
    assertTrue(synchronization.tryLockEntered);
    assertTrue(synchronization.lockResult.locked);
    assertTrue(synchronization.tryLockResult.success);
  }
  if (synchronization.condition) {
    assertEquals(0, synchronization.notifyNoWaiters);
  }

  globalThis.heapDumpAtomicsSharedMemoryDeepSample = catalog;
})();
