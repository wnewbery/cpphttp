#pragma once
#include <string>

namespace http
{
    void init_net();

    // extern SecurityFunctionTableW *sspi;

    std::string errno_string(int err);
    std::string win_error_string(int err);
    inline std::string wsa_error_string(int err)
    {
        return win_error_string(err);
    }
    inline std::string socket_error_string(int err)
    {
        return wsa_error_string(err);
    }
    int last_net_error();

    class NetworkError : public std::runtime_error
    {
    public:
        explicit NetworkError(const char *str) : std::runtime_error(str) {}
        explicit NetworkError(const std::string &str) : std::runtime_error(str) {}
    };

    class SocketError : public NetworkError
    {
    public:
        SocketError(const std::string &msg) : NetworkError(msg) {}
        SocketError(int err) : NetworkError(socket_error_string(err)) {}
        SocketError(const std::string &msg, int err)
            : NetworkError(msg + ": " + socket_error_string(err))
        {}
        SocketError(const char *msg, int err)
            : NetworkError(std::string(msg) + ": " + socket_error_string(err))
        {}
    };

    class ConnectionError : public SocketError
    {
    public:
        ConnectionError(int err, const std::string &host, int port)
            : SocketError(make_message(err, host, port))
        {
        }
        ConnectionError(const std::string &err, const std::string &host, int port)
            : SocketError(make_message(err, host, port))
        {
        }
        ConnectionError(const std::string &host, int port)
            : SocketError(make_message(last_net_error(), host, port))
        {
        }

        static std::string make_message(int err, const std::string &host, int port)
        {
            return make_message(socket_error_string(err), host, port);
        }
        static std::string make_message(const std::string &err, const std::string &host, int port)
        {
            std::string msg = "Failed to connect to " + host + ":" + std::to_string(port);
            msg += ". " + err;
            return msg;
        }
    };
}
