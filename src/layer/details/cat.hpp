

#ifndef DL_SOURCE_LAYER_DETAILS_CAT_HPP_
#define DL_SOURCE_LAYER_DETAILS_CAT_HPP_
#include "layer/abstract/non_param_layer.hpp"
namespace black_scholes
{
class CatLayer : public NonParamLayer
{
   public:
    explicit CatLayer(int32_t dim);

    StatusCode Forward(const std::vector<std::shared_ptr<Tensor<float>>>& inputs,
                       std::vector<std::shared_ptr<Tensor<float>>>& outputs) override;

    StatusCode Check(const std::vector<sftensor>& inputs, const std::vector<sftensor>& outputs) override;

    static StatusCode CreateInstance(const std::shared_ptr<RuntimeOperator>& op,
                                     std::shared_ptr<Layer<float>>& cat_layer);

   private:
    int32_t dim_ = 0;
};
}  // namespace black_scholes
#endif