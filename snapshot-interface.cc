#include "snapshot-interface.h"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sstream>
#include <cstring>
#include <cassert>

#define MYBINARYNAME "model"
#define MYLIBRARYNAME "libmodel.so"
#define PROCNAME      "/proc/*/maps"
#define REPLACEPOS		6
#define PAGESIZE 4096

snapshotStack * snapshotObject;

/*This looks like it might leak memory...  Subramanian should fix this. */

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
static void takeSegmentSnapshot( const MyString & lineText ){
	std::vector< MyString, MyAlloc< MyString > > firstSplit = splitString( lineText, ' ' );
	if( checkPermissions( firstSplit[ 1 ] ) ){
		std::vector< MyString, MyAlloc< MyString > > secondSplit = splitString( firstSplit[ 0 ], '-' );    
		size_t val1 = 0, val2 = 0;
		sscanf( secondSplit[ 0 ].c_str(), "%zx", &val1 );
		sscanf( secondSplit[ 1 ].c_str(), "%zx", &val2 );
		size_t len = ( val2 - val1 ) / PAGESIZE;    
		if( 0 != len ){
			addMemoryRegionToSnapShot( ( void * )val1, len );        
		}
	}
}
void SnapshotGlobalSegments(){
	MyString fn = PROCNAME;
	static char sProcessSize[ 12 ] = { 0 };
	std::pair< const char *, bool > dataSect[ 2 ];
	dataSect[ 0 ] = std::make_pair( MYBINARYNAME, false );
	dataSect[ 1 ] = std::make_pair( MYLIBRARYNAME, false );
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
			if( !procName.good() )return;
			getline( procName, line );
			takeSegmentSnapshot( line );    
		}	
	}
}

//class definition of snapshotStack.....
//declaration of constructor....
snapshotStack::snapshotStack(){
	SnapshotGlobalSegments();
	stack=NULL;
}
	
snapshotStack::~snapshotStack(){
}
	
int snapshotStack::backTrackBeforeStep(int seqindex) {
	while(true) {
		if (stack->index<=seqindex) {
			//have right entry
			rollBack(stack->snapshotid);
			return stack->index;
		}
		struct stackEntry *tmp=stack;
		free(tmp);
		stack=stack->next;
	}
}

void snapshotStack::snapshotStep(int seqindex) {
	struct stackEntry *tmp=(struct stackEntry *)malloc(sizeof(struct stackEntry));
	tmp->next=stack;
	tmp->index=seqindex;
	tmp->snapshotid=takeSnapshot();
	stack=tmp;
}
