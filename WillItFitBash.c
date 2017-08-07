///       Will It Fit        ///
///                          ///
///     Copyright 2013,      ///
/// University of Copenhagen ///
///                          ///
///  Martin Cramer Pedersen  ///
///       mcpe@nbi.dk        ///

/// This file is part of WillItFit.                                      ///
///                                                                      ///
/// WillItFit is free software: you can redistribute it and/or modify    ///
/// it under the terms of the GNU General Public License as published by ///
/// the Free Software Foundation, either version 3 of the License, or    ///
/// (at your option) any later version.                                  ///
///                                                                      ///
/// WillItFit is distributed in the hope that it will be useful,         ///
/// but WITHOUT ANY WARRANTY; without even the implied warranty of       ///
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        ///
/// GNU General Public License for more details.                         ///
///                                                                      ///
/// You should have received a copy of the GNU General Public License    ///
/// along with WillItFit. If not, see <http://www.gnu.org/licenses/>.    ///

/// If you use this software in your work, please cite: ///
///                                                     ///
/// Pedersen, M. C., Arleth, L. & Mortensen, K.         ///
/// J. Appl. Cryst. 46(6), 1894-1898                    ///

/// Defines
#define pi 3.14159265
#define EstimatedNumberOfStepsInLikelihoodProfile 10
#define MaxNumberOfConstraints 50
#define MaxSizeOfCardfile 1024

/// Inclusions
// Include build-in libraries
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <complex.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
// Include parallelisation
#include "Auxillary/Parallelisation.h"

// Include supporting headers
#include "Auxillary/Structs.h"
#include "Auxillary/Allocation.h"
#include "Auxillary/AuxillaryFunctions.h"
#include "Auxillary/IncludeUserDefined.h"

// Include mathematical headers
#include "Formfactors/FormfactorDebye.h"
#include "Formfactors/FormfactorHalfBelt.h"
#include "Formfactors/FormfactorSphere.h"
#include "Formfactors/FormfactorCylinder.h"
#include "Formfactors/FormfactorCylinderDisplaced.h"
#include "Formfactors/FormfactorHammouda.h"
#include "Formfactors/FormfactorEllipticCylinderWithEllipsoidalEndcaps.h"
#include "Formfactors/FormfactorRodWithoutEndcaps.h"

// Include I/O
#include "InputOutput/ImportSpectra.h"
#include "InputOutput/ImportParameters.h"
#include "InputOutput/ImportSampleInfo.h"
#include "InputOutput/ImportPDBFile.h"
#include "InputOutput/OutputSpectra.h"
#include "InputOutput/OutputParameters.h"
#include "InputOutput/BetaIO.h"

// Include model
#include "Auxillary/ModelLocation.h"
//#include "Models/Protein/ModelInfo.h" //Locked model for cmd functionality.


// Include functions accounting for instrumental smearing
#include "Resolution/SigmaOfQ.h"
#include "Resolution/Resolution.h"

// Import fitting routines
#include "FittingRoutines/LevenbergMarquardtSupportingFunctions.h"
#include "FittingRoutines/BFGSSupportingFunctions.h"
#include "FittingRoutines/SwarmSupportingFunctions.h"
#include "FittingRoutines/GeneticSupportingFunctions.h"
#include "FittingRoutines/LevenbergMarquardt.h"
#include "FittingRoutines/BFGS.h"
#include "FittingRoutines/ComputeModel.h"
#include "FittingRoutines/Swarm.h"
#include "FittingRoutines/Genetic.h"
#include "FittingRoutines/GridsearchBFGS.h"
#include "FittingRoutines/GridsearchLM.h"
#include "FittingRoutines/ProfileLikelihood.h"
#include "FittingRoutines/ProfileLikelihoodSingleParameter.h"

/// Main program
int main(int argc, char *argv[])
{

    /// Declarations
    // Dummy variables
    int i;
    int j;
    // Variables describing the spectras
    struct Dataset * Data;
    char CardFileLocation[256];

    //ReturnMessage("Program terminated before error catching.");
    int NumberOfSpectra;
    int HighestNumberOfDatapoints;
    int TotalNumberOfDatapoints = 0;

    // Variables describing the sample info
    char SamplesFileLocation[256];
    char PDBFileLocation[256];
    char ResultsDirectory[256];

    int NumberOfSampleInformations;
    double * VolumesOfMolecules;
    struct Protein ProteinStructure;
    struct UserDefined UserDefinedStructure;

    // Variables describing the parameters
    struct Parameter * Parameters;
    char ParameterFileLocation[256];
    int NumberOfParameters = 0;
    int NumberOfFreeParameters = 0;

    // Variables describing the properties of the fit
    double QMin = 0.0;
    double QMax = 1.0;
    double DeltaForDifferentiations = 0.001;
    double ChiSquare;
    double ChiSquareFractile;

    int ChooseFittingRoutine = 0;
    int FittingRoutineArgument1 = 50;
    int FittingRoutineArgument2 = 10;
    int FittingRoutineArgument3 = 32;
    int FittingRoutineError = -1;

    bool PrintCovarianceMatrix = false;
    char Message[64];

    // Variables describing the resolution
    char ResolutionFileLocation[256];

    int NumberOfSmearingFolds = 0;
    int ResolutionError;

    int CalculateModel = 0;

    bool IncludeResolutionEffects = false;
    printf("Arguments will now be assigned.\n");
    /// Setup errorchecking
    /// Obtain arguments from program or request them in console
    AssignArgumentsBash(argc, argv, CardFileLocation, SamplesFileLocation, ParameterFileLocation, &QMin, &QMax,
                    &IncludeResolutionEffects, &NumberOfSmearingFolds, ResolutionFileLocation,
                    PDBFileLocation, &CalculateModel);

    /// Retrieve parameters
    printf("\n");
    printf("Reading initial values of parameters. \n");

    NumberOfParameters = CheckNumberOfParameters(ParameterFileLocation);
    Errorcheck(NumberOfParameters, "reading the parameter-file");

    AllocateParameters(&Parameters, NumberOfParameters);

    ImportParameters(Parameters, ParameterFileLocation);

    for (i = 0; i < NumberOfParameters; ++i) {

        if (Parameters[i].iParameter == true) {
            NumberOfFreeParameters += 1;
        }
    }
    printf("\n");
    printf("Found %d parameters. \n", NumberOfParameters);

    /// Import data from .card-file
    printf("\n");
    ClearScreen();

    HighestNumberOfDatapoints = CheckSizeOfData(CardFileLocation, &NumberOfSpectra);
    Errorcheck(HighestNumberOfDatapoints, "reading the datafiles");

    AllocateData(&Data, NumberOfSpectra);

    for (i = 0; i < NumberOfSpectra; ++i) {
        Initialize1DArray(&Data[i].QValues,           HighestNumberOfDatapoints);
        Initialize1DArray(&Data[i].IValues,           HighestNumberOfDatapoints);
        Initialize1DArray(&Data[i].FitValues,         HighestNumberOfDatapoints);
        Initialize1DArray(&Data[i].SigmaValues,       HighestNumberOfDatapoints);
        Initialize1DArray(&Data[i].SigmaQValues,      HighestNumberOfDatapoints);
        Initialize2DArray(&Data[i].ResolutionWeights, HighestNumberOfDatapoints, NumberOfSmearingFolds);
        Initialize1DArray(&Data[i].Constraints,       MaxNumberOfConstraints);

        Data[i].IncludeResolutionEffects = false;
    }

    ImportSpectra(Data, CardFileLocation, NumberOfSpectra);

    for (i = 0; i < NumberOfSpectra; ++i) {
        TotalNumberOfDatapoints += Data[i].NumberOfDatapoints;
    }

    /// Retrieve sample info
    printf("\n");
    ClearScreen();

    printf("Reading sample compositions. \n");

    NumberOfSampleInformations = CheckSizeOfSampleInformation(SamplesFileLocation);
    Errorcheck(NumberOfSampleInformations, "reading the sample information-file");

    Initialize1DArray(&VolumesOfMolecules, NumberOfSampleInformations);

    for (i = 0; i < NumberOfSpectra; ++i) {
        Initialize1DArray(&Data[i].ScatteringLengths, NumberOfSampleInformations);
    }

  //  ImportSampleInformation(Data, VolumesOfMolecules, SamplesFileLocation, NumberOfSampleInformations, NumberOfSpectra);

    /// Import the PDB-file
    ProteinStructure.NumberOfAtoms = 0;
    if (strcmp(PDBFileLocation, "N/A") != 0) {
        ClearScreen();

        sprintf(ProteinStructure.PDBFileLocation, "%s", PDBFileLocation);

        ProteinStructure.NumberOfResidues = CheckNumberOfResiduesInPDBFile(PDBFileLocation);
        Errorcheck(ProteinStructure.NumberOfResidues, "reading the PDB-file...");

        ProteinStructure.NumberOfAtoms = CheckNumberOfAtomsInPDBFile(PDBFileLocation);
        Errorcheck(ProteinStructure.NumberOfAtoms, "reading the PDB-file...");

        printf("Importing PDB-file. \n");
        printf("Found %d atoms distributed amongst %d residues. \n", ProteinStructure.NumberOfAtoms, ProteinStructure.NumberOfResidues);

        AllocateProteinStructure(&ProteinStructure, ProteinStructure.NumberOfResidues, ProteinStructure.NumberOfAtoms);
    }

    ImportSampleInformation(Data, VolumesOfMolecules, SamplesFileLocation, NumberOfSampleInformations, NumberOfSpectra, &ProteinStructure);
	printf("****  %s\n",ProteinStructure.ModificationName);

   if (strcmp(PDBFileLocation, "N/A") != 0) {
        ImportResiduesFromPDBFile(PDBFileLocation, ProteinStructure, ProteinStructure.NumberOfResidues);
        ImportAtomsFromPDBFile(PDBFileLocation, ProteinStructure, ProteinStructure.NumberOfAtoms);
    }


    /// Decide fitting range and initialize the userdefined structure
    printf("\n");
    ClearScreen();

    AssignFittingRanges(Data, QMin, QMax, NumberOfSpectra);
    InitializeUserDefinedStructure(&UserDefinedStructure);

    /// Include or exclude resolution effects
    printf("\n");
    ClearScreen();

    if (IncludeResolutionEffects == true) {
        printf("Including resolution effects. \n");

        ResolutionError = Resolution(Data, NumberOfSpectra, ResolutionFileLocation, NumberOfSmearingFolds);

        Errorcheck(ResolutionError, "reading the resolution information-file...");
    } else {
        printf("Excluding resolution effects. \n");
    }

    /// Run fitting routine
    printf("\n");
    ClearScreen();


    //FittingRoutineError = LevenbergMarquardt(Data, NumberOfSpectra, Parameters, NumberOfParameters, FittingRoutineArgument1, &ChiSquare, NumberOfSmearingFolds,
    //                                                 VolumesOfMolecules, false, PrintCovarianceMatrix, ProteinStructure, &UserDefinedStructure, DeltaForDifferentiations,
    //                                                 NumberOfSampleInformations, TotalNumberOfDatapoints, NumberOfFreeParameters, HighestNumberOfDatapoints);

    switch (CalculateModel) {
	    case 0:
    		FittingRoutineError = ComputeModel(Data, NumberOfSpectra, Parameters, NumberOfParameters, &ChiSquare, NumberOfSmearingFolds, VolumesOfMolecules, ProteinStructure,
                                               &UserDefinedStructure, TotalNumberOfDatapoints, NumberOfFreeParameters);
    	    break;
    	    case 1:
    FittingRoutineError = LevenbergMarquardt(Data, NumberOfSpectra, Parameters, NumberOfParameters, FittingRoutineArgument1, &ChiSquare, NumberOfSmearingFolds,
                                                     VolumesOfMolecules, false, PrintCovarianceMatrix, ProteinStructure, &UserDefinedStructure, DeltaForDifferentiations,
                                                     NumberOfSampleInformations, TotalNumberOfDatapoints, NumberOfFreeParameters, HighestNumberOfDatapoints);

    }

    Errorcheck(FittingRoutineError, "running the selected fitting routine");

    // Output data and parameters
    printf("\n");
    ClearScreen();
    //Create directory for output
    //This might be platform specific
    sprintf(ResultsDirectory, "%s-results", CardFileLocation);
    mkdir(ResultsDirectory, 0700);

    OutputData(ChiSquare, QMin, QMax, Parameters, NumberOfParameters, Data, NumberOfSpectra, CardFileLocation, ProteinStructure, UserDefinedStructure, SamplesFileLocation, ResultsDirectory);
    OutputSpectra(Data, NumberOfSpectra, ResultsDirectory);
    OutputParameters(Parameters, NumberOfParameters, ChooseFittingRoutine, ChiSquare, ResultsDirectory);
    /// Conclusion
    printf("\n");
    ClearScreen();

    printf("Fit complete...! \n");
    printf("\n");

    ClearScreen();

    /// Free variables
    // Data
    for (i = 0; i < NumberOfSpectra; ++i) {
        free(Data[i].QValues);
        free(Data[i].IValues);
        free(Data[i].FitValues);
        free(Data[i].SigmaValues);
        free(Data[i].SigmaQValues);
        free(Data[i].Constraints);
        free(Data[i].ScatteringLengths);

        for (j = 0; j < HighestNumberOfDatapoints; ++j) {
            free(Data[i].ResolutionWeights[j]);
        }

        free(Data[i].ResolutionWeights);
    }

    free(Data);

    // Free other variables
    free(VolumesOfMolecules);
    free(Parameters);
    FreeUserDefined(&UserDefinedStructure);

    if (strcmp(PDBFileLocation, "N/A") != 0) {
        free(ProteinStructure.Residues);
        free(ProteinStructure.Atoms);
    }

    /// Return the chisquare if the algorithm executes correctly
    sprintf(Message, "%g", ChiSquare);
    //ReturnMessage(Message);

    return 0;
}