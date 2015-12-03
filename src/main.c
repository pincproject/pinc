/**
 * @file	    main.c
 * @author	    Sigvald Marholm <sigvaldm@fys.uio.no>,
 *				Gullik Vetvik Killie <gullikvk@fys.uio.no>
 * @copyright   University of Oslo, Norway
 * @brief	    PINC main routine.
 * @date        08.10.15
 *
 * Main routine for PINC (Particle-IN-Cell).
 * Replaces old DiP3D main.c file by Wojciech Jacek Miloch.
 */

#include <gsl/gsl_rng.h>
#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <hdf5.h>

#include "pinc.h"
#include "iniparser.h"
#include "multigrid.h"
#include "test.h"

int main(int argc, char *argv[]){


	/*
	 * INITIALIZE THIRD PARTY LIBRARIES
	 */
	MPI_Init(&argc,&argv);
	msg(STATUS|ONCE,"PINC started.");
	MPI_Barrier(MPI_COMM_WORLD);
	Timer *timer = allocTimer(0);

	/*
	 * INITIALIZE PINC VARIABLES
	 */
	dictionary *ini = iniOpen(argc,argv);
	Population *pop = allocPopulation(ini);
	// Grid *grid = allocGrid(ini);
	// GridQuantity *gridQuantity = allocGridQuantity(ini, grid, 2);
	MpiInfo *mpiInfo = allocMpiInfo(ini);
	gsl_rng *rng = gsl_rng_alloc(gsl_rng_mt19937);
	tMsg(timer,"Initialized structures");

	/*
	 *	TEST AREA
	 */
	// posDebug(ini,pop);

	posUniform(ini,pop,mpiInfo,rng);
	tMsg(timer,"Assigned position");
	// gridValDebug(gridQuantity,mpiInfo);
	gsl_rng_set(rng,mpiInfo->mpiRank);
	velMaxwell(ini,pop,rng);
	tMsg(timer,"Assigned velocity");

	// int nValues = gridQuantity->nValues;
	// double *denorm = malloc(nValues*sizeof(*denorm));
	// double *dimen = malloc(nValues*sizeof(*dimen));
	// for(int d=0;d<nValues;d++){
	// 	denorm[d] = 1000+(double)d/10;
	// 	dimen[d] = 100+d+(double)d/10;
	// }
	//
	// createGridQuantityH5(ini,gridQuantity,mpiInfo,denorm,dimen,"rho");
	createPopulationH5(ini,pop,mpiInfo,"pop");
	tMsg(timer,"Created H5-file");

	int N=3;
	for(int n=0;n<N;n++){
		// writeGridQuantityH5(gridQuantity,mpiInfo,(double)n);
		writePopulationH5(pop,mpiInfo,(double)n,(double)n+0.5);
	}
	tMsg(timer,"Stored to H5-file");

	// closeGridQuantityH5(gridQuantity);
	closePopulationH5(pop);

	/*
	 * FINALIZE PINC VARIABLES
	 */

	freeMpiInfo(mpiInfo);
	freePopulation(pop);
	// freeGrid(grid);
	// freeGridQuantity(gridQuantity);
	iniparser_freedict(ini);
	gsl_rng_free(rng);
	tMsg(timer,"freeing structs");

	/*
	 * FINALIZE THIRD PARTY LIBRARIES
	 */

	MPI_Barrier(MPI_COMM_WORLD);
	msg(STATUS|ONCE,"PINC completed successfully!"); // Needs MPI
	MPI_Finalize();

	return 0;
}
