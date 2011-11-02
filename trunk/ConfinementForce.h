/*===- ConfinementForce.h - libSimulation -=====================================
*
*                                  DEMON
*
* This file is distributed under the BSD Open Source License. See LICENSE.TXT  
* for details. 
*
*===-----------------------------------------------------------------------===*/

#ifndef CONFINEMENTFORCE_H
#define CONFINEMENTFORCE_H

#include "Force.h"

class ConfinementForce : public Force {
public:
	ConfinementForce(Cloud * const C, double confineConst)
	: Force(C), confine(confineConst) {}
	// IMPORTANT: In the above constructor, confineConst must be positive!
	~ConfinementForce() {}

// public functions:
	void force1(const double currentTime); // rk substep 1
	void force2(const double currentTime); // rk substep 2
	void force3(const double currentTime); // rk substep 3
	void force4(const double currentTime); // rk substep 4

	void writeForce(fitsfile * const file, int * const error) const;
	void readForce(fitsfile * const file, int * const error);

private:
// private variables:
	double confine;

// private functions:
	void force(const cloud_index currentParticle, const __m128d currentPositionX, const __m128d currentPositionY);	// common force calculator
};

#endif // CONFINEMENTFORCE_H
