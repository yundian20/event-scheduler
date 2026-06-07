/**
 * @file event_scheduler.c
 * @brief 事件调度器实现
 */

#include "event_scheduler.h"
#include <string.h>

/*============================================================
 *  内部函数（静态，外部不可见）
 *============================================================*/

/**
 * @brief 从空闲池分配一个事件对象
 * @param sched 调度器指针
 * @return 事件指针（可能为NULL）
 */
static Event *alloc_event(EventScheduler *sched)
{
    Event *evt = sched->free_list;
    if (evt) {
        sched->free_list = evt->next;
        evt->next = NULL;
        sched->total_alloc++;
    } else {
        sched->total_drop++;  /* 池已满，分配失败 */
    }
    return evt;
}

/**
 * @brief 释放事件对象回空闲池
 * @param sched 调度器指针
 * @param evt 事件指针
 */
static void free_event(EventScheduler *sched, Event *evt)
{
    evt->next       = sched->free_list;
    evt->type       = 0;
    evt->data_len   = 0;
    evt->period     = 0;
    evt->next_fire  = 0;
    sched->free_list = evt;
}

/**
 * @brief 按优先级插入事件到有序队列
 * @param sched 调度器指针
 * @param evt 事件指针
 */
static void insert_sorted(EventScheduler *sched, Event *evt)
{
    Event **pp = &sched->head;
    while (*pp && (*pp)->priority <= evt->priority) {
        pp = &(*pp)->next;
    }
    evt->next = *pp;
    *pp = evt;
}

/**
 * @brief 从队列头部取出事件
 * @param sched 调度器指针
 * @return 事件指针（队列为空时返回NULL）
 */
static Event *sched_fetch(EventScheduler *sched)
{
    Event *evt = sched->head;
    if (!evt) return NULL;
    sched->head = evt->next;
    evt->next = NULL;
    return evt;
}

/*============================================================
 *  临界区保护宏
 *============================================================*/

#define IRQ_ENTER(s)  do { if ((s)->irq_disable) (s)->irq_disable(); } while(0)
#define IRQ_EXIT(s)   do { if ((s)->irq_enable)  (s)->irq_enable();  } while(0)

/*============================================================
 *  公开 API 实现（与头文件声明顺序一致）
 *============================================================*/

/**
 * @brief 初始化事件调度器
 * @note 必须在使用前调用
 */
void sched_init(EventScheduler *sched)
{
    memset(sched, 0, sizeof(*sched));
    sched->head = NULL;
    sched->binding_count = 0;

    /* 初始化空闲事件池链表 */
    for (uint32_t i = 0; i < EVENT_POOL_SIZE; i++) {
        sched->pool[i].next = &sched->pool[i + 1];
    }
    sched->pool[EVENT_POOL_SIZE - 1].next = NULL;
    sched->free_list = &sched->pool[0];
}

/**
 * @brief 投递事件到队列
 */
event_handle_t sched_post(EventScheduler *sched,
                          uint8_t type,
                          uint8_t prio,
                          const void *data,
                          uint8_t data_len)
{
    /* 参数校验 */
    if (!sched || data_len > EVENT_DATA_MAX_LEN ||
       (data == NULL && data_len > 0)) {
        return NULL;
    }

    IRQ_ENTER(sched);
    Event *evt = alloc_event(sched);
    if (!evt) {
        IRQ_EXIT(sched);
        return NULL;
    }

    evt->type     = type;
    evt->priority = prio;
    evt->data_len = data_len;
    evt->period   = 0;        /* 一次性事件 */
    evt->next_fire = 0;

    if (data_len > 0) {
        memcpy(evt->data, data, data_len);
    }

    insert_sorted(sched, evt);
    IRQ_EXIT(sched);
    return evt;
}

/**
 * @brief 投递周期事件到队列
 */
event_handle_t sched_post_periodic(EventScheduler *sched,
                                   uint8_t type,
                                   uint8_t prio,
                                   const void *data,
                                   uint8_t data_len,
                                   uint16_t period_ticks)
{
    /* 参数校验 */
    if (!sched || data_len > EVENT_DATA_MAX_LEN ||
       (data == NULL && data_len > 0)) {
        return NULL;
    }

    IRQ_ENTER(sched);
    Event *evt = alloc_event(sched);
    if (!evt) {
        IRQ_EXIT(sched);
        return NULL;
    }

    evt->type      = type;
    evt->priority  = prio;
    evt->data_len  = data_len;
    evt->period    = period_ticks;
    evt->next_fire = sched->tick + period_ticks;

    if (data_len > 0) {
        memcpy(evt->data, data, data_len);
    }

    insert_sorted(sched, evt);
    IRQ_EXIT(sched);
    return evt;
}

/**
 * @brief 取消事件
 * @note 周期事件会被标记为不再触发，一次性事件会被立即释放
 */
bool sched_cancel(EventScheduler *sched, event_handle_t handle)
{
    if (!sched || !handle) {
        return false;
    }

    Event *evt = handle;
    if (evt->period == 0) {
        /* 一次性事件，直接释放 */
        free_event(sched, evt);
    } else {
        /* 周期事件，标记为不触发（period设为0） */
        evt->period = 0;
        evt->next_fire = 0;
    }
    return true;
}

/**
 * @brief 注册事件处理函数
 */
bool sched_register(EventScheduler *sched,
                    uint8_t type,
                    EventHandler handler)
{
    if (!sched || !handler || sched->binding_count >= MAX_BINDINGS) {
        return false;
    }

    sched->bindings[sched->binding_count].type    = type;
    sched->bindings[sched->binding_count].handler = handler;
    sched->binding_count++;
    return true;
}

/**
 * @brief 注销事件处理函数
 */
bool sched_unregister(EventScheduler *sched,
                      uint8_t type,
                      EventHandler handler)
{
    if (!sched || !handler) {
        return false;
    }

    for (uint8_t i = 0; i < sched->binding_count; i++) {
        if (sched->bindings[i].type == type &&
            sched->bindings[i].handler == handler) {
            sched->bindings[i].type = 0;
            sched->bindings[i].handler = NULL;
            sched->binding_count--;
            return true;
        }
    }
    return false;
}

/**
 * @brief 注销所有指定类型的事件处理函数
 */
uint32_t sched_unregister_type(EventScheduler *sched, uint8_t type)
{
    if (!sched) {
        return 0;
    }

    uint32_t count = 0;
    for (uint8_t i = 0; i < sched->binding_count; i++) {
        if (sched->bindings[i].type == type) {
            count++;
            sched->bindings[i].type = 0;
            sched->bindings[i].handler = NULL;
            sched->binding_count--;
        }
    }
    return count;
}

/**
 * @brief 处理所有待处理事件
 * @note 周期事件处理后会重新插入队列
 */
void sched_process_all(EventScheduler *sched)
{
    Event *evt;
    while ((evt = sched_fetch(sched)) != NULL) {
        bool handled = false;

        /* 查找并调用对应的处理函数 */
        for (uint8_t i = 0; i < sched->binding_count; i++) {
            if (sched->bindings[i].type == evt->type) {
                sched->bindings[i].handler(evt);
                handled = true;
                break;
            }
        }

        /* 未处理的事件可在此扩展默认处理 */

        /* 处理周期事件 */
        if (evt->period > 0) {
            /* 计算下次触发时间 */
            evt->next_fire = sched->tick + evt->period;

            /* 防止时间回绕 */
            if ((int32_t)(evt->next_fire - sched->tick) < 0) {
                evt->next_fire = sched->tick + evt->period;
            }

            /* 重新插入队列 */
            insert_sorted(sched, evt);
        } else {
            /* 一次性事件，释放回池 */
            IRQ_ENTER(sched);
            free_event(sched, evt);
            IRQ_EXIT(sched);
        }
    }
}

/**
 * @brief 查询待处理事件数量
 */
uint32_t sched_pending(const EventScheduler *sched)
{
    uint32_t count = 0;
    Event *p = sched->head;
    while (p) {
        count++;
        p = p->next;
    }
    return count;
}

/**
 * @brief 设置中断保护函数（可选）
 * @note 如果不需要中断保护，可以不调用此函数
 */
void sched_set_irq(EventScheduler *sched,
                   irq_lock_fn disable,
                   irq_lock_fn enable)
{
    if (!sched) return;
    sched->irq_disable = disable;
    sched->irq_enable  = enable;
}

/**
 * @brief 增加系统tick计数
 * @note 应在定时器中断或主循环中定期调用
 */
void sched_tick(EventScheduler *sched)
{
    if (sched) {
        sched->tick++;
    }
}

/**
 * @brief 获取当前tick计数
 */
uint32_t Get_sched_tick(const EventScheduler *sched)
{
    return sched ? sched->tick : 0;
}
