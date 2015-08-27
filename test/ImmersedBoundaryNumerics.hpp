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

class ImmseredBoundaryNumerics : public AbstractCellBasedTestSuite
{
private:

    /** Directory to output csv data to */
    std::string mOutputDirectory;

    /** Globally accessible timer */
    Timer mTimer;

public:

    void TestSetOutputDirectory() throw(Exception)
    {
        /*
         * This function decides on the correct output directory depending on which computer I'm working on
         */
        std::vector<std::string> candidate_directories;

        // Path within Dropbox
        std::string dropbox = "Dropbox/DPhil/chaste/ib_nemerics/csv/";

        // Maths machine
        candidate_directories.push_back("/home/cooper/" + dropbox);

        // Laptop
        candidate_directories.push_back("/media/fergus/Storage/" + dropbox);

        // Verify one candidate is correct, and set member variable accordingly
        struct stat sb;
        for (unsigned idx = 0 ; idx < candidate_directories.size() ; idx++)
        {
            if (stat(candidate_directories[idx].c_str(), &sb) == 0 && S_ISDIR(sb.st_mode))
            {
                mOutputDirectory = candidate_directories[idx];
            }
        }

        if (mOutputDirectory.empty())
        {
            EXCEPTION("No specified output directory was valid");
        }
    }

    void TestSingleCellVolumeChangeWithNodeSpacing() throw(Exception)
    {
        /**
         * This test relaxes a single circular cell for a fixed simulation time.
         *
         * All parameters are fixed except the number of nodes, and the following are exported to a csv file:
         *  * Number of nodes in the cell
         *  * Node spacing to mesh spacing ratio
         *  * Change in volume as ratio: absolute change / initial volume
         *  * Computation time for the simulation
         */

        std::vector<unsigned> num_nodes_vec;
        std::vector<double> node_spacing_ratio;
        std::vector<double> volume_change;
        std::vector<double> computation_time;

        unsigned initial_num_nodes = 20;
        unsigned num_nodes = initial_num_nodes;
        unsigned max_sim_idx = 40;

        for (unsigned sim_idx = 1 ; sim_idx < max_sim_idx ; sim_idx++)
        {
            SimulationTime::Instance()->Destroy();
            SimulationTime::Instance()->SetStartTime(0.0);

            /*
             * Create an Immersed Boundary Mesh using a SuperellipseGenerator
             *
             * 1: Num nodes
             * 2: Superellipse exponent
             * 3: Width
             * 4: Height
             * 5: Bottom-left x
             * 6: Botton-left y
             */
            SuperellipseGenerator* p_gen = new SuperellipseGenerator(num_nodes, 1.0, 0.4, 0.4, 0.3, 0.3);

            // Generate a mesh using this superellipse
            std::vector<c_vector<double, 2> > locations = p_gen->GetPointsAsVectors();
            delete p_gen;

            std::vector<Node<2>* > nodes;
            for (unsigned node_idx = 0 ; node_idx < locations.size() ; node_idx++)
            {
                nodes.push_back(new Node<2>(node_idx, locations[node_idx], true));
            }

            std::vector<ImmersedBoundaryElement<2,2>* > elements;
            elements.push_back(new ImmersedBoundaryElement<2,2>(0, nodes));

            ImmersedBoundaryMesh<2,2> mesh(nodes, elements, 128, 128);

            double mesh_spacing = mesh.GetCharacteristicNodeSpacing();

            /*
             * Set up cell population
             */
            std::vector<CellPtr> cells;
            MAKE_PTR(DifferentiatedCellProliferativeType, p_diff_type);
            CellsGenerator<StochasticDurationCellCycleModel, 2> cells_generator;
            cells_generator.GenerateBasicRandom(cells, mesh.GetNumElements(), p_diff_type);

            ImmersedBoundaryCellPopulation <2> cell_population(mesh, cells);


            ImmersedBoundaryElement <2, 2> *p_elem = mesh.GetElement(0u);
            p_elem->SetMembraneRestLength(0.1 * mesh.GetCharacteristicNodeSpacing());
            p_elem->SetMembraneSpringConstant(1e4);

            double vol_at_t0 = mesh.GetVolumeOfElement(0);
            double node_spacing = mesh.GetSurfaceAreaOfElement(0) / p_elem->GetNumNodes();

            OffLatticeSimulation<2> *p_simulator = new OffLatticeSimulation<2>(cell_population);

            // Add main immersed boundary simulation modifier
            MAKE_PTR(ImmersedBoundarySimulationModifier < 2 >, p_main_modifier);
            p_simulator->AddSimulationModifier(p_main_modifier);

            std::string output_dir = "ImmersedBoundaryNumerics/TestSingleCellVolumeChangeWithNodeSpacing";
            output_dir += boost::lexical_cast<std::string>(num_nodes);

            // Set simulation properties
            p_simulator->SetOutputDirectory(output_dir);
            p_simulator->SetDt(0.01);
            p_simulator->SetSamplingTimestepMultiple(1000);
            p_simulator->SetEndTime(10.0);

            // Run and time the simulation
            mTimer.Reset();            
            p_simulator->Solve();
            computation_time.push_back(mTimer.GetElapsedTime());

            double vol_at_t1 = mesh.GetVolumeOfElement(0);

            num_nodes_vec.push_back(num_nodes);
            node_spacing_ratio.push_back(node_spacing / mesh_spacing);
            volume_change.push_back(fabs(vol_at_t0 - vol_at_t1) / vol_at_t0);


            mesh.GetSurfaceAreaOfElement(0);

            unsigned new_num_nodes = unsigned(ceil(double(initial_num_nodes) / (1.0 - (1.0 / double(max_sim_idx)) * double(sim_idx))));

            while (new_num_nodes == num_nodes)
            {
                sim_idx++;
                new_num_nodes = unsigned(ceil(double(initial_num_nodes) / (1.0 - (1.0 / double(max_sim_idx)) * double(sim_idx))));
            }

            num_nodes = new_num_nodes;

            delete p_simulator;
        }

        CsvWriter csv_writer;
        csv_writer.AddData(num_nodes_vec);
        csv_writer.AddData(node_spacing_ratio);
        csv_writer.AddData(volume_change);
        csv_writer.AddData(computation_time);

        std::vector<std::string> header_names;
        header_names.push_back("Number of Nodes");
        header_names.push_back("Node spacing ratio");
        header_names.push_back("Volume change ratio");
        header_names.push_back("Computation time (s)");
        csv_writer.AddHeaders(header_names);

        csv_writer.SetDirectoryName(mOutputDirectory);
        csv_writer.SetFileName("TestSingleCellVolumeChangeWithNodeSpacing");

        csv_writer.WriteDataToFile();
    }

    void xTestSingleCellVolumeChangeWithSimulationTime() throw(Exception)
    {
        /**
         * This test tracks the change in volume of a single cell at various timepoints of a single simulation.
         *
         * All parameters are fixed, and the following are exported to a csv file:
         *  * Simulation time
         *  * Change in volume as ratio: absolute change / initial volume
         *  * Computation time of each portion of the simulation
         */

        std::vector<double> volume_change;
        std::vector<double> simulation_time;
        std::vector<double> computation_time;

        unsigned num_nodes = 200;
        unsigned max_sim_idx = 40;

        /*
         * Create an Immersed Boundary Mesh using a SuperellipseGenerator
         *
         * 1: Num nodes
         * 2: Superellipse exponent
         * 3: Width
         * 4: Height
         * 5: Bottom-left x
         * 6: Botton-left y
         */
        SuperellipseGenerator *p_gen = new SuperellipseGenerator(num_nodes, 1.0, 0.4, 0.4, 0.3, 0.3);

        // Generate a mesh using this superellipse
        std::vector<c_vector<double, 2> > locations = p_gen->GetPointsAsVectors();
        delete p_gen;

        std::vector<Node<2>* > nodes;
        for (unsigned node_idx = 0 ; node_idx < locations.size() ; node_idx++)
        {
            nodes.push_back(new Node<2>(node_idx, locations[node_idx], true));
        }

        std::vector<ImmersedBoundaryElement<2,2>* > elements;
        elements.push_back(new ImmersedBoundaryElement<2,2>(0, nodes));

        ImmersedBoundaryMesh<2,2> mesh(nodes, elements, 128, 128);

        double mesh_spacing = mesh.GetCharacteristicNodeSpacing();

        /*
         * Set up cell population
         */
        std::vector<CellPtr> cells;
        MAKE_PTR(DifferentiatedCellProliferativeType, p_diff_type);
        CellsGenerator<StochasticDurationCellCycleModel, 2> cells_generator;
        cells_generator.GenerateBasicRandom(cells, mesh.GetNumElements(), p_diff_type);

        ImmersedBoundaryCellPopulation <2> cell_population(mesh, cells);


        ImmersedBoundaryElement <2, 2> *p_elem = mesh.GetElement(0u);
        p_elem->SetMembraneRestLength(0.1 * mesh.GetCharacteristicNodeSpacing());
        p_elem->SetMembraneSpringConstant(1e4);

        double vol_at_t0 = mesh.GetVolumeOfElement(0);
        double node_spacing = mesh.GetSurfaceAreaOfElement(0) / p_elem->GetNumNodes();

        OffLatticeSimulation<2> *p_simulator = new OffLatticeSimulation<2>(cell_population);

        // Add main immersed boundary simulation modifier
        MAKE_PTR(ImmersedBoundarySimulationModifier < 2 >, p_main_modifier);
        p_simulator->AddSimulationModifier(p_main_modifier);

        std::string output_dir = "ImmersedBoundaryNumerics/TestSingleCellVolumeChangeWithSimulationTime";
        output_dir += boost::lexical_cast<std::string>(num_nodes);

        p_simulator->SetOutputDirectory(output_dir);
        p_simulator->SetDt(0.01);
        p_simulator->SetSamplingTimestepMultiple(1000);

        for (unsigned sim_idx = 1 ; sim_idx < max_sim_idx ; sim_idx++)
        {
            // Set changing simulation properties

            p_simulator->SetEndTime(10.0 * sim_idx);

            // Run and time the simulation
            mTimer.Reset();
            p_simulator->Solve();
            computation_time.push_back(mTimer.GetElapsedTime());

            simulation_time.push_back(10.0 * sim_idx);

            double vol_at_t1 = mesh.GetVolumeOfElement(0);

            volume_change.push_back(fabs(vol_at_t0 - vol_at_t1) / vol_at_t0);
        }

        delete p_simulator;

        CsvWriter csv_writer;
        csv_writer.AddData(simulation_time);
        csv_writer.AddData(volume_change);
        csv_writer.AddData(computation_time);

        std::vector<std::string> header_names;
        header_names.push_back("Simulation time (h)");
        header_names.push_back("Volume change ratio");
        header_names.push_back("Computation time (s)");
        csv_writer.AddHeaders(header_names);

        csv_writer.SetDirectoryName(mOutputDirectory);
        csv_writer.SetFileName("TestSingleCellVolumeChangeWithSimulationTime");

        csv_writer.WriteDataToFile();
    }

    void xTestSingleCellRelaxation() throw(Exception)
    {
        /**
         * This test relaxes a single elliptical cell from an ellipse to a circle, in a fixed simulation time.
         *
         * A second cell is then simulated with twice the node-density, and the spring constant in membrane-membrane
         * interactions is varied.  Membrane-membrane rest length is set at 10% of node spacing in both cases.
         *
         * All other parameters are fixed, and the following are exported to a csv file:
         *  * Simulation time
         *  * Reference-cell aspect ratio
         *  * Cell aspect ratio: major axis / minor axis, for each variation in spring constant
         */

        std::vector<double> simulation_time;
        std::vector<double> ref_aspect_ratio;
        std::vector<std::vector<double> > observed_aspect_ratios;

        unsigned num_sim_timesteps = 40;

        double ref_spring_const = 1e4;

        /*
         * Create an Immersed Boundary Mesh using a SuperellipseGenerator
         *
         * 1: Num nodes
         * 2: Superellipse exponent
         * 3: Width
         * 4: Height
         * 5: Bottom-left x
         * 6: Botton-left y
         */
        SuperellipseGenerator* p_gen = new SuperellipseGenerator(128, 1.0, 0.3, 0.6, 0.35, 0.2);

        // Generate a mesh using this superellipse
        std::vector<c_vector<double, 2> > locations = p_gen->GetPointsAsVectors();
        delete p_gen;

        std::vector<Node<2>* > nodes;
        for (unsigned node_idx = 0 ; node_idx < locations.size() ; node_idx++)
        {
            nodes.push_back(new Node<2>(node_idx, locations[node_idx], true));
        }

        std::vector<ImmersedBoundaryElement<2,2>* > elements;
        elements.push_back(new ImmersedBoundaryElement<2,2>(0, nodes));

        ImmersedBoundaryMesh<2,2> mesh(nodes, elements, 64, 64);
        
         // Set up cell population
        std::vector<CellPtr> cells;
        MAKE_PTR(DifferentiatedCellProliferativeType, p_diff_type);
        CellsGenerator<StochasticDurationCellCycleModel, 2> cells_generator;
        cells_generator.GenerateBasicRandom(cells, mesh.GetNumElements(), p_diff_type);
        ImmersedBoundaryCellPopulation <2> cell_population(mesh, cells);

        // Set element properties
        ImmersedBoundaryElement<2, 2>* p_elem = mesh.GetElement(0u);
        p_elem->SetMembraneRestLength(0.1 * mesh.GetCharacteristicNodeSpacing());
        p_elem->SetMembraneSpringConstant(ref_spring_const);

        // Create simulation
        OffLatticeSimulation<2> *p_simulator = new OffLatticeSimulation<2>(cell_population);

        // Add main immersed boundary simulation modifier
        MAKE_PTR(ImmersedBoundarySimulationModifier < 2 >, p_main_modifier);
        p_simulator->AddSimulationModifier(p_main_modifier);

        std::string output_dir = "ImmersedBoundaryNumerics/TestSingleCellRelaxation";
        output_dir += boost::lexical_cast<std::string>(p_elem->GetMembraneSpringConstant() / ref_spring_const);

        // Set simulation properties
        p_simulator->SetOutputDirectory(output_dir);
        p_simulator->SetDt(0.01);
        p_simulator->SetSamplingTimestepMultiple(10);

        // Calculate aspect ratio - this relies on the cell staying symmetric
        c_vector<double, 2> long_axis = p_elem->GetNode(32)->rGetLocation() - p_elem->GetNode(96)->rGetLocation();
        c_vector<double, 2> short_axis = p_elem->GetNode(0)->rGetLocation() - p_elem->GetNode(64)->rGetLocation();

        // Store initial data point, and run simulation for all required timesteps
        simulation_time.push_back(0.0);
        ref_aspect_ratio.push_back(norm_2(long_axis) / norm_2(short_axis));

        for (unsigned sim_time_idx = 1 ; sim_time_idx <= num_sim_timesteps ; sim_time_idx++)
        {
            // Calculate new end time and run simulation
            double new_end_time = 0.1 * double(sim_time_idx);
            p_simulator->SetEndTime(new_end_time);
            p_simulator->Solve();

            // Calculate aspect ratio and store data
//            c_vector<double, 2> long_axis = p_elem->GetNode(32)->rGetLocation() - p_elem->GetNode(96)->rGetLocation();
//            c_vector<double, 2> short_axis = p_elem->GetNode(0)->rGetLocation() - p_elem->GetNode(64)->rGetLocation();

            simulation_time.push_back(new_end_time);
            ref_aspect_ratio.push_back(mesh.GetElongationShapeFactorOfElement(0));
        }

        delete p_gen;
        delete p_simulator;

        /*
         * Now do the same again in a loop where we change the spring constant
         */

        unsigned num_springs = 3;
        observed_aspect_ratios.resize(num_springs);

        for (unsigned sc_idx = 0 ; sc_idx < num_springs ; sc_idx++)
        {
            /*
             * Create an Immersed Boundary Mesh using a SuperellipseGenerator
             *
             * 1: Num nodes
             * 2: Superellipse exponent
             * 3: Width
             * 4: Height
             * 5: Bottom-left x
             * 6: Botton-left y
             */
            SuperellipseGenerator* p_gen = new SuperellipseGenerator(256, 1.0, 0.3, 0.6, 0.35, 0.2);

            // Because we have already run a simulation in this test, we must destroy the SimulationTime singleton
            SimulationTime::Instance()->Destroy();
            SimulationTime::Instance()->SetStartTime(0.0);

            // Generate a mesh using this superellipse
            std::vector<c_vector<double, 2> > locations = p_gen->GetPointsAsVectors();

            std::vector<Node<2>* > nodes;
            for (unsigned node_idx = 0 ; node_idx < locations.size() ; node_idx++)
            {
                nodes.push_back(new Node<2>(node_idx, locations[node_idx], true));
            }

            std::vector<ImmersedBoundaryElement<2,2>* > elements;
            elements.push_back(new ImmersedBoundaryElement<2,2>(0, nodes));

            ImmersedBoundaryMesh<2,2> mesh(nodes, elements, 64, 64);

            // Set up cell population
            std::vector<CellPtr> cells;
            MAKE_PTR(DifferentiatedCellProliferativeType, p_diff_type);
            CellsGenerator<StochasticDurationCellCycleModel, 2> cells_generator;
            cells_generator.GenerateBasicRandom(cells, mesh.GetNumElements(), p_diff_type);
            ImmersedBoundaryCellPopulation <2> cell_population(mesh, cells);

            // Set element properties
            ImmersedBoundaryElement<2, 2>* p_elem = mesh.GetElement(0u);

            p_elem->SetMembraneRestLength(0.1 * mesh.GetCharacteristicNodeSpacing());
            p_elem->SetMembraneSpringConstant(2.0 * double(sc_idx + 1) * ref_spring_const);

            // Create simulation
            OffLatticeSimulation<2> *p_simulator = new OffLatticeSimulation<2>(cell_population);

            // Add main immersed boundary simulation modifier
            MAKE_PTR(ImmersedBoundarySimulationModifier < 2 >, p_main_modifier);
            p_simulator->AddSimulationModifier(p_main_modifier);

            std::string output_dir = "ImmersedBoundaryNumerics/TestSingleCellRelaxationSC";
            output_dir += boost::lexical_cast<std::string>(p_elem->GetMembraneSpringConstant() / ref_spring_const);

            // Set simulation properties
            p_simulator->SetOutputDirectory(output_dir);
            p_simulator->SetDt(0.01);
            p_simulator->SetSamplingTimestepMultiple(10);

            c_vector<double, 2> long_axis = p_elem->GetNode(64)->rGetLocation() - p_elem->GetNode(192)->rGetLocation();
            c_vector<double, 2> short_axis = p_elem->GetNode(0)->rGetLocation() - p_elem->GetNode(128)->rGetLocation();

            observed_aspect_ratios[sc_idx].push_back(norm_2(long_axis) / norm_2(short_axis));

            for (unsigned sim_time_idx = 1 ; sim_time_idx <= num_sim_timesteps ; sim_time_idx++)
            {
                // Calculate new end time and run simulation
                double new_end_time = 0.1 * double(sim_time_idx);
                p_simulator->SetEndTime(new_end_time);
                p_simulator->Solve();

                // Calculate aspect ratio and store data
                c_vector<double, 2> long_axis = p_elem->GetNode(64)->rGetLocation() - p_elem->GetNode(192)->rGetLocation();
                c_vector<double, 2> short_axis = p_elem->GetNode(0)->rGetLocation() - p_elem->GetNode(128)->rGetLocation();

                observed_aspect_ratios[sc_idx].push_back(norm_2(long_axis) / norm_2(short_axis));
            }

            delete p_gen;
            delete p_simulator;
        }


        CsvWriter csv_writer;
        csv_writer.AddData(simulation_time);
        csv_writer.AddData(ref_aspect_ratio);

        for (unsigned sc_idx = 0 ; sc_idx < num_springs ; sc_idx++)
        {
            csv_writer.AddData(observed_aspect_ratios[sc_idx]);
        }


        std::vector<std::string> header_names;
        header_names.push_back("Simulation time (h)");
        header_names.push_back("Reference aspect ratio");

        for (unsigned sc_idx = 0 ; sc_idx < num_springs ; sc_idx++)
        {
            std::string header = "Spring constant mult ";

            std::ostringstream suffix;
            suffix << std::fixed << std::setprecision(1);
            suffix << 1.0 * double(sc_idx + 1);
            header += suffix.str();

            header_names.push_back(header);
        }

        csv_writer.AddHeaders(header_names);

        csv_writer.SetDirectoryName(mOutputDirectory);
        csv_writer.SetFileName("TestSingleCellRelaxation");

        csv_writer.WriteDataToFile();
    }

    void xTestSingleCellComputationTimeWithGridSpacing() throw(Exception)
    {
        /**
         * This test tracks the change in computation time of a single cell relaxing for a fixed simulation time.
         *
         * All parameters are fixed, and the following are exported to a csv file:
         *  * Number of nodes in the fluid grid
         *  * Computation time of each simulation
         *  * Change in elongation shape factor, a measure of how the simulation is progressing
         */

        std::vector<unsigned> num_gridpts_vec;
        std::vector<double> computation_time;
        std::vector<double> shape_change;

        for (unsigned num_gridpts = 128 ; num_gridpts < 513 ; num_gridpts += 64)
        {
            SimulationTime::Instance()->Destroy();
            SimulationTime::Instance()->SetStartTime(0.0);

            /*
             * Create an Immersed Boundary Mesh using a SuperellipseGenerator
             *
             * 1: Num nodes
             * 2: Superellipse exponent
             * 3: Width
             * 4: Height
             * 5: Bottom-left x
             * 6: Botton-left y
             */
            SuperellipseGenerator *p_gen = new SuperellipseGenerator(256, 1.0, 0.3, 0.6, 0.35, 0.2);

            // Generate a mesh using this superellipse
            std::vector<c_vector<double, 2> > locations = p_gen->GetPointsAsVectors();
            delete p_gen;

            std::vector<Node<2>* > nodes;
            for (unsigned node_idx = 0 ; node_idx < locations.size() ; node_idx++)
            {
                nodes.push_back(new Node<2>(node_idx, locations[node_idx], true));
            }

            std::vector<ImmersedBoundaryElement<2,2>* > elements;
            elements.push_back(new ImmersedBoundaryElement<2,2>(0, nodes));

            ImmersedBoundaryMesh<2,2> mesh(nodes, elements, num_gridpts, num_gridpts);

            /*
             * Set up cell population
             */
            std::vector<CellPtr> cells;
            MAKE_PTR(DifferentiatedCellProliferativeType, p_diff_type);
            CellsGenerator<StochasticDurationCellCycleModel, 2> cells_generator;
            cells_generator.GenerateBasicRandom(cells, mesh.GetNumElements(), p_diff_type);

            ImmersedBoundaryCellPopulation <2> cell_population(mesh, cells);


            ImmersedBoundaryElement <2, 2> *p_elem = mesh.GetElement(0u);

            p_elem->SetMembraneRestLength(0.1 * mesh.GetCharacteristicNodeSpacing());
            p_elem->SetMembraneSpringConstant(1e4);

            double elongation_change = mesh.GetElongationShapeFactorOfElement(0);

            OffLatticeSimulation<2> *p_simulator = new OffLatticeSimulation<2>(cell_population);

            // Add main immersed boundary simulation modifier
            MAKE_PTR(ImmersedBoundarySimulationModifier < 2 >, p_main_modifier);
            p_simulator->AddSimulationModifier(p_main_modifier);

            std::string output_dir = "ImmersedBoundaryNumerics/TestSingleCellComputationTimeWithGridSpacing";
            output_dir += boost::lexical_cast<std::string>(num_gridpts);

            double this_dt = 0.01;

            p_simulator->SetOutputDirectory(output_dir);
            p_simulator->SetDt(this_dt);
            p_simulator->SetSamplingTimestepMultiple(10);
            p_simulator->SetEndTime(7.0);

            // Run and time the simulation
            mTimer.Reset();
            p_simulator->Solve();

            computation_time.push_back(mTimer.GetElapsedTime());
            num_gridpts_vec.push_back(num_gridpts);
            shape_change.push_back(mesh.GetElongationShapeFactorOfElement(0));

            delete p_simulator;
        }

        CsvWriter csv_writer;
        csv_writer.AddData(num_gridpts_vec);
        csv_writer.AddData(computation_time);
        csv_writer.AddData(shape_change);

        std::vector<std::string> header_names;
        header_names.push_back("Number of points in fluid grid");
        header_names.push_back("Computation time (s)");
        header_names.push_back("Shape change");
        csv_writer.AddHeaders(header_names);

        csv_writer.SetDirectoryName(mOutputDirectory);
        csv_writer.SetFileName("TestSingleCellComputationTimeWithGridSpacing");

        csv_writer.WriteDataToFile();
    }

    void xTestPalisadeSimulation() throw(Exception)
    {
        /*
         * 1: Num cells
         * 2: Num nodes per cell
         * 3: Superellipse exponent
         * 4: Superellipse aspect ratio
         * 5: Random y-variation
         * 6: Include membrane
         */
        ImmersedBoundaryPalisadeMeshGenerator gen(11, 50, 0.2, 2.0, 1.0, true);
        ImmersedBoundaryMesh<2,2>* p_mesh = gen.GetMesh();

        p_mesh->GetMembraneElement()->SetMembraneSpringConstant(10000000.0);
        p_mesh->GetMembraneElement()->SetMembraneRestLength(0.0001);

        std::vector<CellPtr> cells;
        MAKE_PTR(DifferentiatedCellProliferativeType, p_diff_type);
        CellsGenerator<StochasticDurationCellCycleModel, 2> cells_generator;
        cells_generator.GenerateBasicRandom(cells, p_mesh->GetNumElements(), p_diff_type);

        ImmersedBoundaryCellPopulation<2> cell_population(*p_mesh, cells);

        OffLatticeSimulation<2> simulator(cell_population);

        // Add main immersed boundary simulation modifier
        MAKE_PTR(ImmersedBoundarySimulationModifier<2>, p_main_modifier);
        simulator.AddSimulationModifier(p_main_modifier);

        // Set simulation properties
        simulator.SetOutputDirectory("ImmersedBoundaryNumerics/TestPalisadeSimulation");
        simulator.SetDt(0.05);
        simulator.SetSamplingTimestepMultiple(1);
        simulator.SetEndTime(0.05);

        // Run the simulation
        simulator.Solve();
    }
};