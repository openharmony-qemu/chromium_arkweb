// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Heap-dump sample corpus:
// - dynamic import-attributes failure paths
// - options objects with throwing getters and Proxy traps
// - retained rejected promises, rejection reasons, proxy handlers, and a
//   successful JSON module namespace for contrast

const catalog = {};
function keep(name, value) {
  catalog[name] = value;
  return value;
}

const jsonSpecifier = './helpers/import-attributes-payload.json';
const moduleSpecifier = './module-live-binding-helper.mjs';

const badWithGetter = keep('badWithGetter', {
  get with() {
    throw 'heap-dump bad with getter';
  },
});
const badAttributeGetter = keep('badAttributeGetter', {
  with: {
    get assertionKey() {
      throw 'heap-dump bad assertion getter';
    },
  },
});
const badOwnKeysProxy = keep(
    'badOwnKeysProxy',
    new Proxy({}, {
      ownKeys() {
        throw 'heap-dump bad ownKeys';
      },
    }));
const badDescriptorProxy = keep(
    'badDescriptorProxy',
    new Proxy({type: 'json'}, {
      getOwnPropertyDescriptor() {
        throw 'heap-dump bad getOwnPropertyDescriptor';
      },
    }));
const badDescriptorOptions =
    keep('badDescriptorOptions', {with: badDescriptorProxy});
const badOwnKeysOptions = keep('badOwnKeysOptions', {with: badOwnKeysProxy});

const cases = keep('cases', [
  ['nullOptions', jsonSpecifier, null],
  ['numericOptions', jsonSpecifier, 7],
  ['stringOptions', jsonSpecifier, 'string'],
  ['nullWith', jsonSpecifier, {with: null}],
  ['numericWith', jsonSpecifier, {with: 7}],
  ['nonStringAttributeNull', jsonSpecifier, {with: {type: null}}],
  ['nonStringAttributeObject', jsonSpecifier, {with: {type: {}}}],
  ['badTypeAttribute', jsonSpecifier, {with: {type: 'notARealType'}}],
  ['badWithGetter', jsonSpecifier, badWithGetter],
  ['badAttributeGetter', jsonSpecifier, badAttributeGetter],
  ['badOwnKeysProxy', moduleSpecifier, badOwnKeysOptions],
  ['badDescriptorProxy', jsonSpecifier, badDescriptorOptions],
]);

function dynamicImportResult(name, specifier, options) {
  const promise = import(specifier, options).then(
      (namespace) => ({name, status: 'fulfilled', namespace}),
      (reason) => ({name, status: 'rejected', reason}));
  keep(`${name}Promise`, promise);
  return promise;
}

const results =
    keep('results', await Promise.all(cases.map((entry) =>
      dynamicImportResult(entry[0], entry[1], entry[2]))));
const successfulJsonModule =
    keep('successfulJsonModule',
         await import(jsonSpecifier, {with: {type: 'json'}}));
const successfulModule =
    keep('successfulModule', await import(moduleSpecifier));

const rejectionReasons = keep(
    'rejectionReasons',
    Object.fromEntries(results.map((result) => [
      result.name,
      result.reason,
    ])));
const rejectionMessages = keep(
    'rejectionMessages',
    Object.fromEntries(results.map((result) => [
      result.name,
      typeof result.reason === 'string' ? result.reason :
          String(result.reason && result.reason.message),
    ])));
const allSettled = keep(
    'allSettled',
    await Promise.allSettled([
      import(jsonSpecifier, {with: {type: 'json'}}),
      import(jsonSpecifier, {with: {type: 'notARealType'}}),
      import(moduleSpecifier, badOwnKeysOptions),
    ]));

assertTrue(results.every((result) => result.status === 'rejected'));
assertEquals('heap-dump bad with getter',
             rejectionReasons.badWithGetter);
assertEquals('heap-dump bad assertion getter',
             rejectionReasons.badAttributeGetter);
assertEquals('heap-dump bad ownKeys', rejectionReasons.badOwnKeysProxy);
assertEquals('heap-dump bad getOwnPropertyDescriptor',
             rejectionReasons.badDescriptorProxy);
assertTrue(rejectionMessages.nullOptions.includes(
    'second argument to import() must be an object'));
assertTrue(rejectionMessages.nullWith.includes(
    "'with' option must be an object"));
assertTrue(rejectionMessages.nonStringAttributeNull.includes(
    'Import attribute value must be a string'));
assertTrue(rejectionMessages.badTypeAttribute.length > 0);
assertEquals('heap-dump-json-module', successfulJsonModule.default.label);
assertEquals(0, successfulModule.liveCounter);
assertEquals('function', typeof successfulModule.bumpLiveCounter);
assertEquals('fulfilled', allSettled[0].status);
assertEquals('rejected', allSettled[1].status);
assertEquals('rejected', allSettled[2].status);

keep('catalog', catalog);
globalThis.__heapDumpModuleImportAttributesErrors = catalog;
