#ifndef EVHR_H_
#define EVHR_H_

#include <stdio.h>

#define EVHR_EPOLL_MAX_EVENTS           32
#define EVHR_EPOLL_WAIT_TIMEOUT_MS      10000

#define EVHR_LOG_ENABLE
#undef  EVHR_LOG_ENABLE

#ifdef EVHR_LOG_ENABLE
#define EVHR_LOG_ERR(FMT, args...) printf("[EVHR][ERR]: " FMT "\n", ## args)
#define EVHR_LOG_DBG(FMT, args...) printf("[EVHR][DBG]: " FMT "\n", ## args)
#define EVHR_LOG_MSG(FMT, args...) printf("[EVHR][MSG]: " FMT "\n", ## args)
#else
#define EVHR_LOG_ERR(FMT, args...)
#define EVHR_LOG_DBG(FMT, args...)
#define EVHR_LOG_MSG(FMT, args...)
#endif

typedef enum evhr_rtn_e {
	EVHR_RTN_SUCCESS 			        = 0,
	EVHR_RTN_FAIL 				        = -1,
	EVHR_RTN_FAIL_NO_MEMORY		        = -10,
	EVHR_RTN_FAIL_IS_NULL			    = -20,
} EVHR_RTN;

typedef enum evhr_event_type_e {
    EVHR_EVENT_TYPE_SOCKET,
    EVHR_EVENT_TYPE_TIMER_ONCE,
    EVHR_EVENT_TYPE_TIMER_PERIODIC,
} EVHR_EVENT_TYPE;

typedef void (*EVHR_EVENT_CALLBACK) (int, void *);

typedef struct evhr_ctx_s {
    int                     running;
    int                     epfd;   // epoll file descriptor
    int                     events_count;
    struct epoll_event *    events;
    void *                  pdata;
} EVHR_CTX;

typedef struct evhr_event_record_s {
    int                     fd;
    EVHR_EVENT_TYPE        type;
    void *                  pdata;
    EVHR_EVENT_CALLBACK    in_cb;
    EVHR_EVENT_CALLBACK    err_cb;
} EVHR_EVENT_RECORD;

EVHR_RTN evhr_create(EVHR_CTX ** evhr);
EVHR_RTN evhr_release(EVHR_CTX * evhr);
EVHR_RTN evhr_dispatch(EVHR_CTX * evhr);
EVHR_RTN evhr_stop(EVHR_CTX * evhr);

EVHR_RTN evhr_event_del(EVHR_CTX * evhr, int fd);
EVHR_RTN evhr_event_add(EVHR_CTX * evhr, int fd, int type, void *pdata, 
        EVHR_EVENT_CALLBACK in_cb, EVHR_EVENT_CALLBACK err_cb);

EVHR_RTN evhr_event_add_socket(EVHR_CTX * evhr, int fd, void *pdata, 
        EVHR_EVENT_CALLBACK in_cb, EVHR_EVENT_CALLBACK err_cb);

EVHR_RTN evhr_event_add_timer_periodic(EVHR_CTX * evhr, int timerfd, 
        int sec, int nsec, void *pData, EVHR_EVENT_CALLBACK in_cb);
EVHR_RTN evhr_event_add_timer_once(EVHR_CTX * evhr, int timerfd, 
        int sec, int nsec, void *pData, EVHR_EVENT_CALLBACK in_cb);
EVHR_RTN evhr_event_stop_timer(int timerfd);

// Refer : http://neokentblog.blogspot.tw/2012/11/linux-fd-handler-timer.html

#endif 
