// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Wasm function/import/export/table wrapper shapes alive for heap dump
// smoke. This focuses on imported tables, exported wasm functions, re-exported
// JS imports, optional WebAssembly.Function wrappers, table mutation, and
// indirect calls.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function addAddConstFunction(builder, name, value) {
    return builder.addFunction(name, kSig_i_i)
        .addBody([kExprLocalGet, 0, kExprI32Const, value, kExprI32Add])
        .exportFunc()
        .index;
  }

  const providerBuilder = new WasmModuleBuilder();
  const plusOne = addAddConstFunction(providerBuilder, 'plusOne', 1);
  const plusTwo = addAddConstFunction(providerBuilder, 'plusTwo', 2);
  const plusThree = addAddConstFunction(providerBuilder, 'plusThree', 3);
  const providerTable =
      providerBuilder.addTable(kWasmFuncRef, 4, 8).exportAs('table').index;
  providerBuilder.addActiveElementSegment(
      providerTable, wasmI32Const(0), [plusOne, plusTwo, plusThree]);
  const providerModule = keep('providerModule', providerBuilder.toModule());
  const providerInstance =
      keep('providerInstance', new WebAssembly.Instance(providerModule, {}));
  const table = keep('table', providerInstance.exports.table);

  const consumerBuilder = new WasmModuleBuilder();
  const indirectSig = consumerBuilder.addType(kSig_i_i);
  const importedTable =
      consumerBuilder.addImportedTable('imports', 'table', 4, 8, kWasmFuncRef);
  const importedJsFunction =
      consumerBuilder.addImport('imports', 'jsAdd', kSig_i_i);
  consumerBuilder.addExport('reexportedJsAdd', importedJsFunction);
  consumerBuilder.addFunction('callAt', makeSig([kWasmI32, kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 1,
        kExprLocalGet, 0,
        kExprCallIndirect, indirectSig, importedTable,
      ])
      .exportFunc();
  const consumerModule = keep('consumerModule', consumerBuilder.toModule());
  const jsAdd = keep('jsAdd', function jsAdd(value) {
    return value + 20;
  });
  const consumerInstance = keep(
      'consumerInstance',
      new WebAssembly.Instance(consumerModule, {
        imports: {
          table,
          jsAdd,
        },
      }));

  const optional = keep('optional', {});
  if (typeof WebAssembly.Function === 'function') {
    optional.jsFunctionWrapper = new WebAssembly.Function(
        {parameters: ['i32'], results: ['i32']}, (value) => value + 10);
    table.set(3, optional.jsFunctionWrapper);
  } else {
    table.set(3, providerInstance.exports.plusThree);
  }

  const tableGrowth = keep('tableGrowth', {
    before: table.length,
    delta: table.grow(1),
  });
  table.set(4, providerInstance.exports.plusTwo);
  tableGrowth.after = table.length;

  const reexportBuilder = new WasmModuleBuilder();
  const reexportImport = reexportBuilder.addImport('m', 'fun', kSig_i_i);
  reexportBuilder.addExport('fun', reexportImport);
  const reexportModule = keep('reexportModule', reexportBuilder.toModule());
  const reexportInstance = keep(
      'reexportInstance',
      new WebAssembly.Instance(reexportModule, {m: {fun: jsAdd}}));

  const results = keep('results', {
    directPlusOne: providerInstance.exports.plusOne(4),
    directPlusTwo: providerInstance.exports.plusTwo(4),
    indirectPlusOne: consumerInstance.exports.callAt(0, 4),
    indirectPlusTwo: consumerInstance.exports.callAt(1, 4),
    indirectPlusThree: consumerInstance.exports.callAt(2, 4),
    indirectSlotThree: consumerInstance.exports.callAt(3, 4),
    indirectGrownSlot: consumerInstance.exports.callAt(4, 4),
    reexportedImport: consumerInstance.exports.reexportedJsAdd(4),
    reexportedJsFunction: reexportInstance.exports.fun(4),
  });

  keep('exportedFunctions', {
    plusOne: providerInstance.exports.plusOne,
    plusTwo: providerInstance.exports.plusTwo,
    plusThree: providerInstance.exports.plusThree,
    callAt: consumerInstance.exports.callAt,
    reexportedJsAdd: consumerInstance.exports.reexportedJsAdd,
    reexportedJsFunction: reexportInstance.exports.fun,
  });
  keep('tableEntries', [
    table.get(0),
    table.get(1),
    table.get(2),
    table.get(3),
    table.get(4),
  ]);

  assertEquals(5, results.directPlusOne);
  assertEquals(6, results.directPlusTwo);
  assertEquals(5, results.indirectPlusOne);
  assertEquals(6, results.indirectPlusTwo);
  assertEquals(7, results.indirectPlusThree);
  assertEquals(typeof WebAssembly.Function === 'function' ? 14 : 7,
               results.indirectSlotThree);
  assertEquals(6, results.indirectGrownSlot);
  assertEquals(24, results.reexportedImport);
  assertEquals(24, results.reexportedJsFunction);
  assertEquals(4, tableGrowth.before);
  assertEquals(5, tableGrowth.after);

  globalThis.heapDumpWasmFunctionTableDeepSample = catalog;
})();
