// Heap-dump shape sample for SourceTextModule top-level-await states.

globalThis.__heapDumpTlaNeverResolve = new Promise(() => {});

const pendingImportPromise =
    import("./helpers/tla-pending-helper.mjs").catch((error) => error);
const resolvedNamespace = await import("./helpers/tla-resolved-helper.mjs");

await Promise.resolve();

assertEquals("tla-resolved-object", resolvedNamespace.resolvedObject.kind);
assertEquals("tla-awaited-value", resolvedNamespace.awaited.kind);
assertEquals("tla-awaited-value", resolvedNamespace.readAwaitedKind());
assertTrue(pendingImportPromise instanceof Promise);

globalThis.__heapDumpModuleTlaDynamicKeepalive = {
  pendingImportPromise,
  resolvedNamespace,
  blocker: globalThis.__heapDumpTlaNeverResolve,
  pendingHelperStarted: globalThis.__heapDumpTlaPendingHelperStarted,
  resolvedHelperKeepalive: globalThis.__heapDumpTlaResolvedHelperKeepalive,
};
