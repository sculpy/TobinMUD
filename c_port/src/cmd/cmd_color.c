#include "cmd_internal.h"

#include <strings.h>

bool cmd_color(descriptor_t *d, const char *args) {
    if (strcasecmp(args, "on") == 0) {
        d->color_enabled = true;
        descriptor_send(d, "Color is now ON.\r\n");
    } else if (strcasecmp(args, "off") == 0) {
        d->color_enabled = false;
        descriptor_send(d, "Color is now OFF.\r\n");
    } else if (!*args) {
        descriptor_send(d, d->color_enabled
                             ? "Color is currently ON. Usage: color on|off\r\n"
                             : "Color is currently OFF. Usage: color on|off\r\n");
    } else {
        descriptor_send(d, "Usage: color on|off\r\n");
    }
    return true; /* instant -- no wait-state cost */
}
