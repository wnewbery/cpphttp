#pragma once
#include "SocketFactory.hpp"
#include "ClientConnection.hpp"
#include <functional>
#include <thread>
#include <future>
#include <deque>
#include <list>
#include <chrono>
#include "../Request.hpp"
#include "../Response.hpp"
#include "../Headers.hpp"
namespace http
{
    class AsyncClient;
    class AsyncRequest;
    /**Request that can be processed asyncronously.
     * This object must remain valid until the async client is finished with it.
     */
    class AsyncRequest : public Request
    {
    public:
        /**If not null, then this function will be invoked once the request has been completed
         * and before promise.set_value.
         *
         * If the handler throws and exception derived from std::exception, it will be passed to
         * the future *instead of* the response object. Other exception types are not supported,
         * how the async client handles them is undefined. on_exception will not be called.
         *
         * Note that it is not defined how many threads the AsyncClient will use. Users must be
         * prepared for multiple completion handlers to be invoked at once by different threads,
         * but also for the client to be blocked waiting for the handler to return as one thread
         * may process multiple requests.
         *
         * Users should therefore ensure that a completion handler never blocks waiting for some
         * other async request, or for the client itself to exit. For performance, users should
         * also ensure handlers return quickly.
         */
        std::function<void(AsyncRequest *request, Response &response)> on_completion;

        /**If not null, then this function will be invoked within a std::exception catch
         * and before promise.set_exception.
         *
         * The same thread saftey and exception concerns apply as to on_completion.
         *
         * Note that if this handler throws an exception, that exception will be lost. The
         * promise will still use the original exception.
         */
        std::function<void(AsyncRequest *request)> on_exception;

        AsyncRequest() {}
        AsyncRequest(const AsyncRequest&) = delete;
        AsyncRequest& operator = (const AsyncRequest&) = delete;
        AsyncRequest(AsyncRequest&&) = default;
        AsyncRequest& operator = (AsyncRequest&&) = default;

        ~AsyncRequest();

        /**Wait for the promise. The returned promise is valid as long as this request object is.
         * The implementation uses std::future, and exceptions in the async client processing this
         * request will be forwarded and rethrown.
         */
        Response *wait()
        {
            return detail.future.get();
        }
        /**Reset so ready to be re-used.
         * The Request fields, on_completion and on_exception are left unaltered.
         * 
         * Not thread safe.
         */
        void reset()
        {
            detail.promise = std::promise<Response*>();
            detail.response.body.clear();
            detail.response.headers.clear();
        }

        /**Implementation details for use by AsyncClient.*/
        struct Detail
        {
            /**Promise that can recieve the response syncronously.*/
            std::promise<Response*> promise;
            /**std::future for promise.*/
            std::future<Response*> future;
            /**The client performing this request.*/
            std::atomic<AsyncClient*> client;

            /**Storage for the response, intended for the async implementations benefit.
             * Client should get the response from the callback or future, and this field is not
             * defined to be valid at any point in time.
             */
            Response response;

            Detail() : promise(), future(), client(nullptr), response() {}

            Detail(const Detail&) = delete;
            Detail& operator = (const Detail&) = delete;

            Detail(Detail &&mv)
                : promise(std::move(mv.promise))
                , future(std::move(mv.future))
                , client(mv.client.load())
            {
                mv.client = nullptr;
            }
            Detail& operator = (Detail &&mv)
            {
                promise = std::move(mv.promise);
                future = std::move(mv.future);
                client = mv.client.load();
                mv.client = nullptr;
                return *this;
            }
        }detail;
    };
    /**Params for AsyncClient.*/
    struct AsyncClientParams
    {
        AsyncClientParams()
            : host(), port(80), tls(false), max_connections(4), rate_limit(-1)
            , socket_factory(nullptr), default_headers()
        {}

        /**Host to connect to.*/
        std::string host;
        /**Port to connect to.*/
        uint16_t port;
        /**Use TLS.*/
        bool tls;
        /**The maximun number of connections that the client may establish as any one time.
         * Multiple connections can improve throughput, due to the blocking nature of the
         * underlying protocol, but each has some overhead, and remote servers often impose limits.
         */
        unsigned max_connections;
        /**If non-zero, this is the maximun number of requests that will be sent in a second across
         * all connections.
         */
        int rate_limit;
        /**The socket factory to use to establish connections.
         * The socket factory must remain valid as long as the client, and the "connect" method
         * must be thread-safe.
         */
        SocketFactory *socket_factory;
        /**Headers to add to requests by default, if the request does not already have such a
         * header.
         *
         * Note that client automatically adds Date, Content-Length, Transfer-Encoding and
         * Connection: Keep-Alive as needed.
         */
        Headers default_headers;
    };
    /**HTTP client using background threads to process requests.
     * See AsyncClientParams for configuration details.
     *
     * Requests are started in a FIFO manner, but because each request is run in parallel (upto
     * AsyncClientParams::max_connections), and since each parallel request is independent and can
     * take a different amount of time, the order of completion is likely to *not* be FIFO.
     */
    class AsyncClient
    {
    public:
        AsyncClient(const AsyncClientParams &params);
        ~AsyncClient();

        /**Queue a request to be processed.
         * @return The std::future for the request, allthough its use is not mandatory if the
         * request included callbacks.
         */
        std::future<Response*>& queue(AsyncRequest *request);
        /**Abort a pending request.*/
        void abort(AsyncRequest *request);
        /**Stop processing requests and wait for all internal threads to exit.
         * Any request already being executed will be completed, but future queued ones will not be
         * started, the promise will not be fullfilled, and callbacks will not be invoked.
         */
        void exit();
        /**Start processing requests (note that the client is constructed in the start state). */
        void start();
    private:
        typedef std::unique_lock<std::mutex> Lock;
        class Thread
        {
        public:
            Thread(AsyncClient *client);
            ~Thread();

            void main();
            void process_request(AsyncRequest *request);
            void wait_for_exit();

            AsyncClient *client;
            std::thread thread;
            /**The connection this thread uses.*/
            ClientConnection conn;
        };
        /**User-defined parameters used to process requests.*/
        AsyncClientParams params;

        /**Mutex protects shared state.
         * @todo Use of a lockless queue for request_queue would remove most of this requirement.
         */
        std::mutex mutex;
        /**List of threads.*/
        std::list<Thread> threads;
        /**FI-FO queue of requests to process.*/
        std::deque<AsyncRequest*> request_queue;
        /**Signals a change to request_queue or exiting.*/
        std::condition_variable condition_var;
        /**True if this client is exiting, and that threads should exit.*/
        std::atomic<bool> exiting;
        /**Current rate limit allowance.*/
        std::atomic<int> rate_limit;
        /**Time of last rate limit allocation.*/
        std::chrono::seconds rate_limit_time;
        /**Mutex to control resetting rate_limit and rate_limit_time.*/
        std::mutex rate_limit_mutex;
        /**Used by connections to block if needed to avoid exceeding the rate limit.*/
        void rate_limit_wait();
    };
}
