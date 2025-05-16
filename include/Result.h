#pragma once
#ifndef RESULT_H
#define RESULT_H

template<typename V, typename E>
struct Result {
    bool success;
    V value;
    E error;
    static Result<V,E> Ok(const V& v) { return {true, v, E{}}; }
    static Result<V,E> Err(const E& e) { return {false, V{}, e}; }
};

#endif // RESULT_H