#include "MXParser/ast.hpp"
#include <sstream>

namespace mx {

    std::string NumberNode::to_string() const {
        std::ostringstream stream;
        stream << value.get_token();
        return stream.str();
    }

    std::string BinaryNode::to_string() const {
        std::ostringstream stream;
        stream << "(";
        stream << left->to_string();
        stream << op.get_token();
        stream << right->to_string();
        stream << ")";
        return stream.str();
    }

} // namespace mx
