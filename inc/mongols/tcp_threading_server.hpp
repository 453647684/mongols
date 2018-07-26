#ifndef TCP_THREADING_SERVER_HPP
#define TCP_THREADING_SERVER_HPP

#include "tcp_server.hpp"
#include "thead_pool.hpp"
#include <mutex>

namespace mongols {

    class tcp_threading_server : public tcp_server {
    public:
        tcp_threading_server() = delete;

        tcp_threading_server(const std::string& host, int port
                , int timeout = 5000
                , size_t buffer_size = 1024
                , size_t thread_size = std::thread::hardware_concurrency()
                , int max_event_size = 64);
        virtual~tcp_threading_server();
    public:
        virtual std::string get_session(const std::string&);
        virtual bool find_session(const std::string&);
        virtual void set_session(const std::string&, const std::string&);
    protected:

        virtual void add_client(int);
        virtual void del_client(int);
        virtual void process(int, const handler_function&);
        virtual bool send_to_all_client(int, const std::string&, const filter_handler_function&);
        virtual bool work(int, const handler_function&);

    private:

        std::mutex main_mtx;
        mongols::thread_pool work_pool;





    };
}

#endif /* TCP_THREADING_SERVER_HPP */

