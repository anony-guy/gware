import re
import sys

def replace_errors(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    # Include setjmp and stdarg if not there
    if '<setjmp.h>' not in content:
        content = content.replace('#include "eval.h"', '#include "eval.h"\n#include <setjmp.h>\n#include <stdarg.h>\n\nstatic jmp_buf jmp_stack[64];\nstatic int jmp_stack_ptr = -1;\nstatic char last_error_msg[1024];\n\nvoid throw_error(const char* fmt, ...) {\n    va_list args;\n    va_start(args, fmt);\n    vsnprintf(last_error_msg, sizeof(last_error_msg), fmt, args);\n    va_end(args);\n    if (jmp_stack_ptr >= 0) {\n        longjmp(jmp_stack[jmp_stack_ptr], 1);\n    } else {\n        printf(ANSI_COLOR_RED "Runtime Error: %s\\n" ANSI_COLOR_RESET, last_error_msg);\n        exit(1);\n    }\n}\n')

    # Now we need to replace:
    # printf(ANSI_COLOR_RED "Runtime Error: ...\n" ANSI_COLOR_RESET, args...); exit(1);
    # with throw_error("...", args...);
    # And:
    # printf(ANSI_COLOR_RED "Runtime Error: ...\n" ANSI_COLOR_RESET); exit(1);
    # with throw_error("...");

    # Pattern for printf with args followed by exit(1)
    # Using re.sub with a callback or robust pattern
    
    # Simple replacement:
    # 1. Without args: printf(ANSI_COLOR_RED "Runtime Error: TEXT\n" ANSI_COLOR_RESET); exit(1);
    content = re.sub(
        r'printf\(ANSI_COLOR_RED "Runtime Error: (.*?)\\n" ANSI_COLOR_RESET\);\s*exit\(1\);',
        r'throw_error("\1");',
        content
    )
    
    # 2. With args: printf(ANSI_COLOR_RED "Runtime Error: TEXT\n" ANSI_COLOR_RESET, ARGS); exit(1);
    content = re.sub(
        r'printf\(ANSI_COLOR_RED "Runtime Error: (.*?)\\n" ANSI_COLOR_RESET\s*,\s*(.*?)\);\s*exit\(1\);',
        r'throw_error("\1", \2);',
        content
    )

    with open(filepath, 'w') as f:
        f.write(content)

replace_errors('src/eval.c')
