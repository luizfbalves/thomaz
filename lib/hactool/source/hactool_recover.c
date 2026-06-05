/* hactool_recover.c — recoverable replacement for hactool's process-killing exit().
 *
 * The -include of hactool_recover.h (see CMakeLists.txt) has already rewritten
 * exit() to hactool_recover_exit() at the top of THIS translation unit too, so
 * #undef it first: this file must be able to call the real libc exit() as the
 * non-guarded fallback.
 */
#undef exit

#include <stdlib.h>
#include <setjmp.h>

jmp_buf      g_hactool_recover_jmp;
volatile int g_hactool_recover_active = 0;

void hactool_recover_exit(int code)
{
    if (g_hactool_recover_active) {
        g_hactool_recover_active = 0;
        /* Jump back to the setjmp() in the C++ wrapper. A non-zero value is
         * required by longjmp; map a 0 exit code to 1. */
        longjmp(g_hactool_recover_jmp, code ? code : 1);
    }
    /* Not inside a guarded call: behave like a normal exit. */
    exit(code);
}
