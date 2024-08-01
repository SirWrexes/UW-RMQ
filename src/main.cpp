#include <amqpcpp.h>
#include <amqpcpp/channel.h>
#include <amqpcpp/connection.h>
#include <amqpcpp/exchangetype.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <netinet/in.h>
#include <uv.h>
#include <uvw.hpp>
#include <uvw/dns.h>
#include <uvw/emitter.h>
#include <uvw/handle.hpp>
#include <uvw/stream.h>
#include <uvw/tcp.h>
#include <uvw/util.h>

#include "error.hpp"

namespace Rabbit
{
    namespace
    {
        std::shared_ptr<uvw::loop> loop;

        const uvw::socket_address SOCKET = {.ip = "localhost", .port = 5672};

        const std::string user     = "guest";
        const std::string password = "guest";
    }

    class ConnectionHandler : public AMQP::ConnectionHandler {
      public:
        ConnectionHandler();
        ~ConnectionHandler();

        int run(void) { return loop->run(); }

        void onData(AMQP::Connection *con, const char *bugger, size_t size) override;

        void onReady(AMQP::Connection *con) override;

        void onError(AMQP::Connection *con, const char *message) override;

      private:
        /** Connection handle */
        std::shared_ptr<uvw::tcp_handle> _tcp_con = nullptr;
        /** LiSteN handle */
        std::shared_ptr<uvw::tcp_handle> _tcp_lsn = nullptr;

        std::shared_ptr<AMQP::Connection> _rmq_con  = nullptr;
        std::shared_ptr<AMQP::Channel>    _rmq_chan = nullptr;

        std::vector<char> _frame = {};
    };

    ConnectionHandler::ConnectionHandler()
    {
        fmt::print("Creating conection handler\n");
        loop = uvw::loop::create();

        _tcp_con = Rabbit::loop->resource<uvw::tcp_handle>();
        _tcp_lsn = Rabbit::loop->resource<uvw::tcp_handle>();

        _tcp_lsn->on<uvw::listen_event>([](const uvw::listen_event &, uvw::tcp_handle &srv) {
            fmt::print("LSN.Listen\n");
            std::shared_ptr<uvw::tcp_handle> client = srv.parent().resource<uvw::tcp_handle>();

            client->no_delay(true);
            client->keep_alive(true, uvw::tcp_handle::time(60));
            client->on<uvw::error_event>([](const uvw::error_event &ev, uvw::tcp_handle &) {
                fmt::print("Client.Error: {}\n", ev.what());
            });
            client->on<uvw::close_event>([](const uvw::close_event &, uvw::tcp_handle &) {
                fmt::print("Client.Close\n");
            });
            client->on<uvw::end_event>([&client](const uvw::end_event &, uvw::tcp_handle &) {
                fmt::print("Client.End\n");
                client->close();
            });
            client->on<uvw::data_event>([](const uvw::data_event &ev, uvw::tcp_handle &) {
                // --Force linebreak in formatting--
                fmt::print("Client.Data: {}\n", std::string(ev.data.get(), ev.data.get() + ev.length));
            });
            client->on<uvw::write_event>([](const uvw::write_event &, uvw::tcp_handle &) {
                fmt::print("Client.Write\n");
            });
            if (srv.accept(*client) == 0)
                client->read();
        });

        _tcp_con->on<uvw::error_event>([](const uvw::error_event &ev, uvw::tcp_handle &) {
            FATAL("CON.Error: {}\n", ev.what());
        });

        _tcp_con->on<uvw::connect_event>([this](const uvw::connect_event &, uvw::tcp_handle &) {
            fmt::print("CON.Connect\n");
            _rmq_con =
                std::shared_ptr<AMQP::Connection>(new AMQP::Connection(this, AMQP::Login("guest", "guest"), "/"));
            _tcp_con->read();
        });

        _tcp_con->on<uvw::data_event>([this](const uvw::data_event &ev, uvw::tcp_handle &) {
            _frame.insert(_frame.end(), ev.data.get(), ev.data.get() + ev.length);

            if (_rmq_con->parse(_frame.begin().base(), _frame.size()) != 0)
                _frame.clear();
        });

        _tcp_lsn->bind({.ip = "::1", .port = 7100});
        _tcp_con->connect({.ip = "::1", .port = 5672});
    }

    ConnectionHandler::~ConnectionHandler()
    {
        fmt::print("Destructing the connection handler\n");
        _tcp_con->close();
        _tcp_lsn->close();
    }

    void ConnectionHandler::onReady(AMQP::Connection *con)
    {
        fmt::print("One day I'll see this message, I believe!\n");
        _rmq_chan = std::shared_ptr<AMQP::Channel>(new AMQP::Channel(con));

        _rmq_chan->declareExchange("BigStore", AMQP::topic).onError([](const char *message) {
            FATAL("{}\n", message);
        });

        _rmq_chan->declareQueue("Item", AMQP::durable)
            .onSuccess([]() { fmt::print("Declared queue: Item\n"); })
            .onError([](const char *message) { fmt::print("Error declaring queue Item: {}\n", message); });

        _tcp_lsn->listen();
    }

    void ConnectionHandler::onError(AMQP::Connection *, const char *message) { FATAL("Rabbit error: {}\n", message); }

    void ConnectionHandler::onData(AMQP::Connection *, const char *buffer, size_t size)
    {
        fmt::print("Data?\n");
        _tcp_con->write(const_cast<char *>(buffer), size);
    }
}

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    Rabbit::ConnectionHandler rabbit;

    fmt::print("If you keep trying hard enough, at one point you might get lucky.\n");
    fmt::print("Alive ? {}\n", Rabbit::loop->alive());
    return rabbit.run();
}
