#ifndef _LOG_H
#define _LOG_H

#include "kvmapp.h"

void error (const char *, ...) PRINTF(1, 2);
void errorx(const char *, ...) PRINTF(1, 2);

void fail (const char *, ...) PRINTF(1, 2) NORETURN;
void failx(const char *, ...) PRINTF(1, 2) NORETURN;

#endif /* _LOG_H */
