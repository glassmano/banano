#include <nano/boost/asio/bind_executor.hpp>
#include <nano/lib/rpc_handler_interface.hpp>
#include <nano/rpc/rpc.hpp>
#include <nano/rpc/rpc_connection.hpp>

#include <boost/format.hpp>

#include <iostream>

nano::rpc::rpc (std::shared_ptr<boost::asio::io_context> io_ctx_a, nano::rpc_config config_a, nano::rpc_handler_interface & rpc_handler_interface_a) :
	config (std::move (config_a)),
	io_ctx_shared (io_ctx_a),
	io_ctx (*io_ctx_shared),
	acceptor (io_ctx),
	rpc_handler_interface (rpc_handler_interface_a)
{
	rpc_handler_interface.rpc_instance (*this);
}

nano::rpc::~rpc ()
{
	if (!stopped)
	{
		stop ();
	}
}

void nano::rpc::start ()
{
	auto endpoint (boost::asio::ip::tcp::endpoint (boost::asio::ip::make_address_v6 (config.address), config.port));

	bool const is_loopback = (endpoint.address ().is_loopback () || (endpoint.address ().to_v6 ().is_v4_mapped () && boost::asio::ip::make_address_v4 (boost::asio::ip::v4_mapped, endpoint.address ().to_v6 ()).is_loopback ()));
	if (!is_loopback && config.enable_control)
	{
		logger.warn (nano::log::type::rpc, "WARNING: Control-level RPCs are enabled on non-local address {}, potentially allowing wallet access outside local computer", endpoint.address ().to_string ());
	}

	acceptor.open (endpoint.protocol ());
	acceptor.set_option (boost::asio::ip::tcp::acceptor::reuse_address (true));

	boost::system::error_code ec;
	acceptor.bind (endpoint, ec);
	if (ec)
	{
		logger.critical (nano::log::type::rpc, "Error while binding for RPC on port: {} ({})", endpoint.port (), ec.message ());
		throw std::runtime_error (ec.message ());
	}
	logger.info (nano::log::type::rpc, "RPC listening address: {}", fmt::streamed (acceptor.local_endpoint ()));
	acceptor.listen ();
	accept ();
}

void nano::rpc::accept ()
{
	auto connection (std::make_shared<nano::rpc_connection> (config, io_ctx, logger, rpc_handler_interface));
	acceptor.async_accept (connection->socket,
	boost::asio::bind_executor (connection->strand, [this_w = std::weak_ptr{ shared_from_this () }, connection] (boost::system::error_code const & ec) {
		auto this_l = this_w.lock ();
		if (!this_l)
		{
			return;
		}
		if (ec != boost::asio::error::operation_aborted && this_l->acceptor.is_open ())
		{
			this_l->accept ();
		}
		if (!ec)
		{
			connection->parse_connection ();
		}
		else
		{
			this_l->logger.error (nano::log::type::rpc, "Error accepting RPC connection: {}", ec.message ());
		}
	}));
}

void nano::rpc::stop ()
{
	stopped = true;
	acceptor.close ();
}

std::shared_ptr<nano::rpc> nano::get_rpc (std::shared_ptr<boost::asio::io_context> io_ctx_a, nano::rpc_config const & config_a, nano::rpc_handler_interface & rpc_handler_interface_a)
{
	return std::make_shared<nano::rpc> (io_ctx_a, config_a, rpc_handler_interface_a);
}
