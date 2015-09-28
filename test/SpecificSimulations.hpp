/*

Copyright (c) 2005-2014, University of Oxford.
All rights reserved.

University of Oxford means the Chancellor, Masters and Scholars of the
University of Oxford, having an administrative office at Wellington
Square, Oxford OX1 2JD, UK.

This file is part of Chaste.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 * Neither the name of the University of Oxford nor the names of its
   contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

// Needed for the test environment
#include <cxxtest/TestSuite.h>
#include "AbstractCellBasedTestSuite.hpp"

// External library - not part of Chaste
#include <fftw3.h>

// External library - included in Chaste
#include "boost/lexical_cast.hpp"

#include <sys/stat.h>
#include "CheckpointArchiveTypes.hpp"


#include "OffLatticeSimulation.hpp"
#include "StochasticDurationCellCycleModel.hpp"
#include "Timer.hpp"

#include "CellsGenerator.hpp"
#include "ImmersedBoundaryMesh.hpp"
#include "ImmersedBoundaryMeshWriter.hpp"
#include "ImmersedBoundaryMeshReader.hpp"
#include "ImmersedBoundarySimulationModifier.hpp"
#include "ImmersedBoundaryPalisadeMeshGenerator.hpp"
#include "SuperellipseGenerator.hpp"

// User project fcooper
#include "CsvWriter.hpp"
#include "Debug.hpp"

// Simulations do not run in parallel
#include "FakePetscSetup.hpp"

class SpecificSimulations : public AbstractCellBasedTestSuite
{
private:

    /** Directory to output csv data to */
    std::string mOutputDirectory;

    /** Globally accessible timer */
    Timer mTimer;

public:

    void SimulationWithVariableGridSpacing(unsigned num_grid_pts)
    {
//        AbstractCellBasedTestSuite::setUp();

        /*
         * 1: Num cells
         * 2: Num nodes per cell
         * 3: Superellipse exponent
         * 4: Superellipse aspect ratio
         * 5: Random y-variation
         * 6: Include membrane
         */
        ImmersedBoundaryPalisadeMeshGenerator gen(11, 200, 0.2, 2.0, 1.0, true);
        ImmersedBoundaryMesh<2,2>* p_mesh = gen.GetMesh();

        p_mesh->GetMembraneElement()->SetMembraneSpringConstant(400000.0);
        p_mesh->GetMembraneElement()->SetMembraneRestLength(0.4/100.0);

        p_mesh->SetNumGridPtsXAndY(num_grid_pts);

        std::vector<CellPtr> cells;
        MAKE_PTR(DifferentiatedCellProliferativeType, p_diff_type);
        CellsGenerator<StochasticDurationCellCycleModel, 2> cells_generator;
        cells_generator.GenerateBasicRandom(cells, p_mesh->GetNumElements(), p_diff_type);

        ImmersedBoundaryCellPopulation<2> cell_population(*p_mesh, cells);

        OffLatticeSimulation<2> simulator(cell_population);

        // Add main immersed boundary simulation modifier
        MAKE_PTR(ImmersedBoundarySimulationModifier<2>, p_main_modifier);
        simulator.AddSimulationModifier(p_main_modifier);

        std::string output_name = "ImmersedBoundaryNumerics/Test";
        output_name += boost::lexical_cast<std::string>(num_grid_pts);
        output_name += "GridPts";

        // Set simulation properties
        simulator.SetOutputDirectory(output_name);
        simulator.SetDt(0.001);
        simulator.SetSamplingTimestepMultiple(10);
        simulator.SetEndTime(0.1);

        // Run and time the simulation
        mTimer.Reset();
        simulator.Solve();
        double simulation_time = mTimer.GetElapsedTime();

        PRINT_VARIABLE(simulation_time);
    }
};