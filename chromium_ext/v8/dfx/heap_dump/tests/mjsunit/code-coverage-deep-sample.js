// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-stress-flush-code

// Keep block-coverage metadata alive for heap dump smoke. This follows V8's
// code-coverage mjsunit pattern but avoids exact source-range assertions here;
// cctests own exact heap graph checks.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  %DebugToggleBlockCoverage(true);

  function branchy(seed) {
    const local = {seed, path: []};
    if ((seed & 1) === 0) {
      local.path.push('even');
    } else {
      local.path.push('odd');
    }
    if (seed > 10) {
      local.path.push('large');
      return local;
    }
    local.path.push('small');
    return local;
  }

  function loopAndSwitch(seed) {
    const values = [];
    for (let i = 0; i < seed; ++i) {
      switch (i % 4) {
        case 0:
          values.push({zero: i});
          break;
        case 1:
          values.push({one: i});
          break;
        case 2:
          values.push({two: i});
          break;
        default:
          values.push({other: i});
          break;
      }
    }
    return values;
  }

  async function asyncCovered(seed) {
    const before = branchy(seed);
    await Promise.resolve({seed});
    return {before, after: loopAndSwitch(seed % 5)};
  }

  function* generatorCovered(seed) {
    yield branchy(seed);
    yield loopAndSwitch(seed % 6);
    return {done: seed};
  }

  class CoverageClass {
    static staticField = {kind: 'coverage-static'};

    constructor(seed) {
      this.seed = seed;
      this.created = branchy(seed);
    }

    method(delta) {
      return loopAndSwitch(this.seed + delta);
    }

    get summary() {
      return {seed: this.seed, path: this.created.path};
    }
  }

  const branchResults = keep('branchResults', []);
  for (let i = 0; i < 24; ++i) {
    branchResults.push(branchy(i));
  }

  const switchResults = keep('switchResults', [
    loopAndSwitch(0),
    loopAndSwitch(3),
    loopAndSwitch(9),
  ]);

  const asyncPromises = keep('asyncPromises', []);
  for (let i = 0; i < 8; ++i) {
    asyncPromises.push(asyncCovered(i + 3));
  }

  const generators = keep('generators', []);
  for (let i = 0; i < 8; ++i) {
    const generator = generatorCovered(i + 5);
    generators.push({
      generator,
      first: generator.next(),
      second: generator.next(),
    });
  }

  const classInstances = keep('classInstances', []);
  for (let i = 0; i < 12; ++i) {
    const instance = new CoverageClass(i);
    classInstances.push({
      instance,
      method: instance.method(2),
      summary: instance.summary,
    });
  }

  keep('branchy', branchy);
  keep('loopAndSwitch', loopAndSwitch);
  keep('asyncCovered', asyncCovered);
  keep('generatorCovered', generatorCovered);
  keep('CoverageClass', CoverageClass);
  keep('coverageSnapshot', %DebugCollectCoverage());

  assertEquals('even', branchResults[0].path[0]);
  assertEquals('odd', branchResults[1].path[0]);
  assertTrue(asyncPromises[0] instanceof Promise);
  assertFalse(generators[0].first.done);
  assertEquals(0, classInstances[0].instance.seed);
  assertTrue(Array.isArray(catalog.coverageSnapshot));

  globalThis.heapDumpCodeCoverageDeepSample = catalog;
})();
