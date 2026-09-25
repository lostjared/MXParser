#pragma once

#include<memory>
#include<string>
#include<iostream>
#include<print>


namespace mx {

    class ASTVisitor;

    class ASTNode {
      public:
        virtual ~ASTNode() = default;
        virtual void accept(ASTVisitor &visitor) = 0;
        virtual std::string toString() const = 0;
    };


}


