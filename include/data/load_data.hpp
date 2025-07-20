#ifndef DL_LOAD_DATA_HPP
#define DL_LOAD_DATA_HPP
#include <armadillo>
#include <string>

namespace block_scholes
{

class CSVDataLoader
{
   public:
    static arma::fmat LoadData(const std::string &file_path, char split_chat = ',');

   private:
    static std::pair<size_t, size_t> GetMatrixSize(std::ifstream &file, char split_char);
};

}  // namespace block_scholes
#endif