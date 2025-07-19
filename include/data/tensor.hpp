#ifndef DL_TENSOR_HPP
#define DL_TENSOR_HPP

#include <cstdint>
#include <vector>
#include <armadillo>
#include <memory>

namespace block_scholes
{
    template <typename T = float>
    class Tensor
    {
    };

    template <>
    class Tensor<uint8_t>
    {
    };

    template <>
    class Tensor<float>
    {
    public:
        explicit Tensor() = default;
        explicit Tensor(uint32_t channels, uint32_t rows, uint32_t cols);
        /*
        1-D tensor
        */
        explicit Tensor(uint32_t size);
        explicit Tensor(uint32_t rows, uint32_t cols);
        explicit Tensor(const std::vector<uint32_t> &shape);
        Tensor(const Tensor &tensor);
        Tensor(Tensor &&tensor) noexcept;
        Tensor<float> &operator=(Tensor &&tensor) noexcept;
        Tensor<float> &operator=(const Tensor &tensor);

        uint32_t rows() const;
        uint32_t cols() const;
        uint32_t channels() const;
        uint32_t size() const;

        void set_data(const arma::fcube &data);
        bool empty() const;
        float index(uint32_t offset) const;
        float &index(uint32_t offset);
        std::vector<uint32_t> shapes() const;
        const std::vector<uint32_t> &raw_shapes() const;
        arma::fcube &data();
        const arma::fcube &data() const;
        arma::fmat &slice(uint32_t channel);
        const arma::fmat &slice(uint32_t channel) const;
        float at(uint32_t channel, uint32_t row, uint32_t col) const;
        float &at(uint32_t channel, uint32_t row, uint32_t col);
        void Padding(const std::vector<uint32_t> &pads, float padding_value);
        void Fill(float value);
        void Fill(const std::vector<float> &values, bool row_major = true);
        std::vector<float> values(bool row_major = true);
        void Ones();
        void Rand();
        void Show();
        void Reshape(const std::vector<uint32_t> &shapes, bool row_major = false);
        void Flatten(bool row_major = false);
        void Transform(const std::function<float(float)> &filter);
        float *raw_ptr();
        float *raw_ptr(uint32_t offset);
        float *matrix_raw_ptr(uint32_t index);

    private:
        std::vector<uint32_t> raw_shapes_;
        arma::fcube data_;
    };

    using ftensor = Tensor<float>;
    using sftensor = std::shared_ptr<Tensor<float>>;

}

#endif