#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Token {
  std::string text;
  double logit;
};

std::string trim(const std::string &s) {
  const auto begin = std::find_if_not(s.begin(), s.end(), [](unsigned char ch) {
    return std::isspace(ch) != 0;
  });
  if (begin == s.end())
    return "";

  const auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char ch) {
    return std::isspace(ch) != 0;
  }).base();
  return std::string(begin, end);
}

std::vector<std::string> splitWords(const std::string &text) {
  std::istringstream stream(text);
  std::vector<std::string> words;
  std::string word;
  while (stream >> word) {
    words.push_back(word);
  }
  return words;
}

std::string readTextFile(const std::string &path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Unable to open file: " + path);
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::vector<Token> buildToyPromptWindow(const std::vector<std::string> &tokens,
                                        size_t windowSize) {
  std::vector<Token> result;
  const size_t limit = std::min(tokens.size(), windowSize);
  for (size_t i = 0; i < limit; ++i) {
    result.push_back(Token{tokens[i], static_cast<double>(i + 1)});
  }
  return result;
}

std::string generateResponse(const std::vector<Token> &window, const std::string &userText) {
  std::ostringstream out;
  out << "LLM context window preview\n";
  out << "Input length: " << userText.size() << " chars\n";
  out << "Window size: " << window.size() << " tokens\n";
  out << "Latest tokens:\n";
  for (const auto &token : window) {
    out << "  - " << token.text << " (logit=" << token.logit << ")\n";
  }
  out << "\nThis is a compact text-window LLM prototype using a rolling token window.";
  return out.str();
}

}  // namespace

int main(int argc, char **argv) {
  std::string inputText;
  size_t windowSize = 8;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--window" && i + 1 < argc) {
      windowSize = static_cast<size_t>(std::stoul(argv[++i]));
    } else if (arg == "--file" && i + 1 < argc) {
      inputText = readTextFile(argv[++i]);
    } else if (arg == "--text" && i + 1 < argc) {
      inputText = argv[++i];
    } else if (arg == "--help") {
      std::cout << "Usage: Phonix_main [--text TEXT | --file PATH] [--window N]\n";
      return 0;
    }
  }

  if (inputText.empty()) {
    std::cout << "Enter prompt text: ";
    std::getline(std::cin, inputText);
  }

  inputText = trim(inputText);
  if (inputText.empty()) {
    std::cout << "No input provided.\n";
    return 1;
  }

  const auto words = splitWords(inputText);
  const auto window = buildToyPromptWindow(words, windowSize);
  std::cout << generateResponse(window, inputText) << std::endl;
  return 0;
}
