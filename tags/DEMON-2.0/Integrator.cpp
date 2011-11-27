/*===- Integrator.cpp - libSimulation -=========================================
*
*                                  DEMON
*
* This file is distributed under the BSD Open Source License. See LICENSE.TXT 
* for details.
*
*===-----------------------------------------------------------------------===*/

#include "Integrator.h"
#include "CacheOperator.h"
#include <cmath>
#include <limits>

Integrator::Integrator(Cloud * const C, const ForceArray &FA,
                       const double timeStep, double startTime)
: currentTime(startTime), cloud(C), forces(FA), init_dt(timeStep),
operations({{new CacheOperator(C)}})
SEMAPHORES_MALLOC(1) {
    SEMAPHORES_INIT(1);
}

Integrator::~Integrator() {
	for (Operator * const opt : operations)
		delete opt;
    
    SEMAPHORES_FREE(1);
}

// If particle spacing is less than the specified distance reduce timestep by a
// factor of 10 and recheck with disance reduced by a factor of 10. Once all
// particle spacings are outside the specified distance use the current 
// timestep. This allows fine grain control of reduced timesteps.
const double Integrator::modifyTimeStep(float currentDist, double currentTimeStep) const {
	// set constants:	
	const cloud_index numPar = cloud->n;
	const float redFactor = 10.0f;
    
#ifdef DISPATCH_QUEUES
    // You cannot block capture method arguments. Store these values in to non
    // const block captured variables. The correct names supsequently used by
    // BLOCK_VALUE_DIST and BLOCK_VALUE_TIME
    __block float currDist = currentDist;
    __block double currTimeStep = currentTimeStep;
#endif
    
	// Loop through entire cloud, or until reduction occures. Reset innerIndex 
    // after each loop iteration.
#ifdef DISPATCH_QUEUES
	const cloud_index outerLoop = numPar;
#else
	const cloud_index outerLoop = numPar - 1;
#endif
    BEGIN_PARALLEL_FOR(outerIndex, e, outerLoop, 4, dynamic)
		// caculate separation distance b/t adjacent elements:
		const __m128 outPosX = loadFloatVector(cloud->x + outerIndex);
		const __m128 outPosY = loadFloatVector(cloud->y + outerIndex);
	
		__m128 sepx = outPosX - _mm_shuffle_ps(outPosX, outPosX, _MM_SHUFFLE(0, 1, 2, 3));
		__m128 sepy = outPosY - _mm_shuffle_ps(outPosY, outPosY, _MM_SHUFFLE(0, 1, 2, 3));
        
		// If particles are too close, reduce time step:
        while (isWithInDistance(sepx, sepy, BLOCK_VALUE_DIST)) {
            // Only one thread should modify the distance and timesStep at a time.
            SEMAPHORE_WAIT(0)
            if (isWithInDistance(sepx, sepy, BLOCK_VALUE_DIST)) {
                BLOCK_VALUE_DIST /= redFactor;
                BLOCK_VALUE_TIME /= redFactor;
            }
            SEMAPHORE_SIGNAL(0)
        }
		
		sepx = outPosX - _mm_shuffle_ps(outPosX, outPosX, _MM_SHUFFLE(1, 0, 3, 2));
		sepy = outPosY - _mm_shuffle_ps(outPosY, outPosY, _MM_SHUFFLE(1, 0, 3, 2));
	
		// If particles are too close, reduce time step:
		while (isWithInDistance(sepx, sepy, BLOCK_VALUE_DIST)) {
			// Only one thread should modify the distance and timesStep at a time.
			SEMAPHORE_WAIT(0)
			if (isWithInDistance(sepx, sepy, BLOCK_VALUE_DIST)) {
				BLOCK_VALUE_DIST /= redFactor;
				BLOCK_VALUE_TIME /= redFactor;
			}
			SEMAPHORE_SIGNAL(0)
		}
	
		sepx = outPosX - _mm_shuffle_ps(outPosX, outPosX, _MM_SHUFFLE(2, 3, 0, 1));
		sepy = outPosY - _mm_shuffle_ps(outPosY, outPosY, _MM_SHUFFLE(2, 3, 0, 1));
	
		// If particles are too close, reduce time step:
		while (isWithInDistance(sepx, sepy, BLOCK_VALUE_DIST)) {
			// Only one thread should modify the distance and timesStep at a time.
			SEMAPHORE_WAIT(0)
			if (isWithInDistance(sepx, sepy, BLOCK_VALUE_DIST)) {
				BLOCK_VALUE_DIST /= redFactor;
				BLOCK_VALUE_TIME /= redFactor;
			}
			SEMAPHORE_SIGNAL(0)
		}
        
		// Calculate separation distance b/t nonadjacent elements:
		for (cloud_index innerIndex = outerIndex + 4; innerIndex < numPar; innerIndex += 4) {
			const __m128 inPosX = loadFloatVector(cloud->x + innerIndex);
			const __m128 inPosY = loadFloatVector(cloud->y + innerIndex);
			
			sepx = outPosX - inPosX;
			sepy = outPosY - inPosY;
			
			// If particles are too close, reduce time step:
			while (isWithInDistance(sepx, sepy, BLOCK_VALUE_DIST)) {
				// Only one thread should modify the distance and timesStep at a time.
				SEMAPHORE_WAIT(0)
				if (isWithInDistance(sepx, sepy, BLOCK_VALUE_DIST)) {
					BLOCK_VALUE_DIST /= redFactor;
					BLOCK_VALUE_TIME /= redFactor;
				}
				SEMAPHORE_SIGNAL(0)
			}
			
			sepx = outPosX - _mm_shuffle_ps(inPosX, inPosX, _MM_SHUFFLE(0, 1, 2, 3));
			sepy = outPosY - _mm_shuffle_ps(inPosY, inPosY, _MM_SHUFFLE(0, 1, 2, 3));
			
			// If particles are too close, reduce time step:
			while (isWithInDistance(sepx, sepy, BLOCK_VALUE_DIST)) {
				// Only one thread should modify the distance and timesStep at a time.
				SEMAPHORE_WAIT(0)
				if (isWithInDistance(sepx, sepy, BLOCK_VALUE_DIST)) {
					BLOCK_VALUE_DIST /= redFactor;
					BLOCK_VALUE_TIME /= redFactor;
				}
				SEMAPHORE_SIGNAL(0)
			}
			
			sepx = outPosX - _mm_shuffle_ps(inPosX, inPosX, _MM_SHUFFLE(1, 0, 3, 2));
			sepy = outPosY - _mm_shuffle_ps(inPosY, inPosY, _MM_SHUFFLE(1, 0, 3, 2));
			
			// If particles are too close, reduce time step:
			while (isWithInDistance(sepx, sepy, BLOCK_VALUE_DIST)) {
				// Only one thread should modify the distance and timesStep at a time.
				SEMAPHORE_WAIT(0)
				if (isWithInDistance(sepx, sepy, BLOCK_VALUE_DIST)) {
					BLOCK_VALUE_DIST /= redFactor;
					BLOCK_VALUE_TIME /= redFactor;
				}
				SEMAPHORE_SIGNAL(0)
			}
			
			sepx = outPosX - _mm_shuffle_ps(inPosX, inPosX, _MM_SHUFFLE(2, 3, 0, 1));
			sepy = outPosY - _mm_shuffle_ps(inPosY, inPosY, _MM_SHUFFLE(2, 3, 0, 1));
			
			// If particles are too close, reduce time step:
			while (isWithInDistance(sepx, sepy, BLOCK_VALUE_DIST)) {
				// Only one thread should modify the distance and timesStep at a time.
				SEMAPHORE_WAIT(0)
				if (isWithInDistance(sepx, sepy, BLOCK_VALUE_DIST)) {
					BLOCK_VALUE_DIST /= redFactor;
					BLOCK_VALUE_TIME /= redFactor;
				}
				SEMAPHORE_SIGNAL(0)
			}
		}
	END_PARALLEL_FOR

    return BLOCK_VALUE_TIME;
}

inline __m128 Integrator::loadFloatVector(double * const x) {
	return _mm_set_ps((float)x[0], (float)x[1], (float)x[2], (float)x[3]);
}

inline bool Integrator::isWithInDistance(const __m128 a, const __m128 b, const float dist) {
	return (bool)_mm_movemask_ps(_mm_cmple_ps(_mm_sqrt_ps(a*a + b*b), _mm_set1_ps(dist)));
}