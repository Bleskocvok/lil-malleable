#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <expected>
#include <fstream>
#include <functional>
#include <istream>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <ranges>
#include <utility>
#include <iostream>
#include <vector>
#include <filesystem>

using Id = std::string;

using FieldName = std::string;

using Fields = std::unordered_map<std::string, std::string>;

struct Result
{
    Id id;
    double score = 0;
};

using Results = std::vector<Result>;

using Words = std::vector<std::string>;

using Error = std::runtime_error;

struct Document
{
    Id id;
    Fields data;
};

inline std::ostream& log()
{
    return std::cerr;
}

struct Index
{
    using DocumentMap = std::unordered_map<Id, std::uint64_t>;

    std::unordered_map<std::string, DocumentMap> keyword_counts;

    std::unordered_map<Id, Words> documents;

    double avg_len = 0;

    void add_document(Id id, Words words)
    {
        if (documents.contains(id))
            return;

        avg_len = ( documents.size() * avg_len + words.size() )
                / ( documents.size() + 1 );

        for (const auto& w : words)
        {
            auto it = keyword_counts.find(w);
            if (it == keyword_counts.end())
            {
                auto [counts, ok] = keyword_counts.emplace(w, DocumentMap{});
                // Insert word count as one for this document.
                counts->second.emplace(id, 1);
            }
            else
            {
                auto& counts = it->second;
                auto count_it = counts.find(id);
                if (count_it == counts.end())
                    // Insert word count as one for this document.
                    counts.emplace(id, 1);
                else
                    count_it->second++;
            }
        }

        documents.insert_or_assign(std::move(id), std::move(words));
    }

    Results search(const Words& words) const
    {
        Results results;

        if (documents.empty())
            return results;

        struct WordInfo
        {
            const DocumentMap* counts = nullptr;
            double idf = 0;
        };

        auto word_maps = std::unordered_map<std::string, WordInfo>{};
        for (const auto& w : words)
        {
            if (word_maps.contains(w))
                continue;

            WordInfo info;

            auto it = keyword_counts.find(w);
            if (it != keyword_counts.end())
                info.counts = &it->second;

            double N = documents.size();
            double n_q = info.counts != nullptr
                       ? info.counts->size()
                       : 0;

            // log1p( x ) = log_e( 1 + x )
            info.idf = std::log1p(
                    ( N - n_q + 0.5 )
                    / ( n_q + 0.5 )
            );

            word_maps.emplace(w, info);
        }

        constexpr double k1 = 1.2;
        constexpr double b = 0.75;

        auto map = std::unordered_map<Id, double>{};

        for (const auto& w : words)
        {
            const auto& info = word_maps.at(w);

            if (info.counts == nullptr)
                continue;

            for (const auto& [id, f_doc] : *info.counts)
            {
                double& score = map[id];

                // TODO: Expensive, I'm sure.
                double D = documents.at(id).size();

                double formula = ( f_doc * ( k1 + 1 ) )
                                / ( f_doc + k1 * ( 1 - b + b * D / avg_len ) );
                score += info.idf * formula;
            }
        }

        for (const auto& [id, score] : map)
            results.push_back(Result{ .id = id, .score = score });

        return results;
    }
};

struct Tokenizer
{
    static bool forbidden(char c)
    {
        static std::array<bool, 256> map = []()
        {
            auto map = std::array<bool, 256>{ 0 };

            for (auto& b : map)
                b = false;

            auto forbid = [&](char c)
            {
                map[ static_cast<unsigned char>(c) ] = true;
            };

            forbid(',');
            forbid('\n');
            forbid('\t');
            forbid('?');
            forbid('!');

            return map;
        }();

        return map[ static_cast<unsigned char>(c) ];
    }

    Words operator()(std::string txt) const
    {
        auto lowercase = [](unsigned char c) -> char { return std::tolower(c); };

        std::ranges::transform(txt.begin(), txt.end(), txt.begin(), [](char c) -> char
        {
            if (forbidden(c))
                return ' ';
            return c;
        });

        std::ranges::transform(txt.begin(), txt.end(), txt.begin(), lowercase);

        return txt
            | std::views::split(' ')
            | std::views::transform([](auto&& r){ return std::string(r.begin(), r.end()); })
            | std::views::filter([](const std::string& s){ return not s.empty(); })
            | std::ranges::to<std::vector<std::string>>();
    }
};

struct Search
{
    Tokenizer tokenizer;
    Index index_;

    void index(Id id, std::string txt)
    {
        index_.add_document(id, tokenizer(std::move(txt)));
    }

    Results search(std::string q) const
    {
        auto words = tokenizer(std::move(q));
        constexpr double epsilon = 1e-6;
        auto res = index_.search(words)
                    | std::views::filter([](const Result& r){ return r.score >= epsilon; })
                    | std::ranges::to<std::vector>();

        std::ranges::sort(res, std::greater{}, [](const auto& x){ return x.score; });
        return res;
    }

    const Words& get(Id id) const
    {
        return index_.documents.at(id);
    }
};

struct Serializer
{
    void write(std::ostream& out, const Search& s)
    {
        for (const auto& [id, words] : s.index_.documents)
        {
            out << id << ",";

            bool fst = true;
            for (const auto& w : words)
            {
                out << (fst ? "" : " ") << w;
                fst = false;
            }
            out << "\n";
        }
    }

    void read(std::istream& in, Search& s)
    {
        std::string line;

        while (in.good())
        {
            std::getline(in, line);

            if (line.empty())
                continue;

            auto idx = line.find(',');
            if (idx == line.npos)
            {
                log() << "comma expected" << std::endl;
                throw Error("comma expected");
            }

            Id id = line.substr(0, idx);
            auto str = line.substr(idx + 1);

            // TODO: Copying navíc. Bylo by fajn mít line jako view a s.index
            // by ho akceptoval bez koerce.
            s.index(std::move(id), std::move(str));
        }
    }
};

using SearchName = std::string;

struct SearchManager
{
    std::map<SearchName, Search> searches;

    void create(SearchName n)
    {
        searches.emplace(std::move(n), Search{});
    }

    bool remove(const SearchName& n)
    {
        return searches.erase(n);
    }

    bool exists(const SearchName& n) const { return searches.contains(n); }

    const auto& get(const SearchName& n) const { return searches.at(n); }
          auto& get(const SearchName& n)       { return searches.at(n); }

    std::vector<SearchName> list() const
    {
        return searches
            | std::ranges::views::transform([](const auto& x){ return x.first; })
            | std::ranges::to<std::vector>();
    }

    void save(const SearchName& n)
    {
        Serializer ser;
        auto out = std::ofstream("data/" + n);
        ser.write(out, get(n));
    }

    void load(const SearchName& n)
    {
        if (not exists(n))
            create(n);

        Serializer ser;
        auto in = std::ifstream("data/" + n);
        ser.read(in, get(n));
    }

    void load_all()
    {
        namespace fs = std::filesystem;

        auto path = fs::path("data");

        for (const auto& entry : fs::directory_iterator(path))
        {
            if (not entry.is_regular_file())
                continue;

            auto index = entry.path().filename().string();
            log() << "Loaded index: " << index << "\n";
            load(index);
        }
    }
};
