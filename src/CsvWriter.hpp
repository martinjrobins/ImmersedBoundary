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

#ifndef CSVWRITER_HPP_
#define CSVWRITER_HPP_

#include <string>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include "Exception.hpp"

/**
 * A class to write simple data collected during simulations to a CSV file.
 */
class CsvWriter
{
private:
    /** The directory for output */
    std::string mDirectoryName;

    /** The output file name */
    std::string mFileName;

    /** The number of data points to be expected */
    unsigned mDataLength;

    /** Whether mDataLength has been set yet */
    bool mDataLengthSet;

    /** Whether a header row is to be written to the file */
    bool mHeader;

    /** Vector strings for header row */
    std::vector<std::string> mHeaderStrings;

    /** Vector of vectors containing unsigned integer data */
    std::vector<std::vector<unsigned> > mVecUnsigned;

    /** Vector of vectors containing double precision values */
    std::vector<std::vector<double> > mVecDoubles;

    /** Vector of vectors containing strings */
    std::vector<std::vector<std::string> > mVecStrings;

    /**
     * Helper method for AddData()
     *
     * @param length of data vector to be added to this writer
     */
    void ValidateNewData(unsigned dataLength);

public:

    /**
     * Default constructor.
     */
    CsvWriter();

    /**
     * Destructor.
     */
    virtual ~CsvWriter();

    /**
     * Add header row.
     *
     * @param rData reference to new unsigned data
     */
    void AddHeaders(std::vector<std::string>& rHeaders);

    /**
     * Add a new vector of data to mVecUnsigned.
     *
     * @param rData reference to new unsigned data
     */
    void AddData(std::vector<unsigned>& rData);

    /**
     * Add a new vector of data to mVecDoubles.
     *
     * @param rData reference to new unsigned data
     */
    void AddData(std::vector<double>& rData);

    /**
     * Add a new vector of data to mVecStrings.
     *
     * @param rData reference to new unsigned data
     */
    void AddData(std::vector<std::string>& rData);

    /**
     * Write the data from member vectors to file.
     */
    void WriteDataToFile();

    /**
     * @return the directory that the output will be written to
     */
    const std::string& GetDirectoryName() const;

    /**
     * @return the output file name
     */
    const std::string& GetFileName() const;

    /**
     * @param the directory that the output will be written to
     */
    void SetDirectoryName(std::string directoryName);

    /**
     * @param the output file name
     */
    void SetFileName(std::string fileName);
};

#endif /*CSVWRITER_HPP_*/