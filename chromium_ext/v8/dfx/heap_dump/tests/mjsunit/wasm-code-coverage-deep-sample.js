// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --wasm-code-coverage --wasm-staging

// Keep Wasm code-coverage data alive for heap dump smoke. This is a compact
// version of V8's wasm-code-coverage mjsunit patterns.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const imports = keep('imports', {
    env: {
      bump(value) {
        return (value + 3) | 0;
      },
    },
  });

  const builder = keep('builder', new WasmModuleBuilder());
  const bumpSig = builder.addType(kSig_i_i);
  const bumpImport = builder.addImport('env', 'bump', bumpSig);

  builder.addFunction('branch', kSig_i_i)
      .addBody([
        kExprLocalGet, 0,
        kExprI32Eqz,
        kExprIf, kWasmI32,
          kExprI32Const, 11,
        kExprElse,
          kExprLocalGet, 0,
          kExprI32Const, 2,
          kExprI32Mul,
        kExprEnd,
      ])
      .exportFunc();

  builder.addFunction('loop_sum', kSig_i_i)
      .addLocals(kWasmI32, 1)
      .addBody([
        kExprBlock, kWasmVoid,
          kExprLoop, kWasmVoid,
            kExprLocalGet, 0,
            kExprI32Eqz,
            kExprBrIf, 1,
            kExprLocalGet, 1,
            kExprLocalGet, 0,
            kExprI32Add,
            kExprLocalSet, 1,
            kExprLocalGet, 0,
            kExprI32Const, 1,
            kExprI32Sub,
            kExprLocalSet, 0,
            kExprBr, 0,
          kExprEnd,
        kExprEnd,
        kExprLocalGet, 1,
      ])
      .exportFunc();

  builder.addFunction('call_import', kSig_i_i)
      .addBody([
        kExprLocalGet, 0,
        kExprCallFunction, bumpImport,
      ])
      .exportFunc();

  const instance = keep('instance', builder.instantiate(imports));
  const exports = keep('exports', instance.exports);
  assertEquals(11, exports.branch(0));
  assertEquals(14, exports.branch(7));
  assertEquals(15, exports.loop_sum(5));
  assertEquals(12, exports.call_import(9));

  const wasmCoverage = keep('wasmCoverage', %DebugCollectWasmCoverage());
  assertTrue(Array.isArray(wasmCoverage));
  assertTrue(wasmCoverage.length > 0);
  assertTrue(wasmCoverage[wasmCoverage.length - 1].length >= 3);

  keep('catalog', catalog);
})();
