// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep CallSiteInfo, Error stack, eval-origin, constructor-frame, async-frame,
// and builtin-frame-adjacent objects alive for heap dump smoke. This follows
// V8 mjsunit stack-trace patterns while avoiding native syntax/debugger APIs.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function summarizeFrame(frame) {
    const summary = {
      thisValue: undefined,
      typeName: undefined,
      functionValue: undefined,
      functionName: undefined,
      methodName: undefined,
      fileName: undefined,
      lineNumber: undefined,
      columnNumber: undefined,
      evalOrigin: undefined,
      isToplevel: undefined,
      isEval: undefined,
      isNative: undefined,
      isConstructor: undefined,
      isAsync: undefined,
      isPromiseAll: undefined,
      promiseIndex: undefined,
      toStringValue: undefined,
    };
    const readers = [
      ['thisValue', () => frame.getThis()],
      ['typeName', () => frame.getTypeName()],
      ['functionValue', () => frame.getFunction()],
      ['functionName', () => frame.getFunctionName()],
      ['methodName', () => frame.getMethodName()],
      ['fileName', () => frame.getFileName()],
      ['lineNumber', () => frame.getLineNumber()],
      ['columnNumber', () => frame.getColumnNumber()],
      ['evalOrigin', () => frame.getEvalOrigin()],
      ['isToplevel', () => frame.isToplevel()],
      ['isEval', () => frame.isEval()],
      ['isNative', () => frame.isNative()],
      ['isConstructor', () => frame.isConstructor()],
      ['isAsync', () => typeof frame.isAsync === 'function' && frame.isAsync()],
      ['isPromiseAll',
       () => typeof frame.isPromiseAll === 'function' && frame.isPromiseAll()],
      ['promiseIndex',
       () => typeof frame.getPromiseIndex === 'function' ?
           frame.getPromiseIndex() : undefined],
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

  function capture(label, callback) {
    const state = {label};
    const previousPrepareStackTrace = Error.prepareStackTrace;
    Error.prepareStackTrace = function(error, frames) {
      state.error = error;
      state.frames = frames;
      state.frameSummaries = frames.slice(0, 12).map(summarizeFrame);
      return frames;
    };
    try {
      state.stack = callback();
    } finally {
      Error.prepareStackTrace = previousPrepareStackTrace;
    }
    return state;
  }

  class ConstructorFrameCarrier {
    constructor(seed) {
      this.seed = seed;
      const error = new Error('constructor-frame-' + seed);
      this.stack = error.stack;
    }

    methodFrame(delta) {
      return this.seed + delta;
    }
  }

  const plainStack = keep('plainStack', capture('plain', function plainOuter() {
    return (function plainMiddle() {
      return (function plainInner() {
        return new Error('plain-stack').stack;
      })();
    })();
  }));

  const methodCarrier = keep('methodCarrier', {
    label: 'method-carrier',
    methodCapture() {
      return capture('method', () => new Error('method-stack').stack);
    },
  });
  const methodStack = keep('methodStack', methodCarrier.methodCapture());

  const constructorStack = keep('constructorStack', capture('constructor', () => {
    return new ConstructorFrameCarrier(17).stack;
  }));

  const evalStack = keep('evalStack', capture('eval', () => eval(`
    (function evalOuter() {
      return (function evalInner() {
        return new Error('eval-stack').stack;
      })();
    })()
    //# sourceURL=heap-dump-callsite-eval.js
  `)));

  const indirectEval = (0, eval);
  const indirectEvalStack = keep('indirectEvalStack', capture(
      'indirect-eval', () => indirectEval(`
        (function indirectEvalOuter() {
          return new Error('indirect-eval-stack').stack;
        })()
        //# sourceURL=heap-dump-callsite-indirect-eval.js
      `)));

  const boundTarget = function boundTarget(delta) {
    if (delta === 0) return new Error('bound-stack').stack;
    return delta;
  };
  const boundStack = keep('boundStack', capture(
      'bound', () => boundTarget.bind({boundThis: true})(0)));

  const builtinStack = keep('builtinStack', capture('builtin', () => {
    const throwingValue = {
      valueOf() {
        throw new Error('builtin-valueof-stack');
      },
    };
    try {
      Math.max(throwingValue);
    } catch (e) {
      return e.stack;
    }
    return null;
  }));

  const asyncState = keep('asyncState', {});
  async function asyncLeaf() {
    const error = new Error('async-stack');
    asyncState.stack = error.stack;
    return error.stack;
  }
  async function asyncRoot() {
    await Promise.resolve({asyncMarker: true});
    return asyncLeaf();
  }
  const asyncStackPromise = keep('asyncStackPromise', capture(
      'async', () => asyncRoot().then((stack) => {
        asyncState.resolvedStack = stack;
        return stack;
      })));

  const preparedErrors = keep('preparedErrors', [
    plainStack.error,
    methodStack.error,
    constructorStack.error,
    evalStack.error,
    indirectEvalStack.error,
    boundStack.error,
    builtinStack.error,
  ]);
  keep('ConstructorFrameCarrier', ConstructorFrameCarrier);
  keep('boundTarget', boundTarget);

  assertTrue(Array.isArray(plainStack.frames));
  assertTrue(plainStack.frames.length > 0);
  assertTrue(methodStack.frameSummaries.some(
      (summary) => summary.methodName === 'methodCapture' ||
          summary.functionName === 'methodCapture'));
  assertTrue(constructorStack.frameSummaries.some(
      (summary) => summary.isConstructor === true));
  assertTrue(evalStack.frames.length > 0);
  assertTrue(indirectEvalStack.frames.length > 0);
  assertTrue(boundStack.frames.length > 0);
  assertTrue(builtinStack.frames.length > 0);
  assertTrue(asyncStackPromise.stack instanceof Promise);

  globalThis.heapDumpCallSiteStackDeepSample = catalog;
})();
