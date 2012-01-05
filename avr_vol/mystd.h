/* mystd.h */
#ifndef __MYSTD_H__
#define __MYSTD_H__

/* types */
typedef unsigned char  u1;
typedef unsigned short u2;
typedef unsigned long  u4;

typedef signed char  s1;
typedef signed short s2;
typedef signed long  s4;

typedef unsigned char bool;
#define FALSE 0
#define TRUE  1

#define LO    0
#define HI    1

#define OFF   0
#define ON    1


/* pseudo function */
#define enable_interrupt()    sei()
#define disable_interrupt()   cli()



#endif  /* __MYSTD_H__ */

