/*===- Runge_Kutta.cpp - libSimulation -========================================
*
*                                  DEMON
*
* This file is distributed under the BSD Open Source License. See LICENSE.TXT 
* for details.
*
*===-----------------------------------------------------------------------===*/

#include "Runge_Kutta.h"
#include <cmath>
#include <limits>
#include "VectorCompatibility.h"

using namespace std;

Runge_Kutta::Runge_Kutta(Cloud * const myCloud, Force **forces, const double timeStep, const unsigned int forcesSize, const double startTime)
: cloud(myCloud), theForce(forces), numForces(forcesSize), init_dt(timeStep), red_dt(timeStep/100.0), currentTime(startTime) {}

//4th order Runge-Kutta algorithm:
void Runge_Kutta::moveParticles(const double endTime)
{
	//create vector constants:
	const __m128d v2 = _mm_set1_pd(2.0);
	const __m128d v6 = _mm_set1_pd(6.0);
    
	while(currentTime < endTime)
	{
		const double dt = modifyTimeStep();	//implement dynamic timstep (if necessary):
	
		const __m128d vdt = _mm_set1_pd(dt);	//store timestep as vector const
        
		force1(currentTime);	//compute net force1
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate k1 and l1 for entire cloud
		{
			const __m128d vmass = _mm_load_pd(&cloud->mass[i]);	//load ith and (i+1)th mass into vector

			//assign force pointers for stylistic purposes:
			double *pFx = &cloud->forceX[i];
			double *pFy = &cloud->forceY[i];
           
			//calculate ith and (i+1)th tidbits: 
			_mm_store_pd(&cloud->k1[i], vdt*_mm_load_pd(pFx)/vmass);     //velocityX tidbit
			_mm_store_pd(&cloud->l1[i], vdt*_mm_load_pd(&cloud->Vx[i])); //positionX tidbit
			_mm_store_pd(&cloud->m1[i], vdt*_mm_load_pd(pFy)/vmass);     //velocityY tidbit
			_mm_store_pd(&cloud->n1[i], vdt*_mm_load_pd(&cloud->Vy[i])); //positionY tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
		}
		
		force2(currentTime + dt/2.0);	//compute net force2
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate k2 and l2 for entire cloud
		{
			const __m128d vmass = _mm_load_pd(&cloud->mass[i]);	//load ith and (i+1)th mass

			//assign force pointers:
			double *pFx = &cloud->forceX[i];
			double *pFy = &cloud->forceY[i];

			//calculate ith and (i+1)th tidbits: 
			_mm_store_pd(&cloud->k2[i], vdt*_mm_load_pd(pFx)/vmass);	                                   //velocityX tidbit
			_mm_store_pd(&cloud->l2[i], vdt*(_mm_load_pd(&cloud->Vx[i]) + _mm_load_pd(&cloud->k1[i])/v2)); //positionX tidbit
			_mm_store_pd(&cloud->m2[i], vdt*_mm_load_pd(pFy)/vmass);	                                   //velocityY tidbit
			_mm_store_pd(&cloud->n2[i], vdt*(_mm_load_pd(&cloud->Vy[i]) + _mm_load_pd(&cloud->m1[i])/v2)); //positionY tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
		}
		
		force3(currentTime + dt/2.0);	//compute net force3
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate k3 and l3 for entire cloud
		{
			const __m128d vmass = _mm_load_pd(&cloud->mass[i]);	//load ith and (i+1)th mass

			//assign force pointers:
			double *pFx = &cloud->forceX[i];
			double *pFy = &cloud->forceY[i];

			//calculate ith and (i+1)th tibits: 
			_mm_store_pd(&cloud->k3[i], vdt*_mm_load_pd(pFx)/vmass);	                                   //velocityX tidbit
			_mm_store_pd(&cloud->l3[i], vdt*(_mm_load_pd(&cloud->Vx[i]) + _mm_load_pd(&cloud->k2[i])/v2)); //positionX tidbit
			_mm_store_pd(&cloud->m3[i], vdt*_mm_load_pd(pFy)/vmass);	                                   //velocityY tidbit
			_mm_store_pd(&cloud->n3[i], vdt*(_mm_load_pd(&cloud->Vy[i]) + _mm_load_pd(&cloud->m2[i])/v2)); //positionY tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
		}
		
		force4(currentTime+dt);	//compute net force4
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate k4 and l4 for entire cloud
		{
			const __m128d vmass = _mm_load_pd(&cloud->mass[i]);	//load ith and (i+1)th mass

			//assign force pointers:
			double *pFx = &cloud->forceX[i];
			double *pFy = &cloud->forceY[i];
            
			_mm_store_pd(&cloud->k4[i], vdt*_mm_load_pd(pFx)/vmass);	                              //velocityX tidbit
			_mm_store_pd(&cloud->l4[i], vdt*(_mm_load_pd(&cloud->Vx[i]) + _mm_load_pd(&cloud->k3[i]))); //positionX tidbit
			_mm_store_pd(&cloud->m4[i], vdt*_mm_load_pd(pFy)/vmass);	                              //velocityY tidbit
			_mm_store_pd(&cloud->n4[i], vdt*(_mm_load_pd(&cloud->Vy[i]) + _mm_load_pd(&cloud->m3[i]))); //positionY tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
		}

		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate next position and next velocity for entire cloud
		{
			//load ith and (i+1)th k's into vectors:
			const __m128d vk1 = _mm_load_pd(&cloud->k1[i]);
			const __m128d vk2 = _mm_load_pd(&cloud->k2[i]);
			const __m128d vk3 = _mm_load_pd(&cloud->k3[i]);
			const __m128d vk4 = _mm_load_pd(&cloud->k4[i]);

			//load ith and (i+1)th l's into vectors: 
			const __m128d vl1 = _mm_load_pd(&cloud->l1[i]);
			const __m128d vl2 = _mm_load_pd(&cloud->l2[i]);
			const __m128d vl3 = _mm_load_pd(&cloud->l3[i]);
			const __m128d vl4 = _mm_load_pd(&cloud->l4[i]);

			//load ith and (i+1)th m's into vectors: 
			const __m128d vm1 = _mm_load_pd(&cloud->m1[i]);
			const __m128d vm2 = _mm_load_pd(&cloud->m2[i]);
			const __m128d vm3 = _mm_load_pd(&cloud->m3[i]);
			const __m128d vm4 = _mm_load_pd(&cloud->m4[i]);

			//load ith and (i+1)th n's into vectors:
			const __m128d vn1 = _mm_load_pd(&cloud->n1[i]);
			const __m128d vn2 = _mm_load_pd(&cloud->n2[i]);
			const __m128d vn3 = _mm_load_pd(&cloud->n3[i]);
			const __m128d vn4 = _mm_load_pd(&cloud->n4[i]);

			//assign position and velocity pointers (stylistic):
			double *px = &cloud->x[i];
			double *py = &cloud->y[i];
			double *pVx = &cloud->Vx[i];
			double *pVy = &cloud->Vy[i];

			//calculate next positions and velocities:
			_mm_store_pd(pVx, _mm_load_pd(pVx) + (vk1 + v2*(vk2 + vk3) + vk4)/v6);
			_mm_store_pd(px, _mm_load_pd(px) + (vl1 + v2*(vl2 + vl3) + vl4)/v6);
			_mm_store_pd(pVy, _mm_load_pd(pVy) + (vm1 + v2*(vm2 + vm3) + vm4)/v6);
			_mm_store_pd(py, _mm_load_pd(py) + (vn1 + v2*(vn2 + vn3) + vn4)/v6);
		}

		currentTime += dt;
	}
}

inline void Runge_Kutta::force1(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force1(time);
}

inline void Runge_Kutta::force2(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force2(time);
}

inline void Runge_Kutta::force3(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force3(time);
}

inline void Runge_Kutta::force4(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force4(time);
}

/*--------------------------------------------------------------------------
* If any two particles are within 100*(1.45E-6) meters of one another, then
* use reduced time step. Do not reduce time step if already in reduced mode.
* Resume normal time step once all particles are sufficiently separated.
--------------------------------------------------------------------------*/
const double Runge_Kutta::modifyTimeStep() const
{
	//set constants:	
	const unsigned int numPar = cloud->n;
	const __m128d dist = _mm_set1_pd(1.45E-4);

	//loop through entire cloud, or until reduction occures
	for(unsigned int j = 0, e = numPar - 1; j < e; j += 2)
	{
		//caculate separation distance b/t adjacent elements:
		const double sepx = &cloud->x[j] - &cloud->x[j+1];
		const double sepy = &cloud->y[j] - &cloud->y[j+1];

		//if particles too close, reduce time step:
		if(sqrt(sepx*sepx + sepy*sepy) <= 1.45E-4)
			return red_dt;

		//load positions into vectors:
		const __m128d vx1 = _mm_load_pd(&cloud->x[j]);	//x vector
		const __m128d vy1 = _mm_load_pd(&cloud->y[j]);	//y vector

		//calculate separation distance b/t nonadjacent elements:
		for(unsigned int i = j + 2; i < numPar; i += 2)
		{
			//assign position pointers:
			const double *px2 = &cloud->x[i];
			const double *py2 = &cloud->y[i];

			//calculate j,i and j+1,i+1 separation distances:
			__m128d vx2 = vx1 - _mm_load_pd(px2);
			__m128d vy2 = vy1 - _mm_load_pd(py2);
           
			//check separation distances against dist:
			__m128d comp = _mm_cmple_pd(_mm_sqrt_pd(vx2*vx2 + vy2*vy2), dist);
            
			double low, high;
			_mm_storel_pd(&low, comp);
			_mm_storeh_pd(&high, comp);
			if (isnan(low) || isnan(high))	//if either are too close, reduce time step
				return red_dt;

			//calculate j,i+1 and j+1,i separation distances:
			vx2 = vx1 - _mm_loadr_pd(px2);
			vy2 = vy1 - _mm_loadr_pd(py2);
           
			//check separation distances against dist: 
			comp = _mm_cmple_pd(_mm_sqrt_pd(vx2*vx2 + vy2*vy2), dist);
            
			_mm_storel_pd(&low, comp);
			_mm_storeh_pd(&high, comp);
			if (isnan(low) || isnan(high))	//if either are too close, reduce time step
				return red_dt;
		}
	}
    
    //reset time step:
    return init_dt;
}