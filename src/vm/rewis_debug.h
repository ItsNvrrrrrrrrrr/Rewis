#ifndef rewis_debug_h
#define rewis_debug_h

#include "rewis_value.h"
#include "rewis_vm.h"

// Prints the stack trace for the current fiber.
//
// Used when a fiber throws a runtime error which is not caught.
void rewisDebugPrintStackTrace(RewisVM* vm);

// The "dump" functions are used for debugging Rewis itself. Normal code paths
// will not call them unless one of the various DEBUG_ flags is enabled.

// Prints a representation of [value] to stdout.
void rewisDumpValue(Value value);

// Prints a representation of the bytecode for [fn] at instruction [i].
int rewisDumpInstruction(RewisVM* vm, ObjFn* fn, int i);

// Prints the disassembled code for [fn] to stdout.
void rewisDumpCode(RewisVM* vm, ObjFn* fn);

// Prints the contents of the current stack for [fiber] to stdout.
void rewisDumpStack(ObjFiber* fiber);

#endif
