// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging --experimental-wasm-type-reflection

// Keep Wasm exception/tag/exnref-adjacent objects alive for heap dump smoke.
// This follows V8's exceptions-api, exnref-api, and exceptions-type-reflection
// mjsunit patterns while retaining JS-created tags, module-exported tags,
// imported/re-exported tags, JSTag, and exception payload packages.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const keepAlive = [];
globalThis.__heapDumpWasmExceptionExnrefKeepAlive = keepAlive;

function keep(name, value) {
  keepAlive.push({name, value});
  return value;
}

(function KeepJsTagsAndExceptions() {
  assertEquals('function', typeof WebAssembly.Tag);
  assertEquals('function', typeof WebAssembly.Exception);

  const externPayload = keep('externPayload', {kind: 'exception-externref'});
  const i32Tag = keep('i32Tag', new WebAssembly.Tag({parameters: ['i32']}));
  const externTag =
      keep('externTag', new WebAssembly.Tag({parameters: ['externref']}));
  const mixedTag = keep(
      'mixedTag',
      new WebAssembly.Tag({
        parameters: ['i32', 'i64', 'f64', 'externref'],
      }));

  const i32Exception =
      keep('i32Exception',
           new WebAssembly.Exception(i32Tag, [42], {traceStack: true}));
  const externException =
      keep('externException',
           new WebAssembly.Exception(externTag, [externPayload]));
  const mixedException = keep(
      'mixedException',
      new WebAssembly.Exception(
          mixedTag, [7, 8n, 9.5, externPayload], {traceStack: true}));

  assertTrue(i32Exception.is(i32Tag));
  assertTrue(externException.is(externTag));
  assertTrue(mixedException.is(mixedTag));
  assertEquals(42, i32Exception.getArg(i32Tag, 0));
  assertSame(externPayload, externException.getArg(externTag, 0));
  assertSame(externPayload, mixedException.getArg(mixedTag, 3));
  assertEquals({parameters: ['externref']}, externTag.type());

  keep('jsTagRoot', WebAssembly.JSTag);
  assertEquals({parameters: ['externref']}, WebAssembly.JSTag.type());
})();

(function KeepModuleTagsAndExceptionPackages() {
  const importedTag =
      keep('importedTag', new WebAssembly.Tag({parameters: ['externref']}));
  const imports = keep('imports', {m: {importedTag}});

  const builder = new WasmModuleBuilder();
  const importedTagIndex =
      builder.addImportedTag('m', 'importedTag',
                             makeSig([kWasmExternRef], []));
  const moduleI32Tag = builder.addTag(kSig_v_i);
  const moduleExternTag =
      builder.addTag(makeSig([kWasmExternRef], []));

  builder.addExportOfKind('moduleI32Tag', kExternalTag, moduleI32Tag);
  builder.addExportOfKind('moduleExternTag', kExternalTag, moduleExternTag);
  builder.addExportOfKind('reexportedImportedTag', kExternalTag,
                          importedTagIndex);

  const module = keep('module', builder.toModule());
  const instance = keep('instance', new WebAssembly.Instance(module, imports));
  const exports = keep('exports', instance.exports);
  const payload = keep('modulePayload', {kind: 'module-exception-payload'});

  const moduleI32Exception =
      keep('moduleI32Exception',
           new WebAssembly.Exception(exports.moduleI32Tag, [55]));
  const moduleExternException =
      keep('moduleExternException',
           new WebAssembly.Exception(exports.moduleExternTag, [payload]));
  const importedException =
      keep('importedException',
           new WebAssembly.Exception(exports.reexportedImportedTag, [payload]));

  assertTrue(moduleI32Exception.is(exports.moduleI32Tag));
  assertTrue(moduleExternException.is(exports.moduleExternTag));
  assertTrue(importedException.is(importedTag));
  assertTrue(importedException.is(exports.reexportedImportedTag));
  assertEquals(55, moduleI32Exception.getArg(exports.moduleI32Tag, 0));
  assertSame(payload, moduleExternException.getArg(exports.moduleExternTag, 0));
  assertSame(payload, importedException.getArg(importedTag, 0));
  assertEquals({parameters: ['externref']},
               exports.reexportedImportedTag.type());
})();
