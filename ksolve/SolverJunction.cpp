/**********************************************************************
** This program is part of 'MOOSE', the
** Messaging Object Oriented Simulation Environment.
**           Copyright (C) 2003-2012 Upinder S. Bhalla. and NCBS
** It is made available under the terms of the
** GNU Lesser General Public License version 2.1
** See the file COPYING.LIB for the full notice.
**********************************************************************/

#include "header.h"
#include "SolverJunction.h"
#include "StoichPools.h"
#include "UpFunc.h"

SrcFinfo1< vector< double > >* updateJunction()
{
	static SrcFinfo1< vector< double > > updateJunction(
		"updateJunction",
		"Sends out vector of all mol # changes to cross junction."
	);
	return &updateJunction;
}

const Cinfo* SolverJunction::initCinfo()
{
	//////////////////////////////////////////////////////////////////
	// Fields
	//////////////////////////////////////////////////////////////////
	
	static ReadOnlyValueFinfo< SolverJunction, unsigned int > numReacs(
	   	"numReacs",
		"Number of cross-compartment reactions on this Junction",
		&SolverJunction::getNumReacs
	);

	static ReadOnlyValueFinfo< SolverJunction, unsigned int > 
		numDiffMols( 
		"numDiffMols",
		"Number of molecule species diffusing across this Junction",
		&SolverJunction::getNumDiffMols
	);

	static ReadOnlyValueFinfo< SolverJunction, unsigned int > 
		numMeshEntries( 
		"numMeshEntries",
		"Number of voxels (mesh entries) handled by Junction",
		&SolverJunction::getNumMeshIndex
	);

	static ReadOnlyValueFinfo< SolverJunction, Id > otherCompartment( 
		"otherCompartment",
		"Id of compartment on other side of this Junction. "
		"Readily obtained by message traversal, just a utility field.",
		&SolverJunction::getOtherCompartment
	);
	//////////////////////////////////////////////////////////////////
	// DestFinfos
	//////////////////////////////////////////////////////////////////
	static DestFinfo handleJunction( "handleJunction",
		"Handles arriving Junction messages, by redirecting up to"
	   	" parent StoichPools object",
		new UpFunc1< StoichPools, vector< double > >( 
				&StoichPools::handleJunction ) );

	//////////////////////////////////////////////////////////////////
	// Shared Finfos
	//////////////////////////////////////////////////////////////////
	static Finfo* junctionShared[] = {
			&handleJunction,
			updateJunction()
	};

	static SharedFinfo junction( "junction",
		"Shared message between SolverJunctions to handle cross-solver "
		"reactions and diffusion.",
		junctionShared,
		sizeof( junctionShared ) / sizeof( const Finfo* )
	);

	static Finfo* synapseFinfos[] = {
		&numReacs,				// ReadOnly Fields
		&numDiffMols,			// ReadOnly Fields
		&numMeshEntries,		// ReadOnly Fields
		&otherCompartment,		// ReadOnly Fields
		&junction, 				// SharedFinfo
	};

	static Cinfo synapseCinfo (
		"SolverJunction",
		Neutral::initCinfo(),
		synapseFinfos,
		sizeof( synapseFinfos ) / sizeof ( Finfo* ),
		new Dinfo< SolverJunction >()
	);

	return &synapseCinfo;
}

static const Cinfo* synapseCinfo = SolverJunction::initCinfo();

SolverJunction::SolverJunction()
{
	;
}

SolverJunction::~SolverJunction()
{
	;
}

/////////////////////////////////////////////////////////////////////////
// Fields
/////////////////////////////////////////////////////////////////////////

unsigned int SolverJunction::getNumReacs() const
{
		return reacTerms_.size();
}

unsigned int SolverJunction::getNumDiffMols() const
{
		return diffTerms_.size();
}
unsigned int SolverJunction::getNumMeshIndex() const
{
		return meshIndex_.size();
}

Id SolverJunction::getOtherCompartment() const
{
		return Id(); // Dummy for now.
}

/////////////////////////////////////////////////////////////////////////
// Utility functions
/////////////////////////////////////////////////////////////////////////
const vector< unsigned int >& SolverJunction::reacTerms() const
{
	return reacTerms_;
}


const vector< unsigned int >& SolverJunction::diffTerms() const
{
	return diffTerms_;
}

const vector< unsigned int >& SolverJunction::meshIndex() const
{
	return meshIndex_;
}

void SolverJunction::incrementTargets( 
				vector< vector< double > >& y,
				const vector< double >& v ) const
{
	typedef vector< pair< unsigned int, unsigned int> >::const_iterator VPI;
	unsigned int numTerms = reacTerms_.size() + diffTerms_.size();
	assert( v.size() == numTerms * meshIndex_.size() );

	for ( VPI i = targetMeshIndices_.begin(); 
					i != targetMeshIndices_.end(); ++i ) {
		unsigned int k = numTerms * i->first;
		assert( k < v.size() );
		for ( VPI j = targetMols_.begin(); j != targetMols_.end(); ++j ) {
			y[ i->second ][ j->second ] = v[ k + j->first];
		}
	}
}
