/******************************************************************************
Copyright (c) 2017, Farbod Farshidian. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#ifndef MRT_ROS_DUMMY_LOOP_OCS2_H_
#define MRT_ROS_DUMMY_LOOP_OCS2_H_

#include <ocs2_comm_interfaces/ocs2_ros_interfaces/mrt/MRT_ROS_Interface.h>

namespace ocs2 {

/**
 * This class implements a loop to test MPC-MRT communication interface using ROS.
 *
 * @tparam STATE_DIM: Dimension of the state space.
 * @tparam INPUT_DIM: Dimension of the control input space.
 */
template <size_t STATE_DIM, size_t INPUT_DIM>
class MRT_ROS_Dummy_Loop {
 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  using mrt_t = ocs2::MRT_ROS_Interface<STATE_DIM, INPUT_DIM>;
  using mrt_ptr_t = typename mrt_t::Ptr;

  using controller_t = typename mrt_t::controller_t;
  using scalar_t = typename mrt_t::scalar_t;
  using scalar_array_t = typename mrt_t::scalar_array_t;
  using size_array_t = typename mrt_t::size_array_t;
  using state_vector_t = typename mrt_t::state_vector_t;
  using state_vector_array_t = typename mrt_t::state_vector_array_t;
  using input_vector_t = typename mrt_t::input_vector_t;
  using input_vector_array_t = typename mrt_t::input_vector_array_t;
  using input_state_matrix_t = typename mrt_t::input_state_matrix_t;
  using input_state_matrix_array_t = typename mrt_t::input_state_matrix_array_t;

  using system_observation_t = typename mrt_t::system_observation_t;
  using cost_desired_trajectories_t = typename mrt_t::cost_desired_trajectories_t;
  using commandData_t = typename mrt_t::CommandData;
  using policyData_t = typename mrt_t::PolicyData;

  using controlled_system_base_t = ControlledSystemBase<STATE_DIM, INPUT_DIM>;

  /**
   * Constructor.
   *
   * @param [in] mrtPtr
   * @param [in] mrtDesiredFrequency: MRT loop frequency in Hz. This should always set to a positive number.
   * @param [in] mpcDesiredFrequency: MPC loop frequency in Hz. If set to a positive number, MPC loop
   * will be simulated to run by this frequency. Note that this might not be the MPC's real-time frequency.
   * @param [in] systemPtr: Optional pointer to the system dynamics. If provided, the dummy will roll out the
   * received controller using these dynamics instead of just sending back a planned state.
   * @param [in] rolloutSettings settings to use when dummy rolls out the received controller
   */
  MRT_ROS_Dummy_Loop(const mrt_ptr_t& mrtPtr, const scalar_t& mrtDesiredFrequency = 100, const scalar_t& mpcDesiredFrequency = -1,
                     controlled_system_base_t* systemPtr = nullptr, Rollout_Settings rolloutSettings = Rollout_Settings());

  /**
   * Destructor.
   */
  virtual ~MRT_ROS_Dummy_Loop() = default;

  /**
   * Initializes the MRT node and visualization node.
   *
   * @param [in] argc: command line number of inputs.
   * @param [in] argv: command line inputs' value.
   */
  void launchNodes(int argc, char* argv[]);

  /**
   * Runs the dummy MRT loop.
   *
   * @param [in] initObservation: The initial observation.
   * @param [in] initCostDesiredTrajectories: The initial desired cost trajectories.
   */
  void run(const system_observation_t& initObservation, const cost_desired_trajectories_t& initCostDesiredTrajectories);

 protected:
  /**
   * A user-defined function which modifies the observation before publishing.
   *
   * @param [in] observation: The current observation.
   */
  virtual void modifyObservation(system_observation_t& observation) {}

  /**
   * Launches the visualization node
   *
   * @param [in] argc: command line number of inputs.
   * @param [in] argv: command line inputs' value.
   */
  virtual void launchVisualizerNode(int argc, char* argv[]) {}

  /**
   * Visualizes the current observation.
   *
   * @param [in] observation: The current observation.
   * @param [in] command: Contains costdesired trajectory and init observation.
   * @param [in] policy: Contains the optimized mpc policy.
   */
  virtual void publishVisualizer(const system_observation_t& observation, const commandData_t& command, const policyData_t& policy) {}

 protected:
  /*
   * Variables
   */
  mrt_ptr_t mrtPtr_;
  scalar_t mrtDesiredFrequency_;
  scalar_t mpcDesiredFrequency_;
  controlled_system_base_t* systemPtr_;

  bool realtimeLoop_;

  system_observation_t observation_;
};

}  // namespace ocs2

#include "implementation/MRT_ROS_Dummy_Loop.h"

#endif /* MRT_ROS_DUMMY_LOOP_OCS2_H_ */
