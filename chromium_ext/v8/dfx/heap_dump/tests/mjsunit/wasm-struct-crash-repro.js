// Flags: --allow-natives-syntax

// Repro sample for unsupported WasmStruct handling in heap translate.
// This script itself should run successfully. Repro steps:
// 1) out/x64.release/d8 --heapdump-out tmp/wasm-struct-repro.rawheap \
//      arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-struct-crash-repro.js
// 2) out/x64.release/d8 --translate tmp/wasm-struct-repro.rawheap \
//      --translate-out tmp/wasm-struct-repro.heapsnapshot

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  function check(condition, message) {
    if (!condition) throw new Error(message);
  }

  const builder = new WasmModuleBuilder();
  const structType =
      builder.addStruct([makeField(kWasmI64, true), makeField(kWasmI32, true)]);

  builder.addFunction(
             'makeStruct',
             makeSig([kWasmI64, kWasmI32], [wasmRefType(structType)]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructNew, structType
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const structObject = instance.exports.makeStruct(99n, 11);

  globalThis.heapDumpWasmStructCrashRepro = {
    instance,
    structObject,
    makeStruct: instance.exports.makeStruct,
  };

  check(typeof globalThis.heapDumpWasmStructCrashRepro.makeStruct === 'function',
        'makeStruct type check failed');
})();
