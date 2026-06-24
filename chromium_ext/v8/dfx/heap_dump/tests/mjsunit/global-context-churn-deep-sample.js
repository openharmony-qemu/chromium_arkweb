// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep global/script-context and property-cell churn alive for heap dump smoke.
// This follows V8 global/eval/with/catch patterns to exercise Context,
// ContextCell, ScriptContextTable, PropertyCell, ScopeInfo, and dictionary
// property shapes from script-level JavaScript.

const heapDumpGlobalContextCatalog = {};
function heapDumpGlobalContextKeep(name, value) {
  heapDumpGlobalContextCatalog[name] = value;
  return value;
}

var heapDumpContextVar = {kind: 'var-binding', count: 1};
let heapDumpContextLet = {kind: 'let-binding', count: 2};
const heapDumpContextConst = {kind: 'const-binding', count: 3};

function heapDumpContextReadTopLevel() {
  return [
    heapDumpContextVar.kind,
    heapDumpContextLet.kind,
    heapDumpContextConst.kind,
  ].join(':');
}

heapDumpGlobalContextKeep('topLevelBindings', {
  varBinding: heapDumpContextVar,
  letBinding: heapDumpContextLet,
  constBinding: heapDumpContextConst,
  reader: heapDumpContextReadTopLevel,
});

globalThis.heapDumpContextCellChurn = {phase: 'initial-object'};
globalThis.heapDumpContextCellChurn = 11;
globalThis.heapDumpContextCellChurn = 11.5;
globalThis.heapDumpContextCellChurn = 'string-phase';
globalThis.heapDumpContextCellChurn = {phase: 'object-again'};

Object.defineProperty(globalThis, 'heapDumpContextAccessorCell', {
  configurable: true,
  enumerable: true,
  get() {
    return heapDumpContextLet;
  },
  set(value) {
    heapDumpContextLet = value;
  },
});
globalThis.heapDumpContextAccessorCell = {kind: 'accessor-updated', count: 4};

Object.defineProperty(globalThis, 'heapDumpContextReadonlyCell', {
  configurable: true,
  enumerable: true,
  writable: false,
  value: {kind: 'readonly-cell'},
});

globalThis.heapDumpContextDeleteCell = {phase: 'before-delete'};
delete globalThis.heapDumpContextDeleteCell;
globalThis.heapDumpContextDeleteCell = {phase: 'after-delete'};

Object.defineProperty(globalThis, 'heapDumpContextNonConfigurableCell', {
  configurable: false,
  enumerable: true,
  writable: true,
  value: {phase: 'non-configurable-initial'},
});
globalThis.heapDumpContextNonConfigurableCell = {
  phase: 'non-configurable-updated',
};

const heapDumpContextDynamicNames =
    heapDumpGlobalContextKeep('dynamicNames', []);
for (let i = 0; i < 24; ++i) {
  const name = 'heapDumpContextDynamicGlobal_' + i;
  globalThis[name] = {index: i, parity: i % 2};
  if (i % 3 === 0) delete globalThis[name];
  globalThis[name] = {index: i, phase: 'rebuilt'};
  heapDumpContextDynamicNames.push(name);
}

const heapDumpContextDirectEvalState = heapDumpGlobalContextKeep(
    'directEvalState',
    eval(`
      var heapDumpContextDirectEvalVar = {kind: 'direct-eval-var'};
      let heapDumpContextDirectEvalLet = {kind: 'direct-eval-let'};
      const heapDumpContextDirectEvalConst = {kind: 'direct-eval-const'};
      function heapDumpContextDirectEvalFunction() {
        return heapDumpContextDirectEvalVar.kind + ':' +
            heapDumpContextDirectEvalLet.kind + ':' +
            heapDumpContextDirectEvalConst.kind;
      }
      ({
        varBinding: heapDumpContextDirectEvalVar,
        read: heapDumpContextDirectEvalFunction,
        lazyClosure: function heapDumpContextDirectEvalLazy() {
          return heapDumpContextDirectEvalLet.kind;
        },
      })
      //# sourceURL=heap-dump-global-context-direct-eval.js
    `));

const heapDumpContextIndirectEval = (0, eval);
const heapDumpContextIndirectEvalState = heapDumpGlobalContextKeep(
    'indirectEvalState',
    heapDumpContextIndirectEval(`
      var heapDumpContextIndirectEvalVar = {kind: 'indirect-eval-var'};
      this.heapDumpContextIndirectGlobal = {kind: 'indirect-global-object'};
      ({
        read: function heapDumpContextIndirectRead() {
          return heapDumpContextIndirectEvalVar.kind + ':' +
              globalThis.heapDumpContextIndirectGlobal.kind;
        },
        lazyClosure: function heapDumpContextIndirectLazy() {
          return heapDumpContextIndirectEvalVar.kind;
        },
      })
      //# sourceURL=heap-dump-global-context-indirect-eval.js
    `));

function heapDumpMakeWithClosure(scope, fallback) {
  with (scope) {
    return function heapDumpWithClosure(extra) {
      return value + ':' + fallback.kind + ':' + extra;
    };
  }
}

const heapDumpWithScopes = heapDumpGlobalContextKeep('withScopes', []);
for (let i = 0; i < 8; ++i) {
  const scope = {value: 'with-value-' + i};
  const fallback = {kind: 'fallback-' + i};
  heapDumpWithScopes.push({
    scope,
    fallback,
    closure: heapDumpMakeWithClosure(scope, fallback),
  });
}

function heapDumpMakeCatchClosure(tag) {
  try {
    throw {tag, payload: {kind: 'catch-payload-' + tag}};
  } catch (caught) {
    let local = {kind: 'catch-local-' + tag};
    return function heapDumpCatchClosure(extra) {
      return caught.payload.kind + ':' + local.kind + ':' + extra;
    };
  }
}

const heapDumpCatchClosures = heapDumpGlobalContextKeep('catchClosures', []);
for (let i = 0; i < 8; ++i) {
  heapDumpCatchClosures.push(heapDumpMakeCatchClosure(i));
}

{
  let blockLet = {kind: 'block-let'};
  const blockConst = {kind: 'block-const'};
  function blockFunction() {
    return blockLet.kind + ':' + blockConst.kind;
  }
  heapDumpGlobalContextKeep('blockContext', {
    blockLet,
    blockConst,
    blockFunction,
  });
}

heapDumpGlobalContextKeep('propertyCells', {
  churn: globalThis.heapDumpContextCellChurn,
  accessor: globalThis.heapDumpContextAccessorCell,
  readonly: globalThis.heapDumpContextReadonlyCell,
  deleted: globalThis.heapDumpContextDeleteCell,
  nonConfigurable: globalThis.heapDumpContextNonConfigurableCell,
  indirectGlobal: globalThis.heapDumpContextIndirectGlobal,
});

assertEquals('var-binding:accessor-updated:const-binding',
             heapDumpContextReadTopLevel());
assertEquals('direct-eval-var:direct-eval-let:direct-eval-const',
             heapDumpContextDirectEvalState.read());
assertEquals('indirect-eval-var:indirect-global-object',
             heapDumpContextIndirectEvalState.read());
assertEquals('with-value-3:fallback-3:ok',
             heapDumpWithScopes[3].closure('ok'));
assertEquals('catch-payload-4:catch-local-4:ok',
             heapDumpCatchClosures[4]('ok'));
assertEquals('block-let:block-const',
             heapDumpGlobalContextCatalog.blockContext.blockFunction());
assertEquals('object-again', globalThis.heapDumpContextCellChurn.phase);
assertEquals('after-delete', globalThis.heapDumpContextDeleteCell.phase);
assertEquals('rebuilt', globalThis.heapDumpContextDynamicGlobal_21.phase);

globalThis.heapDumpGlobalContextChurnDeepSample =
    heapDumpGlobalContextCatalog;
