#include <mpi.h>

#include <conduit_blueprint.hpp>

#include <vtkObjectFactory.h>
#include <vtkCellArray.h>
#include <vtkDataObject.h>
#include <vtkDataArray.h>
#include <vtkFieldData.h>
#include <vtkDataSet.h>
#include <vtkDataSetAttributes.h>
#include <vtkCompositeDataIterator.h>
#include <vtkCompositeDataSet.h>
#include <vtkImageData.h>
#include <vtkRectilinearGrid.h>
#include <vtkStructuredGrid.h>
#include <vtkUnstructuredGrid.h>
#include <vtkCellData.h>
#include <vtkPointData.h>
#include <vtkAOSDataArrayTemplate.h>
#include <vtkSOADataArrayTemplate.h>
#include <vtkDataArrayTemplate.h>
#include <vtkUnsignedCharArray.h>

#include "AscentAnalysisAdaptor.h"
#include "DataAdaptor.h"
#include "Error.h"
#include "VTKUtils.h"

namespace sensei
{

//#define DEBUG_SAVE_DATA
#ifdef DEBUG_SAVE_DATA
void DebugSaveAscentData( conduit::Node &data, conduit::Node &_optionsNode )
{
    ascent::Ascent a;

    std::cout << "DebugSaveAscentData" << std::endl;

    // Open ascent
    a.open( _optionsNode );

    // Publish data to ascent
    a.publish( data );

    conduit::Node extracts;
    extracts["e1/type"] = "relay";
    extracts["e1/params/path"] = "debugAscentDataSave";
    extracts["e1/params/protocol"] = "blueprint/mesh/hdf5";

    // Setup actions
    conduit::Node actions;
    conduit::Node &add_act = actions.append();
    add_act["action"] = "add_extracts";
    add_act["extracts"] = extracts;

    actions.append()["action"] = "execute";

    // execute
    a.execute(actions);
    std::cout << "DebugSaveAscentData: execute" << std::endl;

    // close ascent
    a.close();
}
#else
void DebugSaveAscentData( conduit::Node &, conduit::Node & ) {}
#endif  // DEBUG_SAVE_DATA

//------------------------------------------------------------------------------
senseiNewMacro(AscentAnalysisAdaptor);

//------------------------------------------------------------------------------
AscentAnalysisAdaptor::AscentAnalysisAdaptor()
{
}

//------------------------------------------------------------------------------
AscentAnalysisAdaptor::~AscentAnalysisAdaptor()
{
}

//------------------------------------------------------------------------------
template<typename n_t> struct conduit_tt {};

#define declare_conduit_tt(cpp_t, conduit_t) \
template<> struct conduit_tt<cpp_t>          \
{                                            \
  using conduit_type = conduit_t;            \
};

declare_conduit_tt(char, conduit::int8);
declare_conduit_tt(signed char, conduit::int8);
declare_conduit_tt(unsigned char, conduit::uint8);
declare_conduit_tt(short, conduit::int16);
declare_conduit_tt(unsigned short, conduit::uint16);
declare_conduit_tt(int, conduit::int32);
declare_conduit_tt(unsigned int, conduit::uint32);
declare_conduit_tt(long, conduit::int32);
declare_conduit_tt(unsigned long, conduit::uint32);
declare_conduit_tt(long long, conduit::int64);
declare_conduit_tt(unsigned long long, conduit::uint64);
declare_conduit_tt(float, conduit::float32);
declare_conduit_tt(double, conduit::float64);
declare_conduit_tt(long double, conduit::float64);

//------------------------------------------------------------------------------
void GetShape(std::string &shape, int type)
{
  if(type == 1) shape = "point";
  else if(type == 2) shape = "line";
  else if(type == 3) shape = "tri";
  else if(type == 4) shape = "quad";
  else if(type == 8) shape = "hex";
  else SENSEI_ERROR("Error: Unsupported element shape");
}

/* TODO look at
int VTK_To_Fields(vtkDataSetAttributes *dsa, int centering, conduit::Node &node)
{
  int nArrays = dsa->GetNumberOfArrays();
  for (int i = 0; i < nArrays; ++i)
  {
    vtkDataArray *da = dsa->GetArray(i);
    const char *name = da->GetName();
    long nElem = da->GetNumberOfTuples();
    int nComps = da->GetNumberOfComponents();

    std::string arrayName(name);

    //nComp = 1 -> type = scalar; nComp > 1 -> type = vector
    std::stringstream ss;
    ss << "fields/" << arrayName << "/type";
    std::string typePath = ss.str();
    ss.str(std::string());

    //nComp = 1
    ss << "fields/" << arrayName << "/values";
    std::string valPath = ss.str();
    ss.str(std::string());

    //nComp > 1
    ss << "fields/" << arrayName << "/values/u";
    std::string uValPath = ss.str();
    ss.str(std::string());

    ss << "fields/" << arrayName << "/values/v";
    std::string vValPath = ss.str();
    ss.str(std::string());

    ss << "fields/" << arrayName << "/values/w";
    std::string wValPath = ss.str();
    ss.str(std::string());


    switch (da->GetDataType())
    {
      vtkTemplateMacro(
        vtkAOSDataArrayTemplate<VTK_TT> *aosda =
          dynamic_cast<vtkAOSDataArrayTemplate<VTK_TT>*>(da);

        vtkSOADataArrayTemplate<VTK_TT> *soada =
          dynamic_cast<vtkSOADataArrayTemplate<VTK_TT>*>(da);

        if (aosda)
        {
          // AOS
          //VTK_TT *ptr = aosda->GetPointer(0);

          // TODO -- Have to set the u v w separately -- like below
          // Not sure how to do that with zero copy
          // Original code copied the data into three separate vectors
          //node[valPath].set_external(ptr, nElem);
          //node[uValPath].set_external(ptr, nElem);
          //node[vValPath].set_external(ptr, nElem);
          //node[wValPath].set_external(ptr, nElem);
        }
        else if (soada)
        {
          // SOA
          for (int j = 0; j < nComps; ++j)
          {
            VTK_TT *ptr = soada->GetComponentArrayPointer(j);
            if(nComps == 1)
            {
              node[valPath].set_external((conduit_tt<VTK_TT>::conduit_type*)ptr, nElem, 0,
                sizeof(VTK_TT), sizeof(VTK_TT), conduit::Endianness::DEFAULT_ID);
              node[typePath] = "scalar";
            }
            else
            {
              switch(j)
              {
                 case 0: node[uValPath].set_external((conduit_tt<VTK_TT>::conduit_type*)ptr, nElem, 0,
                           sizeof(VTK_TT), sizeof(VTK_TT), conduit::Endianness::DEFAULT_ID);
                         node[typePath] = "vector";
                         break;
                 case 1: node[vValPath].set_external((conduit_tt<VTK_TT>::conduit_type*)ptr, nElem, 0,
                           sizeof(VTK_TT), sizeof(VTK_TT), conduit::Endianness::DEFAULT_ID);
                         break;
                 case 2: node[wValPath].set_external((conduit_tt<VTK_TT>::conduit_type*)ptr, nElem, 0,
                           sizeof(VTK_TT), sizeof(VTK_TT), conduit::Endianness::DEFAULT_ID);
                         break;

              }
            }
          }
        }
        else
        {
          // this should never happen
          SENSEI_ERROR("Invalid array type \"" << da->GetClassName() << "\" at " << i);
        }
        );
      default:
        SENSEI_ERROR("Invalid type from " << VTKUtils::GetAttributesName(centering)
          << " data array " << i << " named \"" << (name ? name : "") << "\"");
    }
  }

  return( 0 );
}

int VTK_To_Fields(int bid, vtkDataSet *ds, conduit::Node &node)
{
  // handle the arrays
  if (VTK_To_Fields(ds->GetPointData(), vtkDataObject::POINT, node))
  {
    SENSEI_ERROR("Failed to transfer point data from block " << bid);
    return( -1 );
  }

  if (VTK_To_Fields(ds->GetCellData(), vtkDataObject::CELL, node))
  {
    SENSEI_ERROR("Failed to transfer cell data from block " << bid);
    return( -1 );
  }

  return( 0 );
}
*/

//------------------------------------------------------------------------------
void AddGhostsZones(vtkDataSet* ds, conduit::Node& node)
{
  // Check if the mesh has ghost zone data.
  if( ds->HasAnyGhostCells() || ds->HasAnyGhostPoints() )
  {
    // If so, add the data for Acsent.
    node["fields/ascent_ghosts/association"] = "element";
    node["fields/ascent_ghosts/topology"] = "mesh";
    node["fields/ascent_ghosts/type"] = "scalar";

    vtkUnsignedCharArray *gc = vtkUnsignedCharArray::SafeDownCast(ds->GetCellData()->GetArray("vtkGhostType"));
    unsigned char *gcp = (unsigned char *)gc->GetVoidPointer( 0 );
    auto size = gc->GetSize();

    // In Acsent, 0 means real data, 1 means ghost data, and 2 or greater means garbage data.
    std::vector<conduit::int32> ghost_flags(size);

    // Ascent needs int32 not unsigned char. I don't know why, that is the way.
    for(vtkIdType i=0; i < size ;++i)
    {
        ghost_flags[i] = gcp[i];
    }
    node["fields/ascent_ghosts/values"].set(ghost_flags);
  }
}

//------------------------------------------------------------------------------
int VTK_To_Fields(vtkDataSet* ds, conduit::Node& node, const std::string &arrayName, vtkDataObject *obj)
{
  vtkImageData *uniform             = vtkImageData::SafeDownCast(ds);
  vtkRectilinearGrid *rectilinear   = vtkRectilinearGrid::SafeDownCast(ds);
  vtkStructuredGrid *structured     = vtkStructuredGrid::SafeDownCast(ds);
  vtkUnstructuredGrid *unstructured = vtkUnstructuredGrid::SafeDownCast(ds);

  std::stringstream ss;
  ss << "fields/" << arrayName << "/association";
  std::string assocPath = ss.str();
  ss.str(std::string());

  //ss << "fields/" << arrayName << "/volume_dependent";
  //std::string volPath = ss.str();
  //ss.str(std::string());

  ss << "fields/" << arrayName << "/topology";
  std::string topoPath = ss.str();
  ss.str(std::string());

  ss << "fields/" << arrayName << "/type";
  std::string typePath = ss.str();
  ss.str(std::string());

  ss << "fields/" << arrayName << "/values";
  std::string valPath = ss.str();
  ss.str(std::string());

  ss << "fields/" << arrayName << "/values/u";
  std::string uValPath = ss.str();
  ss.str(std::string());

  ss << "fields/" << arrayName << "/values/v";
  std::string vValPath = ss.str();
  ss.str(std::string());

  ss << "fields/" << arrayName << "/values/w";
  std::string wValPath = ss.str();
  ss.str(std::string());

  //ss << "fields/" << arrayName << "/grid_function";
  //std::string gridPath = ss.str();
  //ss.str(std::string());

  //ss << "fields/" << arrayName << "/matset";
  //std::string matPath = ss.str();
  //ss.str(std::string());

  //ss << "fields/" << arrayName << "/matset_values";
  //std::string matValPath = ss.str();
  //ss.str(std::string());

  AddGhostsZones(ds, node);

  if(uniform != nullptr)
  {
    vtkDataSetAttributes *attrP = obj->GetAttributes(vtkDataObject::POINT);
    vtkDataSetAttributes *attrC = obj->GetAttributes(vtkDataObject::CELL);
    vtkDataArray *point = attrP->GetArray(0);
    vtkDataArray *cell = attrC->GetArray(0);

    if(point)
    {
      node[assocPath] = "vertex";

      int components = point->GetNumberOfComponents();
      int tuples     = point->GetNumberOfTuples();

      if(components == 1)
      {
        node[typePath] = "scalar";

        std::vector<conduit::float64> vals(tuples, 0.0);
        for(int i = 0; i < tuples; ++i)
        {
          vals[i] = point->GetComponent(i, 0);
        }
        node[valPath].set(vals);
      }
      else if(components == 2)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = point->GetComponent(i, 0);
          vVals[i] = point->GetComponent(i, 1);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
      }
      else if(components == 3)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);
        std::vector<conduit::float64> wVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = point->GetComponent(i, 0);
          vVals[i] = point->GetComponent(i, 1);
          wVals[i] = point->GetComponent(i, 2);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
        node[wValPath].set(wVals);
      }
      else
      {
        SENSEI_ERROR("Too many components (" << components << ") associated with " << arrayName);
        return( -1 );
      }
    }
    else if(cell)
    {
      node[assocPath] = "element";

      int components = cell->GetNumberOfComponents();
      int tuples     = cell->GetNumberOfTuples();

      if(components == 1)
      {
        node[typePath] = "scalar";

        std::vector<conduit::float64> vals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          vals[i] = cell->GetComponent(i, 0);
        }
        node[valPath].set(vals);
      }
      else if(components == 2)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = cell->GetComponent(i, 0);
          vVals[i] = cell->GetComponent(i, 1);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
      }
      else if(components == 3)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);
        std::vector<conduit::float64> wVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = cell->GetComponent(i, 0);
          vVals[i] = cell->GetComponent(i, 1);
          wVals[i] = cell->GetComponent(i, 2);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
        node[wValPath].set(wVals);
      }
      else
      {
        SENSEI_ERROR("Too many components (" << components << ") associated with " << arrayName);
        return( -1 );
      }
    }
    else
    {
      SENSEI_ERROR("Field Orientation unsupported :: Must be vertex or element");
      return( -1 );
    }
    node[topoPath] = "mesh";
  }
  else if(rectilinear != nullptr)
  {
    vtkDataSetAttributes *attrP = obj->GetAttributes(vtkDataObject::POINT);
    vtkDataSetAttributes *attrC = obj->GetAttributes(vtkDataObject::CELL);
    vtkDataArray *point = attrP->GetArray(0);
    vtkDataArray *cell = attrC->GetArray(0);

    if(point)
    {
      node[assocPath] = "vertex";

      int components = point->GetNumberOfComponents();
      int tuples     = point->GetNumberOfTuples();

      if(components == 1)
      {
        node[typePath] = "scalar";

        std::vector<conduit::float64> vals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          vals[i] = point->GetComponent(i, 0);
        }
        node[valPath].set(vals);
      }
      else if(components == 2)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = point->GetComponent(i, 0);
          vVals[i] = point->GetComponent(i, 1);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
      }
      else if(components == 3)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);
        std::vector<conduit::float64> wVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = point->GetComponent(i, 0);
          vVals[i] = point->GetComponent(i, 1);
          wVals[i] = point->GetComponent(i, 2);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
        node[wValPath].set(wVals);
      }
      else
      {
        SENSEI_ERROR("Too many components (" << components << ") associated with " << arrayName);
        return( -1 );
      }
    }
    else if(cell)
    {
      node[assocPath] = "element";

      int components = cell->GetNumberOfComponents();
      int tuples     = cell->GetNumberOfTuples();

      if(components == 1)
      {
        node[typePath] = "scalar";

        std::vector<conduit::float64> vals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
          vals[i] = cell->GetComponent(i, 0);
        node[valPath].set(vals);
      }
      else if(components == 2)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = cell->GetComponent(i, 0);
          vVals[i] = cell->GetComponent(i, 1);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
      }
      else if(components == 3)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);
        std::vector<conduit::float64> wVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = cell->GetComponent(i, 0);
          vVals[i] = cell->GetComponent(i, 1);
          wVals[i] = cell->GetComponent(i, 2);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
        node[wValPath].set(wVals);
      }
      else
      {
        SENSEI_ERROR("Too many components (" << components << ") associated with " << arrayName);
        return( -1 );
      }
    }
    else
    {
      SENSEI_ERROR("Field Orientation unsupported :: Must be vertex or element");
      return( -1 );
    }
    node[topoPath] = "mesh";
  }
  else if(structured != nullptr)
  {
    vtkDataSetAttributes *attrP = obj->GetAttributes(vtkDataObject::POINT);
    vtkDataSetAttributes *attrC = obj->GetAttributes(vtkDataObject::CELL);
    vtkDataArray *point = attrP->GetArray(0);
    vtkDataArray *cell = attrC->GetArray(0);

    if(point)
    {
      node[assocPath] = "vertex";

      int components = point->GetNumberOfComponents();
      int tuples     = point->GetNumberOfTuples();

      if(components == 1)
      {
        node[typePath] = "scalar";

        std::vector<conduit::float64> vals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          vals[i] = point->GetComponent(i, 0);
        }
        node[valPath].set(vals);
      }
      else if(components == 2)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = point->GetComponent(i, 0);
          vVals[i] = point->GetComponent(i, 1);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
      }
      else if(components == 3)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);
        std::vector<conduit::float64> wVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = point->GetComponent(i, 0);
          vVals[i] = point->GetComponent(i, 1);
          wVals[i] = point->GetComponent(i, 2);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
        node[wValPath].set(wVals);
      }
      else
      {
        SENSEI_ERROR("Too many components (" << components << ") associated with " << arrayName);
        return( -1 );
      }
    }
    else if(cell)
    {
      node[assocPath] = "element";

      int components = cell->GetNumberOfComponents();
      int tuples     = cell->GetNumberOfTuples();

      if(components == 1)
      {
        node[typePath] = "scalar";

        std::vector<conduit::float64> vals(tuples,0.0);

        for(int i = 0; i < tuples; ++i)
        {
          vals[i] = cell->GetComponent(i, 0);
        }
        node[valPath].set(vals);
      }
      else if(components == 2)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = cell->GetComponent(i, 0);
          vVals[i] = cell->GetComponent(i, 1);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
      }
      else if(components == 3)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);
        std::vector<conduit::float64> wVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = cell->GetComponent(i, 0);
          vVals[i] = cell->GetComponent(i, 1);
          wVals[i] = cell->GetComponent(i, 2);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
        node[wValPath].set(wVals);
      }
      else
      {
        SENSEI_ERROR("Too many components (" << components << ") associated with " << arrayName);
        return( -1 );
      }
    }
    else
    {
      SENSEI_ERROR("Field Orientation unsupported :: Must be vertex or element");
      return( -1 );
    }
    node[topoPath] = "mesh";
  }
  else if(unstructured != nullptr)
  {
    vtkDataSetAttributes *attrP = obj->GetAttributes(vtkDataObject::POINT);
    vtkDataSetAttributes *attrC = obj->GetAttributes(vtkDataObject::CELL);
    vtkDataArray *point = attrP->GetArray(0);
    vtkDataArray *cell = attrC->GetArray(0);

    if(point)
    {
      node[assocPath] = "vertex";

      int components = point->GetNumberOfComponents();
      int tuples     = point->GetNumberOfTuples();

      if(components == 1)
      {
        node[typePath] = "scalar";

        std::vector<conduit::float64> vals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          vals[i] = point->GetComponent(i, 0);
        }
        node[valPath].set(vals);
      }
      else if(components == 2)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = point->GetComponent(i, 0);
          vVals[i] = point->GetComponent(i, 1);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
      }
      else if(components == 3)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);
        std::vector<conduit::float64> wVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = point->GetComponent(i, 0);
          vVals[i] = point->GetComponent(i, 1);
          wVals[i] = point->GetComponent(i, 2);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
        node[wValPath].set(wVals);
      }
      else
      {
        SENSEI_ERROR("Too many components (" << components << ") associated with " << arrayName);
        return( -1 );
      }
    }
    else if(cell)
    {
      node[assocPath] = "element";

      int components = cell->GetNumberOfComponents();
      int tuples     = cell->GetNumberOfTuples();

      if(components == 1)
      {
        node[typePath] = "scalar";

        std::vector<conduit::float64> vals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          vals[i] = cell->GetComponent(i, 0);
        }
        node[valPath].set(vals);
      }
      else if(components == 2)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = cell->GetComponent(i, 0);
          vVals[i] = cell->GetComponent(i, 1);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
      }
      else if(components == 3)
      {
        node[typePath] = "vector";

        std::vector<conduit::float64> uVals(tuples, 0.0);
        std::vector<conduit::float64> vVals(tuples, 0.0);
        std::vector<conduit::float64> wVals(tuples, 0.0);

        for(int i = 0; i < tuples; ++i)
        {
          uVals[i] = cell->GetComponent(i, 0);
          vVals[i] = cell->GetComponent(i, 1);
          wVals[i] = cell->GetComponent(i, 2);
        }
        node[uValPath].set(uVals);
        node[vValPath].set(vVals);
        node[wValPath].set(wVals);
      }
      else
      {
        SENSEI_ERROR("Too many components (" << components << ") associated with " << arrayName);
        return( -1 );
      }
    }
    else
    {
      SENSEI_ERROR("Field Orientation unsupported :: Must be vertex or element");
      return (-1 );
    }
    node[topoPath] = "mesh";
  }
  else
  {
    SENSEI_ERROR("Mesh structure not supported");
    return( -1 );
  }

  return( 1 );
}

//------------------------------------------------------------------------------
int VTK_To_Topology(vtkDataSet* ds, conduit::Node& node)
{
  vtkImageData *uniform             = vtkImageData::SafeDownCast(ds);
  vtkRectilinearGrid *rectilinear   = vtkRectilinearGrid::SafeDownCast(ds);
  vtkStructuredGrid *structured     = vtkStructuredGrid::SafeDownCast(ds);
  vtkUnstructuredGrid *unstructured = vtkUnstructuredGrid::SafeDownCast(ds);

  if(uniform != nullptr)
  {
    node["topologies/mesh/type"]     = "uniform";
    node["topologies/mesh/coordset"] = "coords";

    int dims[3] = {0, 0, 0};
    uniform->GetDimensions(dims);

    double origin[3] = {0.0, 0.0, 0.0};
    uniform->GetOrigin(origin);

    int extents[6] = {0, 0, 0, 0, 0, 0};
    uniform->GetExtent(extents);

    double spacing[3] = {0.0, 0.0, 0.0};
    uniform->GetSpacing(spacing);

    node["topologies/mesh/elements/origin/i0"] = origin[0] + (extents[0] * spacing[0]);
    node["topologies/mesh/elements/origin/j0"] = origin[1] + (extents[2] * spacing[1]);
    if(dims[2] != 0 && dims[2] != 1)
      node["topologies/mesh/elements/origin/k0"] = origin[2] + (extents[4] * spacing[2]);
  }
  else if(rectilinear != nullptr)
  {
    node["topologies/mesh/type"]     = "rectilinear";
    node["topologies/mesh/coordset"] = "coords";

  }
  else if(structured != nullptr)
  {
    node["topologies/mesh/type"]     = "structured";
    node["topologies/mesh/coordset"] = "coords";

    int dims[3] = {0, 0, 0};
    structured->GetDimensions(dims);

    node["topologies/mesh/elements/dims/i"] = dims[0] - 1;
    node["topologies/mesh/elements/dims/j"] = dims[1] - 1;
    if(dims[2] != 0 && dims[2] != 1)
      node["topologies/mesh/elements/dims/k"] = dims[2] - 1;
  }
  else if(unstructured != nullptr)
  {
    if(!unstructured->IsHomogeneous())
    {
      SENSEI_ERROR("Unstructured cells must be homogenous");
      return( -1 );
    }
    node["topologies/mesh/type"]     = "unstructured";
    node["topologies/mesh/coordset"] = "coords";

    vtkCellArray* cellarray = unstructured->GetCells();
    vtkIdType *ptr = cellarray->GetPointer();

    std::string shape;
    GetShape(shape, ptr[0]);
    node["topologies/mesh/elements/shape"] = shape;

    int ncells = unstructured->GetNumberOfCells();
    int connections = ncells*(ptr[0] + 1);
    std::vector<int> data(ncells*ptr[0], 0);

    int count = 0;
    for(int i = 0; i < connections; ++i)
    {
      int offset = ptr[0] + 1;
      if(i%offset == 0)
        continue;
      else
      {
        data[count] = ptr[i];
        count++;
      }
    }
    node["topologies/mesh/elements/connectivity"].set(data);
  }
  else
  {
    SENSEI_ERROR("Mesh structure not supported");
    return( -1 );
  }

  return( 1 );
}

static unsigned int cycle = 0;
//------------------------------------------------------------------------------
int VTK_To_State(vtkDataSet* , conduit::Node& node)
{
#ifdef DEBUG_SAVE_DATA
    //node["state/time"] = ;
    node["state/cycle"] = cycle;
    //node["state/domain"] = ;
#else
    (void)node;
#endif

    return( 1 );
}

//------------------------------------------------------------------------------
int VTK_To_Coordsets(vtkDataSet* ds, conduit::Node& node)
{
  vtkImageData *uniform             = vtkImageData::SafeDownCast(ds);
  vtkRectilinearGrid *rectilinear   = vtkRectilinearGrid::SafeDownCast(ds);
  vtkStructuredGrid *structured     = vtkStructuredGrid::SafeDownCast(ds);
  vtkUnstructuredGrid *unstructured = vtkUnstructuredGrid::SafeDownCast(ds);

  if(uniform != nullptr)
  {
    node["coordsets/coords/type"] = "uniform";

    //Local Dimensions
    int dims[3] = {0,0,0};
    uniform->GetDimensions(dims);
    node["coordsets/coords/dims/i"] = dims[0];
    node["coordsets/coords/dims/j"] = dims[1];
    node["coordsets/coords/dims/k"] = dims[2];

    //Global Origin
    double origin[3] = {0.0, 0.0, 0.0};
    uniform->GetOrigin(origin);

    int extents[6] = {0, 0, 0, 0, 0, 0};
    uniform->GetExtent(extents);

    double spacing[3] = {0.0, 0.0, 0.0};
    uniform->GetSpacing(spacing);

    node["coordsets/coords/origin/x"] = origin[0] + (extents[0] * spacing[0]);
    node["coordsets/coords/origin/y"] = origin[1] + (extents[2] * spacing[1]);
    node["coordsets/coords/origin/z"] = origin[2] + (extents[4] * spacing[2]);

    //Global Spacing == Local Spacing
    node["coordsets/coords/spacing/dx"] = spacing[0];
    node["coordsets/coords/spacing/dy"] = spacing[1];
    node["coordsets/coords/spacing/dz"] = spacing[2];
  }
  else if(rectilinear != nullptr)
  {
    node["coordsets/coords/type"] = "rectilinear";

    int dims[3] = {0, 0, 0};
    rectilinear->GetDimensions(dims);

    vtkDataArray *x = rectilinear->GetXCoordinates();
    vtkDataArray *y = rectilinear->GetYCoordinates();
    vtkDataArray *z = rectilinear->GetZCoordinates();

    if (!x || !y || !z)
    {
      SENSEI_ERROR("Invalid coordinate arrays in rectilinear");
      return( -1 );
    }

    switch (x->GetDataType())
    {
      vtkTemplateMacro(
        // x
        vtkAOSDataArrayTemplate<VTK_TT> *tx =
           dynamic_cast<vtkAOSDataArrayTemplate<VTK_TT>*>(x);
        VTK_TT *ptr = tx->GetPointer(0);
        long nElem = tx->GetNumberOfTuples();
        node["coordsets/coords/values/x"].set_external((conduit_tt<VTK_TT>::conduit_type*)ptr, nElem, 0,
          sizeof(VTK_TT), sizeof(VTK_TT), conduit::Endianness::DEFAULT_ID);
        // y
        vtkAOSDataArrayTemplate<VTK_TT> *ty =
           dynamic_cast<vtkAOSDataArrayTemplate<VTK_TT>*>(y);
        ptr = ty->GetPointer(0);
        nElem = ty->GetNumberOfTuples();
        node["coordsets/coords/values/y"].set_external((conduit_tt<VTK_TT>::conduit_type*)ptr, nElem, 0,
          sizeof(VTK_TT), sizeof(VTK_TT), conduit::Endianness::DEFAULT_ID);
        // z
        vtkAOSDataArrayTemplate<VTK_TT> *tz =
           dynamic_cast<vtkAOSDataArrayTemplate<VTK_TT>*>(z);
        ptr = tz->GetPointer(0);
        nElem = tz->GetNumberOfTuples();
        if( nElem > 1 )
            node["coordsets/coords/values/z"].set_external((conduit_tt<VTK_TT>::conduit_type*)ptr, nElem, 0,
                sizeof(VTK_TT), sizeof(VTK_TT), conduit::Endianness::DEFAULT_ID);
        );
      default:
        SENSEI_ERROR("Invlaid data type for recilinear grid coordinates");
        return( -1 );
    }
  }
  else if(structured != nullptr)
  {
    node["coordsets/coords/type"] = "explicit";

    int dims[3] = {0, 0, 0};
    structured->GetDimensions(dims);

    int numPoints = structured->GetPoints()->GetNumberOfPoints();
    double point[3] = {0, 0, 0};
    std::vector<conduit::float64> x(numPoints, 0.0);
    std::vector<conduit::float64> y(numPoints, 0.0);
    std::vector<conduit::float64> z(numPoints, 0.0);

    for(int i = 0; i < numPoints; ++i)
    {
      structured->GetPoints()->GetPoint(i, point);
      x[i] = point[0];
      y[i] = point[1];
      z[i] = point[2];
    }
    node["coordsets/coords/values/x"].set(x);
    node["coordsets/coords/values/y"].set(y);
    if(dims[2] != 0 && dims[2] != 1)
      node["coordsets/coords/values/z"].set(z);
  }
  else if(unstructured != nullptr)
  {
    node["coordsets/coords/type"] = "explicit";

    int numPoints = unstructured->GetPoints()->GetNumberOfPoints();
    double point[3] = {0, 0, 0};
    std::vector<conduit::float64> x(numPoints, 0.0);
    std::vector<conduit::float64> y(numPoints, 0.0);
    std::vector<conduit::float64> z(numPoints, 0.0);

    for(int i = 0; i < numPoints; ++i)
    {
      unstructured->GetPoints()->GetPoint(i, point);
      x[i] = point[0];
      y[i] = point[1];
      z[i] = point[2];
    }
    node["coordsets/coords/values/x"].set(x);
    node["coordsets/coords/values/y"].set(y);
    node["coordsets/coords/values/z"].set(z);
  }
  else
  {
    SENSEI_ERROR("Mesh type not supported");
    return( -1 );
  }

  return( 1 );
}

void NodeIter(const conduit::Node& node, std::set<std::string>& fields)
{
  if(node.number_of_children() > 0)
  {
    if(node.has_child("field"))
    {
      std::string field = node["field"].as_string();
      fields.insert(field);
    }
    else
    {
      conduit::NodeConstIterator itr = node.children();
      while(itr.has_next())
      {
        const conduit::Node& child = itr.next();
        NodeIter(child, fields);
      }
    }
  }
}

//------------------------------------------------------------------------------
void AscentAnalysisAdaptor::GetFieldsFromActions()
{
  // TODO why the temp node.
  const conduit::Node& temp = this->actionsNode;
  NodeIter(temp, this->Fields);
}

//------------------------------------------------------------------------------
void JSONFileToNode(const std::string &file_name, conduit::Node& node)
{
  if(conduit::utils::is_file(file_name))
  {
    // TODO why the temp file_node, load to node variable.
    conduit::Node file_node;
    file_node.load(file_name, "json");
    node.update(file_node);
  }
}

//------------------------------------------------------------------------------
void AscentAnalysisAdaptor::Initialize(const std::string &json_file_path, const std::string &options_file_path)
{
  JSONFileToNode(options_file_path, this->optionsNode);
  optionsNode["mpi_comm"] = MPI_Comm_c2f(this->GetCommunicator());
  //optionsNode["runtime/type"] = "ascent";

  this->_ascent.open(this->optionsNode);

  JSONFileToNode(json_file_path, this->actionsNode);

  // Debug
#ifdef DEBUG_SAVE_DATA
  std::cout << "------ "  << options_file_path << " ------" << std::endl;
  this->optionsNode.print();
  std::cout << "------ "  << json_file_path << " ------" << std::endl;
  this->actionsNode.print();
  std::cout << "----------------------------" << std::endl;
#endif
}

int Fill_VTK(vtkDataSet* ds, conduit::Node& node, const std::string &arrayName, vtkDataObject *obj)
{
    //TODO: Zero copy for Coordsets and Topology
    VTK_To_State(ds, node);
    VTK_To_Coordsets(ds, node);
    VTK_To_Topology(ds, node);

    VTK_To_Fields(ds, node, arrayName, obj);
/* TODO new stuff not working
    if (VTK_To_Fields(bid, ds, node))
    {
        SENSEI_ERROR("Failed to transfer block " << bid);
        return( -1 );
    }
*/

    return( 0 );
}

//------------------------------------------------------------------------------
bool AscentAnalysisAdaptor::Execute(DataAdaptor* dataAdaptor)
{
  conduit::Node root;
  vtkDataObject* obj = nullptr;
  std::string meshName = "mesh";

  if(dataAdaptor->GetMesh(meshName, false, obj))
  {
    SENSEI_ERROR("Failed to get mesh");
    return( false );
  }

  GetFieldsFromActions();
  std::vector<std::string> vec(this->Fields.begin(), this->Fields.end());
  int size = vec.size();

  for(int i = 0; i < size; ++i)
  {
    std::string arrayName = vec[i];

    if(dataAdaptor->AddArray(obj, meshName, 1, arrayName))
    {
        SENSEI_ERROR("Failed to add "
        << " data array \"" << arrayName << "\" to mesh \""
        << meshName << "\"");
        return( -1 );
    }


    int numGhostCellLayers = 0;
    if(dataAdaptor->GetMeshHasGhostNodes(meshName, numGhostCellLayers) == 0)
    {
        if(numGhostCellLayers > 0)
        {
            if(dataAdaptor->AddGhostNodesArray(obj, meshName))
            {
                SENSEI_ERROR("Failed to get ghost points for mesh \"" << meshName << "\"");
            }
        }
    }
    else
    {
        SENSEI_ERROR("Failed to get ghost point information for \"" << meshName << "\"");
    }

    if(dataAdaptor->GetMeshHasGhostCells(meshName, numGhostCellLayers) == 0)
    {
        if(numGhostCellLayers > 0)
        {
            if(dataAdaptor->AddGhostCellsArray(obj, meshName))
            {
                SENSEI_ERROR("Failed to get ghost cells for mesh \"" << meshName << "\"");
            }
        }
    }
    else
    {
        SENSEI_ERROR("Failed to get ghost cell information for \"" << meshName << "\"");
    }

    int domainNum = 0;
    if (vtkCompositeDataSet *cds = dynamic_cast<vtkCompositeDataSet*>(obj))
    {
      size_t numBlocks = 0;
      vtkCompositeDataIterator *itr = cds->NewIterator();

      // TODO: Is there a better way to get the number of data sets?
      vtkCompositeDataIterator *iter = cds->NewIterator();
      iter->SkipEmptyNodesOn();
      for(iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem())
        ++numBlocks;

      itr->SkipEmptyNodesOn();
      itr->InitTraversal();
      while(!itr->IsDoneWithTraversal())
      {
        //int bid = std::max(0u, itr->GetCurrentFlatIndex() - 1);
        if (vtkDataSet *ds = dynamic_cast<vtkDataSet*>(cds->GetDataSet(itr)))
        {
          char domain[20] = "";
          if( numBlocks > 1 )
          {
            snprintf( domain, sizeof(domain), "domain_%.6d", domainNum );
            ++domainNum;
            conduit::Node &temp_node = root[domain];

            Fill_VTK(ds, temp_node, arrayName, ds);
          }
          else
          {
            conduit::Node &temp_node = root;
            Fill_VTK(ds, temp_node, arrayName, ds);
          }
        }
        itr->GoToNextItem();
      }
    }
    else if (vtkDataSet *ds = vtkDataSet::SafeDownCast(obj))
    {
      conduit::Node &temp_node = root;

      Fill_VTK(ds, temp_node, arrayName, obj);
    }
    else
    {
      SENSEI_ERROR("Data object " << obj->GetClassName() << " is not supported.");
      return( -1 );
    }

    // Debug
    /*
    std::cout << "----------------------------" << std::endl;
    std::cout << "i: " << i << " size: " << size << std::endl;
    std::cout << "ACTIONS" << std::endl;
    this->actionsNode.print();
    std::cout << "NODE" << std::endl;
    root.print();
    std::cout << "----------------------------" << std::endl;
    */

    DebugSaveAscentData( root, this->optionsNode );

    this->_ascent.publish(root);
    this->_ascent.execute(this->actionsNode);
    root.reset();
  }

  ++cycle;
  return( true );
}

//------------------------------------------------------------------------------
int AscentAnalysisAdaptor::Finalize()
{
  this->_ascent.close();
  this->Fields.clear();

  return( 0 );
}

}   // namespace sensei

