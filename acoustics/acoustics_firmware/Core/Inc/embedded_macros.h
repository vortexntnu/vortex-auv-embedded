/*
 * embedded_macros.h
 *
 *  Created on: 23. mar. 2026
 *      Author: vikin
 */

#ifndef INC_EMBEDDED_MACROS_H_
#define INC_EMBEDDED_MACROS_H_

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#endif /* INC_EMBEDDED_MACROS_H_ */
