#include <stdlib.h>

#include "evhr.h"
#include "mmb_miband.h"

static int led_rand_color()
{
    int rand = 0;
    int colors[6] = { 
        MMB_LED_COLOR_RED, 
        MMB_LED_COLOR_GREEN, 
        MMB_LED_COLOR_YELLOW, 
        MMB_LED_COLOR_GREEN, 
        MMB_LED_COLOR_BLUE,
        MMB_LED_COLOR_WHITE
    };

    rand = (random() % 6);
    return colors[rand];
}

static void led_timer_cb(EVHR_EVENT * ev)
{
    MMB_MIBAND * this = ev->pdata;
    unsigned int next_sec = 0;
    unsigned int next_msec = 0;
    int color = MMB_LED_COLOR_OFF;
    int send  = 0;

    switch (this->led_mode)
    {
        // off-mode
        default:
        case 0:
            break;

        // connected
        case 1:
            send  = 1;

            if (this->led_index == 0)
                color = MMB_LED_COLOR_OFF;
            else if (this->led_index == 1)
                color = MMB_LED_COLOR_RED;
            else if (this->led_index == 2)
                color = MMB_LED_COLOR_OFF;
            else if (this->led_index == 3)
                color = MMB_LED_COLOR_ORANGE;
            else if (this->led_index == 4)
                color = MMB_LED_COLOR_OFF;
            else if (this->led_index == 5)
                color = MMB_LED_COLOR_YELLOW;
            else if (this->led_index == 6)
                color = MMB_LED_COLOR_OFF;
            else if (this->led_index == 7)
                color = MMB_LED_COLOR_GREEN;
            else if (this->led_index == 8)
                color = MMB_LED_COLOR_OFF;
            else if (this->led_index == 9)
                color = MMB_LED_COLOR_BLUE;
            else
            {
                this->led_mode = 2;
                this->led_index = 0;
                send = 0;
                break;
            }

            next_sec = 0;
            next_msec = 250;

            this->led_index++;

            break;

        // sleep mode
        case 2:
            send  = 1;
            color = MMB_LED_COLOR_BLUE;
            next_sec = 60;
            break;

        // notify-trigger
        case 3:

            printf("led index = %d\n", this->led_index);

            send = 1;

            if (this->led_index == 0)
            {
                color = MMB_LED_COLOR_OFF;
                this->led_mode = 2;
                next_sec  = 1;
                next_msec = 0;
                break;
            }

            if (this->led_index % 2 == 0)
                color = MMB_LED_COLOR_OFF;
            else
                color = led_rand_color();

            if (this->led_index < 10)
            {
                send = 0;
                next_sec = 0;
                next_msec = 100;
            }
            else if (this->led_index < 20)
            {
                next_sec = 1;
                next_msec = 0;
            }
            else
            {
                next_sec = 0;
                next_msec = 250;
            }

            this->led_index--;
            
            if (this->led_index > 50)
                this->led_index = 50;
            if (this->led_index == 19)
                this->led_index = 13;

            break;
    }
   
    if (send)
        mmb_miband_send_ledcolor(this, color);

    if (next_sec > 0 || next_msec > 0) {
        evhr_event_set_timer(this->ev_led_timer, next_sec, next_msec, 1);
    }

}

int mmb_miband_led_mode_change(MMB_MIBAND * this, int mode)
{
    this->led_mode = mode;
    this->led_index = 0;

    evhr_event_set_timer(this->ev_led_timer, 0, 0, 1);
    return 0;
}

int mmb_miband_led_start(MMB_MIBAND * this)
{

    this->led_mode = 0;
    this->led_index = 0;

    // Add timer into event handler
    if ((this->ev_led_timer = evhr_event_add_timer_once(
            this->evhr, 0, 10, this, led_timer_cb)) == NULL)
    {
        printf("[MMB][MIBAND][ERROR] Bind Timer event failed!\n");
        mmb_miband_stop(this);
        return -4;
    }

    return 0;
}

int mmb_miband_led_stop(MMB_MIBAND * this)
{

    if (this->ev_led_timer) {
        evhr_event_del(this->ev_led_timer);
        this->ev_led_timer = NULL;
    }

    return 0;
}
