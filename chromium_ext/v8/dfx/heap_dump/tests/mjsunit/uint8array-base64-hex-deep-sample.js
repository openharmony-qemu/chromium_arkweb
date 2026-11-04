// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-base-64

const keepalive = [];

function keep(name, value) {
  keepalive.push({ name, value });
  return value;
}

function keepError(name, fn) {
  try {
    fn();
  } catch (e) {
    return keep(name, e);
  }
  return keep(name, null);
}

if (typeof Uint8Array.fromBase64 === 'function' &&
    typeof Uint8Array.fromHex === 'function') {
  const source = keep('source',
                      new Uint8Array([102, 111, 111, 98, 97, 114, 255]));
  const base64 = keep('base64', source.toBase64());
  const hex = keep('hex', source.toHex());
  assertEquals('Zm9vYmFy/w==', base64);
  assertEquals('666f6f626172ff', hex);

  const fromBase64 = keep('fromBase64', Uint8Array.fromBase64('Zm9vYmFy'));
  const fromBase64Url =
      keep('fromBase64Url',
           Uint8Array.fromBase64('Zm9v-YXI', {alphabet: 'base64url'}));
  const fromHex = keep('fromHex', Uint8Array.fromHex('666f6f626172'));
  const fromTwoByteHex =
      keep('fromTwoByteHex', Uint8Array.fromHex('666f6f626172' + 'ff'));
  assertArrayEquals([102, 111, 111, 98, 97, 114], Array.from(fromBase64));
  assertArrayEquals([102, 111, 111, 249, 133, 200],
                    Array.from(fromBase64Url));
  assertArrayEquals([102, 111, 111, 98, 97, 114], Array.from(fromHex));
  assertArrayEquals([102, 111, 111, 98, 97, 114, 255],
                    Array.from(fromTwoByteHex));

  const base64Target = keep('base64Target', new Uint8Array(8).fill(255));
  const base64Result =
      keep('base64Result', base64Target.setFromBase64('Zm9vYmFy'));
  assertEquals(8, base64Target.length);
  assertEquals(6, base64Result.written);
  assertEquals(8, base64Result.read);
  assertArrayEquals([102, 111, 111, 98, 97, 114, 255, 255],
                    Array.from(base64Target));

  const hexTarget = keep('hexTarget', new Uint8Array(8).fill(255));
  const hexResult = keep('hexResult', hexTarget.setFromHex('666f6f626172'));
  assertEquals(6, hexResult.written);
  assertEquals(12, hexResult.read);
  assertArrayEquals([102, 111, 111, 98, 97, 114, 255, 255],
                    Array.from(hexTarget));

  if (typeof SharedArrayBuffer === 'function') {
    const sab = keep('sharedBuffer', new SharedArrayBuffer(8));
    const sharedView = keep('sharedView', new Uint8Array(sab));
    sharedView.set([102, 111, 111, 98, 97, 114, 255, 255]);
    keep('sharedBase64', sharedView.subarray(0, 6).toBase64());
    keep('sharedHex', sharedView.subarray(0, 6).toHex());
    assertEquals('Zm9vYmFy', keepalive[keepalive.length - 2].value);
    assertEquals('666f6f626172', keepalive[keepalive.length - 1].value);
  }

  keepError('badBase64Syntax', () => Uint8Array.fromBase64('%%%'));
  keepError('badHexSyntax', () => Uint8Array.fromHex('666f6g'));
  keepError('nullFromBase64Options',
            () => Uint8Array.fromBase64('Zm9vYmE=', null));
  keepError('nullSetFromHexOptions', () => hexTarget.setFromHex('66', null));
}

globalThis.__heapDumpUint8ArrayBase64HexKeepalive = keepalive;
