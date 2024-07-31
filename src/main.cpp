#include <amqpcpp.h>
#include <amqpcpp/channel.h>
#include <amqpcpp/connection.h>
#include <amqpcpp/exchangetype.h>
#include <cstddef>
#include <cstdint>
#include <netinet/in.h>
#include <uv.h>
#include <uvw.hpp>
#include <uvw/dns.h>
#include <uvw/emitter.h>
#include <uvw/handle.hpp>
#include <uvw/stream.h>
#include <uvw/tcp.h>

#include "error.hpp"

enum HostKey {
    LOCAL,
    RABBIT,

    COUNT,
};

struct Host {
    const std::string address;
    const uint16_t    port;
};

inline void *get_in_addr(struct sockaddr *sa)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    if (sa->sa_family == AF_INET)
        return &((struct sockaddr_in *) sa)->sin_addr;
    return &((struct sockaddr_in6 *) sa)->sin6_addr;
#pragma GCC diagnostic pop
}

const std::string resolve_host(const std::shared_ptr<uvw::loop> &loop, const std::string &address, uint16_t port)
{
    sockaddr_in ipv4;
    if (uv_ip4_addr(address.c_str(), port, &ipv4) == 0)
        return address;

    sockaddr_in6 ipv6;
    if (uv_ip6_addr(address.c_str(), port, &ipv6) == 0)
        return address;

    // Address is neither IPv4 nor IPv6, try to resolve via DNS
    auto dnsReq = loop->resource<uvw::get_addr_info_req>();
    auto dnsRes = dnsReq->addr_info_sync(address, std::to_string(port));
    if (!dnsRes.first)
        FATAL("Failed to resolve endpoint {}:{}\n", address, port);

    char resolved[INET6_ADDRSTRLEN];
    if (inet_ntop(dnsRes.second->ai_family, get_in_addr(dnsRes.second->ai_addr), resolved, dnsRes.second->ai_addrlen))
        return {resolved};

    FATAL("Failed to parse resolve address for {}:{}\n", address, port);
}

namespace Rabbit
{
    namespace
    {
        std::shared_ptr<uvw::loop> loop;

        static const Host HOSTS[HostKey::COUNT] = {
            /* [HostKey::LOCAL] = */
            {.address = "localhost", .port = 2323},

            /* [HostKey::RABBIT] = */
            // Currently RabbitMQ is hosted on a docker on my local machine.
            // I have another part of the project in Rust that connected successfuly, declared queues etc without issue.
            // Thus, I *know* the issue comes from this implementation.
            {.address = "localhost", .port = 5672},
        };

        const std::string user     = "guest";
        const std::string password = "guest";
    }

    class ConnectionHandler : public AMQP::ConnectionHandler {
      public:
        ConnectionHandler();
        ~ConnectionHandler();

        int run(void) { return loop->run(); }

        void onReady(AMQP::Connection *con) override;

        void onError(AMQP::Connection *con, const char *message) override;

        void onData(AMQP::Connection *con, const char *buffer, size_t size) override;

      private:
        /** Connection handle */
        std::shared_ptr<uvw::tcp_handle> _tcp_con = nullptr;
        /** LiSteN handle */
        std::shared_ptr<uvw::tcp_handle> _tcp_lsn = nullptr;

        AMQP::Connection *_rmq_con  = nullptr;
        AMQP::Channel    *_rmq_chan = nullptr;

        std::vector<char> _frame = {};
    };

    ConnectionHandler::ConnectionHandler()
    {
        fmt::print("Creating conection handler\n");
        loop = uvw::loop::create();

        _tcp_con = Rabbit::loop->resource<uvw::tcp_handle>();
        _tcp_lsn = Rabbit::loop->resource<uvw::tcp_handle>();

        _tcp_con->on<uvw::error_event>([](const uvw::error_event &event, const uvw::tcp_handle &) {
            FATAL("[TCP Con]: Error - {}\n", event.what());
        });

        _tcp_con->on<uvw::connect_event>([this](const uvw::connect_event &, uvw::tcp_handle &) {
            fmt::print("[TCP Con]: Connect @{} | {}\n", user, password);
            _rmq_con = new AMQP::Connection(this, AMQP::Login(user, password), "/");
            _tcp_con->read();
        });

        _tcp_con->on<uvw::data_event>([this](const uvw::data_event &event, uvw::tcp_handle &) {
            _frame.insert(_frame.end(), event.data.get(), event.data.get() + event.length);

            if (_rmq_con->parse(_frame.begin().base(), _frame.size()) != 0)
                _frame.clear();
        });

        _tcp_lsn->on<uvw::listen_event>([](const uvw::listen_event &, uvw::tcp_handle &srv) {
            fmt::print("[TCP Lsn]: Listen\n");
            std::shared_ptr<uvw::tcp_handle> client = srv.parent().resource<uvw::tcp_handle>();

            srv.accept(*client);
            client->read();
        });

        {
            const auto &[address, port] = HOSTS[HostKey::LOCAL];
            _tcp_lsn->bind(resolve_host(loop, address, port), port);
        }

        {
            const auto &[address, port] = HOSTS[HostKey::RABBIT];
            _tcp_con->connect(resolve_host(loop, address, port), port);
        }
    }

    ConnectionHandler::~ConnectionHandler()
    {
        fmt::print("Destructing the connection handler\n");
        _tcp_con->close();
        _tcp_lsn->close();
        delete _rmq_con;
        delete _rmq_chan;
    }

    void ConnectionHandler::onReady(AMQP::Connection *con)
    {
        fmt::print("One day I'll see this message, I believe!\n");
        _rmq_chan = new AMQP::Channel(con);

        _rmq_chan->declareExchange("BigStore", AMQP::topic).onError([](const char *message) {
            FATAL("{}\n", message);
        });

        _rmq_chan->declareQueue("Item", AMQP::durable)
            .onSuccess([]() { fmt::print("Declared queue: Item\n"); })
            .onError([](const char *message) { fmt::print("Error declaring queue Item: {}\n", message); });
    }

    void ConnectionHandler::onError(AMQP::Connection *con, const char *message)
    {
        (void) con;
        FATAL("Rabbit error: {}\n", message);
    }

    void ConnectionHandler::onData(AMQP::Connection *con, const char *buffer, size_t size)
    {
        fmt::print("Data?\n");

        _tcp_con->write(const_cast<char *>(buffer), size);
        (void) con;
        (void) buffer;
        (void) size;
    }
}

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    Rabbit::ConnectionHandler rabbit;

    fmt::print("If you keep trying hard enough, at one point you might get lucky.\n");
    return rabbit.run();
}
