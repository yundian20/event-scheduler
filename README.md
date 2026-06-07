# Event Scheduler

基于优先级的轻量级事件调度器， 设计。

## 特性

- **优先级队列**：事件按优先级（高/普通/低）有序处理
- **对象池管理**：预分配事件池，避免动态内存分配
- **周期事件支持**：支持一次性事件和周期事件
- **中断安全**：支持中断保护函数（可选）
- **轻量实现**：无外部依赖，仅使用标准 C

## 文件结构

```
├── EVENT_SCHEDULER.h    # 头文件（类型定义、API 声明）
├── EVENT_SCHEDULER.c    # 实现文件
├── system.ino          # 示例使用代码
└── README.md
```

## 快速开始

### 1. 初始化

```c
#include "EVENT_SCHEDULER.h"

EventScheduler sched;
sched_init(&sched);

// 可选：设置中断保护
sched_set_irq(&sched, disable_irq, enable_irq);
```

### 2. 注册事件处理函数

```c
// 定义事件类型（用户自定义）
#define MY_EVENT_1   1
#define MY_EVENT_2   2

// 定义处理函数
void my_event_handler(const Event *evt) {
    Serial.print("Event type: ");
    Serial.println(evt->type);
    
    // 访问事件数据
    if (evt->data_len > 0) {
        // 处理 evt->data
    }
}

// 注册
sched_register(&sched, MY_EVENT_1, my_event_handler);
```

### 3. 投递事件

```c
// 投递无数据事件
sched_post(&sched, MY_EVENT_1, EVT_PRIO_NORMAL, NULL, 0);

// 投递带数据事件
uint8_t data[] = {0x01, 0x02};
sched_post(&sched, MY_EVENT_2, EVT_PRIO_HIGH, data, sizeof(data));

// 投递周期事件（每 100 tick 触发一次）
sched_post_periodic(&sched, MY_EVENT_1, EVT_PRIO_NORMAL, NULL, 0, 100);
```

### 4. 在主循环中处理

```c
void loop() {
    // 定期增加 tick
    sched_tick(&sched);
    
    // 处理所有待处理事件
    sched_process_all(&sched);
}
```

## API 参考

| 函数 | 说明 |
|------|------|
| `sched_init()` | 初始化调度器 |
| `sched_post()` | 投递一次性事件 |
| `sched_post_periodic()` | 投递周期事件 |
| `sched_cancel()` | 取消事件 |
| `sched_register()` | 注册事件处理函数 |
| `sched_unregister()` | 注销处理函数 |
| `sched_process_all()` | 处理所有待处理事件 |
| `sched_pending()` | 查询待处理事件数量 |
| `sched_tick()` | 增加系统 tick |
| `Get_sched_tick()` | 获取当前 tick |

## 配置选项

在 `#include` 之前定义可覆盖默认配置：

```c
#define EVENT_POOL_SIZE     32   // 事件池大小（默认 32）
#define EVENT_DATA_MAX_LEN  8    // 最大数据长度（默认 8）
#define MAX_BINDINGS        16   // 最大绑定数量（默认 16）
```

## 事件优先级

```c
typedef enum {
    EVT_PRIO_HIGH   = 0,   // 高优先级
    EVT_PRIO_NORMAL = 1,   // 普通优先级
    EVT_PRIO_LOW    = 2    // 低优先级
} EventPriority;
```

## 许可证

MIT License
