// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

globalThis.__heapDumpModuleThrowingHelperStarted = {
  kind: 'module-throwing-helper-started',
};

export const beforeThrow = {
  kind: 'module-throwing-before-throw',
};

throw new Error('heap-dump module-throwing-helper evaluation error');
