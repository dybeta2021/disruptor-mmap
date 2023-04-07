//
// Created by 稻草人 on 2022/6/15.
//

#ifndef LOGGER_LOGGER_H
#define LOGGER_LOGGER_H

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE//必须定义这个宏,才能输出文件名和行号
#define SPDLOG_LOG_PATTERN "[%m/%d %T.%F] [%^%=8l%$] [%6P/%-6t] [%@#%!] %v"

#include "spdlog/async.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include <iostream>
#include <string>

namespace ots::utils {

    inline bool create_logger(
            const std::string &log_file,
            const std::string &level,
            const bool &use_async,
            const bool &show_use_level,
            const bool &use_format,
            const int &flush_policy = 1,
            const int &thread_count = 1) {
        {
            try {
                // console
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                // Create a daily logger - a new file is created every day on 6:00am
                auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_file, 6, 00);

                if (use_async) {
                    spdlog::init_thread_pool(8192, thread_count);
                    std::vector<spdlog::sink_ptr> log_sinks{console_sink, daily_sink};
                    auto logger = std::make_shared<spdlog::async_logger>("logger",
                                                                         log_sinks.begin(),
                                                                         log_sinks.end(),
                                                                         spdlog::thread_pool(),
                                                                         spdlog::async_overflow_policy::block);
                    if (use_format) {
                        logger->set_pattern(SPDLOG_LOG_PATTERN);
                    }
                    spdlog::set_default_logger(logger);
                } else {
                    spdlog::sinks_init_list log_sinks = {console_sink, daily_sink};
                    auto logger = std::make_shared<spdlog::logger>("logger", log_sinks);
                    if (use_format) {
                        logger->set_pattern(SPDLOG_LOG_PATTERN);
                    }
                    spdlog::set_default_logger(logger);
                }

                if (level == "trace") {
                    spdlog::set_level(spdlog::level::trace);// Set global log level to debug
                } else if (level == "debug") {
                    spdlog::set_level(spdlog::level::debug);
                } else if (level == "info") {
                    spdlog::set_level(spdlog::level::info);
                } else if (level == "warn") {
                    spdlog::set_level(spdlog::level::warn);
                } else if (level == "error") {
                    spdlog::set_level(spdlog::level::err);
                } else if (level == "critical") {
                    spdlog::set_level(spdlog::level::critical);
                } else if (level == "off") {
                    spdlog::set_level(spdlog::level::off);
                } else {
                    std::cout << "level should be in [trace, debug, info, warn, error, critical, off]!";
                    spdlog::set_level(spdlog::level::trace);
                }

                if (show_use_level) {
                    SPDLOG_TRACE("Trace from SpdLog!");
                    SPDLOG_DEBUG("Debug from SpdLog!");
                    SPDLOG_INFO("Info from SpdLog!");
                    SPDLOG_WARN("Warn from SpdLog!");
                    SPDLOG_ERROR("Error from SpdLog!");
                    SPDLOG_CRITICAL("Critical from SpdLog!");
                }

                spdlog::flush_every(std::chrono::seconds(flush_policy));
                return true;
            } catch (const spdlog::spdlog_ex &ex) {
                std::cout << "Log initialization failed: " << ex.what() << std::endl;
                std::cout << ex.what() << std::endl;
                return false;
            }
        }
    }
}// namespace ots::utils

#endif//LOGGER_LOGGER_H
