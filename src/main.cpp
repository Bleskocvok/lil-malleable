#include "index.hpp"

#include <iostream>
#include <sstream>

int main()
{
    Search search;

    search.index("0", "ahoj ahoj");
    search.index("1", "ahoj svete");
    search.index("2", "svete svete");
    search.index("3", "jiri");
    search.index("4", "svete");
    search.index("5", "stepan");

    auto res = search.search("ahoj svete");

    for (const auto& r : res)
    {
        std::ostringstream o;
        for (const auto& w : search.get(r.id))
            o << w << " ";

        std::cout << r.id << " " << r.score << " " << o.str() << std::endl;
    }

    // for (const auto& w : tokenizer("ahoj, svete, jak se mas?"))
    // {
    //     std::cout << w << std::endl;
    // }

    return 0;
}
