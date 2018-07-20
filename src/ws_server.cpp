#include <algorithm>
#include <vector>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstring>

#include "ws_server.hpp"
#include "lib/websocket_parser.h"
#include "lib/libwshandshake.hpp"
#include "lib/json11.hpp"





namespace mongols {

    ws_threading_server::ws_threading_server(const std::string& host, int port, int timeout, size_t buffer_size, size_t thread_size, int max_event_size)
    : tcp_threading_server(host, port, timeout, buffer_size, thread_size, max_event_size) {

    }

    bool ws_threading_server::check_finished(const std::string& temp_input, std::string& input) {
        if (temp_input[0] == 'G') {
            return tcp_threading_server::check_finished(temp_input, input);
        }
        if(temp_input.empty()){
            input.push_back('0');
            return true;
        }

        struct ws_frame_t {
            websocket_flags opcode, is_final;
            char* body;
            size_t len;
            std::string* message;
        };
        ws_frame_t ws_frame;
        ws_frame.message = &input;

        websocket_parser_settings ws_settings;
        websocket_parser_settings_init(&ws_settings);

        ws_settings.on_frame_header = [](websocket_parser * parser) {
            ws_frame_t* THIS = (ws_frame_t*) parser->data;
            THIS->opcode = (websocket_flags) (parser->flags & WS_OP_MASK);
            THIS->is_final = (websocket_flags) (parser->flags & WS_FIN);
            if (parser->length) {
                THIS->body = (char*) malloc(parser->length);
                THIS->len = parser->length;
            }
            return 0;
        };
        ws_settings.on_frame_end = [](websocket_parser * parser) {
            ws_frame_t* THIS = (ws_frame_t*) parser->data;
            THIS->message->append(THIS->body, THIS->len);
            free(THIS->body);
            return 0;
        };

        ws_settings.on_frame_body = [](websocket_parser * parser, const char *at, size_t length) {
            ws_frame_t* THIS = (ws_frame_t*) parser->data;
            if (parser->flags & WS_HAS_MASK) {
                websocket_parser_decode(&THIS->body[parser->offset], at, length, parser);
            } else {
                memcpy(&THIS->body[parser->offset], at, length);

            }
            return 0;
        };


        websocket_parser ws_parser;
        websocket_parser_init(&ws_parser);
        ws_parser.data = &ws_frame;

        size_t nread = websocket_parser_execute(&ws_parser, &ws_settings, temp_input.c_str(), temp_input.size());

        if (nread != temp_input.size()) {
            input.push_back('0');
            return true;
        }

        if (ws_frame.is_final == WS_FIN) {
            if (ws_frame.opcode == WS_OP_TEXT) {
                input.push_back('1');
            } else if (ws_frame.opcode == WS_OP_BINARY) {
                input.push_back('2');
            } else if (ws_frame.opcode == WS_OP_PING) {
                input.push_back('9');
            } else if (ws_frame.opcode == WS_OP_CLOSE) {
                input.push_back('8');
            } else {
                input.push_back('0');
            }
            return true;
        }
        return false;


    }

    ws_server::ws_server(const std::string& host, int port, int timeout
            , size_t buffer_size, size_t thread_size, int max_event_size)
    : server(host, port, timeout, buffer_size, thread_size, max_event_size) {

    }

    void ws_server::run(const handler_function& f) {
        this->server.run(std::bind(&ws_server::work, this
                , std::cref(f)
                , std::placeholders::_1
                , std::placeholders::_2
                , std::placeholders::_3
                , std::placeholders::_4));
    }

    void ws_server::run() {
        handler_function f = std::bind(&ws_server::ws_message_parse, this
                , std::placeholders::_1
                , std::placeholders::_2
                , std::placeholders::_3
                , std::placeholders::_4
                , std::placeholders::_5
                );
        this->server.run(std::bind(&ws_server::work, this
                , f
                , std::placeholders::_1
                , std::placeholders::_2
                , std::placeholders::_3
                , std::placeholders::_4
                ));
    }

    std::string ws_server::ws_message_parse(const std::string& message
            , bool& keepalive
            , bool& send_to_other
            , std::pair<size_t, size_t>& g_u_id
            , tcp_server::filter_handler_function& send_to_other_filter) {

        std::string err;
        json11::Json root = json11::Json::parse(message, err);
        if (err.empty()) {
            if (!root["uid"].is_number()
                    || !root["gid"].is_number()
                    || !root["gfilter"].is_array()
                    || !root["ufilter"].is_array()) {
            } else {
                if (g_u_id.first == 0) {
                    g_u_id.first = root["gid"].int_value();
                }
                if (g_u_id.second == 0) {
                    g_u_id.second = root["uid"].int_value();
                }
                keepalive = KEEPALIVE_CONNECTION;
                send_to_other = true;
                std::vector<size_t> gfilter, ufilter;
                for (auto &i : root["gfilter"].array_items()) {
                    if (i.is_number()) {
                        gfilter.push_back(i.int_value());
                    }
                }
                for (auto &i : root["ufilter"].array_items()) {
                    if (i.is_number()) {
                        ufilter.push_back(i.int_value());
                    }
                }
                send_to_other_filter = [ = ](const std::pair<size_t, size_t>& cur_g_u_id){
                    bool res = false;
                    if (gfilter.empty()) {
                        if (ufilter.empty()) {
                            res = true;
                        } else {
                            res = std::find(ufilter.begin(), ufilter.end(), cur_g_u_id.second) != ufilter.end();
                        }
                    } else {
                        if (ufilter.empty()) {
                            res = std::find(gfilter.begin(), gfilter.end(), cur_g_u_id.first) != gfilter.end();
                        } else {
                            res = std::find(gfilter.begin(), gfilter.end(), cur_g_u_id.first) != gfilter.end()
                                    && std::find(ufilter.begin(), ufilter.end(), cur_g_u_id.second) != ufilter.end();
                        }
                    }
                    return res;

                };
                return message;
            }
        } else {
            keepalive = CLOSE_CONNECTION;
            send_to_other = false;
        }

        return message;
    }

    std::pair<std::string, bool> ws_server::work(const handler_function& f
            , const std::string& input
            , bool& send_to_other
            , std::pair<size_t, size_t>& g_u_id
            , tcp_server::filter_handler_function& send_to_other_filter) {
        std::string response;
        bool keepalive = KEEPALIVE_CONNECTION;
        send_to_other = false;

        if (input[0] == 'G') {

            if (!this->ws_handshake(input, response)) {
                keepalive = CLOSE_CONNECTION;
            }
            goto ws_done;
        } else {

            std::string close_msg = "connection closed.", pong_msg = "pong", error_msg = "error message."
                    , binary_msg = "not accept binary message.", message;


            message.assign(input.c_str(), input.size() - 1);
            char a = input.back();
            if (a == '1') {
                f(message, keepalive, send_to_other, g_u_id, send_to_other_filter);
                size_t frame_len = websocket_calc_frame_size((websocket_flags) (WS_OP_TEXT | WS_FINAL_FRAME), message.size());
                char * frame = (char*) malloc(sizeof (char) * frame_len);
                frame_len = websocket_build_frame(frame, (websocket_flags) (WS_OP_TEXT | WS_FINAL_FRAME), NULL, message.c_str(), message.size());
                response.assign(frame, frame_len);
                free(frame);
            } else if (a == '2') {
                size_t frame_len = websocket_calc_frame_size((websocket_flags) (WS_OP_BINARY | WS_FINAL_FRAME), message.size());
                char * frame = (char*) malloc(sizeof (char) * frame_len);
                frame_len = websocket_build_frame(frame, (websocket_flags) (WS_OP_BINARY | WS_FINAL_FRAME), NULL, message.c_str(), message.size());
                response.assign(frame, frame_len);
                free(frame);
                send_to_other = true;
            } else if (a == '8') {
                size_t frame_len = websocket_calc_frame_size((websocket_flags) (WS_OP_CLOSE | WS_FINAL_FRAME), close_msg.size());
                char * frame = (char*) malloc(sizeof (char) * frame_len);
                frame_len = websocket_build_frame(frame, (websocket_flags) (WS_OP_CLOSE | WS_FINAL_FRAME), NULL, close_msg.c_str(), close_msg.size());
                response.assign(frame, frame_len);
                free(frame);
                keepalive = CLOSE_CONNECTION;

            } else if (a == '9') {
                size_t frame_len = websocket_calc_frame_size((websocket_flags) (WS_OP_PONG | WS_FINAL_FRAME), pong_msg.size());
                char * frame = (char*) malloc(sizeof (char) * frame_len);
                frame_len = websocket_build_frame(frame, (websocket_flags) (WS_OP_PONG | WS_FINAL_FRAME), NULL, pong_msg.c_str(), pong_msg.size());
                response.assign(frame, frame_len);
                free(frame);
            } else if (a == '0') {
                size_t frame_len = websocket_calc_frame_size((websocket_flags) (WS_OP_TEXT | WS_FINAL_FRAME), error_msg.size());
                char * frame = (char*) malloc(sizeof (char) * frame_len);
                frame_len = websocket_build_frame(frame, (websocket_flags) (WS_OP_TEXT | WS_FINAL_FRAME), NULL, error_msg.c_str(), error_msg.size());
                response.assign(frame, frame_len);
                free(frame);
                keepalive = CLOSE_CONNECTION;
                send_to_other = false;
            }
        }
ws_done:
        return std::make_pair(std::move(response), keepalive);
    }

    bool ws_server::ws_handshake(const std::string& request, std::string& response) {
        bool ret = false;
        std::istringstream stream(request.c_str());
        std::string reqType;
        std::getline(stream, reqType);
        if (reqType.substr(0, 4) != "GET ") {
            return ret;
        }

        std::string header;
        std::string::size_type pos = 0;
        std::string websocketKey;
        while (std::getline(stream, header) && header != "\r") {
            header.erase(header.end() - 1);
            pos = header.find(": ", 0);
            if (pos != std::string::npos) {
                std::string key = header.substr(0, pos);
                std::string value = header.substr(pos + 2);
                if (key == "Sec-WebSocket-Key") {
                    ret = true;
                    websocketKey = value;
                    break;
                }
            }
        }

        if (!ret) {
            return ret;
        }

        response = "HTTP/1.1 101 Switching Protocols\r\n";
        response += "Upgrade: websocket\r\n";
        response += "Connection: upgrade\r\n";
        response += "Sec-WebSocket-Accept: ";



        char output[29] = {};
        WebSocketHandshake::generate(websocketKey.c_str(), output);
        std::string serverKey = std::move(output);
        serverKey += "\r\n\r\n";
        response += serverKey;

        return ret;
    }


}
