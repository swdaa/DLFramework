
#ifndef DL_LAYER_DETAILS_SILU_HPP_
#define DL_LAYER_DETAILS_SILU_HPP_
#include "activation.hpp"
#include "layer/abstract/non_param_layer.hpp"
namespace black_scholes {
class SiLULayer : public activation::ActivationLayer {
 public:
  explicit SiLULayer();

  StatusCode Forward(const std::vector<std::shared_ptr<Tensor<float>>>& inputs,
                     std::vector<std::shared_ptr<Tensor<float>>>& outputs) override;

  static StatusCode CreateInstance(const std::shared_ptr<RuntimeOperator>& op,
                                   std::shared_ptr<Layer<float>>& silu_layer);
};
} 
#endif 