// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-shadow-realm

// Keep ShadowRealm importValue state alive for heap dump smoke. This follows
// V8's ShadowRealm mjsunit importValue tests while preserving successful
// wrapped exports, caller-realm promises, module namespace state, and copied
// rejection errors.

const catalog = {};
function keep(name, value) {
  catalog[name] = value;
  return value;
}

globalThis.heapDumpShadowRealmScope = 'outer-scope';

const shadowRealm = keep('shadowRealm', new ShadowRealm());
const functionPromise = keep(
    'functionPromise',
    shadowRealm.importValue('./helpers/shadowrealm-importvalue-payload.mjs',
                            'exportedFunction'));
assertInstanceof(functionPromise, Promise);

const exportedFunction = keep('exportedFunction', await functionPromise);
assertEquals('function', typeof exportedFunction);
assertTrue(exportedFunction instanceof Function);

const functionResult = keep('functionResult', exportedFunction(23));
assertEquals('inner-scope:23', functionResult);
assertEquals('outer-scope', globalThis.heapDumpShadowRealmScope);

const markerPromise = keep(
    'markerPromise',
    shadowRealm.importValue('./helpers/shadowrealm-importvalue-payload.mjs',
                            'marker'));
assertInstanceof(markerPromise, Promise);
const marker = keep('marker', await markerPromise);
assertEquals('heap-dump-shadowrealm-importvalue', marker);

const objectPromise = keep(
    'objectPromise',
    shadowRealm.importValue('./helpers/shadowrealm-importvalue-payload.mjs',
                            'objectExport'));
assertInstanceof(objectPromise, Promise);
const objectError = keep('objectError', await objectPromise.then(
    () => null, (error) => error));
assertInstanceof(objectError, TypeError);
assertTrue(objectError.message.includes('is not a function'));

const missingExportPromise = keep(
    'missingExportPromise',
    shadowRealm.importValue('./helpers/shadowrealm-importvalue-payload.mjs',
                            'missingExport'));
assertInstanceof(missingExportPromise, Promise);
const missingExportError = keep('missingExportError',
                                await missingExportPromise.then(
                                    () => null, (error) => error));
assertInstanceof(missingExportError, TypeError);
assertTrue(missingExportError.message.includes('does not provide an export'));

const throwPromise = keep(
    'throwPromise',
    shadowRealm.importValue('./helpers/shadowrealm-importvalue-throw.mjs',
                            'marker'));
assertInstanceof(throwPromise, Promise);
const throwError = keep('throwError',
                        await throwPromise.then(() => null, (error) => error));
assertInstanceof(throwError, TypeError);
assertTrue(throwError.message.includes(
    'heap-dump-shadowrealm-importvalue-throw'));

const dynamicModule = keep(
    'dynamicModule',
    await import('./helpers/shadowrealm-importvalue-payload.mjs'));
assertEquals('heap-dump-shadowrealm-importvalue', dynamicModule.marker);
assertEquals('inner', dynamicModule.objectExport.realm);

keep('dynamicPromise',
     import('./helpers/shadowrealm-importvalue-payload.mjs'));
keep('catalog', catalog);
globalThis.heapDumpShadowRealmImportValueDeepSample = catalog;
