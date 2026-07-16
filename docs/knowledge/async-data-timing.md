# 跨异步任务的数据时序约束

本文记录数据**生产**（系统服务 / 文件 / 硬件 / 网络读取或解码）与数据**消费**（编码、渲染、传输、解析）跨异步任务边界时必须遵守的同步规则。

## 通用原则

数据生产与消费跨异步任务边界时，必须满足下列之一，禁止纯靠任务调度时序作为隐式同步：

1. **串行化**：生产者在消费者启动前于同一同步调用栈完成全部写入
2. **所有权转移**：通过 `std::move` / `unique_ptr` / `SkBitmap` 等独占所有权类型，把已就绪数据整体移交给消费者
3. **显式同步**：使用 `base::WaitableEvent` / `Future` / `Promise` / mojo 回调等同步原语

## 禁止模式

- 生产者任务和消费者任务并发执行、共享同一缓冲区，仅靠 `shared_ptr` 引用而无同步
- 生产者尚未写入完成（如 `OH_PixelmapNative_ReadPixels` 未返回、网络流未 EOF、文件未读完），消费者就开始读取
- 用某个 flag 被设置作为隐式同步信号，但实际数据写入还在路上

## 典型场景

- **剪贴板图片粘贴**：`chromium_ext/ui/base/clipboard/ohos/clipboard_ohos.cc` 中 `ClipboardOHOSInternal::ReadPng` —— `PasteBoardClientAdapter::GetPasteData` + `PasteDataRecordAdapter::GetImgData`（含 `OH_PixelmapNative_ReadPixels`）必须在 UI 线程同步完成后，再通过 `std::move(img)` 把完整 `SkBitmap` 投递给线程池做 `EncodeBitmapToPng`
- **文件下载分片写入**：分片读取 + 写盘必须在同一任务或通过显式同步衔接
- **媒体采集 + 编码**：硬件 buffer 读取 + 编码投递
- **拖拽数据读取 + UI 渲染**：drag data 读取 + 渲染消费
- **流式网络响应解析**：socket 读 + HTTP/JSON 解析

## 历史教训

剪贴板图片粘贴流程中，曾把"数据获取"（`GetPasteData`）与"PNG 编码"（`EncodeBitmapToPng`）拆到两个并发异步任务，编码任务读到部分填充的像素缓冲区，导致 web 端粘贴图片显示错乱。当前实现已修复为：数据获取与像素读取同步在 UI 线程完成，仅编码任务异步且通过 `std::move(img)` 转移所有权。

## 相关文档

- [input-event-pipeline.md](input-event-pipeline.md)
- [chromium-ext-architecture.md](chromium-ext-architecture.md)
