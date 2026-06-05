#ifndef HACTOOL_RECOVER_H
#define HACTOOL_RECOVER_H

/* Make the vendored hactool fork library-safe.
 *
 * hactool is a CLI tool: its error paths call exit(EXIT_FAILURE) (directly and
 * via the FATAL_ERROR macro in utils.h). Inside thomaz that exit() terminates
 * the WHOLE app ("O software foi fechado porque ocorreu um erro"). This header
 * is force-included (-include) into every hactool translation unit and rewrites
 * exit() into a recoverable longjmp back to the C++ wrapper
 * (nca_extract_switch.cpp), which then reports a clean error instead of crashing.
 */

#include <stdlib.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

extern jmp_buf      g_hactool_recover_jmp;     /* armed by the wrapper before a call */
extern volatile int g_hactool_recover_active;  /* 1 while a guarded call is running   */

/* Recoverable stand-in for exit(): longjmps when a guarded call is active,
 * otherwise performs a real process exit. Never returns. */
void hactool_recover_exit(int code) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

/* Intercept every exit() in the hactool sources. Placed after <stdlib.h> so the
 * real prototype is seen first; hactool_recover.c #undef's this to call libc. */
#define exit(code) hactool_recover_exit((code))

#endif /* HACTOOL_RECOVER_H */
