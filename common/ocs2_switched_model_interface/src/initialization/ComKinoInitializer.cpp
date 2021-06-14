#include "ocs2_switched_model_interface/initialization/ComKinoInitializer.h"

#include "ocs2_switched_model_interface/core/ComModelBase.h"
#include "ocs2_switched_model_interface/core/SwitchedModel.h"

namespace switched_model {

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
ComKinoInitializer::ComKinoInitializer(const com_model_t& comModel, const SwitchedModelModeScheduleManager& modeScheduleManager)
    : comModelPtr_(comModel.clone()), modeScheduleManagerPtr_(&modeScheduleManager) {}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
ComKinoInitializer::ComKinoInitializer(const ComKinoInitializer& rhs)
    : ocs2::Initializer(rhs), comModelPtr_(rhs.comModelPtr_->clone()), modeScheduleManagerPtr_(rhs.modeScheduleManagerPtr_) {}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
ComKinoInitializer* ComKinoInitializer::clone() const {
  return new ComKinoInitializer(*this);
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
void ComKinoInitializer::compute(scalar_t time, const vector_t& state, scalar_t nextTime, vector_t& input, vector_t& nextState) {
  const comkino_state_t comkinoState = state;
  const auto comPose = getComPose(comkinoState);
  const auto contactFlags = modeScheduleManagerPtr_->getContactFlags(time);

  input = weightCompensatingInputs(*comModelPtr_, contactFlags, getOrientation(comPose));

  nextState.resize(STATE_DIM);
  nextState << comPose, base_coordinate_t::Zero(), getJointPositions(comkinoState);
}

}  // namespace switched_model
