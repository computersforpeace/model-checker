#define MYBINARYNAME "model"
#define MYLIBRARYNAME "libmodel.so"
#define MYALLOCNAME  "libmymemory.so"
#define PROCNAME      "/proc/*/maps"
#define REPLACEPOS		6
#define PAGESIZE 4096
#include "snapshot-interface.h"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sstream>
typedef std::basic_stringstream< char, std::char_traits< char >, MyAlloc< char > > MyStringStream;
std::vector< MyString, MyAlloc< MyString> > splitString( MyString input, char delim ){
	std::vector< MyString, MyAlloc< MyString > > splits;
	MyStringStream ss( input );	
	MyString item;
	while( std::getline( ss, item, delim ) ){
		splits.push_back( item );	
	}
	return splits;
}

bool checkPermissions( MyString permStr ){
	return permStr.find("w") != MyString::npos;
}
std::vector< std::pair< void *, size_t >, MyAlloc< std::pair< void *, size_t > > > snapshot_utils::ReturnGlobalSegmentsToSnapshot(){
	std::vector< std::pair< void *, size_t >, MyAlloc< std::pair< void *, size_t > > >  theVec;
	MyString fn = PROCNAME;
	static char sProcessSize[ 12 ] = { 0 };
	std::pair< const char *, bool > dataSect[ 3 ];
	dataSect[ 0 ] = std::make_pair( MYBINARYNAME, false );
	dataSect[ 1 ] = std::make_pair( MYLIBRARYNAME, false );
	dataSect[ 2 ] = std::make_pair( MYALLOCNAME, false );
	static pid_t sProcID = 0;
	if( 0 == sProcID ) {
		sProcID = getpid();	
		sprintf( sProcessSize, "%d", sProcID );
	}
	fn.replace( REPLACEPOS, 1, sProcessSize );
	std::ifstream procName( fn.c_str() );
	if( procName.is_open() ){
		MyString line;
		while( procName.good() ){
			getline( procName, line );
			int i  = 0;
			for( i = 0; i < 3; ++i ){
				if( MyString::npos != line.find( dataSect[ i ].first ) ) break;			
			}
			if( i >= 3 || dataSect[ i ].second == true ) continue;
			dataSect[ i ].second = true;
			if( !procName.good() )return theVec;
			getline( procName, line );
			std::vector< MyString, MyAlloc< MyString > > firstSplit = splitString( line, ' ' );
			if( !checkPermissions( firstSplit[ 1 ]  ) ) continue;
			std::vector< MyString, MyAlloc< MyString > > secondSplit = splitString( firstSplit[ 0 ], '-' );
			size_t val1 = 0, val2 = 0;
			sscanf( secondSplit[ 0 ].c_str(), "%zx", &val1 );
			sscanf( secondSplit[ 1 ].c_str(), "%zx", &val2 );
			size_t len = ( val2 - val1 ) / PAGESIZE;
			theVec.push_back( std::make_pair( ( void * ) val1, len ) );
		}	
	}
	return theVec;
}
