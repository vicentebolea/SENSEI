#ifndef HDF5_DataAdaptor_h
#define HDF5_DataAdaptor_h

#include "DataAdaptor.h"

#include <mpi.h>
#include <hdf5.h>
#include <map>
#include <string>
#include <vtkSmartPointer.h>

#include "HDF5Schema.h"

namespace sensei
{

//
// read from HDF and construct VTK object
//
class HDF5DataAdaptor : public DataAdaptor
{
public:
  static HDF5DataAdaptor* New();
  senseiTypeMacro(HDF5DataAdaptor, DataAdaptor);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  int Open( const std::string& filename);
  
  void SetStreaming(bool s) {m_Streaming = s;}
  void SetCollective(bool s) {m_Collective = s;}

  int Advance();

  int Close();

  /// SENSEI DataAdaptor API
  int GetNumberOfMeshes(unsigned int &numMeshes) override;

  int GetMeshMetadata(unsigned int id, MeshMetadataPtr &metadata) override;

  int GetMesh(const std::string &meshName, bool structure_only,
	      vtkDataObject *&mesh) override;

  int AddGhostNodesArray(vtkDataObject* mesh, const std::string &meshName) override;
  int AddGhostCellsArray(vtkDataObject* mesh, const std::string &meshName) override;

  int AddArray(vtkDataObject* mesh, const std::string &meshName,
	       int association, const std::string &arrayName) override;

  int ReleaseData() override;

protected:
  HDF5DataAdaptor();
  ~HDF5DataAdaptor();

  // reads the current time step and time values from the stream
  // stores them in the base class information object
  int UpdateTimeStep();

private:
  //struct InternalsType;
  //InternalsType *Internals;
  senseiHDF5::ReadStream* m_HDF5Reader;

  bool m_Streaming = false;
  bool m_Collective = false;

  HDF5DataAdaptor(const HDF5DataAdaptor&) = delete;
  void operator=(const HDF5DataAdaptor&) = delete;
};

}

#endif