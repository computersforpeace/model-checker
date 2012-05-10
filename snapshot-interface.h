#ifndef __SNAPINTERFACE_H
#define __SNAPINTERFACE_H
#include "snapshot.h"
#include "mymemory.h"
#include <vector>
#include <utility>
#include <string>
typedef std::basic_string< char, std::char_traits< char >, MyAlloc< char > > MyString;
namespace snapshot_utils{
	std::vector< std::pair< void *, size_t >, MyAlloc< std::pair< void *, size_t > > > ReturnGlobalSegmentsToSnapshot();
};
#endif
