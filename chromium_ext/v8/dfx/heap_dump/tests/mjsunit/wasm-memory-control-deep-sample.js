// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-memory-control

// Keep Wasm memory-control descriptors alive for heap dump smoke. This follows
// V8's memory-control mjsunit coverage, but preserves mapped, anonymous, and
// unmapped descriptor states so the heap visitor sees the weak memory field.

const keepAlive = [];
globalThis.__heapDumpWasmMemoryControlKeepAlive = keepAlive;

function keep(name, value) {
  keepAlive.push({name, value});
  return value;
}

const fileDescriptor = keep('fileDescriptor',
                            d8.file.create_wasm_memory_map_descriptor(
                                'test/mjsunit/wasm/wasm-module-builder.js'));

const anonymousDescriptor =
    keep('anonymousDescriptor', new WebAssembly.MemoryMapDescriptor(128));
const anonymousMemory =
    keep('anonymousMemory', new WebAssembly.Memory({initial: 1, maximum: 2}));
const anonymousView =
    keep('anonymousView', new Uint8Array(anonymousMemory.buffer));
anonymousDescriptor.map(anonymousMemory, 0x2000);
assertEquals(0, anonymousView[0x2000]);
anonymousView[0x2000] = 77;
assertEquals(77, anonymousView[0x2000]);

const unmappedDescriptor =
    keep('unmappedDescriptor', new WebAssembly.MemoryMapDescriptor(64));
const unmappedMemory =
    keep('unmappedMemory', new WebAssembly.Memory({initial: 1}));
const unmappedView = keep('unmappedView', new Uint8Array(unmappedMemory.buffer));
unmappedDescriptor.map(unmappedMemory, 0x3000);
unmappedView[0x3000] = 91;
assertEquals(91, unmappedView[0x3000]);
unmappedDescriptor.unmap();
assertEquals(0, unmappedView[0x3000]);

keep('summary', {
  hasFileDescriptor: fileDescriptor instanceof WebAssembly.MemoryMapDescriptor,
  anonymousByte: anonymousView[0x2000],
  unmappedByte: unmappedView[0x3000],
});
