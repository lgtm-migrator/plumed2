/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2015-2020 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed.org for more information.

   This file is part of plumed, version 2.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#ifndef __PLUMED_clusters_ClusteringBase_h
#define __PLUMED_clusters_ClusteringBase_h

#include "matrixtools/ActionWithInputMatrices.h"
#include "tools/Matrix.h"

namespace PLMD {
namespace clusters {

class ClusteringBase : public matrixtools::ActionWithInputMatrices {
protected:
/// Vector that stores the sizes of the current set of clusters
  std::vector< std::pair<unsigned,unsigned> > cluster_sizes;
/// Used to identify the cluster we are working on
  int number_of_cluster;
/// Vector that identifies the cluster each atom belongs to
  std::vector<unsigned> which_cluster;
/// Get the number of nodes
  unsigned getNumberOfNodes() const ;
/// Get the neighbour list based on the adjacency matrix
  void retrieveAdjacencyLists( std::vector<unsigned>& nneigh, Matrix<unsigned>& adj_list );
public:
/// Create manual
  static void registerKeywords( Keywords& keys );
/// Constructor
  explicit ClusteringBase(const ActionOptions&);
///
  void completeMatrixOperations() override;
///
  virtual void performClustering()=0;
/// Cannot apply forces on a clustering object
  void apply() override ;
  double getForceOnMatrixElement( const unsigned& imat, const unsigned& jrow, const unsigned& krow ) const override { plumed_error(); }
};

inline
unsigned ClusteringBase::getNumberOfNodes() const {
  return getPntrToArgument(0)->getShape()[0];
}

}
}
#endif