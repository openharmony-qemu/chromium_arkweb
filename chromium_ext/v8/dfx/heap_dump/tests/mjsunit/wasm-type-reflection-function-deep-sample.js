// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-type-reflection

// Keep WebAssembly.Function/type-reflection wrapper shapes alive for heap dump
// smoke. This follows V8's type-reflection and wasm-to-js wrapper mjsunit
// patterns, but retains wrappers, imports, re-exports, and table entries from
// global roots.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const keepAlive = [];
globalThis.__heapDumpWasmTypeReflectionFunctionKeepAlive = keepAlive;

function keep(name, value) {
  keepAlive.push({name, value});
  return value;
}

(function KeepWebAssemblyFunctionWrappers() {
  assertEquals('function', typeof WebAssembly.Function);

  const addEleven = keep(
      'addEleven',
      new WebAssembly.Function(
          {parameters: ['i32'], results: ['i32']}, (value) => value + 11));
  const returnExtern = keep(
      'returnExtern',
      new WebAssembly.Function(
          {parameters: ['externref'], results: ['externref']},
          (value) => value));
  const voidCallback = keep(
      'voidCallback',
      new WebAssembly.Function({parameters: [], results: []}, () => {}));

  const builder = new WasmModuleBuilder();
  const importedAdd = builder.addImport('m', 'add', kSig_i_i);
  const importedExtern =
      builder.addImport('m', 'extern', makeSig([kWasmExternRef],
                                               [kWasmExternRef]));
  const importedVoid = builder.addImport('m', 'voidCallback', kSig_v_v);
  const table = builder.addTable(kWasmFuncRef, 4, 8).exportAs('table');

  builder.addExport('reexportedAdd', importedAdd);
  builder.addFunction('callAdd', kSig_i_i)
      .addBody([kExprLocalGet, 0, kExprCallFunction, importedAdd])
      .exportFunc();
  builder.addFunction('callExtern',
                      makeSig([kWasmExternRef], [kWasmExternRef]))
      .addBody([kExprLocalGet, 0, kExprCallFunction, importedExtern])
      .exportFunc();
  builder.addFunction('callVoid', kSig_v_v)
      .addBody([kExprCallFunction, importedVoid])
      .exportFunc();

  const module = keep('module', builder.toModule());
  const instance = keep(
      'instance',
      new WebAssembly.Instance(module, {
        m: {
          add: addEleven,
          extern: returnExtern,
          voidCallback,
        },
      }));
  const exports = keep('exports', instance.exports);
  const tableObject = keep('table', exports.table);

  const externPayload = keep('externPayload', {kind: 'externref-function'});
  tableObject.set(0, addEleven);
  tableObject.set(1, voidCallback);

  const rewrappedAdd = keep(
      'rewrappedAdd',
      new WebAssembly.Function(
          {parameters: ['i32'], results: ['i32']}, exports.reexportedAdd));
  tableObject.set(2, rewrappedAdd);

  const results = keep('results', {
    addDirect: addEleven(7),
    addImported: exports.callAdd(7),
    addReexported: exports.reexportedAdd(7),
    addRewrapped: rewrappedAdd(7),
    externIdentity: exports.callExtern(externPayload),
    tableAdd: tableObject.get(0)(7),
    tableRewrapped: tableObject.get(2)(7),
  });

  exports.callVoid();

  assertTrue(addEleven instanceof WebAssembly.Function);
  assertTrue(rewrappedAdd instanceof WebAssembly.Function);
  assertEquals(18, results.addDirect);
  assertEquals(18, results.addImported);
  assertEquals(18, results.addReexported);
  assertEquals(18, results.addRewrapped);
  assertSame(externPayload, results.externIdentity);
  assertEquals(18, results.tableAdd);
  assertEquals(18, results.tableRewrapped);
})();
