// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Atomics.waitAsync waiter/result objects and invalid futex paths alive
// for heap dump smoke. This follows single-thread V8 waitAsync mjsunit patterns
// while avoiding workers and long blocking waits.

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
    } catch (e) {
      keep(name, e);
      return e;
    }
    return null;
  }

  if (typeof SharedArrayBuffer !== 'function' ||
      typeof Atomics !== 'object' ||
      typeof Atomics.waitAsync !== 'function') {
    keep('unavailable', true);
    globalThis.heapDumpAtomicsWaitAsyncDeepSample = catalog;
    return;
  }

  const sharedBuffer = keep('sharedBuffer', new SharedArrayBuffer(64));
  const int32 = keep('int32', new Int32Array(sharedBuffer));
  const bigInt64 = keep('bigInt64', new BigInt64Array(sharedBuffer));
  const dataView = keep('dataView', new DataView(sharedBuffer));

  Atomics.store(int32, 0, 0);
  Atomics.store(int32, 1, 1);
  Atomics.store(bigInt64, 2, 2n);
  dataView.setInt32(12, 1234, true);

  const immediateResults = keep('immediateResults', {
    notEqual: Atomics.waitAsync(int32, 1, 0, 1000),
    timeoutZero: Atomics.waitAsync(int32, 0, 0, 0),
    bigintNotEqual: Atomics.waitAsync(bigInt64, 2, 3n, 1000),
  });

  const pendingResults = keep('pendingResults', []);
  for (let i = 0; i < 4; ++i) {
    Atomics.store(int32, i + 4, 0);
    const result = Atomics.waitAsync(int32, i + 4, 0);
    pendingResults.push({
      index: i + 4,
      result,
      promise: result.value,
      async: result.async,
    });
  }

  const notifyResults = keep('notifyResults', {
    wakeOne: Atomics.notify(int32, 4, 1),
    wakeSecond: Atomics.notify(int32, 5, 1),
    wakeThird: Atomics.notify(int32, 6, 1),
    wakeFourth: Atomics.notify(int32, 7, 1),
    wakeEmpty: Atomics.notify(int32, 10, 2),
  });

  const postNotifyWaits = keep('postNotifyWaits', [
    Atomics.waitAsync(int32, 4, 0, 0),
    Atomics.waitAsync(int32, 5, 0, 0),
  ]);

  keepError('waitAsyncBadArrayError',
            () => Atomics.waitAsync(new Int32Array(new ArrayBuffer(16)), 0, 0));
  keepError('waitAsyncBadTypeError',
            () => Atomics.waitAsync(new Uint8Array(sharedBuffer), 0, 0));
  keepError('waitAsyncBadIndexError',
            () => Atomics.waitAsync(int32, 1000, 0));
  keepError('waitBadTypeError',
            () => Atomics.wait(new Uint8Array(sharedBuffer), 0, 0, 0));
  keepError('notifyBadArrayError',
            () => Atomics.notify(new Int32Array(new ArrayBuffer(16)), 0, 1));

  keep('summary', {
    immediateKeys: Object.keys(immediateResults),
    pendingCount: pendingResults.length,
    notifiedCount: notifyResults.wakeOne + notifyResults.wakeSecond +
        notifyResults.wakeThird + notifyResults.wakeFourth,
    bufferByteLength: sharedBuffer.byteLength,
  });

  assertFalse(immediateResults.notEqual.async);
  assertEquals('not-equal', immediateResults.notEqual.value);
  assertFalse(immediateResults.timeoutZero.async);
  assertEquals('timed-out', immediateResults.timeoutZero.value);
  assertEquals('not-equal', immediateResults.bigintNotEqual.value);
  assertTrue(pendingResults[0].async);
  assertEquals(1, notifyResults.wakeOne);
  assertEquals(1, notifyResults.wakeSecond);
  assertEquals(1, notifyResults.wakeThird);
  assertEquals(1, notifyResults.wakeFourth);
  assertEquals(0, notifyResults.wakeEmpty);
  assertEquals('timed-out', postNotifyWaits[0].value);

  globalThis.heapDumpAtomicsWaitAsyncDeepSample = catalog;
})();
