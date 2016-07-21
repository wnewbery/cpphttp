#include "client/AsyncClient.hpp"
#include "client/SocketFactory.hpp"
#include "net/Net.hpp"
#include "net/Socket.hpp"
#include <cassert>
namespace http
{
    AsyncClient::AsyncClient(const AsyncClientParams & params)
        : params(params)
        , mutex(), threads(), request_queue(), condition_var(), exiting(false)
    {
        start();
    }

    AsyncClient::~AsyncClient()
    {
        exit();
    }

    std::future<Response*>& AsyncClient::make_request(AsyncRequest *request)
    {
        request->promise = std::promise<Response*>();
        request->future = request->promise.get_future();
        request->response.body.clear();
        request->response.headers.clear();

        Lock lock(mutex);
        request_queue.push(request);
        condition_var.notify_one();
        return request->future;
    }

    void AsyncClient::start()
    {
        Lock lock(mutex);
        exiting = false;
        while (threads.size() < params.max_connections)
        {
            threads.emplace_back(this);
        }
    }

    void AsyncClient::exit()
    {
        {
            Lock lock(mutex);
            exiting = true;
            condition_var.notify_all();
        }
        for (auto &thread : threads)
        {
            thread.wait_for_exit();
        }
        threads.clear();
    }

    AsyncClient::Thread::Thread(AsyncClient * client)
        : client(client), thread(), conn()
    {
        thread = std::thread(std::bind(&AsyncClient::Thread::main, this));
    }

    AsyncClient::Thread::~Thread()
    {
    }

    void AsyncClient::Thread::main()
    {
        while (true)
        {
            AsyncRequest *request;
            while (true)
            {
                Lock lock(client->mutex);
                if (client->exiting) return;
                else if (client->request_queue.empty())
                {
                    client->condition_var.wait(lock);
                }
                else
                {
                    request = client->request_queue.front();
                    client->request_queue.pop();
                    break;
                }
            }

            process_request(request);
        }
    }

    void AsyncClient::Thread::process_request(AsyncRequest * request)
    {
        bool called_complete = false;
        try
        {
            //Send
            bool sent = false;
            if (conn.is_connected())
            {
                // Try to re-use a Keep-Alive connection, but if not able to, use a new one
                try
                {
                    conn.send_request(*request);
                    sent = true;
                }
                catch (const SocketError &) {}
            }
            if (!sent)
            {
                //Send with new connection, if this fails abort
                conn.reset(client->params.socket_factory->connect(
                    client->params.host, client->params.port, client->params.tls));
                conn.send_request(*request);
            }

            //Receive
            request->response = conn.recv_response();

            //Complete
            if (request->on_completion)
            {
                called_complete = true;
                request->on_completion(request, request->response);
            }
            request->promise.set_value(&request->response);
        }
        catch (const std::exception &)
        {
            if (!called_complete && request->on_exception)
            {
                try
                {
                    request->on_exception(request);
                }
                catch (const std::exception &)
                {}
            }
            request->promise.set_exception(std::current_exception());
        }
    }


    void AsyncClient::Thread::wait_for_exit()
    {
        assert(client->exiting);
        if (thread.joinable()) thread.join();
    }
}
