/******************************************************************************
Copyright (c) 2020, Farbod Farshidian. All rights reserved.

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

#pragma once

#include "ocs2_core/penalties/augmented/AugmentedPenaltyBase.h"

namespace ocs2 {
namespace augmented {

/**
 *  Implements the augmented Lagrangian for a single inequality constraint \f$ h \geq 0 \f$ through the modified-log-barrier method.
 *
 *  This leads to the following augmented-Lagrangian penalty function (referred to as the smooth-PHR penalty in the corresponding paper):
 *  \f[
 *      p(h, \lambda) = \lambda^2 \mu \cdot \psi\left(\frac{h}{\lambda \mu}\right).
 *  \f]
 *
 *  where \f$ \psi(.) \f$ is defined as a shifted quadratically-relaxed log barrier function.
 *
 *  This is then minimized by the solver, while the maximization of the approximate dual function is done by updating the Lagrange
 *  multipliers with the following update rule:
 *  \f[
 *      \lambda^*_{k+1} = -\alpha \lambda^*_k \cdot \psi'\left(\frac{h^*_{k+1}}{\lambda^*_k \mu}\right).
 *  \f]
 *
 * where \f$ \psi'(.) \f$ is the total derivative of \f$ \psi(.) \f$.
 */
class ModifiedRelaxedBarrierPenalty final : public AugmentedPenaltyBase {
 public:
  /**
   * Configuration object for the modified relaxed barrier penalty.
   * scale: scaling factor, see class description
   * relaxation: relaxation parameter, see class description
   * stepLenght: step-length parameter, see class description
   */
  struct Config {
    Config(scalar_t scaleParam = 100.0, scalar_t relaxationParam = 1e-2, scalar_t stepSizeParam = 0.0)
        : scale(scaleParam), relaxation(relaxationParam), stepSize(stepSizeParam) {}
    scalar_t scale;
    scalar_t relaxation;
    scalar_t stepSize;
  };

  ModifiedRelaxedBarrierPenalty(Config config) : config_(std::move(config)) {
    a_ = 1.0 / std::pow(1.0 + config_.relaxation, 2);
    b_ = -1.0 / (1.0 + config_.relaxation);
    c_ = -log(1.0 + config_.relaxation);
  }

  ~ModifiedRelaxedBarrierPenalty() override = default;
  ModifiedRelaxedBarrierPenalty* clone() const override { return new ModifiedRelaxedBarrierPenalty(*this); }
  std::string name() const override { return "ModifiedRelaxedBarrierPenalty"; }

  scalar_t getValue(scalar_t t, scalar_t l, scalar_t h) const override {
    const scalar_t v = vFunc(l, h);
    if (v > config_.relaxation) {
      return -v * log(1.0 + wFunc(l));
    } else {
      const scalar_t vTemp = v - config_.relaxation;
      return wFunc(l) * (0.5 * a_ * vTemp * vTemp + b_ * vTemp + c_);
    }
  }

  scalar_t getDerivative(scalar_t t, scalar_t l, scalar_t h) const override {
    const scalar_t v = vFunc(l, h);
    if (v > config_.relaxation) {
      return -wFunc(l) / (1.0 + v) * dldhFunc(l);
    } else {
      return wFunc(l) * (a_ * (v - config_.relaxation) + b_) * dldhFunc(l);
    }
  }

  scalar_t getSecondDerivative(scalar_t t, scalar_t l, scalar_t h) const override {
    const scalar_t v = vFunc(l, h);
    const scalar_t dldh = dldhFunc(l);
    if (v > config_.relaxation) {
      return wFunc(l) / ((1.0 + v) * (1.0 + v)) * dldh * dldh;
    } else {
      return wFunc(l) * a_ * dldh * dldh;
    }
  }

  scalar_t updateMultiplier(scalar_t t, scalar_t l, scalar_t h) const override {
    const scalar_t v = vFunc(l, h);
    constexpr scalar_t lambdaMin = 1e-4;
    if (v > config_.relaxation) {
      return std::max(wFunc(l) * (1.0 / (1 + v)) * dldhFunc(l), lambdaMin);
    } else {
      return std::max(config_.stepSize * wFunc(l) * (-a_ * (v - config_.relaxation) - b_) * dldhFunc(l), lambdaMin);
    }
  }

  scalar_t initializeMultiplier() const override { return 1.0; }

 private:
  ModifiedRelaxedBarrierPenalty(const ModifiedRelaxedBarrierPenalty& other) = default;

  scalar_t dldhFunc(scalar_t l) const { return 1.0 / (config_.scale * l); }
  scalar_t wFunc(scalar_t l) const { return config_.scale * l * l; }
  scalar_t vFunc(scalar_t l, scalar_t h) const { return h / (config_.scale * l); }

  scalar_t a_ = 1.0;
  scalar_t b_ = -1.0;
  scalar_t c_ = 0.0;
  const Config config_;
};

}  // namespace augmented
}  // namespace ocs2
