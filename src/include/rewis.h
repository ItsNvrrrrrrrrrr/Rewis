#ifndef rewis_h
#define rewis_h

#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

// The Rewis semantic version number components.
#define REWIS_VERSION_MAJOR 0
#define REWIS_VERSION_MINOR 4
#define REWIS_VERSION_PATCH 0

// A human-friendly string representation of the version.
#define REWIS_VERSION_STRING "0.4.0"

// A monotonically increasing numeric representation of the version number. Use
// this if you want to do range checks over versions.
#define REWIS_VERSION_NUMBER (REWIS_VERSION_MAJOR * 1000000 +                    \
                             REWIS_VERSION_MINOR * 1000 +                       \
                             REWIS_VERSION_PATCH)

#ifndef REWIS_API
  #if defined(_MSC_VER) && defined(REWIS_API_DLLEXPORT)
    #define REWIS_API __declspec( dllexport )
  #else
    #define REWIS_API
  #endif
#endif //REWIS_API

// A single virtual machine for executing Rewis code.
//
// Rewis has no global state, so all state stored by a running interpreter lives
// here.
typedef struct RewisVM RewisVM;

// A handle to a Rewis object.
//
// This lets code outside of the VM hold a persistent reference to an object.
// After a handle is acquired, and until it is released, this ensures the
// garbage collector will not reclaim the object it references.
typedef struct RewisHandle RewisHandle;

// A generic allocation function that handles all explicit memory management
// used by Rewis. It's used like so:
//
// - To allocate new memory, [memory] is NULL and [newSize] is the desired
//   size. It should return the allocated memory or NULL on failure.
//
// - To attempt to grow an existing allocation, [memory] is the memory, and
//   [newSize] is the desired size. It should return [memory] if it was able to
//   grow it in place, or a new pointer if it had to move it.
//
// - To shrink memory, [memory] and [newSize] are the same as above but it will
//   always return [memory].
//
// - To free memory, [memory] will be the memory to free and [newSize] will be
//   zero. It should return NULL.
typedef void* (*RewisReallocateFn)(void* memory, size_t newSize, void* userData);

// A function callable from Rewis code, but implemented in C.
typedef void (*RewisForeignMethodFn)(RewisVM* vm);

// A finalizer function for freeing resources owned by an instance of a foreign
// class. Unlike most foreign methods, finalizers do not have access to the VM
// and should not interact with it since it's in the middle of a garbage
// collection.
typedef void (*RewisFinalizerFn)(void* data);

// Gives the host a chance to canonicalize the imported module name,
// potentially taking into account the (previously resolved) name of the module
// that contains the import. Typically, this is used to implement relative
// imports.
typedef const char* (*RewisResolveModuleFn)(RewisVM* vm,
    const char* importer, const char* name);

// Forward declare
struct RewisLoadModuleResult;

// Called after loadModuleFn is called for module [name]. The original returned result
// is handed back to you in this callback, so that you can free memory if appropriate.
typedef void (*RewisLoadModuleCompleteFn)(RewisVM* vm, const char* name, struct RewisLoadModuleResult result);

// The result of a loadModuleFn call. 
// [source] is the source code for the module, or NULL if the module is not found.
// [onComplete] an optional callback that will be called once Rewis is done with the result.
typedef struct RewisLoadModuleResult
{
  const char* source;
  RewisLoadModuleCompleteFn onComplete;
  void* userData;
} RewisLoadModuleResult;

// Loads and returns the source code for the module [name].
typedef RewisLoadModuleResult (*RewisLoadModuleFn)(RewisVM* vm, const char* name);

// Returns a pointer to a foreign method on [className] in [module] with
// [signature].
typedef RewisForeignMethodFn (*RewisBindForeignMethodFn)(RewisVM* vm,
    const char* module, const char* className, bool isStatic,
    const char* signature);

// Displays a string of text to the user.
typedef void (*RewisWriteFn)(RewisVM* vm, const char* text);

typedef enum
{
  // A syntax or resolution error detected at compile time.
  REWIS_ERROR_COMPILE,

  // The error message for a runtime error.
  REWIS_ERROR_RUNTIME,

  // One entry of a runtime error's stack trace.
  REWIS_ERROR_STACK_TRACE
} RewisErrorType;

// Reports an error to the user.
//
// An error detected during compile time is reported by calling this once with
// [type] `REWIS_ERROR_COMPILE`, the resolved name of the [module] and [line]
// where the error occurs, and the compiler's error [message].
//
// A runtime error is reported by calling this once with [type]
// `REWIS_ERROR_RUNTIME`, no [module] or [line], and the runtime error's
// [message]. After that, a series of [type] `REWIS_ERROR_STACK_TRACE` calls are
// made for each line in the stack trace. Each of those has the resolved
// [module] and [line] where the method or function is defined and [message] is
// the name of the method or function.
typedef void (*RewisErrorFn)(
    RewisVM* vm, RewisErrorType type, const char* module, int line,
    const char* message);

typedef struct
{
  // The callback invoked when the foreign object is created.
  //
  // This must be provided. Inside the body of this, it must call
  // [rewisSetSlotNewForeign()] exactly once.
  RewisForeignMethodFn allocate;

  // The callback invoked when the garbage collector is about to collect a
  // foreign object's memory.
  //
  // This may be `NULL` if the foreign class does not need to finalize.
  RewisFinalizerFn finalize;
} RewisForeignClassMethods;

// Returns a pair of pointers to the foreign methods used to allocate and
// finalize the data for instances of [className] in resolved [module].
typedef RewisForeignClassMethods (*RewisBindForeignClassFn)(
    RewisVM* vm, const char* module, const char* className);

typedef struct
{
  // The callback Rewis will use to allocate, reallocate, and deallocate memory.
  //
  // If `NULL`, defaults to a built-in function that uses `realloc` and `free`.
  RewisReallocateFn reallocateFn;

  // The callback Rewis uses to resolve a module name.
  //
  // Some host applications may wish to support "relative" imports, where the
  // meaning of an import string depends on the module that contains it. To
  // support that without baking any policy into Rewis itself, the VM gives the
  // host a chance to resolve an import string.
  //
  // Before an import is loaded, it calls this, passing in the name of the
  // module that contains the import and the import string. The host app can
  // look at both of those and produce a new "canonical" string that uniquely
  // identifies the module. This string is then used as the name of the module
  // going forward. It is what is passed to [loadModuleFn], how duplicate
  // imports of the same module are detected, and how the module is reported in
  // stack traces.
  //
  // If you leave this function NULL, then the original import string is
  // treated as the resolved string.
  //
  // If an import cannot be resolved by the embedder, it should return NULL and
  // Rewis will report that as a runtime error.
  //
  // Rewis will take ownership of the string you return and free it for you, so
  // it should be allocated using the same allocation function you provide
  // above.
  RewisResolveModuleFn resolveModuleFn;

  // The callback Rewis uses to load a module.
  //
  // Since Rewis does not talk directly to the file system, it relies on the
  // embedder to physically locate and read the source code for a module. The
  // first time an import appears, Rewis will call this and pass in the name of
  // the module being imported. The method will return a result, which contains
  // the source code for that module. Memory for the source is owned by the 
  // host application, and can be freed using the onComplete callback.
  //
  // This will only be called once for any given module name. Rewis caches the
  // result internally so subsequent imports of the same module will use the
  // previous source and not call this.
  //
  // If a module with the given name could not be found by the embedder, it
  // should return NULL and Rewis will report that as a runtime error.
  RewisLoadModuleFn loadModuleFn;

  // The callback Rewis uses to find a foreign method and bind it to a class.
  //
  // When a foreign method is declared in a class, this will be called with the
  // foreign method's module, class, and signature when the class body is
  // executed. It should return a pointer to the foreign function that will be
  // bound to that method.
  //
  // If the foreign function could not be found, this should return NULL and
  // Rewis will report it as runtime error.
  RewisBindForeignMethodFn bindForeignMethodFn;

  // The callback Rewis uses to find a foreign class and get its foreign methods.
  //
  // When a foreign class is declared, this will be called with the class's
  // module and name when the class body is executed. It should return the
  // foreign functions uses to allocate and (optionally) finalize the bytes
  // stored in the foreign object when an instance is created.
  RewisBindForeignClassFn bindForeignClassFn;

  // The callback Rewis uses to display text when `System.print()` or the other
  // related functions are called.
  //
  // If this is `NULL`, Rewis discards any printed text.
  RewisWriteFn writeFn;

  // The callback Rewis uses to report errors.
  //
  // When an error occurs, this will be called with the module name, line
  // number, and an error message. If this is `NULL`, Rewis doesn't report any
  // errors.
  RewisErrorFn errorFn;

  // The number of bytes Rewis will allocate before triggering the first garbage
  // collection.
  //
  // If zero, defaults to 10MB.
  size_t initialHeapSize;

  // After a collection occurs, the threshold for the next collection is
  // determined based on the number of bytes remaining in use. This allows Rewis
  // to shrink its memory usage automatically after reclaiming a large amount
  // of memory.
  //
  // This can be used to ensure that the heap does not get too small, which can
  // in turn lead to a large number of collections afterwards as the heap grows
  // back to a usable size.
  //
  // If zero, defaults to 1MB.
  size_t minHeapSize;

  // Rewis will resize the heap automatically as the number of bytes
  // remaining in use after a collection changes. This number determines the
  // amount of additional memory Rewis will use after a collection, as a
  // percentage of the current heap size.
  //
  // For example, say that this is 50. After a garbage collection, when there
  // are 400 bytes of memory still in use, the next collection will be triggered
  // after a total of 600 bytes are allocated (including the 400 already in
  // use.)
  //
  // Setting this to a smaller number wastes less memory, but triggers more
  // frequent garbage collections.
  //
  // If zero, defaults to 50.
  int heapGrowthPercent;

  // User-defined data associated with the VM.
  void* userData;

} RewisConfiguration;

typedef enum
{
  REWIS_RESULT_SUCCESS,
  REWIS_RESULT_COMPILE_ERROR,
  REWIS_RESULT_RUNTIME_ERROR
} RewisInterpretResult;

// The type of an object stored in a slot.
//
// This is not necessarily the object's *class*, but instead its low level
// representation type.
typedef enum
{
  REWIS_TYPE_BOOL,
  REWIS_TYPE_NUM,
  REWIS_TYPE_FOREIGN,
  REWIS_TYPE_LIST,
  REWIS_TYPE_MAP,
  REWIS_TYPE_NULL,
  REWIS_TYPE_STRING,

  // The object is of a type that isn't accessible by the C API.
  REWIS_TYPE_UNKNOWN
} RewisType;

// Get the current rewis version number.
//
// Can be used to range checks over versions.
REWIS_API int rewisGetVersionNumber();

// Initializes [configuration] with all of its default values.
//
// Call this before setting the particular fields you care about.
REWIS_API void rewisInitConfiguration(RewisConfiguration* configuration);

// Creates a new Rewis virtual machine using the given [configuration]. Rewis
// will copy the configuration data, so the argument passed to this can be
// freed after calling this. If [configuration] is `NULL`, uses a default
// configuration.
REWIS_API RewisVM* rewisNewVM(RewisConfiguration* configuration);

// Disposes of all resources is use by [vm], which was previously created by a
// call to [rewisNewVM].
REWIS_API void rewisFreeVM(RewisVM* vm);

// Immediately run the garbage collector to free unused memory.
REWIS_API void rewisCollectGarbage(RewisVM* vm);

// Runs [source], a string of Rewis source code in a new fiber in [vm] in the
// context of resolved [module].
REWIS_API RewisInterpretResult rewisInterpret(RewisVM* vm, const char* module,
                                  const char* source);

// Creates a handle that can be used to invoke a method with [signature] on
// using a receiver and arguments that are set up on the stack.
//
// This handle can be used repeatedly to directly invoke that method from C
// code using [rewisCall].
//
// When you are done with this handle, it must be released using
// [rewisReleaseHandle].
REWIS_API RewisHandle* rewisMakeCallHandle(RewisVM* vm, const char* signature);

// Calls [method], using the receiver and arguments previously set up on the
// stack.
//
// [method] must have been created by a call to [rewisMakeCallHandle]. The
// arguments to the method must be already on the stack. The receiver should be
// in slot 0 with the remaining arguments following it, in order. It is an
// error if the number of arguments provided does not match the method's
// signature.
//
// After this returns, you can access the return value from slot 0 on the stack.
REWIS_API RewisInterpretResult rewisCall(RewisVM* vm, RewisHandle* method);

// Releases the reference stored in [handle]. After calling this, [handle] can
// no longer be used.
REWIS_API void rewisReleaseHandle(RewisVM* vm, RewisHandle* handle);

// The following functions are intended to be called from foreign methods or
// finalizers. The interface Rewis provides to a foreign method is like a
// register machine: you are given a numbered array of slots that values can be
// read from and written to. Values always live in a slot (unless explicitly
// captured using rewisGetSlotHandle(), which ensures the garbage collector can
// find them.
//
// When your foreign function is called, you are given one slot for the receiver
// and each argument to the method. The receiver is in slot 0 and the arguments
// are in increasingly numbered slots after that. You are free to read and
// write to those slots as you want. If you want more slots to use as scratch
// space, you can call rewisEnsureSlots() to add more.
//
// When your function returns, every slot except slot zero is discarded and the
// value in slot zero is used as the return value of the method. If you don't
// store a return value in that slot yourself, it will retain its previous
// value, the receiver.
//
// While Rewis is dynamically typed, C is not. This means the C interface has to
// support the various types of primitive values a Rewis variable can hold: bool,
// double, string, etc. If we supported this for every operation in the C API,
// there would be a combinatorial explosion of functions, like "get a
// double-valued element from a list", "insert a string key and double value
// into a map", etc.
//
// To avoid that, the only way to convert to and from a raw C value is by going
// into and out of a slot. All other functions work with values already in a
// slot. So, to add an element to a list, you put the list in one slot, and the
// element in another. Then there is a single API function rewisInsertInList()
// that takes the element out of that slot and puts it into the list.
//
// The goal of this API is to be easy to use while not compromising performance.
// The latter means it does not do type or bounds checking at runtime except
// using assertions which are generally removed from release builds. C is an
// unsafe language, so it's up to you to be careful to use it correctly. In
// return, you get a very fast FFI.

// Returns the number of slots available to the current foreign method.
REWIS_API int rewisGetSlotCount(RewisVM* vm);

// Ensures that the foreign method stack has at least [numSlots] available for
// use, growing the stack if needed.
//
// Does not shrink the stack if it has more than enough slots.
//
// It is an error to call this from a finalizer.
REWIS_API void rewisEnsureSlots(RewisVM* vm, int numSlots);

// Gets the type of the object in [slot].
REWIS_API RewisType rewisGetSlotType(RewisVM* vm, int slot);

// Reads a boolean value from [slot].
//
// It is an error to call this if the slot does not contain a boolean value.
REWIS_API bool rewisGetSlotBool(RewisVM* vm, int slot);

// Reads a byte array from [slot].
//
// The memory for the returned string is owned by Rewis. You can inspect it
// while in your foreign method, but cannot keep a pointer to it after the
// function returns, since the garbage collector may reclaim it.
//
// Returns a pointer to the first byte of the array and fill [length] with the
// number of bytes in the array.
//
// It is an error to call this if the slot does not contain a string.
REWIS_API const char* rewisGetSlotBytes(RewisVM* vm, int slot, int* length);

// Reads a number from [slot].
//
// It is an error to call this if the slot does not contain a number.
REWIS_API double rewisGetSlotDouble(RewisVM* vm, int slot);

// Reads a foreign object from [slot] and returns a pointer to the foreign data
// stored with it.
//
// It is an error to call this if the slot does not contain an instance of a
// foreign class.
REWIS_API void* rewisGetSlotForeign(RewisVM* vm, int slot);

// Reads a string from [slot].
//
// The memory for the returned string is owned by Rewis. You can inspect it
// while in your foreign method, but cannot keep a pointer to it after the
// function returns, since the garbage collector may reclaim it.
//
// It is an error to call this if the slot does not contain a string.
REWIS_API const char* rewisGetSlotString(RewisVM* vm, int slot);

// Creates a handle for the value stored in [slot].
//
// This will prevent the object that is referred to from being garbage collected
// until the handle is released by calling [rewisReleaseHandle()].
REWIS_API RewisHandle* rewisGetSlotHandle(RewisVM* vm, int slot);

// Stores the boolean [value] in [slot].
REWIS_API void rewisSetSlotBool(RewisVM* vm, int slot, bool value);

// Stores the array [length] of [bytes] in [slot].
//
// The bytes are copied to a new string within Rewis's heap, so you can free
// memory used by them after this is called.
REWIS_API void rewisSetSlotBytes(RewisVM* vm, int slot, const char* bytes, size_t length);

// Stores the numeric [value] in [slot].
REWIS_API void rewisSetSlotDouble(RewisVM* vm, int slot, double value);

// Creates a new instance of the foreign class stored in [classSlot] with [size]
// bytes of raw storage and places the resulting object in [slot].
//
// This does not invoke the foreign class's constructor on the new instance. If
// you need that to happen, call the constructor from Rewis, which will then
// call the allocator foreign method. In there, call this to create the object
// and then the constructor will be invoked when the allocator returns.
//
// Returns a pointer to the foreign object's data.
REWIS_API void* rewisSetSlotNewForeign(RewisVM* vm, int slot, int classSlot, size_t size);

// Stores a new empty list in [slot].
REWIS_API void rewisSetSlotNewList(RewisVM* vm, int slot);

// Stores a new empty map in [slot].
REWIS_API void rewisSetSlotNewMap(RewisVM* vm, int slot);

// Stores null in [slot].
REWIS_API void rewisSetSlotNull(RewisVM* vm, int slot);

// Stores the string [text] in [slot].
//
// The [text] is copied to a new string within Rewis's heap, so you can free
// memory used by it after this is called. The length is calculated using
// [strlen()]. If the string may contain any null bytes in the middle, then you
// should use [rewisSetSlotBytes()] instead.
REWIS_API void rewisSetSlotString(RewisVM* vm, int slot, const char* text);

// Stores the value captured in [handle] in [slot].
//
// This does not release the handle for the value.
REWIS_API void rewisSetSlotHandle(RewisVM* vm, int slot, RewisHandle* handle);

// Returns the number of elements in the list stored in [slot].
REWIS_API int rewisGetListCount(RewisVM* vm, int slot);

// Reads element [index] from the list in [listSlot] and stores it in
// [elementSlot].
REWIS_API void rewisGetListElement(RewisVM* vm, int listSlot, int index, int elementSlot);

// Sets the value stored at [index] in the list at [listSlot], 
// to the value from [elementSlot]. 
REWIS_API void rewisSetListElement(RewisVM* vm, int listSlot, int index, int elementSlot);

// Takes the value stored at [elementSlot] and inserts it into the list stored
// at [listSlot] at [index].
//
// As in Rewis, negative indexes can be used to insert from the end. To append
// an element, use `-1` for the index.
REWIS_API void rewisInsertInList(RewisVM* vm, int listSlot, int index, int elementSlot);

// Returns the number of entries in the map stored in [slot].
REWIS_API int rewisGetMapCount(RewisVM* vm, int slot);

// Returns true if the key in [keySlot] is found in the map placed in [mapSlot].
REWIS_API bool rewisGetMapContainsKey(RewisVM* vm, int mapSlot, int keySlot);

// Retrieves a value with the key in [keySlot] from the map in [mapSlot] and
// stores it in [valueSlot].
REWIS_API void rewisGetMapValue(RewisVM* vm, int mapSlot, int keySlot, int valueSlot);

// Takes the value stored at [valueSlot] and inserts it into the map stored
// at [mapSlot] with key [keySlot].
REWIS_API void rewisSetMapValue(RewisVM* vm, int mapSlot, int keySlot, int valueSlot);

// Removes a value from the map in [mapSlot], with the key from [keySlot],
// and place it in [removedValueSlot]. If not found, [removedValueSlot] is
// set to null, the same behaviour as the Rewis Map API.
REWIS_API void rewisRemoveMapValue(RewisVM* vm, int mapSlot, int keySlot,
                        int removedValueSlot);

// Looks up the top level variable with [name] in resolved [module] and stores
// it in [slot].
REWIS_API void rewisGetVariable(RewisVM* vm, const char* module, const char* name,
                     int slot);

// Looks up the top level variable with [name] in resolved [module], 
// returns false if not found. The module must be imported at the time, 
// use rewisHasModule to ensure that before calling.
REWIS_API bool rewisHasVariable(RewisVM* vm, const char* module, const char* name);

// Returns true if [module] has been imported/resolved before, false if not.
REWIS_API bool rewisHasModule(RewisVM* vm, const char* module);

// Sets the current fiber to be aborted, and uses the value in [slot] as the
// runtime error object.
REWIS_API void rewisAbortFiber(RewisVM* vm, int slot);

// Returns the user data associated with the RewisVM.
REWIS_API void* rewisGetUserData(RewisVM* vm);

// Sets user data associated with the RewisVM.
REWIS_API void rewisSetUserData(RewisVM* vm, void* userData);

#endif
