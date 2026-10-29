#if !defined(UNBOUND_SHIM_H)
#define UNBOUND_SHIM_H

#include <stdint.h>

#include <event2/event.h>

uint16_t unbound_shim_start(struct event_base* const event_base);

void unbound_shim_stop(void);

#endif
