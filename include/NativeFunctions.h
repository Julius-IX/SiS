#pragma once

#include <Value.h>
#include <iostream>

namespace eval {
  // This is the single place to register a native functions
  // This is its own file to keep shrink Evaluator.cpp smaller and less unreadable
  // WARN: You can only use this once due to it moving values
  static const std::unordered_map<std::string, Value> native_functions = {

    std::make_pair("print",
                   Value(NativeFunction{
                     .name = "print",
                     .fn = [](std::vector<Value>& args) -> Value {
                       for (size_t i = 0; i < args.size(); ++i) {
                         fmt::print("{}", args[i].toString());
                         if (i + 1 < args.size()) fmt::print(" ");
                       }
                       fmt::print("\n");
                       return Value{};
                     },
                   })),

    std::make_pair("len",
                   Value(NativeFunction{
                     .name = "len",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() != 1) {
                         throw std::runtime_error("len() expects exactly 1 argument, got " + std::to_string(args.size()));
                       }
                       if (const auto* arr = std::get_if<Array>(&args[0].data)) {
                         return {static_cast<double>((*arr)->size())};
                       }
                       if (const auto* str = std::get_if<std::string>(&args[0].data)) {
                         return {static_cast<double>(str->size())};
                       }
                       throw std::runtime_error("len() expects an array or string, got " + args[0].typeName());
                     },
                   })),

    std::make_pair("type",
                   Value(NativeFunction{
                     .name = "type",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() != 1) {
                         throw std::runtime_error("type() expects exactly 1 argument, got " + std::to_string(args.size()));
                       }
                       return {args[0].typeName()};
                     },
                   })),

    std::make_pair("str",
                   Value(NativeFunction{
                     .name = "str",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() != 1) {
                         throw std::runtime_error("str() expects exactly 1 argument, got " + std::to_string(args.size()));
                       }
                       return {args[0].toString()};
                     },
                   })),

    std::make_pair("num",
                   Value(NativeFunction{
                     .name = "num",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() != 1) {
                         throw std::runtime_error("num() expects exactly 1 argument, got " + std::to_string(args.size()));
                       }
                       if (const auto* d = std::get_if<double>(&args[0].data)) {
                         return {*d};
                       }
                       if (const auto* s = std::get_if<std::string>(&args[0].data)) {
                         try {
                           return {std::stod(*s)};
                         } catch (const std::exception&) {
                           throw std::runtime_error("num(): could not convert string '" + *s + "' to a number");
                         }
                       }
                       if (const auto* b = std::get_if<bool>(&args[0].data)) {
                         return {*b ? 1.0 : 0.0};
                       }
                       throw std::runtime_error("num() cannot convert a value of type " + args[0].typeName());
                     },
                   })),

    std::make_pair("push",
                   Value(NativeFunction{
                     .name = "push",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() != 2) {
                         throw std::runtime_error("push() expects exactly 2 arguments (array, value), got " + std::to_string(args.size()));
                       }
                       const auto* arr = std::get_if<Array>(&args[0].data);
                       if (!arr || !*arr) {
                         throw std::runtime_error("push() expects an array as its first argument, got " + args[0].typeName());
                       }
                       Value size = (double)(*arr)->elements.size();
                       (*arr)->elements.emplace_back(size, args[1]);
                       return args[0];
                     },
                   })),

    std::make_pair("pop",
                   Value(NativeFunction{
                     .name = "pop",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() != 1) {
                         throw std::runtime_error("pop() expects exactly 1 argument, got " + std::to_string(args.size()));
                       }
                       const auto* arr = std::get_if<Array>(&args[0].data);
                       if (!arr || !*arr || (*arr)->elements.empty()) {
                         throw std::runtime_error("pop() expects a non-empty array");
                       }
                       Value back = (*arr)->elements.back().second;
                       (*arr)->elements.pop_back();
                       return back;
                     },
                   })),

    std::make_pair("read",
                   Value(NativeFunction{
                     .name = "read",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() > 1) {
                         throw std::runtime_error("read() expects at most 1 argument, got " + std::to_string(args.size()));
                       }
                       if (args.size() == 1) {
                         fmt::print("{}", args[0].toString());
                       }
                       std::string input;
                       std::getline(std::cin, input);
                       return Value{input};
                     },
                   })),
    std::make_pair("isNull",
                   Value(NativeFunction{
                     .name = "isNull",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() != 1) throw std::runtime_error("isNull() expects exactly 1 argument, got " + std::to_string(args.size()));
                       return {args[0].typeName() == "null"};
                     },
                   })),

    std::make_pair("isBool",
                   Value(NativeFunction{
                     .name = "isBool",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() != 1) throw std::runtime_error("isBool() expects exactly 1 argument, got " + std::to_string(args.size()));
                       return {args[0].typeName() == "bool"};
                     },
                   })),

    std::make_pair("isNum",
                   Value(NativeFunction{
                     .name = "isNum",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() != 1) throw std::runtime_error("isNum() expects exactly 1 argument, got " + std::to_string(args.size()));
                       return {args[0].typeName() == "num"};
                     },
                   })),

    std::make_pair("isString",
                   Value(NativeFunction{
                     .name = "isString",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() != 1) throw std::runtime_error("isString() expects exactly 1 argument, got " + std::to_string(args.size()));
                       return {args[0].typeName() == "string"};
                     },
                   })),

    std::make_pair("isArray",
                   Value(NativeFunction{
                     .name = "isArray",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() != 1) throw std::runtime_error("isArray() expects exactly 1 argument, got " + std::to_string(args.size()));
                       return {args[0].typeName() == "array"};
                     },
                   })),

    std::make_pair("isFunction",
                   Value(NativeFunction{
                     .name = "isFunction",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() != 1) throw std::runtime_error("isFunction() expects exactly 1 argument, got " + std::to_string(args.size()));
                       return {args[0].typeName() == "function"};
                     },
                   })),

    std::make_pair("isClass",
                   Value(NativeFunction{
                     .name = "isClass",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() != 1) throw std::runtime_error("isClass() expects exactly 1 argument, got " + std::to_string(args.size()));
                       return {args[0].typeName() == "class"};
                     },
                   })),

    std::make_pair("isInstance",
                   Value(NativeFunction{
                     .name = "isInstance",
                     .fn = [](std::vector<Value>& args) -> Value {
                       if (args.size() != 1) throw std::runtime_error("isInstance() expects exactly 1 argument, got " + std::to_string(args.size()));
                       return {args[0].typeName() == "instance"};
                     },
                   })),
  };
} // namespace eval
