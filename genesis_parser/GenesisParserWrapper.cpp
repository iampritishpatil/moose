/**********************************************************************
** This program is part of 'MOOSE', the
** Messaging Object Oriented Simulation Environment,
** also known as GENESIS 3 base code.
**           copyright (C) 2003 Upinder S. Bhalla and NCBS
** It is made available under the terms of the
** GNU General Public License version 2
** See the file COPYING.LIB for the full notice.
**********************************************************************/

#include "moose.h"
#include <math.h>

#include <setjmp.h>
#include <FlexLexer.h>
#include "script.h"

#include "../shell/Shell.h"
#include "GenesisParser.h"
#include "GenesisParserWrapper.h"

const Cinfo* initGenesisParserCinfo()
{
	/**
	 * This is a shared message to talk to the Shell.
	 */
	static TypeFuncPair parserTypes[] =
	{
		// Setting cwe
		TypeFuncPair( Ftype1< unsigned int >::global(), 0 ),
		// Getting cwe back: First trigger a request
		TypeFuncPair( Ftype0::global(), 0 ),
		// Then receive the cwe info
		TypeFuncPair( Ftype1< unsigned int >::global(),
					RFCAST( &GenesisParserWrapper::recvCwe ) ),

		// Getting a list of child ids: First send a request with
		// the requested parent elm id.
		TypeFuncPair( Ftype1< unsigned int >::global(), 0 ),
		// Then recv the vector of child ids.
		TypeFuncPair( Ftype1< vector< unsigned int > >::global(), 
					RFCAST( &GenesisParserWrapper::recvLe ) ),
		
		// Creating an object: Send out the request.
		TypeFuncPair( 
				Ftype3< string, string, unsigned int >::global(), 0 ),
		// Creating an object: Recv the returned object id.
		TypeFuncPair( Ftype1< unsigned int >::global(),
					RFCAST( &GenesisParserWrapper::recvCreate ) ),
		// Deleting an object: Send out the request.
		TypeFuncPair( Ftype1< unsigned int >::global(), 0 ),

		// Getting a field value as a string: send out request:
		TypeFuncPair( 
				Ftype2< unsigned int, string >::global(), 0 ),
		// Getting a field value as a string: Recv the value.
		TypeFuncPair( Ftype1< string >::global(),
					RFCAST( &GenesisParserWrapper::recvField ) ),
		// Setting a field value as a string: send out request:
		TypeFuncPair( // object, field, value 
				Ftype3< unsigned int, string, string >::global(), 0 ),

		// Setting values for a clock tick: setClock
		TypeFuncPair( // clockNo, dt, stage
				Ftype3< int, double, int >::global(), 0 ),
		// Assigning path and function to a clock tick: useClock
		TypeFuncPair( // tick id, path, function
				Ftype3< unsigned int, vector< unsigned int >, string >::global(), 0 ),

		// Getting a wildcard path of elements: send out request
		// args are path, flag true for breadth-first list.
		TypeFuncPair( Ftype2< string, bool >::global(), 0 ),
		// Getting a wildcard path of elements: Recv the list.
		TypeFuncPair( Ftype1< vector< unsigned int > >::global(),
					RFCAST( &GenesisParserWrapper::recvWildcardList ) ),
	};
	
	static Finfo* genesisParserFinfos[] =
	{
			/*
		new ValueFinfo( "unitTest", ValueFtype1< bool >::global(),
			reinterpret_cast< GetFunc >( &Compartment::doUnitTest ),
			RFCAST( dummyFunc )
		),
		*/
		new SharedFinfo( "parser", parserTypes,
				sizeof( parserTypes ) / sizeof( TypeFuncPair ) ),
		new DestFinfo( "readline",
			Ftype1< string >::global(),
			RFCAST( &GenesisParserWrapper::readlineFunc ) ),
		new DestFinfo( "process",
			Ftype0::global(),
			RFCAST( &GenesisParserWrapper::processFunc ) ), 
		new DestFinfo( "parse",
			Ftype1< string >::global(),
			RFCAST( &GenesisParserWrapper::parseFunc ) ), 
		new SrcFinfo( "echo", Ftype1< string>::global() ),

		// This one is to exchange info with the shell.
		// We'll fill it in later
		/*
		new SharedFinfo( "shell", shellTypes,
				sizeof( shellTypes ) / sizeof( TypeFuncPair ) ),
				*/
		
		/*
		new SingleSrc2Finfo< int, const char** >( "commandOut",
			&GenesisParserWrapper::getCommandSrc, ""),
			*/
	};

	static Cinfo genesisParserCinfo(
		"GenesisParser",
		"Upinder S. Bhalla, NCBS, 2004-2007",
		"Object to handle the old Genesis parser",
		initNeutralCinfo(),
		genesisParserFinfos,
		sizeof(genesisParserFinfos) / sizeof( Finfo* ),
		ValueFtype1< GenesisParserWrapper >::global()
	);

	return &genesisParserCinfo;
}

static const Cinfo* genesisParserCinfo = initGenesisParserCinfo();
static const unsigned int setCweSlot = 
	initGenesisParserCinfo()->getSlotIndex( "parser" ) + 0;
static const unsigned int requestCweSlot = 
	initGenesisParserCinfo()->getSlotIndex( "parser" ) + 1;
static const unsigned int requestLeSlot = 
	initGenesisParserCinfo()->getSlotIndex( "parser" ) + 2;
static const unsigned int createSlot = 
	initGenesisParserCinfo()->getSlotIndex( "parser" ) + 3;
static const unsigned int deleteSlot = 
	initGenesisParserCinfo()->getSlotIndex( "parser" ) + 4;
static const unsigned int requestFieldSlot = 
	initGenesisParserCinfo()->getSlotIndex( "parser" ) + 5;
static const unsigned int setFieldSlot = 
	initGenesisParserCinfo()->getSlotIndex( "parser" ) + 6;
static const unsigned int setClockSlot = 
	initGenesisParserCinfo()->getSlotIndex( "parser" ) + 7;
static const unsigned int useClockSlot = 
	initGenesisParserCinfo()->getSlotIndex( "parser" ) + 8;
static const unsigned int requestWildcardListSlot = 
	initGenesisParserCinfo()->getSlotIndex( "parser" ) + 9;

//////////////////////////////////////////////////////////////////
// Now we have the GenesisParserWrapper functions
//////////////////////////////////////////////////////////////////

/**
 * This initialization also adds the id of the forthcoming element
 * that the GenesisParserWrapper is going into. Note that the 
 * GenesisParserWrapper is made just before the Element is, so the
 * index is used directly. Note also that we assume that no funny
 * threading happens here.
 */
GenesisParserWrapper::GenesisParserWrapper()
		: myFlexLexer( Element::numElements() )
{
		loadBuiltinCommands();
}

void GenesisParserWrapper::readlineFunc( const Conn& c, string s )
{
	GenesisParserWrapper* data =
	static_cast< GenesisParserWrapper* >( c.targetElement()->data() );

	data->AddInput( s );
}

void GenesisParserWrapper::processFunc( const Conn& c )
{
	GenesisParserWrapper* data =
	static_cast< GenesisParserWrapper* >( c.targetElement()->data() );

	data->Process();
}

void GenesisParserWrapper::parseFunc( const Conn& c, string s )
{
	GenesisParserWrapper* data =
	static_cast< GenesisParserWrapper* >( c.targetElement()->data() );

	data->ParseInput( s );
}

void GenesisParserWrapper::setReturnId( const Conn& c, unsigned int id )
{
	GenesisParserWrapper* data =
	static_cast< GenesisParserWrapper* >( c.targetElement()->data() );

	data->returnId_ = id;
}


//////////////////////////////////////////////////////////////////
// GenesisParserWrapper utilities
//////////////////////////////////////////////////////////////////

char* copyString( const string& s )
{
	char* ret = ( char* ) calloc ( s.length() + 1, sizeof( char ) );
	strcpy( ret, s.c_str() );
	return ret;
}

void GenesisParserWrapper::print( const string& s, bool noNewLine )
{
	if ( testFlag_ ) {
		printbuf_ = printbuf_ + s + " ";
	} else {
		cout << s;
		if ( !noNewLine )
				cout << endl;
	}
}

//////////////////////////////////////////////////////////////////
// GenesisParserWrapper Message recv functions
//////////////////////////////////////////////////////////////////

void GenesisParserWrapper::recvCwe( const Conn& c, Id cwe )
{
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( c.targetElement()->data() );
	gpw->cwe_ = cwe;
}

void GenesisParserWrapper::recvLe( const Conn& c, vector< Id > elist )
{
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( c.targetElement()->data() );
	gpw->elist_ = elist;
}

void GenesisParserWrapper::recvCreate( const Conn& c, Id e )
{
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( c.targetElement()->data() );
	gpw->createdElm_ = e;
}

void GenesisParserWrapper::recvField( const Conn& c, string value )
{
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( c.targetElement()->data() );
	gpw->fieldValue_ = value;
}

void GenesisParserWrapper::recvWildcardList(
	const Conn& c, vector< unsigned int > value )
{
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( c.targetElement()->data() );
	gpw->elist_ = value;
}

//////////////////////////////////////////////////////////////////
// GenesisParserWrapper Builtin commands
//////////////////////////////////////////////////////////////////

map< string, string >& sliSrcLookup()
{
	static map< string, string > src;

	if ( src.size() > 0 )
		return src;
	
	src[ "REAC A B" ] = "sub";	// for reactions
	src[ "SUBSTRATE n" ] = "";
	src[ "SUBSTRATE n vol" ] = "reac"; // For concchans.
	src[ "PRODUCT n vol" ] = "reac"; // For concchans.
	src[ "NUMCHAN n" ] = "nOut"; // From molecules to concchans.
	src[ "REAC B A" ] = "prd";
	src[ "PRODUCT n" ] = "";

	src[ "REAC sA B" ] = "sub";	// for enzymes
	src[ "SUBSTRATE n" ] = "";
	src[ "REAC eA B" ] = "";	// Target is molecule. Ignore as it
								// only applies to explicit enz.
	src[ "ENZYME n" ] = "reac"; // target is an enzyme. Use it.
	src[ "PRODUCT n" ] = "";
	src[ "MM_PRD pA" ] = "prdOut";

	src[ "SUMTOTAL n nInit" ] = "nOut";	// for molecules
	src[ "SUMTOTAL output output" ] = "out";	// for tables
	src[ "SLAVE output" ] = "out";	// for tables
	src[ "INTRAMOL n" ] = "nOut"; 	// target is an enzyme.
	src[ "CONSERVE n nInit" ] = ""; 	// Deprecated
	src[ "CONSERVE nComplex nComplexInit" ] = ""; 	// Deprecated

	return src;
}

map< string, string >& sliDestLookup()
{
	static map< string, string > dest;

	if ( dest.size() > 0 )
		return dest;

	dest[ "SUBSTRATE n vol" ] = "influx"; // For channels.
	dest[ "PRODUCT n vol" ] = "efflux";
	dest[ "NUMCHAN n" ] = "nIn"; // From molecules to concchans.
	
	dest[ "REAC A B" ] = "reac";	// for reactions
	dest[ "SUBSTRATE n" ] = "";
	dest[ "REAC B A" ] = "reac";
	dest[ "PRODUCT n" ] = "";

	dest[ "REAC sA B" ] = "reac";	// for enzymes
	dest[ "SUBSTRATE n" ] = "";
	dest[ "REAC eA B" ] = "";		// Target is enzyme, but only used
									// for explicit enzymes. Ignore.
	dest[ "ENZYME n" ] = "enz"; 	// Used both for explicit and MM.
	dest[ "PRODUCT n" ] = "";
	dest[ "MM_PRD pA" ] = "prdIn";

	dest[ "SUMTOTAL n nInit" ] = "sumTotalIn";	// for molecules
	dest[ "SUMTOTAL output output" ] = "sumTotalIn";	// for molecules
	dest[ "SLAVE output" ] = "sumTotalIn";	// for molecules
	dest[ "INTRAMOL n" ] = "intramolIn"; 	// target is an enzyme.
	dest[ "CONSERVE n nInit" ] = ""; 	// Deprecated
	dest[ "CONSERVE nComplex nComplexInit" ] = ""; 	// Deprecated

	return dest;
}

map< string, string >& sliClassNameConvert()
{
	static map< string, string > classnames;

	if ( classnames.size() > 0 )
		return classnames;
	
	classnames[ "neutral" ] = "Neutral";
	classnames[ "pool" ] = "Molecule";
	classnames[ "kpool" ] = "Molecule";
	classnames[ "reac" ] = "Reaction";
	classnames[ "kreac" ] = "Reaction";
	classnames[ "enz" ] = "Enzyme";
	classnames[ "kenz" ] = "Enzyme";
	classnames[ "kchan" ] = "ConcChan";
	classnames[ "conc_chan" ] = "ConcChan";
	classnames[ "Ca_concen" ] = "CaConc";
	classnames[ "compartment" ] = "Compartment";
	classnames[ "hh_channel" ] = "HHChannel";
	classnames[ "tabchannel" ] = "HHChannel";
	classnames[ "vdep_channel" ] = "HHChannel";
	classnames[ "vdep_gate" ] = "HHGate";
	classnames[ "xbutton" ] = "Sli";
	classnames[ "xdialog" ] = "Sli";
	classnames[ "xlabel" ] = "Sli";
	classnames[ "xform" ] = "Sli";
	classnames[ "xtoggle" ] = "Sli";
	classnames[ "xshape" ] = "Sli";
	classnames[ "xgraph" ] = "Sli";
	classnames[ "x1dialog" ] = "Sli";
	classnames[ "x1button" ] = "Sli";
	classnames[ "x1shape" ] = "Sli";
	classnames[ "xtext" ] = "Sli";

	return classnames;
}

/*
map< string, string >& sliFieldNameConvert()
{
	static map< string, string > fieldnames;

	if ( fieldnames.size() > 0 )
		return fieldnames;

	return fieldnames;
}
*/


string sliMessage(
	const string& msgType, map< string, string >& converter )
{
	map< string, string >::iterator i;
	i = converter.find( msgType );
	if ( i != converter.end() ) {
		if ( i->second.length() == 0 ) // A redundant message 
			return "";
		else 
			return i->second; // good message.
	} else 
		cout << "Error:sliMessage: Unknown message " <<
			msgType << "\n";
	return "";
}

void do_add( int argc, const char** const argv, Id s )
{
	Element* e = Element::element( s );
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( e->data() );
	return gpw->doAdd( argc, argv, s );
}

void GenesisParserWrapper::innerAdd(
	Id src, const string& srcF, Id dest, const string& destF )
{
	if ( src != Shell::BAD_ID && dest != Shell::BAD_ID ) {
		Element* se = Element::element( src );
		Element* de = Element::element( dest );

		if ( 
			!se->findFinfo( srcF )->add( se, de, de->findFinfo( destF ) )
		   )
				cout << "Error:innerAdd: Failed to add Message\n";
	}
}

void GenesisParserWrapper::doAdd(
				int argc, const char** const argv, Id s )
{
	if ( argc == 3 ) {
		string srcE = Shell::head( argv[1], "/" );
		string srcF = Shell::tail( argv[1], "/" );
		string destE = Shell::head( argv[2], "/" );
		string destF = Shell::tail( argv[2], "/" );
		Id src = path2eid( srcE, s );
		Id dest = path2eid( destE, s );

		// Should ideally send this off to the shell.
		innerAdd( src, srcF, dest, destF );
	 	cout << "in do_add " << argv[1] << ", " << argv[2] << endl;
	} else if ( argc > 3 ) { 
	// Old-fashioned addmsg. Backward Compatibility conversions here.
	// usage: addmsg source-element dest-delement msg-type [msg-fields]
	// Most of these are handled using the info in the msg-type and
	// msg fields. Often there are redundant messages which are now
	// handled by shared messages. The redundant one is ignored.
		string msgType = argv[3];

		for ( int i = 4; i < argc; i++ )
			msgType = msgType + " " + argv[ i ];

		string srcF = sliMessage( msgType, sliSrcLookup() );
		string destF = sliMessage( msgType, sliDestLookup() );

		if ( srcF.length() > 0 && destF.length() > 0 ) {
			Id src = path2eid( argv[1], s );
			Id dest = path2eid( argv[2], s );
	 		cout << "in do_add " << src << ", " << dest << endl;
			innerAdd( src, srcF, dest, destF );
		}
		//	s->addFuncLocal( src, dest );
	} else {
		cout << "usage:: " << argv[0] << " src dest\n";
		cout << "deprecated usage:: " << argv[0] << " source-element dest-element msg-type [msg-fields]";
	}
}

void do_drop( int argc, const char** const argv, Id s )
{
	if ( argc == 3 ) {
		// s->dropFuncLocal( argv[1], argv[2] );
		cout << "In do_drop " << argv[1] << ", " << argv[2] << endl;
	} else {
		cout << "usage:: " << argv[0] << " src dest\n";
	}
}

/**
 * setfield [obj] field value [field value] ...
 */
void do_set( int argc, const char** const argv, Id s )
{
	Element* e = Element::element( s );
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( e->data() );
	return gpw->doSet( argc, argv, s );
}

void GenesisParserWrapper::doSet( int argc, const char** argv, Id s )
{
	Id e;
	int start = 2;
	if ( argc < 3 ) {
		cout << argv[0] << ": Too few command arguments\n";
		cout << "usage:: " << argv[0] << " [path] field value ...\n";
		return;
	}
	if ( argc % 2 == 1 ) { // 'path' is left out, use current object.
		send0( Element::element( s ), requestCweSlot );
		e = cwe_;
		start = 1;
	} else  {
		e = GenesisParserWrapper::path2eid( argv[1], s );
		if ( e == Shell::BAD_ID )
			return;
		start = 2;
	}

	// Hack here to deal with the special case of filling tables
	// in tabchannels. Example command looks like:
	// 		setfield Ca Y_A->table[{i}] {y}
	// so here we need to do setfield Ca/Y/A->table[{i}] {y}
	for ( int i = start; i < argc; i += 2 ) {
		// s->setFuncLocal( path + "/" + argv[ i ], argv[ i + 1 ] );
		string field = argv[i];
		string value = argv[ i+1 ];
		// cout << "in do_set " << path << "." << field << " " <<
				// value << endl;
		send3< Id, string, string >( Element::element( s ),
			setFieldSlot, e, field, value );
	}
}

void do_call( int argc, const char** const argv, Id s )
{
	if ( argc < 3 ) {
		cout << "usage:: " << argv[0] << " path field/Action [args...]\n";
		return;
	}
	// Ugly hack to avoid LOAD call for notes on kkit dumpfiles
	if ( strcmp ( argv[2], "LOAD" ) == 0 )
		return;
	
	// Ugly hack to handle the TABCREATE calls, which do not go through
	// the normal message destination route.
	if ( strcmp ( argv[2], "TABCREATE" ) == 0 ) {
		// s->tabCreateFunc( argc, argv );
		cout << "in do_call TABCREATE\n";
		return;
	}

	// Ugly hack to handle the TABFILL call, which need to be redirected
	// to the two interpols of the HHGates.
	// Deprecated.
	if ( strcmp ( argv[2], "TABFILL" ) == 0 ) {
		// s->tabFillFunc( argc, argv );
		cout << "in do_call TABFILL\n";
		return;
	}
	string field;
	string value = "";
	field = string( argv[ 1 ] ) + "/" + argv[ 2 ];
	for ( int i = 3; i < argc; i++ ) {
		if ( i > 3 )
			value = value + ",";
		value = value + argv[ i ];
	}
	// s->setFuncLocal( field, value );
	cout << "in do_call " << field << ", " << value << endl;
}

int do_isa( int argc, const char** const argv, Id s )
{
	if ( argc == 3 ) {
		cout << "in do_isa " << argv[1] << ", " << argv[2] << endl;
		// return s->isaFuncLocal( argv[1], argv[2] );
	} else {
		cout << "usage:: " << argv[0] << " type field\n";
	}
	return 0;
}

int do_exists( int argc, const char** const argv, Id s )
{
	if ( argc == 2 ) {
		string temp = argv[1];
		temp = temp + "/name";
		// return s->existsFuncLocal( temp );
		cout << "in do_exists " << temp << endl;
	} else if ( argc == 3 ) {
		string temp = argv[1];
		temp = temp + "/" + argv[2];
		// return s->existsFuncLocal( temp );
		cout << "in do_exists " << temp << endl;
	} else {
		cout << "usage:: " << argv[0] << " element [field]\n";
	}
	return 0;
}

/**
 * getfield [obj] field
 */
char* do_get( int argc, const char** const argv, Id s )
{
	Element* e = Element::element( s );
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( e->data() );
	return gpw->doGet( argc, argv, s );
}

char* GenesisParserWrapper::doGet( int argc, const char** argv, Id s )
{
	string field;
	string value;
	Id e;
	if ( argc == 3 ) {
		e = GenesisParserWrapper::path2eid( argv[1], s );
		if ( e == Shell::BAD_ID )
			return copyString( "" );
		field = argv[2];
	} else if ( argc == 2 ) {
		send0( Element::element( s ), requestCweSlot );
		e = cwe_;
		field = argv[ 1 ];
	} else {
		cout << "usage:: " << argv[0] << " [element] field\n";
		return copyString( "" );
	}
	fieldValue_ = "";
	send2< Id, string >( Element::element( s ),
		requestFieldSlot, e, field );
	if ( fieldValue_.length() == 0 ) // Nothing came back
		return 0;
	return copyString( fieldValue_.c_str() );
}

char* do_getmsg( int argc, const char** const argv, Id s )
{
	static string space = " ";
	if ( argc < 3 ) {
		cout << "usage:: " << argv[0] << " element -incoming -outgoing -slot msg-number slot-number -count -type msg-number -destination msg-number -source msg-number -find srcelem type\n";
		return "";
	}
	string field = argv[ 1];
	string options = argv[ 2 ];
	for ( int i = 3; i < argc; i++ )
		options += space + argv[ i ];

	// return copyString( s->getmsgFuncLocal( field, options ) );
	return copyString( "" );
}

void do_create( int argc, const char** const argv, Id s )
{
	if ( argc != 3 ) {
		cout << "usage:: " << argv[0] << " class name\n";
		return;
	}
	string className = argv[1];
	if ( !Cinfo::find( className ) )  {
		// Possibly it is aliased for backward compatibility.
		map< string, string >::iterator i = 
			sliClassNameConvert().find( argv[1] );
		if ( i != sliClassNameConvert().end() ) {
			className = i->second;
			if ( className == "Sli" ) {
				// We bail out of these classes as MOOSE does not
				// yet handle them.
				cout << "Do not know how to handle class: " << 
						className << endl;
				return;
			}
		} else {
			cout << "GenesisParserWrapper::do_create: Do not know class: " << className << endl;
			return;
		}
	}

	string name = Shell::tail( argv[2], "/" );
	if ( name.length() < 1 ) {
		cout << "Error: invalid object name : " << name << endl;
		return;
	}
	string parent = Shell::head( argv[2], "/" );
	Id pa = GenesisParserWrapper::path2eid( parent, s );

	send3< string, string, unsigned int >( Element::element( s ),
		createSlot, className, name, pa );

		// The return function recvCreate gets the id of the
		// returned elm, but
		// the GenesisParser does not care.
}

void do_delete( int argc, const char** const argv, Id s )
{
	if ( argc == 2 ) {
		Id victim = GenesisParserWrapper::path2eid( argv[1], s );
		if ( victim != 0 )
			send1< Id >( Element::element( s ), deleteSlot, victim );
	} else {
		cout << "usage:: " << argv[0] << " Element/path\n";
	}
}

void do_move( int argc, const char** const argv, Id s )
{
	if ( argc == 3 ) {
		; // s->moveFuncLocal( argv[1], argv[2] );
	} else {
		cout << "usage:: " << argv[0] << " src dest\n";
	}
}

void do_copy( int argc, const char** const argv, Id s )
{
	if ( argc == 3 ) {
		// s->copyFuncLocal( argv[1], argv[2] );
	} else {
		cout << "usage:: " << argv[0] << " src dest\n";
	}
}

void do_copy_shallow( int argc, const char** const argv, Id s )
{
	if ( argc == 3 ) {
		// s->copyShallowFuncLocal( argv[1], argv[2] );
	} else {
		cout << "usage:: " << argv[0] << " src dest\n";
	}
}

void do_copy_halo( int argc, const char** const argv, Id s )
{
	if ( argc == 3 ) {
		// s->copyHaloFuncLocal( argv[1], argv[2] );
	} else {
		cout << "usage:: " << argv[0] << " src dest\n";
	}
}

void do_ce( int argc, const char** const argv, Id s )
{
	if ( argc == 2 ) {
		Id e = GenesisParserWrapper::path2eid( argv[1], s );
		send1< Id >( Element::element( s ), setCweSlot, e );
	} else {
		cout << "usage:: " << argv[0] << " Element\n";
	}
}

void do_pushe( int argc, const char** const argv, Id s )
{
	if ( argc == 2 ) {
		// s->pusheFuncLocal( argv[1] );
	} else {
		cout << "usage:: " << argv[0] << " Element\n";
	}
}

void do_pope( int argc, const char** const argv, Id s )
{
	if ( argc == 1 ) {
		// s->popeFuncLocal( );
	} else {
		cout << "usage:: " << argv[0] << "\n";
	}
}

void do_alias( int argc, const char** const argv, Id s )
{
	string alias = "";
	string old = "";
	if ( argc == 3 ) {
		alias = argv[1];
		old = argv[2];
	} else if ( argc == 2 ) {
		alias = argv[1];
	}
	Element* e = Element::element( s );
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( e->data() );
	gpw->alias( alias, old );
}

void do_quit( int argc, const char** const argv, Id s )
{
	// s->quitFuncLocal( );
		exit( 0 );
}

void do_stop( int argc, const char** const argv, Id s )
{
	if ( argc == 1 ) {
		// s->stopFuncLocal( );
		;
	} else {
		cout << "usage:: " << argv[0] << "\n";
	}
}

void do_reset( int argc, const char** const argv, Id s )
{
	if ( argc == 1 ) {
		;
		// s->resetFuncLocal( );
	} else {
		cout << "usage:: " << argv[0] << "\n";
	}
}

void do_step( int argc, const char** const argv, Id s )
{
	if ( argc == 2 ) {
			;
		// s->stepFuncLocal( argv[1], "" );
	} else if ( argc == 3 ) {
			;
		// s->stepFuncLocal( argv[1], argv[2] );
	} else {
		cout << "usage:: " << argv[0] << " time/nsteps [-t -s(default ]\n";
	}
}

void do_setclock( int argc, const char** const argv, Id s )
{
	if ( argc == 3 ) {
			send3< int, double, int >( Element::element( s ),
				setClockSlot, 
				atoi( argv[1] ), atof( argv[2] ), 0 );
	} else if ( argc == 4 ) {
			send3< int, double, int >( Element::element( s ),
				setClockSlot, 
				atoi( argv[1] ), atof( argv[2] ), atoi( argv[3] ) );
	} else {
		cout << "usage:: " << argv[0] << " clockNum dt [stage]\n";
	}
}

void do_showclocks( int argc, const char** const argv, Id s )
{
	if ( argc == 1 ) {
	;	// s->showClocksFuncLocal( );
	} else {
		cout << "usage:: " << argv[0] << "\n";
	}
}

void do_useclock( int argc, const char** const argv, Id s )
{
	char tickName[40];
	string func = "process";
	if ( argc == 3 ) {
		sprintf( tickName, "/sched/cj/t%s", argv[2] );
	} else if ( argc == 4 ) {
		sprintf( tickName, "/sched/cj/t%s", argv[3] );
		func = argv[2];
	} else {
		cout << "usage:: " << argv[0] << " path [funcname] clockNum\n";
		return;
	}

	Id tickId = GenesisParserWrapper::path2eid( tickName, s );
	if ( tickId == Shell::BAD_ID ) {
		cout << "Error:" << argv[0] << ": Invalid clockNumber " <<
				tickName << "\n";
		return;
	}

	string path = argv[1];
	Element* e = Element::element( s );
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( e->data() );
	gpw->useClock( tickId, path, func, s );
}

void GenesisParserWrapper::useClock(
	Id tickId, const string& path, const string& func, Id s )
{
	Element* e = Element::element( s );

	// Here we use the default form which takes comma-separated lists
	// but may scramble the order.
	// This request elicits a return message to put the list in the
	// elist_ field.

	send2< string, bool >( e, requestWildcardListSlot, path, 0 );

	send3< unsigned int, vector< unsigned int >, string >(
		Element::element( s ),
		useClockSlot, 
		tickId, elist_, func );
}

void do_show( int argc, const char** const argv, Id s )
{
	Element* e = Element::element( s );
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( e->data() );
	gpw->doShow( argc, argv, s );
}

void GenesisParserWrapper::showAllFields( Id e, Id s )
{
	char temp[80];
	
	// Ask for the list of fields as one big string
	send2< Id, string >( Element::element( s ),
		requestFieldSlot, e, "fieldList" );
	vector< string > list;
	vector< string >::iterator i;
	separateString( fieldValue_, list, ", " );
	for ( i = list.begin(); i != list.end(); i++ ) {
		if ( *i == "fieldList" )
			continue;
		fieldValue_ = "";
		send2< Id, string >( Element::element( s ),
			requestFieldSlot, e, *i );
		if ( fieldValue_.length() > 0 ) {
			sprintf( temp, "%-25s%s", i->c_str(), "= " );
			print( temp + fieldValue_ );
		}
	}
}

/**
 * Decide if it is a specific field, or all.
 * If specific, get the value for that specific field and print it.
 * If all, first get the list of all fields (which is a field too),
 * then get the value for each specific field in turn.
 * The first arg could be a field, or it could be the object.
 */
void GenesisParserWrapper::doShow( int argc, const char** argv, Id s )
{
	Id e;
	int firstField = 2;
	char temp[80];

	if ( argc < 2 ) {
		print( "Usage: showfield [object/wildcard] [fields] -all" );
		return;
	}


	if ( argc == 2 ) { // show fields of cwe.
		send0( Element::element( s ), requestCweSlot );
		e = cwe_;
		firstField = 1;
	} else {
		e = path2eid( argv[1], s );
		if ( e == Shell::BAD_ID ) {
			e = cwe_;
			firstField = 1;
		} else {
			firstField = 2;
		}
	}

	print( "[ " + eid2path( e ) + " ]" );

	for ( int i = firstField; i < argc; i++ ) {
		if ( strcmp( argv[i], "*") == 0 ) {
			showAllFields( e, s );
		} else { // get specific field here.
			fieldValue_ = "";
			send2< Id, string >( Element::element( s ),
				requestFieldSlot, e, argv[i] );
			if ( fieldValue_.length() > 0 ) {
				sprintf( temp, "%-25s%s", argv[i], "= " );
				print( temp + fieldValue_ );
			}
		}
	}
}

void do_le( int argc, const char** const argv, Id s )
{
	Element* e = Element::element( s );
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( e->data() );
	gpw->doLe( argc, argv, s );
}

void GenesisParserWrapper::doLe( int argc, const char** argv, Id s )
{
	if ( argc == 1 ) { // Look in the cwe first.
		send0( Element::element( s ), requestCweSlot );
		send1< Id >( Element::element( s ), requestLeSlot, cwe_ );
	} else if ( argc >= 2 ) {
		Id e = path2eid( argv[1], s );
		/// \todo: Use better test for a bad path than this.
		if ( e == Shell::BAD_ID )
			return;
		send1< Id >( Element::element( s ), requestLeSlot, e );
	}
	vector< Id >::iterator i = elist_.begin();
	// This operation should really do it in a parallel-clean way.
	/// \todo If any children, we should suffix the name with a '/'
	for ( i = elist_.begin(); i != elist_.end(); i++ )
		print( Element::element( *i )->name() );
	elist_.resize( 0 );
}

void do_pwe( int argc, const char** const argv, Id s )
{
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( Element::element( s )->data() );
	gpw->doPwe( argc, argv, s );
}

void GenesisParserWrapper::doPwe( int argc, const char** argv, Id s )
{
	send0( Element::element( s ), requestCweSlot );
	// Here we need to wait for the shell to service this message
	// request and put the requested value in the local cwe_.
	
	print( GenesisParserWrapper::eid2path( cwe_ ) );
}

void do_listcommands( int argc, const char** const argv, Id s )
{
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( Element::element( s )->data() );
	gpw->listCommands( );
}

void do_listobjects( int argc, const char** const argv, Id s )
{
	;// s->listClassesFuncLocal( );
}

void do_echo( int argc, const char** const argv, Id s )
{
	// vector< string > vec;
	int options = 0;
	if ( argc > 1 && strncmp( argv[ argc - 1 ], "-n", 2 ) == 0 ) {
		options = 1; // no newline
		argc--;
	}

	string temp = "";
	for (int i = 1; i < argc; i++ )
		temp = temp + argv[i];

	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
			( Element::element( s )->data() );
	gpw->print( temp, options );

	/*
	for (int i = 1; i < argc; i++ )
		vec.push_back( argv[i] );
		*/

	// s->echoFuncLocal( vec, options );
}

// Old GENESIS Usage: addfield [element] field-name -indirect element field -description text
// Here we'll have a subset of it:
// addfield [element] field-name -type field_type
void do_addfield( int argc, const char** const argv, Id s )
{
	if ( argc == 2 ) {
		// const char * nargv[] = { argv[0], ".", argv[1] };
//		s->commandFuncLocal( 3, nargv );
	} else if ( argc == 3 ) {
		// s->commandFuncLocal( argc, argv );
	} else if ( argc == 4 && strncmp( argv[2], "-f", 2 ) == 0 ) {
		// const char * nargv[] = { argv[0], ".", argv[1], argv[3] };
//		s->commandFuncLocal( 4, nargv );
	} else if ( argc == 5 && strncmp( argv[3], "-f", 2 ) == 0 ) {
		// const char * nargv[] = { argv[0], argv[1], argv[3], argv[4] };
//		s->commandFuncLocal( 4, nargv );
//	} else {
		; // s->error( "usage: addfield [element] field-name -type field_type" );
	}
}

void doShellCommand ( int argc, const char** const argv, Id s )
{
; //	s->commandFuncLocal( argc, argv );
}

void do_complete_loading( int argc, const char** const argv, Id s )
{
}

float do_exp( int argc, const char** const argv, Id s )
{
	if ( argc != 2 ) {
; //		s->error( "exp: Too few command arguments\nusage: exp number" );
		return 0.0;
	} else {	
		return exp( atof( argv[ 1 ] ) );
	}
}

float do_log( int argc, const char** const argv, Id s )
{
	if ( argc != 2 ) {
; //		s->error( "log: Too few command arguments\nusage: log number" );
		return 0.0;
	} else {	
		return log( atof( argv[ 1 ] ) );
	}
}

float do_log10( int argc, const char** const argv, Id s )
{
	if ( argc != 2 ) {
; // 		s->error( "log10: Too few command arguments\nusage: log10 number" );
		return 0.0;
	} else {	
		return log10( atof( argv[ 1 ] ) );
	}
}

float do_sin( int argc, const char** const argv, Id s )
{
	if ( argc != 2 ) {
		; // s->error( "sin: Too few command arguments\nusage: sin number" );
		return 0.0;
	} else {	
		return sin( atof( argv[ 1 ] ) );
	}
}

float do_cos( int argc, const char** const argv, Id s )
{
	if ( argc != 2 ) {
		// s->error( "cos: Too few command arguments\nusage: cos number" );
		return 0.0;
	} else {	
		return cos( atof( argv[ 1 ] ) );
	}
}

float do_tan( int argc, const char** const argv, Id s )
{
	if ( argc != 2 ) {
		// s->error( "tan: Too few command arguments\nusage: tan number" );
		return 0.0;
	} else {	
		return tan( atof( argv[ 1 ] ) );
	}
}

float do_sqrt( int argc, const char** const argv, Id s )
{
	if ( argc != 2 ) {
		// s->error( "sqrt: Too few command arguments\nusage: sqrt number" );
		return 0.0;
	} else {	
		return sqrt( atof( argv[ 1 ] ) );
	}
}

float do_pow( int argc, const char** const argv, Id s )
{
	if ( argc != 3 ) {
		// s->error( "pow: Too few command arguments\nusage: exp base exponent" );
		return 0.0;
	} else {	
		return pow( atof( argv[ 1 ] ), atof( argv[ 2 ] ) );
	}
}

float do_abs( int argc, const char** const argv, Id s )
{
	if ( argc != 2 ) {
		// s->error( "abs: Too few command arguments\nusage: abs number" );
		return 0.0;
	} else {	
		return fabs( atof( argv[ 1 ] ) );
	}
}

void do_xshow ( int argc, const char** const argv, Id s )
{
	// s->error( "not implemented yet." );
}

void do_xhide ( int argc, const char** const argv, Id s )
{
	// s->error( "not implemented yet." );
}

void do_xshowontop ( int argc, const char** const argv, Id s )
{
	// s->error( "not implemented yet." );
}

void do_xupdate ( int argc, const char** const argv, Id s )
{
	// s->error( "not implemented yet." );
}

void do_xcolorscale ( int argc, const char** const argv, Id s )
{
	// s->error( "not implemented yet." );
}

void do_x1setuphighlight ( int argc, const char** const argv, Id s )
{
	// s->error( "not implemented yet." );
}

void do_xsendevent ( int argc, const char** const argv, Id s )
{
	//s->error( "not implemented yet." );
}

//////////////////////////////////////////////////////////////////
// GenesisParserWrapper load command
//////////////////////////////////////////////////////////////////

void GenesisParserWrapper::loadBuiltinCommands()
{
	AddFunc( "addmsg", do_add, "void" );
	AddFunc( "deletemsg", do_drop, "void" );
	AddFunc( "setfield", do_set, "void" );
	AddFunc( "getfield", reinterpret_cast< slifunc >( do_get ), "char*" );
	AddFunc( "getmsg", reinterpret_cast< slifunc >( do_getmsg ), "char*" );
	AddFunc( "call", do_call, "void" );
	AddFunc( "isa", reinterpret_cast< slifunc >( do_isa ), "int" );
	AddFunc( "exists", reinterpret_cast< slifunc >( do_exists ), "int");
	AddFunc( "showfield", do_show, "void" );
	AddFunc( "create", do_create, "void" );
	AddFunc( "delete", do_delete, "void" );
	AddFunc( "move", do_move, "void" );
	AddFunc( "copy", do_copy, "void" );
	AddFunc( "copy_shallow", do_copy_shallow, "void" );
	AddFunc( "copy_halo", do_copy_halo, "void" );
	AddFunc( "ce", do_ce, "void" );
	AddFunc( "pushe", do_pushe, "void" );
	AddFunc( "pope", do_pope, "void" );
	AddFunc( "alias", do_alias, "void" );
	AddFunc( "quit", do_quit, "void" );
	AddFunc( "stop", do_stop, "void" );
	AddFunc( "reset", do_reset, "void" );
	AddFunc( "step", do_step, "void" );
	AddFunc( "setclock", do_setclock, "void" );
	AddFunc( "useclock", do_useclock, "void" );
	AddFunc( "showclocks", do_showclocks, "void" );
	AddFunc( "le", do_le, "void" );
	AddFunc( "pwe", do_pwe, "void" );
	AddFunc( "listcommands", do_listcommands, "void" );
	AddFunc( "listobjects", do_listobjects, "void" );
	AddFunc( "echo", do_echo, "void" );
	AddFunc( "simdump", doShellCommand, "void" );
	AddFunc( "simundump", doShellCommand, "void" );
	AddFunc( "simobjdump", doShellCommand, "void" );
	AddFunc( "loadtab", doShellCommand, "void" );
	AddFunc( "readcell", doShellCommand, "void" );
	AddFunc( "setupalpha", doShellCommand, "void" );
	AddFunc( "setuptau", doShellCommand, "void" );
	AddFunc( "tweakalpha", doShellCommand, "void" );
	AddFunc( "tweaktau", doShellCommand, "void" );
	AddFunc( "addfield", do_addfield, "void" );
	AddFunc( "complete_loading", do_complete_loading, "void" );
	AddFunc( "exp", reinterpret_cast< slifunc>( do_exp ), "float" );
	AddFunc( "log", reinterpret_cast< slifunc>( do_log ), "float" );
	AddFunc( "log0", reinterpret_cast< slifunc>( do_log10 ), "float" );
	AddFunc( "sin", reinterpret_cast< slifunc>( do_sin ), "float" );
	AddFunc( "cos", reinterpret_cast< slifunc>( do_cos ), "float" );
	AddFunc( "tan", reinterpret_cast< slifunc>( do_tan ), "float" );
	AddFunc( "sqrt", reinterpret_cast< slifunc>( do_sqrt ), "float" );
	AddFunc( "pow", reinterpret_cast< slifunc>( do_pow ), "float" );
	AddFunc( "abs", reinterpret_cast< slifunc>( do_abs ), "float" );
	AddFunc( "xshow", do_xshow, "void" );
	AddFunc( "xhide", do_xhide, "void" );
	AddFunc( "xshowontop", do_xshowontop, "void" );
	AddFunc( "xupdate", do_xupdate, "void" );
	AddFunc( "xcolorscale", do_xcolorscale, "void" );
	AddFunc( "x1setuphighlight", do_x1setuphighlight, "void" );
	AddFunc( "xsendevent", do_xsendevent, "void" );
}

//////////////////////////////////////////////////////////////////
// GenesisParserWrapper Field commands
//////////////////////////////////////////////////////////////////

/**
 * Looks up the Id of the object specified by the string s.
 * In most cases this call does not need to know about the 
 * GenesisParserWrapper element g, but if it refers to the current
 * working element of the shell then it does need g.
 */
Id GenesisParserWrapper::path2eid( const string& path, Id g )
{
	Element* e = Element::element( g );
	GenesisParserWrapper* gpw = static_cast< GenesisParserWrapper* >
		( e->data() );
	return gpw->innerPath2eid( path, g );
}

Id GenesisParserWrapper::innerPath2eid( const string& path, Id g )
{
	static string separator = "/";

	if ( path == separator || path == separator + "root" )
			return 0;

	if ( path == "" || path == "." ) {
		send0( Element::element( g ), requestCweSlot );
		return cwe_;
	}

	if ( path == ".." ) {
		send0( Element::element( g ), requestCweSlot );
		if ( cwe_ == 0 )
			return 0;
		return Shell::parent( cwe_ );
	}

	vector< string > names;

	unsigned int start;
	if ( path.substr( 0, separator.length() ) == separator ) {
		start = 0;
		separateString( path.substr( separator.length() ), names, separator );
	} else if ( path.substr( 0, 5 ) == "/root" ) {
		start = 0;
		separateString( path.substr( 5 ), names, separator );
	} else {
		send0( Element::element( g ), requestCweSlot );
		start = cwe_;
		separateString( path, names, separator );
	}
	Id ret = Shell::traversePath( start, names );
	if ( ret == Shell::BAD_ID )
			print( string( "cannot find object '" ) + path + "'" );
	return ret;
}

/*
 * Should really refer to the shell for this in case we need to do
 * node traversal.
 */
static Id parent( Id e )
{
	Element* elm = Element::element( e );
	unsigned int ret;
	
	// Check if eid is on local node, otherwise go to remote node
	if ( get< unsigned int >( elm, "parent", ret ) )
		return ret;
	return 0;
}

string GenesisParserWrapper::eid2path( unsigned int eid ) 
{
	static const string slash = "/";
	string n( "" );

	if ( eid == 0 )
		return "/";

	while ( eid != 0 ) {
		n = slash + Element::element( eid )->name() + n;
		eid = parent( eid );
	}
	return n;
}

/**
 * Looks up the shell attached to the parser specified by g.
 * Static func.
 */
Element* GenesisParserWrapper::getShell( Id g )
{
	/// \todo: shift connDestBegin function to base Element class.
	SimpleElement* e = dynamic_cast< SimpleElement *>( 
					Element::element( g ) );
	assert ( e != 0 && e != Element::root() );
	vector< Conn >::const_iterator i = e->connDestBegin( 3 );
	Element* ret = i->targetElement();
	return ret;
}

//////////////////////////////////////////////////////////////////
// Utility function for creating a GenesisParserWrapper, shell and
// connecting them all up.
//////////////////////////////////////////////////////////////////

/**
 * This function is called from main() if there is a genesis parser.
 * It passes in the initial string issued to the program, which
 * the Genesis parser treats as a file argument for loading.
 * Then the parser goes to its infinite loop using the Process call.
 */
void makeGenesisParser( const string& s )
{
	set< string, string >( Element::root(), "create", "Shell", "shell");
	Element* shell = Element::lastElement();
	set< string, string >( shell, "create", "GenesisParser", "sli");
	Element* sli = Element::lastElement();

	assert( shell->findFinfo( "parser" )->add( shell, sli, 
		sli->findFinfo( "parser" ) ) != 0 );

#ifdef DO_UNIT_TESTS
	static_cast< GenesisParserWrapper* >( sli->data() )->unitTest();
#endif

	if ( s.length() > 1 ) {
		set< string >( sli, "parse", s );		
	}
	set( sli, "process" );
}

//////////////////////////////////////////////////////////////////
// GenesisParserWrapper unit tests
//////////////////////////////////////////////////////////////////

#ifdef DO_UNIT_TESTS
void GenesisParserWrapper::gpAssert(
		const string& command, const string& ret )
{
	testFlag_ = 1;
	printbuf_ = "";
	ParseInput( command );
	assert( printbuf_ == ret );
	testFlag_ = 0;
	cout << ".";
}

void GenesisParserWrapper::unitTest()
{
	cout << "\nDoing GenesisParserWrapper tests";
	gpAssert( "le", "sched b c shell " );
	gpAssert( "create neutral /foo", "" );
	gpAssert( "le", "sched b c shell foo " );
	gpAssert( "ce /foo", "" );
	gpAssert( "le", "" );
	gpAssert( "pwe", "/foo " );
	gpAssert( "create neutral ./bar", "" );
	gpAssert( "le", "bar " );
	gpAssert( "le /foo", "bar " );
	gpAssert( "le ..", "sched b c shell foo " );
	gpAssert( "ce bar", "" );
	gpAssert( "pwe", "/foo/bar " );
	gpAssert( "ce ../..", "" );
	gpAssert( "le", "sched b c shell foo " );
	gpAssert( "delete /foo", "" );
	gpAssert( "le", "sched b c shell " );
	gpAssert( "le /foo", "cannot find object '/foo' " );
	gpAssert( "echo foo", "foo " );
	gpAssert( "echo bar -n", "bar " );
	gpAssert( "echo {2 + 3}", "5 " );
	gpAssert( "echo {sqrt { 13 - 4 }}", "3 " );
	gpAssert( "echo {sin 1.5 }", "0.997495 " );
	gpAssert( "echo {log 3 }", "1.09861 " );
	gpAssert( "create compartment /compt", "" );
	gpAssert( "echo {getfield /compt Vm}", "-0.06 " );
	gpAssert( "setfield /compt Vm 1.234", "" );
	gpAssert( "echo {getfield /compt Vm}", "1.234 " );
	gpAssert( "setfield /compt Cm 3.1415", "" );
	gpAssert( "echo {getfield /compt Cm}", "3.1415 " );
	gpAssert( "ce /compt", "" );
	gpAssert( "echo {getfield Cm}", "3.1415 " );
	gpAssert( "echo {getfield Vm}", "1.234 " );
	gpAssert( "setfield Rm 0.1", "" );
	gpAssert( "echo {getfield Rm}", "0.1 " );
	gpAssert( "ce /", "" );
	gpAssert( "showfield /compt Vm",
					"[ /compt ] Vm                       = 1.234 " );
	gpAssert( "showfield compt Em Cm Rm",
					"[ /compt ] Em                       = -0.06 Cm                       = 3.1415 Rm                       = 0.1 " );
	gpAssert( "alias shf showfield", "" );
	gpAssert( "shf /compt Em",
					"[ /compt ] Em                       = -0.06 " );
	gpAssert( "alias gf getfield", "" );
	gpAssert( "alias", "gf\tgetfield shf\tshowfield " );
	gpAssert( "alias gf", "getfield " );
	gpAssert( "le /sched/cj", "t0 " );
	gpAssert( "setclock 1 0.1", "" );
	gpAssert( "le /sched/cj", "t0 t1 " );
	gpAssert( "echo {getfield /sched/cj/t0 dt}", "1 " );
	gpAssert( "echo {getfield /sched/cj/t1 dt}", "0.1 " );
	gpAssert( "useclock /##[TYPE=Compartment] 1", "" );
	cout << "\n";
}
#endif
