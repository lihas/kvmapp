#ifndef _LOG_H
#define _LOG_H

#ifdef __GNUC__
# define PRINTF(s, f) __attribute__((format(printf, s, f)))
# define NORETURN     __attribute__((noreturn))
#else /* __GNUC__ */
# define PRINTF(s, f)
# define NORETURN
#endif /* __GNUC__ */

#ifndef NDEBUG
void error (const char *, ...) PRINTF(1, 2);
void errorx(const char *, ...) PRINTF(1, 2);
#else /* NDEBUG */
static inline void PRINTF(1, 2) error (const char *fmt, ...) { (void) fmt; }
static inline void PRINTF(1, 2) errorx(const char *fmt, ...) { (void) fmt; }
#endif /* NDEBUG */

void fail (const char *, ...) PRINTF(1, 2) NORETURN;
void failx(const char *, ...) PRINTF(1, 2) NORETURN;

#endif /* _LOG_H */
