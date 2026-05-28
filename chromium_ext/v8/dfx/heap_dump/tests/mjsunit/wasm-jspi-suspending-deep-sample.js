// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep JS Promise Integration Wasm suspending/promising wrapper shapes alive
// for heap dump smoke when the current build exposes JSPI.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  if (typeof WebAssembly.Suspending !== 'function' ||
      typeof WebAssembly.promising !== 'function') {
    globalThis.heapDumpWasmJspiSuspendingDeepSample = {
      unavailable: true,
    };
    return;
  }

  function makePending(label) {
    let resolve;
    let reject;
    const promise = new Promise((res, rej) => {
      resolve = res;
      reject = rej;
    });
    return {label, promise, resolve, reject};
  }

  const importCalls = keep('importCalls', []);
  const pendingControls = keep('pendingControls', []);

  const builder = new WasmModuleBuilder();
  const suspendImport = builder.addImport('m', 'suspend', kSig_i_i);
  builder.addFunction('callSuspend', kSig_i_i)
      .addBody([kExprLocalGet, 0, kExprCallFunction, suspendImport])
      .exportFunc();
  builder.addFunction('addOne', kSig_i_i)
      .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])
      .exportFunc();

  const pendingSuspending = keep(
      'pendingSuspending',
      new WebAssembly.Suspending((value) => {
        const control = makePending('pending-' + value);
        pendingControls.push(control);
        importCalls.push({kind: 'pending', value, control});
        return control.promise.then(() => value + 10);
      }));
  const resolvedSuspending = keep(
      'resolvedSuspending',
      new WebAssembly.Suspending((value) => {
        importCalls.push({kind: 'resolved', value});
        return Promise.resolve(value + 20);
      }));
  const proxySuspending = keep(
      'proxySuspending',
      new WebAssembly.Suspending(new Proxy((value) => {
        importCalls.push({kind: 'proxy', value});
        return Promise.resolve(value + 30);
      }, {
        apply(target, receiver, args) {
          importCalls.push({kind: 'proxy-apply', args});
          return Reflect.apply(target, receiver, args);
        },
      })));

  const pendingInstance = keep(
      'pendingInstance',
      builder.instantiate({m: {suspend: pendingSuspending}}));
  const resolvedInstance = keep(
      'resolvedInstance',
      builder.instantiate({m: {suspend: resolvedSuspending}}));
  const proxyInstance = keep(
      'proxyInstance',
      builder.instantiate({m: {suspend: proxySuspending}}));

  const pendingWrapper =
      keep('pendingWrapper',
           WebAssembly.promising(pendingInstance.exports.callSuspend));
  const resolvedWrapper =
      keep('resolvedWrapper',
           WebAssembly.promising(resolvedInstance.exports.callSuspend));
  const proxyWrapper =
      keep('proxyWrapper',
           WebAssembly.promising(proxyInstance.exports.callSuspend));
  const addOneWrapper =
      keep('addOneWrapper',
           WebAssembly.promising(pendingInstance.exports.addOne));

  const pendingPromise = keep('pendingPromise', pendingWrapper(5));
  const resolvedPromise = keep('resolvedPromise', resolvedWrapper(7));
  const proxyPromise = keep('proxyPromise', proxyWrapper(11));
  const addOnePromise = keep('addOnePromise', addOneWrapper(41));
  const allResults = keep(
      'allResults',
      Promise.all([resolvedPromise, proxyPromise, addOnePromise]));

  const rejectedSuspending = keep(
      'rejectedSuspending',
      new WebAssembly.Suspending((value) => {
        importCalls.push({kind: 'rejected', value});
        return Promise.reject(new Error('jspi-rejected-' + value));
      }));
  const rejectedInstance = keep(
      'rejectedInstance',
      builder.instantiate({m: {suspend: rejectedSuspending}}));
  const rejectedWrapper =
      keep('rejectedWrapper',
           WebAssembly.promising(rejectedInstance.exports.callSuspend));
  const rejectedPromise = keep(
      'rejectedPromise',
      rejectedWrapper(13).catch((error) => ({error})));

  assertTrue(pendingSuspending instanceof WebAssembly.Suspending);
  assertTrue(pendingPromise instanceof Promise);
  assertTrue(resolvedPromise instanceof Promise);
  assertTrue(proxyPromise instanceof Promise);
  assertTrue(addOnePromise instanceof Promise);
  assertTrue(allResults instanceof Promise);
  assertTrue(rejectedPromise instanceof Promise);
  assertEquals(1, pendingControls.length);

  globalThis.heapDumpWasmJspiSuspendingDeepSample = catalog;
})();
