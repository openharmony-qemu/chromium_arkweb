// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared --no-wasm-inlining

// Keep shared Wasm GC objects alive for heap dump smoke. This follows V8's
// shared-everything mjsunit coverage for shared structs and arrays, but keeps
// the produced objects reachable from globalThis so the heap dump translator
// sees their visitor paths.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  (function SharedStruct() {
    const builder = new WasmModuleBuilder();
    const structType = builder.addStruct(
        [makeField(kWasmI32, true), makeField(kWasmI64, true)],
        kNoSuperType, false, true);
    const makeStructSig =
        makeSig([kWasmI32, kWasmI64], [wasmRefType(structType)]);
    const getI32Sig = makeSig([wasmRefNullType(structType)], [kWasmI32]);
    const getI64Sig = makeSig([wasmRefNullType(structType)], [kWasmI64]);

    builder.addFunction('makeStruct', makeStructSig)
        .addBody([
          kExprLocalGet, 0,
          kExprLocalGet, 1,
          kGCPrefix, kExprStructNew, structType,
        ])
        .exportFunc();
    builder.addFunction('getStructI32', getI32Sig)
        .addBody([
          kExprLocalGet, 0,
          kGCPrefix, kExprStructGet, structType, 0,
        ])
        .exportFunc();
    builder.addFunction('getStructI64', getI64Sig)
        .addBody([
          kExprLocalGet, 0,
          kGCPrefix, kExprStructGet, structType, 1,
        ])
        .exportFunc();

    const module = keep('sharedStructModule', builder.toModule());
    const instance =
        keep('sharedStructInstance', new WebAssembly.Instance(module));
    const exports = keep('sharedStructExports', instance.exports);
    const structObject = keep(
        'sharedStructObject', exports.makeStruct(17, 19000000000n));

    assertEquals(17, exports.getStructI32(structObject));
    assertEquals(19000000000n, exports.getStructI64(structObject));
  })();

  (function SharedScalarArray() {
    const builder = new WasmModuleBuilder();
    const arrayType =
        builder.addArray(kWasmI64, true, kNoSuperType, false, true);
    const makeArraySig = makeSig([kWasmI64, kWasmI32], [wasmRefType(arrayType)]);
    const getArraySig =
        makeSig([wasmRefNullType(arrayType), kWasmI32], [kWasmI64]);
    const lengthSig = makeSig([wasmRefNullType(arrayType)], [kWasmI32]);

    builder.addFunction('makeArray', makeArraySig)
        .addBody([
          kExprLocalGet, 0,
          kExprLocalGet, 1,
          kGCPrefix, kExprArrayNew, arrayType,
        ])
        .exportFunc();
    builder.addFunction('makeFixedArray', makeArraySig)
        .addBody([
          kExprLocalGet, 0,
          kGCPrefix, kExprArrayNewFixed, arrayType, 1,
        ])
        .exportFunc();
    builder.addFunction('makeDefaultArray', makeArraySig)
        .addBody([
          kExprLocalGet, 1,
          kGCPrefix, kExprArrayNewDefault, arrayType,
        ])
        .exportFunc();
    builder.addFunction('getArrayElement', getArraySig)
        .addBody([
          kExprLocalGet, 0,
          kExprLocalGet, 1,
          kGCPrefix, kExprArrayGet, arrayType,
        ])
        .exportFunc();
    builder.addFunction('arrayLength', lengthSig)
        .addBody([
          kExprLocalGet, 0,
          kGCPrefix, kExprArrayLen,
        ])
        .exportFunc();

    const module = keep('sharedScalarArrayModule', builder.toModule());
    const instance =
        keep('sharedScalarArrayInstance', new WebAssembly.Instance(module));
    const exports = keep('sharedScalarArrayExports', instance.exports);
    const repeatedArray =
        keep('sharedScalarRepeatedArray', exports.makeArray(23n, 5));
    const fixedArray =
        keep('sharedScalarFixedArray', exports.makeFixedArray(29n, 1));
    const defaultArray =
        keep('sharedScalarDefaultArray', exports.makeDefaultArray(31n, 4));

    assertEquals(5, exports.arrayLength(repeatedArray));
    assertEquals(23n, exports.getArrayElement(repeatedArray, 4));
    assertEquals(1, exports.arrayLength(fixedArray));
    assertEquals(29n, exports.getArrayElement(fixedArray, 0));
    assertEquals(4, exports.arrayLength(defaultArray));
    assertEquals(0n, exports.getArrayElement(defaultArray, 0));
  })();

  (function SharedArrayOfStructs() {
    const builder = new WasmModuleBuilder();
    const structType =
        builder.addStruct([makeField(kWasmI64, true)], kNoSuperType, false, true);
    const arrayType = builder.addArray(
        wasmRefNullType(structType), true, kNoSuperType, false, true);
    const makeArraySig =
        makeSig([kWasmI64, kWasmI64], [wasmRefType(arrayType)]);
    const getNestedValueSig =
        makeSig([wasmRefNullType(arrayType), kWasmI32], [kWasmI64]);

    builder.addFunction('makeStructArray', makeArraySig)
        .addBody([
          kExprLocalGet, 0,
          kGCPrefix, kExprStructNew, structType,
          kExprLocalGet, 1,
          kGCPrefix, kExprStructNew, structType,
          kGCPrefix, kExprArrayNewFixed, arrayType, 2,
        ])
        .exportFunc();
    builder.addFunction('getNestedValue', getNestedValueSig)
        .addBody([
          kExprLocalGet, 0,
          kExprLocalGet, 1,
          kGCPrefix, kExprArrayGet, arrayType,
          kGCPrefix, kExprStructGet, structType, 0,
        ])
        .exportFunc();

    const module = keep('sharedStructArrayModule', builder.toModule());
    const instance =
        keep('sharedStructArrayInstance', new WebAssembly.Instance(module));
    const exports = keep('sharedStructArrayExports', instance.exports);
    const structArray =
        keep('sharedStructArrayObject', exports.makeStructArray(41n, 43n));

    assertEquals(41n, exports.getNestedValue(structArray, 0));
    assertEquals(43n, exports.getNestedValue(structArray, 1));
  })();

  globalThis.heapDumpWasmSharedGcDeepSample = catalog;
})();
