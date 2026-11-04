// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep memory64 and table64 Wasm API shapes alive for heap dump smoke. This
// follows V8's memory64/table64 mjsunit patterns while retaining the exported
// memory/table objects and their views from global roots.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const keepAlive = [];
globalThis.__heapDumpWasmMemory64Table64KeepAlive = keepAlive;

function keep(name, value) {
  keepAlive.push({name, value});
  return value;
}

(function KeepMemory64ModuleState() {
  const builder = new WasmModuleBuilder();
  const memory64 = builder.addMemory64(1, 2);
  const memory32 = builder.addMemory(1, 1);

  builder.exportMemoryAs('memory64', memory64);
  builder.exportMemoryAs('memory32', memory32);
  builder.addFunction('memorySize64', kSig_l_v)
      .addBody([kExprMemorySize, memory64])
      .exportFunc();
  builder.addFunction('memorySize32', kSig_i_v)
      .addBody([kExprMemorySize, memory32])
      .exportFunc();

  const module = keep('memory64Module', builder.toModule());
  const instance = keep('memory64Instance', new WebAssembly.Instance(module));
  const exports = keep('memory64Exports', instance.exports);
  const memory64View =
      keep('memory64View', new Uint8Array(exports.memory64.buffer));
  const memory32View =
      keep('memory32View', new Uint8Array(exports.memory32.buffer));

  assertEquals(1n, exports.memorySize64());
  assertEquals(1, exports.memorySize32());
  assertEquals(kPageSize, exports.memory64.buffer.byteLength);
  memory64View[0] = 64;
  memory32View[0] = 32;
  assertEquals(64, memory64View[0]);
  assertEquals(32, memory32View[0]);
})();

(function KeepWasmTable64ModuleState() {
  const builder = new WasmModuleBuilder();
  const table = builder.addTable64(kWasmExternRef, 4, 8).exportAs('table64');
  const sig = makeSig([kWasmI64], [kWasmExternRef]);
  const setSig = makeSig([kWasmI64, kWasmExternRef], []);

  builder.addFunction('table64Get', sig)
      .addBody([kExprLocalGet, 0, kExprTableGet, table.index])
      .exportFunc();
  builder.addFunction('table64Set', setSig)
      .addBody([
        kExprLocalGet, 0, kExprLocalGet, 1, kExprTableSet, table.index
      ])
      .exportFunc();

  const module = keep('table64Module', builder.toModule());
  const instance = keep('table64Instance', new WebAssembly.Instance(module));
  const exports = keep('table64Exports', instance.exports);
  const payload = keep('table64ExternPayload', {kind: 'externref', value: 64});

  assertEquals(null, exports.table64Get(1n));
  exports.table64Set(1n, payload);
  assertSame(payload, exports.table64Get(1n));
  keep('wasmTable64Export', exports.table64);
})();

(function KeepJsTable64ApiState() {
  const jsTable64 = keep(
      'jsTable64',
      new WebAssembly.Table(
          {initial: 2n, maximum: 4n, element: 'externref', address: 'i64'}));
  const payload = keep('jsTable64Payload', {kind: 'js-table64', value: 128});

  assertEquals(undefined, jsTable64.get(1n));
  jsTable64.set(1n, payload);
  assertSame(payload, jsTable64.get(1n));
  assertEquals(2n, jsTable64.grow(1n));
  assertEquals(undefined, jsTable64.get(2n));
})();
