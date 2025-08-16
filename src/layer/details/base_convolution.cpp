
#include "base_convolution.hpp"

#include "convolution.hpp"
#include "deconvolution.hpp"
#include "layer/abstract/layer.hpp"
#include "status_code.hpp"
namespace blackhole
{
BaseConvolutionLayer::BaseConvolutionLayer(ConvType conv_type, uint32_t output_channel, uint32_t in_channel,
                                           uint32_t kernel_h, uint32_t kernel_w, uint32_t padding_h, uint32_t padding_w,
                                           uint32_t stride_h, uint32_t stride_w, uint32_t groups, bool use_bias,
                                           uint32_t output_padding_h, uint32_t output_padding_w, uint32_t dilation_h,
                                           uint32_t dilation_w)
    : ParamLayer("convolution"),
      conv_type_(conv_type),
      use_bias_(use_bias),
      groups_(groups),
      padding_h_(padding_h),
      padding_w_(padding_w),
      stride_h_(stride_h),
      stride_w_(stride_w),
      output_padding_h_(output_padding_h),
      output_padding_w_(output_padding_w),
      dilation_h_(dilation_h),
      dilation_w_(dilation_w)
{
    if (groups != 1)
    {
        in_channel /= groups;
    }

    CHECK_GE(groups_, 1);
    CHECK_GT(kernel_h, 0);
    CHECK_GT(kernel_w, 0);
    CHECK_GT(stride_h_, 0);
    CHECK_GT(stride_w_, 0);
    CHECK_GT(dilation_h_, 0);
    CHECK_GT(dilation_w_, 0);

    if (conv_type_ == ConvType::kOpConv)
    {
        CHECK_EQ(output_padding_h_, 0);
        CHECK_EQ(output_padding_w_, 0);
    }
    else if (conv_type_ == ConvType::kOpDeconv)
    {
        if (dilation_h > 1) kernel_h = (kernel_h - 1) * (dilation_h_ - 1) + kernel_h;
        if (dilation_w > 1) kernel_w = (kernel_w - 1) * (dilation_w_ - 1) + kernel_w;
    }
    else
    {
        LOG(FATAL) << "Unsupported convolution type: " << int(conv_type_);
    }

    CHECK_GT(kernel_h, 0);
    CHECK_GT(kernel_w, 0);
    this->InitWeightParam(output_channel, in_channel, kernel_h, kernel_w);
    if (use_bias_)
    {
        this->InitBiasParam(output_channel, 1, 1, 1);
    }
}

void BaseConvolutionLayer::InitIm2ColWeight()
{
}

void BaseConvolutionLayer::AddBias(arma::fmat& output, uint32_t bias_index) const
{
    if (!this->bias_.empty() && this->use_bias_)
    {
        std::shared_ptr<Tensor<float>> bias;
        bias = this->bias_.at(bias_index);
        if (bias != nullptr && !bias->empty())
        {
            float bias_value = bias->index(0);
            output += bias_value;
        }
        else
        {
            LOG(FATAL) << "Bias tensor is empty or nullptr";
        }
    }
}

StatusCode BaseConvolutionLayer::Forward(const std::vector<std::shared_ptr<Tensor<float>>>& inputs,
                                         std::vector<std::shared_ptr<Tensor<float>>>& outputs)
{
    StatusCode check_code = Check(inputs, outputs);
    if (check_code != StatusCode::kSuccess)
    {
        return check_code;
    }

    const uint32_t kernel_count = this->weights_.size();
    const uint32_t kernel_h = this->weights_.at(0)->rows();
    const uint32_t kernel_w = this->weights_.at(0)->cols();
    const uint32_t kernel_channel = this->weights_.at(0)->channels();

    if (kernel_matrix_arr_.size() != kernel_count)
    {
        InitIm2ColWeight();
    }
    const uint32_t batch_size = inputs.size();
    const uint32_t kernel_count_group = kernel_count / groups_;

#pragma omp parallel for num_threads(batch_size)
    for (uint32_t i = 0; i < batch_size; ++i)
    {
        const std::shared_ptr<Tensor<float>>& input = inputs.at(i);
        const uint32_t input_h = input->rows();
        const uint32_t input_w = input->cols();
        const uint32_t input_c = input->channels();
        CHECK(input_h > 0 && input_w > 0 && input_c > 0);

        const auto& output_size = ComputeOutputSize(input_h, input_w, kernel_h, kernel_w);
        const uint32_t output_h = output_size.first;
        const uint32_t output_w = output_size.second;
        CHECK(output_h > 0 && output_w > 0)
            << "The size of the output tensor should be greater than zero " << i << " th";

        std::shared_ptr<Tensor<float>> output_tensor = outputs.at(i);
        if (output_tensor == nullptr || output_tensor->empty())
        {
            output_tensor = std::make_shared<Tensor<float>>(kernel_count, output_h, output_w);
            outputs.at(i) = output_tensor;
        }

        CHECK(output_tensor->rows() == output_h && output_tensor->cols() == output_w &&
              output_tensor->channels() == kernel_count)
            << "The output tensor array in the convolution layer has an "
               "incorrectly sized tensor "
            << i << "th";

#pragma omp parallel for if (groups_ > 1)
        for (uint32_t group = 0; group < groups_; ++group)
        {
            if (groups_ != 1)
            {
                CHECK(kernel_count % groups_ == 0);
                CHECK(input_c % groups_ == 0);
            }
            const uint32_t channels_per_group = input_c / groups_;
            CHECK(channels_per_group == kernel_channel) << "The number of channel for the kernel "
                                                           "matrix and input tensor do not match";
            ComputeOutput(input, output_tensor, kernel_h, kernel_w, kernel_count_group, input_h, input_w,
                          channels_per_group, output_h, output_w, group);
        }
    }
    return StatusCode::kSuccess;
}

StatusCode BaseConvolutionLayer::CreateInstance(const std::shared_ptr<RuntimeOperator>& op,
                                                std::shared_ptr<Layer<float>>& conv_layer)
{
    if (!op)
    {
        LOG(ERROR) << "The convolution operator parameter in the layer is null pointer.";
        return StatusCode::kParseNullOperator;
    }

    const auto& params = op->params;
    if (params.empty())
    {
        LOG(ERROR) << "The operator parameter in the convolution layer is empty.";
        return StatusCode::kParseParamError;
    }

    if (!op->has_parameter("dilation"))
    {
        LOG(ERROR) << "Can not find the dilation parameter";
        return StatusCode::kParseParamError;
    }

    auto dilation_param = std::dynamic_pointer_cast<RuntimeParameterIntArray>(params.at("dilation"));
    if (dilation_param == nullptr || dilation_param->value.size() != 2)
    {
        LOG(ERROR) << "Can not find the dilation parameter";
        return StatusCode::kParseParamError;
    }

    const uint32_t dilation_h = dilation_param->value.at(0);
    const uint32_t dilation_w = dilation_param->value.at(1);

    if (!op->has_parameter("in_channels"))
    {
        LOG(ERROR) << "Can not find the in channel parameter";
        return StatusCode::kParseParamError;
    }
    auto in_channel = std::dynamic_pointer_cast<RuntimeParameterInt>(params.at("in_channels"));
    if (!in_channel)
    {
        LOG(ERROR) << "Can not find the in channel parameter";
        return StatusCode::kParseParamError;
    }

    if (!op->has_parameter("out_channels"))
    {
        LOG(ERROR) << "Can not find the out channel parameter";
        return StatusCode::kParseParamError;
    }

    auto out_channel = std::dynamic_pointer_cast<RuntimeParameterInt>(params.at("out_channels"));
    if (!out_channel)
    {
        LOG(ERROR) << "Can not find the out channel parameter";
        return StatusCode::kParseParamError;
    }

    if (!op->has_parameter("padding"))
    {
        LOG(ERROR) << "Can not find the padding parameter";
        return StatusCode::kParseParamError;
    }

    auto padding = std::dynamic_pointer_cast<RuntimeParameterIntArray>(params.at("padding"));
    if (!padding)
    {
        LOG(ERROR) << "Can not find the padding parameter";
        return StatusCode::kParseParamError;
    }

    if (!op->has_parameter("bias"))
    {
        LOG(ERROR) << "Can not find the bias parameter";
        return StatusCode::kParseParamError;
    }
    auto use_bias = std::dynamic_pointer_cast<RuntimeParameterBool>(params.at("bias"));
    if (!use_bias)
    {
        LOG(ERROR) << "Can not find the bias parameter";
        return StatusCode::kParseParamError;
    }

    if (!op->has_parameter("stride"))
    {
        LOG(ERROR) << "Can not find the stride parameter";
        return StatusCode::kParseParamError;
    }
    auto stride = std::dynamic_pointer_cast<RuntimeParameterIntArray>(params.at("stride"));
    if (!stride)
    {
        LOG(ERROR) << "Can not find the stride parameter";
        return StatusCode::kParseParamError;
    }

    if (!op->has_parameter("kernel_size"))
    {
        LOG(ERROR) << "Can not find the kernel parameter";
        return StatusCode::kParseParamError;
    }
    auto kernel = std::dynamic_pointer_cast<RuntimeParameterIntArray>(params.at("kernel_size"));
    if (!kernel)
    {
        LOG(ERROR) << "Can not find the kernel parameter";
        return StatusCode::kParseParamError;
    }

    if (op->type == "nn.Conv2d")
    {
        if (op->has_parameter("padding_mode"))
        {
            auto padding_mode = std::dynamic_pointer_cast<RuntimeParameterString>(params.at("padding_mode"));
            if (padding_mode == nullptr)
            {
                LOG(ERROR) << "Can not find the padding parameter";
                return StatusCode::kParseParamError;
            }
            else
            {
                const std::string& padding_mode_str = padding_mode->value;
                if (padding_mode_str != "zeros")
                {
                    LOG(ERROR) << "Padding mode unsupported: " << padding_mode_str;
                    return StatusCode::kParseParamError;
                }
            }
        }
        else
        {
            LOG(ERROR) << "Can not find the padding parameter";
            return StatusCode::kParseParamError;
        }
    }

    if (!op->has_parameter("groups"))
    {
        LOG(ERROR) << "Can not find the groups parameter";
        return StatusCode::kParseParamError;
    }

    auto groups = std::dynamic_pointer_cast<RuntimeParameterInt>(params.at("groups"));
    if (!groups)
    {
        LOG(ERROR) << "Can not find the groups parameter";
        return StatusCode::kParseParamError;
    }

    const uint32_t conv_dims = 2;
    const std::vector<int32_t>& kernels = kernel->value;
    const std::vector<int32_t>& paddings = padding->value;
    const std::vector<int32_t>& strides = stride->value;
    if (paddings.size() != conv_dims)
    {
        LOG(ERROR) << "Can not find the right padding parameter";
        return StatusCode::kParseParamError;
    }

    if (strides.size() != conv_dims)
    {
        LOG(ERROR) << "Can not find the right stride parameter";
        return StatusCode::kParseParamError;
    }

    if (kernels.size() != conv_dims)
    {
        LOG(ERROR) << "Can not find the right kernel size parameter";
        return StatusCode::kParseParamError;
    }

    uint32_t output_padding_h = 0;
    uint32_t output_padding_w = 0;
    if (op->type == "nn.ConvTranspose2d")
    {
        if (op->has_parameter("output_padding"))
        {
            auto output_padding_arr = std::dynamic_pointer_cast<RuntimeParameterIntArray>(params.at("output_padding"));
            if (!output_padding_arr)
            {
                return StatusCode::kParseParamError;
            }
            else
            {
                if (output_padding_arr->value.size() != 2)
                {
                    return StatusCode::kParseParamError;
                }
                output_padding_h = output_padding_arr->value.at(0);
                output_padding_w = output_padding_arr->value.at(1);
            }
        }
        else
        {
            return StatusCode::kParseParamError;
        }
    }

    ConvType conv_type = ConvType::kOpConv;
    if (op->type == "nn.Conv2d")
    {
        conv_type = ConvType::kOpConv;
    }
    else if (op->type == "nn.ConvTranspose2d")
    {
        conv_type = ConvType::kOpDeconv;
    }
    else
    {
        LOG(ERROR) << "Unknown convolution type: " << op->type;
        return StatusCode::kParseParamError;
    }

    if (conv_type == ConvType::kOpConv)
    {
        conv_layer = std::make_shared<ConvolutionLayer>(out_channel->value, in_channel->value, kernels.at(0),
                                                        kernels.at(1), paddings.at(0), paddings.at(1), strides.at(0),
                                                        strides.at(1), groups->value, use_bias->value, output_padding_h,
                                                        output_padding_w, dilation_h, dilation_w);
    }
    else
    {
        conv_layer = std::make_shared<DeconvolutionLayer>(out_channel->value, in_channel->value, kernels.at(0),
                                                          kernels.at(1), paddings.at(0), paddings.at(1), strides.at(0),
                                                          strides.at(1), groups->value, use_bias->value,
                                                          output_padding_h, output_padding_w, dilation_h, dilation_w);
    }

    // load weights
    const std::map<std::string, std::shared_ptr<RuntimeAttribute>>& attrs = op->attribute;
    if (use_bias->value)
    {
        if (!op->has_attribute("bias"))
        {
            LOG(ERROR) << "Can not find the bias attribute";
            return StatusCode::kParseWeightError;
        }
        const auto& bias = attrs.at("bias");
        const std::vector<int32_t>& bias_shape = bias->shape;
        if (bias_shape.empty() || bias_shape.at(0) != out_channel->value)
        {
            LOG(ERROR) << "The attribute of bias shape is wrong";
            return StatusCode::kParseWeightError;
        }

        const std::vector<float>& bias_values = bias->get<float>();
        conv_layer->set_bias(bias_values);
    }

    if (!op->has_attribute("weight"))
    {
        LOG(ERROR) << "Can not find the weight attribute";
        return StatusCode::kParseWeightError;
    }

    const auto& weight = attrs.at("weight");
    const std::vector<int32_t>& weight_shape = weight->shape;
    if (weight_shape.empty())
    {
        LOG(ERROR) << "The attribute of weight shape is wrong";
        return StatusCode::kParseWeightError;
    }

    const std::vector<float>& weight_values = weight->get<float>();
    conv_layer->set_weights(weight_values);

    auto conv_layer_derived = std::dynamic_pointer_cast<BaseConvolutionLayer>(conv_layer);
    CHECK(conv_layer_derived != nullptr);
    conv_layer_derived->InitIm2ColWeight();

    return StatusCode::kSuccess;
}

StatusCode BaseConvolutionLayer::Check(const std::vector<sftensor>& inputs, const std::vector<sftensor>& outputs)
{
    if (inputs.empty())
    {
        LOG(ERROR) << "The input tensor array in the convolution layer is empty";
        return StatusCode::kInferInputsEmpty;
    }

    for (const auto& input_data : inputs)
    {
        if (input_data == nullptr || input_data->empty())
        {
            LOG(ERROR) << "The input tensor array in the maxpooling layer has an "
                          "empty tensor ";
            return StatusCode::kInferInputsEmpty;
        }
    }

    if (outputs.empty())
    {
        LOG(ERROR) << "The output tensor array in the convolution layer is empty";
        return StatusCode::kInferOutputsEmpty;
    }

    if (inputs.size() != outputs.size())
    {
        LOG(ERROR) << "The input and output tensor array size of the convolution "
                      "layer do not match";
        return StatusCode::kInferDimMismatch;
    }

    if (weights_.empty())
    {
        LOG(ERROR) << "The number of kernel matrix in the convolution layer should "
                      "be greater than zero";
        return StatusCode::kInferParamError;
    }

    if (this->use_bias_ && this->bias_.size() != this->weights_.size())
    {
        LOG(ERROR) << "The number of kernel matrix and bias matrix do not match";
        return StatusCode::kInferParamError;
    }

    if (!stride_h_ || !stride_w_)
    {
        LOG(ERROR) << "The stride in the convolution layer should be greater "
                      "than zero";
        return StatusCode::kInferParamError;
    }

    if (!dilation_h_ || !dilation_w_)
    {
        LOG(ERROR) << "The dilation in the convolution layer should be greater "
                      "than zero";
        return StatusCode::kInferParamError;
    }

    if (!groups_)
    {
        LOG(ERROR) << "The group number in the convolution layer should be "
                      "greater than zero ";
        return StatusCode::kInferParamError;
    }

    if (conv_type_ == ConvType::kOpConv)
    {
        if (output_padding_h_ != 0 || output_padding_w_ != 0)
        {
            LOG(ERROR) << "The output padding in the convolution layer should be zero ";
            return StatusCode::kInferParamError;
        }
    }

    const uint32_t kernel_count = this->weights_.size();
    if (!kernel_count)
    {
        LOG(ERROR) << "The size of kernel matrix in the convolution layer should be greater "
                      "than zero";
        return StatusCode::kInferParamError;
    }

    const uint32_t kernel_h = this->weights_.at(0)->rows();
    const uint32_t kernel_w = this->weights_.at(0)->cols();
    const uint32_t kernel_channel = this->weights_.at(0)->channels();

    if (!kernel_h || !kernel_w || !kernel_channel)
    {
        LOG(ERROR) << "The size of kernel matrix in the convolution layer should be greater "
                      "than zero";
        return StatusCode::kInferParamError;
    }

    for (uint32_t k = 0; k < kernel_count; ++k)
    {
        const std::shared_ptr<Tensor<float>>& kernel = this->weights_.at(k);
        if (kernel->rows() != kernel_h)
        {
            return StatusCode::kInferParamError;
        }
        if (kernel->cols() != kernel_w)
        {
            return StatusCode::kInferParamError;
        }
        if (kernel->channels() != kernel_channel)
        {
            return StatusCode::kInferParamError;
        }
    }
    return StatusCode::kSuccess;
}

}  // namespace blackhole