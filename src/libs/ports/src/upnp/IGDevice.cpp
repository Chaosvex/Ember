/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <ports/upnp/IGDevice.h>
#include <ports/upnp/Utility.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <format>
#include <utility>
#include <ranges>
#include <regex>

namespace ember::ports::upnp {

IGDevice::IGDevice(boost::asio::io_context& ctx, std::string bind,
                   std::string service, const std::string& location)
	: ctx_(ctx), port_(80), service_(std::move(service)), bind_(std::move(bind)) {
	parse_location(location);
}

void IGDevice::parse_location(const std::string& location) {
	constexpr auto pattern = R"((?:http:\/\/)(?:((?:[a-zA-Z0-9.-]*))?(?::((?:[1-9]{1}))"
	                         R"((?:[0-9]{0,4})))?)(\/{1}[a-zA-Z0-9-.]*))";
	std::regex regex(pattern, std::regex::ECMAScript);
	std::smatch matches;

	std::regex_search(location, matches, regex);

	if(matches.size() < 3) {
		throw std::invalid_argument("Invalid location");
	}

	http_host_ = matches[1];
	hostname_ = http_host_;

	if(matches.size() == 3) {
		dev_desc_uri_ = matches[2];
	} else {
		port_ = std::stoi(matches[2]);
		dev_desc_uri_ = matches[3];
	}
}

std::string IGDevice::build_soap_request(const UPnPActionArgs&& action) {
	constexpr std::string_view soap =
		R"(<?xml version="1.0"?>)" "\r\n"
		R"(<s:Envelope)" "\r\n"
		R"(	xmlns:s="http://schemas.xmlsoap.org/soap/envelope/")" "\r\n"
		R"(s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">)" "\r\n"
		R"(	<s:Body>)" "\r\n"
		R"(		<u:{})" "\r\n"
		R"(			xmlns:u="urn:schemas-upnp-org:service:{}">)" "\r\n"
		R"(			{})" "\r\n"
		R"(		</u:{}>)" "\r\n"
		R"(	</s:Body>)" "\r\n"
		R"(</s:Envelope>)";

	std::string args;

	for(auto& [k, v] : action.arguments) {
		args += std::format("<{}>{}</{}>\r\n", k, v, k);
	}

	return std::format(soap, action.action, action.service, args, action.action);
}

template<typename BufType>
BufType IGDevice::build_http_request(const HTTPRequest& request) {
	BufType output;
	std::format_to(std::back_inserter(output), "{} {} HTTP/1.1\r\n", request.method, request.url);
	
	for(auto& [k, v]: request.fields) {
		std::format_to(std::back_inserter(output), "{}: {}\r\n", k, v);
	}

	std::format_to(std::back_inserter(output), "User-Agent: Ember\r\n\r\n");

	if(!request.body.empty()) {
		std::format_to(std::back_inserter(output), "{}", request.body);
	}

	return output;
}

std::string IGDevice::protocol_to_string(const Protocol protocol) {
	switch(protocol) {
		case Protocol::TCP:
			return "TCP";
		case Protocol::UDP:
			return "UDP";
		default:
			throw std::invalid_argument("Bad protocol specified");
	}
}

UPnPActionArgs IGDevice::build_upnp_add_mapping(const Mapping& mapping) {
	auto protocol = protocol_to_string(mapping.protocol);

	UPnPActionArgs args {
		.action = "AddPortMapping",
		.service = service_,
		.arguments {
			{"NewInternalPort",   std::to_string(mapping.internal)},
			{"NewExternalPort",   std::to_string(mapping.external)},
			{"NewProtocol",       protocol},
			{"NewLeaseDuration",  std::to_string(mapping.ttl)},
			{"NewEnabled",        "1"},
			{"NewRemoteHost",     "0"},
			{"NewInternalClient", mapping.internal_ip},
			{"NewPortMappingDescription", "Ember"},
		}
	};

	return args;
}

UPnPActionArgs IGDevice::build_upnp_del_mapping(const Mapping& mapping) {
	std::string protocol = protocol_to_string(mapping.protocol);

	UPnPActionArgs args {
		.action = "DeletePortMapping",
		.service = service_,
		.arguments {
			{"NewExternalPort",   std::to_string(mapping.external)},
			{"NewProtocol",       protocol},
			{"NewRemoteHost",     "0"},	
		}
	};

	return args;
}

template<typename BufType>
BufType IGDevice::build_http_post_request(std::string&& body, const std::string& action,
                                          const std::string& control_url) {
	auto soap_action = std::format("{}#{}", service_, action);

	HTTPRequest request {
		.method = "POST",
		.url = control_url,
		.fields {
			{ "Host",           http_host_ },
			{ "Content-Type",   R"(text/xml; charset="utf-8")" },
			{ "Content-Length", std::to_string(body.size()) },
			{ "Connection",     "keep-alive" },
			{ "SOAPAction",     std::move(soap_action) },
		},
		.body = std::move(body)
	};

	return build_http_request<BufType>(request);
}

ba::awaitable<void> IGDevice::request_igdd(HTTPTransport& transport) {
	HTTPRequest http_req{
		.method = "GET",
		.url = dev_desc_uri_,
		.fields {
			{ "Host",           http_host_ },
			{ "Accept",        R"(text/html,application/xhtml+xml,application/xml;)" },
			{ "Connection",     "keep-alive" },
		}
	};

	auto buffer = build_http_request<std::vector<std::uint8_t>>(http_req);
	co_await transport.send(std::move(buffer));
}

ba::awaitable<void> IGDevice::request_scpd(HTTPTransport& transport) {
	auto scpd_uri = igdd_xml_->get_node_value(service_, "SCPDURL");

	if(!scpd_uri) {
		throw std::invalid_argument("Missing SCPDURL");
	}

	HTTPRequest http_req{
		.method = "GET",
		.url = *scpd_uri,
		.fields {
			{ "Host",           http_host_ },
			{ "Accept",        R"(text/html,application/xhtml+xml,application/xml;)" },
			{ "Connection",     "keep-alive" },
		}
	};

	auto buffer = build_http_request<std::vector<std::uint8_t>>(http_req);
	co_await transport.send(std::move(buffer));
}

ba::awaitable<void> IGDevice::refresh_scpd(HTTPTransport& transport) {
	co_await request_scpd(transport);
	const auto& [header, buffer] = co_await transport.receive_http_response();
	const auto body = http_body_view(header, buffer);

	if(auto field = header.fields.find("Cache-Control"); field != header.fields.end()) {
		try {
			const auto value = sv_to_int(split_argument(field->second, '='));
			scpd_cc_ = std::chrono::steady_clock::now() + std::chrono::seconds(value);
		} catch(std::invalid_argument&) {
			scpd_cc_ = std::chrono::steady_clock::now();
		}
	} else {
		scpd_cc_ = std::chrono::steady_clock::now();
	}

	scpd_xml_ = std::make_unique<SCPDXMLParser>(body);
}

std::string_view IGDevice::http_body_view(const HTTPHeader& header, std::span<const char> buffer) {
	if(header.code != HTTPStatus::OK 
	   || header.fields.find("Content-Length") == header.fields.end()) {
		throw std::invalid_argument("Bad HTTP response");
	}

	const auto length = sv_to_int(header.fields.at("Content-Length"));
	std::string_view body { buffer.end() - length, buffer.end() };
	return body;
}

ba::awaitable<void> IGDevice::refresh_igdd(HTTPTransport& transport) {
	co_await request_igdd(transport);
	const auto& [header, buffer] = co_await transport.receive_http_response();
	const auto body = http_body_view(header, buffer);

	if(auto field = header.fields.find("Cache-Control"); field != header.fields.end()) {
		try {
			const auto value = sv_to_int(split_argument(field->second, '='));
			igdd_cc_ = std::chrono::steady_clock::now() + std::chrono::seconds(value);
		} catch(std::invalid_argument&) {
			igdd_cc_ = std::chrono::steady_clock::now();
		}
	} else {
		igdd_cc_ = std::chrono::steady_clock::now();
	}
	
	igdd_xml_ = std::make_unique<XMLParser>(body);
}

ba::awaitable<void> IGDevice::refresh_xml_cache(HTTPTransport& transport) {
	const auto time = std::chrono::steady_clock::now();

	if(time > igdd_cc_) {
		co_await refresh_igdd(transport);
	}

	if(time > scpd_cc_) {
		co_await refresh_scpd(transport);
	}
}

ba::awaitable<ErrorCode> IGDevice::process_request(HTTPTransport& transport, use_awaitable_t) try {
	co_await transport.connect(hostname_, port_);
	co_await refresh_xml_cache(transport);	
	co_return ErrorCode::SUCCESS;
} catch(boost::system::error_code&) {
	co_return ErrorCode::NETWORK_FAILURE;
} catch(std::exception&)  {
	co_return  ErrorCode::HTTP_BAD_RESPONSE;
}

ba::awaitable<void> IGDevice::process_request(std::shared_ptr<UPnPRequest> request) {
	ErrorCode ec { ErrorCode::SUCCESS };

	try {
		co_await request->transport->connect(hostname_, port_);
		co_await refresh_xml_cache(*request->transport);
	}  catch(boost::system::error_code&) {
		ec = ErrorCode::NETWORK_FAILURE;
	} catch(std::exception&)  {
		ec = ErrorCode::HTTP_BAD_RESPONSE;
	}

	co_await request->handler(*request->transport, ec);
}


ba::awaitable<ErrorCode> IGDevice::do_delete_port_mapping(const Mapping& mapping,
                                                          HTTPTransport& transport) {
	const auto post_uri = igdd_xml_->get_node_value(service_, "controlURL");

	if(!post_uri) {
		co_return ErrorCode::SOAP_MISSING_URI;
	}

	if(mapping.protocol == Protocol::ALL) {
		co_return ErrorCode::INVALID_MAPPING_ARG;
	}

	auto args = build_upnp_del_mapping(mapping);

	if(auto ec = validate_soap_arguments(args)) {
		co_return ec;
	}

	auto body = build_soap_request(std::move(args));
	auto request = build_http_post_request<std::vector<std::uint8_t>>(
		std::move(body), "DeletePortMapping", *post_uri
	);

	co_await transport.send(std::move(request));
	const auto& [header, buffer] = co_await transport.receive_http_response();

	if(header.code != HTTPStatus::OK) {
		co_return ErrorCode::HTTP_NOT_OK;
	}

	co_return ErrorCode::SUCCESS;
}

/*
   Checks to ensure that all expected action arguments are present in what
   we're about to send. However, we do not perform type checking.
 */
ErrorCode IGDevice::validate_soap_arguments(const UPnPActionArgs& args) {
	const auto& expected_args = scpd_xml_->arguments(args.action, "in");

	if(expected_args.empty()) {
		return ErrorCode::SOAP_NO_ARGUMENTS;
	}

	for(const auto& arg : expected_args) {
		bool found = false;

		// didn't originally have a use for a map
		for(const auto& key : args.arguments | std::views::keys) {
			if(key == arg) {
				found = true;
				break;
			}
		}

		if(!found) {
			return ErrorCode::SOAP_ARGUMENTS_MISMATCH;
		}
	}

	return ErrorCode::SUCCESS;
}

ba::awaitable<ErrorCode> IGDevice::do_add_port_mapping(Mapping mapping, HTTPTransport& transport) {
	const auto post_uri = igdd_xml_->get_node_value(service_, "controlURL");

	if(!post_uri) {
		co_return ErrorCode::SOAP_MISSING_URI;
	}

	if(mapping.internal_ip.empty()) {
		mapping.internal_ip = transport.local_endpoint().address().to_string();
	}

	if(mapping.protocol == Protocol::ALL) {
		co_return ErrorCode::INVALID_MAPPING_ARG;
	}

	auto args = build_upnp_add_mapping(mapping);
	if(auto ec = validate_soap_arguments(args)) {
		co_return ec;
	}

	auto body = build_soap_request(std::move(args));

	auto request = build_http_post_request<std::vector<std::uint8_t>>(
		std::move(body), "AddPortMapping", *post_uri
	);

	co_await transport.send(std::move(request));
	const auto& [header, _] = co_await transport.receive_http_response();
	
	if(header.code != HTTPStatus::OK) {
		co_return ErrorCode::HTTP_NOT_OK;
	}

	co_return ErrorCode::SUCCESS;
}

void IGDevice::launch_request(UPnPRequest::Handler&& handler) {
	auto transport = std::make_unique<HTTPTransport>(ctx_, bind_);

	UPnPRequest request {
		.transport = std::move(transport),
		.handler = std::move(handler),
		.device = shared_from_this()
	};

	auto request_ptr = std::make_shared<UPnPRequest>(std::move(request));
	ba::co_spawn(ctx_, process_request(std::move(request_ptr)), ba::detached);
}

void IGDevice::add_port_mapping(Mapping mapping, Result cb) {
	auto handler = [&, mapping, cb = std::move(cb)](HTTPTransport& transport,
	                                                ErrorCode ec) -> ba::awaitable<void> {
		if(!ec) {
			auto result = co_await do_add_port_mapping(mapping, transport);
			cb(result);
		} else {
			cb(ec);
		}
	};

	launch_request(std::move(handler));
}

void IGDevice::delete_port_mapping(Mapping mapping, Result cb) {
	auto handler = [&, mapping, cb = std::move(cb)](HTTPTransport& transport,
	                                                ErrorCode ec) -> ba::awaitable<void> {
		if(!ec) {
			auto result = co_await do_delete_port_mapping(mapping, transport);
			cb(result);
		} else {
			cb(ec);
		}
	};

	launch_request(std::move(handler));
}

ba::awaitable<ErrorCode> IGDevice::add_port_mapping(const Mapping& mapping, use_awaitable_t) {
	HTTPTransport transport(ctx_, bind_);

	auto res = co_await process_request(transport, use_awaitable);

	if(!res) {
		co_return res;
	}

	co_return co_await do_add_port_mapping(mapping, transport);
}

std::future<ErrorCode> IGDevice::add_port_mapping(const Mapping& mapping, use_future_t) {
	auto promise = std::make_shared<std::promise<ErrorCode>>();
	auto future = promise->get_future();
	
	add_port_mapping(mapping, [&, promise](ErrorCode ec) {
		promise->set_value(ec);
	});

	return future;
}

ba::awaitable<ErrorCode> IGDevice::delete_port_mapping(Mapping mapping, use_awaitable_t) {
	HTTPTransport transport(ctx_, bind_);
	auto res = co_await process_request(transport, use_awaitable);

	if(!res) {
		co_return res;
	}

	co_return co_await do_delete_port_mapping(mapping, transport);
}

std::future<ErrorCode> IGDevice::delete_port_mapping(Mapping mapping, use_future_t) {
	auto promise = std::make_shared<std::promise<ErrorCode>>();
	auto future = promise->get_future();
	
	delete_port_mapping(mapping, [&, promise](ErrorCode ec) {
		promise->set_value(ec);
	});

	return future;
}

const std::string& IGDevice::host() const {
	return hostname_;
}

} // upnp, ports, ember