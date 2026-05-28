// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --turbo-fast-api-calls --expose-fast-api --allow-natives-syntax
// Flags: --turbofan --fast-api-allow-float-in-sim --wasm-fast-api --no-liftoff
// Flags: --wasm-lazy-compilation

// Keep d8 Fast API call objects and Wasm fast-api import state alive for heap
// dump smoke. This uses small slices from V8's fast-api mjsunit coverage while
// avoiding the full stress matrix.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  assertThrows(() => d8.test.FastCAPI());
  const fastCapi = keep('fastCapi', new d8.test.FastCAPI());
  const results = keep('results', []);

  function add32() {
    return fastCapi.add_32bit_int(-42, 45);
  }
  %PrepareFunctionForOptimization(add32);
  assertEquals(3, add32());
  %OptimizeFunctionOnNextCall(add32);
  fastCapi.reset_counts();
  assertEquals(3, add32());
  assertOptimized(add32);
  assertEquals(1, fastCapi.fast_call_count());
  assertEquals(0, fastCapi.slow_call_count());
  results.push(add32);

  function add32Mismatch(a, b) {
    return fastCapi.add_32bit_int(a, b);
  }
  %PrepareFunctionForOptimization(add32Mismatch);
  assertEquals(3, add32Mismatch(-42, 45));
  %OptimizeFunctionOnNextCall(add32Mismatch);
  assertEquals(3, add32Mismatch(-42, 45));
  fastCapi.reset_counts();
  assertEquals(0, add32Mismatch(-4294967296, Symbol()));
  assertUnoptimized(add32Mismatch);
  assertEquals(0, fastCapi.fast_call_count());
  assertEquals(1, fastCapi.slow_call_count());
  results.push(add32Mismatch);

  const int32Array = keep('int32Array', new Int32Array([-8, 13, 21, -3]));
  const uint32Array = keep('uint32Array', new Uint32Array([7, 11, 19, 23]));
  function typedArrayCall() {
    return [
      fastCapi.add_all_int32_typed_array(int32Array),
      fastCapi.add_all_uint32_typed_array(uint32Array),
    ];
  }
  %PrepareFunctionForOptimization(typedArrayCall);
  const typedArrayWarmup = typedArrayCall();
  %OptimizeFunctionOnNextCall(typedArrayCall);
  const typedArrayOptimized = typedArrayCall();
  assertOptimized(typedArrayCall);
  assertEquals(typedArrayWarmup, typedArrayOptimized);
  keep('typedArrayWarmup', typedArrayWarmup);
  keep('typedArrayOptimized', typedArrayOptimized);
  results.push(typedArrayCall);

  const maxSafeFloat = 2 ** 24 - 1;
  const addAllExpected =
      -42 + 45 + Number.MIN_SAFE_INTEGER + Number.MAX_SAFE_INTEGER +
      maxSafeFloat * 0.5 + Math.PI;
  function addAll() {
    return fastCapi.add_all(
        -42, 45, Number.MIN_SAFE_INTEGER, Number.MAX_SAFE_INTEGER,
        maxSafeFloat * 0.5, Math.PI);
  }
  %PrepareFunctionForOptimization(addAll);
  assertEquals(addAllExpected, addAll());
  %OptimizeFunctionOnNextCall(addAll);
  fastCapi.reset_counts();
  assertEquals(addAllExpected, addAll());
  assertOptimized(addAll);
  if (fastCapi.supports_fp_params) {
    assertEquals(1, fastCapi.fast_call_count());
    assertEquals(0, fastCapi.slow_call_count());
  }
  results.push(addAll);

  const builder = new WasmModuleBuilder();
  const sig = makeSig(
      [kWasmI32, kWasmI32, kWasmI64, kWasmI64, kWasmF32, kWasmF64],
      [kWasmF64]);
  const importIndex = builder.addImport('mod', 'fast', sig);
  builder.addFunction('main', sig)
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kExprLocalGet, 3,
        kExprLocalGet, 4,
        kExprLocalGet, 5,
        kExprCallFunction, importIndex,
      ])
      .exportFunc();

  const wasmImports = keep('wasmImports', [
    fastCapi.add_all_no_options.bind(0),
    fastCapi.add_all_no_options.bind(%GetUndetectable()),
  ]);
  const wasmInstances = keep('wasmInstances', []);
  const wasmResults = keep('wasmResults', []);
  for (const imported of wasmImports) {
    const instance = builder.instantiate({mod: {fast: imported}});
    wasmInstances.push(instance);
    wasmResults.push(instance.exports.main(1, 2, 3n, 4n, 5, 6));
  }
  assertEquals([21, 21], wasmResults);

  keep('catalog', catalog);
  keep('builder', builder);
})();
