#pragma once

#include <stdexcept>

class InvalidDLLException : public std::runtime_error {
public:
    explicit InvalidDLLException(const std::string& message)
        : std::runtime_error(message) {}
};

class ImageAllocationExcepetion : public std::runtime_error {
public:
    explicit ImageAllocationExcepetion(const std::string& message)
        : std::runtime_error(message) {}
};

class ImportedFunctionNotFoundException : public std::runtime_error {
public:
    explicit ImportedFunctionNotFoundException(const std::string& message)
        : std::runtime_error(message) {}
};