/**
 * @file		pinc.h
 * @author		Sigvald Marholm <sigvaldm@fys.uio.no>,
 				Gullik Vetvik Killie <gullikvk@fys.uio.no>
 * @copyright	University of Oslo, Norway
 * @brief		PINC main header.
 * @date		11.10.15
 *
 * The PINC main header file constitutes a framework of function and struct
 * declarations central to all PINC modules.
 */

#ifndef PINC_H
#define PINC_H

#include "iniparser.h"
#include <time.h>
#include <hdf5.h>
#include <gsl/gsl_rng.h>

/******************************************************************************
 * DEFINING PHYSICAL CONSTANTS (following SI standard)
 *****************************************************************************/

#define BOLTZMANN 1.3806488e-23
#define ELECTRON_MASS 9.10938215e-31

/******************************************************************************
 * DECLARING DATATYPES
 *****************************************************************************/
/**
 * @brief Contains a population of particles.
 *
 * The position and velocity of particles is stored in a flat manner, such that
 * (x,y,z) of particle 0 comes first, then (x,y,z) of particle 1, and so on
 * (assuming 3D). As an example, printing the (x,y,z) position of the first 3
 * particles:
 *
 * @code
 *	Population pop;
 *  ...
 *	for(int i=0;i<3;i++){
 *		double *iPos = &pop.pos[i*pop.nDims];
 *		printf("Particle %i is located at (%f,%f,%f).\n",i,iPos[0],iPos[1],iPos[2]);
 *	}
 * @endcode
 *
 * The subset of particles belonging to specie s start at particle i=iStart[s]
 * and stop at i=iStop[s]-1 (the -1 simplifies many calculations and is more
 * consistent with the C way of counting). Due to particle increase/decrease
 * there is allocated space for more particles than are present. Thus the
 * _allocated_ space for specie s start at i=iStart[s] and stop at
 * i=iStart[s+1]-1. For convenience, iStart has nSpecies+1 elements such that
 * this is true also for the last specie. The last element is then simply the
 * number of particles allocated in total.
 *
 * The position of the particles is normalized with respect to 'stepSize' in
 * Grid, such that a particle with local position (1,2,3) is located _on_ node
 * (1,2,3) in the grid. Particles are usually specified in local frame but may
 * temporarily be expressed in global frame. See MpiInfo.
 *
 * kinEnergy and potEnergy stores the kinetic and potential energy of the
 * particles if an energy-computing function is utilized (see e.g. puAcc3D1KE
 * and gPotEnergy). Some energy-computing functions may be able to compute the
 * energy per specie, in which case kinEnergy[s] and potEnergy[s] is the energy
 * for specie s. Others may only be able to compute the net energy for all
 * species, in which case this is stored in the last element (
 * kinEnergy[nSpecies] or potEnergy[nSpecies]). Note that this is only the
 * energy for the current subdomain, and a separate function must be employed
 * to sum the energy across the subdomains and store it to an .h5-file.
 *
 * If a population h5 output file is created, the handler to this file is
 * stored in h5.
 */
typedef struct{
	double *pos;		///< Position
	double *vel;		///< Velocity
	long int *iStart;	///< First index of specie s (nSpecies+1 elements)
	long int *iStop;	///< First index not of specie s (nSpecies elements)
	double *renormRho;	///< Re-normalization factors for rho (nSpecies elements)
	double *renormE;	///< Re-normalization factors for E (nSpecies elements)
	double *charge;		///< Normalized charge (q-bar)
	double *mass;		///< Normalized mass (m-bar)
	double *kinEnergy;	///< Kinetic energy (nSpecies+1 elements, last is sum over all species)
	double *potEnergy;	///< Potential energy (nSpecies+1 elements)
	int nSpecies;		///< Number of species
	int nDims;			///< Number of dimensions (usually 3)
	hid_t h5;			///< HDF5 file handler
} Population;

/**
 * @brief Contains information regarding how the PIC code is parallelized.
 *
 * The total simulation domain can be split across several subdomains, one for
 * each MPI node, where position (J,K,L) (in case of 3D) of each subdomain is
 * stored in 'subomain'. 'nSubdomains' represents the number of subdomains along
 * each dimension, and nSubdomainsProd is the cumulative product of nSubdomains
 * similarly as sizeProd in Grid.
 *
 * The local reference frame is defined such that a particle with integer
 * position (j,k,l) would be located _on_ grid point (j,k,l). This makes it fast
 * to determine the index of the nodes surrounding a particle. See Population.
 *
 * offset is the offset of this MPI node's subdomain with respect to the global
 * reference frame. Adding/subtracting this to a position converts to/from the
 * global reference frame. See toLocalFrame(), toGlobalFrame().
 *
 * posToSubdomain is a factor which can be used to determine which subdomain a
 * globally specified position belongs to, e.g. for 1D:
 *
 * @code
 *	int J = (int)(posToNode[0]*pos[0]);
 * @endcode
 */
typedef struct{
	int mpiRank;				///< MPI rank
	int mpiSize;				///< MPI size
	int nDims;					///< Number of dimensions
	int *subdomain;				///< MPI node (nDims elements)
	int *nSubdomains;			///< Number of MPI nodes (nDims elements)
	int *nSubdomainsProd;		///< Cumulative product of nSubdomains (nDims+1 elements)
	int *offset;				///< Offset from global reference frame (nDims elements)
	double *posToSubdomain;		///< Factor for converting position to subdomain (nDims elements)

	int nSpecies;				///< Number of species
	int nNeighbors;				///< Number of neighbors (3^nDims-1) TBD: Omit if it's faster to recompute each time
	int neighborhoodCenter;		///< Index of center/self in neighborhood
	long int **migrants;		///< nMigrants (DEPRECATED)
	long int **migrantsDummy;	///< Useful in computations (DEPRECATED)
	long int *nEmigrants;		///< Number of migrants of each specie to each neighbor (nSpecies*nNeighbor elements)
	long int *nEmigrantsAlloc;	///< Number of migrants allocated for to each neighbor (nNeighbor elements)
	long int *nImmigrants;		///< Number of immigrants of each specie from each neighbour (nSpecies*nNeighbor elements)
	long int nImmigrantsAlloc;
	double **emigrants;			///< Buffer to house emigrants
	double **emigrantsDummy;	///< YAY
	double *immigrants;			///< Buffer to house immigrants
	double *thresholds;			///< Threshold for migration (2*nDims elements)

	MPI_Request *send;
	MPI_Request *recv;
} MpiInfo;

/**
 * @brief Defines different types of boundary conditons
 * @see gAlloc
 * @see gBnd
 */
typedef enum{
	PERIODIC = 0x01,		///< Periodic boundary conditions.
	DIRICHLET = 0x02,		///< Dirichlet boundary condtions.
	NEUMANN = 0x03,			///< Neumann boundary conditons.
	NONE = 0x10				///< For nValues
} bndType;


/**
 * @brief A grid-valued quantity, for instance charge density or E-field.
 *
 * This datatype can represent both scalar fields and vector fields on an N-
 * dimensional grid. Basically, it can be thought of as an arbitrarily
 * dimensioned array object which is stored flat/linearly in memory but which
 * contains supporting variables in order to work with it efficiently.
 *
 * The values in the array is stored in natural/lexicographical ordering in
 * 'val', such that for a 3D array, the elements has the following order:
 *
 *	(0,0,0), (1,0,0), (2,0,0), ..., (0,1,0), (1,1,0), (2,1,0), ...
 *
 * 'rank' is the number of dimensions of the array, while 'size' is the size
 * along each dimension. A 128x128x128 array therefore has rank=3 and
 * size={128,128,128}. The following is an example of how to access element
 * (a,b,c) in the array:
 *
 * @code
 *	Grid *rho;
 *	...
 *	int *size = rho->size;
 *	long int p = a + b*size[0] + c*size[0]*size[1];
 *	double element = rho->val[p];
 * @endcode
 *
 * For convenience and to speed up computations, 'sizeProd' is the cumulative
 * product of 'size' starting at 1. For our example,
 * sizeProd={1,128,128*128,128*128*128}. Using this, the linear index p in the
 * above code can equivalently be computed as:
 *
 * @code
 *	long int p = a*sizeProd[0] + b*sizeProd[1] + c*sizeProd[2];
 * @endcode
 *
 * Also note that adding sizeProd[d] to a linear index p corresponds to to
 * incrementing one step along dimension d in the array. This can be utilized
 * for speeding up certain calculations. sizeProd[rank] is the total number of
 * elements in the array.
 *
 * While this struct is generic and may store any kind of array, the standard
 * way of storing quantities on a grid in PINC is that the first dimension in
 * the array represents the field component (e.g. x, y or z-component of a 3D
 * electric field) while the consecutive dimensions represents the physical
 * dimensions in the grid. For instance a vector field on a 3D grid of
 * 128x128x128 grid points would be represented by a 3x128x128x128 array of
 * rank 4. This means that all field components are lumped together, stored one
 * grid point at a time. A scalar field on the same grid would be represented by
 * a 1x128x128x128 array also of rank 4. The rank is therefore one more than the
 * number of dimensions of the grid. sizeProd[2], for instance, then represents
 * an increment in y-direction in the grid. The following more involved example
 * prints the 3D vector value E on grid points (2,k,3) for all k avoiding
 * complete re-computation of the linear index p each time:
 *
 * @code
 *	Grid *E;
 *
 *	...
 *
 *	int *size = E->size;
 *	long int *sizeProd = E->sizeProd;
 *
 *	int j = 2;
 *	int l = 3;
 *	long int p = j*sizeProd[1] + l*sizeProd[3];
 *
 *	for(int k=0;k<size[2];k++){
 *
 *		double *value = E->val[p];
 *		printf("E(%i,%i,%i)=(%f,%f,%f)\n",j,k,l,value[0],value[1],value[2]);
 *		p += sizeProd[2];
 *	}
 * @endcode
 *
 * In the case of paralellization through domain decomposition this datatype
 * represents the local sub-domain and an MpiInfo object keeps track of how the
 * sub-domaines are related to one another. In the case of domain decomposition,
 * however, each MPI node will need data on grid points outside of its own
 * sub-domain. Thus the outermost layers of grid points will often be ghost
 * points of neighbouring sub-domains. Also, one might need ghost points for
 * some boundary condition implementations regardless of domain decomposition.
 * In any case, 'nGhostLayers' represents the number of ghost layers at each
 * boundary. For 3D (ND), the first 3 (N) elements of 'nGhostLayers' indicates
 * the number of ghost points along the lower boundaries of dimensions x, y, and
 * z, respectively. The last 3 (N) elements indicates the number of ghost points
 * along the upper boundaries. 'trueSize' indicates how many grid points truly
 * belongs to this sub-domain.
 *
 * Consider for instance a vector field on a sub-domain of 128x128x128 grid
 * padded with 1 layer of ghost nodes along the whole sub-domain. This will lead
 * to the struct having the following member values:
 *	- rank = 4
 *	- size = {3,130,130,130}
 *	- trueSize = {3,128,128,128}
 *	- sizeProd = {1,3,390,50700,6591000}
 *	- nGhostLayers = {0,1,1,1,0,1,1,1}
 *
 * Naturally, domain decomposition doesn't affect the first (non-physical)
 * dimension in the array.
 *
 * 'stepSize' is the step-size of each dimension in terms of Debye lengths (or
 * possibly some other quantity in the future). This doesn't make sense for the
 * first non-physical dimension and is therefore arbitrarily set to 1. Thence
 * the product of all elements in 'stepSize' is the volume of a cell.
 *
 * 'h5' is a HDF5 file identifier used to store the grid quantity to an .h5-file
 * and are used by gWriteH5(). The other two h5-variables are also used by
 * gWriteH5() since they only needs to be computed once.
 *
 * 'slice' is a buffer which is large enough to store any slice cut through the
 * array using getSlice().
 */

typedef struct{
	double *val;				///< Array of values on the grid
	int rank;					///< Number of dimensions of array (not grid)
	int *size;					///< Size of array (including ghosts) (rank elements)
	int *trueSize;				///< Size of array (excluding ghosts) (rank elements)
	long int *sizeProd;			///< Cumulative product of size (rank+1 elements)
	int *nGhostLayers;			///< Number of ghost layers in grid (2*rank elements)
	double *stepSize;			///< Step-sizes in Debye lengths (rank elements)

	double *sendSlice;			///< Slice buffer of the grid sent to other
	double *recvSlice;			///< Slice buffer of the grid sent to other
	hid_t h5;					///< HDF5 file handler
	hid_t h5MemSpace;			///< HDF5 memory space description
	hid_t h5FileSpace;			///< HDF5 file space description

	bndType *bnd;				///< Array storing boundary conditions
} Grid;

/**
 * @brief	Timer struct for simple profiling
 * @see allocTimer(), freeTimer(), tMsg()
 *
 *	Simple timer struct to keep track of time.
 *	A simple example of where the time to add 10 to an integer 10 times is
 *	computed:
 *	\code
 Timer *t = tAlloc();

 int k = 0;
 for (int i = 0; i < 10; i++){
	 tStart(t);
	 k += 10;
	 tStop(t);
	 tMsg(t->total, "Hello: ");
 }

 tFree(t);
 *	\endcode
 */
typedef struct{
	unsigned long long int total;		/// Total time
	unsigned long long int start;		/// Previous start time
} Timer;
//
// unsigned long long int getNanoSec();
// void tMsg(int rank, Timer *timer, format....);
// void tStart(...);
// void tStop(...);
// void tic();
// void toc();


/**
 * @brief Defines different types of messages
 * @see msg()
 */
typedef enum{
	STATUS = 0x00,		///< Normal status output about the progress of execution.
	WARNING = 0x01,		///< Warning. Something might not be like the user intended.
	ERROR = 0x02,		///< Error which makes the program unable to proceed. Program will stop.
	TIMER = 0x03,		///< Printing out formatted timing result
	ONCE = 0x10			///< Output message from all MPI-nodes. To be bitwise ORed.
} msgKind;

/**
 * @brief	Function pointers for the different slice operations
 * @see gHaloOp
 */
typedef void (*SliceOpPointer)(const double *slice, Grid *grid, int d, int offset);


/******************************************************************************
 * DEFINED IN POPULATION.C
 *****************************************************************************/
/**
 * @name Population functions (population.c)
 */
///@{

/**
 * @brief	Allocates memory for Population according to ini-file
 * @param	ini		Dictionary to input file
 * @see freePopulation(), posUniform(), velMaxwell()
 *
 * Allocates memory for as many particles and species as specified in
 * populations:nSpecies and population:nAlloc in ini-file. This function only
 * allocates the memory for the particles, it does not generate them.
 *
 * Remember to call freePopulation() to free memory.
 */
Population *pAlloc(const dictionary *ini);

/**
 * @brief					Frees memory for Population
 * @param[in,out]	pop		Pointer to population to be freed
 * @see allocPopulation()
 */
void pFree(Population *pop);

/**
 * @brief	Assign particles uniformly distributed positions
 * @param			ini		Dictionary to input file
 * @param[in,out]	pop		Population of particles
 * @param			grid	Grid in which the particles are to be distributed
 * @param			rngSync	Synchronized random number generator
 * @return			void
 *
 * The amount of particles specified by population:nParticles in ini will be
 * generated with uniformly distributed random positions within the simulation
 * domain (global reference frame). In case of multiple subdomains only
 * particles residing in this MPI node's subdomain will be stored, and will be
 * transformed to its local reference frame. The rng should have the same seed
 * (be synchronized) on all MPI nodes when calling this function as that will
 * ensure that all nodes generates the same particles and discards particles
 * not belonging to their subdomain. Failure to do so may lead to the number of
 * particles generated being different than specified in ini.
 *
 * Beware that this function do not assign any velocity to the particles.
 * @see pVelMaxwell()
 */
void pPosUniform(const dictionary *ini, Population *pop, const MpiInfo *mpiInfo, const gsl_rng *rngSync);

/**
 * @brief	Assign particles artificial positions suitable for debugging
 * @param			ini		Dictionary to input file
 * @param[in,out]	pop		Population
 *
 * The amount of particles specified by population:nParticles in ini will be
 * generated with values given by the following code:
 *
 * @code
 *	pos[i*nDims+d] = 1000*mpiRank + i + (double)d/10 + (double)s/100;
 * @endcode
 */
void pPosDebug(const dictionary *ini, Population *pop);

/**
 * @brief Set the same velocity to all particles
 * @param[in,out]	pop		Population
 * @param			vel		Velocity to set (expected to be pop->nDims long)
 */
void pVelSet(Population *pop, const double *vel);

/**
 * @brief	Assign particles Maxwellian distributed velocities
 * @param			ini		Dictionary to input file
 * @param[in,out]	pop		Population of particles
 * @param			rng		Random number generator
 * @return			void
 *
 * Iterates through all particles belonging to pop and assignes Maxwellian
 * distributed velocities to them, according to the temperature specified in
 * ini. Contrary to in posUniform() rng should at this point _not_ have the same
 * seed as that will lead to particles in different sub-domains having identical
 * velocities.
 */
void pVelMaxwell(const dictionary *ini, Population *pop, const gsl_rng *rng);

/**
 * @brief	Add new particle to population
 * @param[in,out]	pop		Population
 * @param			s		Specie of new particle
 * @param			pos		Position of new particle (nDims elements)
 * @param			vel		Velocity of new particle (nDims elements)
 * @return			void
 */
void pNew(Population *pop, int s, const double *pos, const double *vel);

/**
 * @brief	Cut a particle from a population
 * @param[in,out]	pop		Population
 * @param			s		Which specie the particle belongs to
 * @param			p		Which index p the particle starts at
 * @param[out]		pos		The particle's position
 * @param[out]		vel		The particle's velocity
 * @return					void
 *
 * Note that the particle to fetch is adressed by the array index p. Thus,
 * if the third particle in the population is to be fetched p=2*nDims. In
 * addition, the specie it belongs to, s, must be specified. This rather odd
 * way of specifying which particle to fetch is because p and s of the particle
 * in quest is often already known and calculating p and s from for instance the
 * particle number i is then unnecessary amount of operations. Failure to
 * provide valid values of p and s results in unpredictable behaviour, with
 * the likely consequence of corrupting the whole population.
 */
void pCut(Population *pop, int s, long int p, double *pos, double *vel);

/**
 * @brief	Creates .pop.h5-file to store population in
 * @param	ini				Dictionary to input file
 * @param	pop[in,out]		Population
 * @param	fName			Filename
 * @return	void
 * @see pWriteH5(), pCloseH5()
 *
 * An output file is created whose filename is as explained in openH5File().
 * Remember to call pCloseH5().
 *
 * The file will have one group "/pos" for position data and one group "/vel"
 * for velocity data. Each of these will have groups "specie <s>" for each
 * specie. For each time-step, the population data will be stored in a dataset
 * named "n=<timestep>" where <timestep> is signified with one decimal allowing
 * interleaved quantities.
 *
 * In PINC it is made an distinction between _non-dimensionalizing_ and
 * _normalizing_. Input quantities are non-dimensionalized by specifying them
 * in terms of Debye lengths, plasma frequency, elementary charges and so on
 * rather than using SI-units. Further on, the program normalizes them with
 * respect to for instance cell size in order to make computations as fast as
 * possible. The data stored in .pop.h5 is non-dimensionalized _and_ normalized
 * as it is often much cheaper to just rescale the axes in the visualization
 * tool rather than re-scaling all quantities in PINC.
 *
 * The file will have four attributes of size nDims attached to the root group
 * ("/") which is useful for interpreting the data. These are:
 *	- Position denormalization factor
 *	- Position dimensionalizing factor
 *	- Velocity denormalization factor
 *	- Velocity dimensionalizing factor
 *
 * The position denormalization factor can be multiplied to the integer axis to
 * convert it to be in terms of Debye lengths. Another multiplication by axis
 * dimensionalizing factor converts the axes to meters. Likewise for the
 * velocity factors.
 */
void pOpenH5(const dictionary *ini, Population *pop, const char *fName);

/**
 * @brief	Stores particles in Population in .pop.h5-file
 * @param	pop		Population
 * @param	mpiInfo	MpiInfo
 * @param	posN	Timestep of position data to be stored
 * @param	velN	Timestep of velocity data to be stored
 * @return			void
 *
 * The position and velocity of all particles are stored, referred to global
 * reference frame. The function takes care of merging the particles from all
 * MPI nodes to one file.
 *
 * NB: pop is not constified because all particles are transformed to global
 * reference frame before writing to .h5-file. However, they are transferred
 * back to local reference frame after writing so pop should remain unchanged to
 * within machine precision.
 */
void pWriteH5(Population *pop, const MpiInfo *mpiInfo, double posN, double velN);

/**
 * @brief	Closes .pop.h5-file
 * @param	pop		Population
 * @return	void
 */
void pCloseH5(Population *pop);

/**
 * @brief Transforms particles to local reference frame
 * @param	pop			Population of particles
 * @param	mpiInfo		MPI information about the reference frames
 * @return	void
 * @see toGlobalFrame()
 */
void pToLocalFrame(Population *pop, const MpiInfo *mpiInfo);

/**
 * @brief Transforms particles to global reference frame
 * @param	pop			Population of particles
 * @param	mpiInfo		MPI information about the reference frames
 * @return	void
 * @see toLocalFrame()
 *
 * For parallelization by means of configuration space decomposition, the
 * the particles' positions are usually specified with respect to a local
 * reference frame to that subdomain in order to ease computation. Some
 * operations may require the position in global reference frame (e.g. when
 * storing to file) for which purpose it can be transformed using this function.
 */
void pToGlobalFrame(Population *pop, const MpiInfo *mpiInfo);

/**
 * @brief Creates datasets in .xy.h5-file for storing energy
 * @param	xy		.xy.h5-identifier
 * @param	pop		Population
 * @return			void
 *
 * Uses xyCreateDataset() to create datasets corresponding to the potential and
 * kinetic energies stored in Population. Datasets for both kinetic and
 * potential energy is created for each specie separately, as well as total for
 * for all species, e.g. for two species:
 *	- /energy/kinetic/specie 0
 *  - /energy/kinetic/specie 1
 *	- /energy/kinetic/total
 *	- /energy/potential/specie 0
 *	- /energy/potential/specie 1
 *	- /energy/potential/total
 *
 * pWriteEnergy() can be used to populate these datasets.
 */
void pCreateEnergyDatasets(hid_t xy, Population *pop);

/**
 * @brief Writes energies to .xy.h5-file
 * @param	xy		.xy.h5-identifier
 * @param	pop		Population
 * @return			void
 *
 * Uses xyWrite() to write potential and kinetic energies stored in Population
 * to .xy.h5 datasets. These datasets must first be created using
 * pCreateEnergyDatasets(). Note that this function does not populate the energy
 * variables in Population with meaningful values, energy-computing functions
 * such as gPotEnergy() and puAcc3D1KE() must be used for that. Beware though
 * that energy computing functions typically only populate the energy per
 * specie, or if that is unobtainable by the algorithm, the summed (total)
 * energy for all species. In the former case, the total energy can be obtained
 * simply by addition during post-processing.
 */
void pWriteEnergy(hid_t xy, Population *pop, double x);

///@}

/******************************************************************************
 * DEFINED IN GRID.C
 *****************************************************************************/

 /**
  * @name Grid functions (grid.c)
  */
 ///@{

/**
 * @brief Allocates a Grid object as specified in the input file
 * @param	ini			Input file
 * @param	nValues		Number of values per grid point
 * @return				Pointer to Grid
 *
 * Use nValues=1 for scalar field, nValues=3 for 3D vector field and so on.
 *
 * Remember to free using gFree().
 *
 * NB! Assumes 1 ghost point on all edges for now.
 */

Grid *gAlloc(const dictionary *ini, int nValues);

/**
 * @brief Frees allocated grid
 * @param	grid	Grid
 * @return	void
 */
void gFree(Grid *grid);

/**
 * @brief Allocates the memory for an MpiInfo struct according to input file
 * @param	ini		Input file dictionary
 * @return	Pointer to MpiInfo
 */
MpiInfo *gAllocMpi(const dictionary *ini);

/**
 * @brief Frees the memory of an MpiInfo struct
 * @param	mpiInfo		MpiInfo
 * @return	void
 */
void gFreeMpi(MpiInfo *mpiInfo);

/**
 * @brief Send and recieves the overlapping layers of the subdomains
 * @param sliceOp			SliceOpPointer
 * @param *Grid	Grid struct
 * @param *mpiInfo		MpiInfo struct
 * @param d				Along which dimension it should exhange ghost cells
 *
 *
 * Since each true subdomain is surrounded by ghost layers, both to implement boundary conditions
 * and to facility communication between different processes each taking care of a spesified subdomain,
 * this function fills the ghost layers with the values from the surrounding subdomains. When called
 * it has the option to either set the ghost layer as neighboring subdomain, or add to the existing
 * ghost layers. This is specified by the first argument which can either be setSlice or addSlice.
 *
 *
 * Example with 2 subdomains and a 2D 2x2 true grid:
 * @code
  	1	1	1	1			2	2	2	2
 	1	1	1	1			2	2	2	2
 	1	1	1	1			2	2	2	2
 	1	1	1	1			2	2	2	2
 * @endcode
 *
 *	If we then want to the ghost layers in the subdomains with the values from the surrounding subdomain
 *	we use the following syntax.
 * @code
	gHaloOpDim(setSlice, grid, mpiInfo, 1)
 * @endcode
 * The result should look like:
 *
 * @code
  	2	1	1	2			1	2	2	1
 	2	1	1	2			1	2	2	1
 	2	1	1	2			1	2	2	1
 	2	1	1	2			1	2	2	1
 * @endcode
 *
 *	If we go back to the original grids and we instead want to add to the ghostlayer
 *	we can use addSlice instead.
 * @code
	gHaloOpDim(addSlice, grid, mpiInfo, 1)
 * @endcode
 * This should produce:
 * @code
  	3	1	1	3			3	2	2	3
 	3	1	1	3			3	2	2	3
 	3	1	1	3			3	2	2	3
 	3	1	1	3			3	2	2	3
 * @endcode
 *
 *	If needed it should be quick to facilitate for more slice operations, in addition to set and add.
 *
 * NB! Only works with 1 ghost layer.
 * @see gHaloOp
 */
void gHaloOpDim(SliceOpPointer sliceOp, Grid *grid, const MpiInfo *mpiInfo, int d);

/**
 * @brief Send and recieves the overlapping layers of the subdomains
 * @param sliceOp			SliceOpPointer
 * @param *grid				Grid struct
 * @param *mpiInfo			MpiInfo struct
 *
 * A wrapper to the gHaloOpDim function, that is used when the user wants the interaction in
 * all the dimensions.
 *
 * NB! Only works with 1 ghost layer.
 * @see gExchangeSlice
 * @see gHaloOpDim
 * @see SliceOpPointer
 */
void gHaloOp(SliceOpPointer sliceOp, Grid *grid, const MpiInfo *mpiInfo);

/**
 * @brief Extracts a (dim-1) dimensional slice of grid values.
 * @param	slice 		Return array
 * @param	grid		Grid
 * @param	d			Perpendicular direction to slice
 * @param	offset 		Offset of slice
 * @return				Void
 *
 * This function gets extracts a slice from a N dimensional grid. The integer d
 * decides in which direction the slice is perpendicular to, and the offset decides
 * which which slice it picks out. It needs a preallocated slice array where
 * the extracted slice will be stored.
 *
 * 2D example: Here we have a 5x4 grid and we want to extract a slice corresponding
 * to the second row, where x (d=0) is a constant 1.
 *
 * @code
 * 15   16   17   18   19
 *
 * 10   11   12   13   14
 *
 *  5    6    7    8    9

 *  0    1    2    3    4
 * @endcode
 *
 * @code
	 getSlice(slice, grid, 0, 1);
 * @endcode
 * After running this the slice array consists of
 * slice = \f( [1, 6, 11, 16] \f)
 *
 * @see setSlice
 * @see gHaloOpDim
 **/

void getSlice(double *slice, const Grid *grid, int d, int offset);

/**
 * @brief places a (dim-1) dimensional slice onto a selected slice on the grid.
 * @param	slice		Slice containing a layer of values
 * @param	grid		Grid
 * @param	d 			Perpendicular direction to slice
 * @param	offset 		Offset of slice
 * @return				Void
 *
 * This function places a a slice on a grid. If we have a slice and want to
 * insert it onto a grid this function is used.
 *
 * Example: We have a 1D slice consisting of 6 2s and want to insert it onto
 * the third row, from the bottom.
 * @code
 *	111111
 *	111111
 *	111111
 *	111111
 *
 *	setSlice(slice, grid, 0, 2);
 *
 *	111111
 *	222222
 *	111111
 *	111111
 * @endcode
 *
 * @see setSlice
 * @see gHaloOpDim
 */
void setSlice(const double *slice, Grid *grid, int d, int offset);

/**
 * @brief Adds a slice to a slice in a Grid
 * @param	slice		Slice of values to add into grid
 * @param	grid		Grid
 * @param	d			Perpendicular direction to slice grid
 * @param	offset		Offset of slice in grid
 * @return				void
 *
 * Similar to setSlice() but adds slice to existing values rather than replacing
 * them.
 */
void addSlice(const double *slice, Grid *grid, int d, int offset);


/**
 * @brief Set all values in grid to zero
 * @param	grid	Grid
 * @return			void
 */
void gZero(Grid *grid);

/**
 * @brief Set grid quantity to vector (or scalar) for all grid points
 * @param	grid	Grid
 * @param	value	Array (vector) of values to set
 *
 * Each grid point is set to have the vector value specified by 'value'. Hence
 * value is expected to have length grid->size[0]
 */
void gSet(Grid *grid, const double *value);

/**
 * @brief Multiply all values in grid by a number
 * @param	grid	Grid
 * @param	num		Number to multiply by
 * @return			void
 */
void gMul(Grid *grid, double num);

/**
 * @brief Performs a central space finite difference on a grid
 * @param 	scalar 	Value to do the finite differencing on
 * @return	field	Field returned after derivating
 */

void gFinDiff1st(const Grid *scalar, Grid *field);

/**
 * @brief Performs a 2nd order central space finite difference on a grid
 * @param 	rho 	Value to do the finite differencing on
 * @return	phi		Field returned after derivating
 *
 *
 */
void gFinDiff2nd3D(Grid *phi,const Grid *rho);

/**
 * @brief Performs a 2nd order central space finite difference on a grid
 * @param 	rho 	Value to do the finite differencing on
 * @return	phi		Field returned after derivating
 *
 *
 */
void gFinDiff2nd2D(Grid *phi,const Grid *rho);

 /**
 * @brief Normalize E-field
 * @param	ini		Input file dictionary
 * @param	E		E-field
 *
 * Normalizes an non-normalized (but non-dimensional) E-field store in a Grid
 * object according to step-size, time step and mass and charge of specie 0.
 */
void gNormalizeE(const dictionary *ini, Grid *E);


/**
* @brief Adds a grid to another.
* @param	result		Grid added to
* @param	addition	Grid that is added to the other
*
*	Adds one grid to another. result = result + addition
*
*/
void gAddTo(Grid *result, Grid *addition);

/**
 * @brief Applies boundary conditions to edge
 * @param 	grid		Grid to apply boundary conditions to
 * @param	mpiInfo		Info about subdomain
 *
 * @return 	grid		Returns grid with changed boundary
 *
 * Applies boundary conditions
 */
void gBnd(Grid *grid, const MpiInfo *mpiInfo);

/**
 * @brief	Assign particles artificial positions suitable for debugging
 * @param			ini				Input file dictionary
 * @param[in,out]	grid			Grid
 *
 * A quantity will be artificially assigned values depending on the position of
 * the grid points. For scalar valued quantities, node j will have value j,
 * node (j,k) will have value j+k*10 and so on for higher dimensions. For
 * instance at node (j,k,l)=(4,5,6) the value will be 456.
 *
 * For vector valued quantities the integer part of all values at a given point
 * are the same (unless there is 10 values or more), but the decimal part
 * increments by 0.1 for each value.
 *
 * In addition, 1000*mpiRank is added to all values. For instance the third
 * value (z-component) at grid point (j,k,l)=(4,5,6) of the subdomain with MPI
 * rank 2 will be 2654.3.
 */
void gValDebug(Grid *grid, const MpiInfo *mpiInfo);

/**
 * @brief	Creates .grid.h5-file to store population in
 * @param	ini				Dictionary to input file
 * @param	grid			Grid
 * @param	mpiInfo			MpiInfo
 * @param	denorm			Quantity denormalization factors
 * @param	dimen			Quantity dimensionalizing factors
 * @param	fName			Filename
 * @return	void
 * @see gWriteH5(), gCloseH5()
 *
 * An output file is created whose filename is as explained in openH5File().
 * Remember to call gCloseH5().
 *
 * The file will have one dataset in the root group for each time-step a grid
 * quantity is stored, named "n=<timestep>" where <timestep> is signified with
 * one decimal allowing for interleaved quantities.
 *
 * In PINC it is made an distinction between _non-dimensionalizing_ and
 * _normalizing_. Input quantities are non-dimensionalized by specifying them
 * in terms of Debye lengths, plasma frequency, elementary charges and so on
 * rather than using SI-units. Further on, the program normalizes them with
 * respect to for instance cell size in order to make computations as fast as
 * possible. The data stored in .grid.h5 is non-dimensionalized _and_ normalized
 * as it is often much cheaper to just rescale the axis in the visualization
 * tool rather than re-scaling all quantities in PINC.
 *
 * The file will have four attributes of size nDims attached to the root group
 * ("/") which is useful for interpreting the data. These are:
 *	- Axis denormalization factor
 *	- Axis dimensionalizing factor
 *	- Quantity denormalization factor
 *	- Quantity dimensionalizing factor
 *
 * The axis denormalization factor can be multiplied to the integer axis to
 * convert it to be in terms of Debye lengths. Another multiplication by axis
 * dimensionalizing factor converts the axes to meters. Likewise for the
 * quantity factors. However, since the quantity factors depend on which
 * quantity it is (e.g. charge density or electric field), it must be specified
 * in the inputs denorm and dimen in this function. They are expected to be of
 * length nDims.
 */
void gOpenH5(const dictionary *ini, Grid *grid, const MpiInfo *mpiInfo, const double *denorm, const double *dimen, const char *fName);

/**
 * @brief	Store values in Grid to .grid.h5-file
 * @param	grid			Grid
 * @param	mpiInfo			MpiInfo
 * @param	n				Timestep of quantity to be stored
 * @return	void
 *
 * n is double to allow storing quantities at half time-steps
 * (e.g. leapfrog). All but the most significant decimals are discarded.
 *
 * The function will fail ungracefully if trying to write to an existing
 * dataset (testing omitted for performance reasons).
 */
void gWriteH5(const Grid *grid, const MpiInfo *mpiInfo, double n);

/**
 * @brief	Read values from .grid.h5-fiel to Grid
 * @param	grid			Grid
 * @param	mpiInfo			MpiInfo
 * @param	n				Timestep to read from file
 * @return	void
 *
 * n is double to allow reading quantities from half time-steps
 * (e.g. leapfrog). All but the most significant decimals are discarded.
 *
 * The function will fail ungracefully if trying to read from a non-existing
 * dataset (testing omitted for performance reasons).
 */
void gReadH5(Grid *grid, const MpiInfo *mpiInfo, double n);

/**
 * @brief	Closes .grid.h5-file
 * @param	grid		Grid
 * @return	void
 */
void gCloseH5(Grid *grid);

/**
 * @brief Creates a neighborhood in MpiInfo
 * @param			ini		Dictionary to input file
 * @param[in,out]	mpiInfo	MpiInfo
 * @param			grid	Some grid
 *
 * Prior to creating a neighborhood with this function domain decomposition
 * functions for particles (particle migration) will not work.
 */
void gCreateNeighborhood(const dictionary *ini, MpiInfo *mpiInfo, Grid *grid);

/**
 * @brief Destroys a neighborhood
 * @param	mpiInfo	MpiInfo
 */
void gDestroyNeighborhood(MpiInfo *mpiInfo);

/**
 * @brief Computes potential energy
 * @param		rho		Charge density
 * @param		phi		Electric potential
 * @param[out]	pop		Population to store results in
 *
 * Computes the potential energy as
 * \f[
 *	U=\sum_j\rho_j\phi_j
 * \f]
 *
 * Hence the computed energy will be for the same timestep as rho and phi is in.
 * Only total energy (for all species) is calculated, and hence, the result is
 * stored in potEnergy[nSpecies] in Population. Only the energy for this
 * subdomain is computed. The energy for the whole domain is gathered during
 * storing to h5-file.
 *
 * NB: rho and phi must be same kind of Grid.
 * NB: Assumes electrostatic approximation.
 */
void gPotEnergy(const Grid *rho, const Grid *phi, Population *pop);

/**
 * @brief Returns normalized size of global domain
 * @param	ini		Input file dictionary
 * @return	Array of normalized dimensions.
 *
 * trueSize*nSubdomains-1 for all dimension.
 */
int *gGetGlobalSize(dictionary *ini);

///@}

/******************************************************************************
 * DEFINED IN IO.C
 *****************************************************************************/

/**
 * @brief	The PINC equivalent of printf().
 * @param	kind	STATUS, WARNING or ERROR depending on what to output.
 * @param	format	printf-like format specifier
 * @param	...		printf-like arguments
 * @return	void
 * @see msgKind, fMsg(), printf()
 *
 * This replaces printf() in the PINC context. Similar syntax to printf().
 * In the case of an ERROR, the program is terminated. Appends end-of-line
 * automatically at the end.
 *
 * The message will by default be printed by all nodes calling msg(), however
 * kind can be bitwise ORed with ONCE to only allow the master to display this
 * message, e.g. STATUS|ONCE.
 */
void msg(msgKind kind, const char* restrict format,...);

/**
 * @brief	Prints message to file given by msgfiles:<fNameKey> in ini-file.
 * @param	ini			Input file dictionary
 * @param	fNameKey	Name of key holding the filename
 * @param	format		printf-like format specifier
 * @param	...			printf-like arguments
 * @return	void
 * @see		msg(), printf(), fprintf()
 *
 * This replaces fprintf() in the PINC context being the preferred way to write
 * messages (not data) to files. This guarantees that no file names are hard
 * coded but configurable from the ini-file.
 *
 * Beware that this is not a high performance writing function, and very
 * frequent invocations (for instance per particle) should be avoided.
 */
void fMsg(dictionary *ini, const char* restrict fNameKey, const char* restrict format, ...);

/**
 * @name ini functions
 * Opens .ini input files as dictionary and reads from it. The base
 * functionality is provided by the iniparser library which provides the
 * "dictionary" datatype, but its features is largely extended by the
 * ini-functions (e.g. error handling and array inputs) which serves as a layer
 * on top of iniparser. Do not call iniparser directly.
 *
 * The notation "section:key" is used to indicate keys under various sections
 * in the ini-file.
 *
 * Comma-separated lists (using "," as delimeter) is interpreted as arrays.
 *
 * Remember to free returned arrays.
 *
 * Example of use:
 *
 * @code
 *	dictionary *ini = iniOpen(argc,argv);
 *	int value = iniGetInt(ini,"section:key");
 *	iniClose(ini);
 * @endcode
 *
 * @param		ini			ini-file dictionary
 * @param		key			Key to get value from ("section:key")
 * @param[out]	nElements	Number of elements in returned array
 */
///@{

/**
 * @brief	Opens PINC input ini-file specified in PINC's arguments.
 * @brief	argc	Argument count (as passed to PINC)
 * @param	argv	Arugment vector (as passed to PINC)
 * @return	Allocated iniparser dictionary holding data from ini-file
 *
 * Performs sanity check on argc and argv and opens the input file specified in
 * argv[1]. It also empties all files specified in msgfiles.
 *
 * Following arguments are used to override settings from the input file, which
 * may be useful if running parameter sweeps from external scripts. E.g. if
 * argv[2]=="grid:nSubdomains=2,2,2" the value of this key is overriden in
 * the returned dictionary. Any number of such arguments can be accepted, and
 * in any order, but the key must already exist in the input file (a
 * restriction inherited from the underlying iniparser library).
 *
 * The special argument "getnp" is used to get the number of MPI processes PINC
 * requires within an external script, for instance to know which "-np"
 * argument to pass to mpirun. When PINC stumbles upon "getnp" it immediately
 * terminates after returning the requested number, even before processing
 * further arguments. Since changing settings (i.e. "grid:nSubdomains") may
 * change the required number of processes, "getnp" should always be the last
 * argument.
 *
 * Close dictionary using iniClose() after use.
 */
dictionary* iniOpen(int argc, char *argv[]);

///@brief Close dictionary
void iniClose(dictionary *ini);

///@brief Get integer
int iniGetInt(const dictionary* ini, const char *key);
///@brief Get long int
long int iniGetLongInt(const dictionary* ini, const char *key);
///@brief Get double
double iniGetDouble(const dictionary* ini, const char *key);
///@brief Allocate and get string (remeber to free)
char* iniGetStr(const dictionary *ini, const char *key);

///@brief Allocate and get array of integers (remember to free)
int* iniGetIntArr(const dictionary *ini, const char *key, int *nElements);
///@brief Allocate and get array of long ints (remember to free)
long int* iniGetLongIntArr(const dictionary *ini, const char *key, int *nElements);
///@brief Allocate and get array of doubles (remember to free)
double* iniGetDoubleArr(const dictionary *ini, const char *key, int *nElements);

/**
 * @brief Get the array of strings associated to a key.
 * @param			ini			Dictionary to search
 * @param			key			Key string to look for
 * @param[out]		nElements	Number of elements in returned array
 * @return			NULL-terminated array of NULL-terminated strings
 *
 * Output is similar to listToStrArr(). Remember to free resulting string array
 * using freeStrArr().
 */
char** iniGetStrArr(const dictionary *ini, const char *key, int *nElements);

///@brief Get the number of elements in an array/comma-separated list
int iniGetNElements(const dictionary* ini, const char* key);

/**
 * @brief Assert that a number of entries are arrays of equal length.
 * @param			ini			Dictionary to search
 * @param			nKey		Number of keys to search for
 * @param			...			Keys to search for
 * @return			Number of elements in arrays
 * @see				iniGetNElements()
 *
 * Example:
 * @code
 *	iniAssertEqualNElements(ini,3,"mySec:a","mySec:b","mySec:c");
 * @endcode
 *
 * This code does nothing if the specified entries have equal length. Unequal
 * lengths will lead to an error.
 */
int iniAssertEqualNElements(const dictionary *ini, int nKeys, ...);

///@}

/**
 * @brief Frees dynamically allocated NULL-terminated array of NULL-terminated strings
 * @param	strArr	Pointer to array of strings
 * @return	void
 * @see listToStrArr(), iniGetStrArr()
 */
void freeStrArr(char** strArr);

/**
 * @brief Open (or create) an .h5-file
 * @param	ini		Input file dictionary
 * @param	fName	File name
 * @param	fSubExt	File sub-extension
 * @return	HDF5 file identifier
 * @see		H5Fcreate(), H5Fclose()
 *
 * This function opens an existing .h5 file or creates it if it doesn't
 * already exist. It takes the place of H5Fopen() and H5Fcreate() in PINC. The
 * file name is <fName>.<fSubExt>.h5 or possibly <prefix>_<fName>.<fSubExt>.h5
 * (see below).
 *
 * Similarly as .h5 indicates the file type being h5, <fSubExt> indicates _what
 * kind_ of .h5-file it is, e.g. if it is a grid quantity (.grid.h5) or
 * population data (.pop.h5). These kinds are standardized within PINC and have
 * dedicated functions unifying file handling of these data types. fName is a
 * name given by the developer to further specify the _contents_ of the file.
 * E.g. is it charge density (rho.grid.h5) or electric field (E.grid.h5).
 *
 * The file will be stored in the folder specified by "files:output" in the
 * input file. Examples of valid values of "files:output":
 *	- output = data/
 *  - output = ./data/
 *	- output = ../data/
 *	- output = ~/data/
 *	- output = /home/me/data/
 *
 * However, this variable also allows the user to specify a prefix to each file.
 * This is indicated by _not_ ending the variable with '/'. For instance:
 *	- output = prefix
 *	- output = data/prefix
 *
 * This will output files such a "prefix_rho.grid.h5".
 *
 * Parent directories are created unless they already exists.
 *
 * Close return value using H5Fclose().
 */
hid_t openH5File(const dictionary* ini, const char *fName, const char *fSubExt);

/**
 * @brief Sets array of double as attributes in h5-file
 * @param	h5		.h5-file identifier
 * @param	name	Attribute name
 * @param	value	Attribute value
 * @param	size	Size of attribute
 *
 * If the attribute name already exists, the old attributes are overwritten.
 * This function is not capable of setting multi-dimensional or non-double
 * arrays as attributes (that's part of its simplification compared to the
 * functions in the HDF5 library).
 */
void setH5Attr(hid_t h5, const char *name, const double *value, int size);

/**
 * @brief Creates a group in a .h5-file recursively
 * @param	h5		.h5-file identifier
 * @param	name	Group name
 * @return	void
 *
 * createH5Group() handles creating multiple levels of groups recursively unlike
 * H5Gcreate() which will crash ungracefully. It also will not crash if a group
 * already exists. Note that the last group must have a trailing slash.
 *
 * Examples:
 *	- /group/group/dataset	- Will create /group/group
 *	- /group/group/		 	- Will create /group/group
 */
void createH5Group(hid_t h5, const char *name);

/**
 * @brief Creates a .xy.h5-file for storing (x,y) datasets
 * @param	ini		Dictionary to input file
 * @param	name	File name
 * @return	Handle for H5-file
 *
 * For conventions regarding the file name, see openH5File().
 * See xyWriteH5() for how to write (x,y) datapoits to the file.
 * Remember to close using xyCloseH5() or PINC will fail ungracefully.
 */
hid_t xyOpenH5(const dictionary *ini, const char *fName);

/**
 * @brief Closes a .xy.h5-file
 * @param	h5		Identifier to h5-file to close
 * @param			void
 */
void xyCloseH5(hid_t h5);

/**
 * @brief Writes an (x,y) datapoint to a dataset in an H5-file
 * @param	h5		.h5-file identifier
 * @param	name	Dataset name
 * @param	x		x-value
 * @param	y		y-value
 * @param	op		MPI reduction operation performed on y
 * @return	void
 *
 * A datapoint on a curve in the xy-plane is appended at the end of a dataset in
 * the h5-file. For instance, the x-axis can represent the time steps and the y
 * axis some kind of energy or error residual.
 *
 * In parallel executions, the y value is reduced across all MPI nodes using the
 * specified MPI reduction operation, for instance MPI_SUM to sum the y-value of
 * all MPI nodes before writing to file. If the x value differs amongst the
 * nodes, the x-value of rank 0 is simply used.
 *
 * The dataset must be created beforehand by calling xyCreateDataset() and the
 * file is created by xyOpenH5(). Remember to close the H5 file using
 * xyCloseH5().
 *
 * Example:
 * @code
 *	hid_t hist = xyOpenH5(ini,"timesweep");
 *
 *	xyCreateDataset(hist,"/energy/potential");
 *	xyCreateDataset(hist,"residual");
 *
 *	for(int n=0;n<N;n++){
 *		...
 *		// Energy summed across nodes using MPI_SUM and stored to file
 *		xyWrite(hist,"/energy/potential",(double)n,energy,MPI_SUM);
 *		xyWrite(hist,"residual",(double)n,res,MPI_SUM);
 *		...
 *	}
 *	xyCloseH5(hist);
 * @endcode
 */
void xyWrite(hid_t h5, const char* name, double x, double y, MPI_Op op);

/**
 * @brief Creates a dataset in a .xy.h5 file
 * @param	h5		Identifier to .h5-file to create dataset in
 * @param	name	Dataset name
 * @return			void
 *
 * Creates a dataset specified by its name and all parent groups with it. For
 * example, see xyWrite().
 */
void xyCreateDataset(hid_t h5, const char *name);




/******************************************************************************
 * DEFINED IN AUX.C
 *****************************************************************************/

/**
 * @name Timer functions
 */
///@{


/**
 * @brief	Allocates a Timer struct
 * @return	Pointer to Timer struct
 * @see		Timer, tFree, tMsg()
 *
 * Remember to free using tFree().
 */
Timer *tAlloc();

/**
 * @brief	Frees a Timer struct allocated with tAlloc()
 * @param	timer 	Pointer to Timer struct
 * @see		Timer, tAlloc()
 */
void tFree(Timer *t);


/**
 * @brief	Starts the time counter
 * @param	timer 	Pointer to Timer struct
 * @see		tStop()
 *
 *	Sets t->start = clock().
 *
 */
void tStart(Timer *t);


/**
 * @brief	Stops the timer stopwatch
 * @param	timer 	Pointer to Timer struct
 * @see		Timer, tStart()
 *
 *	Computes the time, since tStart was called and then the result is adtimerded to the
 *	total time the stopwatch have been running.
 *
 */
void tStop(Timer *t);

/**
 * @brief	Resets the timer
 * @param	timer 	Pointer to Timer struct
 * @see		Timer, tStart()
 */
void tReset(Timer *t);

/**
 * @brief	Prints a message along with a time converted to a sensible format
 * @param	nanoSec 	nanoseconds
 * @see		string		message before the time measurement
 *
 *	Prints the message along with the the time measurement, converted from
 *	nanoseconds to a suitable format.
 *
 * Useful for testing execution speed of chunks of code. Each call to tMsg()
 * prints the total time the program has been running, along with the time since
 * last call to tMsg() before it resets the timer.
 *
 */
void tMsg(long long int nanoSec, const char *string);

//
// /**
//  * @brief	Prints a message along with timing information
//  * @param	timer		Pointer to timer
//  * @param	format		printf-like format specifier
//  * @param	...			printf-like arguments
//  * @return	void
//  * @see 	Timer, allocTimer(), printf()
//  *
//  * Useful for testing execution speed of chunks of code. Each call to tMsg()
//  * prints the total time the program has been running, along with the time since
//  * last call to tMsg() before it resets the timer.
//  *
//  * To reset the timer without printing set format=NULL.
//  *
//  * Only MPI nodes for which the timer is activated by allocTimer() will print
//  * the messages.
//  */
// void tMsg(Timer *timer, const char *restrict format, ...);

///@}

/**
 * @brief Concatenates strings
 * @param	n	Number of strings to concatenate
 * @param	...	List of pointer to strings to concatenate
 * @return		Pointer to concatenated string
 * @see	strcat()
 *
 * Contrary to strcat() this function allocates a new string of suitable length.
 * Make sure to call free().
 */
char *strCatAlloc(int n,...);

/** @name Array functions
 * This group of functions perform simple and common array/vector operations on
 * one array/vector 'a' (unary operations) or two arrays/vectors 'a' and 'b'
 * (binary operations). Some operations may have scalar-valued results, in which
 * case the return value of the function is used, while others have
 * vector-valued results, in which case a variable 'res' must be pre-allocated
 * to hold the result. Example:
 *
 * @code
 *	int n = 5;
 *	int a[] = {1,2,3,4,5};
 *	int b[] = {2,3,4,5,6};
 *	int *res = malloc(n*sizeof(*res));
 *	int s = aiSum(a,n);	// The sum of all elements in a (the scalar 15)
 *	aiAdd(a,b,res,n);		// The sum of a and b (the vector {3,5,7,9,11})
 * @endcode
 *
 * All functions starts with the prefix 'a' (for array) and a second letter
 * signifying the datatype of the arrays:
 *
 *  prefix  | datatype
 *	--------|----------
 *	ad      | double
 *	ai      | int
 *	al      | long int
 *
 * The (scalar-valued) return values of certain 'ai'-functions may still be long
 * int since, for instance, the product of many int's may end up in the long int
 * range. If the result is stored in a plain int the result will be
 * automatically cast to int without emitting a warning. Likewise, the number
 * of elements of the arrays (n) is always of type long int to support large
 * arrays, but using a plain int should cast nicely (strictly speaking, int and
 * long int is the same on most modern architectures).
 *
 * In-place operations are supported, i.e. the 'res' array may very well be the
 * same as one or both of the input array. Example:
 *
 * @code
 *	int n = 3;
 *	int a[] = {1,2,3}
 *	aiMul(a,a,a,n);		// a now equals {1,4,9}
 * @endcode
 *
 * Finally notice that pointer arithmetics along with adjusted values of 'n' may
 * be utilized to affect only parts of an array. Example:
 *
 * @code
 *	int n = 5;
 *	int a[] = {1,1,1,1,1};
 *	aiSetAll(a,n,2);		// a now equals {2,2,2,2,2}
 *	aiSetAll(a+1,3,5);		// a now equals {2,5,5,5,2}
 *	aiSetAll(&a[1],3,7);	// a now equals {2,7,7,7,2}
 * @endcode
 *
 * @param		a		Input array/vector (unary operations)
 * @param		b		Input array/vector (unary and binary operations)
 * @param[out]	res		Resulting array/vector (if result is vector-valued)
 * @param		n		Number of elements in a (and b and typically res)
 * @param		value	Input scalar value to use in operation (if any)
 * @return				Resulting scalar (if result is scalar-valued)
 */
///@{
///@brief Adds two arrays
void adAdd(const double *a, const double *b, double *res, long int n);
///@brief Adds two arrays
void aiAdd(const int *a, const int *b, int *res, long int n);
///@brief Adds two arrays
void alAdd(const long int *a, const long int *b, long int *res, long int n);
///@brief Multiplies two arrays element-wise (Hadamard).
void adMul(const double *a, const double *b, double *res, long int n);
///@brief Multiplies two arrays element-wise (Hadamard).
void aiMul(const int *a, const int *b, int *res, long int n);
///@brief Multiplies two arrays element-wise (Hadamard).
void alMul(const long int *a, const long int *b, long int *res, long int n);
///@brief Shifts an array by a constant value
void adShift(double *a, long int n, double value);
///@brief Shifts an array by a constant value
void aiShift(int *a, long int n, int value);
///@brief Shifts an array by a constant value
void alShift(long int *a, long int n, long int value);
///@brief Returns maximum value in an array
double adMax(const double *a, long int n);
///@brief Returns maximum value in an array
int aiMax(const int *a, long int n);
///@brief Returns maximum value in an array
long int alMax(const long int *a, long int n);
///@brief Returns minimum value in an array
double adMin(const double *a, long int n);
///@brief Returns minimum value in an array
int aiMin(const int *a, long int n);
///@brief Returns minimum value in an array
long int alMin(const long int *a, long int n);
///@brief Returns most significant extremum (maximum or minimum) of an array.
/// For instance, if -6 is the minimum and 5 is the maximum the return value is
/// -6.
double adExt(const double *a, long int n);
///@brief Returns most significant extremum (maximum or minimum) of an array.
/// For instance, if -6 is the minimum and 5 is the maximum the return value is
/// -6.
int aiExt(const int *a, long int n);
///@brief Returns most significant extremum (maximum or minimum) of an array.
/// For instance, if -6 is the minimum and 5 is the maximum the return value is
/// -6.
long int alExt(const long int *a, long int n);
///@brief Returns sum of all elements
double adSum(const double *a, long int n);
///@brief Returns sum of all elements
long int aiSum(const int *a, long int n);
///@brief Returns sum of all elements
long int alSum(const long int *a, long int n);
///@brief Returns average of all elements
double adAvg(const double *a, long int n);
///@brief Returns average of all elements
double aiAvg(const int *a, long int n);
///@brief Returns average of all elements
double alAvg(const long int *a, long int n);
///@brief Returns product of all elements
double adProd(const double *a, long int n);
///@brief Returns product of all elements
long int aiProd(const int *a, long int n);
///@brief Returns product of all elements
long int alProd(const int *a, long int n);
///@brief Returns dot product of vectors
int adDotProd(const double *a, const double *b, long int n);
///@brief Returns dot product of vectors
int aiDotProd(const int *a, const int *b, long int n);
///@brief Returns dot product of vectors
int alDotProd(const long int *a, const long int *b, long int n);
///@brief Returns 1 if arrays are equal, 0 otherwise. Every element must be
/// at maximum 'tol' apart to be considered equal (max-norm).
int adEq(const double *a, const double *b, long int n, double tol);
///@brief Returns 1 if arrays are equal, 0 otherwise
int aiEq(const int *a, const int *b, long int n);
///@brief Returns 1 if arrays are equal, 0 otherwise
int alEq(const long int *a, const long int *b, long int n);
///@brief Determine cumulative product of elements in 'a' starting at 1.
/// Hence the cumulative product of {5,4,3} is {1,5,20,60}. Notice that the
/// result is of lenght n+1 in this case.
void adCumProd(const double *a, double *res, long int n);
///@brief Determine cumulative product of elements in 'a' starting at 1.
/// Hence the cumulative product of {5,4,3} is {1,5,20,60}. Notice that the
/// result is of lenght n+1 in this case.
void aiCumProd(const int *a, int *res, long int n);
///@brief Determine cumulative product of elements in 'a' starting at 1.
/// Hence the cumulative product of {5,4,3} is {1,5,20,60}. Notice that the
/// result is of lenght n+1 in this case. Notice that this function is of mixed
/// datatype: input is int while output is long int to facilitate the possibly
/// larger values of the output. Pointers to arrays of different types do not
/// cast nicely since data may be mis-aligned upon casting.
void ailCumProd(const int *a, long int *res, long int n);
///@brief Determine cumulative product of elements in 'a' starting at 1.
/// Hence the cumulative product of {5,4,3} is {1,5,20,60}. Notice that the
/// result is of lenght n+1 in this case.
void alCumProd(const long int *a, long int *res, long int n);
///@brief Sets all elements in array to 'value'
void adSetAll(double *a, long int n, double value);
///@brief Sets all elements in array to 'value'
void aiSetAll(int *a, long int n, int value);
///@brief Sets all elements in array to 'value'
void alSetAll(long int *a, long int n, long int value);
///@brief Set n elements in array manually, e.g. adSet(a,5,1.,2.,3.,4.,5.);
void adSet(double *a, long int n, ...);
///@brief Set n elements in array manually, e.g. adSet(a,5,1.,2.,3.,4.,5.);
void aiSet(int *a, long int n, ...);
///@brief Set n elements in array manually, e.g. adSet(a,5,1.,2.,3.,4.,5.);
void alSet(long int *a, long int n, ...);
///@brief See adPrint(). varName is the name to output for the variable.
void adPrintInner(double *a, long int n, char *varName);
///@brief See aiPrint(). varName is the name to output for the variable.
void aiPrintInner(int *a, long int n, char *varName);
///@brief See alPrint(). varName is the name to output for the variable.
void alPrintInner(long int *a, long int n, char *varName);
///@brief Prints an array in a nice format (for debugging only).
#define adPrint(a,n) do { adPrintInner(a,n,#a); } while (0)
///@brief Prints an array in a nice format (for debugging only).
#define aiPrint(a,n) do { aiPrintInner(a,n,#a); } while (0)
///@brief Prints an array in a nice format (for debugging only).
#define alPrint(a,n) do { alPrintInner(a,n,#a); } while (0)
///@}

/**
 * @brief Writes grid structs to a parsefile
 * @param ini 		dictionary of the input file
 * @param grid 		grid struct
 *
 * Debug help
 */

void dumpWholeGrid(dictionary *ini, Grid *grid);

void dumpTrueGrid(dictionary *ini, Grid *grid);

#endif // PINC_H
