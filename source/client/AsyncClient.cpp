#include "client/AsyncClient.hpp"
#include "client/SocketFactory.hpp"
#include "net/Net.hpp"
#include "net/Socket.hpp"
#include "util/Thread.hpp"
#include <cassert>
namespace http
{
    AsyncRequest::~AsyncRequest()
    {
        if (auto client = detail.client.load())
        {
            //TODO: Ensure client can not get invalidated while still in this block
            assert(detail.future.valid());
            client->abort(this);
            detail.future.wait();
            assert(!detail.client.load());
        }
        else
        {
            assert(!detail.future.valid());
        }
    }

    AsyncClient::AsyncClient(const AsyncClientParams & _params)
        : params(_params)
        , mutex(), threads(), request_queue(), condition_var(), exiting(false)
        , rate_limit(params.rate_limit), rate_limit_time(time(nullptr))
    {
        params.default_headers.set_default("Host", params.host + ":" + std::to_string(params.port));

        start();
    }

    AsyncClient::~AsyncClient()
    {
        exit();
    }

    std::future<Response*>& AsyncClient::queue(AsyncRequest *request)
    {
        request->detail.future = request->detail.promise.get_future();
        request->detail.client = this;

        Lock lock(mutex);
        request_queue.push_back(request);
        condition_var.notify_one();
        return request->detail.future;
    }

    void AsyncClient::abort(AsyncRequest *request)
    {
        Lock lock(mutex);
        for (auto it = request_queue.begin(); it != request_queue.end(); ++it)
        {
            if (*it == request)
            {
                request_queue.erase(it);
                request->detail.promise.set_value(nullptr);
                request->detail.client = nullptr;
                return;
            }
        }
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
        {
            Lock lock(mutex);
            for (auto &req : request_queue)
            {
                req->detail.promise.set_value(nullptr);
                req->detail.client = nullptr;
            }
            request_queue.clear();
        }
    }

    void AsyncClient::rate_limit_wait()
    {
        if (params.rate_limit <= 0) return; //no limit

        //atomic decrement, no need to lock if not managing the reset
        if (--rate_limit >= 0) return;
        //only one thread actively waits to reset the count, the others will block on the mutex and
        //then continue using this second int decrement
        std::unique_lock<std::mutex> lock(rate_limit_mutex);
        if (--rate_limit >= 0) return;


        //Make the thread sleep until the second after the previous allowance timestamp
        using std::chrono::steady_clock;
        using std::chrono::seconds;
        auto now = std::chrono::duration_cast<seconds>(steady_clock::now().time_since_epoch());

        rate_limit_time += seconds(1);
        if (rate_limit_time < now)
        {   // More than a second passed since the last allowance anyway
            rate_limit_time = now;
        }
        else
        {   //Wait for reset second
            std::this_thread::sleep_until(steady_clock::time_point(rate_limit_time));
        }
        rate_limit = params.rate_limit;
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
        set_thread_name("http::AsyncClient");
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
                    client->request_queue.pop_front();
                    break;
                }
            }

            process_request(request);
        }
    }

    void AsyncClient::Thread::process_request(AsyncRequest * request)
    {
        bool complete = false;
        bool called_complete = false;
        try
        {
            for (auto &header : client->params.default_headers)
            {
                request->headers.set_default(header.first, header.second);
            }
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
            request->detail.response = conn.recv_response();

            //Complete
            complete = true;
            request->detail.client = nullptr;
            if (request->on_completion)
            {
                called_complete = true;
                request->on_completion(request, request->detail.response);
            }
            request->detail.promise.set_value(&request->detail.response);
        }
        catch (const std::exception &)
        {
            request->detail.client = nullptr;
            if (!complete) //Some sort of connection or protocol error, dont try to reuse
            {
                conn.reset(nullptr);
            }
            if (!called_complete && request->on_exception)
            {
                try
                {
                    request->on_exception(request);
                }
                catch (const std::exception &)
                {}
            }
            request->detail.promise.set_exception(std::current_exception());
        }
    }


    void AsyncClient::Thread::wait_for_exit()
    {
        assert(client->exiting);
        if (thread.joinable()) thread.join();
    }
}
