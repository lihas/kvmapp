#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

#include <err.h>

#include "log.h"

/**
 * error() - print error message, including errno string, to stderr
 *
 * @fmt: format string
 * @...: format arguments
 */
void error(const char *fmt, ...)
{
	va_list ap;

	assert(fmt != NULL);

	va_start(ap, fmt);
	vwarn(fmt, ap);
	va_end(ap);
}

/**
 * errorx() - print error message to stderr
 *
 * @fmt: format string
 * @...: format arguments
 */
void errorx(const char *fmt, ...)
{
	va_list ap;

	assert(fmt != NULL);

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
}

/**
 * fail() - print error message, including errno string, to stderr and exit
 *          with non-zero exit status
 *
 * @fmt: format string
 * @...: format arguments
 */
void fail(const char *fmt, ...)
{
	va_list ap;

	assert(fmt != NULL);

	va_start(ap, fmt);
	verr(EXIT_FAILURE, fmt, ap);
	va_end(ap);
}

/**
 * failx() - print error message to stderr and exit with non-zero exit status
 *
 * @fmt: format string
 * @...: format arguments
 */
void failx(const char *fmt, ...)
{
	va_list ap;

	assert(fmt != NULL);

	va_start(ap, fmt);
	verrx(EXIT_FAILURE, fmt, ap);
	va_end(ap);
}
