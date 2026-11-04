// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-type-reflection

// Keep JS API reference globals and imported Wasm reference globals alive for
// heap dump smoke. This follows V8 reference-global and type-reflection
// patterns with mutable funcref and externref globals.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  'use strict';

  const keepalive = {};
  function keep(name, value) {
    keepalive[name] = value;
    return value;
  }

  assertEquals('function', typeof WebAssembly.Function);

  const addOne = keep(
      'addOne',
      new WebAssembly.Function(
          {parameters: ['i32'], results: ['i32']}, value => value + 1));
  const addTwo = keep(
      'addTwo',
      new WebAssembly.Function(
          {parameters: ['i32'], results: ['i32']}, value => value + 2));
  const externPayloadA = keep('externPayloadA', {kind: 'extern-a'});
  const externPayloadB = keep('externPayloadB', {kind: 'extern-b'});

  const jsGlobals = keep('jsGlobals', {
    funcref: new WebAssembly.Global(
        {value: 'anyfunc', mutable: true}, addOne),
    externref: new WebAssembly.Global(
        {value: 'externref', mutable: true}, externPayloadA),
  });

  const builder = new WasmModuleBuilder();
  const importedFunGlobal =
      builder.addImportedGlobal('m', 'fun', kWasmFuncRef, true);
  const importedExternGlobal =
      builder.addImportedGlobal('m', 'extern', kWasmExternRef, true);
  builder.addExportOfKind(
      'importedFunGlobal', kExternalGlobal, importedFunGlobal);
  builder.addExportOfKind(
      'importedExternGlobal', kExternalGlobal, importedExternGlobal);
  builder.addFunction('getFun', makeSig([], [kWasmFuncRef]))
      .addBody([kExprGlobalGet, importedFunGlobal])
      .exportFunc();
  builder.addFunction('setFun', makeSig([kWasmFuncRef], []))
      .addBody([kExprLocalGet, 0, kExprGlobalSet, importedFunGlobal])
      .exportFunc();
  builder.addFunction('getExtern', makeSig([], [kWasmExternRef]))
      .addBody([kExprGlobalGet, importedExternGlobal])
      .exportFunc();
  builder.addFunction('setExtern', makeSig([kWasmExternRef], []))
      .addBody([kExprLocalGet, 0, kExprGlobalSet, importedExternGlobal])
      .exportFunc();
  builder.addFunction(
             'returnReferenceGlobals',
             makeSig([], [kWasmFuncRef, kWasmExternRef]))
      .addBody([kExprGlobalGet, importedFunGlobal,
                kExprGlobalGet, importedExternGlobal])
      .exportFunc();

  const module = keep('module', builder.toModule());
  const instance = keep(
      'instance',
      new WebAssembly.Instance(module, {
        m: {
          fun: jsGlobals.funcref,
          extern: jsGlobals.externref,
        },
      }));
  const exports = keep('exports', instance.exports);
  const exportedGlobals = keep('exportedGlobals', {
    funcref: exports.importedFunGlobal,
    externref: exports.importedExternGlobal,
  });

  const before = keep('before', {
    directFun: jsGlobals.funcref.value(10),
    wasmFun: exports.getFun()(11),
    directExtern: jsGlobals.externref.value,
    wasmExtern: exports.getExtern(),
  });

  exports.setFun(addTwo);
  exports.setExtern(externPayloadB);

  const returnedReferences = keep(
      'returnedReferences', exports.returnReferenceGlobals());
  const after = keep('after', {
    directFun: jsGlobals.funcref.value(12),
    exportedFun: exportedGlobals.funcref.value(13),
    returnedFun: returnedReferences[0](14),
    directExtern: jsGlobals.externref.value,
    exportedExtern: exportedGlobals.externref.value,
    returnedExtern: returnedReferences[1],
  });

  const errors = keep('errors', {});
  try {
    jsGlobals.funcref.value = function plainFunction(value) {
      return value;
    };
  } catch (e) {
    errors.badFuncrefAssignment = e;
  }

  assertTrue(addOne instanceof WebAssembly.Function);
  assertTrue(addTwo instanceof WebAssembly.Function);
  assertTrue(jsGlobals.funcref instanceof WebAssembly.Global);
  assertTrue(jsGlobals.externref instanceof WebAssembly.Global);
  assertTrue(exportedGlobals.funcref instanceof WebAssembly.Global);
  assertTrue(exportedGlobals.externref instanceof WebAssembly.Global);
  assertEquals(11, before.directFun);
  assertEquals(12, before.wasmFun);
  assertSame(externPayloadA, before.directExtern);
  assertSame(externPayloadA, before.wasmExtern);
  assertEquals(14, after.directFun);
  assertEquals(15, after.exportedFun);
  assertEquals(16, after.returnedFun);
  assertSame(externPayloadB, after.directExtern);
  assertSame(externPayloadB, after.exportedExtern);
  assertSame(externPayloadB, after.returnedExtern);
  assertTrue(errors.badFuncrefAssignment instanceof TypeError);

  globalThis.heapDumpWasmReferenceGlobalsDeepSample = keepalive;
})();
