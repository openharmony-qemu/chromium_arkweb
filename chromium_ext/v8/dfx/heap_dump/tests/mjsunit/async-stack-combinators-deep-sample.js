// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --async-stack-traces

// Keep async-stack CallSiteInfo variants alive for heap dump smoke. This
// follows V8 async-stack-traces Promise.all/allSettled/any patterns and retains
// CallSite arrays, AggregateError payloads, async generator frames, and Promise
// combinator frame summaries.

(function () {
  'use strict';

  const catalog = {
    records: [],
    promises: [],
    errors: [],
  };

  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function summarizeFrame(frame) {
    const summary = {};
    const readers = [
      ['functionName', () => frame.getFunctionName()],
      ['methodName', () => frame.getMethodName()],
      ['fileName', () => frame.getFileName()],
      ['lineNumber', () => frame.getLineNumber()],
      ['columnNumber', () => frame.getColumnNumber()],
      ['isAsync', () => typeof frame.isAsync === 'function' && frame.isAsync()],
      ['isPromiseAll',
       () => typeof frame.isPromiseAll === 'function' && frame.isPromiseAll()],
      ['promiseIndex',
       () => typeof frame.getPromiseIndex === 'function' ?
           frame.getPromiseIndex() : undefined],
      ['isConstructor', () => frame.isConstructor()],
      ['isEval', () => frame.isEval()],
      ['toStringValue', () => frame.toString()],
    ];
    for (const [name, reader] of readers) {
      try {
        summary[name] = reader();
      } catch (e) {
        summary[name + 'Error'] = e;
      }
    }
    return summary;
  }

  function recordStack(label, error) {
    const frames = error.stack;
    const record = {
      label,
      error,
      frames,
      summaries: Array.isArray(frames) ? frames.map(summarizeFrame) : [],
    };
    catalog.records.push(record);
    catalog.errors.push(error);
    return record;
  }

  async function fine(label) {
    await Promise.resolve({kind: 'fine', label});
    return label;
  }

  async function thrower(label) {
    await fine('inner-' + label);
    throw new Error('heap-dump-async-stack-' + label);
  }

  async function promiseAllDriver() {
    return await Promise.all([
      fine('all-0'),
      fine('all-1'),
      thrower('all-2'),
      thrower('all-3'),
    ]);
  }

  async function promiseAllSettledDriver() {
    return await Promise.allSettled([
      fine('settled-0'),
      thrower('settled-1'),
      fine('settled-2'),
      thrower('settled-3'),
    ]);
  }

  async function promiseAnyDriver() {
    return await Promise.any([
      thrower('any-0'),
      thrower('any-1'),
      thrower('any-2'),
    ]);
  }

  async function* asyncGeneratorDriver() {
    await fine('generator-before-yield');
    yield {kind: 'generator-yield'};
    await fine('generator-before-throw');
    throw new Error('heap-dump-async-generator-stack');
  }

  async function runAsyncStackCatalog() {
    try {
      await promiseAllDriver();
    } catch (e) {
      recordStack('promise-all', e);
    }

    const allSettled = await promiseAllSettledDriver();
    keep('allSettledResults', allSettled);
    for (let i = 0; i < allSettled.length; ++i) {
      if (allSettled[i].status === 'rejected') {
        recordStack('promise-all-settled-' + i, allSettled[i].reason);
      }
    }

    if (typeof Promise.any === 'function') {
      try {
        await promiseAnyDriver();
      } catch (e) {
        keep('aggregateAnyError', e);
        if (Array.isArray(e.errors)) {
          for (let i = 0; i < e.errors.length; ++i) {
            recordStack('promise-any-' + i, e.errors[i]);
          }
        }
      }
    }

    const generator = keep('asyncGenerator', asyncGeneratorDriver());
    const first = await generator.next();
    keep('asyncGeneratorFirst', first);
    try {
      await generator.next();
    } catch (e) {
      recordStack('async-generator', e);
    }

    return catalog;
  }

  const previousPrepareStackTrace = Error.prepareStackTrace;
  Error.prepareStackTrace = function(error, frames) {
    return frames;
  };

  keep('drivers', {
    fine,
    thrower,
    promiseAllDriver,
    promiseAllSettledDriver,
    promiseAnyDriver,
    asyncGeneratorDriver,
    runAsyncStackCatalog,
  });

  const catalogPromise = keep('catalogPromise', runAsyncStackCatalog());
  catalog.promises.push(catalogPromise);

  assertPromiseResult(catalogPromise, () => {
    Error.prepareStackTrace = previousPrepareStackTrace;
    assertTrue(catalog.records.length >= 4);
    assertTrue(catalog.records.some((record) => record.summaries.some(
        (summary) => summary.functionName === 'thrower')));
    assertTrue(catalog.records.some((record) => record.summaries.some(
        (summary) => summary.isAsync === true)));
    assertTrue(catalog.records.some((record) => record.summaries.some(
        (summary) => summary.promiseIndex !== null &&
            summary.promiseIndex !== undefined)));
    assertTrue(catalog.asyncGeneratorFirst.done === false);
    assertTrue(catalog.errors.every((error) => error instanceof Error));
    globalThis.heapDumpAsyncStackCombinatorsDeepSample = catalog;
  }, (error) => {
    Error.prepareStackTrace = previousPrepareStackTrace;
    throw error;
  });
})();
