# 上游来源

- 基础仓库：`xv6-labs-2025`，`lock` 分支，提交 `af12b48372ebf7a5d9f441524ebbbc4e255dfe72`
- Buffer cache 任务与评分项：`xv6-labs-2024`，`lock` 分支，提交 `fc27289d78a415ed4e4264a80b88c8496db70eef`

整合内容：

- 保留 2025 的内存分配器起始代码、读写锁接口、`kalloctest` 和 `rwlktest`。
- 启用仓库中已有的 `bcachetest`，恢复 2024 对 `bcache*` 锁的竞争统计，并合并评分项。
- 新增 `rwexample` 课堂示例及说明文件。

上游实验说明：

- https://pdos.csail.mit.edu/6.1810/2024/labs/lock.html
- https://pdos.csail.mit.edu/6.1810/2025/labs/lock.html
