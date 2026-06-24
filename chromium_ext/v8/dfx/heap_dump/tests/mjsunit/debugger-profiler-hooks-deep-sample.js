// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep debugger/profiler/promise-hook-adjacent execution metadata alive for
// heap dump smoke. This follows V8 mjsunit patterns that enable d8.debugger
// without depending on debugger-suite-only helper APIs or native syntax.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const debugState = keep('debugState', {
    enabled: false,
    calls: [],
    errors: [],
  });

  if (typeof d8 === 'object' && d8.debugger &&
      typeof d8.debugger.enable === 'function') {
    d8.debugger.enable();
    debugState.enabled = true;
  }

  function debuggerLeaf(seed) {
    const local = {kind: 'debugger-local', seed};
    debugger;
    debugState.calls.push(local);
    return local.seed + 1;
  }

  function debuggerMiddle(seed) {
    const middleLocal = {kind: 'debugger-middle', seed};
    return debuggerLeaf(middleLocal.seed) + middleLocal.seed;
  }

  async function debuggerAsync(seed) {
    const asyncLocal = {kind: 'debugger-async-local', seed};
    debugger;
    await Promise.resolve({kind: 'debugger-await', seed});
    return asyncLocal;
  }

  function debuggerThrow(seed) {
    try {
      debugger;
      throw new Error('heap-dump-debugger-error-' + seed);
    } catch (error) {
      debugState.errors.push(error);
      return error;
    }
  }

  keep('debuggerLeaf', debuggerLeaf);
  keep('debuggerMiddle', debuggerMiddle);
  keep('debuggerAsync', debuggerAsync);
  keep('debuggerThrow', debuggerThrow);
  keep('debuggerResults', [
    debuggerMiddle(3),
    debuggerLeaf(11),
    debuggerThrow(7),
  ]);
  keep('debuggerAsyncPromise', debuggerAsync(19));

  const structuredState = keep('structuredState', {});
  const previousPrepareStackTrace = Error.prepareStackTrace;
  Error.prepareStackTrace = function(error, frames) {
    structuredState.error = error;
    structuredState.frames = frames;
    structuredState.topFrame = frames[0];
    return frames;
  };
  try {
    keep('debuggerStructuredStack', debuggerThrow(23).stack);
  } finally {
    Error.prepareStackTrace = previousPrepareStackTrace;
  }

  const profilerState = keep('profilerState', {
    callbacks: [],
    samples: 0,
  });
  if (typeof d8 === 'object' && d8.profiler) {
    if (typeof d8.profiler.setOnProfileEndListener === 'function') {
      d8.profiler.setOnProfileEndListener((profile) => {
        profilerState.callbacks.push(profile);
      });
    }
    if (typeof d8.profiler.triggerSample === 'function') {
      for (let i = 0; i < 16; ++i) {
        debuggerMiddle(i);
        d8.profiler.triggerSample();
        ++profilerState.samples;
      }
    }
  }

  const promiseHookState = keep('promiseHookState', {
    enabled: false,
    init: [],
    before: [],
    after: [],
    resolve: [],
    error: null,
  });
  function makePending(label) {
    let resolve;
    let reject;
    const promise = new Promise((res, rej) => {
      resolve = res;
      reject = rej;
    });
    return {label, promise, resolve, reject};
  }

  if (typeof d8 === 'object' && d8.promise &&
      typeof d8.promise.setHooks === 'function') {
    try {
      d8.promise.setHooks(
          (promise, parent) => promiseHookState.init.push({promise, parent}),
          (promise) => promiseHookState.before.push(promise),
          (promise) => promiseHookState.after.push(promise),
          (promise) => promiseHookState.resolve.push(promise));
      promiseHookState.enabled = true;
    } catch (e) {
      promiseHookState.error = e;
    }
  }

  const hookRoots = keep('hookRoots', []);
  for (let i = 0; i < 8; ++i) {
    const root = makePending('hook-root-' + i);
    hookRoots.push({
      root,
      then: root.promise.then((value) => ({i, value, kind: 'then'})),
      catch: root.promise.catch((reason) => ({i, reason, kind: 'catch'})),
      finally: root.promise.finally(() => ({i, kind: 'finally'})),
    });
  }
  keep('hookResolvedChain',
       Promise.resolve({kind: 'hook-resolved'}).then((value) => value));
  keep('hookRejectedChain',
       Promise.reject(new Error('hook-rejected')).catch((error) => error));

  if (promiseHookState.enabled) {
    d8.promise.setHooks();
  }

  assertTrue(debugState.enabled);
  assertEquals(7, catalog.debuggerResults[0]);
  assertEquals(12, catalog.debuggerResults[1]);
  assertTrue(catalog.debuggerResults[2] instanceof Error);
  assertTrue(catalog.debuggerAsyncPromise instanceof Promise);
  assertTrue(Array.isArray(structuredState.frames));
  assertEquals(16, profilerState.samples);
  assertEquals(8, hookRoots.length);
  assertTrue(catalog.hookResolvedChain instanceof Promise);
  assertTrue(catalog.hookRejectedChain instanceof Promise);

  globalThis.heapDumpDebuggerProfilerHooksDeepSample = catalog;
})();
