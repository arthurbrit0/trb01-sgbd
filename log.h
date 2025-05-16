/* ===== log.h =========================================================== */
#pragma once
#include <iostream>
#ifdef DEBUG_LOG
#   define LOG(MSG) (std::cerr << "[LOG] " << MSG << '\n')
#else
#   define LOG(MSG)
#endif
