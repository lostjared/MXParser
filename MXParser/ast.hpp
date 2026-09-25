#pragma once

#include <iostream>
#include <memory>
#include <print>
#include <string>

namespace mx {

    class ASTVisitor;

    class ASTNode {
      public:
        virtual ~ASTNode() = default;
        virtual void accept(ASTVisitor &visitor) = 0;
        virtual std::string toString() const = 0;
    };

} // namespace mx
