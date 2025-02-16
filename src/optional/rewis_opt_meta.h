#ifndef rewis_opt_meta_h
#define rewis_opt_meta_h

#include "rewis_common.h"
#include "rewis.h"

// This module defines the Meta class and its associated methods.
#if REWIS_OPT_META

const char* rewisMetaSource();
RewisForeignMethodFn rewisMetaBindForeignMethod(RewisVM* vm,
                                              const char* className,
                                              bool isStatic,
                                              const char* signature);

#endif

#endif
