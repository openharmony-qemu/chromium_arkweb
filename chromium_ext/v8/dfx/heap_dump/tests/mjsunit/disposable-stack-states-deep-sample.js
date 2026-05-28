// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep explicit-resource-management stack states alive for heap dump smoke.
// This complements the broad iterator/disposable sample with moved, disposed,
// deferred/adopted/used, async-dispose, and SuppressedError states.

const keepAlive = [];
globalThis.__heapDumpDisposableStackStatesKeepAlive = keepAlive;

function keep(name, value) {
  keepAlive.push({name, value});
  return value;
}

(function KeepDisposableStackStates() {
  if (typeof DisposableStack !== 'function') return;

  const log = keep('syncDisposeLog', []);
  const source = keep('syncSourceStack', new DisposableStack());
  source.defer(() => log.push('source-defer'));
  source.adopt({kind: 'adopted-resource'}, (value) => {
    log.push(['adopt', value.kind]);
  });
  if (typeof Symbol.dispose === 'symbol') {
    source.use({
      kind: 'used-resource',
      [Symbol.dispose]() {
        log.push('symbol-dispose');
      },
    });
  }

  const moved = keep('syncMovedStack', source.move());
  assertTrue(source.disposed);
  assertFalse(moved.disposed);
  moved.dispose();
  assertTrue(moved.disposed);
  assertTrue(log.length >= 2);

  let suppressed = null;
  try {
    const throwing = keep('throwingStack', new DisposableStack());
    throwing.defer(() => {
      throw new Error('heap-dump-dispose-first');
    });
    throwing.defer(() => {
      throw new Error('heap-dump-dispose-second');
    });
    throwing.dispose();
  } catch (error) {
    suppressed = keep('suppressedError', error);
  }

  assertTrue(suppressed instanceof SuppressedError);
  assertTrue(suppressed.error instanceof Error);
  assertTrue(suppressed.suppressed instanceof Error);
})();

(function KeepAsyncDisposableStackStates() {
  if (typeof AsyncDisposableStack !== 'function') return;

  const log = keep('asyncDisposeLog', []);
  const source = keep('asyncSourceStack', new AsyncDisposableStack());
  source.defer(async () => log.push('async-defer'));
  source.adopt({kind: 'async-adopted-resource'}, async (value) => {
    log.push(['async-adopt', value.kind]);
  });
  if (typeof Symbol.asyncDispose === 'symbol') {
    source.use({
      kind: 'async-used-resource',
      async [Symbol.asyncDispose]() {
        log.push('async-symbol-dispose');
      },
    });
  }

  const moved = keep('asyncMovedStack', source.move());
  assertTrue(source.disposed);
  assertFalse(moved.disposed);
  const disposePromise = keep('asyncDisposePromise', moved.disposeAsync());
  disposePromise.then(
      () => keep('asyncDisposeFulfilled', log.slice()),
      (error) => keep('asyncDisposeRejected', error));
  assertTrue(disposePromise instanceof Promise);
})();
