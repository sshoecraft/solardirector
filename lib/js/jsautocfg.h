
#ifndef __JSAUTOCFG_H
#define __JSAUTOCFG_H

#if defined _WIN64
//#error X1
  #include "jscpu_win64.h"
#elif defined _WIN32
//#error X2
  #include "jscpu_win32.h"
#elif defined(__arm__) && (__SIZEOF_LONG__ == 4)
//#error X3
  #include "jscpu_arm32.h"
#elif defined __x86_64__ && !defined __ILP32__
//#error X4
  #include "jscpu_lin64.h"
#elif defined(__aarch64__) || defined(_M_ARM64)
  #include "jscpu_lin64.h"
#else
//#error X5
  #include "jscpu_lin32x.h"
#endif

#endif
