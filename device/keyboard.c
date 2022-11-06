#include "keyboard.h"
#include "interrupt.h"
#include "io.h"
#include "global.h"
#include "print.h"

#define KBD_BUF_PORT 0x60 /* keyboard R/W buffer register port is 0x60 */

static void intr_keyboard_handler(void) {
    uint8_t scan_code = inb(KBD_BUF_PORT);
    put_int(scan_code); put_char(' ');
    return;
}

/* keyboard init */
void keyboard_init(void) {
    put_str("keyboard init start\n");
    intr_handler_register(0x21, intr_keyboard_handler);
    put_str("keyboard init done\n");
}