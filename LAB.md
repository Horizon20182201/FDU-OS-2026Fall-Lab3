# OSLab3：锁竞争与读写锁

这是一份合并后的学生起始仓库。它以 MIT xv6-labs-2025 的 `lock` 分支为底座，加入了 MIT xv6-labs-2024 的 Buffer cache 任务和测试。仓库中的三个作业均保留为未完成状态。

## 先运行课堂例题

```sh
make qemu
rwexample
```

`kernel/rwexample.c` 是完整的课堂例题。三个读者和一个写者并发访问含 `lower`、`upper` 两个字段的共享配置。普通自旋锁覆盖整次读取或整次更新，因此读者只能看到 `(0, 10)` 或 `(20, 30)`，不会看到混合值。

这个例题保证一致性，但普通自旋锁也会让读者彼此等待。第 3 题要求实现读写锁，在保持一致性的同时允许读者并行。

## 3.1 Memory allocator

修改 `kernel/kalloc.c`：

- 将单个 `kmem.freelist` 改为每 CPU 一条空闲链表，并为每条链表设置锁。
- `kfree()` 将页放入当前 CPU 的链表。
- `kalloc()` 先从本地链表取页；本地为空时，从其他 CPU 窃取空闲页。
- 锁名必须以 `kmem` 开头。读取 `cpuid()` 时正确使用 `push_off()` / `pop_off()`。

测试：`kalloctest`、`usertests sbrkmuch`、`usertests -q`。

## 3.2 Buffer cache

修改 `kernel/bio.c`：

- 用哈希桶或等价的数据结构减少 `bcache.lock` 竞争。
- 同一 `(dev, blockno)` 最多对应一个缓存块。
- 保持 `NBUF` 不变，只能复用 `refcnt == 0` 的块；不要求保持 LRU。
- 相关锁名必须以 `bcache` 开头。

测试：`bcachetest`。其中 test0 要求本次运行增加的失败 test-and-set 总数小于 500。随后运行 `usertests -q`。

## 3.3 Read-write lock

修改 `kernel/spinlock.h` 和 `kernel/spinlock.c`：

- 重新设计 `struct rwspinlock`。
- 实现 `initrwlock()`、`read_acquire()`、`read_release()`、`write_acquire()`、`write_release()`。
- 允许多个读者同时持锁；写者必须独占。
- 写者开始等待后，新读者必须等待，防止写者饥饿。
- 使用原子操作维护共享状态。不要用普通自旋锁把所有读者串行化。

测试：`rwlktest`、`usertests -q`。

## 综合测试

```sh
make grade
```

评分脚本依次运行课堂例题、三道题的专项测试和 `usertests -q`。课堂例题计 0 分，仅用于确认仓库和环境可用；三道作业与回归测试共 100 分。

在仓库根目录创建 `time.txt`，写入完成实验所用的整数小时数。

## 教师说明

这份仓库只包含公开起始代码和公开测试，不包含三道作业的参考解答。课堂例题是完整代码，可直接讲解。
