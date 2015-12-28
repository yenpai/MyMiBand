#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>

#include "mmb_util.h"
#include "mmb_event.h"

static void read_cb(EVHR_EVENT * ev)
{
    MMB_EVENT * this = ev->pdata;
    MMB_EVENT_DATA data;
    int len;

    while (1)
    {
        len = read(this->sfd[0], &data, sizeof(data));
        if (len < (int) sizeof(data))
            break;;

        printf("[MMB][EVENT] Read type:0x%04x, size:%lu\n", data.type, data.size);

        if (this->cb)
            this->cb(&data, this->cb_pdata);

    }

}

int mmb_event_init(MMB_EVENT ** this)
{
    MMB_EVENT * obj;

    obj = malloc(sizeof(MMB_EVENT));
    if (obj == NULL)
        return -1;

    obj->status = 0;
    obj->ev = NULL;
    obj->sfd[0] = 0;
    obj->sfd[1] = 0;

    *this = obj;
    return 0;
}

int mmb_event_start(MMB_EVENT * this, EVHR_CTX * evhr, MMB_EVENT_CB cb, void * cb_pdata)
{

    if (socketpair(AF_UNIX,SOCK_STREAM, 0, this->sfd) == -1)
    { 
        printf("ERR: socketpair failed!\n");
        return -1;
    }

    socket_setting_non_blocking(this->sfd[0]);

    this->ev = evhr_event_add_socket(evhr, this->sfd[0], this, read_cb, NULL, NULL);

    this->cb = cb;
    this->cb_pdata = cb_pdata;

    this->status = 1;

    return 0;
}

int mmb_event_stop(MMB_EVENT * this)
{
    if (this->ev)
    {
        evhr_event_del(this->ev);
        this->ev = NULL;
    }

    if (this->sfd[0] > 0)
    {
        close(this->sfd[0]);
        close(this->sfd[1]);
        this->sfd[0] = 0;
        this->sfd[1] = 0;
    }

    this->status = 0;

    return 0;
}

int mmb_event_send(MMB_EVENT * this, uint16_t type, void * buf, size_t size)
{
    MMB_EVENT_DATA data;

    data.type   = type;
    data.size   = size;
    data.buf    = buf;

    printf("[MMB][EVENT] Send type:0x%04x, size:%lu\n", data.type, data.size);

    return write(this->sfd[1], &data, sizeof(data));
}
