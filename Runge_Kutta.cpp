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

//4th order Runge-Kutta algorithm, 1D:
void Runge_Kutta::moveParticles1D(const double endTime)
{
	//create vector constants:
	const __m128d v2 = _mm_set1_pd(2.0);
	const __m128d v6 = _mm_set1_pd(6.0);

	while(currentTime < endTime)
	{
		const double dt = modifyTimeStep1D();	//implement dynamic timstep (if necessary):
	
		const __m128d vdt = _mm_set1_pd(dt);	//store timestep as vector const

		force1_3D(currentTime);	//compute net force1
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate 1st substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);	//load ith and (i+1)th mass into vector

			//assign force pointers for stylistic purposes:
			double * const pFx = cloud->forceX + i;

			//calculate ith and (i+1)th tidbits: 
			_mm_store_pd(cloud->k1 + i, vdt*_mm_load_pd(pFx)/vmass);	//velocityX tidbit
			_mm_store_pd(cloud->l1 + i, vdt*cloud->getVx1_pd(i));		//positionX tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
		}

		force2_3D(currentTime + dt/2.0);	//compute net force2
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate 2nd substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);	//load ith and (i+1)th mass

			//assign force pointers:
			double * const pFx = cloud->forceX + i;

			//calculate ith and (i+1)th tidbits: 
			_mm_store_pd(cloud->k2 + i, vdt*_mm_load_pd(pFx)/vmass);	//velocityX tidbit
			_mm_store_pd(cloud->l2 + i, vdt*cloud->getVx2_pd(i));		//positionX tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
		}
		
		force3_3D(currentTime + dt/2.0);	//compute net force3
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate 3rd substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);	//load ith and (i+1)th mass

			//assign force pointers:
			double * const pFx = cloud->forceX + i;

			//calculate ith and (i+1)th tibits: 
			_mm_store_pd(cloud->k3 + i, vdt*_mm_load_pd(pFx)/vmass);	//velocityX tidbit
			_mm_store_pd(cloud->l3 + i, vdt*cloud->getVx3_pd(i));		//positionX tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
		}
		
		force4_3D(currentTime + dt);	//compute net force4
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate 4th substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);	//load ith and (i+1)th mass

			//assign force pointers:
			double * const pFx = cloud->forceX + i;

			_mm_store_pd(cloud->k4 + i, vdt*_mm_load_pd(pFx)/vmass);	//velocityX tidbit
			_mm_store_pd(cloud->l4 + i, vdt*cloud->getVx4_pd(i));		//positionX tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
		}

		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate next position and next velocity for entire cloud
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

			//assign position and velocity pointers (stylistic):
			double * const px = cloud->x + i;
			double * const pVx = cloud->Vx + i;

			//calculate next positions and velocities:
			_mm_store_pd(pVx, _mm_load_pd(pVx) + (vk1 + v2*(vk2 + vk3) + vk4)/v6);
			_mm_store_pd(px, _mm_load_pd(px) + (vl1 + v2*(vl2 + vl3) + vl4)/v6);
		}

		currentTime += dt;
	}
}

//4th order Runge-Kutta algorithm, 2D:
void Runge_Kutta::moveParticles2D(const double endTime)
{
	//create vector constants:
	const __m128d v2 = _mm_set1_pd(2.0);
	const __m128d v6 = _mm_set1_pd(6.0);

	while(currentTime < endTime)
	{
		const double dt = modifyTimeStep2D();	//implement dynamic timstep (if necessary):
	
		const __m128d vdt = _mm_set1_pd(dt);	//store timestep as vector const

		force1_3D(currentTime);	//compute net force1
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate 1st substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);	//load ith and (i+1)th mass into vector

			//assign force pointers for stylistic purposes:
			double * const pFx = cloud->forceX + i;
			double * const pFy = cloud->forceY + i;

			//calculate ith and (i+1)th tidbits: 
			_mm_store_pd(cloud->k1 + i, vdt*_mm_load_pd(pFx)/vmass);	//velocityX tidbit
			_mm_store_pd(cloud->l1 + i, vdt*cloud->getVx1_pd(i));		//positionX tidbit
			_mm_store_pd(cloud->m1 + i, vdt*_mm_load_pd(pFy)/vmass);	//velocityY tidbit
			_mm_store_pd(cloud->n1 + i, vdt*cloud->getVy1_pd(i));   	//positionY tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
		}

		force2_3D(currentTime + dt/2.0);	//compute net force2
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate 2nd substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);	//load ith and (i+1)th mass

			//assign force pointers:
			double * const pFx = cloud->forceX + i;
			double * const pFy = cloud->forceY + i;

			//calculate ith and (i+1)th tidbits: 
			_mm_store_pd(cloud->k2 + i, vdt*_mm_load_pd(pFx)/vmass);	//velocityX tidbit
			_mm_store_pd(cloud->l2 + i, vdt*cloud->getVx2_pd(i));		//positionX tidbit
			_mm_store_pd(cloud->m2 + i, vdt*_mm_load_pd(pFy)/vmass);	//velocityY tidbit
			_mm_store_pd(cloud->n2 + i, vdt*cloud->getVy2_pd(i));		//positionY tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
		}
		
		force3_3D(currentTime + dt/2.0);	//compute net force3
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate 3rd substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);	//load ith and (i+1)th mass

			//assign force pointers:
			double * const pFx = cloud->forceX + i;
			double * const pFy = cloud->forceY + i;

			//calculate ith and (i+1)th tibits: 
			_mm_store_pd(cloud->k3 + i, vdt*_mm_load_pd(pFx)/vmass);	//velocityX tidbit
			_mm_store_pd(cloud->l3 + i, vdt*cloud->getVx3_pd(i));		//positionX tidbit
			_mm_store_pd(cloud->m3 + i, vdt*_mm_load_pd(pFy)/vmass);	//velocityY tidbit
			_mm_store_pd(cloud->n3 + i, vdt*cloud->getVy3_pd(i));		//positionY tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
		}
		
		force4_3D(currentTime + dt);	//compute net force4
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate 4th substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);	//load ith and (i+1)th mass

			//assign force pointers:
			double * const pFx = cloud->forceX + i;
			double * const pFy = cloud->forceY + i;

			_mm_store_pd(cloud->k4 + i, vdt*_mm_load_pd(pFx)/vmass);	//velocityX tidbit
			_mm_store_pd(cloud->l4 + i, vdt*cloud->getVx4_pd(i));		//positionX tidbit
			_mm_store_pd(cloud->m4 + i, vdt*_mm_load_pd(pFy)/vmass);	//velocityY tidbit
			_mm_store_pd(cloud->n4 + i, vdt*cloud->getVy4_pd(i));		//positionY tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
		}

		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate next position and next velocity for entire cloud
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

			//assign position and velocity pointers (stylistic):
			double * const px = cloud->x + i;
			double * const py = cloud->y + i;
			double * const pVx = cloud->Vx + i;
			double * const pVy = cloud->Vy + i;

			//calculate next positions and velocities:
			_mm_store_pd(pVx, _mm_load_pd(pVx) + (vk1 + v2*(vk2 + vk3) + vk4)/v6);
			_mm_store_pd(px, _mm_load_pd(px) + (vl1 + v2*(vl2 + vl3) + vl4)/v6);
			_mm_store_pd(pVy, _mm_load_pd(pVy) + (vm1 + v2*(vm2 + vm3) + vm4)/v6);
			_mm_store_pd(py, _mm_load_pd(py) + (vn1 + v2*(vn2 + vn3) + vn4)/v6);
		}

		currentTime += dt;
	}
}

//4th order Runge-Kutta algorithm, 3D:
void Runge_Kutta::moveParticles3D(const double endTime)
{
	//create vector constants:
	const __m128d v2 = _mm_set1_pd(2.0);
	const __m128d v6 = _mm_set1_pd(6.0);

	while(currentTime < endTime)
	{
		const double dt = modifyTimeStep3D();	//implement dynamic timstep (if necessary):
	
		const __m128d vdt = _mm_set1_pd(dt);	//store timestep as vector const

		force1_3D(currentTime);	//compute net force1
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate 1st substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);	//load ith and (i+1)th mass into vector

			//assign force pointers for stylistic purposes:
			double * const pFx = cloud->forceX + i;
			double * const pFy = cloud->forceY + i;
			double * const pFz = cloud->forceZ + i;

			//calculate ith and (i+1)th tidbits: 
			_mm_store_pd(cloud->k1 + i, vdt*_mm_load_pd(pFx)/vmass);	//velocityX tidbit
			_mm_store_pd(cloud->l1 + i, vdt*cloud->getVx1_pd(i));		//positionX tidbit
			_mm_store_pd(cloud->m1 + i, vdt*_mm_load_pd(pFy)/vmass);	//velocityY tidbit
			_mm_store_pd(cloud->n1 + i, vdt*cloud->getVy1_pd(i));   	//positionY tidbit
			_mm_store_pd(cloud->o1 + i, vdt*_mm_load_pd(pFz)/vmass);	//velocityZ tidbit
			_mm_store_pd(cloud->p1 + i, vdt*cloud->getVz1_pd(i));		//positionZ tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
			_mm_store_pd(pFz, _mm_setzero_pd());
		}

		force2_3D(currentTime + dt/2.0);	//compute net force2
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate 2nd substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);	//load ith and (i+1)th mass

			//assign force pointers:
			double * const pFx = cloud->forceX + i;
			double * const pFy = cloud->forceY + i;
			double * const pFz = cloud->forceZ + i;

			//calculate ith and (i+1)th tidbits: 
			_mm_store_pd(cloud->k2 + i, vdt*_mm_load_pd(pFx)/vmass);	//velocityX tidbit
			_mm_store_pd(cloud->l2 + i, vdt*cloud->getVx2_pd(i));		//positionX tidbit
			_mm_store_pd(cloud->m2 + i, vdt*_mm_load_pd(pFy)/vmass);	//velocityY tidbit
			_mm_store_pd(cloud->n2 + i, vdt*cloud->getVy2_pd(i));		//positionY tidbit
			_mm_store_pd(cloud->o2 + i, vdt*_mm_load_pd(pFz)/vmass);	//velocityZ tidbit
			_mm_store_pd(cloud->p2 + i, vdt*cloud->getVz2_pd(i));		//positionZ tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
			_mm_store_pd(pFz, _mm_setzero_pd());
		}
		
		force3_3D(currentTime + dt/2.0);	//compute net force3
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate 3rd substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);	//load ith and (i+1)th mass

			//assign force pointers:
			double * const pFx = cloud->forceX + i;
			double * const pFy = cloud->forceY + i;
			double * const pFz = cloud->forceZ + i;

			//calculate ith and (i+1)th tibits: 
			_mm_store_pd(cloud->k3 + i, vdt*_mm_load_pd(pFx)/vmass);	//velocityX tidbit
			_mm_store_pd(cloud->l3 + i, vdt*cloud->getVx3_pd(i));		//positionX tidbit
			_mm_store_pd(cloud->m3 + i, vdt*_mm_load_pd(pFy)/vmass);	//velocityY tidbit
			_mm_store_pd(cloud->n3 + i, vdt*cloud->getVy3_pd(i));		//positionY tidbit
			_mm_store_pd(cloud->o3 + i, vdt*_mm_load_pd(pFz)/vmass);	//velocityZ tidbit
			_mm_store_pd(cloud->p3 + i, vdt*cloud->getVz3_pd(i));		//positionZ tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
			_mm_store_pd(pFz, _mm_setzero_pd());
		}
		
		force4_3D(currentTime + dt);	//compute net force4
		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate 4th substep components
		{
			const __m128d vmass = _mm_load_pd(cloud->mass + i);	//load ith and (i+1)th mass

			//assign force pointers:
			double * const pFx = cloud->forceX + i;
			double * const pFy = cloud->forceY + i;
			double * const pFz = cloud->forceZ + i;

			_mm_store_pd(cloud->k4 + i, vdt*_mm_load_pd(pFx)/vmass);	//velocityX tidbit
			_mm_store_pd(cloud->l4 + i, vdt*cloud->getVx4_pd(i));		//positionX tidbit
			_mm_store_pd(cloud->m4 + i, vdt*_mm_load_pd(pFy)/vmass);	//velocityY tidbit
			_mm_store_pd(cloud->n4 + i, vdt*cloud->getVy4_pd(i));		//positionY tidbit
			_mm_store_pd(cloud->o4 + i, vdt*_mm_load_pd(pFz)/vmass);	//velocityZ tidbit
			_mm_store_pd(cloud->p4 + i, vdt*cloud->getVz4_pd(i));		//positionZ tidbit

			//reset forces to zero:
			_mm_store_pd(pFx, _mm_setzero_pd());
			_mm_store_pd(pFy, _mm_setzero_pd());
			_mm_store_pd(pFz, _mm_setzero_pd());
		}

		for(unsigned int i = 0, numParticles = cloud->n; i < numParticles; i += 2)	//calculate next position and next velocity for entire cloud
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

			//assign position and velocity pointers (stylistic):
			double * const px = cloud->x + i;
			double * const py = cloud->y + i;
			double * const pz = cloud->z + i;
			double * const pVx = cloud->Vx + i;
			double * const pVy = cloud->Vy + i;
			double * const pVz = cloud->Vz + i;

			//calculate next positions and velocities:
			_mm_store_pd(pVx, _mm_load_pd(pVx) + (vk1 + v2*(vk2 + vk3) + vk4)/v6);
			_mm_store_pd(px, _mm_load_pd(px) + (vl1 + v2*(vl2 + vl3) + vl4)/v6);
			_mm_store_pd(pVy, _mm_load_pd(pVy) + (vm1 + v2*(vm2 + vm3) + vm4)/v6);
			_mm_store_pd(py, _mm_load_pd(py) + (vn1 + v2*(vn2 + vn3) + vn4)/v6);
			_mm_store_pd(pVz, _mm_load_pd(pVz) + (vo1 + v2*(vo2 + vo3) + vo4)/v6);
			_mm_store_pd(pz, _mm_load_pd(pz) + (vp1 + v2*(vp2 + vp3) + vp4)/v6);
		}

		currentTime += dt;
	}
}

//1D:
inline void Runge_Kutta::force1_1D(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force1_1D(time);
}

inline void Runge_Kutta::force2_1D(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force2_1D(time);
}

inline void Runge_Kutta::force3_1D(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force3_1D(time);
}

inline void Runge_Kutta::force4_1D(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force4_1D(time);
}

//2D:
inline void Runge_Kutta::force1_2D(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force1_2D(time);
}

inline void Runge_Kutta::force2_2D(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force2_2D(time);
}

inline void Runge_Kutta::force3_2D(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force3_2D(time);
}

inline void Runge_Kutta::force4_2D(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force4_2D(time);
}

//3D:
inline void Runge_Kutta::force1_3D(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force1_3D(time);
}

inline void Runge_Kutta::force2_3D(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force2_3D(time);
}

inline void Runge_Kutta::force3_3D(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force3_3D(time);
}

inline void Runge_Kutta::force4_3D(const double time) const
{
 	for(unsigned int i = 0; i < numForces; i++)
		theForce[i]->force4_3D(time);
}

/*--------------------------------------------------------------------------
* If any two particles are within 100*(1.45E-6) meters of one another, then
* use reduced time step. Do not reduce time step if already in reduced mode.
* Resume normal time step once all particles are sufficiently separated.
--------------------------------------------------------------------------*/
const double Runge_Kutta::modifyTimeStep1D() const
{
	//set constants:	
	const unsigned int numPar = cloud->n;
	const __m128d dist = _mm_set1_pd(1.45E-4);

	//loop through entire cloud, or until reduction occures
	for(unsigned int j = 0, e = numPar - 1; j < e; j += 2)
	{
		//caculate separation distance b/t adjacent elements:
		const double sepx = cloud->x[j] - cloud->x[j + 1];

		//if particles too close, reduce time step:
		if(abs(sepx) <= 1.45E-4)
			return red_dt;

		//load positions into vectors:
		const __m128d vx1 = cloud->getx1_pd(j);	//x vector

		//calculate separation distance b/t nonadjacent elements:
		for(unsigned int i = j + 2; i < numPar; i += 2)
		{
			//assign position pointers:
			const double * const px2 = cloud->x + i;

			//calculate j,i and j+1,i+1 separation distances:
			__m128d vx2 = vx1 - _mm_load_pd(px2);

			//check separation distances against dist:
			__m128d comp = _mm_cmple_pd(_mm_sqrt_pd(vx2*vx2), dist);

			double low, high;
			_mm_storel_pd(&low, comp);
			_mm_storeh_pd(&high, comp);
			if (isnan(low) || isnan(high))	//if either are too close, reduce time step
				return red_dt;

			//calculate j,i+1 and j+1,i separation distances:
			vx2 = vx1 - _mm_loadr_pd(px2);

			//check separation distances against dist: 
			comp = _mm_cmple_pd(_mm_sqrt_pd(vx2*vx2), dist);

			_mm_storel_pd(&low, comp);
			_mm_storeh_pd(&high, comp);
			if (isnan(low) || isnan(high))	//if either are too close, reduce time step
				return red_dt;
		}
	}

	//reset time step:
	return init_dt;
}

const double Runge_Kutta::modifyTimeStep2D() const
{
	//set constants:	
	const unsigned int numPar = cloud->n;
	const __m128d dist = _mm_set1_pd(1.45E-4);

	//loop through entire cloud, or until reduction occures
	for(unsigned int j = 0, e = numPar - 1; j < e; j += 2)
	{
		//caculate separation distance b/t adjacent elements:
		const double sepx = cloud->x[j] - cloud->x[j + 1];
		const double sepy = cloud->y[j] - cloud->y[j + 1];

		//if particles too close, reduce time step:
		if(sqrt(sepx*sepx + sepy*sepy) <= 1.45E-4)
			return red_dt;

		//load positions into vectors:
		const __m128d vx1 = cloud->getx1_pd(j);	//x vector
		const __m128d vy1 = cloud->gety1_pd(j);	//y vector

		//calculate separation distance b/t nonadjacent elements:
		for(unsigned int i = j + 2; i < numPar; i += 2)
		{
			//assign position pointers:
			const double * const px2 = cloud->x + i;
			const double * const py2 = cloud->y + i;

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

const double Runge_Kutta::modifyTimeStep3D() const
{
	//set constants:	
	const unsigned int numPar = cloud->n;
	const __m128d dist = _mm_set1_pd(1.45E-4);

	//loop through entire cloud, or until reduction occures
	for(unsigned int j = 0, e = numPar - 1; j < e; j += 2)
	{
		//caculate separation distance b/t adjacent elements:
		const double sepx = cloud->x[j] - cloud->x[j + 1];
		const double sepy = cloud->y[j] - cloud->y[j + 1];
		const double sepz = cloud->z[j] - cloud->z[j + 1];

		//if particles too close, reduce time step:
		if(sqrt(sepx*sepx + sepy*sepy + sepz*sepz) <= 1.45E-4)
			return red_dt;

		//load positions into vectors:
		const __m128d vx1 = cloud->getx1_pd(j);	//x vector
		const __m128d vy1 = cloud->gety1_pd(j);	//y vector
		const __m128d vz1 = cloud->getz1_pd(j);	//z vector

		//calculate separation distance b/t nonadjacent elements:
		for(unsigned int i = j + 2; i < numPar; i += 2)
		{
			//assign position pointers:
			const double * const px2 = cloud->x + i;
			const double * const py2 = cloud->y + i;
			const double * const pz2 = cloud->z + i;

			//calculate j,i and j+1,i+1 separation distances:
			__m128d vx2 = vx1 - _mm_load_pd(px2);
			__m128d vy2 = vy1 - _mm_load_pd(py2);
			__m128d vz2 = vz1 - _mm_load_pd(pz2);

			//check separation distances against dist:
			__m128d comp = _mm_cmple_pd(_mm_sqrt_pd(vx2*vx2 + vy2*vy2 + vz2*vz2), dist);

			double low, high;
			_mm_storel_pd(&low, comp);
			_mm_storeh_pd(&high, comp);
			if (isnan(low) || isnan(high))	//if either are too close, reduce time step
				return red_dt;

			//calculate j,i+1 and j+1,i separation distances:
			vx2 = vx1 - _mm_loadr_pd(px2);
			vy2 = vy1 - _mm_loadr_pd(py2);
			vz2 = vz1 - _mm_loadr_pd(pz2);

			//check separation distances against dist: 
			comp = _mm_cmple_pd(_mm_sqrt_pd(vx2*vx2 + vy2*vy2 + vz2*vz2), dist);

			_mm_storel_pd(&low, comp);
			_mm_storeh_pd(&high, comp);
			if (isnan(low) || isnan(high))	//if either are too close, reduce time step
				return red_dt;
		}
	}

	//reset time step:
	return init_dt;
}
