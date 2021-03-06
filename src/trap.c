#include "trap.h"

#include "exc.h"
#include "io.h"
#include "sched.h"
#include "syscall.h"
#include "timer.h"

#define CORE_INT_SRC_0 ((volatile unsigned int *)0x40000060)
#define CNTPNSIRQ_INT (1 << 1)

#define EC_MASK (0b111111 << 26)
#define SVC_AARCH64 (0b010101 << 26)

void exc_not_imp() {
  log("exception not implemented\n", LOG_ERROR);
  die();
}

void syn_handler(unsigned long el1_esr, void *el1_elr, void *el0_sp,
                 unsigned long el1_spsr, saved_reg *el1_sp) {
  unsigned long prev_int_stat = get_int_stat();
  set_int_stat(1);
  exc_lvl_gain();
  enable_interrupt();
  log("SYN\n", LOG_DEBUG);
  switch (el1_esr & EC_MASK) {
    case SVC_AARCH64:
      log_hex("svc", el1_esr & SVC_MASK, LOG_DEBUG);
      switch (el1_esr & SVC_MASK) {
        case SYS_READ:
          el1_sp->x0 = sys_read(el1_sp->x0, (char *)(el1_sp->x1), el1_sp->x2);
          break;
        case SYS_WRITE:
          el1_sp->x0 = sys_write(el1_sp->x0, (char *)(el1_sp->x1), el1_sp->x2);
          break;
        case SYS_OPEN:
          el1_sp->x0 = vfs_open((char *)(el1_sp->x0), el1_sp->x1);
          break;
        case SYS_CLOSE:
          el1_sp->x0 = vfs_close(el1_sp->x0);
          break;
        case SYS_GETPID:
          el1_sp->x0 = get_pid();
          break;
        case SYS_FORK:
          fork(&el0_sp, &el1_sp);
          break;
        case SYS_EXECVE:
          execve((char *)(el1_sp->x0), (char **)(el1_sp->x1));
          break;
        case SYS_EXIT:
          die();
          break;
        case SYS_MKDIR:
          el1_sp->x0 = vfs_mkdir((char *)el1_sp->x0);
          break;
        case SYS_RMDIR:
          el1_sp->x0 = vfs_rmdir((char *)el1_sp->x0);
          break;
        case SYS_CREATE:
          el1_sp->x0 = vfs_create((char *)el1_sp->x0);
          break;
        case SYS_REMOVE:
          el1_sp->x0 = vfs_remove((char *)el1_sp->x0);
          break;
        case SYS_CHDIR:
          el1_sp->x0 = vfs_chdir((char *)el1_sp->x0);
          break;
        case SYS_MOUNT:
          el1_sp->x0 = vfs_mount((char *)el1_sp->x0, (char *)el1_sp->x1,
                                 (char *)el1_sp->x2);
          break;
        case SYS_UMOUNT:
          el1_sp->x0 = vfs_umount((char *)el1_sp->x0);
          break;
        case SYS_GETDENT:
          el1_sp->x0 =
              vfs_getdent(el1_sp->x0, el1_sp->x1, (dirent *)el1_sp->x2);
          break;
        default:
          break;
      }
      break;
    default:
      nonblock = 0;
      // flush();
      log_hex("SYN NI", el1_esr, LOG_ERROR);
      log_hex("lr", (unsigned long)el1_elr, LOG_ERROR);
      log_hex("spsr", el1_spsr, LOG_ERROR);
      log_hex("x0", el1_sp->x0, LOG_ERROR);
      log_hex("x5", el1_sp->x0, LOG_ERROR);
      die();
      break;
  }
  disable_interrupt();
  exc_lvl_consume();
  set_int_stat(prev_int_stat);
  ret_exc(el1_elr, el0_sp, el1_spsr, (void *)el1_sp);
}

void irq_handler(void *el1_elr, void *el0_sp, unsigned long el1_spsr,
                 saved_reg *el1_sp) {
  set_int_stat(1);
  exc_lvl_gain();
  log("IRQ\n", LOG_DEBUG);
  if ((*CORE_INT_SRC_0 & CNTPNSIRQ_INT) != 0) {
    core_timer_handler();
  } else if ((*IRQ_PENDING_1 & (1 << 29)) != 0) {
    uart_handler();
  } else {
    log("IRQ not implement\n", LOG_ERROR);
  }
  exc_lvl_consume();
  set_int_stat(0);
  ret_exc(el1_elr, el0_sp, el1_spsr, (void *)el1_sp);
}