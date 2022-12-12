#include <iostream>
#include <uwebsockets/App.h>
#include <nlohmann/json.hpp>

void process_public_msg(const auto data, auto* ws)
{
    int user_id = ws->getUserData()->user_id;

    nlohmann::json payload  = {
        {"command",data["command"]},
        {"text", data["text"]},
        {"user_from", user_id}
    };

    ws->publish("public", payload.dump());
    std::cout << "[User "<<user_id<<"] sent public message\n";
}

void process_private_msg(const auto data, auto* ws)
{
    int user_id = ws->getUserData()->user_id;
    int user_to = data["user_to"];

    nlohmann::json payload = {
        {"command",data["command"]},
        {"text", data["text"]},
        {"user_from", user_id}
    };

    ws->publish("user"+std::to_string(user_to), payload.dump());
    std::cout << "[User " <<user_id<<"] sent private message\n";
}

void process_set_name(const auto data, auto* ws)
{
    int user_id = ws->getUserData()->user_id;
    ws->getUserData()->name = data["name"];
    std::cout << "[User "<<user_id<< "] set his name\n";
}

std::string process_status(const auto data, bool online)
{
    nlohmann::json payload = {
        {"command", "status"},
        {"user_id", data->user_id},
        {"name", data->name},
        {"online", online}
    };
    return payload.dump();
}

struct UserData {
    int user_id;
    std::string name;
};

std::map<int, UserData*> online_users;

int main() {  

    int latest_user_id = 10;

    uWS::App app = uWS::App().ws<UserData>("/*", {
            .idleTimeout = 300,
            /* New connection */
            .open = [&latest_user_id](auto* ws) {
               auto* data = ws->getUserData();
               data->user_id = latest_user_id++;
               data->name = "noname";

               std::cout << "New user conneected: " << data->user_id << std::endl;
               ws->subscribe("public"); // default public subscribe
               ws->subscribe("user" + std::to_string(data->user_id)); //every user subscribed to its own channel
               ws->publish("public", process_status(data, true)); //user id/ name/ status(online/ offline)

               for (auto& entry : online_users)
               {
                   ws->send(process_status(entry.second,true), uWS::OpCode::TEXT);
               }

               online_users[data->user_id] = data;
            },
            // New data from client
            .message = [](auto* ws, std::string_view data, uWS::OpCode opCode) {
                nlohmann::json parsed_data = nlohmann::json::parse(data);// add later: format check, handle exeption(try/catch)

                if (parsed_data["command"] == "public_msg")
                {
                    process_public_msg(parsed_data, ws);
                }

                if (parsed_data["command"] == "private_msg")
                {
                    process_private_msg(parsed_data, ws);
                }

                if (parsed_data["command"] == "set_name")
                {
                    process_set_name(parsed_data, ws);
                    auto* data = ws->getUserData();
                    ws->publish("public", process_status(data, true));
                }
            },
            .close = [&app](auto* ws, int /*code*/, std::string_view /*message*/) {
                auto* data = ws->getUserData();
                online_users.erase(data->user_id);
                app.publish("public", process_status(data,false), uWS::OpCode::TEXT);

            }
            }).listen(9001, [](auto* listen_socket) {
                if (listen_socket) {
                    std::cout << "Listening on port " << 9001 << std::endl;
                }
                }).run();
}
