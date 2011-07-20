/*===- ConfinementForceVoid.cpp - libSimulation -===============================
*
*                                  DEMON
*
* This file is distributed under the BSD Open Source License. See LICENSE.TXT  
* for details. 
*
*===-----------------------------------------------------------------------===*/

#include "ConfinementForceVoid.h"
#include <cmath>
#include <iostream>

using namespace std;
	
ConfinementForceVoid::ConfinementForceVoid(Cloud * const myCloud, double confineConst, double voidDecay, double plasmaPotential) : Force(myCloud), confine(confineConst), decay(voidDecay), potentialOffset(plasmaPotential) {}

void ConfinementForceVoid::force1(const double currentTime)
{
	for (cloud_index currentParticle = 0, numParticles = cloud->n; currentParticle < numParticles; currentParticle += 2)
		force(currentParticle, cloud->getx1_pd(currentParticle), cloud->gety1_pd(currentParticle), cloud->getq1_pd(currentParticle));
}

void ConfinementForceVoid::force2(const double currentTime)
{
	for (cloud_index currentParticle = 0, numParticles = cloud->n; currentParticle < numParticles; currentParticle += 2)
		force(currentParticle, cloud->getx2_pd(currentParticle), cloud->gety2_pd(currentParticle), cloud->getq2_pd(currentParticle));
}

void ConfinementForceVoid::force3(const double currentTime)
{
	for (cloud_index currentParticle = 0, numParticles = cloud->n; currentParticle < numParticles; currentParticle += 2)
		force(currentParticle, cloud->getx3_pd(currentParticle), cloud->gety3_pd(currentParticle), cloud->getq3_pd(currentParticle));
}

void ConfinementForceVoid::force4(const double currentTime)
{
	for (cloud_index currentParticle = 0, numParticles = cloud->n; currentParticle < numParticles; currentParticle += 2)
		force(currentParticle, cloud->getx4_pd(currentParticle), cloud->gety4_pd(currentParticle), cloud->getq4_pd(currentParticle));
}

inline void ConfinementForceVoid::force(const cloud_index currentParticle, const __m128d currentPositionX, const __m128d currentPositionY, const __m128d charge)
{
	const __m128d confineV = _mm_set1_pd(confine);
	const __m128d decayV = _mm_set1_pd(decay);
	const __m128d rr = currentPositionX*currentPositionX + currentPositionY*currentPositionY;
	const __m128d r = _mm_sqrt_pd(rr);

	double * const pFx = cloud->forceX + currentParticle;
	double * const pFy = cloud->forceY + currentParticle;

	double rL, rH;
	_mm_storel_pd(&rL, r);
	_mm_storeh_pd(&rH, r);
	const __m128d expR = _mm_set_pd(exp(-decay*rH), exp(-decay*rL));

	double xL, xH, yL, yH;
	_mm_storel_pd(&xL, currentPositionX);
	_mm_storeh_pd(&xH, currentPositionX);
	_mm_storel_pd(&yL, currentPositionY);
	_mm_storeh_pd(&yH, currentPositionY);

	double thetaL, thetaH;
	if(yL > 0.0)         //quadrants I and II
		thetaL = atan2(yL,xL);
	else                 //quadrants III and IV
		thetaL = 2*M_PI - atan2(-yL,xL);
	if(yH > 0.0)         //quadrants I and II
		thetaH = atan2(yH,xH);
	else                 //quadrants III and IV
		thetaH = 2*M_PI - atan2(-yH,xH);

	const __m128d cosV = _mm_set_pd(cos(thetaH), cos(thetaL));
	const __m128d sinV = _mm_set_pd(sin(thetaH), sin(thetaL));

	_mm_store_pd(pFx, _mm_load_pd(pFx) + charge*(confineV*r - decayV*expR)*cosV);
	_mm_store_pd(pFy, _mm_load_pd(pFy) + charge*(confineV*r - decayV*expR)*sinV);

	double * pPhi = cloud->phi + currentParticle;
	_mm_store_pd(pPhi, _mm_set1_pd(-0.5)*confineV*rr + _mm_set1_pd(potentialOffset) + expR);
}

void ConfinementForceVoid::writeForce(fitsfile * const file, int * const error) const
{
	// move to primary HDU:
	if (!*error)
		// file, # indicating primary HDU, HDU type, error
 		fits_movabs_hdu(file, 1, IMAGE_HDU, error);
	
	// add flag indicating that the confinement force is used:
	if (!*error) 
	{
		long forceFlags = 0;
		fits_read_key_lng(file, const_cast<char *> ("FORCES"), &forceFlags, NULL, error);

		// add ConfinementForceVoid bit:
		forceFlags |= ConfinementForceVoidFlag;	// compound bitwise OR

		if (*error == KEY_NO_EXIST || *error == VALUE_UNDEFINED)
			*error = 0; // clear above error

		// add or update keyword:
		if (!*error) 
			fits_update_key(file, TLONG, const_cast<char *> ("FORCES"), &forceFlags, const_cast<char *> ("Force configuration."), error);
	}

	if (!*error)
		// file, key name, value, precision (scientific format), comment
		fits_write_key_dbl(file, const_cast<char *> ("confineConst"), confine, 6, const_cast<char *> ("[V/m^2] (ConfinementForceVoid)"), error);
	if (!*error)
		fits_write_key_dbl(file, const_cast<char *> ("decay"), decay, 6, const_cast<char *> ("[1/m] (ConfinementForceVoid)"), error);

	// write background plasma potential offset:
	if (!*error)
		fits_write_key_dbl(file, const_cast<char *> ("plasmaPotential"), potentialOffset, 6, const_cast<char *> ("[V] (background plasma potential offset)"), error);
}

void ConfinementForceVoid::readForce(fitsfile * const file, int * const error)
{
	// move to primary HDU:
	if (!*error)
		// file, # indicating primary HDU, HDU type, error
 		fits_movabs_hdu(file, 1, IMAGE_HDU, error);

	if (!*error)
		// file, key name, value, don't read comment, error
		fits_read_key_dbl(file, const_cast<char *> ("confineConst"), &confine, NULL, error);
		fits_read_key_dbl(file, const_cast<char *> ("decay"), &decay, NULL, error);
}