#include <iostream>

#include "core/version.hpp"

int main() {
  std::cout << "pokebot " << pokebot::version() << '\n';
  return 0;
}
