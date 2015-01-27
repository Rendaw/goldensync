#ifndef types_h
#define types_h

#include <stdint.h>

#include "../ren-cxx-basics/stricttype.h"
#include "../ren-cxx-basics/variant.h"

typedef uint32_t InstanceIndexT;
typedef uint32_t InstanceUniqueT;
typedef uint64_t IDBaseT;
typedef StrictType(IDBaseT) NodeIndexT;
typedef StrictType(IDBaseT) ChangeIndexT;
typedef StrictType(IDBaseT) StorageIndexT;
typedef uint64_t TimeT; // UTC seconds since Jan 1 1970

typedef StorageIndexT StorageIDT;

#endif

