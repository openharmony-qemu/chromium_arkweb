// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --max-lazy

// Keep uncalled lazy/preparse candidates alive. Unlike
// lazy-preparse-uncompiled-deep-sample.js, this test intentionally avoids
// invoking most stored functions so heap dump sees more boundary states around
// SharedFunctionInfo, UncompiledData, PreparseData, and generated source.

(function () {
  'use strict';

  const catalog = [];
  function keep(label, value) {
    catalog.push({label, value});
    return value;
  }

  function makeOrdinaryBoundary(seed) {
    const anchor = {seed, tag: 'ordinary'};

    function hot(delta) {
      return anchor.seed + delta;
    }

    function lazyDefault(delta = anchor.seed) {
      return delta + 1000;
    }

    function lazyDestructuring({left}, [right], ...rest) {
      return anchor.seed + left + right + rest.length + 2000;
    }

    function lazyNested(delta) {
      function inner(extra) {
        return anchor.seed + delta + extra + 3000;
      }
      return inner(1);
    }

    async function lazyAsync(value) {
      return anchor.seed + value + 4000;
    }

    function* lazyGenerator(value) {
      yield anchor.seed + value + 5000;
    }

    const lazyArrow = (value) => {
      const local = {anchor, value};
      return local.anchor.seed + local.value + 6000;
    };

    return {
      anchor,
      hot,
      lazyDefault,
      lazyDestructuring,
      lazyNested,
      lazyAsync,
      lazyGenerator,
      lazyArrow,
    };
  }

  const ordinary = keep('ordinary', makeOrdinaryBoundary(10));

  class BoundaryClass {
    #privateState = {base: 20};

    constructor(offset) {
      this.offset = offset;
    }

    hot(delta) {
      return this.#privateState.base + this.offset + delta;
    }

    lazyMethod(delta = this.offset) {
      return this.#privateState.base + delta + 7000;
    }

    get lazyAccessor() {
      return this.#privateState.base + this.offset + 8000;
    }

    static lazyStatic({delta}) {
      return delta + 9000;
    }
  }

  const klass = keep('class', BoundaryClass);
  const instance = keep('class-instance', new BoundaryClass(3));
  keep('class-prototype', BoundaryClass.prototype);

  const generatedFactory = keep('generated-factory', new Function('seed', `
    const anchor = {seed: seed, tag: 'generated-boundary'};

    function hot(delta) {
      return anchor.seed + delta;
    }

    function generatedLazyDefault(delta = anchor.seed) {
      return delta + 10000;
    }

    function generatedLazyNested(delta) {
      return (function generatedLazyInner(extra) {
        return anchor.seed + delta + extra + 11000;
      })(1);
    }

    async function generatedLazyAsync(value) {
      return anchor.seed + value + 12000;
    }

    function* generatedLazyGenerator(value) {
      yield anchor.seed + value + 13000;
    }

    class GeneratedBoundaryClass {
      constructor(offset) {
        this.offset = offset;
      }

      hot(delta) {
        return anchor.seed + this.offset + delta;
      }

      lazyMethod(delta) {
        return anchor.seed + this.offset + delta + 14000;
      }
    }

    return {
      anchor,
      hot,
      generatedLazyDefault,
      generatedLazyNested,
      generatedLazyAsync,
      generatedLazyGenerator,
      GeneratedBoundaryClass,
      instance: new GeneratedBoundaryClass(5),
    };
    //# sourceURL=heap-dump-generated-lazy-uncompiled-boundary.js
  `));

  const generated = keep('generated', generatedFactory(30));
  keep('generated-class', generated.GeneratedBoundaryClass);
  keep('generated-prototype', generated.GeneratedBoundaryClass.prototype);

  const directEvalFactory = keep('direct-eval-factory', eval(`
    (function makeDirectEvalBoundary(seed) {
      const anchor = {seed: seed, tag: 'direct-eval-boundary'};

      function hot(delta) {
        return anchor.seed + delta;
      }

      function directEvalLazy(delta) {
        return anchor.seed + delta + 15000;
      }

      const directEvalLazyArrow = (delta) => anchor.seed + delta + 16000;

      return {anchor, hot, directEvalLazy, directEvalLazyArrow};
    })
    //# sourceURL=heap-dump-direct-eval-lazy-uncompiled-boundary.js
  `));

  const directEval = keep('direct-eval', directEvalFactory(40));

  const indirectEval = (0, eval);
  const indirectEvalFactory = keep('indirect-eval-factory', indirectEval(`
    (function makeIndirectEvalBoundary(seed) {
      var anchor = {seed: seed, tag: 'indirect-eval-boundary'};

      function hot(delta) {
        return anchor.seed + delta;
      }

      function indirectEvalLazy(delta) {
        return anchor.seed + delta + 17000;
      }

      return {
        anchor: anchor,
        hot: hot,
        indirectEvalLazy: indirectEvalLazy,
      };
    })
    //# sourceURL=heap-dump-indirect-eval-lazy-uncompiled-boundary.js
  `));

  const indirectEvalState = keep('indirect-eval', indirectEvalFactory(50));

  const lazyFunctionArray = keep('lazy-function-array', [
    ordinary.lazyDefault,
    ordinary.lazyDestructuring,
    ordinary.lazyNested,
    ordinary.lazyAsync,
    ordinary.lazyGenerator,
    ordinary.lazyArrow,
    klass.prototype.lazyMethod,
    Object.getOwnPropertyDescriptor(klass.prototype, 'lazyAccessor').get,
    klass.lazyStatic,
    generated.generatedLazyDefault,
    generated.generatedLazyNested,
    generated.generatedLazyAsync,
    generated.generatedLazyGenerator,
    generated.GeneratedBoundaryClass.prototype.lazyMethod,
    directEval.directEvalLazy,
    directEval.directEvalLazyArrow,
    indirectEvalState.indirectEvalLazy,
  ]);

  assertEquals(12, ordinary.hot(2));
  assertEquals(27, instance.hot(4));
  assertEquals(36, generated.hot(6));
  assertEquals(38, generated.instance.hot(3));
  assertEquals(48, directEval.hot(8));
  assertEquals(60, indirectEvalState.hot(10));

  assertEquals('function', typeof lazyFunctionArray[0]);
  assertEquals('function', typeof lazyFunctionArray[3]);
  assertEquals('function', typeof lazyFunctionArray[8]);
  assertEquals('function', typeof lazyFunctionArray[13]);
  assertEquals(17, lazyFunctionArray.length);

  globalThis.heapDumpLazyUncompiledBoundaryDeepSample = catalog;
})();
