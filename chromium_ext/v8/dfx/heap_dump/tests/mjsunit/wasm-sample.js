// Heap-dump sample corpus:
// - WebAssembly.Module / Instance / Memory / Table / Global
// - exported wasm function and memory buffer

(function() {
  const bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,
    0x01, 0x00, 0x00, 0x00,
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
    0x03, 0x02, 0x01, 0x00,
    0x07, 0x07, 0x01, 0x03, 0x6e, 0x6f, 0x70, 0x00, 0x00,
    0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b
  ]);

  const module = new WebAssembly.Module(bytes);
  const instance = new WebAssembly.Instance(module, {});
  const memory = new WebAssembly.Memory({ initial: 1, maximum: 1 });
  const table = new WebAssembly.Table({ initial: 2, element: 'anyfunc' });
  const globalValue = new WebAssembly.Global({ value: 'i32', mutable: true }, 7);

  globalThis.heapDumpWasmSample = {
    module,
    instance,
    memory,
    table,
    globalValue,
    buffer: memory.buffer,
    exported: instance.exports.nop,
  };

  assertEquals('function', typeof globalThis.heapDumpWasmSample.exported);
})();
