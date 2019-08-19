//
// Created by rgrandia on 16.08.19.
//

#pragma once

// Eigen
#include <Eigen/Core>

// STL
#include <functional>  // missing header in cg.hpp
#include <string>

// CppAD
#include <cppad/cg.hpp>

// CppAD helpers
#include <ocs2_core/automatic_differentiation/CppAdSparsity.h>

namespace ocs2 {

template <typename scalar_t>
class CppAdInterface {
 public:
  using ad_base_t = CppAD::cg::CG<scalar_t>;
  using ad_scalar_t = CppAD::AD<ad_base_t>;
  using dynamic_vector_t = Eigen::Matrix<scalar_t, Eigen::Dynamic, 1>;
  using dynamic_matrix_t = Eigen::Matrix<scalar_t, Eigen::Dynamic, Eigen::Dynamic>;
  using ad_dynamic_vector_t = Eigen::Matrix<ad_scalar_t, Eigen::Dynamic, 1>;
  using ad_function_t = std::function<void(const ad_dynamic_vector_t&, ad_dynamic_vector_t&)>;
  using ad_parameterized_function_t = std::function<void(const ad_dynamic_vector_t&, const ad_dynamic_vector_t&, ad_dynamic_vector_t&)>;

  using ad_fun_t = CppAD::ADFun<ad_base_t>;

  using SparsityPattern = cppad_sparsity::SparsityPattern;

  /**
   * Constructor for parameterized functions
   *
   * @param adFunction : parameterized function f(x,p,y)
   * @param rangeDim : size of y
   * @param variableDim : size of x
   * @param parameterDim : size of p
   * @param modelName : Name of the library to be generated.
   * @param folderName : Folder to save library files to, either absolute of relative
   * @param compileFlags : Compilation flags for the model library.
   */
  CppAdInterface(ad_parameterized_function_t adFunction, int rangeDim, int variableDim, int parameterDim, std::string modelName,
                 std::string folderName = "/tmp/ocs2",
                 std::vector<std::string> compileFlags = {"-O3", "-march=native", "-mtune=native", "-ffast-math"});

  /**
   * Constructor for functions without parameters
   *
   * @param adFunction : function f(x, y)
   * @param rangeDim : size of y
   * @param variableDim : size of x
   * @param modelName : Name of the library to be generated.
   * @param folderName : Folder to save library files to, either absolute of relative
   * @param compileFlags : Compilation flags for the model library.
   */
  CppAdInterface(ad_function_t adFunction, int rangeDim, int variableDim, std::string modelName, std::string folderName = "/tmp/ocs2",
                 std::vector<std::string> compileFlags = {"-O3", "-march=native", "-mtune=native", "-ffast-math"});

  ~CppAdInterface() = default;

  /**
   * Copy constructor. Models are reloaded if available.
   */
  CppAdInterface(const CppAdInterface& rhs);

  //! Not implemented
  CppAdInterface& operator=(const CppAdInterface& rhs) = delete;
  CppAdInterface(CppAdInterface&& rhs) = delete;
  CppAdInterface& operator=(CppAdInterface&& rhs) = delete;

  /**
   * Loads earlier created model from disk
   */
  void loadModels(bool verbose = true);

  /**
   * Creates models, compiles them, and saves them to disk
   *
   * @param computeForwardModel : Whether or not the function itself should be included into the model
   * @param computeJacobian : Whether or not the Jacobian should be generated
   * @param computeHessian : Whether or not the Hessian should be generated
   * @param verbose : Print out extra information
   */
  void createModels(bool computeForwardModel = true, bool computeJacobian = true, bool computeHessian = true, bool verbose = true);

  /**
   * Load models if they are available on disk. Creates a new library otherwise.
   *
   * @param computeForwardModel : Whether or not the function itself should be included into the model
   * @param computeJacobian : Whether or not the Jacobian should be generated
   * @param computeHessian : Whether or not the Hessian should be generated
   * @param verbose : Print out extra information
   */
  void loadModelsIfAvailable(bool computeForwardModel = true, bool computeJacobian = true, bool computeHessian = true, bool verbose = true);

  /**
   * @param x : input vector of size variableDim
   * @param p : parameter vector of size parameterDim
   * @return y = f(x,p)
   */
  dynamic_vector_t getFunctionValue(const dynamic_vector_t& x, const dynamic_vector_t& p = dynamic_vector_t(0)) const;

  /**
   * Jacobian with gradient of each output w.r.t the variables x in the rows.
   *
   * @param x : input vector of size variableDim
   * @param p : parameter vector of size parameterDim
   * @return d/dx( f(x,p) )
   */
  dynamic_matrix_t getJacobian(const dynamic_vector_t& x, const dynamic_vector_t& p = dynamic_vector_t(0)) const;

  /**
   * Hessian, available per output.
   *
   * @param outputIndex : Output to get the hessian for.
   * @param x : input vector of size variableDim
   * @param p : parameter vector of size parameterDim
   * @return dd/dxdx( f_i(x,p) )
   */
  dynamic_matrix_t getHessian(size_t outputIndex, const dynamic_vector_t& x, const dynamic_vector_t& p = dynamic_vector_t(0)) const;

  /**
   * Weighted hessian
   *
   * @param w: vector of weights of size rangeDim
   * @param x : input vector of size variableDim
   * @param p : parameter vector of size parameterDim
   * @return dd/dxdx(sum_i  w_i*f_i(x,p) )
   */
  dynamic_matrix_t getHessian(const dynamic_vector_t& w, const dynamic_vector_t& x, const dynamic_vector_t& p = dynamic_vector_t(0)) const;

 private:
  /**
   * Defines library folder names
   */
  void setFolderNames();

  /**
   * Creates folders on disk
   */
  void createFolderStructure() const;

  /**
   * Checks if library can already be found on disk.
   * @return isLibraryAvailable
   */
  bool isLibraryAvailable() const;

  /**
   * Creates a random temporary folder name
   * @return folder name
   */
  std::string getUniqueTemporaryFolderName() const;

  /**
   * Configures the compiler that compiles the model library
   * @param compiler : compiler to be configured
   */
  void setCompilerOptions(CppAD::cg::GccCompiler<scalar_t>& compiler) const;

  /**
   * Creates sparsity pattern for the Jacobian that will be generated
   * @param fun : taped ad function
   * @return Sparsity pattern that contains entries for variables only, not for parameters
   */
  SparsityPattern createJacobianSparsity(ad_fun_t& fun) const;

  /**
   * Creates sparsity pattern for the Hessian that will be generated
   * @param fun : taped ad function
   * @return Sparsity pattern that contains entries for variables only, not for parameters
   */
  SparsityPattern createHessianSparsity(ad_fun_t& fun) const;

  std::unique_ptr<CppAD::cg::DynamicLib<scalar_t>> dynamicLib_;
  std::unique_ptr<CppAD::cg::GenericModel<scalar_t>> model_;
  ad_parameterized_function_t adFunction_;
  std::vector<std::string> compileFlags_;

  // Sizes
  int rangeDim_;
  int variableDim_;
  int parameterDim_;

  // Names
  std::string modelName_;
  std::string folderName_;
  std::string libraryFolder_;
  std::string tmpFolder_;
  std::string libraryName_;
};

}  // namespace ocs2

#include <ocs2_core/automatic_differentiation/implementation/CppAdInterface.h>

extern template class ocs2::CppAdInterface<double>;
extern template class ocs2::CppAdInterface<float>;