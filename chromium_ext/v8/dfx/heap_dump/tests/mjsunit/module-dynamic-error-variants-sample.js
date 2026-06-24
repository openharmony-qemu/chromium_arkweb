// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Heap-dump shape sample for dynamic-import failure states. This complements
// namespace/live-binding/TLA samples by retaining rejected import promises,
// missing/evaluation errors, repeated failed imports, and allSettled result
// objects for errored SourceTextModule paths.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  async function runDynamicImportErrorVariants() {
    const missingImport = keep(
        'missingImport',
        import('./helpers/module-missing-helper.mjs').catch((error) => error));
    const throwingImport = keep(
        'throwingImport',
        import('./helpers/module-throwing-helper.mjs').catch((error) => error));
    const throwingImportAgain = keep(
        'throwingImportAgain',
        import('./helpers/module-throwing-helper.mjs')
            .catch((error) => error));

    const successfulNamespace = keep(
        'successfulNamespace',
        await import('./module-live-binding-helper.mjs'));

    const rejectedResults = keep('rejectedResults', await Promise.all([
      missingImport,
      throwingImport,
      throwingImportAgain,
    ]));

    const allSettled = keep('allSettled', await Promise.allSettled([
      import('./helpers/module-missing-helper.mjs'),
      import('./helpers/module-throwing-helper.mjs'),
      import('./module-live-binding-helper.mjs'),
    ]));

    const repeatedMissing = keep(
        'repeatedMissing',
        await Promise.allSettled([
          import('./helpers/module-missing-helper.mjs'),
          import('./helpers/module-missing-helper.mjs'),
        ]));

    const errorSummaries = keep(
        'errorSummaries',
        rejectedResults.map((error) => ({
          name: error && error.name,
          message: error && String(error.message),
          stack: error && error.stack,
        })));

    const namespaceKeys =
        keep('namespaceKeys', Reflect.ownKeys(successfulNamespace));
    const namespaceDescriptors = keep(
        'namespaceDescriptors',
        Object.fromEntries(namespaceKeys.map((key) => [
          key,
          Object.getOwnPropertyDescriptor(successfulNamespace, key),
        ])));

    assertEquals(3, rejectedResults.length);
    assertTrue(rejectedResults.every((error) => error instanceof Error));
    assertTrue(errorSummaries.some((summary) =>
      /module-throwing-helper/.test(summary.message)));
    assertEquals('fulfilled', allSettled[2].status);
    assertEquals('rejected', allSettled[0].status);
    assertEquals('rejected', repeatedMissing[0].status);
    assertTrue(namespaceKeys.includes('liveCounter'));
    assertTrue(namespaceDescriptors.liveCounter.enumerable);

    globalThis.__heapDumpModuleDynamicErrorVariantsSample = {
      catalog,
      runDynamicImportErrorVariants,
    };
    return catalog;
  }

  const runPromise = keep('runPromise', runDynamicImportErrorVariants());
  assertPromiseResult(runPromise, (result) => {
    assertSame(catalog, result);
    assertEquals(3, catalog.rejectedResults.length);
  });
})();
