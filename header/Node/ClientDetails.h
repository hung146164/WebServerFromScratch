#pragma once

template <typename T>
struct ClientDetail
{
    char buffer[8192];
    T *detail;
};