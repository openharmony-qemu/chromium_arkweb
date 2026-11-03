// Flags: --allow-natives-syntax

// Heap-dump sample corpus:
// - Wasm GC objects via struct/array types
// - keep wasm struct and wasm array values alive on globalThis

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  function check(condition, message) {
    if (!condition) throw new Error(message);
  }

  const builder = new WasmModuleBuilder();

  const structType =
      builder.addStruct([makeField(kWasmI64, true), makeField(kWasmI32, true)]);
  const arrayType = builder.addArray(wasmRefNullType(structType), true);

  builder.addFunction(
             'makeStruct',
             makeSig([kWasmI64, kWasmI32], [wasmRefType(structType)]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructNew, structType
      ])
      .exportFunc();

  builder.addFunction('makeArray', makeSig([kWasmI32], [wasmRefType(arrayType)]))
      .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayNewDefault, arrayType])
      .exportFunc();

  builder.addFunction('arrayLen', makeSig([wasmRefType(arrayType)], [kWasmI32]))
      .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayLen])
      .exportFunc();

  const instance = builder.instantiate();
  const structObject = instance.exports.makeStruct(42n, 7);
  const arrayObject = instance.exports.makeArray(5);

  globalThis.heapDumpWasmGcSample = {
    instance,
    structObject,
    arrayObject,
    makeStruct: instance.exports.makeStruct,
    makeArray: instance.exports.makeArray,
    arrayLen: instance.exports.arrayLen,
  };

  check(globalThis.heapDumpWasmGcSample.arrayLen(arrayObject) === 5,
        'array length check failed');
  check(typeof globalThis.heapDumpWasmGcSample.makeStruct === 'function',
        'makeStruct type check failed');
})();
