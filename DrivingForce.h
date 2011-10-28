/*===- DrivingForce.h - libSimulation -=========================================
*
*                                  DEMON
*
* This file is distributed under the BSD Open Source License. See LICENSE.TXT  
* for details. 
*
*===-----------------------------------------------------------------------===*/

#ifndef DRIVINGFORCE_H
#define DRIVINGFORCE_H

#include "Force.h"
#include "VectorCompatibility.h"

class DrivingForce : public Force {
public:
	DrivingForce(Cloud * const myCloud, const double dampConst, const double amp, const double drivingShift);
	~DrivingForce() {} // destructor

// public functions:
	void force1(const double currentTime); // rk substep 1
	void force2(const double currentTime); // rk substep 2
	void force3(const double currentTime); // rk substep 3
	void force4(const double currentTime); // rk substep 4

	void writeForce(fitsfile * const file, int * const error) const;
	void readForce(fitsfile * const file, int * const error);

private:
// private variables:
	double amplitude; // [N]
	double driveConst; // [m]
	double shift; // [m]
	static const double waveNum; // [1/m]
	static const double angFreq; // [rad*Hz]

// private functions:
	void force(const cloud_index currentParticle, const __m128d currentTime, const __m128d currentPositionX);
};

#endif // DRIVINGFORCE_H
