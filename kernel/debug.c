#include "debug.h"
#include "print.h"
#include "interrupt.h"

/* print filename, line no, function name, condition and hang out program  */
void panic_spin(char* filename, int line, const char* func, const char* condition) {
    intr_disable(); /* close interrupt, because sometimes we will individually call panic_spin */
    
	put_str("\n\n !!!!!! error !!!!!! \n");
    put_str("filename : ");
    put_str(filename);
    put_str("\n");
    put_str("line : 0x");
    put_int(line);
    put_str("\n");
    put_str("function : ");
    put_str((char*)func);
    put_str("\n");
    put_str("condition : ");
    put_str((char*)condition);
    put_str("\n");
    while(1);
}