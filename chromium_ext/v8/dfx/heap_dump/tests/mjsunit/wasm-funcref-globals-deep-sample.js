// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Wasm funcref globals, ref.func element expressions, table.get wrappers,
// and multi-value funcref results alive for heap dump smoke.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const builder = new WasmModuleBuilder();
  const idSig = builder.addType(kSig_i_i);
  const addSig = builder.addType(kSig_i_i);
  const id = builder.addFunction('id', idSig)
      .addBody([kExprLocalGet, 0])
      .exportFunc();
  const addSeven = builder.addFunction('addSeven', addSig)
      .addBody([kExprLocalGet, 0, kExprI32Const, 7, kExprI32Add])
      .exportFunc();

  const funGlobal = builder.addGlobal(
      kWasmFuncRef, false, false, [kExprRefFunc, id.index]);
  funGlobal.exportAs('funGlobal');

  const table = builder.addTable(kWasmFuncRef, 4, 8);
  table.exportAs('table');
  builder.addActiveElementSegment(
      table.index, wasmI32Const(0),
      [
        [kExprGlobalGet, funGlobal.index],
        [kExprRefFunc, id.index],
        [kExprRefFunc, addSeven.index],
      ],
      kWasmFuncRef);

  builder.addFunction('getGlobalFun', makeSig([], [kWasmFuncRef]))
      .addBody([kExprGlobalGet, funGlobal.index])
      .exportFunc();
  builder.addFunction('getTableFun', makeSig([kWasmI32], [kWasmFuncRef]))
      .addBody([kExprLocalGet, 0, kExprTableGet, table.index])
      .exportFunc();
  builder.addFunction(
             'returnManyFuncrefs',
             makeSig([], [kWasmFuncRef, kWasmFuncRef, kWasmFuncRef,
                          kWasmFuncRef]))
      .addBody([
        kExprRefFunc, id.index,
        kExprGlobalGet, funGlobal.index,
        kExprI32Const, 0, kExprTableGet, table.index,
        kExprI32Const, 2, kExprTableGet, table.index,
      ])
      .exportFunc();
  const module = keep('module', builder.toModule());
  const instance = keep('instance', new WebAssembly.Instance(module, {}));
  const exports = keep('exports', instance.exports);
  const globalFunction = keep('globalFunction', exports.funGlobal.value);
  const tableEntries = keep('tableEntries', [
    exports.table.get(0),
    exports.table.get(1),
    exports.table.get(2),
  ]);
  const returnedFunctions = keep(
      'returnedFunctions', exports.returnManyFuncrefs());
  const tableFunctionFromWasm = keep('tableFunctionFromWasm',
                                     exports.getTableFun(2));
  const globalFunctionFromWasm = keep('globalFunctionFromWasm',
                                      exports.getGlobalFun());
  const nullSlot = keep('nullSlot', exports.table.get(3));

  const results = keep('results', {
    globalDirect: globalFunction(11),
    table0Direct: tableEntries[0](12),
    table2Direct: tableEntries[2](13),
    returned0: returnedFunctions[0](14),
    returned1: returnedFunctions[1](15),
    returned2: returnedFunctions[2](16),
    returned3: returnedFunctions[3](17),
    tableFunctionFromWasm: tableFunctionFromWasm(21),
    globalFunctionFromWasm: globalFunctionFromWasm(22),
  });

  assertEquals('function', typeof globalFunction);
  assertEquals(null, nullSlot);
  assertEquals(11, results.globalDirect);
  assertEquals(12, results.table0Direct);
  assertEquals(20, results.table2Direct);
  assertEquals(14, results.returned0);
  assertEquals(15, results.returned1);
  assertEquals(16, results.returned2);
  assertEquals(24, results.returned3);
  assertEquals(28, results.tableFunctionFromWasm);
  assertEquals(22, results.globalFunctionFromWasm);

  globalThis.heapDumpWasmFuncRefGlobalsDeepSample = catalog;
})();
