#include "console.h"

/* locker of console */
static locker_t console_locker;

/* init console */
void console_init(void) {
    locker_init(&console_locker);
}

/* get console */
void console_acquire(void) {
    locker_lock(&console_locker);
}

/* release consle */
void console_release(void) {
    locker_unlock(&console_locker);
}

/* terminator print string */
void console_put_str(char* str) {
    console_acquire();
    put_str(str);
    console_release();
}

/* terminator print character */
void console_put_char(uint8_t char_asci) {
    console_acquire();
    put_char(char_asci);
    console_release();
}

/* terminator print integer */
void console_put_int (uint32_t num) {
    console_acquire() ;
    put_int (num);
    console_release();
}