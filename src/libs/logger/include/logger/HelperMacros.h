/*
* Copyright (c) 2015 - 2024 Ember
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <logger/Severity.h>

inline auto& log_deref(auto& x) { return x; }
inline auto& log_deref(auto* x) { return *x; }

#if !NO_LOGGING && !NO_TRACE_LOGGING
#define LOG_TRACE(logger) \
	if(logger->severity() <= ember::log::Severity::TRACE) { \
		log_deref(logger) << ember::log::Severity::TRACE
#else
#define LOG_TRACE(logger) \
	if(false) { \
		log_deref(logger)
#endif

#if !NO_LOGGING && !NO_DEBUG_LOGGING
#define LOG_DEBUG(logger) \
	if(logger->severity() <= ember::log::Severity::DEBUG) { \
		log_deref(logger) << ember::log::Severity::DEBUG
#else
#define LOG_DEBUG(logger) \
	if(false) { \
		log_deref(logger)
#endif

#if !NO_LOGGING && !NO_INFO_LOGGING
#define LOG_INFO(logger) \
	if(logger->severity() <= ember::log::Severity::INFO) { \
		log_deref(logger) << ember::log::Severity::INFO
#else
#define LOG_INFO(logger) \
	if(false) {	\
		log_deref(logger)
#endif

#if !NO_LOGGING && !NO_WARN_LOGGING
#define LOG_WARN(logger) \
if(logger->severity() <= ember::log::Severity::WARN) { \
		log_deref(logger) << ember::log::Severity::WARN
#else
#define LOG_WARN(logger) \
	if(false) { \
		log_deref(logger)
#endif

#if !NO_LOGGING && !NO_ERROR_LOGGING
#define LOG_ERROR(logger) \
	if(logger->severity() <= ember::log::Severity::ERROR_) { \
		log_deref(logger) << ember::log::Severity::ERROR_
#else
#define LOG_ERROR(logger) \
	if(false) { \
		log_deref(logger)
#endif


#if !NO_LOGGING && !NO_FATAL_LOGGING
#define LOG_FATAL(logger) \
	if(logger->severity() <= ember::log::Severity::FATAL) { \
		log_deref(logger) << ember::log::Severity::FATAL
#else
#define LOG_FATAL(logger) \
	if(false) { \
		log_deref(logger)
#endif

#if !NO_LOGGING && !NO_TRACE_LOGGING
#define LOG_TRACE_FILTER(logger, type) \
	if(logger->severity() <= ember::log::Severity::TRACE && !(logger->filter() & type)) { \
		log_deref(logger) << ember::log::Severity::TRACE << ember::log::Filter(type)
#else
#define LOG_TRACE_FILTER(logger, type) \
	if(false) { \
		log_deref(logger)
#endif

#if !NO_LOGGING && !NO_DEBUG_LOGGING
#define LOG_DEBUG_FILTER(logger, type) \
	if(logger->severity() <= ember::log::Severity::DEBUG && !(logger->filter() & type)) { \
		log_deref(logger) << ember::log::Severity::DEBUG << ember::log::Filter(type)
#else
#define LOG_DEBUG_FILTER(logger, type) \
	if(false) { \
		log_deref(logger)
#endif

#if !NO_LOGGING && !NO_INFO_LOGGING
#define LOG_INFO_FILTER(logger, type) \
	if(logger->severity() <= ember::log::Severity::INFO && !(logger->filter() & type)) { \
		log_deref(logger) << ember::log::Severity::INFO << ember::log::Filter(type)
#else
#define LOG_INFO_FILTER(logger, type) \
	if(false) { \
		log_deref(logger)
#endif

#if !NO_LOGGING && !NO_WARN_LOGGING
#define LOG_WARN_FILTER(logger, type) \
	if(logger->severity() <= ember::log::Severity::WARN && !(logger->filter() & type)) { \
		log_deref(logger) << ember::log::Severity::WARN << ember::log::Filter(type)
#else
#define LOG_WARN_FILTER(logger, type) \
	if(false) { \
		log_deref(logger)
#endif

#if !NO_LOGGING && !NO_ERROR_LOGGING
#define LOG_ERROR_FILTER(logger, type) \
	if(logger->severity() <= ember::log::Severity::ERROR_ && !(logger->filter() & type)) { \
		log_deref(logger) << ember::log::Severity::ERROR_ << ember::log::Filter(type)
#else
#define LOG_ERROR_FILTER(logger, type) \
	if(false) { \
		log_deref(logger)
#endif


#if !NO_LOGGING && !NO_FATAL_LOGGING
#define LOG_FATAL_FILTER(logger, type) \
	if(logger->severity() <= ember::log::Severity::FATAL && !(logger->filter() & type)) { \
		log_deref(logger) << ember::log::Severity::FATAL << ember::log::Filter(type)
#else
#define LOG_FATAL_FILTER(logger, type) \
	if(false) \
		log_deref(logger)
#endif

#define LOG_TRACE_GLOB \
	LOG_TRACE(ember::log::global_logger())

#define LOG_DEBUG_GLOB \
	LOG_DEBUG(ember::log::global_logger())

#define LOG_INFO_GLOB \
	LOG_INFO(ember::log::global_logger())

#define LOG_WARN_GLOB \
	LOG_WARN(ember::log::global_logger())

#define LOG_ERROR_GLOB \
	LOG_ERROR(ember::log::global_logger())

#define LOG_FATAL_GLOB \
	LOG_FATAL(ember::log::global_logger())

#define LOG_TRACE_FILTER_GLOB(filter) \
	LOG_TRACE_FILTER(ember::log::global_logger(), filter)

#define LOG_DEBUG_FILTER_GLOB(filter) \
	LOG_DEBUG_FILTER(ember::log::global_logger(), filter)

#define LOG_INFO_FILTER_GLOB(filter) \
	LOG_INFO_FILTER(ember::log::global_logger(), filter)

#define LOG_WARN_FILTER_GLOB(filter) \
	LOG_WARN_FILTER(ember::log::global_logger(), filter)

#define LOG_ERROR_FILTER_GLOB(filter) \
	LOG_ERROR_FILTER(ember::log::global_logger(), filter)

#define LOG_FATAL_FILTER_GLOB(filter) \
	LOG_FATAL_FILTER(ember::log::global_logger(), filter)

#define LOG_ASYNC ember::log::flush; }
#define LOG_SYNC  ember::log::flush_sync; }

#if !NO_LOGGING && !NO_TRACE_LOGGING
#define LOG_TRACE_ASYNC(logger, fmt_str, ...) \
	if(logger->severity() <= ember::log::Severity::TRACE) \
		logger->fmt_write<true>(ember::log::Severity::TRACE, fmt_str __VA_OPT__(,) __VA_ARGS__);
#else
#define LOG_TRACE_ASYNC(logger, fmt_str, ...) \
	if(false);
#endif

#if !NO_LOGGING && !NO_DEBUG_LOGGING
#define LOG_DEBUG_ASYNC(logger, fmt_str, ...) \
	if(logger->severity() <= ember::log::Severity::DEBUG) \
		logger->fmt_write<true>(ember::log::Severity::DEBUG, fmt_str __VA_OPT__(,) __VA_ARGS__);
#else
#define LOG_DEBUG_ASYNC(logger, fmt_str, ...) \
	if(false);
#endif

#if !NO_LOGGING && !NO_INFO_LOGGING
#define LOG_INFO_ASYNC(logger, fmt_str, ...) \
	if(logger->severity() <= ember::log::Severity::INFO) \
		logger->fmt_write<true>(ember::log::Severity::INFO, fmt_str __VA_OPT__(,) __VA_ARGS__);
#else
#define LOG_INFO_ASYNC(logger, fmt_str, ...) \
	if(false);
#endif

#if !NO_LOGGING && !NO_WARN_LOGGING
#define LOG_WARN_ASYNC(logger, fmt_str, ...) \
	if(logger->severity() <= ember::log::Severity::WARN) \
		logger->fmt_write<true>(ember::log::Severity::WARN, fmt_str __VA_OPT__(,) __VA_ARGS__);
#else
#define LOG_WARN_ASYNC(logger, fmt_str, ...) \
	if(false);
#endif

#if !NO_LOGGING && !NO_ERROR_LOGGING
#define LOG_ERROR_ASYNC(logger, fmt_str, ...) \
	if(logger->severity() <= ember::log::Severity::ERROR_) \
		logger->fmt_write<true>(ember::log::Severity::ERROR_, fmt_str __VA_OPT__(,) __VA_ARGS__);
#else
#define LOG_ERROR_ASYNC(logger, fmt_str, ...) \
	if(false);
#endif

#if !NO_LOGGING && !NO_FATAL_LOGGING
#define LOG_FATAL_ASYNC(logger, fmt_str, ...) \
	if(logger->severity() <= ember::log::Severity::FATAL) \
		logger->fmt_write<true>(ember::log::Severity::FATAL, fmt_str __VA_OPT__(,) __VA_ARGS__);
#else
#define LOG_FATAL_ASYNC(logger, fmt_str, ...) \
	if(false);
#endif

#if !NO_LOGGING && !NO_TRACE_LOGGING
#define LOG_TRACE_SYNC(logger, fmt_str, ...) \
	if(logger->severity() <= ember::log::Severity::TRACE) \
		logger->fmt_write<false>(ember::log::Severity::TRACE, fmt_str __VA_OPT__(,) __VA_ARGS__);
#else
#define LOG_TRACE_SYNC(logger, fmt_str, ...) \
	if(false);
#endif

#if !NO_LOGGING && !NO_DEBUG_LOGGING
#define LOG_DEBUG_SYNC(logger, fmt_str, ...) \
	if(logger->severity() <= ember::log::Severity::DEBUG) \
		logger->fmt_write<false>(ember::log::Severity::DEBUG, fmt_str __VA_OPT__(,) __VA_ARGS__);
#else
#define LOG_DEBUG_SYNC(logger, fmt_str, ...) \
	if(false);
#endif

#if !NO_LOGGING && !NO_INFO_LOGGING
#define LOG_INFO_SYNC(logger, fmt_str, ...) \
	if(logger->severity() <= ember::log::Severity::INFO) \
		logger->fmt_write<false>(ember::log::Severity::INFO, fmt_str __VA_OPT__(,) __VA_ARGS__);
#else
#define LOG_INFO_SYNC(logger, fmt_str, ...) \
	if(false);
#endif

#if !NO_LOGGING && !NO_WARN_LOGGING
#define LOG_WARN_SYNC(logger, fmt_str, ...) \
	if(logger->severity() <= ember::log::Severity::WARN) \
		logger->fmt_write<false>(ember::log::Severity::WARN, fmt_str __VA_OPT__(,) __VA_ARGS__);
#else
#define LOG_WARN_SYNC(logger, fmt_str, ...) \
	if(false);
#endif

#if !NO_LOGGING && !NO_ERROR_LOGGING
#define LOG_ERROR_SYNC(logger, fmt_str, ...) \
	if(logger->severity() <= ember::log::Severity::ERROR_) \
		logger->fmt_write<false>(ember::log::Severity::ERROR_, fmt_str __VA_OPT__(,) __VA_ARGS__);
#else
#define LOG_ERROR_SYNC(logger, fmt_str, ...) \
	if(false);
#endif

#if !NO_LOGGING && !NO_FATAL_LOGGING
#define LOG_FATAL_SYNC(logger, fmt_str, ...) \
	if(logger->severity() <= ember::log::Severity::FATAL) \
		logger->fmt_write<false>(ember::log::Severity::FATAL, fmt_str __VA_OPT__(,) __VA_ARGS__);
#else
#define LOG_FATAL_SYNC(logger, fmt_str, ...) \
	if(false);
#endif

// used to generate decorated output (e.g. 'namespace::func' vs simply 'func')
#if _MSC_VER && !__INTEL_COMPILER
#define log_func __FUNCTION__
#elif __clang__ || __GNUC__
#define log_func __PRETTY_FUNCTION__
#endif
