#include "index.hpp"

#include "deps/httplib.h"
#include "deps/json.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

inline std::optional<std::string> maybe_getenv(const std::string& e)
{
    const auto* ptr = std::getenv(e.c_str());
    if (not ptr)
        return std::nullopt;

    return std::string(ptr);
}

inline std::string getenv_required(const std::string& e)
{
    return maybe_getenv(e)
        .or_else([&e]()
        {
            throw std::runtime_error("missing " + e);
            return maybe_getenv(e);
        })
        .value();
}

inline std::string getenv_default(const std::string& e, const std::string& def)
{
    return maybe_getenv(e).value_or(def);
}

inline int getenv_default_int(const std::string& e, int def)
{
    return maybe_getenv(e).transform([](const auto& x){ return std::stoi(x); }).value_or(def);
    // Why no workey?
    // return maybe_getenv(e).transform(std::bind_front(&std::stoi)).value_or(def);
    // return maybe_getenv(e).transform(&std::stoi).value_or(def);
}

void index(Search& s, json& j)
{
    auto num = j["id"].template get<int>();
    std::string id = j["_t"].get<std::string>() + "_" + std::to_string(num);
    s.index(id, j["name"]);
    std::cout << id << std::endl;
}

int main(int /*argc*/, char** /*argv*/)
{
    Search search;

    std::string host = getenv_default("HOST", "0.0.0.0");
    int port = getenv_default_int("PORT", 8080);

    std::cerr << "Listening on " << host << ":" << port << std::endl;

    httplib::Server server;
    server.Get("/health", [](const httplib::Request& req, httplib::Response& res)
    {
        auto q = req.get_param_value("q");
        res.set_content(R"({"status": "ok"})", "application/json");
    });

    server.Post("/index", [&search](const httplib::Request& req, httplib::Response& res)
    {
        json j = json::parse(req.body);

        auto id = j.at("_id").get<std::string>();
        auto txt = j.at("text").get<std::string>();

        // std::cerr << "index " << id << std::endl;

        search.index(std::move(id), std::move(txt));

        res.status = 200;
    });

    server.Get("/search", [&search](const httplib::Request& req, httplib::Response& res)
    {
        auto q = req.get_param_value("q");
        // TODO: Error handling.

        int limit = -1;
        if (req.has_param("limit"))
            limit = std::stoi( req.get_param_value("limit") );
        // TODO: Error handling.
        // TODO: Error handling.

        auto found = search.search(q);
        if (limit > 0 && found.size() > static_cast<unsigned>(limit))
            found.resize(limit);

        json results = json::array();
        for (const auto& f : found)
        {
            auto obj = json::object();
            obj["id"] = f.id;
            obj["score"] = f.score;
            results.push_back(std::move(obj));
        }

        json resp = json::object();
        resp["results"] = std::move(results);
        res.set_content(resp.dump(), "application/json");
    });

    server.Get("/count", [&search](const httplib::Request& req, httplib::Response& res)
    {
        json resp = json::object();
        resp["count"] = search.index_.documents.size();
        res.set_content(resp.dump(), "application/json");
    });

    server.listen(host, port);
}
