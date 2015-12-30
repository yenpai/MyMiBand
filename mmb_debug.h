#ifndef MMB_DEBUG_H_
#define MMB_DEBUG_H_

#define MMB_DEBUG_MODE

#ifdef MMB_DEBUG_MODE
    #include <assert.h>
    #define MMB_ASSERT(fmt) assert(fmt)
    #define MMB_DBG(tag, fmt, args...) \
        printf("[MMB][DBG]%-10s " fmt "\n", tag, ## args)
    #define MMB_LOG(tag, fmt, args...) \
        printf("[MMB][LOG]%-10s " fmt "\n", tag, ## args)
#else
    #define MMB_NOTHING(args...) do {} while(0)
    #define MMB_ASSERT(args...) MMB_NOTHING()
    #define MMB_DBG(tag, fmt, args...) MMB_NOTHING()
    #define MMB_LOG(tag, fmt, args...) \
        printf("[MMB]%-10s " fmt "\n", tag, ## args)
#endif

#endif
