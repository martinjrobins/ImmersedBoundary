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

#include "ImmersedBoundaryMesh.hpp"
#include "RandomNumberGenerator.hpp"
#include "UblasCustomFunctions.hpp"
#include "Warnings.hpp"

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::ImmersedBoundaryMesh(std::vector<Node<SPACE_DIM>*> nodes,
                                                                   std::vector<ImmersedBoundaryElement<ELEMENT_DIM,SPACE_DIM>*> elements,
                                                                   unsigned numGridPtsX,
                                                                   unsigned numGridPtsY,
                                                                   unsigned membraneIndex)
    : mNumGridPtsX(numGridPtsX),
      mNumGridPtsY(numGridPtsY),
      mMembraneIndex(membraneIndex),
      mElementDivisionSpacing(DOUBLE_UNSET)
{
    // Clear mNodes and mElements
    Clear();

    m2dVelocityGrids.resize(extents[2][mNumGridPtsX][mNumGridPtsY]);

    switch (SPACE_DIM)
    {
        case 2:
            m2dVelocityGrids.resize(extents[2][mNumGridPtsX][mNumGridPtsY]);
            break;

        case 3:
            EXCEPTION("Not implemented yet in 3D");
            break;

        default:
            NEVER_REACHED;
    }

    // If the membrane index is UINT_MAX, there is no membrane; if not, there is
    mMeshHasMembrane = mMembraneIndex != UINT_MAX;

    // Populate mNodes and mElements
    for (unsigned node_index=0; node_index<nodes.size(); node_index++)
    {
        Node<SPACE_DIM>* p_temp_node = nodes[node_index];
        this->mNodes.push_back(p_temp_node);
    }
    for (unsigned elem_index=0; elem_index<elements.size(); elem_index++)
    {
        ImmersedBoundaryElement<ELEMENT_DIM, SPACE_DIM>* p_temp_element = elements[elem_index];
        mElements.push_back(p_temp_element);
    }

    // Register elements with nodes
    for (unsigned index=0; index<mElements.size(); index++)
    {
        ImmersedBoundaryElement<ELEMENT_DIM, SPACE_DIM>* p_element = mElements[index];

        unsigned element_index = p_element->GetIndex();
        unsigned num_nodes_in_element = p_element->GetNumNodes();

        for (unsigned node_index=0; node_index<num_nodes_in_element; node_index++)
        {
            p_element->GetNode(node_index)->AddElement(element_index);
        }
    }

    // Set characteristic node spacing to the average distance between nodes
    double total_perimeter = 0.0;
    unsigned total_nodes = 0;
    for (unsigned elem_index = 0; elem_index < elements.size(); elem_index++)
    {
        if (elem_index != mMembraneIndex)
        {
            total_perimeter += this->GetSurfaceAreaOfElement(elem_index);
            total_nodes += mElements[elem_index]->GetNumNodes();
        }
    }
    mCharacteristicNodeSpacing = total_perimeter / double(total_nodes);

    // Position fluid sources at the centroid of each cell, and set strength to zero
    for (unsigned elem_it = 0; elem_it < elements.size(); elem_it++)
    {
        unsigned this_elem_idx = mElements[elem_it]->GetIndex();

        // Each element other than the membrane element will have a source associated with it
        if (this_elem_idx != mMembraneIndex)
        {
            // Create a new fluid source at the correct location for each element
            unsigned source_idx = mElementFluidSources.size();
            c_vector<double, SPACE_DIM> source_location = this->GetCentroidOfElement(this_elem_idx);
            mElementFluidSources.push_back(new FluidSource<SPACE_DIM>(source_idx, source_location));

            // Set source parameters
            mElementFluidSources.back()->SetAssociatedElementIndex(this_elem_idx);
            mElementFluidSources.back()->SetStrength(0.0);

            // Associate source with element
            mElements[elem_it]->SetFluidSource(mElementFluidSources.back());
        }
    }

    /*
     * Set up a number of sources to balance any active sources associated with elements
     */
    double balancing_source_spacing = 4.0 / (double)numGridPtsX;

    // We start 1/2 a grid-spacing in from the left-hand end, and place a source every 4-grid-spacings
    double current_location = balancing_source_spacing / 8.0;

    while (current_location < 1.0)
    {
        // Create a new fluid source at the current x-location and zero y-location
        unsigned source_idx = mBalancingFluidSources.size();
        mBalancingFluidSources.push_back(new FluidSource<SPACE_DIM>(source_idx, current_location));

        // Increment the current location
        current_location += balancing_source_spacing;
    }

    this->mMeshChangesDuringSimulation = true;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
double ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetElongationShapeFactorOfElement(unsigned index)
{
    assert(SPACE_DIM == 2);

    c_vector<double, 3> moments = CalculateMomentsOfElement(index);

    double discriminant = sqrt((moments(0) - moments(1))*(moments(0) - moments(1)) + 4.0*moments(2)*moments(2));

    // Note that as the matrix of second moments of area is symmetric, both its eigenvalues are real
    double largest_eigenvalue = (moments(0) + moments(1) + discriminant)*0.5;
    double smallest_eigenvalue = (moments(0) + moments(1) - discriminant)*0.5;

    double elongation_shape_factor = sqrt(largest_eigenvalue/smallest_eigenvalue);
    return elongation_shape_factor;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
double ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetTortuosityOfMesh()
{
    assert(SPACE_DIM == 2);

    // Compute tortuosity (defined as ratio of total length to straight-line length) of piecewise linear curve through centroids of successive elements
    double total_length = 0.0;

    // We assume that if there is a membrane present, it has index 0
    unsigned first_elem_idx = this->mMeshHasMembrane ? 1 : 0;

    c_vector<double, SPACE_DIM> previous_centroid = this->GetCentroidOfElement(first_elem_idx);

    for (unsigned elem_idx = first_elem_idx; elem_idx < this->GetNumElements(); elem_idx++)
    {
        c_vector<double, 2> this_centroid = this->GetCentroidOfElement(elem_idx);
        total_length += norm_2(this->GetVectorFromAtoB(previous_centroid, this_centroid));
        previous_centroid = this_centroid;
    }

    c_vector<double, 2> first_centroid = this->GetCentroidOfElement(first_elem_idx);
    c_vector<double, 2> last_centroid = this->GetCentroidOfElement(this->GetNumElements()-1);

    double straight_line_length = norm_2(this->GetVectorFromAtoB(first_centroid, last_centroid));
    straight_line_length = std::max(straight_line_length, 1.0-straight_line_length);

    return total_length / straight_line_length;
}


bool CustomComparisonForSkewnessMeasure(std::pair<unsigned, c_vector<double, 2> > pairA, std::pair<unsigned, c_vector<double, 2> > pairB)
{
    return pairA.second[0] < pairB.second[0];
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
double ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetSkewnessOfElementMassDistributionAboutAxis(unsigned elemIndex, c_vector<double, SPACE_DIM> axis)
{
    /*
     * Method outline:
     *
     * Given an arbitrary axis and a closed polygon, we calculate the skewness of the mass distribution of the polygon
     * perpendicular to the axis.  This is used as a measure of asymmetry.
     *
     * To simplify calculating the mass distribution, we translate the centroid of the element to the origin and rotate
     * about the centroid so the axis is vertical; then we sort all the nodes in ascending order of their x-coordinate.
     *
     * For each node in order, we need to know the length of the intersection through the node of the vertical line with
     * the polygon.  Once calculated, we have a piecewise-linear PDF for the mass distribution, which can be normalised
     * by the surface area of the polygon.
     *
     * By integrating the pdf directly, we can calculate exactly the necessary moments of the distribution needed for
     * the skewness.
     */

    // This method only works in 2D
    assert(ELEMENT_DIM == 2 && SPACE_DIM == 2);

    // Get relevant info about the element
    ImmersedBoundaryElement<ELEMENT_DIM,SPACE_DIM>* p_elem = this->GetElement(elemIndex);
    unsigned num_nodes = p_elem->GetNumNodes();
    double area_of_elem = this->GetVolumeOfElement(elemIndex);
    c_vector<double, SPACE_DIM> centroid = this->GetCentroidOfElement(elemIndex);

    // Get the unit axis and trig terms for rotation
    c_vector<double, SPACE_DIM> unit_axis = axis / norm_2(axis);
    double sin_theta = unit_axis[0];
    double cos_theta = unit_axis[1];

    // We need the (rotated) node locations in two orders - original and ordered left-to-right.
    // For the latter we need to keep track of index, so we store that as part of a pair.
    std::vector<c_vector<double, SPACE_DIM> > node_locations_original_order;
    std::vector<std::pair<unsigned, c_vector<double, SPACE_DIM> > > ordered_locations;

    // Get the node locations of the current element relative to its centroid, and rotate them
    for (unsigned node_idx = 0; node_idx < num_nodes; node_idx++)
    {
        const c_vector<double, SPACE_DIM>& node_location = p_elem->GetNode(node_idx)->rGetLocation();

        c_vector<double, SPACE_DIM> displacement = this->GetVectorFromAtoB(centroid, node_location);

        c_vector<double, SPACE_DIM> rotated_location;
        rotated_location[0] = cos_theta * displacement[0] - sin_theta * displacement[1];
        rotated_location[1] = sin_theta * displacement[0] + cos_theta * displacement[1];

        node_locations_original_order.push_back(rotated_location);
    }

    // Fill up a vector of identical points, and sort it so nodes are ordered in ascending x value
    for (unsigned i=0; i<node_locations_original_order.size(); i++)
    {
        ordered_locations.push_back(std::pair<unsigned, c_vector<double, SPACE_DIM> >(i, node_locations_original_order[i]));
    }

    std::sort(ordered_locations.begin(), ordered_locations.end(), CustomComparisonForSkewnessMeasure);

    /*
     * For each node, we must find every place where the axis (now rotated to be vertical) intersects the polygon:
     *
     *       |
     *     __|______
     *    /  |      \
     *   /   |       \
     *  /____|___    |
     *       |  |    |
     *  _____|__|    |
     *  \    |       |
     *   \   |      /
     *    \__|_____/
     *       |
     *       |
     *       ^
     * For instance, the number of times the vertical intersects the polygon above is 4 and, for each node, we need to
     * find all such intersections.  We can do this by checking where the dot product of the the vector a with the unit
     * x direction changes sign as we iterate over the original node locations, where a is the vector from the current
     * node to the test node.
     */

    // For each node, we keep track of all the y-locations where the vertical through the node meets the polygon
    std::vector<std::vector<double> > knots(num_nodes);

    for (unsigned location = 0; location < num_nodes; location++)
    {
        // Get the two parts of the pair
        unsigned this_idx = ordered_locations[location].first;
        c_vector<double, SPACE_DIM> this_location = ordered_locations[location].second;

        // The y-coordinate of the current location is always a knot
        knots[location].push_back(this_location[1]);

        // To calculate all the intersection points, we need to iterate over every other location and see, sequentially,
        // if the x-coordinate of location i+1 and i+2 crosses the x-coordinate of the current location.
        unsigned next_idx = (this_idx + 1) % num_nodes;
        c_vector<double, SPACE_DIM> to_previous = node_locations_original_order[next_idx] - this_location;

        for (unsigned node_idx = this_idx + 2; node_idx < this_idx + num_nodes; node_idx++)
        {
            unsigned idx = node_idx % num_nodes;

            c_vector<double, SPACE_DIM> to_next = node_locations_original_order[idx] - this_location;

            // If the segment between to_previous and to_next intersects the vertical through this_location, the clause
            // in the if statement below will be triggered
            if (to_previous[0] * to_next[0] <= 0.0)
            {
                // Find how far between to_previous and to_next the point of intersection is
                double interp = to_previous[0] / (to_previous[0] - to_next[0]);

                assert(interp >= 0.0 && interp <= 1.0);

                // Record the y-value of the intersection point
                double new_intersection = this_location[1] + to_previous[1] + interp * (to_next[1] - to_previous[1]);
                knots[location].push_back(new_intersection);
            }

            to_previous = to_next;
        }

        if (knots[location].size() > 2)
        {
            WARN_ONCE_ONLY("Axis intersects polygon more than 2 times (concavity) - check element is fairly convex.");
        }
    }

    // For ease, construct a vector of the x-locations of all the nodes, in order
    std::vector<double> ordered_x(num_nodes);
    for (unsigned location = 0; location < num_nodes; location++)
    {
        ordered_x[location]  = ordered_locations[location].second[0];
    }

    // Calculate the mass contributions at each x-location - this is the length of the intersection of the vertical
    // through each location
    std::vector<double> mass_contributions(num_nodes);
    for (unsigned i=0; i<num_nodes; i++)
    {
        std::sort(knots[i].begin(), knots[i].end());

        switch (knots[i].size())
        {
            case 1:
                mass_contributions[i] = 0.0;
                break;

            case 2:
                mass_contributions[i] = knots[i][1] - knots[i][0];
                break;

            default:
                mass_contributions[i] = knots[i][knots[i].size() - 1] - knots[i][0];
        }

        // Normalise, so that these lengths define a pdf
        mass_contributions[i] /= area_of_elem;
    }

    // Calculate moments. Because we just have a bunch of linear segments, we can integrate the pdf exactly
    double e_x0 = 0.0;
    double e_x1 = 0.0;
    double e_x2 = 0.0;
    double e_x3 = 0.0;

    for (unsigned i=1; i<num_nodes; i++)
    {
        double x0 = ordered_x[i-1];
        double x1 = ordered_x[i];

        double fx0 = mass_contributions[i-1];
        double fx1 = mass_contributions[i];

        // We need squared, cubed, ..., order 5 for each x
        double x0_2 = x0 * x0;
        double x0_3 = x0_2 * x0;
        double x0_4 = x0_3 * x0;
        double x0_5 = x0_4 * x0;

        double x1_2 = x1 * x1;
        double x1_3 = x1_2 * x1;
        double x1_4 = x1_3 * x1;
        double x1_5 = x1_4 * x1;

        if (x1 - x0 > 0)
        {
            // Calculate y = mx + c for this section of the pdf
            double m = (fx1 - fx0) / (x1 - x0);
            double c = fx0 - m * x0;

            e_x0 += m * (x1_2 - x0_2) / 2.0 + c * (x1 - x0);
            e_x1 += m * (x1_3 - x0_3) / 3.0 + c * (x1_2 - x0_2) / 2.0;
            e_x2 += m * (x1_4 - x0_4) / 4.0 + c * (x1_3 - x0_3) / 3.0;
            e_x3 += m * (x1_5 - x0_5) / 5.0 + c * (x1_4 - x0_4) / 4.0;
        }
    }

    // Check that we have correctly defined a pdf
    assert(fabs(e_x0 - 1.0) < 1e-6);

    // Calculate the standard deviation, and return the skewness
    double sd = sqrt(e_x2 - e_x1 * e_x1);
    return (e_x3 - 3.0 * e_x1 * sd * sd - e_x1 * e_x1 * e_x1) / (sd * sd * sd);
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
ChasteCuboid<SPACE_DIM> ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::CalculateBoundingBoxOfElement(unsigned index)
{
    ImmersedBoundaryElement<ELEMENT_DIM,SPACE_DIM>* p_elem = this->GetElement(index);

    // Get the location of node zero as a reference point
    c_vector<double, SPACE_DIM> ref_point = p_elem->GetNode(0)->rGetLocation();

    // Vector to represent the n-dimensional 'bottom left'-most node
    c_vector<double, SPACE_DIM> bottom_left = zero_vector<double>(SPACE_DIM);

    // Vector to represent the n-dimensional 'top right'-most node
    c_vector<double, SPACE_DIM> top_right = zero_vector<double>(SPACE_DIM);

    // Loop over all nodes in the element and update bottom_left and top_right, relative to node zero to account for periodicity
    for (unsigned node_idx = 0; node_idx < p_elem->GetNumNodes(); node_idx++)
    {
        c_vector<double, SPACE_DIM> vec_to_node = this->GetVectorFromAtoB(ref_point, p_elem->GetNode(node_idx)->rGetLocation());

        for (unsigned dim = 0; dim < SPACE_DIM; dim++)
        {
            if (vec_to_node[dim] < bottom_left[dim])
            {
                bottom_left[dim] = vec_to_node[dim];
            }
            else if (vec_to_node[dim] > top_right[dim])
            {
                top_right[dim] = vec_to_node[dim];
            }
        }
    }

    // Create Chaste points, rescaled by the location of node zero
    ChastePoint<SPACE_DIM> min(bottom_left + ref_point);
    ChastePoint<SPACE_DIM> max(top_right + ref_point);

    return ChasteCuboid<SPACE_DIM>(min, max);
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::ImmersedBoundaryMesh()
{
    this->mMeshChangesDuringSimulation = false;
    Clear();
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::~ImmersedBoundaryMesh()
{
    Clear();
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
unsigned ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::SolveNodeMapping(unsigned index) const
{
    assert(index < this->mNodes.size());
    return index;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
unsigned ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::SolveElementMapping(unsigned index) const
{
    assert(index < this->mElements.size());
    return index;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
unsigned ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::SolveBoundaryElementMapping(unsigned index) const
{
    return index;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
void ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::Clear()
{
    // Delete elements
    for (unsigned i=0; i<mElements.size(); i++)
    {
        delete mElements[i];
    }
    mElements.clear();

    // Delete nodes
    for (unsigned i=0; i<this->mNodes.size(); i++)
    {
        delete this->mNodes[i];
    }
    this->mNodes.clear();
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
double ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetCharacteristicNodeSpacing() const
{
    return mCharacteristicNodeSpacing;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
double ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetSpacingRatio() const
{
    ///todo If we ever permit mNumGridPtsX != mNumGridPtsY, need to decide how SpacingRatio is defined
    return mCharacteristicNodeSpacing / (1.0 / double(mNumGridPtsX));
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
unsigned ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetNumGridPtsX() const
{
    return mNumGridPtsX;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
unsigned ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetNumGridPtsY() const
{
    return mNumGridPtsY;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
void ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::SetNumGridPtsX(unsigned mesh_points_x)
{
    mNumGridPtsX = mesh_points_x;
    m2dVelocityGrids.resize(extents[2][mNumGridPtsX][mNumGridPtsY]);
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
void ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::SetNumGridPtsY(unsigned mesh_points_y)
{
    mNumGridPtsY = mesh_points_y;
    m2dVelocityGrids.resize(extents[2][mNumGridPtsX][mNumGridPtsY]);
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
void ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::SetNumGridPtsXAndY(unsigned numGridPts)
{
    mNumGridPtsX = numGridPts;
    mNumGridPtsY = numGridPts;
    m2dVelocityGrids.resize(extents[2][mNumGridPtsX][mNumGridPtsY]);
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
void ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::SetCharacteristicNodeSpacing(double node_spacing)
{
    mCharacteristicNodeSpacing = node_spacing;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
void ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::SetMembraneIndex(unsigned membrane_index)
{
    mMembraneIndex = membrane_index;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
ImmersedBoundaryElement<ELEMENT_DIM, SPACE_DIM>* ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetMembraneElement()
{
    if (mMembraneIndex < UINT_MAX)
    {
        return this->GetElement(mMembraneIndex);
    }
    else
    {
        return NULL;
    }
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
unsigned ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetMembraneIndex()
{
    return mMembraneIndex;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
std::vector<FluidSource<SPACE_DIM>*>& ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::rGetElementFluidSources()
{
    return mElementFluidSources;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
std::vector<FluidSource<SPACE_DIM>*>& ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::rGetBalancingFluidSources()
{
    return mBalancingFluidSources;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
const multi_array<double, 3>& ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::rGet2dVelocityGrids() const
{
    return m2dVelocityGrids;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
const multi_array<double, 4>& ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::rGet3dVelocityGrids() const
{
    return m3dVelocityGrids;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
multi_array<double, 3>& ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::rGetModifiable2dVelocityGrids()
{
    return m2dVelocityGrids;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
multi_array<double, 4>& ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::rGetModifiable3dVelocityGrids()
{
    return m3dVelocityGrids;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
std::vector<Node<SPACE_DIM>*>& ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::rGetNodes()
{
    return AbstractMesh<ELEMENT_DIM, SPACE_DIM>::mNodes;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
c_vector<double, SPACE_DIM> ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetVectorFromAtoB(const c_vector<double, SPACE_DIM>& rLocation1, const c_vector<double, SPACE_DIM>& rLocation2)
{
    // This code currently assumes the grid is precisely [0,1)x[0,1)
    c_vector<double, SPACE_DIM> vector = rLocation2 - rLocation1;

    /*
     * Handle the periodic condition here: if the points are more
     * than 0.5 apart in any direction, choose -(1.0-dist).
     */
    for (unsigned dim = 0; dim < SPACE_DIM; dim++)
    {
        if (fabs(vector[dim]) > 0.5)
        {
            vector[dim] = copysign(fabs(vector[dim]) - 1.0, -vector[dim]);
        }
    }

    return vector;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
void ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::SetNode(unsigned nodeIndex, ChastePoint<SPACE_DIM> point)
{
    this->mNodes[nodeIndex]->SetPoint(point);
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
unsigned ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetNumNodes() const
{
    return this->mNodes.size();
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
unsigned ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetNumElements() const
{
    return mElements.size();
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
unsigned ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetNumAllElements() const
{
    return mElements.size();
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
ImmersedBoundaryElement<ELEMENT_DIM, SPACE_DIM>* ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetElement(unsigned index) const
{
    assert(index < mElements.size());
    return mElements[index];
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
c_vector<double, SPACE_DIM> ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetCentroidOfElement(unsigned index)
{
    // Only implemented in 2D
    assert(SPACE_DIM == 2);

    ImmersedBoundaryElement<ELEMENT_DIM, SPACE_DIM>* p_element = GetElement(index);

    // The membrane must be treated differently
    if (index == mMembraneIndex)
    {
        return zero_vector<double>(2);
    }

    else
    {
        unsigned num_nodes = p_element->GetNumNodes();
        c_vector<double, SPACE_DIM> centroid = zero_vector<double>(SPACE_DIM);

        double centroid_x = 0;
        double centroid_y = 0;

        // Note that we cannot use GetVolumeOfElement() below as it returns the absolute, rather than signed, area
        double element_signed_area = 0.0;

        // Map the first vertex to the origin and employ GetVectorFromAtoB() to allow for periodicity
        c_vector<double, SPACE_DIM> first_node_location = p_element->GetNodeLocation(0);
        c_vector<double, SPACE_DIM> pos_1 = zero_vector<double>(SPACE_DIM);

        // Loop over vertices
        for (unsigned local_index = 0; local_index < num_nodes; local_index++)
        {
            c_vector<double, SPACE_DIM> next_node_location = p_element->GetNodeLocation((local_index + 1) % num_nodes);
            c_vector<double, SPACE_DIM> pos_2 = GetVectorFromAtoB(first_node_location, next_node_location);

            double this_x = pos_1[0];
            double this_y = pos_1[1];
            double next_x = pos_2[0];
            double next_y = pos_2[1];

            double signed_area_term = this_x * next_y - this_y * next_x;

            centroid_x += (this_x + next_x) * signed_area_term;
            centroid_y += (this_y + next_y) * signed_area_term;
            element_signed_area += 0.5 * signed_area_term;

            pos_1 = pos_2;
        }

        assert(element_signed_area != 0.0);

        // Finally, map back and employ GetVectorFromAtoB() to allow for periodicity
        centroid = first_node_location;
        centroid[0] += centroid_x / (6.0 * element_signed_area);
        centroid[1] += centroid_y / (6.0 * element_signed_area);

        centroid[0] = centroid[0] < 0 ? centroid[0] + 1.0 : fmod(centroid[0], 1.0);
        centroid[1] = centroid[1] < 0 ? centroid[1] + 1.0 : fmod(centroid[1], 1.0);

        return centroid;
    }
}


/// \cond Get Doxygen to ignore, since it's confused by these templates
template<>
void ImmersedBoundaryMesh<1,1>::ConstructFromMeshReader(AbstractMeshReader<1,1>& rMeshReader)
/// \endcond Get Doxygen to ignore, since it's confused by these templates
{
}

/// \cond Get Doxygen to ignore, since it's confused by these templates
template<>
void ImmersedBoundaryMesh<1,2>::ConstructFromMeshReader(AbstractMeshReader<1,2>& rMeshReader)
/// \endcond Get Doxygen to ignore, since it's confused by these templates
{
}

/// \cond Get Doxygen to ignore, since it's confused by these templates
template<>
void ImmersedBoundaryMesh<1,3>::ConstructFromMeshReader(AbstractMeshReader<1,3>& rMeshReader)
/// \endcond Get Doxygen to ignore, since it's confused by these templates
{
}

/// \cond Get Doxygen to ignore, since it's confused by these templates
template<>
void ImmersedBoundaryMesh<2,3>::ConstructFromMeshReader(AbstractMeshReader<2,3>& rMeshReader)
/// \endcond Get Doxygen to ignore, since it's confused by these templates
{
}

/// \cond Get Doxygen to ignore, since it's confused by these templates
template<>
void ImmersedBoundaryMesh<2,2>::ConstructFromMeshReader(AbstractMeshReader<2,2>& rMeshReader)
/// \endcond Get Doxygen to ignore, since it's confused by these templates
{
    ImmersedBoundaryMeshReader<2,2>& rIBMeshReader = dynamic_cast<ImmersedBoundaryMeshReader<2,2>&>(rMeshReader);

    assert(rIBMeshReader.HasNodePermutation() == false);
    // Store numbers of nodes and elements
    unsigned num_nodes = rIBMeshReader.GetNumNodes();
    unsigned num_elements = rIBMeshReader.GetNumElements();
    this->mCharacteristicNodeSpacing = rIBMeshReader.GetCharacteristicNodeSpacing();

    // Reserve memory for nodes
    this->mNodes.reserve(num_nodes);

    rIBMeshReader.Reset();

    // Add nodes
    std::vector<double> node_data;
    for (unsigned i=0; i<num_nodes; i++)
    {
        node_data = rIBMeshReader.GetNextNode();
        unsigned is_boundary_node = (bool) node_data[2];
        node_data.pop_back();
        this->mNodes.push_back(new Node<2>(i, node_data, is_boundary_node));
    }

    rIBMeshReader.Reset();

    // Reserve memory for nodes
    mElements.reserve(rIBMeshReader.GetNumElements());

    // Initially ensure there is no boundary element - this will be updated in the next loop if there is
    this->mMembraneIndex = UINT_MAX;
    this->mMeshHasMembrane = false;

    // Add elements
    for (unsigned elem_index=0; elem_index<num_elements; elem_index++)
    {
        // Get the data for this element
        ImmersedBoundaryElementData element_data = rIBMeshReader.GetNextImmersedBoundaryElementData();

        // Get the nodes owned by this element
        std::vector<Node<2>*> nodes;
        unsigned num_nodes_in_element = element_data.NodeIndices.size();
        for (unsigned j=0; j<num_nodes_in_element; j++)
        {
            assert(element_data.NodeIndices[j] < this->mNodes.size());
            nodes.push_back(this->mNodes[element_data.NodeIndices[j]]);
        }

        // Use nodes and index to construct this element
        ImmersedBoundaryElement<2,2>* p_element = new ImmersedBoundaryElement<2,2>(elem_index, nodes);
        mElements.push_back(p_element);

        if (element_data.MembraneElement)
        {
            this->mMeshHasMembrane = true;
            this->mMembraneIndex = elem_index;
        }

        if (rIBMeshReader.GetNumElementAttributes() > 0)
        {
            assert(rIBMeshReader.GetNumElementAttributes() == 1);
            unsigned attribute_value = (unsigned) element_data.AttributeValue;
            p_element->SetAttribute(attribute_value);
        }
    }

    // Get grid dimensions from grid file and set up grids accordingly
    this->mNumGridPtsX = rIBMeshReader.GetNumGridPtsX();
    this->mNumGridPtsY = rIBMeshReader.GetNumGridPtsY();
    m2dVelocityGrids.resize(extents[2][mNumGridPtsX][mNumGridPtsY]);

    // Construct the velocity grids from mesh reader
    for (unsigned dim = 0; dim < 2; dim++)
    {
        for (unsigned grid_row = 0; grid_row < mNumGridPtsY; grid_row++)
        {
            std::vector<double> next_row = rIBMeshReader.GetNextGridRow();
            assert(next_row.size() == mNumGridPtsX);

            for (unsigned i = 0; i < mNumGridPtsX; i++)
            {
                m2dVelocityGrids[dim][i][grid_row] = next_row[i];
            }
        }
    }
}

/// \cond Get Doxygen to ignore, since it's confused by these templates
template<>
void ImmersedBoundaryMesh<3,3>::ConstructFromMeshReader(AbstractMeshReader<3,3>& rMeshReader)
/// \endcond Get Doxygen to ignore, since it's confused by these templates
{
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
double ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetVolumeOfElement(unsigned index)
{
    assert(SPACE_DIM == 2);

    // Get pointer to this element
    ImmersedBoundaryElement<ELEMENT_DIM, SPACE_DIM>* p_element = GetElement(index);

    double element_volume = 0.0;

    // We map the first vertex to the origin and employ GetVectorFromAtoB() to allow for periodicity
    c_vector<double, SPACE_DIM> first_node_location = p_element->GetNodeLocation(0);
    c_vector<double, SPACE_DIM> pos_1 = zero_vector<double>(SPACE_DIM);

    unsigned num_nodes = p_element->GetNumNodes();
    for (unsigned local_index=0; local_index<num_nodes; local_index++)
    {
        c_vector<double, SPACE_DIM> next_node_location = p_element->GetNodeLocation((local_index+1)%num_nodes);
        c_vector<double, SPACE_DIM> pos_2 = GetVectorFromAtoB(first_node_location, next_node_location);

        double this_x = pos_1[0];
        double this_y = pos_1[1];
        double next_x = pos_2[0];
        double next_y = pos_2[1];

        element_volume += 0.5*(this_x*next_y - next_x*this_y);

        pos_1 = pos_2;
    }

    // We take the absolute value just in case the nodes were really oriented clockwise
    return fabs(element_volume);
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
double ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetSurfaceAreaOfElement(unsigned index)
{
    assert(SPACE_DIM == 2);

    // Get pointer to this element
    ImmersedBoundaryElement<ELEMENT_DIM, SPACE_DIM>* p_element = GetElement(index);

    double surface_area = 0.0;
    unsigned num_nodes = p_element->GetNumNodes();
    unsigned this_node_index = p_element->GetNodeGlobalIndex(0);
    for (unsigned local_index=0; local_index<num_nodes; local_index++)
    {
        unsigned next_node_index = p_element->GetNodeGlobalIndex((local_index+1)%num_nodes);

        surface_area += this->GetDistanceBetweenNodes(this_node_index, next_node_index);
        this_node_index = next_node_index;
    }

    return surface_area;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
double ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetAverageNodeSpacingOfElement(unsigned index, bool recalculate)
{
    if (recalculate || (this->GetElement(index)->GetAverageNodeSpacing() == DOUBLE_UNSET) )
    {
        double average_node_spacing = this->GetSurfaceAreaOfElement(index) / this->GetElement(index)->GetNumNodes();
        this->GetElement(index)->SetAverageNodeSpacing(average_node_spacing);

        return average_node_spacing;
    }
    else
    {
        return this->GetElement(index)->GetAverageNodeSpacing();
    }
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
double ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetElementDivisionSpacing()
{
    return mElementDivisionSpacing;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
void ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::SetElementDivisionSpacing(double elementDivisionSpacing)
{
    mElementDivisionSpacing = elementDivisionSpacing;
}

//////////////////////////////////////////////////////////////////////
//                        2D-specific methods                       //
//////////////////////////////////////////////////////////////////////

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
c_vector<double, 3> ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::CalculateMomentsOfElement(unsigned index)
{
    assert(SPACE_DIM == 2);

    // Define helper variables
    ImmersedBoundaryElement<ELEMENT_DIM, SPACE_DIM>* p_element = GetElement(index);
    unsigned num_nodes = p_element->GetNumNodes();
    c_vector<double, 3> moments = zero_vector<double>(3);

    // Since we compute I_xx, I_yy and I_xy about the centroid, we must shift each vertex accordingly
    c_vector<double, SPACE_DIM> centroid = GetCentroidOfElement(index);

    c_vector<double, SPACE_DIM> this_node_location = p_element->GetNodeLocation(0);
    c_vector<double, SPACE_DIM> pos_1 = this->GetVectorFromAtoB(centroid, this_node_location);

    for (unsigned local_index=0; local_index<num_nodes; local_index++)
    {
        unsigned next_index = (local_index+1)%num_nodes;
        c_vector<double, SPACE_DIM> next_node_location = p_element->GetNodeLocation(next_index);
        c_vector<double, SPACE_DIM> pos_2 = this->GetVectorFromAtoB(centroid, next_node_location);

        double signed_area_term = pos_1(0)*pos_2(1) - pos_2(0)*pos_1(1);
        // Ixx
        moments(0) += (pos_1(1)*pos_1(1) + pos_1(1)*pos_2(1) + pos_2(1)*pos_2(1) ) * signed_area_term;

        // Iyy
        moments(1) += (pos_1(0)*pos_1(0) + pos_1(0)*pos_2(0) + pos_2(0)*pos_2(0)) * signed_area_term;

        // Ixy
        moments(2) += (pos_1(0)*pos_2(1) + 2*pos_1(0)*pos_1(1) + 2*pos_2(0)*pos_2(1) + pos_2(0)*pos_1(1)) * signed_area_term;

        pos_1 = pos_2;
    }

    moments(0) /= 12;
    moments(1) /= 12;
    moments(2) /= 24;

    /*
     * If the nodes owned by the element were supplied in a clockwise rather
     * than anticlockwise manner, or if this arose as a result of enforcing
     * periodicity, then our computed quantities will be the wrong sign, so
     * we need to fix this.
     */
    if (moments(0) < 0.0)
    {
        moments(0) = -moments(0);
        moments(1) = -moments(1);
        moments(2) = -moments(2);
    }
    return moments;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
c_vector<double, SPACE_DIM> ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::GetShortAxisOfElement(unsigned index)
{
    assert(SPACE_DIM == 2);

    c_vector<double, SPACE_DIM> short_axis = zero_vector<double>(SPACE_DIM);

    // Calculate the moments of the element about its centroid (recall that I_xx and I_yy must be non-negative)
    c_vector<double, 3> moments = CalculateMomentsOfElement(index);

    // If the principal moments are equal...
    double discriminant = (moments(0) - moments(1))*(moments(0) - moments(1)) + 4.0*moments(2)*moments(2);
    if (fabs(discriminant) < 1e-10) ///\todo remove magic number? (see #1884 and #2401)
    {
        // ...then every axis through the centroid is a principal axis, so return a random unit vector
        short_axis(0) = RandomNumberGenerator::Instance()->ranf();
        short_axis(1) = sqrt(1.0 - short_axis(0)*short_axis(0));
    }
    else
    {
        // If the product of inertia is zero, then the coordinate axes are the principal axes
        if (moments(2) == 0.0)
        {
            if (moments(0) < moments(1))
            {
                short_axis(0) = 0.0;
                short_axis(1) = 1.0;
            }
            else
            {
                short_axis(0) = 1.0;
                short_axis(1) = 0.0;
            }
        }
        else
        {
            // Otherwise we find the eigenvector of the inertia matrix corresponding to the largest eigenvalue
            double lambda = 0.5*(moments(0) + moments(1) + sqrt(discriminant));

            short_axis(0) = 1.0;
            short_axis(1) = (moments(0) - lambda)/moments(2);

            double magnitude = norm_2(short_axis);
            short_axis = short_axis / magnitude;
        }
    }

    return short_axis;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
unsigned ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::DivideElementAlongGivenAxis(ImmersedBoundaryElement<ELEMENT_DIM,SPACE_DIM>* pElement,
                                                                                   c_vector<double, SPACE_DIM> axisOfDivision,
                                                                                   bool placeOriginalElementBelow)
{
    assert(SPACE_DIM == 2);
    assert(ELEMENT_DIM == SPACE_DIM);

    // Get the centroid of the element
    c_vector<double, SPACE_DIM> centroid = this->GetCentroidOfElement(pElement->GetIndex());

    // Create a vector perpendicular to the axis of division
    c_vector<double, SPACE_DIM> perp_axis;
    perp_axis(0) = -axisOfDivision(1);
    perp_axis(1) = axisOfDivision(0);

    /*
     * Find which edges the axis of division crosses by finding any node
     * that lies on the opposite side of the axis of division to its next
     * neighbour.
     */
    unsigned num_nodes = pElement->GetNumNodes();
    std::vector<unsigned> intersecting_nodes;
    bool is_current_node_on_left = (inner_prod(this->GetVectorFromAtoB(pElement->GetNodeLocation(0), centroid), perp_axis) >= 0);
    for (unsigned i=0; i<num_nodes; i++)
    {
        bool is_next_node_on_left = (inner_prod(this->GetVectorFromAtoB(pElement->GetNodeLocation((i+1)%num_nodes), centroid), perp_axis) >= 0);
        if (is_current_node_on_left != is_next_node_on_left)
        {
            intersecting_nodes.push_back(i);
        }
        is_current_node_on_left = is_next_node_on_left;
    }

    // If the axis of division does not cross two edges then we cannot proceed
    if (intersecting_nodes.size() != 2)
    {
        EXCEPTION("Cannot proceed with element division: the given axis of division does not cross two edges of the element");
    }

    // Now call DivideElement() to divide the element using the nodes found above
    unsigned new_element_index = DivideElement(pElement,
                                               pElement->GetNodeLocalIndex(intersecting_nodes[0]),
                                               pElement->GetNodeLocalIndex(intersecting_nodes[1]),
                                               centroid,
                                               axisOfDivision);

    return new_element_index;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
unsigned ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::DivideElementAlongShortAxis(ImmersedBoundaryElement<ELEMENT_DIM,SPACE_DIM>* pElement,
                                                                                   bool placeOriginalElementBelow)
{
    assert(SPACE_DIM == 2);
    assert(ELEMENT_DIM == SPACE_DIM);

    c_vector<double, SPACE_DIM> short_axis = this->GetShortAxisOfElement(pElement->GetIndex());

    unsigned new_element_index = DivideElementAlongGivenAxis(pElement, short_axis, placeOriginalElementBelow);
    return new_element_index;
}

template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
unsigned ImmersedBoundaryMesh<ELEMENT_DIM, SPACE_DIM>::DivideElement(ImmersedBoundaryElement<ELEMENT_DIM,SPACE_DIM>* pElement,
                                                                     unsigned nodeAIndex,
                                                                     unsigned nodeBIndex,
                                                                     c_vector<double, SPACE_DIM> centroid,
                                                                     c_vector<double, SPACE_DIM> axisOfDivision)
{
    assert(SPACE_DIM == 2);
    assert(ELEMENT_DIM == SPACE_DIM);

    if (mElementDivisionSpacing == DOUBLE_UNSET)
    {
        EXCEPTION("The value of mElementDivisionSpacing has not been set.");
    }

    /*
     * Method outline:
     *
     *   Each element needs to end up with the same number of nodes as the original element, and those nodes will be
     *   equally spaced around the outline of each of the two daughter elements.
     *
     *   The two elements need to be divided by a distance of mElementDivisionSpacing, where the distance is measured
     *   perpendicular to the axis of division.
     *
     *   To achieve this, we find four 'corner' locations, each of which has a perpendicular distance from the axis of
     *   half the required spacing, and are found by using the locations from the existing element as a stencil.
     */

    double half_spacing = 0.5 * mElementDivisionSpacing;

    // Get unit vectors in the direction of the division axis, and the perpendicular
    c_vector<double, SPACE_DIM> unit_axis = axisOfDivision / norm_2(axisOfDivision);
    c_vector<double, SPACE_DIM> unit_perp;
    unit_perp[0] = -unit_axis[1];
    unit_perp[1] = unit_axis[0];

    unsigned num_nodes = pElement->GetNumNodes();

    /*
     * We first identify the start and end indices of the nodes which will form the location stencil for each daughter
     * cell.  Our starting point is the node indices already identified.
     *
     * In order to ensure the resulting gap between the elements is the correct size, we remove as many nodes as
     * necessary until the perpendicular distance between the centroid and the node is at least half the required
     * spacing.
     *
     * Finally, we move the relevant node to be exactly half the required spacing.
     */
    unsigned start_a = (nodeAIndex + 1) % num_nodes;
    unsigned end_a = nodeBIndex;

    unsigned start_b = (nodeBIndex + 1) % num_nodes;
    unsigned end_b = nodeAIndex;

    // Find correct start_a
    bool no_node_satisfied_condition_1 = true;
    for (unsigned i = start_a ; i != end_a ;)
    {
        c_vector<double, SPACE_DIM> centroid_to_i = this->GetVectorFromAtoB(centroid, pElement->GetNode(i)->rGetLocation());
        double perpendicular_dist = inner_prod(centroid_to_i, unit_perp);

        if (fabs(perpendicular_dist) >= half_spacing)
        {
            no_node_satisfied_condition_1 = false;
            start_a = i;

            // Calculate position so it's exactly 0.5 * elem_spacing perpendicular distance from the centroid
            c_vector<double, SPACE_DIM> new_location = pElement->GetNode(i)->rGetLocation();
            new_location -= unit_perp * copysign(fabs(perpendicular_dist) - half_spacing, perpendicular_dist);

            pElement->GetNode(i)->SetPoint(ChastePoint<SPACE_DIM>(new_location));
            break;
        }

        // Go to the next node
        i = (i + 1) % num_nodes;
    }

    // Find correct end_a
    bool no_node_satisfied_condition_2 = true;
    for (unsigned i = end_a ; i != start_a ;)
    {
        c_vector<double, SPACE_DIM> centroid_to_i = this->GetVectorFromAtoB(centroid, pElement->GetNode(i)->rGetLocation());
        double perpendicular_dist = inner_prod(centroid_to_i, unit_perp);

        if (fabs(perpendicular_dist) >= half_spacing)
        {
            no_node_satisfied_condition_2 = false;
            end_a = i;

            // Calculate position so it's exactly 0.5 * elem_spacing perpendicular distance from the centroid
            c_vector<double, SPACE_DIM> new_location = pElement->GetNode(i)->rGetLocation();
            new_location -= unit_perp * copysign(fabs(perpendicular_dist) - half_spacing, perpendicular_dist);

            pElement->GetNode(i)->SetPoint(ChastePoint<SPACE_DIM>(new_location));
            break;
        }

        // Go to the previous node
        i = (i + num_nodes - 1) % num_nodes;
    }

    // Find correct start_b
    bool no_node_satisfied_condition_3 = true;
    for (unsigned i = start_b ; i != end_b ;)
    {
        c_vector<double, SPACE_DIM> centroid_to_i = this->GetVectorFromAtoB(centroid, pElement->GetNode(i)->rGetLocation());
        double perpendicular_dist = inner_prod(centroid_to_i, unit_perp);

        if (fabs(perpendicular_dist) >= half_spacing)
        {
            no_node_satisfied_condition_3 = false;
            start_b = i;

            // Calculate position so it's exactly 0.5 * elem_spacing perpendicular distance from the centroid
            c_vector<double, SPACE_DIM> new_location = pElement->GetNode(i)->rGetLocation();
            new_location -= unit_perp * copysign(fabs(perpendicular_dist) - half_spacing, perpendicular_dist);

            pElement->GetNode(i)->SetPoint(ChastePoint<SPACE_DIM>(new_location));
            break;
        }

        // Go to the next node
        i = (i + 1) % num_nodes;
    }

    // Find correct end_b
    bool no_node_satisfied_condition_4 = true;
    for (unsigned i = end_b ; i != start_b ;)
    {
        c_vector<double, SPACE_DIM> centroid_to_i = this->GetVectorFromAtoB(centroid, pElement->GetNode(i)->rGetLocation());
        double perpendicular_dist = inner_prod(centroid_to_i, unit_perp);

        if (fabs(perpendicular_dist) >= half_spacing)
        {
            no_node_satisfied_condition_4 = false;
            end_b = i;

            // Calculate position so it's exactly 0.5 * elem_spacing perpendicular distance from the centroid
            c_vector<double, SPACE_DIM> new_location = pElement->GetNode(i)->rGetLocation();
            new_location -= unit_perp * copysign(fabs(perpendicular_dist) - half_spacing, perpendicular_dist);

            pElement->GetNode(i)->SetPoint(ChastePoint<SPACE_DIM>(new_location));
            break;
        }

        // Go to the previous node
        i = (i + num_nodes - 1) % num_nodes;
    }

    if (no_node_satisfied_condition_1 || no_node_satisfied_condition_2 || no_node_satisfied_condition_3 || no_node_satisfied_condition_4)
    {
        EXCEPTION("Could not space elements far enough apart during cell division.  Cannot currently handle this case");
    }

    /*
     * Create location stencils for each of the daughter cells
     */
    std::vector<c_vector<double, SPACE_DIM> > daughter_a_location_stencil;
    for (unsigned node_idx = start_a; node_idx != (end_a + 1) % num_nodes; )
    {
        daughter_a_location_stencil.push_back(c_vector<double, SPACE_DIM>(pElement->GetNode(node_idx)->rGetLocation()));

        // Go to next node
        node_idx = (node_idx + 1) % num_nodes;
    }

    std::vector<c_vector<double, SPACE_DIM> > daughter_b_location_stencil;
    for (unsigned node_idx = start_b; node_idx != (end_b + 1) % num_nodes; )
    {
        daughter_b_location_stencil.push_back(c_vector<double, SPACE_DIM>(pElement->GetNode(node_idx)->rGetLocation()));

        // Go to next node
        node_idx = (node_idx + 1) % num_nodes;
    }

    assert(daughter_a_location_stencil.size() > 1);
    assert(daughter_b_location_stencil.size() > 1);

    // To help calculating cumulative distances, add the first location on to the end
    daughter_a_location_stencil.push_back(daughter_a_location_stencil[0]);
    daughter_b_location_stencil.push_back(daughter_b_location_stencil[0]);

    // Calculate the cumulative distances around the stencils
    std::vector<double> cumulative_distances_a;
    std::vector<double> cumulative_distances_b;
    cumulative_distances_a.push_back(0.0);
    cumulative_distances_b.push_back(0.0);
    for (unsigned loc_idx = 1; loc_idx < daughter_a_location_stencil.size(); loc_idx++)
    {
        cumulative_distances_a.push_back(cumulative_distances_a.back() +
                                         norm_2(this->GetVectorFromAtoB(daughter_a_location_stencil[loc_idx - 1], daughter_a_location_stencil[loc_idx])));
    }
    for (unsigned loc_idx = 1; loc_idx < daughter_b_location_stencil.size(); loc_idx++)
    {
        cumulative_distances_b.push_back(cumulative_distances_b.back() +
                                         norm_2(this->GetVectorFromAtoB(daughter_b_location_stencil[loc_idx - 1], daughter_b_location_stencil[loc_idx])));
    }

    // Find the target node spacing for each of the daughter elements
    double target_spacing_a = cumulative_distances_a.back() / (double)num_nodes;
    double target_spacing_b = cumulative_distances_b.back() / (double)num_nodes;

    // Move the existing nodes into position to become daughter-A nodes
    unsigned last_idx_used = 0;
    for (unsigned node_idx = 0; node_idx < num_nodes; node_idx++)
    {
        double location_along_arc = (double)node_idx * target_spacing_a;

        while (location_along_arc > cumulative_distances_a[last_idx_used + 1])
        {
            last_idx_used++;
        }

        // Interpolant is the extra distance past the last index used divided by the length of the next line segment
        double interpolant = (location_along_arc - cumulative_distances_a[last_idx_used]) /
                             (cumulative_distances_a[last_idx_used + 1] - cumulative_distances_a[last_idx_used]);

        c_vector<double, SPACE_DIM> this_to_next = this->GetVectorFromAtoB(daughter_a_location_stencil[last_idx_used],
                                                                           daughter_a_location_stencil[last_idx_used + 1]);

        c_vector<double, SPACE_DIM> new_location_a = daughter_a_location_stencil[last_idx_used] + interpolant * this_to_next;

        pElement->GetNode(node_idx)->SetPoint(ChastePoint<SPACE_DIM>(new_location_a));
    }

    // Create new nodes at positions around the daughter-B stencil
    last_idx_used = 0;
    std::vector<Node<SPACE_DIM>*> new_nodes_vec;
    for (unsigned node_idx = 0; node_idx < num_nodes; node_idx++)
    {
        double location_along_arc = (double)node_idx * target_spacing_b;

        while (location_along_arc > cumulative_distances_b[last_idx_used + 1])
        {
            last_idx_used++;
        }

        // Interpolant is the extra distance past the last index used divided by the length of the next line segment
        double interpolant = (location_along_arc - cumulative_distances_b[last_idx_used]) /
                             (cumulative_distances_b[last_idx_used + 1] - cumulative_distances_b[last_idx_used]);

        c_vector<double, SPACE_DIM> this_to_next = this->GetVectorFromAtoB(daughter_b_location_stencil[last_idx_used],
                                                                           daughter_b_location_stencil[last_idx_used + 1]);

        c_vector<double, SPACE_DIM> new_location_b = daughter_b_location_stencil[last_idx_used] + interpolant * this_to_next;

        unsigned new_node_idx = this->mNodes.size();
        this->mNodes.push_back(new Node<SPACE_DIM>(new_node_idx, new_location_b, true));
        new_nodes_vec.push_back(this->mNodes.back());
    }

    // Copy node attributes
    for (unsigned node_idx = 0; node_idx < num_nodes; node_idx++)
    {
        new_nodes_vec[node_idx]->SetRegion(pElement->GetNode(node_idx)->GetRegion());

        for (unsigned node_attribute = 0; node_attribute < pElement->GetNode(node_idx)->GetNumNodeAttributes(); node_attribute++)
        {
            new_nodes_vec[node_idx]->AddNodeAttribute(pElement->GetNode(node_idx)->rGetNodeAttributes()[node_attribute]);
        }
    }

    // Create the new element
    unsigned new_elem_idx = this->mElements.size();
    this->mElements.push_back(new ImmersedBoundaryElement<ELEMENT_DIM,SPACE_DIM>(new_elem_idx, new_nodes_vec));
    this->mElements.back()->RegisterWithNodes();

    // Copy any element attributes
    for (unsigned elem_attribute = 0; elem_attribute < pElement->GetNumElementAttributes(); elem_attribute++)
    {
        this->mElements.back()->AddElementAttribute(pElement->rGetElementAttributes()[elem_attribute]);
    }

    // Add the necessary corners to keep consistency with the other daughter element
    for (unsigned corner = 0; corner < pElement->rGetCornerNodes().size(); corner ++)
    {
        this->mElements.back()->rGetCornerNodes().push_back(pElement->rGetCornerNodes()[corner]);
    }

    // Update fluid source location for the existing element
    pElement->GetFluidSource()->rGetModifiableLocation() = this->GetCentroidOfElement(pElement->GetIndex());

    // Add a fluid source for the new element
    c_vector<double, SPACE_DIM> new_centroid = this->GetCentroidOfElement(new_elem_idx);
    mElementFluidSources.push_back(new FluidSource<SPACE_DIM>(new_elem_idx, new_centroid));

    // Set source parameters
    mElementFluidSources.back()->SetAssociatedElementIndex(new_elem_idx);
    mElementFluidSources.back()->SetStrength(0.0);

    // Associate source with element
    mElements[new_elem_idx]->SetFluidSource(mElementFluidSources.back());

    return new_elem_idx;
}

// Explicit instantiation
template class ImmersedBoundaryMesh<1,1>;
template class ImmersedBoundaryMesh<1,2>;
template class ImmersedBoundaryMesh<1,3>;
template class ImmersedBoundaryMesh<2,2>;
template class ImmersedBoundaryMesh<2,3>;
template class ImmersedBoundaryMesh<3,3>;

// Serialization for Boost >= 1.36
#include "SerializationExportWrapperForCpp.hpp"
EXPORT_TEMPLATE_CLASS_ALL_DIMS(ImmersedBoundaryMesh)
