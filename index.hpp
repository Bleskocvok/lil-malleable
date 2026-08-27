#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <ranges>
#include <utility>
#include <vector>

using Id = std::string;

using FieldName = std::string;

using Fields = std::unordered_map<std::string, std::string>;

struct Result
{
    Id id;
    double score = 0;
};

using Results = std::vector<Result>;

using Error = std::string;

using Words = std::vector<std::string>;

struct Document
{
    Id id;
    Fields data;
};

// double calc_bm25()
// {
// }

struct Index
{
    using DocumentMap = std::unordered_map<Id, std::uint64_t>;

    std::unordered_map<std::string, DocumentMap> keyword_counts;

    std::unordered_map<Id, Words> documents;

    double avg_len = 0;

    void add_document(Id id, Words words)
    {
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

    Results search(const Words& words)
    {
        Results results;
        results.reserve(documents.size());

        if (documents.empty())
            return results;

        struct WordInfo
        {
            DocumentMap* counts = nullptr;
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

            info.idf = std::log1p(
                    ( N - n_q + 0.5 )
                    / ( n_q + 0.5 )
            );

            word_maps.emplace(w, info);
        }

        constexpr double k1 = 1.2;
        constexpr double b = 0.75;

        for (const auto& [id, doc] : documents)
        {
            auto& res = results.emplace_back();

            res.id = id;
            double& score = res.score;

            for (const auto& w : words)
            {
                const auto& info = word_maps.at(w);

                double f_doc = 0;
                if (info.counts != nullptr)
                {
                    if (auto it = info.counts->find(id); it != info.counts->end())
                    {
                        f_doc = it->second;
                    }
                }

                double D = doc.size();
                double formula = ( f_doc * ( k1 + 1 ) )
                                / ( f_doc + k1 * ( 1 - b + b * D / avg_len ) );
                score += info.idf * formula;
            }
        }

        return results;
    }
};

// struct IndexManager
// {
//     std::unordered_map<Id, Document> documents;

//     void add_document(const Document& doc)
//     {
//
//     }

//     std::expected<Results, Error> search(const FieldName& fname,
//                                          const std::string& query)
//     {
//         return std::unexpected("error");
//     }
// };

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

    Words operator()(std::string txt)
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

    Results search(std::string q)
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
