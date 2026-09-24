#include "itch/metrics.hpp"
#include <iomanip>
#include <sstream>
namespace itch {
std::string json_string(const std::string &s) {
    std::ostringstream o;
    o << '"';
    for (char raw : s) {
        auto c = static_cast<unsigned char>(raw);
        if (c == '"' || c == '\\')
            o << '\\' << c;
        else if (c < 32 || c >= 127)
            o << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<unsigned>(c) << std::dec;
        else
            o << c;
    }
    o << '"';
    return o.str();
}
void Metrics::write_json(std::ostream &o, const std::string &input, const std::string &error,
                         std::size_t active) const {
    o << std::setprecision(12) << "{\n  \"input\": " << json_string(input)
      << ",\n  \"complete\": " << (error.empty() ? "true" : "false")
      << ",\n  \"error\": " << json_string(error) << ",\n  \"messages\": " << messages
      << ",\n  \"bytes\": " << bytes << ",\n  \"unsupported\": " << unsupported
      << ",\n  \"filtered\": " << filtered << ",\n  \"errors\": " << errors
      << ",\n  \"active_orders\": " << active << ",\n  \"elapsed_seconds\": " << elapsed_seconds
      << ",\n  \"messages_per_second\": "
      << (elapsed_seconds > 0 ? static_cast<double>(messages) / elapsed_seconds : 0)
      << ",\n  \"by_type\": {";
    bool first = true;
    for (auto [t, n] : by_type) {
        if (!first)
            o << ',';
        first = false;
        o << json_string(std::string(1, t)) << ':' << n;
    }
    o << "}\n}\n";
}
} // namespace itch
