# 跨异步任务的数据时序约束

本文记录数据**生产**（系统服务 / 文件 / 硬件 / 网络读取或解码）与数据**消费**（编码、渲染、传输、解析）跨异步任务边界时必须遵守的同步规则。

## 核心原则

跨异步任务边界传递数据时，**禁止依赖任务调度时序作为隐式同步**。必须满足下列之一：

1. **串行化**：生产者在消费者启动前于同一同步调用栈完成全部写入
2. **所有权转移**：通过 `std::move` / `unique_ptr` / `SkBitmap` 等独占所有权类型，把已就绪数据整体移交给消费者
3. **显式同步**：使用 `base::WaitableEvent` / `Future` / `Promise` / mojo 回调等同步原语

## 禁止模式

- 生产者任务和消费者任务并发执行、共享同一缓冲区，仅靠 `shared_ptr` 引用而无同步
- 生产者尚未写入完成（如 `OH_PixelmapNative_ReadPixels` 未返回、网络流未 EOF、文件未读完），消费者就开始读取
- 用某个 flag 被设置作为隐式同步信号，但实际数据写入还在路上

## 判断方法

修改跨线程/跨异步任务的数据传递代码时，逐项检查：

1. **数据是否已就绪？** 消费者拿到数据时，生产者是否已经同步完成全部写入？如果不是，需要串行化或显式同步。
2. **所有权是否清晰？** 消费者拿到数据后，生产者是否还会访问同一块内存？如果是，需要所有权转移（`std::move`）或拷贝。
3. **flag 是否等于同步？** 如果代码依赖一个 bool flag 判断"数据准备好了"，检查 flag 设置和数据写入是否在同一同步上下文。如果不是，flag 是假的同步。

## 典型场景

- **剪贴板图片粘贴**：`ClipboardOHOSInternal::ReadPng` —— `PasteBoardClientAdapter::GetPasteData` + `PasteDataRecordAdapter::GetImgData`（含 `OH_PixelmapNative_ReadPixels`）必须在 UI 线程同步完成后，再通过 `std::move(img)` 把完整 `SkBitmap` 投递给线程池做 `EncodeBitmapToPng`
- **文件下载分片写入**：分片读取 + 写盘必须在同一任务或通过显式同步衔接
- **媒体采集 + 编码**：硬件 buffer 读取 + 编码投递
- **拖拽数据读取 + UI 渲染**：drag data 读取 + 渲染消费
- **流式网络响应解析**：socket 读 + HTTP/JSON 解析

## 相关文档

- [input-event-pipeline.md](input-event-pipeline.md)
- [chromium-ext-architecture.md](chromium-ext-architecture.md)
