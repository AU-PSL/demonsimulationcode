/*===- Runge_Kutta.cpp - libSimulation -========================================
*
*                                  DEMON
*
* This file is distributed under the BSD Open Source License. See LICENSE.TXT 
* for details.
*
*===-----------------------------------------------------------------------===*/

#include "Runge_Kutta.h"
#include "CacheOperator.h"
#include <cmath>
#include <limits>

using namespace std;

const double Runge_Kutta::plasmaDensity = 1.0E15;
const double Runge_Kutta::electronMass = 9.109382E-31;
const double Runge_Kutta::ionMass = 6.63352E-26;
const double Runge_Kutta::electronDebye = 37E-6;
const double Runge_Kutta::ionDebye = 370E-6;

Runge_Kutta::Runge_Kutta(Cloud * const myCloud, Force **forces, const double timeStep, const force_index forcesSize, const double startTime)
: cloud(myCloud), theForce(forces), numForces(forcesSize), init_dt(timeStep), currentTime(startTime), 
numOperators(1), operations(new Operator*[numOperators])
{
	operations[0] = new CacheOperator(cloud); // operators are order dependent

	setDynamicChargeParameters(plasmaDensity, electronMass, ionMass, electronDebye, ionDebye);
}

Runge_Kutta::~Runge_Kutta()
{
	for (operator_index i = 0; i < numOperators; i++)
		delete operations[i];
	delete[] operations;
}

// 4th order Runge-Kutta algorithm:
void Runge_Kutta::moveParticles(const double endTime)
{
	//create vector constants:
	const __m128d v2 = _mm_set1_pd(2.0);
	const __m128d v6 = _mm_set1_pd(6.0);

#ifdef CHARGE
	const __m128d qConst3 = _mm_set1_pd(4.0*M_PI*Cloud::particleRadius*Cloud::epsilon0);
#endif

	while (currentTime < endTime)
	{
		// Second argument must be 2 more than the first.
		const double dt = modifyTimeStep(0, 2, 1.0e-4, init_dt); // implement dynamic timstep (if necessary):
		const __m128d vdt = _mm_set1_pd(dt); // store timestep as vector const

		const cloud_index numParticles = cloud->n;
        
		operate1(currentTime);
		force1(currentTime); // compute net force1
		for (cloud_index i = 0; i < numParticles; i += 2) //calculate 1st RK substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);               //load ith and (i+1)th mass into vector

			//assign force pointers for stylistic purposes:
			double * const pFx = cloud->forceX + i;
			double * const pFy = cloud->forceY + i;
			double * const pFz = cloud->forceZ + i;
			double * const pPhi = cloud->phi + i;

			//calculate ith and (i+1)th tidbits: 
			_mm_store_pd(cloud->k1 + i, vdt*_mm_load_pd(pFx)/vmass); //velocityX tidbit
			_mm_store_pd(cloud->l1 + i, vdt*cloud->getVx1_pd(i));    //positionX tidbit
			_mm_store_pd(cloud->m1 + i, vdt*_mm_load_pd(pFy)/vmass); //velocityY tidbit
			_mm_store_pd(cloud->n1 + i, vdt*cloud->getVy1_pd(i));    //positionY tidbit
			_mm_store_pd(cloud->o1 + i, vdt*_mm_load_pd(pFz)/vmass); //velocityZ tidbit
			_mm_store_pd(cloud->p1 + i, vdt*cloud->getVz1_pd(i));    //positionZ tidbit

#ifdef CHARGE
			const __m128d pQ = cloud->getq1_pd(i);
			setChargeConsts(pQ);
			_mm_store_pd(cloud->q1 + i, -vdt*(qConst1*pQ + qConst2*qConst3*_mm_load_pd(pPhi)));
#else
			_mm_store_pd(cloud->q1 + i, _mm_setzero_pd());
#endif

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
			_mm_store_pd(pFz, _mm_setzero_pd());
			_mm_store_pd(pPhi, _mm_setzero_pd());
		}

		operate2(currentTime + dt/2.0);
		force2(currentTime + dt/2.0); // compute net force2
		for (cloud_index i = 0; i < numParticles; i += 2) //calculate 2nd RK substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);               //load ith and (i+1)th mass

			//assign force pointers:
			double * const pFx = cloud->forceX + i;
			double * const pFy = cloud->forceY + i;
			double * const pFz = cloud->forceZ + i;
			double * const pPhi = cloud->phi + i;

			//calculate ith and (i+1)th tidbits: 
			_mm_store_pd(cloud->k2 + i, vdt*_mm_load_pd(pFx)/vmass); //velocityX tidbit
			_mm_store_pd(cloud->l2 + i, vdt*cloud->getVx2_pd(i));    //positionX tidbit
			_mm_store_pd(cloud->m2 + i, vdt*_mm_load_pd(pFy)/vmass); //velocityY tidbit
			_mm_store_pd(cloud->n2 + i, vdt*cloud->getVy2_pd(i));    //positionY tidbit
			_mm_store_pd(cloud->o2 + i, vdt*_mm_load_pd(pFz)/vmass); //velocityZ tidbit
			_mm_store_pd(cloud->p2 + i, vdt*cloud->getVz2_pd(i));    //positionZ tidbit

#ifdef CHARGE
			const __m128d pQ = cloud->getq2_pd(i);
			setChargeConsts(pQ);
			_mm_store_pd(cloud->q2 + i, -vdt*(qConst1*pQ + qConst2*qConst3*_mm_load_pd(pPhi)));
#else
			_mm_store_pd(cloud->q2 + i, _mm_setzero_pd());
#endif

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
			_mm_store_pd(pFz, _mm_setzero_pd());
			_mm_store_pd(pPhi, _mm_setzero_pd());
		}

		operate3(currentTime + dt/2.0);
		force3(currentTime + dt/2.0); // compute net force3
		for (cloud_index i = 0; i < numParticles; i += 2) //calculate 3rd RK substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);               //load ith and (i+1)th mass

			//assign force pointers:
			double * const pFx = cloud->forceX + i;
			double * const pFy = cloud->forceY + i;
			double * const pFz = cloud->forceZ + i;
			double * const pPhi = cloud->phi + i;

			//calculate ith and (i+1)th tibits: 
			_mm_store_pd(cloud->k3 + i, vdt*_mm_load_pd(pFx)/vmass); //velocityX tidbit
			_mm_store_pd(cloud->l3 + i, vdt*cloud->getVx3_pd(i));    //positionX tidbit
			_mm_store_pd(cloud->m3 + i, vdt*_mm_load_pd(pFy)/vmass); //velocityY tidbit
			_mm_store_pd(cloud->n3 + i, vdt*cloud->getVy3_pd(i));    //positionY tidbit
			_mm_store_pd(cloud->o3 + i, vdt*_mm_load_pd(pFz)/vmass); //velocityZ tidbit
			_mm_store_pd(cloud->p3 + i, vdt*cloud->getVz3_pd(i));    //positionZ tidbit

#ifdef CHARGE
			const __m128d pQ = cloud->getq3_pd(i);
			setChargeConsts(pQ);
			_mm_store_pd(cloud->q3 + i, -vdt*(qConst1*pQ + qConst2*qConst3*_mm_load_pd(pPhi)));
#else
			_mm_store_pd(cloud->q3 + i, _mm_setzero_pd());
#endif

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
			_mm_store_pd(pFz, _mm_setzero_pd());
			_mm_store_pd(pPhi, _mm_setzero_pd());
		}

		operate4(currentTime + dt);
		force4(currentTime + dt); // compute net force4
		for (cloud_index i = 0; i < numParticles; i += 2) //calculate 4th RK substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);               //load ith and (i+1)th mass

			//assign force pointers:
			double * const pFx = cloud->forceX + i;
			double * const pFy = cloud->forceY + i;
			double * const pFz = cloud->forceZ + i;
			double * const pPhi = cloud->phi + i;

			_mm_store_pd(cloud->k4 + i, vdt*_mm_load_pd(pFx)/vmass); //velocityX tidbit
			_mm_store_pd(cloud->l4 + i, vdt*cloud->getVx4_pd(i));    //positionX tidbit
			_mm_store_pd(cloud->m4 + i, vdt*_mm_load_pd(pFy)/vmass); //velocityY tidbit
			_mm_store_pd(cloud->n4 + i, vdt*cloud->getVy4_pd(i));    //positionY tidbit
			_mm_store_pd(cloud->o4 + i, vdt*_mm_load_pd(pFz)/vmass); //velocityZ tidbit
			_mm_store_pd(cloud->p4 + i, vdt*cloud->getVz4_pd(i));    //positionZ tidbit

#ifdef CHARGE
			const __m128d pQ = cloud->getq4_pd(i);
			setChargeConsts(pQ);
			_mm_store_pd(cloud->q4 + i, -vdt*(qConst1*pQ + qConst2*qConst3*_mm_load_pd(pPhi)));
#else
			_mm_store_pd(cloud->q4 + i, _mm_setzero_pd());
#endif

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
			_mm_store_pd(pFz, _mm_setzero_pd());
			_mm_store_pd(pPhi, _mm_setzero_pd());
		}

		for (cloud_index i = 0; i < numParticles; i += 2) // calculate next position and next velocity for entire cloud
		{
			//load ith and (i+1)th k's into vectors:
			const __m128d vk1 = _mm_load_pd(cloud->k1 + i);
			const __m128d vk2 = _mm_load_pd(cloud->k2 + i);
			const __m128d vk3 = _mm_load_pd(cloud->k3 + i);
			const __m128d vk4 = _mm_load_pd(cloud->k4 + i);

			//load ith and (i+1)th l's into vectors: 
			const __m128d vl1 = _mm_load_pd(cloud->l1 + i);
			const __m128d vl2 = _mm_load_pd(cloud->l2 + i);
			const __m128d vl3 = _mm_load_pd(cloud->l3 + i);
			const __m128d vl4 = _mm_load_pd(cloud->l4 + i);

			//load ith and (i+1)th m's into vectors: 
			const __m128d vm1 = _mm_load_pd(cloud->m1 + i);
			const __m128d vm2 = _mm_load_pd(cloud->m2 + i);
			const __m128d vm3 = _mm_load_pd(cloud->m3 + i);
			const __m128d vm4 = _mm_load_pd(cloud->m4 + i);

			//load ith and (i+1)th n's into vectors:
			const __m128d vn1 = _mm_load_pd(cloud->n1 + i);
			const __m128d vn2 = _mm_load_pd(cloud->n2 + i);
			const __m128d vn3 = _mm_load_pd(cloud->n3 + i);
			const __m128d vn4 = _mm_load_pd(cloud->n4 + i);

			//load ith and (i+1)th o's into vectors: 
			const __m128d vo1 = _mm_load_pd(cloud->o1 + i);
			const __m128d vo2 = _mm_load_pd(cloud->o2 + i);
			const __m128d vo3 = _mm_load_pd(cloud->o3 + i);
			const __m128d vo4 = _mm_load_pd(cloud->o4 + i);

			//load ith and (i+1)th p's into vectors: 
			const __m128d vp1 = _mm_load_pd(cloud->p1 + i);
			const __m128d vp2 = _mm_load_pd(cloud->p2 + i);
			const __m128d vp3 = _mm_load_pd(cloud->p3 + i);
			const __m128d vp4 = _mm_load_pd(cloud->p4 + i);

			//load ith and (i+1)th q's into vectors:
			const __m128d vq1 = _mm_load_pd(cloud->q1 + i);
			const __m128d vq2 = _mm_load_pd(cloud->q2 + i);
			const __m128d vq3 = _mm_load_pd(cloud->q3 + i);
			const __m128d vq4 = _mm_load_pd(cloud->q4 + i);

			//assign pointers:
			double * const px = cloud->x + i;
			double * const py = cloud->y + i;
			double * const pz = cloud->z + i;
			double * const pVx = cloud->Vx + i;
			double * const pVy = cloud->Vy + i;
			double * const pVz = cloud->Vz + i;
			double * const pC = cloud->charge + i;

			//calculate next positions, velocities, and charges:
			_mm_store_pd(pVx, _mm_load_pd(pVx) + (vk1 + v2*(vk2 + vk3) + vk4)/v6);
			_mm_store_pd(px, _mm_load_pd(px) + (vl1 + v2*(vl2 + vl3) + vl4)/v6);
			_mm_store_pd(pVy, _mm_load_pd(pVy) + (vm1 + v2*(vm2 + vm3) + vm4)/v6);
			_mm_store_pd(py, _mm_load_pd(py) + (vn1 + v2*(vn2 + vn3) + vn4)/v6);
			_mm_store_pd(pVz, _mm_load_pd(pVz) + (vo1 + v2*(vo2 + vo3) + vo4)/v6);
			_mm_store_pd(pz, _mm_load_pd(pz) + (vp1 + v2*(vp2 + vp3) + vp4)/v6);
			_mm_store_pd(pC, _mm_load_pd(pC) + (vq1 + v2*(vq2 + vq3) + vq4)/v6);
		}

		currentTime += dt;
	}
}

inline void Runge_Kutta::operate1(const double time) const
{
 	for (operator_index i = 0; i < numOperators; i++)
		operations[i]->operation1(time);
}

inline void Runge_Kutta::operate2(const double time) const
{
 	for (operator_index i = 0; i < numOperators; i++)
		operations[i]->operation2(time);
}

inline void Runge_Kutta::operate3(const double time) const
{
 	for (operator_index i = 0; i < numOperators; i++)
		operations[i]->operation3(time);
}

inline void Runge_Kutta::operate4(const double time) const
{
 	for (operator_index i = 0; i < numOperators; i++)
		operations[i]->operation4(time);
}

inline void Runge_Kutta::force1(const double time) const
{
 	for (force_index i = 0; i < numForces; i++)
		theForce[i]->force1(time);
}

inline void Runge_Kutta::force2(const double time) const
{
 	for (force_index i = 0; i < numForces; i++)
		theForce[i]->force2(time);
}

inline void Runge_Kutta::force3(const double time) const
{
 	for (force_index i = 0; i < numForces; i++)
		theForce[i]->force3(time);
}

inline void Runge_Kutta::force4(const double time) const
{
 	for (force_index i = 0; i < numForces; i++)
		theForce[i]->force4(time);
}

/*------------------------------------------------------------------------------
* If a particle spacing is less than the specified distance reduce timestep by a
* factor of 10 and recheck with disance reduced by a factor of 10. Once all
* particle spacings are outside the specified distance use the current timestep.
* This allows fine grain control of reduced timesteps.
------------------------------------------------------------------------------*/
const double Runge_Kutta::modifyTimeStep(cloud_index outerIndex, cloud_index innerIndex, const double currentDist, const double currentTimeStep) const
{
	//set constants:
	const cloud_index numPar = cloud->n;
	const __m128d distv = _mm_set1_pd(currentDist);
	const double redFactor = 10.0;

	// loop through entire cloud, or until reduction occures. Reset innerIndex after each loop iteration.
	for (cloud_index e = numPar - 1; outerIndex < e; outerIndex += 2, innerIndex = outerIndex + 2)
	{
		//caculate separation distance b/t adjacent elements:
		const double sepx = cloud->x[outerIndex] - cloud->x[outerIndex + 1];
		const double sepy = cloud->y[outerIndex] - cloud->y[outerIndex + 1];
		const double sepz = cloud->z[outerIndex] - cloud->z[outerIndex + 1];

		//if particles too close, reduce time step:
		if (sqrt(sepx*sepx + sepy*sepy + sepz*sepz) <= currentDist)
			return modifyTimeStep(outerIndex, innerIndex, currentDist/redFactor, currentTimeStep/redFactor);

		// load positions into vectors:
		const __m128d vx1 = cloud->getx1_pd(outerIndex); //x vector
		const __m128d vy1 = cloud->gety1_pd(outerIndex); //y vector
		const __m128d vz1 = cloud->getz1_pd(outerIndex); //z vector

		// calculate separation distance b/t nonadjacent elements:
		for (; innerIndex < numPar; innerIndex += 2)
		{
			//assign position pointers:
			const double * const px2 = cloud->x + innerIndex;
			const double * const py2 = cloud->y + innerIndex;
			const double * const pz2 = cloud->z + innerIndex;

			//calculate j,i and j+1,i+1 separation distances:
			__m128d vx2 = vx1 - _mm_load_pd(px2);
			__m128d vy2 = vy1 - _mm_load_pd(py2);
			__m128d vz2 = vz1 - _mm_load_pd(pz2);

			// check separation distances against dist:
			if (isLessThanOrEqualTo(_mm_sqrt_pd(vx2*vx2 + vy2*vy2 + vz2*vz2), distv))
				return modifyTimeStep(outerIndex, innerIndex, currentDist/redFactor, currentTimeStep/redFactor);

			//calculate j,i+1 and j+1,i separation distances:
			vx2 = vx1 - _mm_loadr_pd(px2);
			vy2 = vy1 - _mm_loadr_pd(py2);
			vz2 = vz1 - _mm_loadr_pd(pz2);
           
			// check separation distances against dist:
			if (isLessThanOrEqualTo(_mm_sqrt_pd(vx2*vx2 + vy2*vy2 + vz2*vz2), distv))
				return modifyTimeStep(outerIndex, innerIndex, currentDist/redFactor, currentTimeStep/redFactor);
		}
	}

	//reset time step:
	return currentTimeStep;
}

inline bool Runge_Kutta::isLessThanOrEqualTo(const __m128d a, const __m128d b)
{
	__m128d comp = _mm_cmple_pd(a, b);
	
	double low, high;
	_mm_storel_pd(&low, comp);
	_mm_storeh_pd(&high, comp);
	
	return isnan(low) || isnan(high);
}

void Runge_Kutta::setDynamicChargeParameters(const double plasmaDensity, const double electronMass, const double ionMass, const double electronDebye, const double ionDebye)
{
	const double e = Cloud::electronCharge;
	const double ee = e*e;

	electronFreqTerm = _mm_set1_pd(sqrt(4.0*M_PI*plasmaDensity*ee/electronMass)/electronDebye);
	ionFreqTerm = _mm_set1_pd(sqrt(4.0*M_PI*plasmaDensity*ee/ionMass)/ionDebye);
	radTerm = _mm_set1_pd(Cloud::particleRadius/sqrt(2.0*M_PI));
	etaDenominator = _mm_set1_pd(4.0*M_PI*Cloud::particleRadius*plasmaDensity*e);

}

void Runge_Kutta::setChargeConsts(const __m128d charge)
{
	const __m128d electronEta = charge/(_mm_set1_pd(electronDebye*electronDebye)*etaDenominator);
	const __m128d ionEta = charge/(_mm_set1_pd(ionDebye*ionDebye)*etaDenominator);

	double valExpL, valExpH;
	_mm_storel_pd(&valExpL, electronEta);
	_mm_storeh_pd(&valExpH, electronEta);
	const __m128d expTerm = _mm_set_pd(exp(-valExpH), exp(-valExpL));

	qConst1 = radTerm*(electronFreqTerm*expTerm + ionFreqTerm);
	qConst2 = radTerm*(electronFreqTerm*expTerm + ionFreqTerm*(_mm_set1_pd(1.0) + ionEta));
}

