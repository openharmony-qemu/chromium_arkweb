// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --mega-dom-ic --allow-natives-syntax

// Keep MegaDOM IC/API accessor handler shapes alive for heap dump smoke.
// This follows the existing ic-megadom mjsunit patterns and keeps the
// optimized load sites, API receivers, plain JS fallbacks, and signature
// mismatch error objects reachable until the dump is written.

(function() {
  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function loadNodeType(object) {
    return object.nodeType;
  }
  %PrepareFunctionForOptimization(loadNodeType);

  const divs = keep('divs', []);
  for (let i = 0; i < 6; ++i) {
    const div = new d8.dom.Div();
    div['own_' + i] = {i, kind: 'own-data'};
    divs.push(div);
  }

  function loadAllDivNodeTypes() {
    let result = 0;
    for (let i = 0; i < divs.length; ++i) {
      result += loadNodeType(divs[i]);
    }
    return result;
  }
  %PrepareFunctionForOptimization(loadAllDivNodeTypes);

  const results = keep('results', []);
  results.push(loadAllDivNodeTypes());
  assertEquals(6, results[0]);

  %OptimizeFunctionOnNextCall(loadAllDivNodeTypes);
  results.push(loadAllDivNodeTypes());
  assertEquals(6, results[1]);

  const mismatchErrors = keep('mismatchErrors', []);
  try {
    loadNodeType(new d8.dom.EventTarget());
  } catch (error) {
    mismatchErrors.push(error);
    assertInstanceof(error, TypeError);
    assertEquals('Illegal invocation', error.message);
  }
  assertEquals(1, mismatchErrors.length);

  const plainObjects = keep('plainObjects', [
    {},
    {nodeType: 'plain-node-type'},
    {nodeType: {kind: 'plain-object-node-type'}},
  ]);
  results.push(loadNodeType(plainObjects[0]));
  results.push(loadNodeType(plainObjects[1]));
  results.push(loadNodeType(plainObjects[2]));
  assertEquals(undefined, results[2]);
  assertEquals('plain-node-type', results[3]);
  assertEquals('plain-object-node-type', results[4].kind);

  keep('catalog', catalog);
  keep('loadNodeType', loadNodeType);
  keep('loadAllDivNodeTypes', loadAllDivNodeTypes);
})();
