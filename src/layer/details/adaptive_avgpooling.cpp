
#include "adaptive_avgpooling.hpp"

#include <glog/logging.h>

#include "layer/abstract/layer_factory.hpp"

namespace black_scholes
{

AdaptiveAveragePoolingLayer::AdaptiveAveragePoolingLayer(uint32_t output_h, uint32_t output_w)
    : NonParamLayer("AdaptiveAveragePooling"), output_h_(output_h), output_w_(output_w)
{
    CHECK_GT(output_h_, 0);
    CHECK_GT(output_w_, 0);
}

StatusCode AdaptiveAveragePoolingLayer::Forward(const std::vector<std::shared_ptr<Tensor<float>>>& inputs,
                                                std::vector<std::shared_ptr<Tensor<float>>>& outputs)
{
    StatusCode status_code = Check(inputs, outputs);
    if (status_code != StatusCode::kSuccess)
    {
        return status_code;
    }

    const uint32_t batch = inputs.size();
#pragma omp parallel for num_threads(batch)
    for (uint32_t i = 0; i < batch; ++i)
    {
        const std::shared_ptr<Tensor<float>>& input_data = inputs.at(i);

        const uint32_t input_h = input_data->rows();
        const uint32_t input_w = input_data->cols();
        const uint32_t input_c = input_data->channels();
        const uint32_t stride_h = uint32_t(std::floor(input_h / output_h_));
        const uint32_t stride_w = uint32_t(std::floor(input_w / output_w_));
        CHECK(stride_w > 0 && stride_h > 0) << "The stride parameter is set incorrectly. It must always be greater "
                                               "than 0";

        const uint32_t pooling_h = (int32_t)input_h - (int32_t(output_h_) - 1) * int32_t(stride_h);
        const uint32_t pooling_w = (int32_t)input_w - (int32_t(output_w_) - 1) * int32_t(stride_w);

        CHECK(pooling_w > 0 && pooling_h > 0) << "The pooling parameter is set incorrectly. It must always be "
                                                 "greater than 0";

        std::shared_ptr<Tensor<float>> output_data = outputs.at(i);
        if (output_data == nullptr || output_data->empty())
        {
            output_data = std::make_shared<Tensor<float>>(input_c, output_h_, output_w_);
            outputs.at(i) = output_data;
        }

        CHECK(output_data->rows() == output_h_ && output_data->cols() == output_w_ &&
              output_data->channels() == input_c)
            << "The output tensor array in the adaptive pooling layer has an "
               "incorrectly sized tensor "
            << i << "th";

        const uint32_t pooling_size = pooling_h * pooling_w;
        for (uint32_t ic = 0; ic < input_c; ++ic)
        {
            const arma::fmat& input_channel = input_data->slice(ic);
            arma::fmat& output_channel = output_data->slice(ic);
            for (uint32_t c = 0; c < input_w - pooling_w + 1; c += stride_w)
            {
                uint32_t output_col = uint32_t(c / stride_w);
                for (uint32_t r = 0; r < input_h - pooling_h + 1; r += stride_h)
                {
                    float mean_value = 0.f;
                    uint32_t output_row = uint32_t(r / stride_h);
                    float* output_channel_ptr = output_channel.colptr(output_col);
                    for (uint32_t w = 0; w < pooling_w; ++w)
                    {
                        const float* col_ptr = input_channel.colptr(c + w) + r;
                        for (uint32_t h = 0; h < pooling_h; ++h)
                        {
                            float current_value = *(col_ptr + h);
                            mean_value = mean_value + current_value;
                        }
                    }
                    *(output_channel_ptr + output_row) = mean_value / float(pooling_size);
                }
            }
        }
    }
    return StatusCode::kSuccess;
}

StatusCode AdaptiveAveragePoolingLayer::CreateInstance(const std::shared_ptr<RuntimeOperator>& op,
                                                       std::shared_ptr<Layer<float>>& avg_layer)
{
    if (!op)
    {
        LOG(ERROR) << "The adaptive pooling operator parameter in the layer is null pointer.";
        return StatusCode::kParseNullOperator;
    }

    const auto& params = op->params;
    if (params.empty())
    {
        LOG(ERROR) << "The operator parameter in the adaptive pooling layer is empty.";
        return StatusCode::kParseParamError;
    }

    auto output_size_param = std::dynamic_pointer_cast<RuntimeParameterIntArray>(params.at("output_size"));
    if (!output_size_param)
    {
        LOG(ERROR) << "Can not find the output size parameter in the parameter list.";
        return StatusCode::kParseParamError;
    }

    const auto& output_size_arr = output_size_param->value;
    if (output_size_arr.size() != 2)
    {
        LOG(ERROR) << "The dimension of the output size parameter should be 2.";
        return StatusCode::kParseParamError;
    }
    avg_layer = std::make_shared<AdaptiveAveragePoolingLayer>(output_size_arr.at(0), output_size_arr.at(1));
    return StatusCode::kSuccess;
}

StatusCode AdaptiveAveragePoolingLayer::Check(const std::vector<sftensor>& inputs, const std::vector<sftensor>& outputs)
{
    if (inputs.empty())
    {
        LOG(ERROR) << "The input tensor array in the adaptive pooling layer is empty";
        return StatusCode::kInferInputsEmpty;
    }

    if (outputs.empty())
    {
        LOG(ERROR) << "The output tensor array in the adaptive pooling layer is empty";
        return StatusCode::kInferOutputsEmpty;
    }

    if (inputs.size() != outputs.size())
    {
        LOG(ERROR) << "The input and output tensor array size of the adaptive "
                      "pooling layer do not match";
        return StatusCode::kInferDimMismatch;
    }

    for (const auto& input_data : inputs)
    {
        if (input_data == nullptr || input_data->empty())
        {
            LOG(ERROR) << "The input tensor array in the adaptive pooling layer has an empty "
                          "tensor ";
            return StatusCode::kInferInputsEmpty;
        }
    }

    if (!output_h_ || !output_w_)
    {
        LOG(ERROR) << "The output_h and output_w in the adaptive pooling layer should be "
                      "greater than zero";
        return StatusCode::kInferParamError;
    }
    return StatusCode::kSuccess;
}

LayerRegistererWrapper kAdaptiveAvgPoolingCreateInstance(AdaptiveAveragePoolingLayer::CreateInstance,
                                                         "nn.AdaptiveAvgPool2d", "F.adaptive_avg_pool2d");

}  // namespace black_scholes