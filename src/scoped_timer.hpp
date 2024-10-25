#pragma once

#include <iostream>
#include <string>

struct scoped_timer {
  using clock = std::chrono::high_resolution_clock;

  scoped_timer(const std::string& n) {
    name = n;
    start = clock::now();
  }

  ~scoped_timer() {
    auto end = clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(end - start);
    std::cout << name << " took " << ms.count() << " ms" << std::endl;
  }

  std::string name;
  clock::time_point start;
};
