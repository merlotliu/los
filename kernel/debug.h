#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H

void panic_spin(char* finename, int line, const char* func, const char* condition);

/* __VA_ARGS__ 是预处理器的专用标识符，代表所有与省略号对应的参数
 * '...' 表示可变参数
 */
#define PANIC(...) panic_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef NDEBUG
	#define ASSERT(CONDITION) ((void)0)
#else
    	/* 符号 # 让编译器将宏的参数转化为字符串字面量 */
	#define ASSERT(CONDITION)\
        if(CONDITION) {} else { PANIC(#CONDITION); }
		
#endif /* __NDEBUG */

#endif /* __KERNEL_DEBUG_H */
