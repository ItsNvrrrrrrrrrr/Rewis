#ifndef rewis_opt_random_h
#define rewis_opt_random_h

#include "rewis_common.h"
#include "rewis.h"

#if REWIS_OPT_RANDO

const char* rewisRandomSource();
RewisForeignClassMethods rewisRandomBindForeignClass(RewisVM* vm,
                                                   const char* module,
                                                   const char* className);
RewisForeignMethodFn rewisRandomBindForeignMethod(RewisVM* vm,
                                                const char* className,
                                                bool isStatic,
                                                const char* signature);

#endif

#endif
