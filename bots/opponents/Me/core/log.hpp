#pragma once

#include "../config.hpp"

#if LOG_LEVEL > 0
#define LOG(level, ...) do { (void)(level); } while (false)
#else
#define LOG(level, ...) do { (void)(level); } while (false)
#endif
