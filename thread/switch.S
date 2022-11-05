[bits 32]
section .text
global switch_to

; switch cur_tcb to next_tcb
; 此时栈顶的三个元素从高地址到低地址依次是：
;   next_tcb cur_tcb return_address
switch_to:
    push esi
    push edi
    push ebx
    push ebp 

    mov eax, [esp + 20] ; 获取当前 tcb 地址
    mov [eax], esp ; 保存当前 tcb 的栈顶指针到 self_kstack，以便恢复环境

    ; 恢复下一个线程的环境 
    mov eax, [esp + 24] ; 获取下一个 tcb 地址
    mov esp, [eax] ; 取得其保存的栈顶指针，用以恢复环境

    pop ebp 
    pop ebx 
    pop edi
    pop esi
    ret