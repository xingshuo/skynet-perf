#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <string>
#include <map>
#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>


extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

struct skynet_context;
extern "C" void skynet_error(struct skynet_context* context, const char* msg, ...);

// Notice: same with 'snlua' in skynet/service-src/service_snlua.c
struct snlua {
    lua_State* L;
    struct skynet_context* ctx;
    size_t mem;
    size_t mem_report;
    size_t mem_limit;
    lua_State* activeL;
#if LUA_VERSION_NUM >= 504
    int trap; // ATOM_INT trap
#endif
};

static const uint8_t MAX_FUNC_NAME_SIZE = 80;
static const char* const IGNORE_NAME[] = {"?", "function 'xpcall'", "function 'pcall'"};
static const int VALID_MIN_ID = sizeof(IGNORE_NAME) / sizeof(const char*);

static const int PROF_HZ = 50; // hard code here
static const int MAX_STACK_SIZE = 64;
static const int MAX_BUCKET_SIZE = 1 << 12;

struct sample {
    int count;
    int depth;
    uint32_t stack[MAX_STACK_SIZE];
    struct sample* next;
};

class lperf_app {
public:
    lperf_app(const char* prof_file);
    ~lperf_app();

    uint32_t get_func_Id(std::string name);
    void add_record(uint32_t* stack, int depth);
    void save_records();
    void write_file(const char* buf, size_t len);

public:
    std::unordered_map<std::string, uint32_t> m_func_name2Id;
    std::unordered_map<uint32_t, std::string> m_func_Id2name;
    std::string m_filename;
    int m_fd;
    lua_State* m_gL;
    int m_records_num;
    int m_samples_num;
    struct sample* m_buckets[MAX_BUCKET_SIZE];
};