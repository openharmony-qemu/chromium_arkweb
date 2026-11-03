// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep function/source/feedback-adjacent metadata alive for heap dump smoke.
// This intentionally avoids native syntax so it can run through the generic
// heap-dump mjsunit runner and the CLI dump/translate smoke path.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function warmNamedFunction(object, key, value) {
    object[key] = value;
    return object[key] + object.fixed + object.method(value);
  }

  const warmTargets = [];
  for (let i = 0; i < 64; ++i) {
    const target = {
      fixed: i,
      method(delta) {
        return this.fixed + delta;
      },
    };
    warmTargets.push({
      target,
      result: warmNamedFunction(target, 'dynamic_' + (i % 8), i + 1),
    });
  }
  keep('warmNamedFunction', warmNamedFunction);
  keep('warmTargets', warmTargets);

  const arrowFactory = (seed) => {
    const captured = {seed};
    return (delta = 1) => captured.seed + delta;
  };
  const arrowClosure = keep('arrowClosure', arrowFactory(10));

  const methodCarrier = keep('methodCarrier', {
    label: 'method-carrier',
    conciseMethod(seed) {
      const local = {seed, owner: this};
      return function methodInner(delta) {
        return local.owner.label + ':' + (local.seed + delta);
      };
    },
    get accessorMethod() {
      return this.label;
    },
  });
  const methodClosure = keep('methodClosure', methodCarrier.conciseMethod(20));

  const computedName = 'computed-' + 'method';
  class MetadataBase {
    constructor(seed) {
      this.baseSeed = seed;
    }

    baseRead() {
      return this.baseSeed;
    }
  }

  class MetadataDerived extends MetadataBase {
    static staticField = {kind: 'static-field'};
    static ['static' + 'Computed']() {
      return this.staticField.kind;
    }

    instanceField = {kind: 'instance-field'};

    [computedName](delta) {
      return super.baseRead() + delta;
    }
  }
  const classInstance = keep('classInstance', new MetadataDerived(30));
  keep('classConstructor', MetadataDerived);
  keep('classPrototype', MetadataDerived.prototype);

  const functionConstructor = keep(
      'functionConstructor',
      new Function(
          'seed',
          'return function generatedFunction(delta) { return seed + delta; }'));
  const generatedFunction = keep('generatedFunction', functionConstructor(40));

  const directEvalClosure = keep('directEvalClosure', eval(`
    (function makeDirectEvalClosure(seed) {
      const evalLocal = {seed};
      return function directEvalInner(delta) {
        return evalLocal.seed + delta;
      };
    })(50)
    //# sourceURL=heap-dump-direct-eval.js
  `));

  const indirectEval = (0, eval);
  const indirectEvalClosure = keep('indirectEvalClosure', indirectEval(`
    (function makeIndirectEvalClosure(seed) {
      var indirectLocal = {seed: seed};
      return function indirectEvalInner(delta) {
        return indirectLocal.seed + delta;
      };
    })(60)
    //# sourceURL=heap-dump-indirect-eval.js
  `));

  function makeBlockClosure(seed) {
    let result;
    {
      const blockLocal = {seed};
      result = function blockInner(delta) {
        return blockLocal.seed + delta;
      };
    }
    return result;
  }
  const blockClosure = keep('blockClosure', makeBlockClosure(70));

  function makeDefaultParameterClosure(seed, helper = (delta) => seed + delta) {
    return function defaultParameterInner(delta) {
      return helper(delta);
    };
  }
  const defaultParameterClosure =
      keep('defaultParameterClosure', makeDefaultParameterClosure(80));

  function makeDestructuringClosure({seed}, [deltaBase]) {
    return function destructuringInner(delta) {
      return seed + deltaBase + delta;
    };
  }
  const destructuringClosure =
      keep('destructuringClosure', makeDestructuringClosure({seed: 90}, [2]));

  const stackState = {};
  const previousPrepareStackTrace = Error.prepareStackTrace;
  Error.prepareStackTrace = function(error, frames) {
    stackState.error = error;
    stackState.frames = frames;
    stackState.first = frames[0];
    return frames;
  };
  try {
    function stackOuter() {
      return stackMiddle();
    }
    function stackMiddle() {
      return stackInner();
    }
    function stackInner() {
      const error = new Error('heap-dump function metadata stack');
      Error.captureStackTrace(error, stackInner);
      return error.stack;
    }
    keep('structuredStackFrames', stackOuter());
  } finally {
    Error.prepareStackTrace = previousPrepareStackTrace;
  }
  keep('stackState', stackState);

  const regexpCatalog = keep('regexpCatalog', {
    oneByteGlobal: /function-metadata-(\d+)/g,
    unicodeNamed: /(?<kind>heap)-(?<name>dump)/du,
    stickyIndices: /meta(?<digits>\d+)/dy,
    twoByte: new RegExp('metadata-\\u0100-(?<tail>.)', 'u'),
  });
  keep('regexpResults', [
    regexpCatalog.oneByteGlobal.exec('function-metadata-42'),
    regexpCatalog.unicodeNamed.exec('heap-dump'),
    regexpCatalog.stickyIndices.exec('meta123'),
    regexpCatalog.twoByte.exec('metadata-\u0100-z'),
  ]);

  assertEquals(11, arrowClosure(1));
  assertEquals('method-carrier:23', methodClosure(3));
  assertEquals('method-carrier', methodCarrier.accessorMethod);
  assertEquals(35, classInstance[computedName](5));
  assertEquals('static-field', MetadataDerived.staticComputed());
  assertEquals(42, generatedFunction(2));
  assertEquals(53, directEvalClosure(3));
  assertEquals(64, indirectEvalClosure(4));
  assertEquals(75, blockClosure(5));
  assertEquals(86, defaultParameterClosure(6));
  assertEquals(99, destructuringClosure(7));
  assertTrue(Array.isArray(stackState.frames));
  assertTrue(stackState.frames.length > 0);
  assertEquals('42', catalog.regexpResults[0][1]);
  assertEquals('dump', catalog.regexpResults[1].groups.name);
  assertEquals('123', catalog.regexpResults[2].groups.digits);
  assertEquals('z', catalog.regexpResults[3].groups.tail);

  globalThis.heapDumpFunctionMetadataDeepSample = catalog;
})();
