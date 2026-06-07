/*--- 用户可以覆盖配置 ---*/
#define EVENT_POOL_SIZE 64       // 我要更大的池
#define MAX_BINDINGS    32       // 我有更多事件类型

#include "event_scheduler.h"

/*--- 用户定义自己的事件类型 ---*/
typedef enum {
    TASK1,
    TASK2,
    EVT_SEND_DATA,
    EVT_NONE
} MyEventType;

/*--- 用户写处理函数 ---*/
void TASK1_RUN(const Event *evt) {
    Serial.println("TASK1 running");
}

void TASK2_RUN(const Event *evt) {
    Serial.println("TASK2 running");
}

EventScheduler sched;


void setup() {
    Serial.begin(115200);
    sched_init(&sched);

    sched_register(&sched, TASK1, TASK1_RUN);
    sched_register(&sched, TASK2,  TASK2_RUN);
}

void loop() {
    sched_post(&sched, TASK1, EVT_PRIO_NORMAL, NULL, 0);
    sched_post(&sched, TASK2,  EVT_PRIO_NORMAL, NULL, 0);

    sched_process_all(&sched);
    delay(500);
}
