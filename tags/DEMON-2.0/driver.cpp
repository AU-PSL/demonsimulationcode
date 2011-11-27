/*===- driver_2D.cpp - Driver -=================================================
*
*                                  DEMON
*
* This file is distributed under the BSD Open Source License. See LICENSE.TXT 
* for details.
*
*===-----------------------------------------------------------------------===*/

#include "ConfinementForceVoid.h"
#include "DrivingForce.h"
#include "MagneticForce.h"
#include "RectConfinementForce.h"
#include "RotationalForce.h"
#include "Runge_Kutta4.h"
#include "ShieldedCoulombForce.h"
#include "ThermalForceLocalized.h"
#include "TimeVaryingDragForce.h"
#include "TimeVaryingThermalForce.h"

#include <iostream>
#include <cstdarg>
#include <cassert>

// Cannot include cmath because it causes a conflict with gamma.
extern "C" double sqrt(double);

void help();
void checkForce(const size_t numChecks, ...);
bool isUnsigned(const char *val);
bool isDouble(const char *val);
bool isOption(const char *val);
void checkOption(const int argc, char * const argv[], int &optionIndex, 
                 const char option, unsigned numOptions, ...);
void parseCommandLineOptions(int argc, char * const argv[]);
void checkFitsError(const int error, const int lineNumber);
void deleteFitsFile(char * const filename, int &error);
void fitsFileExists(char * const filename, int &error);
void fitsFileCreate(fitsfile **file, char * const fileName, int &error);

using namespace std;
using namespace chrono;

#define clear_line "\33[2K" // VT100 signal to clear line.
typedef int file_index;
typedef duration<long, ratio<86400> > days;

enum clFlagType : int {
	CI, // cloud_Index
	D,  // double
	F   // file_index
};

bool Mach = false;                  // true -> perform Mach Cone experiment
bool rk4 = true;
double voidDecay = 0.4;             // decay constant in ConfinementForceVoid [m^-1]
double magneticFieldStrength = 1.0; // magnitude of B-field in z-direction [T]
double startTime = 0.0;             // [s]
double dataTimeStep = 0.01;         // [s]
double simTimeStep =
    dataTimeStep/100.0;             // [s]
double endTime = 5.0;               // [s]
double confinementConst = 100.0;    // confinementForce [V/m^2]
double confinementConstX = 100.0;   // RectConfinementForce [V/m^2]
double confinementConstY = 1000.0;  // RectConfinementForce [V/m^2]
double shieldingConstant = 2E4;     // corresponds to 10*(ion debye length) [m^-1]
double gamma = 10.0;                // dust drag frequency [Hz]
double thermRed = 1E-14;            // default thermal reduction factor [N]
double thermRed1 = thermRed;        // default outer reduction factor (-L) [N]
double thermScale = 1E-14;          // default for TimeVaryingThermalForce [N/s]
double thermOffset = 0.0;           // default for TimeVaryingThermalForce [N]
double heatRadius = 0.001;          // apply thermal force only within this radius [m]
double driveConst = 0.00001;        // used in DrivingForce.cpp for waves [m^2]
double waveAmplitude = 1E-13;       // driving wave amplitude (default comparable to other forces throughout cloud) [N]
double waveShift = 0.007;           // driving wave shift [m]
double machSpeed = 0.2;             // firing speed for Mach Cone experiment [m/s]
double massFactor = 100;            // mass multiplier for fired Mach Cone particle
double qMean = 6000.0;              // Mean number of charges of the guassian charge distriburion [c]
double qSigma = 100.0;              // Standard deviation of number of charges [c]
double rMean = 1.45E-6;             // Mean dust particle radius of the guassian size distribution [m]
double rSigma = 0.0;                // Standard deviation of the dust sise distribution [m]
double rmin = 
	Cloud::interParticleSpacing*5.0;  // inner radius of shear layer [m]
double rmax = 
	Cloud::interParticleSpacing*10.0; // outer ratius of shear layer [m]
double rotConst = 1E-15;            // rotational force in shear layer [N]
double dragScale = -1.0;            // used in TimeVaryingDragForce [Hz/s]
file_index continueFileIndex = 0;   // Index of argv array that holds the file name of the fitsfile to continue. 
file_index finalsFileIndex = 0;     // Index of argv array that holds the file name of the fitsfile to use finals of.
file_index outputFileIndex = 0;     // Index of argv array that holds the file name of the fitsfile to output.
force_flags usedForces = 0;         // bitpacked forces
cloud_index numParticles = 10;

// Displat help. This section is white space sensitive to render correctly in an 
// 80 column terminal environment. There should be no tabs.
// 80 cols is ********************************************************************************
void help() {
     cout << endl 
          << "                                      DEMON" << endl
          << "        Dynamic Exploration of Microparticle clouds Optimized Numerically" << endl << endl
          << "Options:" << endl << endl
          << " -B 1.0                 set magnitude of B-field in z-direction [T]" << endl
          << " -c noDefault.fits      continue run from file" << endl
          << " -C 100.0               set confinementConst [V/m^2]" << endl
          << " -D -1.0 10.0           use TimeVaryingDragForce; set scale [Hz/s], offset [Hz]" << endl
          << " -e 5.0                 set simulation end time [s]" << endl
          << " -f noDefaut.fits       use final positions and velocities from file" << endl
          << " -g 10.0                set gamma (magnitute of drag constant) [Hz]" << endl
          << " -h                     display Help (instead of running)" << endl
          << " -I                     use 2nd order Runge-Kutta integrator" << endl
          << " -L 0.001 1E-14 1E-14   use ThermalForceLocalized; set radius [m], in/out" << endl
          << "                        thermal values [N]" << endl
          << " -M 0.2 100             create Mach Cone; set bullet velocity [m/s], mass factor" << endl
          << " -n 10                  set number of particles" << endl
          << " -o 0.01                set the data Output time step [s]" << endl
          << " -O data.fits           set the name of the output file" << endl
          << " -q 6000.0 100.0        set charge mean and sigma [c]" << endl
          << " -R 100.0 1000.0        use RectConfinementForce; set confineConstX,Y [V/m^2]" << endl
          << " -r 1.45E-6 0.0         set mean particle radius and sigma [m]" << endl
          << " -s 2E4                 set coulomb shelding constant [m^-1]" << endl
          << " -S 1E-15 0.005 0.007   use RotationalForce; set strength [N], rmin, rmax [m]" << endl
          << " -t 0.0001              set the simulation time step [s]" << endl
          << " -T 1E-14               use ThermalForce; set thermal reduction factor [N]" << endl
          << " -v 1E-14 0.0           use TimeVaryingThermalForce; set scale [N/s]" << endl
          << "                        and offset [N]" << endl
          << " -V 0.4                 use ConfinementForceVoid; set void decay constant [m^-1]" << endl
          << " -w 1E-13 0.007 0.00001 use DrivingForce; set amplitude [N], shift [m]," << endl
          << "                        driveConst [m^-2]" << endl << endl
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

// check if force is used or conflicts with a previously set force.
void checkForce(const size_t numChecks, ...) {
	va_list arglist;
	va_start(arglist, numChecks);
	
	const char firstOption = (char)va_arg(arglist, int);
	const ForceFlag firstFlag = (ForceFlag)va_arg(arglist, long);
	
	if (usedForces & firstFlag) {
		cout << "Error: option -" << firstOption << " already set." << endl;
		help();
		va_end(arglist);
		exit(1);
	}
	
	for (size_t i = 1; i < numChecks; i++) {
		const char nextOption = (char)va_arg(arglist, int);
		const ForceFlag nextFlag = (ForceFlag)va_arg(arglist, int);
		
		if (usedForces & nextFlag) {
			cout << "Error: option -" << firstOption << " conflicts with option -" << nextOption << endl;
			help();
			va_end(arglist);
			exit(1);
		}
	}
	
	va_end(arglist);
	usedForces |= firstFlag;
}

// Check if string is a positive integer.
bool isUnsigned(const char *val) {
	for (const char *c = val; *c != '\0'; c++)
		if (!isdigit(*c))
			return false;
	return true;
}

// Check if string uses a decimal or scientific notation.
bool isDouble(const char *val) {
	for (const char *c = val; *c != '\0'; c++)
		if (!isdigit(*c) && *c != 'e' && *c != 'E' && *c != '.' && *c != '-')
			return false;
	return true;
}

// Check if string had the form "-x".
bool isOption(const char *val) {
	return val[0] == '-' && isalpha(val[1]) && val[2] == '\0';
}

// Warn about incomplete command line parses.
template <typename T>
void optionWarning(const char option, const char *name, const T val) {
	cout << "Warning: -" << option << " option incomplete. Using default " 
	<< name << " (" << val << ")." << endl;
}

// Check commandline options. Use defaults if values are missing.
void checkOption(const int argc, char * const argv[], int &optionIndex, const char option, unsigned numOptions, ...) {
	++optionIndex;
	va_list arglist;
	va_start(arglist, numOptions);
	
	for (unsigned int i = 0; i < numOptions; i++) {
		const char *name = va_arg(arglist, char *);
		const clFlagType type = (clFlagType)va_arg(arglist, int);
		void *val = va_arg(arglist, void *);
		
		switch (type) {
			case CI: { // cloud_index argument
				cloud_index *ci = (cloud_index *)val;
				if (optionIndex < argc && isUnsigned(argv[optionIndex]))
					*ci = (cloud_index)atoi(argv[optionIndex++]);
				else
					optionWarning<cloud_index> (option, name, *ci);
				break;
			}
			case D: { // double argument 
				double *d = (double *)val;
				if (optionIndex < argc && !isOption(argv[optionIndex]) && isDouble(argv[optionIndex]))
					*d = atof(argv[optionIndex++]);
				else
					optionWarning<double> (option, name, *d);
				break;
			}
			case F: { // file_index argument
				const char *defaultFileName = va_arg(arglist, char *);
				file_index *fi = (file_index *)val;
				if (optionIndex < argc && !isOption(argv[optionIndex]) && !isDouble(argv[optionIndex]) && !isUnsigned(argv[optionIndex]))
					*fi = optionIndex++;
				else
					optionWarning<const char *> (option, name, defaultFileName);
				break;
			}
			default:
				va_end(arglist);
				assert("Undefined Argument Type");
		}
	}
	va_end(arglist);
}

void parseCommandLineOptions(int argc, char * const argv[]) {
	// argv[0] is the name of the exicutable. THe routine checkOption increments
    // the array index internally. If check option is not used, i must be
    // incremented manually.
	for (int i = 1; i < argc;) {
		switch (argv[i][1]) {
			case 'B': // set "B"-field:
				checkForce(1, 'B', MagneticForceFlag);
				checkOption(argc, argv, i, 'B', 1, 
                            "magnetic field", D, &magneticFieldStrength);
				break;
			case 'c': // "c"ontinue from file:
				checkOption(argc, argv, i, 'c', 1, 
                            "contine file", F, &continueFileIndex, "");
				break;
			case 'C': // set "C"onfinementConst:
				checkOption(argc, argv, i, 'C', 1, 
                            "confinementConst", D, &confinementConst);
				break;
			case 'D': // use TimeVarying"D"ragForce:
				checkForce(1, 'D', TimeVaryingDragForceFlag);
				checkOption(argc, argv, i, 'D', 2, 
                            "scale factor", D, &dragScale, 
                            "offset",       D, &gamma);
				break;
			case 'e': // set "e"nd time:
				checkOption(argc, argv, i, 'e', 1, 
                            "end time", D, &endTime);
				break;		
			case 'f': // use "f"inal positions and velocities from previous run:
				checkOption(argc, argv, i, 'f', 1, 
                            "finals file", F, &finalsFileIndex, "");
				break;
			case 'g': // set "g"amma:
				checkOption(argc, argv, i, 'g', 1, 
                            "gamma", D, &gamma);
				break;
			case 'h': // display "h"elp:
				help();
				exit(0);
            case 'I': // use 2nd order "i"ntegrator
                rk4 = false;
                i++;
                break;
			case 'L': // perform "L"ocalized heating experiment:
				checkForce(3, 
                           'L', ThermalForceLocalizedFlag, 
                           'T', ThermalForceFlag, 
                           'v', TimeVaryingThermalForceFlag);
				checkOption(argc, argv, i, 'L', 3, 
                            "radius",       D, &heatRadius, 
                            "heat factor1", D, &thermRed, 
                            "heat factor2", D, &thermRed1);
				break;
			case 'M': // perform "M"ach Cone experiment:
				Mach = true;
				checkOption(argc, argv, i, 'M', 2, 
                            "velocity", D, &machSpeed, 
                            "mass",     D, &massFactor);
				break;
			case 'n': // set "n"umber of particles:
				checkOption(argc, argv, i, 'n', 1, 
                            "number of particles", CI, &numParticles);
				if (numParticles%4) {
                    numParticles += numParticles%4;
                    cout << "Warning: -n requires multiples of 4 numbers of particles. Incrementing number of particles to (" 
					<< numParticles << ")." << endl;
                }
				break;
			case 'o': // set dataTimeStep, which conrols "o"utput rate:
				checkOption(argc, argv, i, 'o', 1, 
                            "data time step", D, &dataTimeStep);
				break;
			case 'O': // name "O"utput file:
				checkOption(argc, argv, i, 'O', 1, 
                            "output file", F, &outputFileIndex, "data.fits");
				break;
            case 'q':
                checkOption(argc, argv, i, 'q', 2, 
                            "mean number of charges",  D, &qMean,
                            "number of charges sigma", D, &qSigma);
                break;
            case 'r': // set dust "r"adius
                checkOption(argc, argv, i, 'r', 2, 
                            "mean dust radius", D, &rMean,
							"dust radius sigma", D, &rSigma);
                break;
			case 'R': // use "R"ectangular confinement:
				checkForce(1, 'R', RectConfinementForceFlag);
				checkOption(argc, argv, i, 'R', 2, 
                            "confine constantX", D, &confinementConstX, 
                            "confine constantY", D, &confinementConstY);
				break;
			case 's': // set "s"hielding constant:
				checkOption(argc, argv, i, 's', 1, 
                            "shielding constant", D, &shieldingConstant);
				break;
			case 'S': // create rotational "S"hear layer:
				checkForce(1, 'S', RotationalForceFlag);
				checkOption(argc, argv, i, 'S', 3, 
                            "force constant", D, &rotConst, 
                            "rmin",           D, &rmin, 
                            "rmax",           D, &rmax);
				break;
			case 't': // set "t"imestep:
				checkOption(argc, argv, i, 't', 1, "time step", D, &simTimeStep);
				break;
			case 'T': // set "T"emperature reduction factor:
				checkForce(3, 
                           'T', ThermalForceFlag, 
                           'L', ThermalForceLocalizedFlag, 
                           'v', TimeVaryingThermalForceFlag);
				checkOption(argc, argv, i, 'T', 1, 
                            "heat factor", D, &thermRed);
				break;
			case 'v': // use time ""arying thermal force:
				checkForce(3,
                           'v', TimeVaryingThermalForceFlag,
                           'L', ThermalForceLocalizedFlag,
                           'T', ThermalForceFlag);
				checkOption(argc, argv, i, 'v', 2,
                            "heat value scale",  D, &thermScale,
                            "heat value offset", D, &thermOffset);
				break;
			case 'V': // use ConfinementForceVoid:
				checkForce(1, 'V', ConfinementForceVoidFlag);
				checkOption(argc, argv, i, 'V', 1, 
                            "void decay", D, &voidDecay);
				break;
			case 'w': // drive "w"aves:
				checkForce(1, 'w', DrivingForceFlag);
				checkOption(argc, argv, i, 'w', 3, 
                            "amplitude",        D, &waveAmplitude, 
                            "wave shift",       D, &waveShift, 
                            "driving constant", D, &driveConst);
				break;
			default: // Handle unknown options by issuing error.
				cout << "Error: Unknown option " << argv[i] << endl;
				help();
				exit(1);
		}
	}
}

// Check fits file for errors.
void checkFitsError(const int error, const int lineNumber) {
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

// Delete existing fits file.
void deleteFitsFile(char * const filename, int &error) {
	int exists = 0;
	fits_file_exists(filename, &exists, &error);

	if (exists) {
		cout << "Warning: Removing pre-existing \"" << filename << "\" file." << endl;
		remove(filename);
	}
	checkFitsError(error, __LINE__);
}

// Check if fits file exists.
void fitsFileExists(char * const filename, int &error) {
    int exists = 0;
    fits_file_exists(filename, &exists, &error);
    if (!exists) {
        cout << "Error: Fits file \"" << filename << "\" does not exist." << endl;
        help();
        exit(1);
    }
    checkFitsError(error, __LINE__);
}

// Create a new fits file by deleting an exiting one if it has the same name.
void fitsFileCreate(fitsfile **file, char * const fileName, int &error) {
	deleteFitsFile(fileName, error);
	fits_create_file(file, fileName, &error);
	checkFitsError(error, __LINE__);
}

int main (int argc, char * const argv[]) {
	steady_clock::time_point start = steady_clock::now();
	parseCommandLineOptions(argc, argv);

    // All simulations require the folling three forces if subsitutes are not 
    // used.
	if (!(usedForces & TimeVaryingDragForceFlag))
		usedForces |= DragForceFlag;
	if (!(usedForces & RectConfinementForceFlag) && !(usedForces & ConfinementForceVoidFlag))
		usedForces |= ConfinementForceFlag;
	usedForces |= ShieldedCoulombForceFlag;

	fitsfile *file = NULL;
	int error = 0;
	Cloud *cloud;

	if (continueFileIndex) {
        // Create a cloud using a specified fits file. Subsequent time step data
        // will be appended to this fits file.
		fitsFileExists(argv[continueFileIndex], error);

		fits_open_file(&file, argv[continueFileIndex], READWRITE, &error);
		checkFitsError(error, __LINE__);

		fits_read_key_lng(file, const_cast<char *> ("FORCES"), &usedForces, NULL, &error);
		checkFitsError(error, __LINE__);

		cloud = Cloud::initializeFromFile(file, error, &startTime);
		checkFitsError(error, __LINE__);
	} else if (finalsFileIndex) {
        // Create a cloud using the last time step of a specified fits file.
        // Subsequent time step data will be written to a new file.
		fitsFileExists(argv[finalsFileIndex], error);
		fits_open_file(&file, argv[finalsFileIndex], READONLY, &error);
		checkFitsError(error, __LINE__);
        
		cloud = Cloud::initializeFromFile(file, error, NULL);
		checkFitsError(error, __LINE__);
		
 		fits_close_file(file, &error);
		checkFitsError(error, __LINE__);
	} else
		cloud = Cloud::initializeGrid(numParticles, rMean, rSigma, qMean, qSigma);

	// Create a new file if we aren't continuing an old one.
	if (!continueFileIndex) {
		fitsFileCreate(&file, outputFileIndex ? argv[outputFileIndex] 
											  : const_cast<char *> ("data.fits"), error);
	
		// create "proper" primary HDU
		// (prevents fits from generating errors when creating binary tables)
		fits_create_img(file, 16, 0, NULL, &error);
		checkFitsError(error, __LINE__);
	}
	
    // Create all forces specified in used forces.
    ForceArray forces;
	if (usedForces & ConfinementForceFlag)
		forces.push_back(new ConfinementForce(cloud, confinementConst));
	if (usedForces & ConfinementForceVoidFlag)
		forces.push_back(new ConfinementForceVoid(cloud, confinementConst, voidDecay));
	if (usedForces & DragForceFlag) 
		forces.push_back(new DragForce(cloud, gamma));
	if (usedForces & DrivingForceFlag)
		forces.push_back(new DrivingForce(cloud, driveConst, waveAmplitude, waveShift));
	if (usedForces & MagneticForceFlag)
		forces.push_back(new MagneticForce(cloud, magneticFieldStrength));
	if (usedForces & RectConfinementForceFlag)
		forces.push_back(new RectConfinementForce(cloud, confinementConstX, confinementConstY));
	if (usedForces & RotationalForceFlag)
		forces.push_back(new RotationalForce(cloud, rmin, rmax, rotConst));
	if (usedForces & ShieldedCoulombForceFlag) 
		forces.push_back(new ShieldedCoulombForce(cloud, shieldingConstant));
	if (usedForces & ThermalForceFlag)
		forces.push_back(new ThermalForce(cloud, thermRed));
	if (usedForces & ThermalForceLocalizedFlag)
		forces.push_back(new ThermalForceLocalized(cloud, thermRed, thermRed1, heatRadius));
	if (usedForces & TimeVaryingDragForceFlag)
		forces.push_back(new TimeVaryingDragForce(cloud, dragScale, gamma));
	if (usedForces & TimeVaryingThermalForceFlag)
		forces.push_back(new TimeVaryingThermalForce(cloud, thermScale, thermOffset));
	
	if (continueFileIndex) { // Initialize forces from old file.
		for (Force * const F : forces)
			F->readForce(file, &error);
		checkFitsError(error, __LINE__);
	} else { // Write force config to new file.
		for (Force * const F : forces)
			F->writeForce(file, &error);
		checkFitsError(error, __LINE__);
	}

	
    // Write initial data.
	if (!continueFileIndex) {
		cloud->writeCloudSetup(file, error);
		checkFitsError(error, __LINE__);
	} else {
		fits_movnam_hdu(file, BINARY_TBL, const_cast<char *> ("TIME_STEP"), 0, &error);
		checkFitsError(error, __LINE__);
	}
	
    // If performing the mach cone experiments alter the first particle. The 
    // first particles is move to the left of the cloud. It's mass is increased
    // and it given an inital velocity toward the main cloud.
	if (Mach) {
		cloud->x[0] = -0.75*sqrt((double)cloud->n)*Cloud::interParticleSpacing;
		cloud->y[0] = 0.0;
		cloud->Vx[0] = machSpeed;
		cloud->Vy[0] = 0.0;
		cloud->mass[0] *= massFactor;
	}
    
    // Create 2nd or 4th order Runge-Kutta integrator.
    Integrator * const I = rk4 ? new Runge_Kutta4(cloud, forces, simTimeStep, startTime)
                               : new Runge_Kutta2(cloud, forces, simTimeStep, startTime);

	// Run the simulation. Add a blank line to provide space between warnings
    // the completion counter.
    cout << endl;
	while (startTime < endTime) {
		cout << clear_line << "\rCurrent Time: " << I->currentTime << "s (" 
		<< I->currentTime/endTime*100.0 << "% Complete)" << flush;
		
		// Advance simulation to next timestep.
		I->moveParticles(startTime += dataTimeStep);
		cloud->writeTimeStep(file, error, I->currentTime);
	}

	// Close fits file.
	fits_close_file(file, &error);

	// clean up objects:
	for (Force * const F : forces)
		delete F;
	delete cloud;
    delete I;

	// Calculate and display elapsed time.
	const auto totalTime = steady_clock::now() - start;
	const days d = duration_cast<days> (totalTime);
	const hours h = duration_cast<hours> (totalTime - d);
	const minutes m = duration_cast<minutes> (totalTime - d - h);
	const seconds s = duration_cast<seconds> (totalTime - d - h - m);
	
	cout << clear_line << "\rTime elapsed: " 
	<< d.count() << (d.count() == 1 ? " day, " : " days, ") 
	<< h.count() << (h.count() == 1 ? " hour " : " hours, ") 
	<< m.count() << (m.count() == 1 ? " minute " : " minutes, ") 
	<< s.count() << (s.count() == 1 ? " second " : " seconds.") << endl;
	
	return 0;
}
