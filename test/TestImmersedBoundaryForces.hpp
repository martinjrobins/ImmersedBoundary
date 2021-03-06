/*

Copyright (c) 2005-2016, University of Oxford.
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

// Needed for test framework
#include <cxxtest/TestSuite.h>

#include "CheckpointArchiveTypes.hpp"
#include "FileComparison.hpp"
#include "CellsGenerator.hpp"
#include "UniformlyDistributedCellCycleModel.hpp"

// Includes from projects/ImmersedBoundary
#include "ImmersedBoundaryCellCellInteractionForce.hpp"
#include "ImmersedBoundaryMembraneElasticityForce.hpp"
#include "ImmersedBoundaryPalisadeMeshGenerator.hpp"

// This test is never run in parallel
#include "FakePetscSetup.hpp"

class TestImmersedBoundaryForces : public CxxTest::TestSuite
{
public:

    void TestImmersedBoundaryCellCellInteractionForceMethods() throw (Exception)
    {
        ///\todo Test this class
    }

    void TestArchivingOfImmersedBoundaryCellCellInteractionForce() throw (Exception)
    {
        EXIT_IF_PARALLEL; // Beware of processes overwriting the identical archives of other processes
        OutputFileHandler handler("archive", false);
        std::string archive_filename = handler.GetOutputDirectoryFullPath() + "ImmersedBoundaryCellCellInteractionForce.arch";

        {
            ImmersedBoundaryCellCellInteractionForce<2> force;

            std::ofstream ofs(archive_filename.c_str());
            boost::archive::text_oarchive output_arch(ofs);

            // Set member variables
            force.SetSpringConstant(1.2);
            force.SetRestLength(3.4);
            force.UseMorsePotential();

            // Serialize via pointer to most abstract class possible
            AbstractImmersedBoundaryForce<2>* const p_force = &force;
            output_arch << p_force;
        }

        {
            AbstractImmersedBoundaryForce<2>* p_force;

            // Create an input archive
            std::ifstream ifs(archive_filename.c_str(), std::ios::binary);
            boost::archive::text_iarchive input_arch(ifs);

            // Restore from the archive
            input_arch >> p_force;

            // Check member variables have been correctly archived
            TS_ASSERT_DELTA(static_cast<ImmersedBoundaryCellCellInteractionForce<2>*>(p_force)->GetSpringConstant(), 1.2, 1e-6);
            TS_ASSERT_DELTA(static_cast<ImmersedBoundaryCellCellInteractionForce<2>*>(p_force)->GetRestLength(), 3.4, 1e-6);
            TS_ASSERT_EQUALS(static_cast<ImmersedBoundaryCellCellInteractionForce<2>*>(p_force)->IsLinearSpringLaw(), false);
            TS_ASSERT_EQUALS(static_cast<ImmersedBoundaryCellCellInteractionForce<2>*>(p_force)->IsMorsePotential(), true);

            // Tidy up
            delete p_force;
        }
    }

    void TestImmersedBoundaryMembraneElasticityForce() throw (Exception)
    {
        ///\todo Test this class
    }

    void TestArchivingOfImmersedBoundaryMembraneElasticityForce() throw (Exception)
    {
        EXIT_IF_PARALLEL; // Beware of processes overwriting the identical archives of other processes
        OutputFileHandler handler("archive", false);
        std::string archive_filename = handler.GetOutputDirectoryFullPath() + "ImmersedBoundaryMembraneElasticityForce.arch";

        {
            ImmersedBoundaryMembraneElasticityForce<2> force;

            std::ofstream ofs(archive_filename.c_str());
            boost::archive::text_oarchive output_arch(ofs);

            // Set member variables
            force.SetSpringConstant(1.2);
            force.SetRestLengthMultiplier(7.8);

            // Serialize via pointer to most abstract class possible
            AbstractImmersedBoundaryForce<2>* const p_force = &force;
            output_arch << p_force;
        }

        {
            AbstractImmersedBoundaryForce<2>* p_force;

            // Create an input archive
            std::ifstream ifs(archive_filename.c_str(), std::ios::binary);
            boost::archive::text_iarchive input_arch(ifs);

            // Restore from the archive
            input_arch >> p_force;

            // Check member variables have been correctly archived
            TS_ASSERT_DELTA(static_cast<ImmersedBoundaryMembraneElasticityForce<2>*>(p_force)->GetSpringConstant(), 1.2, 1e-6);
            TS_ASSERT_DELTA(static_cast<ImmersedBoundaryMembraneElasticityForce<2>*>(p_force)->GetRestLengthMultiplier(), 7.8, 1e-6);

            // Tidy up
            delete p_force;
        }
    }

    void TestImmersedBoundaryForceOutputParameters()
    {
        EXIT_IF_PARALLEL;
        std::string output_directory = "TestForcesOutputParameters";
        OutputFileHandler output_file_handler(output_directory, false);

        // Test with ImmersedBoundaryCellCellInteractionForce
        ImmersedBoundaryCellCellInteractionForce<2> cell_cell_force;
        cell_cell_force.SetSpringConstant(1.2);
        cell_cell_force.SetRestLength(3.4);
        cell_cell_force.UseMorsePotential();

        TS_ASSERT_EQUALS(cell_cell_force.GetIdentifier(), "ImmersedBoundaryCellCellInteractionForce-2");

        out_stream cell_cell_force_parameter_file = output_file_handler.OpenOutputFile("ImmersedBoundaryCellCellInteractionForce.parameters");
        cell_cell_force.OutputImmersedBoundaryForceParameters(cell_cell_force_parameter_file);
        cell_cell_force_parameter_file->close();

        {
            FileFinder generated_file = output_file_handler.FindFile("ImmersedBoundaryCellCellInteractionForce.parameters");
            FileFinder reference_file("projects/ImmersedBoundary/test/data/TestImmersedBoundaryForces/ImmersedBoundaryCellCellInteractionForce.parameters",
                                      RelativeTo::ChasteSourceRoot);
            FileComparison comparer(generated_file,reference_file);
            TS_ASSERT(comparer.CompareFiles());
        }

        // Test with ImmersedBoundaryMembraneElasticityForce
        ImmersedBoundaryMembraneElasticityForce<2> membrane_force;
        membrane_force.SetSpringConstant(1.2);
        membrane_force.SetRestLengthMultiplier(7.8);

        TS_ASSERT_EQUALS(membrane_force.GetIdentifier(), "ImmersedBoundaryMembraneElasticityForce-2");

        out_stream membrane_force_parameter_file = output_file_handler.OpenOutputFile("ImmersedBoundaryMembraneElasticityForce.parameters");
        membrane_force.OutputImmersedBoundaryForceParameters(membrane_force_parameter_file);
        membrane_force_parameter_file->close();

        {
            FileFinder generated_file = output_file_handler.FindFile("ImmersedBoundaryMembraneElasticityForce.parameters");
            FileFinder reference_file("projects/ImmersedBoundary/test/data/TestImmersedBoundaryForces/ImmersedBoundaryMembraneElasticityForce.parameters",
                                      RelativeTo::ChasteSourceRoot);
            FileComparison comparer(generated_file,reference_file);
            TS_ASSERT(comparer.CompareFiles());
        }

        // For coverage of AbstractImmersedBoundaryForce::OutputImmersedBoundaryForceInfo()
        out_stream other_file = output_file_handler.OpenOutputFile("other_file.parameters");
        membrane_force.OutputImmersedBoundaryForceInfo(other_file);
        other_file->close();
    }
};
