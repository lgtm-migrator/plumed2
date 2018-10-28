/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2015-2018 The plumed team
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
#include "core/ActionRegister.h"
#include "tools/KernelFunctions.h"
#include "multicolvar/MultiColvarBase.h"
#include "tools/IFile.h"
#include "core/ActionSetup.h"

//+PLUMEDOC MCOLVARF PAMM
/*
Probabilistic analysis of molecular mofifs.

Probabilistic analysis of molecular motifs (PAMM) was introduced in this paper \cite{pamm}.
The essence of this approach involves calculating some large set of collective variables
for a set of atoms in a short trajectory and fitting this data using a Gaussian Mixture Model.
The idea is that modes in these distributions can be used to identify features such as hydrogen bonds or
secondary structure types.

The assumption within this implementation is that the fitting of the Gaussian mixture model has been
done elsewhere by a separate code.  You thus provide an input file to this action which contains the
means, covariances and weights for a set of Gaussian kernels, \f$\{ \phi \}\f$.  The values and
derivatives for the following set of quantities is then computed:

\f[
s_k = \frac{ \phi_k}{ \sum_i \phi_i }
\f]

Each of the \f$\phi_k\f$ is a Gaussian function that acts on a set of quantities calculated within
a \ref mcolv.  These might be \ref TORSIONS, \ref DISTANCES, \ref ANGLES or any one of the many
symmetry functions that are available within \ref mcolv actions.  These quantities are then inserted into
the set of \f$n\f$ kernels that are in the the input file.   This will be done for multiple sets of values
for the input quantities and a final quantity will be calculated by summing the above \f$s_k\f$ values or
some transformation of the above.  This sounds less complicated than it is and is best understood by
looking through the example given below.

\warning Mixing periodic and aperiodic \ref mcolv actions has not been tested

\par Examples

In this example I will explain in detail what the following input is computing:

\plumedfile
MOLINFO MOLTYPE=protein STRUCTURE=M1d.pdb
psi: TORSIONS ATOMS1=@psi-2 ATOMS2=@psi-3 ATOMS3=@psi-4
phi: TORSIONS ATOMS1=@phi-2 ATOMS2=@phi-3 ATOMS3=@phi-4
p: PAMM DATA=phi,psi CLUSTERS=clusters.dat MEAN1={COMPONENT=1} MEAN2={COMPONENT=2}
PRINT ARG=p.mean-1,mean-2 FILE=colvar
\endplumedfile

The best place to start our explanation is to look at the contents of the clusters.dat file

\verbatim
#! FIELDS height phi psi sigma_phi_phi sigma_phi_psi sigma_psi_phi sigma_psi_psi
#! SET multivariate von-misses
#! SET kerneltype gaussian
      0.4     -1.0      -1.0      0.2     -0.1    -0.1    0.2
      0.6      1.0      +1.0      0.1     -0.03   -0.03   0.1
\endverbatim

This files contains the parameters of two two-dimensional Gaussian functions.  Each of these Gaussians has a weight, \f$w_k\f$,
a vector that specifies the position of its centre, \f$\mathbf{c}_k\f$, and a covariance matrix, \f$\Sigma_k\f$.  The \f$\phi_k\f$ functions that
we use to calculate our PAMM components are thus:

\f[
\phi_k = \frac{w_k}{N_k} \exp\left( -(\mathbf{s} - \mathbf{c}_k)^T \Sigma^{-1}_k (\mathbf{s} - \mathbf{c}_k) \right)
\f]

In the above \f$N_k\f$ is a normalisation factor that is calculated based on \f$\Sigma\f$.  The vector \f$\mathbf{s}\f$ is a vector of quantities
that are calculated by the \ref TORSIONS actions.  This vector must be two dimensional and in this case each component is the value of a
torsion angle.  If we look at the two \ref TORSIONS actions in the above we are calculating the \f$\phi\f$ and \f$\psi\f$ backbone torsional
angles in a protein (Note the use of \ref MOLINFO to make specification of atoms straightforward).  We thus calculate the values of our
2 \f$ \{ \phi \} \f$  kernels 3 times.  The first time we use the \f$\phi\f$ and \f$\psi\f$ angles in the 2nd resiude of the protein,
the second time it is the \f$\phi\f$ and \f$\psi\f$ angles of the 3rd residue of the protein and the third time it is the \f$\phi\f$ and \f$\psi\f$ angles
of the 4th residue in the protein.  The final two quantities that are output by the print command, p.mean-1 and p.mean-2, are the averages
over these three residues for the quantities:
\f[
s_1 = \frac{ \phi_1}{ \phi_1 + \phi_2 }
\f]
and
\f[
s_2 = \frac{ \phi_2}{ \phi_1 + \phi_2 }
\f]
There is a great deal of flexibility in this input.  We can work with, and examine, any number of components, we can use any set of collective variables
and compute these PAMM variables and we can transform the PAMM variables themselves in a large number of different ways when computing these sums.
*/
//+ENDPLUMEDOC

namespace PLMD {
namespace pamm {

class PAMM : public ActionSetup {
public:
  static void registerKeywords( Keywords& keys );
  explicit PAMM(const ActionOptions&);
  static void shortcutKeywords( Keywords& keys );
  static void expandShortcut( const std::string& lab, const std::vector<std::string>& words,
                              const std::map<std::string,std::string>& keys,
                              std::vector<std::vector<std::string> >& actions );
};

PLUMED_REGISTER_SHORTCUT(PAMM,"PAMM")

void PAMM:: shortcutKeywords( Keywords& keys ) {
  keys.add("compulsory","DATA","the vectors from which the pamm coordinates are calculated");
  keys.add("compulsory","CLUSTERS","the name of the file that contains the definitions of all the clusters");
  keys.add("compulsory","REGULARISE","0.001","don't allow the denominator to be smaller then this value");
  multicolvar::MultiColvarBase::shortcutKeywords( keys );
}

void PAMM::expandShortcut( const std::string& lab, const std::vector<std::string>& words,
                              const std::map<std::string,std::string>& keys,
                              std::vector<std::vector<std::string> >& actions ) {
   plumed_assert( words[0]=="PAMM" );
   // Must get list of input value names
   std::vector<std::string> valnames = Tools::getWords( keys.find("DATA")->second,"\t\n ," );

   // Create actions to calculate all pamm kernels
   unsigned nkernels = 0; 
   std::string fname = keys.find("CLUSTERS")->second; 
   IFile ifile; ifile.open(fname); ifile.allowIgnoredFields();
   for(unsigned k=0;; ++k) {
      std::unique_ptr<KernelFunctions> kk = KernelFunctions::read( &ifile, false, valnames );
      if( !kk ) break ;
      std::string num; Tools::convert( k+1, num );
      std::vector<std::string> kinput; kinput.push_back( lab + "_kernel-" + num + ":" );
      kinput.push_back("KERNEL"); kinput.push_back("NORMALIZED");
      for(unsigned j=0;j<valnames.size();++j){ 
          std::string jstr; Tools::convert(j+1,jstr); kinput.push_back("ARG" + jstr + "=" + valnames[j] );
      }
      kinput.push_back("KERNEL=" + kk->getInputString() );
      actions.push_back(kinput); nkernels++;
      // meanwhile, I just release the unique_ptr herelease the unique_ptr here. GB
      ifile.scanField();
   }
   ifile.close();

   // Now combine all the PAMM objects
   std::vector<std::string> cinput; cinput.push_back( lab + "_ksum:"); cinput.push_back("COMBINE");
   for(unsigned k=0;k<nkernels;++k) {
       std::string num; Tools::convert( k+1, num ); 
       cinput.push_back("ARG" + num + "=" + lab + "_kernel-" + num );
   }
   cinput.push_back("PERIODIC=NO"); actions.push_back( cinput );
   
   // And add on the regularization 
   std::vector<std::string> minput; minput.push_back( lab + "_rksum:"); minput.push_back("MATHEVAL");
   minput.push_back("ARG1=" + lab + "_ksum"); minput.push_back("FUNC=x+" + keys.find("REGULARISE")->second );
   minput.push_back("PERIODIC=NO"); actions.push_back( minput );
   
   // And now compute all the pamm kernels
   for(unsigned k=0;k<nkernels;++k) {
       std::string num; Tools::convert( k+1, num );
       std::vector<std::string> finput; finput.push_back( lab + "-" + num + ":" ); 
       finput.push_back("MATHEVAL"); finput.push_back("ARG1=" + lab + "_kernel-" + num );
       finput.push_back("ARG2=" + lab + "_rksum"); finput.push_back("FUNC=x/y");
       finput.push_back("PERIODIC=NO"); actions.push_back( finput );
       multicolvar::MultiColvarBase::expandFunctions( lab + "-" + num, lab + "-" + num, "", words, keys, actions );
   }
}


void PAMM::registerKeywords( Keywords& keys ) {
  ActionSetup::registerKeywords( keys );
}

PAMM::PAMM(const ActionOptions&ao):
Action(ao),
ActionSetup(ao)
{
  plumed_error();
}

}
}