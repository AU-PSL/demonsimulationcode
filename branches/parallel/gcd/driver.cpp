/*===- driver_2D.cpp - Driver -=================================================
*
*                                  DEMON
*
* This file is distributed under the BSD Open Source License. See LICENSE.TXT 
* for details.
*
*===-----------------------------------------------------------------------===*/

#include "ConfinementForce.h"
#include "DragForce.h"
#include "DrivingForce.h"
#include "RectConfinementForce.h"
#include "RotationalForce.h"
#include "Runge_Kutta.h"
#include "ShieldedCoulombForce.h"
#include "ThermalForce.h"
#include "ThermalForceLocalized.h"
#include "TimeVaryingDragForce.h"
#include "TimeVaryingThermalForce.h"

#include <ctime>
#include <iostream>
#include <cstdarg>
#include <cassert>
using namespace std;

#define clear_line "\33[2K" // VT100 signal to clear line.
typedef int file_index;

enum clFlagType
{
	CI, // Cloud Index
	D,  // Double
	F   // File index
};

bool Mach = false;                  // true -> perform Mach Cone experiment
double startTime = 0.0;
double dataTimeStep = 0.01;
double simTimeStep = dataTimeStep/100.0;
double endTime = 5.0;
double cloudSize = 0.01;            // one-half side length (aka "radius")
double confinementConst = 1E-13;    // confinementForce
double confinementConstX = 1E-13;   // RectConfinementForce
double confinementConstY = 1E-12;   // RectConfinementForce
double shieldingConstant = 2E4;     // corresponds to 10*(ion debye length)
double gamma = 10.0;
double thermRed = 1E-14;            // default thermal reduction factor
double thermRed1 = thermRed;        // default outer reduction factor (-L)
double thermScale = 1E-14;          // default for TimeVaryingThermalForce
double thermOffset = 0.0;           // default for TimeVaryingThermalForce
double heatRadius = .001;           // apply thermal force only within this radius
double driveConst = .00001;         // used in DrivingForce.cpp for waves
double waveAmplitude = 1E-13;       // driving wave amplitude (default comparable to other forces throughout cloud)
double waveShift = 0.007;           // driving wave shift
double machSpeed = 0.2;             // firing speed for Mach Cone experiment
double massFactor = 100;            // mass multiplier for fired Mach Cone particle
double rmin = cloudSize/2.0;        // inner radius of shear layer
double rmax = rmin + cloudSize/5.0; // outer ratius of shear layer
double rotConst = 1E-15;            // rotational force in shear layer
double dragScale = -1.0;            // used in TimeVaryingDragForce
file_index continueFileIndex = 0;   // Index of argv array that holds the file name of the fitsfile to continue. 
file_index finalsFileIndex = 0;     // Index of argv array that holds the file name of the fitsfile to use finals of.
file_index outputFileIndex = 0;     // Index of argv array that holds the file name of the fitsfile to output.
force_flags usedForces = 0;         // bitpacked forces
cloud_index numParticles = 10;

void help()
{
// This section is white space sensitive to render correctly in an 80 column 
// terminal environment. There should be no tabs.
// 80 cols is ********************************************************************************
     cout << endl 
          << "                                      DEMON" << endl
          << "        Dynamic Exploration of Microparticle clouds Optimized Numerically" << endl << endl
          << "Options:" << endl << endl
          << " -c noDefault.fits      continue run from file" << endl
          << " -C 1E-13               set confinementConst" << endl
          << " -D -1.0 10.0           use TimeVaryingDragForce; set scale, offset" << endl
          << " -e 5.0                 set simulation end time" << endl
          << " -f noDefaut.fits       use final positions and velocities from file" << endl
          << " -g 10.0                set gamma (magnitute of drag constant)" << endl
          << " -h                     display Help (instead of running)" << endl
          << " -L 0.001 1E-14 1E-14   use ThermalForceLocalized; set rad, in/out therm vals" << endl
          << " -M 0.2 100             create Mach Cone; set bullet velocity, mass factor" << endl
          << " -n 10                  set number of particles" << endl
          << " -o 0.01                set the data Output time step" << endl
          << " -O data.fits           set the name of the output file" << endl
          << " -r 0.01                set cloud radius (one-half side length)" << endl
          << " -R 1E-13 1E-12         use RectConfinementForce; set confineConstX,Y" << endl
          << " -s 2E4                 set coulomb shelding constant" << endl
          << " -S 1E-15 0.005 0.007   use RotationalForce; set strength, rmin, rmax" << endl
          << " -t 0.0001              set the simulation time step" << endl
          << " -T 1E-14               use ThermalForce; set thermal reduction factor" << endl
          << " -v 1E-14 0.0           use TimeVaryingThermalForce; set scale and offset" << endl
          << " -w 1E-13 0.007 0.00001 use DrivingForce; set amplitude, shift, driveConst" << endl << endl
          << "Notes: " << endl << endl
          << " Parameters specified above represent the default values and accepted type," << endl
          << "    with the exception of -c and -f, for which there are no default values." << endl
          << " -c appends to file; ignores all force flags (use -f to run with different" << endl
          << "    forces). -c overrides -f if both are specified" << endl
          << " -D uses strengthening drag if scale > 0, weakening drag if scale < 0." << endl
          << " -M is best used by loading up a previous cloud that has reached equalibrium." << endl
          << " -n expects even number, else will add 1 (required for SIMD)." << endl
          << " -S creates a shear layer between rmin = cloudsize/2 and" << endl
          << "    rmax = rmin + cloudsize/5." << endl
          << " -T runs with heat; otherwise, runs cold." << endl
          << " -v uses increases temp if scale > 0, decreasing temp if scale < 0." << endl
          << " -w creates acoustic waves along the x-axis (best with -R)." << endl << endl;
}

// check if force is used or conflicts with a perviously set force.
void checkForce(const force_index numChecks, ...)
{
	va_list arglist;
	va_start(arglist, numChecks);
	
	const char firstOption = (char)va_arg(arglist, int);
	const ForceFlag firstFlag = (ForceFlag)va_arg(arglist, int);
	
	if (usedForces & firstFlag) 
	{
		cout << "Error: option -" << firstOption << " already set." << endl;
		help();
		va_end(arglist);
		exit(1);
	}
	
	for (force_index i = 0; i < numChecks; i++)
	{
		const char nextOption = (char)va_arg(arglist, int);
		const ForceFlag nextFlag = (ForceFlag)va_arg(arglist, int);
		
		if (usedForces & nextFlag)
		{
			cout << "Error: option -" << firstOption << " conflicts with option -" << nextOption << endl;
			help();
			va_end(arglist);
			exit(1);
		}
	}
	
	va_end(arglist);
	usedForces |= firstFlag;
}

bool isUnsigned(const char *val)
{
	for (const char *c = val; *c != '\0'; c++)
		if (*c < '0' || *c > '9')
			return false;
	return true;
}

bool isDouble(const char *val)
{
	for (const char *c = val; *c != '\0'; c++)
		if ((*c < '0' || *c > '9') && *c != 'e' && *c != 'E' && *c != '.' && *c != '-')
			return false;
	return true;
}

bool isOption(const char *val)
{
	return val[0] == '-' && val[2] == '\0';
}

template <typename T>
void optionWarning(const char option, const char *name, const T val)
{
	cout << "Warning: -" << option << " option incomplete. Using default " 
	<< name << " (" << val << ")." << endl;
}

// Check commandline options. Use defaults if values are missing.
void checkOption(const int argc, char * const argv[], int &optionIndex, const char option, unsigned numOptions, ...)
{
	++optionIndex;
	va_list arglist;
	va_start(arglist, numOptions);
	
	for (unsigned int i = 0; i < numOptions; i++)
	{
		const char *name = va_arg(arglist, char *);
		const clFlagType type = (clFlagType)va_arg(arglist, int);
		void *val = va_arg(arglist, void *);
		
		switch (type) {
			case CI: 
			{
				cloud_index *ci = (cloud_index *)val;
				if (optionIndex < argc && isUnsigned(argv[optionIndex]))
					*ci = atoi(argv[optionIndex++]);
				else
					optionWarning<cloud_index> (option, name, *ci);
				break;
			}
			case D:
			{
				double *d = (double *)val;
				if (optionIndex < argc && isDouble(argv[optionIndex]))
					*d = atoi(argv[optionIndex++]);
				else
					optionWarning<double> (option, name, *d);
				break;
			}
			case F:
			{
				const char *defaultFileName = va_arg(arglist, char *);
				file_index *fi = (file_index *)val;
				if (optionIndex < argc && !isDouble(argv[optionIndex]) && !isUnsigned(argv[optionIndex]) && !isOption(argv[optionIndex]))
					*fi = optionIndex++;
				else
					optionWarning<const char *> (option, name, defaultFileName);
				break;
			}
			default:
				assert("Undefined Argument Type");
		}
	}
}

void parseCommandLineOptions(int argc, char * const argv[])
{
	for (int i = 1; i < argc; i++) // argv[0] is the name of the exicutable.
	{
		switch (argv[i][1])
		{
			case 'c': // "c"ontinue from file:
				checkOption(argc, argv, i, 'c', 1, "contine file", F, &continueFileIndex, "");
				break;
			case 'C': // set "C"onfinementConst:
				checkOption(argc, argv, i, 'C', 1, "confinementConst", D, &confinementConst);
				break;
			case 'D': // use TimeVarying"D"ragForce:
				checkForce(usedForces, 1, 'D', TimeVaryingDragForceFlag);
				checkOption(argc, argv, i, 'D', 2, "scale factor", D, &dragScale, "offset", D, &gamma);
				break;
			case 'e': // set "e"nd time:
				checkOption(argc, argv, i, 'e', 1, "end time", D, &endTime);
				break;		
			case 'f': // use "f"inal positions and velocities from previous run:
				checkOption(argc, argv, i, 'f', 1, "finals file", F, &finalsFileIndex, "");
				break;
			case 'g': // set "g"amma:
				checkOption(argc, argv, i, 'g', 1, "gamma", D, &gamma);
				break;
			case 'h': // display "h"elp:
				help();
				exit(0);
			case 'L': // perform "L"ocalized heating experiment:
				checkForce(usedForces, 3, 'L', TimeVaryingDragForceFlag, 'T', ThermalForceFlag, 'v', TimeVaryingThermalForceFlag);
				checkOption(argc, argv, i, 'L', 3, "radius", D, &heatRadius, "heat factor1", D, &thermRed, "heat factor2", D, &thermRed1);
				break;
			case 'M': // perform "M"ach Cone experiment:
				Mach = true;
				checkOption(argc, argv, i, 'M', 2, "velocity", D, &machSpeed, "mass", D, &massFactor);
				break;
			case 'n': // set "n"umber of particles:
				checkOption(argc, argv, i, 'n', 1, "number of particles", CI, &numParticles);
				if (numParticles%2) // odd
					cout << "Warning: -n requires even number of particles. Incrementing number of particles to (" 
					<< ++numParticles << ")." << endl;
				break;
			case 'o': // set dataTimeStep, which conrols "o"utput rate:
				checkOption(argc, argv, i, 'o', 1, "data time step", D, &dataTimeStep);
				break;
			case 'O': // name "O"utput file:
				checkOption(argc, argv, i, 'O', 1, "output file", F, &outputFileIndex, "data.fits");
				break;
				// FIXME: Consider Removing this option
			case 'r': // set cloud "r"adius:
				checkOption(argc, argv, i, 'r', 1, "cloud size", D, &cloudSize);
				break;		
			case 'R': // use "R"ectangular confinement:
				checkForce(usedForces, 1, 'R', RectConfinementForceFlag);
				checkOption(argc, argv, i, 'R', 2, "confine constantX", D, &confinementConstX, "confine constantY", D, &confinementConstY);
				break;
			case 's': // set "s"hielding constant:
				checkOption(argc, argv, i, 's', 1, "shielding constant", D, &shieldingConstant);
				break;
			case 'S': // create rotational "S"hear layer:
				checkForce(usedForces, 1, 'S', RotationalForceFlag);
				checkOption(argc, argv, i, 'S', 3, "force constant", D, &rotConst, "rmin", D, &rmin, "rmax", D, &rmax);
				break;
			case 't': // set "t"imestep:
				checkOption(argc, argv, i, 't', 1, "time step", D, &simTimeStep);
				break;
			case 'T': // set "T"emperature reduction factor:
				checkForce(usedForces, 3, 'T', ThermalForceFlag, 'L',ThermalForceLocalizedFlag, 'v', TimeVaryingThermalForceFlag);
				checkOption(argc, argv, i, 'T', 1, "heat factor", D, &thermRed);
				break;
			case 'v': // use time ""arying thermal force:
				checkForce(usedForces, 3, 'v', TimeVaryingThermalForceFlag, 'L',ThermalForceLocalizedFlag, 'T', ThermalForceFlag);
				checkOption(argc, argv, i, 'v', 2, "heat value scale", D, &thermScale, "heat value offset", D, &thermOffset);
				break;
			case 'w': // drive "w"aves:
				checkForce(usedForces, 1, 'w', DrivingForceFlag);
				checkOption(argc, argv, i, 'w', 3, "amplitude", D, &waveAmplitude, "wave shift", D, &waveShift, "driving constant", D, &driveConst);
				break;
			default: // Handle unknown options by issuing error.
				cout << "Error: Unknown option " << argv[i] << endl;
				help();
				exit(1);
		}
	}
}

// count number of forces in use:
const force_index getNumForces()
{
	force_index i = 0;
	if (usedForces & ConfinementForceFlag)
		++i;
	if (usedForces & DragForceFlag)
		++i;
	if (usedForces & ShieldedCoulombForceFlag)
		++i;
	if (usedForces & RectConfinementForceFlag)
		++i;
	if (usedForces & ThermalForceFlag)
		++i;
	if (usedForces & ThermalForceLocalizedFlag)
		++i;
	if (usedForces & DrivingForceFlag)
		++i;
	if (usedForces & RotationalForceFlag)
		++i;
	if (usedForces & TimeVaryingDragForceFlag)
		++i;
	if (usedForces & TimeVaryingThermalForceFlag)
		++i;
	return i;
}

// check fitsfile for errors:
void checkFitsError(const int error, const int lineNumber)
{
	if (!error)
		return;

	char message[80];
	fits_read_errmsg(message);
	cout << "Error: Fits file error " << error 
	<< " at line number " << lineNumber 
	<< " (driver_2D.cpp)" << endl 
	<< message << endl;
	exit(1);
}

// delete fitsfile:
void deleteFitsFile(char * const filename, int * const error)
{
	// check for pre-existing data file:
	int exists = 0;
	fits_file_exists(filename, &exists, error);

	if (exists)
	{
		cout << "Warning: Removing pre-existing \"" << filename << "\" file." << endl;
		remove(filename); // required by fits, else can't create
	}
	checkFitsError(*error, __LINE__);
}

// Check if fits file exists
void fitsFileExists(char * const filename, int * const error) {
    int exists = 0;
    fits_file_exists(filename, &exists, error);
    if (exists != 1)
    {
        cout << "Error: Fits file \"" << filename << "\" does not exist." << endl;
        help();
        exit(1);
    }

    checkFitsError(*error, __LINE__);
    cout << "Initializing with fits file \"" << filename << "\"." << endl;
}

int main (int argc, char * const argv[]) 
{
	time_t timer = time(NULL); // start timer
	parseCommandLineOptions(argc, argv);

	if (!(usedForces & TimeVaryingDragForceFlag))
		usedForces |= DragForceFlag;
	if (!(usedForces & RectConfinementForceFlag))
		usedForces |= ConfinementForceFlag;
	usedForces |= ShieldedCoulombForceFlag;

/*------------------------------------------------------------------------------
 * Initialize cloud:
 -----------------------------------------------------------------------------*/
	cout << "Status: Initializing cloud." << endl;
    
	// declare fits file and error:
	fitsfile *file = NULL;
	int error = 0;
	Cloud *cloud;

	if (continueFileIndex)
	{
		fitsFileExists(argv[continueFileIndex], &error);
		
		// open file:
		fits_open_file(&file, argv[continueFileIndex], READWRITE, &error); // file pointer, file name (char), read/write, error
		checkFitsError(error, __LINE__);
		
		// use the same forces:
		fits_read_key_lng(file, const_cast<char *> ("FORCES"), &usedForces, NULL, &error);
		checkFitsError(error, __LINE__);
		
		// initialize with last time step from file:
		cloud = Cloud::initializeFromFile(file, &error, &startTime);
		checkFitsError(error, __LINE__);
	}
	else if (finalsFileIndex)
	{
		fitsFileExists(argv[finalsFileIndex], &error);

		// open file:
		fits_open_file(&file, argv[finalsFileIndex], READONLY, &error); // file pointer, file name (char), read only, error
		checkFitsError(error, __LINE__);

		// initialize with last time step from file:
		cloud = Cloud::initializeFromFile(file, &error, NULL);
		checkFitsError(error, __LINE__);
		
		// close file:
 		fits_close_file(file, &error);
		checkFitsError(error, __LINE__);
	}
	else // initialize new cloud on grid:
		cloud = Cloud::initializeGrid(numParticles);

	// Create a new file if we aren't continueing one.
	if (!continueFileIndex)
	{
		if (outputFileIndex) // use specified file name
		{	
			deleteFitsFile(argv[outputFileIndex], &error);
			fits_create_file(&file, argv[outputFileIndex], &error);
			checkFitsError(error, __LINE__);
			
			// create "proper" primary HDU
			// (prevents fits from generating errors when creating binary tables)
			fits_create_img(file, 16, 0, NULL, &error);
			checkFitsError(error, __LINE__);
		}
		else // use default file name
		{
			deleteFitsFile(const_cast<char *> ("data.fits"), &error);
			fits_create_file(&file, const_cast<char *> ("data.fits"), &error);
			checkFitsError(error, __LINE__);
			
			// create "proper" primary HDU
			// (prevents fits from generating errors when creating binary tables)
			fits_create_img(file, 16, 0, NULL, &error);
			checkFitsError(error, __LINE__);
		}
	}
	
/*------------------------------------------------------------------------------
 * This concludes initialization of cloud.
 * Initialize array of Force objects:
 -----------------------------------------------------------------------------*/
	cout << "Status: Initializing forces." << endl;
    
	const force_index numForces = getNumForces();
	Force **forceArray = new Force*[numForces];
	
	force_index index = 0;
	if (usedForces & ConfinementForceFlag)
		forceArray[index++] = new ConfinementForce(cloud, confinementConst);
	if (usedForces & DragForceFlag) 
		forceArray[index++] = new DragForce(cloud, gamma);
	if (usedForces & ShieldedCoulombForceFlag) 
		forceArray[index++] = new ShieldedCoulombForce(cloud, shieldingConstant);
	if (usedForces & RectConfinementForceFlag)
		forceArray[index++] = new RectConfinementForce(cloud, confinementConstX, confinementConstY);
	if (usedForces & ThermalForceFlag)
		forceArray[index++] = new ThermalForce(cloud, thermRed);
	if (usedForces & ThermalForceLocalizedFlag)
		forceArray[index++] = new ThermalForceLocalized(cloud, thermRed, thermRed1, heatRadius);
	if (usedForces & DrivingForceFlag)
		forceArray[index++] = new DrivingForce(cloud, driveConst, waveAmplitude, waveShift);
	if (usedForces & RotationalForceFlag)
		forceArray[index++] = new RotationalForce(cloud, rmin, rmax, rotConst);
	if (usedForces & TimeVaryingDragForceFlag)
		forceArray[index++] = new TimeVaryingDragForce(cloud, dragScale, gamma);
	if (usedForces & TimeVaryingThermalForceFlag)
		forceArray[index++] = new TimeVaryingThermalForce(cloud, thermScale, thermOffset);
	
	if (continueFileIndex) // initialize forces from old file
	{
		for (force_index i = 0; i < numForces; i++)
			forceArray[i]->readForce(file, &error);
		checkFitsError(error, __LINE__);
	}
	else // write forces to new file
	{
		for (force_index i = 0; i < numForces; i++)
			forceArray[i]->writeForce(file, &error);
		checkFitsError(error, __LINE__);
	}

/*------------------------------------------------------------------------------
 * Commence Runge-Kutta algorithm:
 -----------------------------------------------------------------------------*/
	cout << "Status: Commencing Runge-Kutta." << endl << endl;
    
	// write initial data:
	if (!continueFileIndex) 
	{
		cloud->writeCloudSetup(file, &error);
		checkFitsError(error, __LINE__);
	}
	else
	{
		fits_movnam_hdu(file, BINARY_TBL, const_cast<char *> ("TIME_STEP"), 0, &error);
		checkFitsError(error, __LINE__);
	}
	
	if (Mach) 
	{
		// reserve particle 1 for mach experiment
		cloud->x[0] = -2.0*cloudSize;
		cloud->y[0] = 0.0;
		cloud->Vx[0] = machSpeed;
		cloud->Vy[0] = 0.0;
		cloud->mass[0] *= massFactor;
	}
	
	Runge_Kutta rk4(cloud, forceArray, simTimeStep, numForces, startTime);

	// execute simulation for desired length of time:
	while (startTime < endTime)
	{
		cout << clear_line << "\rCurrent Time: " << rk4.currentTime << "s (" 
		<< rk4.currentTime/endTime*100.0 << "% Complete)" << flush;
		
		// call Runge-Kutta algorithm:
		rk4.moveParticles(startTime += dataTimeStep);
		// write positions and velocities:
		cloud->writeTimeStep(file, &error, rk4.currentTime);
	}

/*------------------------------------------------------------------------------
 * This concludes the Runge-Kutta algorithm. Clean up.
 -----------------------------------------------------------------------------*/

	// close fits file:
	fits_close_file(file, &error);

	// calculate and display elapsed time:
	time_t seconds = time(NULL) - timer;
	time_t minutes = seconds/60;
	time_t hours = minutes/60;
	time_t days = hours/24;
	hours -= days*24;
	minutes -= hours*60 + days*1440;
	seconds -= minutes*60 + hours*3600 + days*86400;
	
	cout << clear_line << "\rTime elapsed: " 
	<< days << (days == 1 ? " day, " : " days, ") 
	<< hours << (hours == 1 ? " hour " : " hours, ") 
	<< minutes << (minutes == 1 ? " minute " : " minutes, ") 
	<< seconds << (seconds == 1 ? " second " : " seconds.") << endl;
	
	// clean up objects:
	for (force_index i = 0; i < numForces; i++)
		delete forceArray[i];
	delete[] forceArray;
	delete cloud;
	
	return 0;
}