#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/epoll.h>      // For epoll
#include <sys/timerfd.h>    // For timerfd

#include "evhr.h"

EVHR_RTN evhr_create(EVHR_CTX ** handler)
{
    EVHR_RTN ret;
    EVHR_CTX * obj = NULL;

    obj = malloc(sizeof(EVHR_CTX));
    if (obj == NULL)
    {
        EVHR_LOG_ERR("create handler failed!");
        ret = EVHR_RTN_FAIL_NO_MEMORY;
        goto fail_and_exit;
    }
    
    memset(obj, 0, sizeof(EVHR_CTX));
    obj->running = 0;
    obj->events_count = EVHR_EPOLL_MAX_EVENTS;

    // create epoll
    if ((obj->epfd = epoll_create1(EPOLL_CLOEXEC)) == -1)
    {
        EVHR_LOG_ERR("create epoll failed! ret[%d].", obj->epfd);
        ret = EVHR_RTN_FAIL;
        goto fail_and_exit;
    }

    // create events
    obj->events = malloc( EVHR_EPOLL_MAX_EVENTS * sizeof(struct epoll_event));
    if (obj->events == NULL)
    {
        EVHR_LOG_ERR("create epoll events failed!");
        ret = EVHR_RTN_FAIL_NO_MEMORY;
        goto fail_and_exit;
    }

    // Success Exit
    *handler = obj;
    return EVHR_RTN_SUCCESS;

fail_and_exit:

    if (obj)
        evhr_release(obj);

    return ret;
}

EVHR_RTN evhr_release(EVHR_CTX * this)
{

    if (this == NULL)
        return EVHR_RTN_FAIL_IS_NULL;

    if (this->epfd > 0)
        close(this->epfd);

    if (this->events)
        free(this->events);

    free(this);

    return EVHR_RTN_SUCCESS;
}

EVHR_RTN evhr_dispatch(EVHR_CTX * this)
{
    int rval = 0, i = 0;
    unsigned long timer_value = 0;
    EVHR_EVENT *  event = NULL;

    this->running = 1;
    
    // event loop
    while (this->running)
    {
        // Wait events
        while ((rval = epoll_wait(
                        this->epfd, this->events, 
                        this->events_count, -1)) < 0)
        {
            // Check Error
            if ((rval == -1) && (errno != EINTR))
            {
                EVHR_LOG_ERR("epoll wait failed! errno[%d]", errno);
                goto fail_and_exit;
            }
        }

        // Read and process event
        for (i = 0; i < rval; i++)
        {
            event = (EVHR_EVENT *) this->events[i].data.ptr;

            // EPOLLIN
            if (this->events[i].events & EPOLLIN)
            {

                EVHR_LOG_DBG("[EVENT] EPOLLIN, events[%d], type[%d].", 
                        this->events[i].events, record->type) ;

                if ((event->type == EVHR_EVENT_TYPE_TIMER_ONCE) ||
                    (event->type == EVHR_EVENT_TYPE_TIMER_PERIODIC))
                    read( event->fd, &timer_value, 8);
             
                if (event->in_cb)       
                    event->in_cb(event);

                continue;
            }

            // EPOLLERR || EPOLLHUP || EPOLLRDHUP
            if (    (this->events[i].events & EPOLLERR) || 
                    (this->events[i].events & EPOLLHUP) || 
                    (this->events[i].events & EPOLLRDHUP))
            {
                EVHR_LOG_DBG("[EVENT] EVENTS[%d] EPOLLERR[%d]/EPOLLHUP[%d]/EPOLLRDHUP[%d]",
                        this->events[i].events,
                        this->events[i].events & EPOLLERR,
                        this->events[i].events & EPOLLHUP,
                        this->events[i].events & EPOLLRDHUP);
                
                if (event->err_cb)
                    event->err_cb(event);

                continue;
            }
        }
    }

    EVHR_LOG_MSG("Exit event loop.");
    return EVHR_RTN_SUCCESS;

fail_and_exit:

    EVHR_LOG_ERR("Failed event loop! [%d].", this->running);
    this->running = 0;
    return EVHR_RTN_FAIL;

}

EVHR_RTN evhr_stop(EVHR_CTX * this)
{
    this->running = 0;
    return EVHR_RTN_SUCCESS;
}

EVHR_RTN evhr_event_del(EVHR_CTX * this, EVHR_EVENT * event)
{
    struct epoll_event ev;
    epoll_ctl(this->epfd, EPOLL_CTL_DEL, event->fd, &ev);

    free(event);

    return EVHR_RTN_SUCCESS;
}

EVHR_EVENT * evhr_event_add(EVHR_CTX * this, int fd, int type, int mode,
        void *pdata, EVHR_EVENT_CALLBACK in_cb, EVHR_EVENT_CALLBACK err_cb)
{
    struct epoll_event   ev;
    EVHR_EVENT * event = NULL;
  
    event = malloc(sizeof(EVHR_EVENT)); 
    if (event == NULL)
        return NULL;

    event->fd       = fd;
    event->type     = type;
    event->pdata    = pdata;
    event->in_cb    = in_cb;
    event->err_cb   = err_cb;
    ev.data.ptr     = event;

    /*
     EPOLLIN ：可讀事件（包括對方SOCKET正常關閉）
     EPOLLOUT：可寫事件；
     EPOLLPRI：緊急可讀事件（額外數據）；
     EPOLLERR：發生錯誤事件；
     EPOLLHUP：掛斷事件；
     EPOLLET： 設定為邊緣觸發(Edge Triggered)模式，相對水平觸發(Level Triggered)
     EPOLLONESHOT：只觸發事件一次（觸發後移出監聽事件）
    */
    ev.events = EPOLLIN;
    if (mode == EVHR_ET_MODE)
        ev.events |= EPOLLET;
    
    epoll_ctl(this->epfd, EPOLL_CTL_ADD, fd, &ev); 

    return event;
}

EVHR_EVENT * evhr_event_add_socket(EVHR_CTX * this, int fd, 
        void *pdata, EVHR_EVENT_CALLBACK in_cb, EVHR_EVENT_CALLBACK err_cb)
{
    return evhr_event_add(
            this, fd, 
            EVHR_EVENT_TYPE_SOCKET, EVHR_ET_MODE, 
            pdata, in_cb, err_cb);
}

EVHR_TIMER_FD evhr_event_create_timer()
{
    return timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
}

EVHR_RTN evhr_event_set_timer(EVHR_TIMER_FD timerfd, int sec, int nsec, int is_once)
{
    struct itimerspec tval;
    
    tval.it_value.tv_sec  = sec;
    tval.it_value.tv_nsec = nsec;
    if (is_once)
    {
        tval.it_interval.tv_sec  = 0;
        tval.it_interval.tv_nsec = 0;
    }
    else
    {
        tval.it_interval.tv_sec  = sec;
        tval.it_interval.tv_nsec = nsec;
    }
     
    if (timerfd_settime(timerfd, 0, &tval, NULL) == -1)
        return EVHR_RTN_FAIL;
    
    return EVHR_RTN_SUCCESS;
}

EVHR_RTN evhr_event_stop_timer(EVHR_TIMER_FD timerfd)
{
    return evhr_event_set_timer(timerfd, 0, 0, 0);
}

EVHR_EVENT * evhr_event_add_timer_periodic(EVHR_CTX * this, EVHR_TIMER_FD timerfd, 
        int sec, int nsec, void *pdata, EVHR_EVENT_CALLBACK in_cb)
{
    if (evhr_event_set_timer(timerfd, sec, nsec, 0) != EVHR_RTN_SUCCESS)
        return NULL;

    return evhr_event_add(
            this, timerfd, 
            EVHR_EVENT_TYPE_TIMER_PERIODIC, EVHR_ET_MODE,
            pdata, in_cb, NULL);
}

EVHR_EVENT * evhr_event_handler_add_timer_once(EVHR_CTX * this, EVHR_TIMER_FD timerfd, 
        int sec, int nsec, void *pdata, EVHR_EVENT_CALLBACK in_cb)
{
    if (evhr_event_set_timer(timerfd, sec, nsec, 1) != EVHR_RTN_SUCCESS)
        return NULL;

    return evhr_event_add(
            this, timerfd, 
            EVHR_EVENT_TYPE_TIMER_ONCE, EVHR_ET_MODE,
            pdata, in_cb, NULL);
}
