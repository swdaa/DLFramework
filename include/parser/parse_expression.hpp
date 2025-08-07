
#ifndef DL_PARSER_PARSE_EXPRESSION_HPP_
#define DL_PARSER_PARSE_EXPRESSION_HPP_
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace block_scholes {

enum class TokenType {
  TokenUnknown = -9,
  TokenInputNumber = -8,
  TokenComma = -7,
  TokenAdd = -6,
  TokenMul = -5,
  TokenLeftBracket = -4,
  TokenRightBracket = -3,
};

/**
 * @brief Token parsed from expression
 *
 * Represents a token extracted from the input expression string.
 */
struct Token {
  /// Type of this token
  TokenType token_type = TokenType::TokenUnknown;

  /// Start position in the input expression
  int32_t start_pos = 0;

  /// End position in the input expression
  int32_t end_pos = 0;

  /**
   * @brief Construct a new Token
   *
   * @param token_type Token type
   * @param start_pos Start position
   * @param end_pos End position
   */
  Token(TokenType token_type, int32_t start_pos, int32_t end_pos)
      : token_type(token_type), start_pos(start_pos), end_pos(end_pos) {}
};

/**
 * @brief Node in expression syntax tree
 */
struct TokenNode {
  /// Index of input variable
  int32_t num_index = -1;

  /// Left child node
  std::shared_ptr<TokenNode> left = nullptr;

  /// Right child node
  std::shared_ptr<TokenNode> right = nullptr;

  /**
   * @brief Construct a new Token Node
   *
   * @param num_index Index of input variable
   * @param left Left child node
   * @param right Right child node
   */
  TokenNode(int32_t num_index, std::shared_ptr<TokenNode> left, std::shared_ptr<TokenNode> right);

  TokenNode() = default;
};

/**
 * @brief Parser for math expressions
 *
 * Parses a math expression string into a syntax tree.
 * Performs lexical and syntax analysis.
 */
class ExpressionParser {
 public:
  /**
   * @brief Construct a new Expression Parser
   *
   * @param statement The expression string
   */
  explicit ExpressionParser(std::string statement) : statement_(std::move(statement)) {}

  /**
   * @brief Performs lexical analysis
   *
   * Breaks the expression into tokens.
   *
   * @param retokenize Whether to re-tokenize
   */
  void Tokenizer(bool retokenize = false);

  /**
   * @brief Performs syntax analysis
   *
   * Generates a syntax tree from the tokens.
   *
   * @return Vector of root nodes
   */
  std::vector<std::shared_ptr<TokenNode>> Generate();

  /**
   * @brief Gets the tokens
   *
   * @return The tokens
   */
  const std::vector<Token>& tokens() const;

  /**
   * @brief Gets the token strings
   *
   * @return The token strings
   */
  const std::vector<std::string>& token_str_array() const;

 private:
  std::shared_ptr<TokenNode> Generate_(int32_t& index);

 private:
  std::vector<Token> tokens_;
  std::vector<std::string> token_strs_;
  std::string statement_;
};
}  

#endif  