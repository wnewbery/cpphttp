#pragma once
#include <string>
#include <stdexcept>

namespace http
{
    /**Initialise networking components. This must be called before first use.*/
    void init_net();

    /**Get a string description for a C errno code.*/
    std::string errno_string(int err);
#ifdef _WIN32
    /**Get a string description for a Win32 error code.*/
    std::string win_error_string(int err);
    /**Get a string description for a WinSock error code.*/
    inline std::string wsa_error_string(int err)
    {
        return win_error_string(err);
    }
    /**Get a string description for a socket error code.*/
    inline std::string socket_error_string(int err)
    {
        return wsa_error_string(err);
    }
#else
    /**Get a string description for a socket error code.*/
    inline std::string socket_error_string(int err)
    {
        return errno_string(err);
    }
#endif
    /**Get the most recent error code from the networking library (WSAGetLastError or errno).*/
    int last_net_error();
    /**True if the error code says a non-blocking operation would have blocked.
     * WSAEWOULDBLOCK, EAGAIN or EWOULDBLOCK.
     */
    bool would_block(int err);

    /**Base exception type for socket or other low level networking (e.g. DNS) errors.*/
    class NetworkError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    /**Base exception type for all socket errors, such as unexpected disconnects.*/
    class SocketError : public NetworkError
    {
    public:
        /**Construct using a descriptive message.*/
        explicit SocketError(const std::string &msg) : NetworkError(msg) {}
        /**Construct using socket_error_string.*/
        explicit SocketError(int err) : NetworkError(socket_error_string(err)) {}
        /**Construct using a descriptive message plus socket_error_string.*/
        SocketError(const std::string &msg, int err)
            : NetworkError(msg + ": " + socket_error_string(err))
        {}
        /**Construct using a descriptive message plus socket_error_string.*/
        SocketError(const char *msg, int err)
            : NetworkError(std::string(msg) + ": " + socket_error_string(err))
        {}
    };

    /**Errors connecting a socket.*/
    class ConnectionError : public SocketError
    {
    public:
        /**Construct using socket_error_string to get the error reason.*/
        ConnectionError(int err, const std::string &host, int port)
            : SocketError(make_message(err, host, port))
        {
        }
        /**Construct using a specific error message.*/
        ConnectionError(const std::string &err, const std::string &host, int port)
            : SocketError(make_message(err, host, port))
        {
        }
        /**Construct using last_net_error to get the error reason.*/
        ConnectionError(const std::string &host, int port)
            : SocketError(make_message(last_net_error(), host, port))
        {
        }
    private:
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

    /**Failed to verify the servers certificate.*/
    class CertificateVerificationError : public ConnectionError
    {
    public:
        CertificateVerificationError(const std::string &host, int port)
            : ConnectionError("Certificate verification failed.", host, port)
        {
        }
    };
}
