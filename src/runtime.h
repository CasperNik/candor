#ifndef _SRC_RUNTIME_H_
#define _SRC_RUNTIME_H_

#include <stdint.h> // uint32_t
#include <sys/types.h> // size_t

namespace dotlang {

// Forward declarations
class Heap;

// Wrapper for heap()->new_space()->Allocate()
typedef char* (*RuntimeAllocateCallback)(Heap* heap,
                                         uint32_t bytes,
                                         char* context);
char* RuntimeAllocate(Heap* heap, uint32_t bytes, char* context);

// Performs lookup into a hashmap
// if insert=1 - inserts key into map space
typedef char* (*RuntimeLookupPropertyCallback)(Heap* heap,
                                               char* context,
                                               char* obj,
                                               char* key,
                                               off_t insert);
char* RuntimeLookupProperty(Heap* heap,
                            char* context,
                            char* obj,
                            char* key,
                            off_t insert);

typedef char* (*RuntimeGrowObjectCallback)(Heap* heap,
                                           char* context,
                                           char* obj);
char* RuntimeGrowObject(Heap* heap,
                        char* context,
                        char* obj);

// Compares two heap values
typedef size_t (*RuntimeCompareCallback)(char* lhs, char* rhs);
size_t RuntimeCompare(char* lhs, char* rhs);

} // namespace dotlang

#endif // _SRC_RUNTIME_H_