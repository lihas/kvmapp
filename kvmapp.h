#ifndef _KVMAPP_H
#define _KVMAPP_H

/**
 * round_up() - round up a value x to a multiple of y
 *
 * @x: value to round up
 * @y: multiple to round up to (should be a power of two)
 *
 * Return: rounded up value
 */
#define round_up(x, y)   ((((x) - 1) | ((__typeof__(x)) ((y) - 1))) + 1)

/**
 * round_down() - round down a value x to a multiple of y
 *
 * @x: value to round down
 * @y: multiple to round down to (should be a power of two)
 *
 * Return: rounded down value
 */
#define round_down(x, y) ((x) & ~((__typeof__(x)) ((y) - 1)))

#endif /* _KVMAPP_H */
