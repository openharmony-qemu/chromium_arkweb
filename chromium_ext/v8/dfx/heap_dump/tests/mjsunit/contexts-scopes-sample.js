// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep representative scope/context shapes alive for heap dump smoke. This
// file is intentionally sloppy-mode so `with` can construct a WithContext.

let scriptLetObject = {tag: 'script-let'};
const scriptConstArray = [scriptLetObject, 'script-const'];
var scriptVarObject = {tag: 'script-var'};

function scriptContextClosure() {
  return scriptLetObject.tag + ':' + scriptConstArray[1] + ':' +
      scriptVarObject.tag;
}

function makeFunctionContext(seed, {named}, ...rest) {
  let functionLocal = {seed, named, rest};
  const functionConst = ['function-const', functionLocal];
  return function contextClosure(extra) {
    return functionLocal.seed + extra + functionConst[1].rest.length;
  };
}

function makeBlockContext(seed) {
  let result;
  {
    let blockLocal = {seed};
    const blockConst = ['block-const', blockLocal];
    result = function blockContextClosure(delta) {
      return blockLocal.seed + delta + blockConst[0].length;
    };
  }
  return result;
}

function makeCatchContext() {
  try {
    throw {message: 'heap-dump catch context', code: 17};
  } catch (caught) {
    let catchLocal = {caught};
    return function catchContextClosure() {
      return catchLocal.caught.message + ':' + caught.code;
    };
  }
}

function makeEvalContext(seed) {
  let outer = {seed};
  return eval(`
    let evalLocal = {label: 'eval-context', outer};
    (function evalContextClosure(delta) {
      return evalLocal.label + ':' + (evalLocal.outer.seed + delta);
    })
  `);
}

function makeWithContext() {
  var scopeObject = {
    scopedValue: 31,
    nested: {keep: 11},
  };
  with (scopeObject) {
    return function withContextClosure(delta) {
      return scopedValue + delta + nested.keep + scopeObject.scopedValue;
    };
  }
}

function* makeSuspendedGenerator(seed) {
  let generatorLocal = {seed};
  const yielded = function generatorContextClosure(delta) {
    return generatorLocal.seed + delta;
  };
  yield yielded;
  return generatorLocal;
}

async function makeAwaitContext(blocker, seed) {
  let awaitLocal = {seed};
  const awaitConst = ['await-context', awaitLocal];
  await blocker;
  return awaitConst[1];
}

async function* makeQueuedAsyncGenerator(seed) {
  let asyncGeneratorLocal = {seed};
  yield function asyncGeneratorContextClosure(delta) {
    return asyncGeneratorLocal.seed + delta;
  };
  await new Promise(() => {});
}

class ScopeCarrier {
  static staticContextFactory(seed) {
    let staticLocal = {seed};
    return function staticMethodContextClosure(delta) {
      return staticLocal.seed + delta;
    };
  }

  makeMethodClosure(seed) {
    let methodLocal = {seed, receiver: this};
    return function methodContextClosure(delta) {
      return methodLocal.seed + delta + methodLocal.receiver.bias;
    };
  }
}

const functionContextClosure = makeFunctionContext(10, {named: 'named'}, 1, 2);
const blockContextClosure = makeBlockContext(20);
const catchContextClosure = makeCatchContext();
const evalContextClosure = makeEvalContext(40);
const withContextClosure = makeWithContext();
const generatorObject = makeSuspendedGenerator(50);
const generatorYield = generatorObject.next().value;
let awaitResolver;
const awaitBlocker = new Promise((resolve) => {
  awaitResolver = resolve;
});
const pendingAwaitPromise = makeAwaitContext(awaitBlocker, 60);
const asyncGeneratorObject = makeQueuedAsyncGenerator(70);
const asyncGeneratorQueuedNext = asyncGeneratorObject.next();
const carrier = new ScopeCarrier();
carrier.bias = 3;
const methodContextClosure = carrier.makeMethodClosure(80);
const staticMethodContextClosure = ScopeCarrier.staticContextFactory(90);

assertEquals('script-let:script-const:script-var', scriptContextClosure());
assertEquals(13, functionContextClosure(1));
assertEquals(32, blockContextClosure(1));
assertEquals('heap-dump catch context:17', catchContextClosure());
assertEquals('eval-context:42', evalContextClosure(2));
assertEquals(80, withContextClosure(7));
assertEquals(55, generatorYield(5));
assertEquals(88, methodContextClosure(5));
assertEquals(97, staticMethodContextClosure(7));

globalThis.heapDumpContextsScopesSample = {
  scriptLetObject,
  scriptConstArray,
  scriptVarObject,
  scriptContextClosure,
  functionContextClosure,
  blockContextClosure,
  catchContextClosure,
  evalContextClosure,
  withContextClosure,
  generatorObject,
  generatorYield,
  awaitBlocker,
  awaitResolver,
  pendingAwaitPromise,
  asyncGeneratorObject,
  asyncGeneratorQueuedNext,
  carrier,
  methodContextClosure,
  staticMethodContextClosure,
};
