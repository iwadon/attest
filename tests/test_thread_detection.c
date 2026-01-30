/* Test file to verify thread detection macros */
#include "internal/attest_internal.h"
#include <stdio.h>

int main(void)
{
	printf("=== Thread Support Detection Test ===\n");

#if defined(ATT_THREADS_C11)
	printf("Thread support: C11 threads\n");
	printf("TLS qualifier: _Thread_local\n");
#elif defined(ATT_THREADS_POSIX)
	printf("Thread support: POSIX threads (pthread)\n");
#if defined(__GNUC__) || defined(__clang__)
	printf("TLS qualifier: __thread (GCC/Clang extension)\n");
#else
	printf("TLS qualifier: _Thread_local\n");
#endif
#elif defined(ATT_THREADS_WIN32)
	printf("Thread support: Windows threads\n");
	printf("TLS qualifier: __declspec(thread)\n");
#elif defined(ATT_THREADS_NONE)
	printf("Thread support: NONE\n");
	printf("TLS qualifier: (empty - global variables)\n");
#else
	printf("ERROR: No thread detection macro defined!\n");
	return 1;
#endif

	printf("\n=== Platform Detection ===\n");
#if defined(ATT_PLATFORM_WINDOWS)
	printf("Platform: Windows\n");
#elif defined(ATT_PLATFORM_POSIX)
	printf("Platform: POSIX\n");
#else
	printf("ERROR: No platform detected!\n");
	return 1;
#endif

	printf("\n=== Compiler Detection ===\n");
#if defined(ATT_COMPILER_MSVC)
	printf("Compiler: MSVC\n");
#elif defined(ATT_COMPILER_GCC_LIKE)
	printf("Compiler: GCC-like (GCC or Clang)\n");
#else
	printf("Compiler: Unknown\n");
#endif

	printf("\nAll detection macros working correctly.\n");
	return 0;
}
