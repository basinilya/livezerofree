/* local */
#undef DBG
#undef INFO
#undef WARN
#undef ERR
#define DBG MYPROG_LOG_DEBUG
#define INFO MYPROG_LOG_MESSAGE
#define WARN MYPROG_LOG_WARNING
#define ERR MYPROG_LOG_ERROR

#define pSysError     myprog_pSysError
#define pWinsockError myprog_pWinsockError
#define pWin32Error   myprog_pWin32Error
#define log           myprog_log

#define COUNTOF(a) (sizeof(a) / sizeof(a[0]))

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#ifndef NO_DEBUG_MYPROG

#endif
