// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep resizable/growable buffer and view edge states alive for heap dump
// smoke. This follows V8's ArrayBuffer transfer and RAB/GSAB mjsunit patterns,
// preserving fixed-length, length-tracking, out-of-bounds, grown, and detached
// backing-store states.

const keepAlive = [];
globalThis.__heapDumpResizableBuffersKeepAlive = keepAlive;

function keep(name, value) {
  keepAlive.push({name, value});
  return value;
}

(function KeepResizableArrayBufferStates() {
  const buffer = keep(
      'rabBuffer', new ArrayBuffer(128, {maxByteLength: 512}));
  const fixedUint8 = keep('rabFixedUint8', new Uint8Array(buffer, 16, 32));
  const trackingUint8 = keep('rabTrackingUint8', new Uint8Array(buffer));
  const fixedDataView = keep('rabFixedDataView', new DataView(buffer, 32, 32));
  const trackingDataView = keep('rabTrackingDataView', new DataView(buffer, 64));
  const nearEndUint8 = keep('rabNearEndUint8', new Uint8Array(buffer, 96, 32));

  fixedUint8[0] = 11;
  trackingUint8[120] = 22;
  fixedDataView.setUint32(0, 0x12345678, true);
  trackingDataView.setUint16(0, 0xabcd, true);

  buffer.resize(80);
  assertEquals(80, buffer.byteLength);
  assertEquals(0, nearEndUint8.length);
  keep('rabOutOfBoundsView', nearEndUint8);

  buffer.resize(256);
  trackingUint8[200] = 33;
  assertEquals(256, trackingUint8.length);
  assertEquals(33, trackingUint8[200]);

  const transferSource = keep(
      'rabTransferSource', new ArrayBuffer(64, {maxByteLength: 256}));
  new Uint8Array(transferSource).set([1, 2, 3, 4]);
  const sourceView = keep('rabDetachedSourceView', new Uint8Array(transferSource));
  const transferred = keep('rabTransferred', transferSource.transfer(128));
  const transferredView = keep('rabTransferredView', new Uint8Array(transferred));

  assertEquals(0, transferSource.byteLength);
  assertEquals(0, sourceView.length);
  assertEquals(128, transferred.byteLength);
  assertEquals(1, transferredView[0]);
})();

(function KeepGrowableSharedArrayBufferStates() {
  if (typeof SharedArrayBuffer !== 'function') return;

  const buffer = keep(
      'gsabBuffer', new SharedArrayBuffer(128, {maxByteLength: 512}));
  const fixedInt32 = keep('gsabFixedInt32', new Int32Array(buffer, 0, 8));
  const trackingInt32 = keep('gsabTrackingInt32', new Int32Array(buffer));
  const fixedDataView = keep('gsabFixedDataView', new DataView(buffer, 16, 32));
  const trackingDataView = keep('gsabTrackingDataView', new DataView(buffer));

  Atomics.store(fixedInt32, 0, 44);
  Atomics.store(trackingInt32, 1, 55);
  fixedDataView.setUint32(0, 0x55667788, true);
  trackingDataView.setUint8(24, 66);

  buffer.grow(256);
  Atomics.store(trackingInt32, 40, 77);
  assertEquals(256, buffer.byteLength);
  assertEquals(77, Atomics.load(trackingInt32, 40));
  assertEquals(44, Atomics.load(fixedInt32, 0));
})();

keep('summary', {
  arrayBuffers: keepAlive.filter((entry) => entry.value instanceof ArrayBuffer)
                    .length,
  sharedArrayBuffers: typeof SharedArrayBuffer === 'function'
      ? keepAlive.filter((entry) => entry.value instanceof SharedArrayBuffer)
            .length
      : 0,
});
