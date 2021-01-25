#include <psp2/kernel/threadmgr.h>

#define SCE_KERNEL_PRIO_USER_NORMAL 96

#if !defined(THREAD_STACK_SIZE)
#define THREAD_STACK_SIZE       0x10000
#endif

#define VITA_STACKSIZE(x)        (x ? x : THREAD_STACK_SIZE)

/*
 * Initialization.
 */
  static void
PyThread__init_thread(void)
{
  return;
}

typedef struct {
  void (*func)(void*);
  void *arg;
} callobj;

static int bootstrap(SceSize args, void *call){
  callobj *obj = *(callobj**) call;
  void (*func)(void*) = obj->func;
  void *arg = obj->arg;

  printf("Calling Bootstrap: Func: 0x%08x Args: 0x%08x\n", (*func), arg);
  (*func)(arg);
  printf("Ummmm: Func: 0x%08x Args: 0x%08x\n", obj->func, obj->arg);
  free(obj);
  return 0;
}

/*
 * Thread support.
 */
  long
PyThread_start_new_thread(void (*func)(void *), void *arg)
{
  char name[SCE_UID_NAMELEN];
  callobj* obj;

  dprintf(("PyThread_start_new_thread called\n"));

  obj = (callobj *)malloc(sizeof(callobj));

  obj->func = func;
  obj->arg = arg;


  SceUID thid = sceKernelCreateThread("python thread", (SceKernelThreadEntry)bootstrap, SCE_KERNEL_PRIO_USER_NORMAL, VITA_STACKSIZE(_pythread_stacksize), 0, 0, NULL);

  printf("Created Thread: Func: 0x%08x Args: 0x%08x\n", obj->func, obj->arg);

  if(thid < 0) {
    return -1;
  }

  printf("Starting Thread: Func: 0x%08x Args: 0x%08x\n", obj->func, obj->arg);

//  (*func)(arg);

  int success = sceKernelStartThread(thid, sizeof(obj), &obj);


  printf("Done?: Func: 0x%08x Args: 0x%08x\n", (*func), obj->arg);

  if(success != 0) {
    return -1;
  }

  return (long)thid;
}

  long
PyThread_get_thread_ident(void)
{
  int id = sceKernelGetThreadId();
  return (id < 0) ? -1 : (long) id;
}

  void
PyThread_exit_thread(void)
{
  dprintf(("PyThread_exit_thread called\n"));
  sceKernelExitDeleteThread(0);
}

/*
 * Lock support.
 */

PyThread_type_lock PyThread_allocate_lock(void)
{
  SceUID* lock = (SceUID*)malloc(sizeof(SceUID));

  char name[SCE_UID_NAMELEN];

  dprintf(("PyThread_allocate_lock called\n"));

  *lock = sceKernelCreateMutex("python mutex", 0/*SCE_KERNEL_MUTEX_ATTR_RECURSIVE*/, 0, NULL);
  if (*lock < 0) {
    perror("mutex_init");
    return NULL;
  }

  dprintf(("PyThread_allocate_lock() -> %p\n", (SceUID)(*lock)));
  return (PyThread_type_lock) lock;
}

  void
PyThread_free_lock(PyThread_type_lock lock)
{
  SceUID* thelock = (SceUID*) lock;

  if (thelock == NULL)
    return;

  dprintf(("PyThread_free_lock(%p) called\n", (SceUID)(*thelock)));
  sceKernelDeleteMutex(*thelock);
}

  int
PyThread_acquire_lock(PyThread_type_lock lock, int waitflag)
{
  int success, status = 0;
  SceUID* thelock = (SceUID*) lock;

  dprintf(("PyThread_acquire_lock(%p, %d) called\n", (SceUID)(*thelock), waitflag));
  if (waitflag) {             /* blocking */
    status = sceKernelLockMutex(*thelock, 1, NULL);
  } else {                    /* non blocking */
    status = sceKernelTryLockMutex(*thelock, 1);
  }
  success = (status >= 0) ? 1 : 0;
  dprintf(("PyThread_acquire_lock(%p, %d) -> %d\n", (SceUID)(*thelock), waitflag, success));
  return success;
}

  void
PyThread_release_lock(PyThread_type_lock lock)
{
  SceUID* thelock = (SceUID*) lock;
  dprintf(("PyThread_release_lock(%p) called\n", (SceUID)(*thelock)));
  sceKernelUnlockMutex(*thelock, 1);
}

/* minimum/maximum thread stack sizes supported */
#define THREAD_MIN_STACKSIZE    0x1000          /* 32kB */
#define THREAD_MAX_STACKSIZE    0x2000000       /* 32MB */

/* set the thread stack size.
 * Return 0 if size is valid, -1 otherwise.
 */
  static int
_pythread_vita_set_stacksize(size_t size)
{
  /* set to default */
  if (size == 0) {
    _pythread_stacksize = 0;
    return 0;
  }

  /* valid range? */
  if (size >= THREAD_MIN_STACKSIZE && size < THREAD_MAX_STACKSIZE) {
    _pythread_stacksize = size;
    return 0;
  }

  return -1;
}

#define THREAD_SET_STACKSIZE(x) _pythread_vita_set_stacksize(x)