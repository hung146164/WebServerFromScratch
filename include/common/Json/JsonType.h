/*!
    \file JsonType.h
    \brief JsonType
    \author HungForre
    \date 12/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_COMMON_JSONTYPE_H
#define CPPSERVER_COMMON_JSONTYPE_H

namespace Json
{
    enum class JsonType : char
    {
        NUL,
        OBJECT,
        ARRAY,
        STRING,
        NUMBER,
        BOOL
    };
}

#endif
