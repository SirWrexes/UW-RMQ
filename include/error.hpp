#pragma once

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <iostream>

#define FATAL(...)                                                               \
    do {                                                                         \
        std::cerr << fmt::format(fg(fmt::color::red), __VA_ARGS__) << std::endl; \
        std::exit(1);                                                            \
    } while (0)

#define EARLY_RETURN(value, ...)                                                    \
    do {                                                                            \
        std::cerr << fmt::format(fg(fmt::color::yellow), __VA_ARGS__) << std::endl; \
        return value;                                                               \
    } while (0)
