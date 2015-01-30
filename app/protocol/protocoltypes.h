#ifndef protocoltypes_h
#define protocoltypes_h

#include "../../ren-cxx-basics/stricttype.h"

namespace Protocol
{
typedef StrictType(uint8_t) VersionIDT;
typedef StrictType(uint8_t) MessageIDT;
typedef StrictType(uint16_t) SizeT;
typedef StrictType(uint16_t) ArraySizeT;
}

#endif
