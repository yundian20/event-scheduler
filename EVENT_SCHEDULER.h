/**
 * @file event_scheduler.h
 * @brief 事件调度器头文件
 *
 * 功能概述：
 * - 基于优先级的有序事件队列
 * - 支持一次性事件和周期事件
 * - 事件对象池管理，避免动态内存分配
 * - 支持中断保护（可选）
 */

#ifndef EVENT_SCHEDULER_H
#define EVENT_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*============================================================
 *  用户可覆盖的配置项（在 #include 本头文件之前定义）
 *============================================================*/

#ifndef EVENT_POOL_SIZE
#define EVENT_POOL_SIZE       32      /**< 事件对象池大小 */
#endif

#ifndef EVENT_DATA_MAX_LEN
#define EVENT_DATA_MAX_LEN    8       /**< 事件携带数据的最大长度 */
#endif

#ifndef MAX_BINDINGS
#define MAX_BINDINGS          16      /**< 最大事件处理函数绑定数 */
#endif

/*============================================================
 *  类型定义
 *============================================================*/

/**
 * @brief 事件优先级枚举
 */
typedef enum {
    EVT_PRIO_HIGH   = 0,  /**< 高优先级 */
    EVT_PRIO_NORMAL = 1,  /**< 普通优先级 */
    EVT_PRIO_LOW    = 2   /**< 低优先级 */
} EventPriority;

/**
 * @brief 事件结构体
 */
typedef struct Event {
    uint8_t         type;       /**< 事件类型 */
    uint8_t         priority;   /**< 事件优先级 */
    uint8_t         data_len;   /**< 携带数据长度 */
    uint8_t         data[EVENT_DATA_MAX_LEN];  /**< 携带数据 */
    struct Event   *next;       /**< 链表指针 */

    uint16_t        period;     /**< 周期tick数，0=一次性事件 */
    uint32_t        next_fire;  /**< 下次触发的绝对tick */
} Event;

/**
 * @brief 事件处理函数类型
 * @param evt 事件指针
 */
typedef void (*EventHandler)(const Event *evt);

/**
 * @brief 中断锁定函数类型（可选）
 */
typedef void (*irq_lock_fn)(void);

/**
 * @brief 事件句柄类型（用于取消事件）
 */
typedef Event* event_handle_t;

/**
 * @brief 事件调度器结构体
 */
typedef struct {
    Event           *head;              /**< 事件队列头指针 */
    Event           pool[EVENT_POOL_SIZE];  /**< 事件对象池 */
    Event           *free_list;         /**< 空闲事件链表 */
    uint32_t        total_alloc;       /**< 总分配次数（调试用） */
    uint32_t        total_drop;        /**< 分配失败次数（调试用） */

    struct {                            /**< 事件处理函数绑定表 */
        uint8_t     type;
        EventHandler handler;
    } bindings[MAX_BINDINGS];
    uint8_t         binding_count;      /**< 当前绑定数量 */

    irq_lock_fn     irq_disable;        /**< 中断禁用函数（可选） */
    irq_lock_fn     irq_enable;        /**< 中断启用函数（可选） */

    uint32_t        tick;               /**< 系统tick计数器 */
} EventScheduler;

/*============================================================
 *  公开 API 函数声明
 *============================================================*/

/**
 * @brief 初始化事件调度器
 * @param sched 调度器指针
 */
void sched_init(EventScheduler *sched);

/**
 * @brief 投递一个事件到队列
 * @param sched 调度器指针
 * @param type 事件类型
 * @param prio 事件优先级
 * @param data 事件数据（可为NULL）
 * @param data_len 数据长度
 * @return 事件句柄（成功），NULL（失败）
 */
event_handle_t sched_post(EventScheduler *sched,
                          uint8_t type,
                          uint8_t prio,
                          const void *data,
                          uint8_t data_len);

/**
 * @brief 投递一个周期事件到队列
 * @param sched 调度器指针
 * @param type 事件类型
 * @param prio 事件优先级
 * @param data 事件数据（可为NULL）
 * @param data_len 数据长度
 * @param period_ticks 周期tick数
 * @return 事件句柄（成功），NULL（失败）
 */
event_handle_t sched_post_periodic(EventScheduler *sched,
                                   uint8_t type,
                                   uint8_t prio,
                                   const void *data,
                                   uint8_t data_len,
                                   uint16_t period_ticks);

/**
 * @brief 取消一个事件
 * @param sched 调度器指针
 * @param handle 事件句柄
 * @return true（成功），false（失败）
 */
bool sched_cancel(EventScheduler *sched, event_handle_t handle);

/**
 * @brief 注册一个事件处理函数
 * @param sched 调度器指针
 * @param type 事件类型
 * @param handler 事件处理函数
 * @return true（成功），false（失败）
 */
bool sched_register(EventScheduler *sched,
                    uint8_t type,
                    EventHandler handler);

/**
 * @brief 注销一个事件处理函数
 * @param sched 调度器指针
 * @param type 事件类型
 * @param handler 事件处理函数
 * @return true（成功），false（失败）
 */
bool sched_unregister(EventScheduler *sched,
                      uint8_t type,
                      EventHandler handler);

/**
 * @brief 注销所有指定类型的事件处理函数
 * @param sched 调度器指针
 * @param type 事件类型
 * @return 注销的处理函数数量
 */
uint32_t sched_unregister_type(EventScheduler *sched, uint8_t type);

/**
 * @brief 处理队列中所有待处理事件
 * @param sched 调度器指针
 */
void sched_process_all(EventScheduler *sched);

/**
 * @brief 查询待处理事件数量
 * @param sched 调度器指针
 * @return 待处理事件数量
 */
uint32_t sched_pending(const EventScheduler *sched);

/**
 * @brief 设置中断保护函数（可选）
 * @param sched 调度器指针
 * @param disable 中断禁用函数
 * @param enable 中断启用函数
 */
void sched_set_irq(EventScheduler *sched,
                   irq_lock_fn disable,
                   irq_lock_fn enable);

/**
 * @brief 增加系统tick计数
 * @param sched 调度器指针
 */
 //把这个放在定时器里面运行，单位为ms
void sched_tick(EventScheduler *sched);

/**
 * @brief 获取当前tick计数
 * @param sched 调度器指针
 * @return 当前tick值
 */
uint32_t Get_sched_tick(const EventScheduler *sched);

/*============================================================
 *  便捷宏
 *============================================================*/

/**
 * @brief 投递无数据事件（便捷宏）
 */
#define sched_post_event(sched, type, prio) \
    sched_post((sched), (type), (prio), NULL, 0)

#endif /* EVENT_SCHEDULER_H */

#ifdef __cplusplus
}
#endif
