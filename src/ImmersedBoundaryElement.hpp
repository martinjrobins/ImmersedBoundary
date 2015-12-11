/*

Copyright (c) 2005-2015, University of Oxford.
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
#ifndef IMMERSEDBOUNDARYELEMENT_HPP_
#define IMMERSEDBOUNDARYELEMENT_HPP_

#include "MutableElement.hpp"

#include "ChasteSerialization.hpp"
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>

/**
 * An element class for use in the ImmersedBoundaryMesh class. The main
 * difference between this and the Element class is that a
 * ImmersedBoundaryElement can have a variable number of nodes associated
 * with it.
 */
template<unsigned ELEMENT_DIM, unsigned SPACE_DIM>
class ImmersedBoundaryElement : public MutableElement<ELEMENT_DIM, SPACE_DIM>
{
private:

    /** Spring constant associated with the immersed boundary element membrane. */
    double mMembraneSpringConstant;

    /** Spring rest length associated with the immersed boundary element membrane. */
    double mMembraneRestLength;

    /** Spring constant associated cell-cell interactions from this element. */
    double mCellCellSpringConstant;

    /** Spring rest length associated with cell-cell interactions from this element. */
    double mCellCellRestLength;

    /** Node representing a fluid source associated with this element. */
    Node<SPACE_DIM>* mpSourceNode;

    /** Needed for serialization. */
    friend class boost::serialization::access;

    /**
     * Serialize the object and its member variables.
     *
     * Note that serialization of the mesh and cells is handled by load/save_construct_data.
     *
     * Note also that member data related to writers is not saved - output must
     * be set up again by the caller after a restart.
     *
     * @param archive the archive
     * @param version the current version of this class
     */
    template<class Archive>
    void serialize(Archive & archive, const unsigned int version)
    {
        // This needs to be first so that MeshBasedCellPopulation::Validate() doesn't go mental.
        archive & mMembraneSpringConstant;
        archive & mMembraneRestLength;
        archive & boost::serialization::base_object<MutableElement<ELEMENT_DIM, SPACE_DIM> >(*this);
    }

public:

    /**
     * Constructor.
     *
     * @param index global index of the element
     * @param rNodes vector of Nodes associated with the element
     */
    ImmersedBoundaryElement(unsigned index,
                            const std::vector<Node<SPACE_DIM>*>& rNodes);

    /**
     * Destructor.
     */
    ~ImmersedBoundaryElement();

    /**
     * Set mMembraneSpringConstant
     *
     * @param the new spring constant
     */
    void SetMembraneSpringConstant(double springConstant);

    /**
     * Set mRestLength
     *
     * @param the new rest length
     */
    void SetMembraneRestLength(double restLength);

    /**
     * Set mCellCellSpringConstant
     *
     * @param the new spring constant
     */
    void SetCellCellSpringConstant(double springConstant);

    /**
     * Set mRestLength
     *
     * @param the new rest length
     */
    void SetCellCellRestLength(double restLength);

    /**
     * Get mMembraneSpringConstant
     *
     * @return the current spring constant
     */
    double GetMembraneSpringConstant(void);

    /**
     * Get mRestLength
     *
     * @return the current rest length
     */
    double GetMembraneRestLength(void);

    /**
     * Get mCellCellSpringConstant
     *
     * @return the current spring constant
     */
    double GetCellCellSpringConstant(void);

    /**
     * Get mCellCellRestLength
     *
     * @return the current rest length
     */
    double GetCellCellRestLength(void);

    /**
     * Get the source node
     *
     * @return pointer to the source node
     */
    Node<SPACE_DIM>* GetSourceNode(void);

};


//////////////////////////////////////////////////////////////////////
//                  Specialization for 1d elements                  //
//                                                                  //
//                 1d elements are just edges (lines)               //
//////////////////////////////////////////////////////////////////////

/**
 * Specialization for 1d elements so we don't get errors from Boost on some
 * compilers.
 */
template<unsigned SPACE_DIM>
class ImmersedBoundaryElement<1, SPACE_DIM> : public MutableElement<1,SPACE_DIM>
{
public:

    /**
     * Constructor which takes in a vector of nodes.
     *
     * @param index  the index of the element in the mesh
     * @param rNodes the nodes owned by the element
     */
    ImmersedBoundaryElement(unsigned index, const std::vector<Node<SPACE_DIM>*>& rNodes);

    /**
     * Set mMembraneSpringConstant
     *
     * @param the new spring constant
     */
    void SetMembraneSpringConstant(double springConstant);

    /**
     * Set mRestLength
     *
     * @param the new rest length
     */
    void SetMembraneRestLength(double restLength);

    /**
     * Set mCellCellSpringConstant
     *
     * @param the new spring constant
     */
    void SetCellCellSpringConstant(double springConstant);

    /**
     * Set mRestLength
     *
     * @param the new rest length
     */
    void SetCellCellRestLength(double restLength);

    /**
     * Get mMembraneSpringConstant
     *
     * @return the current spring constant
     */
    double GetMembraneSpringConstant(void);

    /**
     * Get mMembraneRestLength
     *
     * @return the current rest length
     */
    double GetMembraneRestLength(void);

    /**
     * Get mCellCellSpringConstant
     *
     * @return the current spring constant
     */
    double GetCellCellSpringConstant(void);

    /**
     * Get mCellCellRestLength
     *
     * @return the current rest length
     */
    double GetCellCellRestLength(void);

    /**
     * Get the source node
     *
     * @return pointer to the source node
     */
    Node<SPACE_DIM>* GetSourceNode(void);

};

#endif /*IMMERSEDBOUNDARYELEMENT_HPP_*/
