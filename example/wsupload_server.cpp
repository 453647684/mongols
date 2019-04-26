#include <fstream>
#include <mongols/ws_server.hpp>

int main(int, char**)
{
    int port = 9090;
    const char* host = "127.0.0.1";
    mongols::ws_server server(host, port, 5000, 8096, std::thread::hardware_concurrency() /*0*/);
    //    if (!server.set_openssl("openssl/localhost.crt", "openssl/localhost.key")) {
    //        return -1;
    //    }

    auto f = [](const std::string& input, bool& keepalive, bool& send_to_other, mongols::tcp_server::client_t& client, mongols::tcp_server::filter_handler_function& send_to_other_filter, mongols::ws_server::ws_message_t& ws_msg_type) -> std::string {
        keepalive = KEEPALIVE_CONNECTION;
        send_to_other = false;
        if (ws_msg_type == mongols::ws_server::ws_message_t::BINARY) {
            std::ofstream of("html/upload.bin", std::ios::binary | std::ios::out);
            of << input;
            ws_msg_type = mongols::ws_server::ws_message_t::TEXT;
            return "upload success";
        }

        return input;
    };
    //server.set_enable_origin_check(true);
    //server.set_origin("http://localhost");
    //server.set_max_send_limit(5);
    server.run(f);
    //server.run();
}