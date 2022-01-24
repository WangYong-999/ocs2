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

#include "ocs2_ddp/DDP_HelperFunctions.h"

#include <algorithm>
#include <iostream>

#include <ocs2_core/PreComputation.h>
#include <ocs2_core/integration/TrapezoidalIntegration.h>
#include <ocs2_oc/approximate_model/LinearQuadraticApproximator.h>
#include <ocs2_oc/oc_data/Metrics.h>

namespace ocs2 {

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
void computeRolloutMetrics(OptimalControlProblem& problem, const PrimalSolution& primalSolution, DualSolutionConstRef dualSolution,
                           MetricsCollection& metrics) {
  const auto& tTrajectory = primalSolution.timeTrajectory_;
  const auto& xTrajectory = primalSolution.stateTrajectory_;
  const auto& uTrajectory = primalSolution.inputTrajectory_;
  const auto& postEventIndices = primalSolution.postEventIndices_;

  clear(metrics);
  metrics.preJumps.reserve(postEventIndices.size());
  metrics.intermediates.reserve(tTrajectory.size());

  auto nextPostEventIndexItr = postEventIndices.begin();
  const auto request = Request::Cost + Request::Constraint + Request::SoftConstraint;
  for (size_t k = 0; k < tTrajectory.size(); k++) {
    // intermediate time cost and constraints
    problem.preComputationPtr->request(request, tTrajectory[k], xTrajectory[k], uTrajectory[k]);
    metrics.intermediates.push_back(
        computeIntermediateMetrics(problem, tTrajectory[k], xTrajectory[k], uTrajectory[k], dualSolution.intermediates[k]));

    // event time cost and constraints
    if (nextPostEventIndexItr != postEventIndices.end() && k + 1 == *nextPostEventIndexItr) {
      const auto m = dualSolution.preJumps[std::distance(postEventIndices.begin(), nextPostEventIndexItr)];
      problem.preComputationPtr->requestPreJump(request, tTrajectory[k], xTrajectory[k]);
      metrics.preJumps.push_back(computePreJumpMetrics(problem, tTrajectory[k], xTrajectory[k], m));
      nextPostEventIndexItr++;
    }
  }

  // final time cost and constraints
  if (!tTrajectory.empty()) {
    problem.preComputationPtr->requestFinal(request, tTrajectory.back(), xTrajectory.back());
    metrics.final = computeFinalMetrics(problem, tTrajectory.back(), xTrajectory.back(), dualSolution.final);
  }
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
PerformanceIndex computeRolloutPerformanceIndex(const scalar_array_t& timeTrajectory, const MetricsCollection& metrics) {
  assert(timeTrajectory.size() == metrics.intermediates.size());

  // sums the penalty part of Metrics::value_t
  const auto sum = [](const std::vector<Metrics::value_t>& termsValue) -> scalar_t {
    scalar_t s = 0.0;
    std::for_each(termsValue.begin(), termsValue.end(), [&s](const Metrics::value_t& v) { s += v.second; });
    return s;
  };

  PerformanceIndex performanceIndex;

  // Total cost:
  // - Final: state cost, state soft-constraints
  // - PreJumps: state cost, state soft-constraints
  // - Intermediates: state/state-input cost, state/state-input soft-constraints
  performanceIndex.totalCost = metrics.final.cost;

  std::for_each(metrics.preJumps.begin(), metrics.preJumps.end(), [&](const Metrics& m) { return performanceIndex.totalCost + m.cost; });

  scalar_array_t costTrajectory(timeTrajectory.size());
  std::transform(metrics.intermediates.begin(), metrics.intermediates.end(), costTrajectory.begin(),
                 [](const Metrics& m) { return m.cost; });
  performanceIndex.totalCost += trapezoidalIntegration(timeTrajectory, costTrajectory);

  // Dynamics violation:
  performanceIndex.dynamicsViolationSSE = 0.0;

  // Equality constraints' SSE:
  // - Final: state equality constraints
  // - PreJumps: state equality constraints
  // - Intermediates: state/state-input equality constraints
  performanceIndex.equalityConstraintsSSE = metrics.final.stateEqConstraint.squaredNorm();

  std::for_each(metrics.preJumps.begin(), metrics.preJumps.end(),
                [&](const Metrics& m) { return performanceIndex.equalityConstraintsSSE + m.stateEqConstraint.squaredNorm(); });

  scalar_array_t equalityNorm2Trajectory(timeTrajectory.size());
  std::transform(metrics.intermediates.begin(), metrics.intermediates.end(), equalityNorm2Trajectory.begin(),
                 [](const Metrics& m) { return m.stateEqConstraint.squaredNorm() + m.stateInputEqConstraint.squaredNorm(); });
  performanceIndex.equalityConstraintsSSE += trapezoidalIntegration(timeTrajectory, equalityNorm2Trajectory);

  // Equality Lagrangians penalty
  // - Final: state equality Lagrangians
  // - PreJumps: state equality Lagrangians
  // - Intermediates: state/state-input equality Lagrangians
  performanceIndex.equalityLagrangiansPenalty = sum(metrics.final.stateEqLagrangian);

  std::for_each(metrics.preJumps.begin(), metrics.preJumps.end(),
                [&](const Metrics& m) { return performanceIndex.equalityLagrangiansPenalty + sum(m.stateEqLagrangian); });

  scalar_array_t equalityPenaltyTrajectory(timeTrajectory.size());
  std::transform(metrics.intermediates.begin(), metrics.intermediates.end(), equalityPenaltyTrajectory.begin(),
                 [&](const Metrics& m) { return sum(m.stateEqLagrangian) + sum(m.stateInputEqLagrangian); });
  performanceIndex.equalityLagrangiansPenalty += trapezoidalIntegration(timeTrajectory, equalityPenaltyTrajectory);

  // Inequality Lagrangians penalty
  // - Final: state inequality Lagrangians
  // - PreJumps: state inequality Lagrangians
  // - Intermediates: state/state-input inequality Lagrangians
  performanceIndex.inequalityLagrangiansPenalty = sum(metrics.final.stateIneqLagrangian);

  std::for_each(metrics.preJumps.begin(), metrics.preJumps.end(),
                [&](const Metrics& m) { return performanceIndex.inequalityLagrangiansPenalty + sum(m.stateIneqLagrangian); });

  scalar_array_t inequalityPenaltyTrajectory(timeTrajectory.size());
  std::transform(metrics.intermediates.begin(), metrics.intermediates.end(), inequalityPenaltyTrajectory.begin(),
                 [&](const Metrics& m) { return sum(m.stateIneqLagrangian) + sum(m.stateInputIneqLagrangian); });
  performanceIndex.inequalityLagrangiansPenalty += trapezoidalIntegration(timeTrajectory, inequalityPenaltyTrajectory);

  return performanceIndex;
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
scalar_t rolloutTrajectory(RolloutBase& rollout, const std::pair<scalar_t, scalar_t>& timePeriod, const vector_t& initState,
                           const ModeSchedule& modeSchedule, PrimalSolution& primalSolution) {
  // fill mode schedule
  primalSolution.modeSchedule_ = modeSchedule;

  // rollout with controller
  const auto xCurrent = rollout.run(timePeriod.first, initState, timePeriod.second, primalSolution.controllerPtr_.get(),
                                    modeSchedule.eventTimes, primalSolution.timeTrajectory_, primalSolution.postEventIndices_,
                                    primalSolution.stateTrajectory_, primalSolution.inputTrajectory_);

  if (!xCurrent.allFinite()) {
    throw std::runtime_error("System became unstable during the rollout.");
  }

  // average time step
  return (timePeriod.second - timePeriod.first) / static_cast<scalar_t>(primalSolution.timeTrajectory_.size());
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
scalar_t computeControllerUpdateIS(const LinearController& controller) {
  scalar_array_t biasArraySquaredNorm(controller.timeStamp_.size());
  std::transform(controller.deltaBiasArray_.begin(), controller.deltaBiasArray_.end(), biasArraySquaredNorm.begin(),
                 [](const vector_t& b) { return b.squaredNorm(); });
  // integrates using the trapezoidal approximation method
  return trapezoidalIntegration(controller.timeStamp_, biasArraySquaredNorm);
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
void incrementController(scalar_t stepLength, const LinearController& unoptimizedController, LinearController& controller) {
  controller.clear();
  controller.timeStamp_ = unoptimizedController.timeStamp_;
  controller.gainArray_ = unoptimizedController.gainArray_;
  controller.biasArray_.resize(unoptimizedController.size());
  for (size_t k = 0; k < unoptimizedController.size(); k++) {
    controller.biasArray_[k] = unoptimizedController.biasArray_[k] + stepLength * unoptimizedController.deltaBiasArray_[k];
  }
}

}  // namespace ocs2
