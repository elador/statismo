/*
 * This file is part of the statismo library.
 *
 * Author: Marcel Luethi (marcel.luethi@unibas.ch)
 *
 * Copyright (c) 2011 University of Basel
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the project's author nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef __VTKPOLYDATAREPRESENTER_CPP
#define __VTKPOLYDATAREPRESENTER_CPP

#include "vtkPoints.h"
#include "vtkPolyDataReader.h"
#include "vtkPolyDataWriter.h"
#include "statismo/HDF5Utils.h"

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#endif

using statismo::VectorType;
using statismo::HDF5Utils;
using statismo::StatisticalModelException;

inline
vtkPolyDataRepresenter::vtkPolyDataRepresenter(DatasetConstPointerType reference, AlignmentType alignment)
  :
        m_alignment(alignment),
        m_pdTransform(vtkTransformPolyDataFilter::New())
{
	   m_reference = vtkPolyData::New();
	   m_reference->DeepCopy(const_cast<DatasetPointerType>(reference));
}

inline
vtkPolyDataRepresenter::vtkPolyDataRepresenter(const std::string& referenceFilename, AlignmentType alignment)
  :
        m_alignment(alignment),
        m_pdTransform(vtkTransformPolyDataFilter::New())
{
		m_reference = ReadDataset(referenceFilename);
}

inline
vtkPolyDataRepresenter::~vtkPolyDataRepresenter() {
	if (m_pdTransform != 0) {
		m_pdTransform->Delete();
		m_pdTransform = 0;
	}
	if (m_reference != 0) {
		m_reference->Delete();
		m_reference = 0;
	}
}

inline
vtkPolyDataRepresenter*
vtkPolyDataRepresenter::Clone() const
{
	// this works since Create deep copies the reference
	return Create(m_reference, m_alignment);
}

inline
vtkPolyDataRepresenter*
vtkPolyDataRepresenter::Load(const H5::CommonFG& fg) {


#ifdef _WIN32
	std::string tmpDirectoryName;
	TCHAR szTempFileName[MAX_PATH];
	DWORD dwRetVal = 0;
	//  Gets the temp path env string (no guarantee it's a valid path).
    dwRetVal = GetTempPath(MAX_PATH,          // length of the buffer
                           szTempFileName); // buffer for path
	tmpDirectoryName.assign(szTempFileName);
	std::string tmpfilename = tmpDirectoryName + "/" + tmpnam(0);
#else
	std::string tmpfilename = tmpnam(0);
#endif

	HDF5Utils::getFileFromHDF5(fg, "./reference", tmpfilename.c_str());
	DatasetConstPointerType ref = ReadDataset(tmpfilename.c_str());
	std::remove(tmpfilename.c_str());

	int alignment = static_cast<AlignmentType>(HDF5Utils::readInt(fg, "./alignment"));
	return vtkPolyDataRepresenter::Create(ref, AlignmentType(alignment));

}


inline
VectorType
vtkPolyDataRepresenter::DatasetToSampleVector(DatasetConstPointerType _pd ) const
{
	assert(m_reference != 0);
	VectorType sample = VectorType::Zero(m_reference->GetNumberOfPoints() * 3);

	vtkPolyData* reference = const_cast<vtkPolyData*>(this->m_reference);
	vtkPolyData* pd = const_cast<vtkPolyData*>(_pd);

	vtkLandmarkTransform* transform = vtkLandmarkTransform::New();
	vtkPolyData* alignedPd  = 0;

	if (m_alignment != NONE) { 
	  // we align all the dataset to the common reference

	  transform->SetSourceLandmarks(pd->GetPoints());
	  transform->SetTargetLandmarks(m_reference->GetPoints());
	  transform->SetMode(m_alignment);

	  m_pdTransform->SetInput(pd);
	  m_pdTransform->SetTransform(transform);
	  m_pdTransform->Update();

	  alignedPd = m_pdTransform->GetOutput();

	}
	else { 
	  // no alignment needed
	  alignedPd = pd; 
	}

	// TODO make this more efficient using SetVoidArray of vtk
	for (unsigned i = 0 ; i < m_reference->GetNumberOfPoints(); i++) {
		for (unsigned j = 0; j < 3; j++) {
			unsigned idx = MapPointIdToInternalIdx(i, j);
			sample(idx) = alignedPd->GetPoint(i)[j];
		}
	}
	transform->Delete();
	return sample;
}

inline
vtkPolyDataRepresenter::DatasetPointerType
vtkPolyDataRepresenter::SampleVectorToSample(const VectorType& sample) const
{

	assert (m_reference != 0);

	vtkPolyData* reference = const_cast<vtkPolyData*>(m_reference);
	vtkPolyData* pd = vtkPolyData::New();
	pd->DeepCopy(reference);

	vtkPoints* points = pd->GetPoints();
	for (unsigned i = 0; i < reference->GetNumberOfPoints(); i++) {
		vtkPoint pt;
		for (unsigned d = 0; d < GetDimensions(); d++) {
			unsigned idx = MapPointIdToInternalIdx(i, d);
			pt[d] = sample(idx);
		}
		points->SetPoint(i, pt.data());
	}

	return pd;
}

inline
vtkPolyDataRepresenter::ValueType
vtkPolyDataRepresenter::PointSampleToValue(const VectorType& pointSample) const
{
	ValueType value;
	const VectorType& v = pointSample;
	for (unsigned i = 0; i < GetDimensions(); i++) {
		value[i] = v(i);
	}
	return value;
}

inline
statismo::VectorType vtkPolyDataRepresenter::ValueToPointSample(const ValueType& v) const
{
	VectorType vec(GetDimensions());
	for (unsigned i = 0; i < GetDimensions(); i++) {
		vec(i) = v[i];
	}
	return vec;
}

inline
void
vtkPolyDataRepresenter::Save(const H5::CommonFG& fg) const {
	using namespace H5;

#ifdef _WIN32
	std::string tmpDirectoryName;
	TCHAR szTempFileName[MAX_PATH];
	DWORD dwRetVal = 0;
	//  Gets the temp path env string (no guarantee it's a valid path).
    dwRetVal = GetTempPath(MAX_PATH,          // length of the buffer
                           szTempFileName); // buffer for path
	tmpDirectoryName.assign(szTempFileName);
	std::string tmpfilename = tmpDirectoryName + "/" + tmpnam(0);
#else
	std::string tmpfilename = tmpnam(0);
#endif


	WriteDataset(tmpfilename.c_str(), this->m_reference);

	HDF5Utils::dumpFileToHDF5(tmpfilename.c_str(), fg, "./reference" );

	std::remove(tmpfilename.c_str());
	HDF5Utils::writeInt(fg, "./alignment", m_alignment);

}


inline
unsigned
vtkPolyDataRepresenter::GetPointIdForPoint(const PointType& pt) const {
	assert (m_reference != 0);
    return this->m_reference->FindPoint(const_cast<double*>(pt.data()));
}

inline
unsigned
vtkPolyDataRepresenter::GetNumberOfPoints() const {
	assert (m_reference != 0);

    return this->m_reference->GetNumberOfPoints();
}


inline
vtkPolyDataRepresenter::DatasetPointerType
vtkPolyDataRepresenter::ReadDataset(const std::string& filename) {
	vtkPolyData* pd = vtkPolyData::New();

    vtkPolyDataReader* reader = vtkPolyDataReader::New();
    reader->SetFileName(filename.c_str());
    reader->Update();
    if (reader->GetErrorCode() != 0) {
        throw StatisticalModelException((std::string("Could not read file ") + filename).c_str());
    }
    pd->ShallowCopy(reader->GetOutput());
    reader->Delete();
    return pd;
}

inline
void vtkPolyDataRepresenter::WriteDataset(const std::string& filename,DatasetConstPointerType pd) {
    vtkPolyDataWriter* writer = vtkPolyDataWriter::New();
    writer->SetFileName(filename.c_str());
    writer->SetInput(const_cast<vtkPolyData*>(pd));
    writer->Update();
    if (writer->GetErrorCode() != 0) {
        throw StatisticalModelException((std::string("Could not read file ") + filename).c_str());
    }
    writer->Delete();
}

inline
vtkPolyDataRepresenter::DatasetPointerType vtkPolyDataRepresenter::NewDataset() {
    return vtkPolyData::New();
}

inline
void vtkPolyDataRepresenter::DeleteDataset(DatasetPointerType d) {
    d->Delete();
}

#endif // __VTKPOLYDATAREPRESENTER_CPP