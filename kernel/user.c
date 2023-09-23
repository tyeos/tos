//
// Created by toney on 23-9-20.
//


#include "../include/task.h"
#include "../include/sys.h"
#include "../include/syscall.h"

// 从汇编中回调
void user_entry() {
    for (int i = 0; i < 300; ++i) {
        int pid = syscall(SYS_GET_PID);
        syscall3(SYS_PRINT, "user print [%d] pid = %d\n", i, pid);
    }
    syscall(SYS_EXIT);
}

