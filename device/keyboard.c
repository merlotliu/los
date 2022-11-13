#include "keyboard.h"
#include "interrupt.h"
#include "io.h"
#include "global.h"
#include "print.h"
#include "stdint.h"
#include "ioqueue.h"

#define KBD_BUF_PORT 0x60 /* keyboard R/W buffer register port is 0x60 */
#define K_ERROR_CODE 0x00 /* keyboard error code */

/* control char */
#define K_ESC       '\x1b'
#define K_BACKSPACE '\b'
#define K_TAB       '\t'
#define K_ENTER     '\r'
#define K_DELETE    '\x7f'

/* invisible char */
#define K_CHAR_INVISIBLE    0
#define K_CTRL_L            K_CHAR_INVISIBLE
#define K_CTRL_R            K_CHAR_INVISIBLE
#define K_SHIFT_L           K_CHAR_INVISIBLE
#define K_SHIFT_R           K_CHAR_INVISIBLE
#define K_ALT_L             K_CHAR_INVISIBLE
#define K_ALT_R             K_CHAR_INVISIBLE
#define K_CAPSLOCK          K_CHAR_INVISIBLE

/* MAKECODE & breakcode of control char */
#define K_SHIFT_L_MAKECODE  0x2a
#define K_SHIFT_R_MAKECODE  0x36
#define K_ALT_L_MAKECODE    0x38
#define K_ALT_R_MAKECODE    0xe038
#define K_CTRL_L_MAKECODE   0x1d
#define K_CTRL_R_MAKECODE   0xe01d
#define K_CTRL_L_BREAKCODE  0xe09d
#define K_CAPSLOCK_MAKECODE 0x3a

ioqueue_t kbd_buf; /* keyboard buffer */

/* 1 is push down, 0 is pop on. As for extern_status, 1 is extern key(begin with 0xe0). */
static bool ctrl_status, shift_status, alt_status, capslock_status, extern_scancode;

/* 以 make code 为索引的二维数组 */
static char keymap [][2] = {
/* 0x00 */ {K_ERROR_CODE, K_ERROR_CODE}, /* error */
/* 0x01 */ {K_ESC, K_ESC},
/* 0x02 */ {'1', '!'}, 
/* 0x03 */ {'2', '@'}, 
/* 0x04 */ {'3', '#'}, 
/* 0x05 */ {'4', '$'}, 
/* 0x06 */ {'5', '%'}, 
/* 0x07 */ {'6', '^'}, 
/* 0x08 */ {'7', '&'}, 
/* 0x09 */ {'8', '*'}, 
/* 0x0a */ {'9', '('}, 
/* 0x0b */ {'0', ')'}, 
/* 0x0c */ {'-', '_'}, 
/* 0x0d */ {'+', '+'}, 
/* 0x0e */ {K_BACKSPACE, K_BACKSPACE}, 
/* 0x0f */ {K_TAB, K_TAB}, 
/* 0x10 */ {'q', 'Q'}, 
/* 0x11 */ {'w', 'W'}, 
/* 0x12 */ {'e', 'E'}, 
/* 0x13 */ {'r', 'R'}, 
/* 0x14 */ {'t', 'T'}, 
/* 0x15 */ {'y', 'Y'}, 
/* 0x16 */ {'u', 'U'}, 
/* 0x17 */ {'i', 'I'}, 
/* 0x18 */ {'o', 'O'}, 
/* 0x19 */ {'p', 'P'}, 
/* 0x1a */ {'[', '{'}, 
/* 0x1b */ {']', '}'}, 
/* 0x1c */ {K_ENTER, K_ENTER}, 
/* 0x1d */ {K_CTRL_L, K_CTRL_L}, 
/* 0x1e */ {'a', 'A'}, 
/* 0x1f */ {'s', 'S'}, 
/* 0x20 */ {'d', 'D'},
/* 0x21 */ {'f', 'F'},
/* 0x22 */ {'g', 'G'},
/* 0x23 */ {'h', 'H'},
/* 0x24 */ {'j', 'J'},
/* 0x25 */ {'k', 'K'},
/* 0x26 */ {'l', 'L'},
/* 0x27 */ {';', ':'},
/* 0x28 */ {'\'', '"'},
/* 0x29 */ {'`', '~'}, 
/* 0x2a */ {K_SHIFT_L, K_SHIFT_L}, 
/* 0x2b */ {'\\', '|'}, 
/* 0x2c */ {'z', 'Z'},
/* 0x2d */ {'x', 'X'},
/* 0x2e */ {'c', 'C'},
/* 0x2f */ {'v', 'V'},
/* 0x30 */ {'b', 'B'},
/* 0x31 */ {'n', 'N'},
/* 0x32 */ {'m', 'M'},
/* 0x33 */ {',', '<'}, 
/* 0x34 */ {'.', '>'}, 
/* 0x35 */ {'/', '?'}, 
/* 0x36 */ {K_CTRL_R, K_CTRL_R}, 
/* 0x37 */ {'*', '*'}, 
/* 0x38 */ {K_ALT_L, K_ALT_L}, 
/* 0x39 */ {' ', ' '}, 
/* 0x3a */ {K_CAPSLOCK, K_CAPSLOCK}
/* ... ... */
};

static void intr_keyboard_handler(void) {
    
    bool UNUSED ctrl_down_last = ctrl_status;
    bool shift_down_last = shift_status;
    bool capslock_last = capslock_status;
    
    bool b_breakcode;
    uint16_t scancode = inb(KBD_BUF_PORT);
    // put_int(scancode); put_char(' ');

    /* 0xe0 开头位为扩展的扫描码，有多个字节 */
    if(scancode == 0xe0) {
        extern_scancode = true; /* open 0xe0 mark */
        return ;
    } else {
        
        b_breakcode = ((scancode & 0x0080) != 0); /* get break code */
        if(b_breakcode) {
            scancode &= 0xff7f; /* convert to makecode */
            uint16_t makecode = scancode;
            switch (makecode) {
                case K_CTRL_L_MAKECODE:
                case K_CTRL_R_MAKECODE: {
                    ctrl_status = false;
                    break;
                }
                case K_SHIFT_L_MAKECODE:
                case K_SHIFT_R_MAKECODE: {
                    shift_status = false;
                    break;
                }
                case K_ALT_L_MAKECODE:
                case K_ALT_R_MAKECODE: {
                    alt_status = false;
                    break;
                }
                case K_CAPSLOCK_MAKECODE: {
                    break;
                }
                default: {
                    break;
                }
            }
        } else if((scancode > 0x00 && scancode < 0x3b) || (scancode == K_ALT_L_MAKECODE) || (scancode == K_CTRL_R_MAKECODE) ) {
            bool b_shift = false;
            switch (scancode) {
                case 0x02: /* '1' */
                case 0x03:
                case 0x04:
                case 0x05:
                case 0x06:
                case 0x07:
                case 0x08:
                case 0x09:
                case 0x0a:
                case 0x0b:
                case 0x0c: /* '-' */
                case 0x0d: /* '=' */
                case 0x29: /* '`' */
                case 0x1a: /* '[' */
                case 0x1b: /* ']' */
                case 0x2b: /* '\\' */
                case 0x27: /* ';' */
                case 0x28: /* '\' */
                case 0x33: /* ',' */
                case 0x34: /* '.' */
                case 0x35: { /* '/' */
                    /* double-char key */
                    if(shift_down_last) {
                        b_shift = true;
                    }
                    break;
                }
                default: {
                    /* alpha key */
                    /* shift 和 capslock  */
                    if(shift_down_last == capslock_last) {
                        b_shift = false;
                    } else {
                        b_shift = true;
                    }
                    break;
                }
            }
            /* 高字节置为0，获取字符在数组的索引 */
            scancode &= 0x00ff;
            uint8_t key_idx = scancode;
            char cur_char = keymap[key_idx][b_shift];
            if(cur_char) { /* 0x0表示error暂不处理 */
                if(!ioq_full(&kbd_buf)) {
                    put_char(cur_char);
                    ioq_putchar(&kbd_buf, cur_char);
                }
                return;
            }
            switch (scancode) {
                case K_CTRL_L_MAKECODE:
                case K_CTRL_R_MAKECODE: {
                    ctrl_status = true;
                    break;
                }
                case K_SHIFT_L_MAKECODE:
                case K_SHIFT_R_MAKECODE: {
                    shift_status = true;
                    break;
                }
                case K_ALT_L_MAKECODE:
                case K_ALT_R_MAKECODE: {
                    alt_status = true;
                    break;
                }
                case K_CAPSLOCK_MAKECODE: {
                    /* 无论上一次 capslock 的状态如何，只要按下就对上一次状态取反 */
                    capslock_status = !capslock_status;
                    break;
                }
                default: {
                    break;
                }
            }
        } else {
            put_str("unknown key\n");
        }
    }
    return;
}

/* keyboard init */
void keyboard_init(void) {
    put_str("keyboard init start\n");
    ioq_init(&kbd_buf);
    put_str("   ");
    intr_handler_register(0x21, intr_keyboard_handler);
    put_str("keyboard init done\n");
}