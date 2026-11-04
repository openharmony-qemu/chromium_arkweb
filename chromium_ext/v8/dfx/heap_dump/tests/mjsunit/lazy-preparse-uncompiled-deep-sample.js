// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep lazy/preparse/uncompiled function shapes alive for heap dump smoke.
// The assertions only pin observable JS behavior; the sample intentionally
// keeps many returned functions and methods uncalled so their internal
// bytecode/preparse state can vary with V8 heuristics without making the test
// brittle.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function makeNestedLazyClosures(seed) {
    const anchor = {seed, tag: 'nested'};

    function hotClosure(delta) {
      return anchor.seed + delta;
    }

    function lazyNeverCalledA(delta) {
      const local = {delta, anchor};
      return local.anchor.seed + local.delta + 1000;
    }

    function lazyNeverCalledB(delta) {
      function deeperLazy(extra) {
        return anchor.seed + delta + extra;
      }
      return deeperLazy(2000);
    }

    function lazyWithDefault(delta = anchor.seed) {
      return delta + 3000;
    }

    function lazyWithDestructuring({value}, [extra]) {
      return anchor.seed + value + extra + 4000;
    }

    class LazyLocalClass {
      constructor(offset) {
        this.offset = offset;
      }

      hotMethod(delta) {
        return anchor.seed + this.offset + delta;
      }

      lazyMethod(delta) {
        return anchor.seed + this.offset + delta + 5000;
      }

      get lazyAccessor() {
        return anchor.tag + ':' + this.offset;
      }

      static lazyStatic(delta) {
        return seed + delta + 6000;
      }
    }

    return {
      anchor,
      hotClosure,
      lazyNeverCalledA,
      lazyNeverCalledB,
      lazyWithDefault,
      lazyWithDestructuring,
      LazyLocalClass,
      instance: new LazyLocalClass(7),
    };
  }

  const nested = keep('nested', makeNestedLazyClosures(10));
  keep('nestedPrototype', nested.LazyLocalClass.prototype);
  keep('nestedConstructor', nested.LazyLocalClass);

  const generatedFactory = keep('generatedFactory', new Function('seed', `
    const anchor = {seed: seed, tag: 'generated'};

    function generatedHot(delta) {
      return anchor.seed + delta;
    }

    function generatedLazyNeverCalledA(delta) {
      const local = {anchor: anchor, delta: delta};
      return local.anchor.seed + local.delta + 7000;
    }

    function generatedLazyNeverCalledB(delta) {
      return (function generatedLazyInner(extra) {
        return anchor.seed + delta + extra + 8000;
      })(1);
    }

    class GeneratedLazyClass {
      constructor(offset) {
        this.offset = offset;
      }

      hotMethod(delta) {
        return anchor.seed + this.offset + delta;
      }

      lazyMethod(delta) {
        return anchor.seed + this.offset + delta + 9000;
      }
    }

    return {
      anchor,
      generatedHot,
      generatedLazyNeverCalledA,
      generatedLazyNeverCalledB,
      GeneratedLazyClass,
      instance: new GeneratedLazyClass(11),
    };
    //# sourceURL=heap-dump-generated-lazy-preparse.js
  `));

  const generated = keep('generated', generatedFactory(20));
  keep('generatedPrototype', generated.GeneratedLazyClass.prototype);
  keep('generatedConstructor', generated.GeneratedLazyClass);

  const directEvalFactory = keep('directEvalFactory', eval(`
    (function makeDirectEvalLazyFactory(seed) {
      const anchor = {seed: seed, tag: 'direct-eval'};

      function evalHot(delta) {
        return anchor.seed + delta;
      }

      function evalLazyNeverCalled(delta) {
        return anchor.seed + delta + 10000;
      }

      return {
        anchor,
        evalHot,
        evalLazyNeverCalled,
      };
    })
    //# sourceURL=heap-dump-direct-eval-lazy-preparse.js
  `));

  const directEval = keep('directEval', directEvalFactory(30));

  const indirectEval = (0, eval);
  const indirectEvalFactory = keep('indirectEvalFactory', indirectEval(`
    (function makeIndirectEvalLazyFactory(seed) {
      var anchor = {seed: seed, tag: 'indirect-eval'};

      function indirectHot(delta) {
        return anchor.seed + delta;
      }

      function indirectLazyNeverCalled(delta) {
        return anchor.seed + delta + 11000;
      }

      return {
        anchor: anchor,
        indirectHot: indirectHot,
        indirectLazyNeverCalled: indirectLazyNeverCalled,
      };
    })
    //# sourceURL=heap-dump-indirect-eval-lazy-preparse.js
  `));

  const indirectEvalState = keep('indirectEval', indirectEvalFactory(40));

  const arrowLazyFactory = keep('arrowLazyFactory', (seed) => {
    const anchor = {seed, tag: 'arrow'};
    const hotArrow = (delta) => anchor.seed + delta;
    const lazyArrowNeverCalled = (delta) => {
      const local = {anchor, delta};
      return local.anchor.seed + local.delta + 12000;
    };
    return {anchor, hotArrow, lazyArrowNeverCalled};
  });

  const arrowState = keep('arrowState', arrowLazyFactory(50));

  assertEquals(12, nested.hotClosure(2));
  assertEquals(20, nested.instance.hotMethod(3));
  assertEquals(23, generated.generatedHot(3));
  assertEquals(36, generated.instance.hotMethod(5));
  assertEquals(34, directEval.evalHot(4));
  assertEquals(45, indirectEvalState.indirectHot(5));
  assertEquals(56, arrowState.hotArrow(6));

  assertEquals(1013, nested.lazyNeverCalledA(3));
  assertEquals(2014, nested.lazyNeverCalledB(4));
  assertEquals(3010, nested.lazyWithDefault());
  assertEquals(4015, nested.lazyWithDestructuring({value: 2}, [3]));
  assertEquals(5018, nested.instance.lazyMethod(1));
  assertEquals('nested:7', nested.instance.lazyAccessor);
  assertEquals(6012, nested.LazyLocalClass.lazyStatic(2));
  assertEquals(7024, generated.generatedLazyNeverCalledA(4));
  assertEquals(8026, generated.generatedLazyNeverCalledB(5));
  assertEquals(9032, generated.instance.lazyMethod(1));
  assertEquals(10036, directEval.evalLazyNeverCalled(6));
  assertEquals(11047, indirectEvalState.indirectLazyNeverCalled(7));
  assertEquals(12058, arrowState.lazyArrowNeverCalled(8));

  globalThis.heapDumpLazyPreparseUncompiledDeepSample = catalog;
})();
