#ifndef hash_h
#define hash_h

#include "../../ren-cxx-basics/variant.h"
#include "../../ren-cxx-filesystem/path.h"

#include <array>

typedef std::array<uint8_t, 16> HashT;

std::string FormatHash(HashT const &Hash);

OptionalT<HashT> UnformatHash(char const *String);

HashT HashString(std::string const &String);
OptionalT<std::pair<HashT, size_t>> HashFile(Filesystem::PathT const &Path);

#endif
