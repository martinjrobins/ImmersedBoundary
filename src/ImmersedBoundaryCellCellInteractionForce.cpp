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

#include "ImmersedBoundaryCellCellInteractionForce.hpp"
#include "ImmersedBoundaryElement.hpp"

template<unsigned DIM>
ImmersedBoundaryCellCellInteractionForce<DIM>::ImmersedBoundaryCellCellInteractionForce()
        : AbstractImmersedBoundaryForce<DIM>(),
          mpMesh(NULL),
          mSpringConst(1e3),
          mRestLength(DOUBLE_UNSET),
          mNumProteins(3)
{
}

template<unsigned DIM>
ImmersedBoundaryCellCellInteractionForce<DIM>::~ImmersedBoundaryCellCellInteractionForce()
{
}

template<unsigned DIM>
void ImmersedBoundaryCellCellInteractionForce<DIM>::AddImmersedBoundaryForceContribution(std::vector<std::pair<Node<DIM>*, Node<DIM>*> >& rNodePairs,
        ImmersedBoundaryCellPopulation<DIM>& rCellPopulation)
{
    /*
     * This force class calculates the force between pairs of nodes in different immersed boundaries.  Each node must
     * therefore store a dimensionless parameter representing the quantity of different transmembrane proteins at that
     * location.  We attach these quantities as node attributes, and keep track of where in the node attributes vector
     * each protein concentration is stored.
     */

    // This will be triggered only once - during simulation set up
    if (mProteinNodeAttributeLocations.empty())
    {
        mpMesh = &(rCellPopulation.rGetMesh());

        mRestLength = 0.25 * rCellPopulation.GetInteractionDistance();

        // First verify that all nodes have the same number of attributes
        unsigned num_node_attributes = rCellPopulation.GetNode(0)->GetNumNodeAttributes();
        for (unsigned node_idx = 0; node_idx < rCellPopulation.GetNumNodes(); node_idx++ )
        {
            if (num_node_attributes != rCellPopulation.GetNode(node_idx)->GetNumNodeAttributes())
            {
                EXCEPTION("All nodes must have the same number of attributes to use this force class.");
            }
        }

        // Set up the number of proteins and keep track of where they will be stored in the node attributes vector
        for (unsigned protein_idx = 0; protein_idx < mNumProteins; protein_idx++)
        {
            mProteinNodeAttributeLocations.push_back(num_node_attributes + protein_idx);
        }

        // Add protein attributes to each node
        for (unsigned node_idx = 0; node_idx < rCellPopulation.GetNumNodes(); node_idx++)
        {
            for (unsigned protein_idx = 0; protein_idx < mNumProteins; protein_idx++)
            {
                rCellPopulation.GetNode(node_idx)->AddNodeAttribute(0.0);
            }
        }

        // Initialize protein levels
        InitializeProteinLevels();
    }

    UpdateProteinLevels();

    // Helper variables for loop
    unsigned e_cad_idx = mProteinNodeAttributeLocations[0];
    unsigned p_cad_idx = mProteinNodeAttributeLocations[1];
    unsigned integrin_idx = mProteinNodeAttributeLocations[2];

    double normed_dist;
    double protein_mult;

    // The spring constant will be scaled by an amount determined by the intrinsic spacing
    double intrinsic_spacing = rCellPopulation.GetIntrinsicSpacing();
    double node_a_elem_spacing;
    double node_b_elem_spacing;
    double elem_spacing;

    // The effective spring constant will be a scaled version of mSpringConst
    double effective_spring_const;

    c_vector<double, DIM> vector_between_nodes;
    c_vector<double, DIM> force_a_to_b;
    c_vector<double, DIM> force_b_to_a;

    Node<DIM>* p_node_a;
    Node<DIM>* p_node_b;

    // If using Morse potential, this can be pre-calculated
    double well_width = 0.25 * rCellPopulation.GetInteractionDistance();

    // Loop over all pairs of nodes that might be interacting
    for (unsigned pair = 0; pair < rNodePairs.size(); pair++)
    {
        /*
         * Interactions only occur between different cells.  Since each node is only ever in a single cell, we can test
         * equality of the first ContainingElement.
         */
        if ( *(rNodePairs[pair].first->ContainingElementsBegin()) !=
             *(rNodePairs[pair].second->ContainingElementsBegin()) )
        {
            p_node_a = rNodePairs[pair].first;
            p_node_b = rNodePairs[pair].second;

            std::vector<double>& r_a_attribs = p_node_a->rGetNodeAttributes();
            std::vector<double>& r_b_attribs = p_node_b->rGetNodeAttributes();

            vector_between_nodes = mpMesh->GetVectorFromAtoB(p_node_a->rGetLocation(), p_node_b->rGetLocation());
            normed_dist = norm_2(vector_between_nodes);

            if (normed_dist < rCellPopulation.GetInteractionDistance())
            {
                // Get the element spacing for each of the nodes concerned and calculate the effective spring constant
                node_a_elem_spacing = mpMesh->GetAverageNodeSpacingOfElement(*(p_node_a->rGetContainingElementIndices().begin()), false);
                node_b_elem_spacing = mpMesh->GetAverageNodeSpacingOfElement(*(p_node_b->rGetContainingElementIndices().begin()), false);
                elem_spacing = 0.5 * (node_a_elem_spacing + node_b_elem_spacing);

                effective_spring_const = mSpringConst * elem_spacing / intrinsic_spacing;

                // The protein multiplier is a function of the levels of each protein in the current and comparison nodes
                protein_mult = std::min(r_a_attribs[e_cad_idx], r_b_attribs[e_cad_idx]) +
                               std::min(r_a_attribs[p_cad_idx], r_b_attribs[p_cad_idx]) +
                               std::max(r_a_attribs[integrin_idx], r_b_attribs[integrin_idx]);

                /*
                 * We must scale each applied force by a factor of elem_spacing / local spacing, so that forces
                 * balance when spread to the grid later (where the multiplicative factor is the local spacing)
                 */

                if (mLinearSpring)
                {
                    vector_between_nodes *=
                            effective_spring_const * protein_mult * (normed_dist - mRestLength) / normed_dist;
                }
                else // Morse potential
                {
                    double morse_exp = exp((mRestLength - normed_dist) / well_width);
                    vector_between_nodes *= 2.0 * well_width * effective_spring_const * protein_mult * morse_exp *
                                            (1.0 - morse_exp) / normed_dist;

                }

                force_a_to_b = vector_between_nodes * elem_spacing / node_a_elem_spacing;
                p_node_a->AddAppliedForceContribution(force_a_to_b);

                force_b_to_a = -1.0 * vector_between_nodes * elem_spacing / node_b_elem_spacing;
                p_node_b->AddAppliedForceContribution(force_b_to_a);
            }
        }
    }
}

template<unsigned DIM>
const std::vector<unsigned>& ImmersedBoundaryCellCellInteractionForce<DIM>::rGetProteinNodeAttributeLocations() const
{
    return mProteinNodeAttributeLocations;
}

template<unsigned DIM>
void ImmersedBoundaryCellCellInteractionForce<DIM>::InitializeProteinLevels()
{
    /*
     * We are thinking of the following proteins:
     *  * 0: E-cadherin
     *  * 1: P-cadherin
     *  * 2: Integrins
     */
    for (unsigned elem_idx = 0; elem_idx < mpMesh->GetNumElements(); elem_idx++)
    {
        double e_cad = 0.0;
        double p_cad = 0.0;
        double integrin = 0.0;

        if (mpMesh->GetElement(elem_idx) == mpMesh->GetMembraneElement())
        {
            e_cad = 1.0;
        }
        else
        {
            e_cad = 1.0;
        }

        for (unsigned node_idx = 0; node_idx < mpMesh->GetElement(elem_idx)->GetNumNodes(); node_idx++)
        {
            std::vector<double>& r_node_attributes = mpMesh->GetElement(elem_idx)->GetNode(node_idx)->rGetNodeAttributes();

            r_node_attributes[mProteinNodeAttributeLocations[0]] += e_cad;
            r_node_attributes[mProteinNodeAttributeLocations[1]] += p_cad;
            r_node_attributes[mProteinNodeAttributeLocations[2]] += integrin;
        }
    }
}

template<unsigned DIM>
void ImmersedBoundaryCellCellInteractionForce<DIM>::UpdateProteinLevels()
{
    ///\todo Do something in this method?
}

template<unsigned DIM>
void ImmersedBoundaryCellCellInteractionForce<DIM>::SetSpringConstant(double springConst)
{
    mSpringConst = springConst;
}

template<unsigned DIM>
double ImmersedBoundaryCellCellInteractionForce<DIM>::GetSpringConstant()
{
    return mSpringConst;
}

template<unsigned DIM>
void ImmersedBoundaryCellCellInteractionForce<DIM>::SetRestLength(double restLength)
{
    mRestLength = restLength;
}

template<unsigned DIM>
double ImmersedBoundaryCellCellInteractionForce<DIM>::GetRestLength()
{
    return mRestLength;
}

template<unsigned DIM>
void ImmersedBoundaryCellCellInteractionForce<DIM>::UseLinearSpringLaw()
{
    mLinearSpring = true;
    mMorse = false;
}

template<unsigned DIM>
void ImmersedBoundaryCellCellInteractionForce<DIM>::UseMorsePotential()
{
    mLinearSpring = false;
    mMorse = true;
}

template<unsigned DIM>
bool ImmersedBoundaryCellCellInteractionForce<DIM>::IsLinearSpringLaw()
{
    return mLinearSpring;
}

template<unsigned DIM>
bool ImmersedBoundaryCellCellInteractionForce<DIM>::IsMorsePotential()
{
    return mMorse;
}

template<unsigned DIM>
void ImmersedBoundaryCellCellInteractionForce<DIM>::OutputImmersedBoundaryForceParameters(out_stream& rParamsFile)
{
    *rParamsFile << "\t\t\t<SpringConst>" << mSpringConst << "</SpringConst>\n";
    *rParamsFile << "\t\t\t<RestLength>" << mRestLength << "</RestLength>\n";
    *rParamsFile << "\t\t\t<NumProteins>" << mNumProteins << "</NumProteins>\n";
    *rParamsFile << "\t\t\t<LinearSpring>" << mLinearSpring << "</LinearSpring>\n";
    *rParamsFile << "\t\t\t<Morse>" << mMorse << "</Morse>\n";

    // Call method on direct parent class
    AbstractImmersedBoundaryForce<DIM>::OutputImmersedBoundaryForceParameters(rParamsFile);
}

// Explicit instantiation
template class ImmersedBoundaryCellCellInteractionForce<1>;
template class ImmersedBoundaryCellCellInteractionForce<2>;
template class ImmersedBoundaryCellCellInteractionForce<3>;

// Serialization for Boost >= 1.36
#include "SerializationExportWrapperForCpp.hpp"
EXPORT_TEMPLATE_CLASS_SAME_DIMS(ImmersedBoundaryCellCellInteractionForce)
