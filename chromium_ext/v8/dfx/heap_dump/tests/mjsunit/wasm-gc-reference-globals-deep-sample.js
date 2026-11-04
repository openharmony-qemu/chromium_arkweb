// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Wasm GC reference-typed JS API globals alive for heap dump smoke. This
// is a compact version of V8's reference-globals-import.js coverage for
// anyref, eqref, structref, arrayref, and i31ref mutable globals.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

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
      assertUnreachable(name);
    } catch (e) {
      keep(name, e);
    }
  }

  const globals = keep('globals', {
    anyref: new WebAssembly.Global(
        {value: 'anyref', mutable: true}, 'initial-anyref'),
    eqref: new WebAssembly.Global({value: 'eqref', mutable: true}, null),
    structref: new WebAssembly.Global(
        {value: 'structref', mutable: true}, null),
    arrayref: new WebAssembly.Global(
        {value: 'arrayref', mutable: true}, null),
    i31ref: new WebAssembly.Global({value: 'i31ref', mutable: true}, 123),
  });

  const builder = new WasmModuleBuilder();
  const anyrefGlobal =
      builder.addImportedGlobal('imports', 'anyref', kWasmAnyRef, true);
  const eqrefGlobal =
      builder.addImportedGlobal('imports', 'eqref', kWasmEqRef, true);
  const structrefGlobal =
      builder.addImportedGlobal('imports', 'structref', kWasmStructRef, true);
  const arrayrefGlobal =
      builder.addImportedGlobal('imports', 'arrayref', kWasmArrayRef, true);
  const i31refGlobal =
      builder.addImportedGlobal('imports', 'i31ref', kWasmI31Ref, true);
  const structType = builder.addStruct([makeField(kWasmI32, false)]);
  const arrayType = builder.addArray(kWasmI32);

  builder.addFunction('anyrefToExtern', makeSig([], [kWasmExternRef]))
      .addBody([
        kExprGlobalGet, anyrefGlobal,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();
  builder.addFunction('eqrefToExtern', makeSig([], [kWasmExternRef]))
      .addBody([
        kExprGlobalGet, eqrefGlobal,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();
  builder.addFunction('getStructField', makeSig([], [kWasmI32]))
      .addBody([
        kExprGlobalGet, structrefGlobal,
        kGCPrefix, kExprRefCast, structType,
        kGCPrefix, kExprStructGet, structType, 0,
      ])
      .exportFunc();
  builder.addFunction('getArrayElement', makeSig([], [kWasmI32]))
      .addBody([
        kExprGlobalGet, arrayrefGlobal,
        kGCPrefix, kExprRefCast, arrayType,
        kExprI32Const, 0,
        kGCPrefix, kExprArrayGet, arrayType,
      ])
      .exportFunc();
  builder.addFunction('getI31', makeSig([], [kWasmI32]))
      .addBody([
        kExprGlobalGet, i31refGlobal,
        kGCPrefix, kExprI31GetS,
      ])
      .exportFunc();
  builder.addFunction('createStructExtern',
                      makeSig([kWasmI32], [kWasmExternRef]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprStructNew, structType,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();
  builder.addFunction('createArrayExtern',
                      makeSig([kWasmI32], [kWasmExternRef]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprArrayNewFixed, arrayType, 1,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();
  builder.addFunction('createStruct',
                      makeSig([kWasmI32], [wasmRefType(structType)]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprStructNew, structType,
      ])
      .exportFunc();
  builder.addFunction('createArray',
                      makeSig([kWasmI32], [wasmRefType(arrayType)]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprArrayNewFixed, arrayType, 1,
      ])
      .exportFunc();

  const module = keep('module', builder.toModule());
  const instance = keep(
      'instance',
      new WebAssembly.Instance(module, {
        imports: {
          anyref: globals.anyref,
          eqref: globals.eqref,
          structref: globals.structref,
          arrayref: globals.arrayref,
          i31ref: globals.i31ref,
        },
      }));
  const wasm = keep('wasmExports', instance.exports);

  const jsObject = keep('jsObject', {kind: 'anyref-object'});
  globals.anyref.value = jsObject;
  const anyrefObjectResult = keep('anyrefObjectResult', wasm.anyrefToExtern());
  globals.anyref.value = wasm.createStructExtern(41);
  const anyrefStruct = keep('anyrefStruct', globals.anyref.value);
  globals.anyref.value = wasm.createArrayExtern(42);
  const anyrefArray = keep('anyrefArray', globals.anyref.value);
  globals.anyref.value = undefined;
  const anyrefUndefinedResult =
      keep('anyrefUndefinedResult', wasm.anyrefToExtern());

  globals.eqref.value = wasm.createStructExtern(43);
  const eqrefStruct = keep('eqrefStruct', globals.eqref.value);
  globals.eqref.value = wasm.createArrayExtern(44);
  const eqrefArray = keep('eqrefArray', globals.eqref.value);
  globals.eqref.value = 12345;
  const eqrefI31Result = keep('eqrefI31Result', wasm.eqrefToExtern());

  globals.structref.value = wasm.createStruct(45);
  const structrefObject = keep('structrefObject', globals.structref.value);
  globals.arrayref.value = wasm.createArray(46);
  const arrayrefObject = keep('arrayrefObject', globals.arrayref.value);
  globals.i31ref.value = -47;

  keepError('badEqrefObjectError', () => {
    globals.eqref.value = {};
  });
  keepError('badStructrefArrayError', () => {
    globals.structref.value = wasm.createArray(1);
  });
  keepError('badArrayrefStructError', () => {
    globals.arrayref.value = wasm.createStruct(1);
  });
  keepError('badI31refStringError', () => {
    globals.i31ref.value = 'not-i31';
  });

  const results = keep('results', {
    anyrefObject: anyrefObjectResult,
    anyrefUndefined: anyrefUndefinedResult,
    eqrefI31: eqrefI31Result,
    structField: wasm.getStructField(),
    arrayElement: wasm.getArrayElement(),
    i31: wasm.getI31(),
  });

  assertTrue(globals.anyref instanceof WebAssembly.Global);
  assertTrue(globals.eqref instanceof WebAssembly.Global);
  assertTrue(globals.structref instanceof WebAssembly.Global);
  assertTrue(globals.arrayref instanceof WebAssembly.Global);
  assertTrue(globals.i31ref instanceof WebAssembly.Global);
  assertSame(jsObject, results.anyrefObject);
  assertEquals(undefined, results.anyrefUndefined);
  assertEquals(12345, results.eqrefI31);
  assertEquals(45, results.structField);
  assertEquals(46, results.arrayElement);
  assertEquals(-47, results.i31);
  assertTrue(eqrefStruct !== null);
  assertTrue(eqrefArray !== null);
  assertTrue(anyrefStruct !== null);
  assertTrue(anyrefArray !== null);
  assertTrue(structrefObject !== null);
  assertTrue(arrayrefObject !== null);
  assertTrue(catalog.badEqrefObjectError instanceof TypeError);
  assertTrue(catalog.badStructrefArrayError instanceof TypeError);
  assertTrue(catalog.badArrayrefStructError instanceof TypeError);
  assertTrue(catalog.badI31refStringError instanceof TypeError);

  globalThis.heapDumpWasmGcReferenceGlobalsDeepSample = catalog;
})();
