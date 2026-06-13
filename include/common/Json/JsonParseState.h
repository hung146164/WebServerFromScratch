/*!
    \file JsonParser.h
    \brief JsonParserState defination
    \author HungForre
    \date 12/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_COMMON_JSONPARSERSTATE_H
#define CPPSERVER_COMMON_JSONPARSERSTATE_H

enum class JsonParseState
{
    START_STATE,
    OBJECT_STATE,
    ARRAY_STATE,
    STRING_STATE,
};

#endif