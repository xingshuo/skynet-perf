#include "lperf.h"

//=============copy && modify lua5.4 source code begin=============
/*
** Search for 'objidx' in table at index -1. ('objidx' must be an
** absolute index.) Return 1 + string at top if it found a good name.
*/
static int findfield (lua_State *L, int objidx, int level) {
  if (level == 0 || !lua_istable(L, -1))
    return 0;  /* not found */
  lua_pushnil(L);  /* start 'next' loop */
  while (lua_next(L, -2)) {  /* for each pair in table */
    if (lua_type(L, -2) == LUA_TSTRING) {  /* ignore non-string keys */
      if (lua_rawequal(L, objidx, -1)) {  /* found object? */
        lua_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        lua_pushliteral(L, ".");  /* place '.' between the two names */
        lua_replace(L, -3);  /* (in the slot occupied by table) */
        lua_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    lua_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}

/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (lua_State *L, lua_Debug *ar) {
  int top = lua_gettop(L);
  lua_getinfo(L, "f", ar);  /* push function */
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
  if (findfield(L, top + 1, 2)) {
    const char *name = lua_tostring(L, -1);
    if (strncmp(name, "_G.", 3) == 0) {  /* name start with '_G.'? */
      lua_pushstring(L, name + 3);  /* push name without prefix */
      lua_remove(L, -2);  /* remove original name */
    }
    lua_copy(L, -1, top + 1);  /* copy name to proper place */
    lua_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    lua_settop(L, top);  /* remove function and global table */
    return 0;
  }
}

// Notice: add utility method here
static const char*
strsuffix(const char* s, char delim) {
    const char* p = s;
    const char* q = s;
    while (*p != '\0') {
        if (*p == delim) {
            q = p + 1;
        }
        p++;
    }

    return q;
}

// Notice: modify source code here
static void pushfuncname(lua_State *L, lua_Debug *ar) {
    if (pushglobalfuncname(L, ar)) {  /* try first a global name */
        lua_pushfstring(L, "function '%s'", lua_tostring(L, -1));
        lua_remove(L, -2);  /* remove name */
    } else if (*ar->namewhat != '\0')  /* is there a name from code? */
        lua_pushfstring(L, "%s '%s(%d):%s'", ar->namewhat, strsuffix(ar->short_src, '/'), ar->linedefined, ar->name);  /* use it */
    else if (*ar->what == 'm')  /* main? */
        lua_pushliteral(L, "main chunk");
    else if (*ar->what != 'C')  /* for Lua functions, use <file:line> */
        lua_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
    else  /* nothing left... */
        lua_pushliteral(L, "?");
}

static int lastlevel (lua_State *L) {
  lua_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (lua_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (lua_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}
//=============copy && modify lua5.4 source code end=============


static lperf_app* g_app = NULL;

lperf_app::lperf_app(const char* prof_file) {
    m_filename = prof_file;
    m_fd = -1;
    int fd = open(prof_file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd > 0) {
        m_fd = fd;
    }

    for (int i = 0; i < MAX_BUCKET_SIZE; i++) {
        m_buckets[i] = NULL;
    }
    for (int i = 0; i < VALID_MIN_ID; i++) {
        m_func_Id2name[i] = IGNORE_NAME[i];
        m_func_name2Id[IGNORE_NAME[i]] = i;
    }
    
    m_records_num = 0;
    m_samples_num = 0;
}

lperf_app::~lperf_app() {
    if (m_fd > 0) {
        close(m_fd);
    }

    for (int i = 0; i < MAX_BUCKET_SIZE; i++) {
        struct sample* pcurr = m_buckets[i];
        while (pcurr) {
            void* tmp = pcurr;
            pcurr = pcurr->next;
            free(tmp);
        }
    }
}

inline uint32_t lperf_app::get_func_Id(std::string name) {
    uint32_t Id;
    auto iter = m_func_name2Id.find(name);
    if (iter == m_func_name2Id.end()) {
        Id = m_func_name2Id.size();
        m_func_name2Id[name] = Id;
        m_func_Id2name[Id] = name;
    } else {
        Id = iter->second;
    }
    return Id;
}

void lperf_app::add_record(uint32_t* stack, int depth) {
    if (depth <= 0) {
        return;
    }
    m_records_num++;

    uint32_t hash = 0;
    for (int i = 0; i < depth; i++) {
        uint32_t funcId = stack[i];
        hash += (funcId << i);
    }

    int slot = hash % MAX_BUCKET_SIZE;
    struct sample* pcurr = m_buckets[slot];
    while (pcurr) {
        if (pcurr->depth == depth && memcmp(pcurr->stack, stack, 4*depth) == 0) {
            pcurr->count++;
            return;
        }
        pcurr = pcurr->next;
    }
    m_samples_num++;

    pcurr = (struct sample*)malloc(sizeof(struct sample));
    pcurr->count = 1;
    pcurr->depth = depth;
    memcpy(pcurr->stack, stack, 4*depth);

    pcurr->next = m_buckets[slot];
    m_buckets[slot] = pcurr;
}

void lperf_app::write_file(const char* buf, size_t len) {
    while (len > 0) {
        ssize_t r = write(m_fd, buf, len);
        buf += r;
        len -= r;
    }
}

static inline void
put_bigendian_u32(uint8_t* buf, uint32_t v) {
    buf[0] = (uint8_t)(v >> 24);
    buf[1] = (uint8_t)(v >> 16);
    buf[2] = (uint8_t)(v >> 8);
    buf[3] = (uint8_t)v;
}

void lperf_app::save_records() {
    if (m_records_num == 0) {
        skynet_error(NULL, "lperf: no save records");
        return;
    }
    // calc func info len
    int func_num = 0;
    int func_info_len = 4; // total func num
    for (auto iter = m_func_name2Id.begin(); iter != m_func_name2Id.end(); iter++) {
        uint32_t Id = iter->second;
        if (Id < VALID_MIN_ID) {
            continue;
        }
        func_num++;
        const std::string& name = iter->first;
        int name_len = name.length();
        name_len = (name_len > MAX_FUNC_NAME_SIZE) ? MAX_FUNC_NAME_SIZE : name_len;
        func_info_len += (name_len + 1); //func name
        func_info_len += 4; //func Id
    }
    // calc samples data len
    int samples_len = 0;
    for (int i = 0; i < MAX_BUCKET_SIZE; i++) {
        struct sample* pcurr = m_buckets[i];
        while (pcurr) {
            samples_len += (8 + pcurr->depth*4); // count + depth + stack[:depth]
            pcurr = pcurr->next;
        }
    }

    int total_len = 4 + func_info_len + samples_len;
    uint8_t* buf = (uint8_t*)malloc(total_len);
    // write profile data len
    put_bigendian_u32(buf, func_info_len + samples_len);
    // write func info
    int offset = 4;
    put_bigendian_u32(buf+offset, func_num);
    offset += 4;
    for (auto iter = m_func_name2Id.begin(); iter != m_func_name2Id.end(); iter++) {
        uint32_t Id = iter->second;
        if (Id < VALID_MIN_ID) {
            continue;
        }
        const std::string& name = iter->first;
        int name_len = name.length();
        name_len = (name_len > MAX_FUNC_NAME_SIZE) ? MAX_FUNC_NAME_SIZE : name_len;
        buf[offset] = name_len;
        offset += 1;
        memcpy(buf+offset, name.c_str(), name_len);
        offset += name_len;

        put_bigendian_u32(buf+offset, Id);
        offset += 4;
    }
    // write samples data
    for (int i = 0; i < MAX_BUCKET_SIZE; i++) {
        struct sample* pcurr = m_buckets[i];
        while (pcurr) {
            put_bigendian_u32(buf+offset, pcurr->count);
            offset += 4;
            put_bigendian_u32(buf+offset, pcurr->depth);
            offset += 4;
            for (int j = 0; j < pcurr->depth; j++) {
                put_bigendian_u32(buf+offset, pcurr->stack[j]);
                offset += 4;
            }

            pcurr = pcurr->next;
        }
    }

    write_file((const char*)buf, total_len);
    skynet_error(NULL, "lperf: save %d records, %d samples, %d nodes to %s", m_records_num, m_samples_num, func_num, m_filename.c_str());
}


static void
signal_hook(lua_State *L, lua_Debug *par) {
    lua_sethook(L, NULL, 0, 0);
    if (!g_app) { // stopped
        skynet_error(NULL, "lperf: touch hook without app");
        return;
    }

    lua_Debug ar;
    int last = lastlevel(L);
    uint32_t stack[MAX_STACK_SIZE];
    int depth = 0;
    while (lua_getstack(L, last, &ar) && depth < MAX_STACK_SIZE) {
        lua_getinfo(L, "Slnt", &ar);
        pushfuncname(L, &ar);
        const char *funcname = lua_tostring(L, -1);
        lua_pop(L, 1);
        last--;
        uint32_t Id = g_app->get_func_Id(funcname);
        if (Id < VALID_MIN_ID) {
            continue;
        }
        stack[depth] = Id;
        depth++;
    }
    g_app->add_record(stack, depth);
}

static void
signal_handler(int sig) {
    if (!g_app) {
        skynet_error(NULL, "lperf: touch SIGPROF without app");
        return;
    }
    void *ud = NULL;
    lua_getallocf(g_app->m_gL, &ud);
    struct snlua *l = (struct snlua *)ud;
    lua_State *L = l->activeL;
    if (L) {
        lua_sethook(L, signal_hook, LUA_MASKCOUNT, 1);
    }
}

static int
lstart(lua_State *L) {
    if (g_app) {
        return luaL_error(L, "lperf: start more than once");
    }
    const char* prof_file = luaL_checkstring(L, 1);
    if (prof_file == NULL) {
        return luaL_error(L, "lperf: start without prof file");
    }
    g_app = new lperf_app(prof_file);
    if (g_app->m_fd == -1) {
        delete g_app;
        g_app = NULL;
        return luaL_error(L, "lperf: open prof file failed, %s", prof_file);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
    g_app->m_gL = lua_tothread(L,-1);
    lua_pop(L, 1);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    if (sigfillset(&sa.sa_mask)) {
        return luaL_error(L, "lperf: sigfillset failed");
    }
    sa.sa_handler = signal_handler;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGPROF, &sa, NULL)) {
        return luaL_error(L, "lperf: register SIGPROF failed");
    }

    struct itimerval timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 1000/PROF_HZ * 1000;
    timer.it_value = timer.it_interval;
    int ret = setitimer(ITIMER_PROF, &timer, NULL);
    if (ret != 0) {
        return luaL_error(L, "lperf: start timer failed, %s", strerror(errno));
    }
    skynet_error(NULL, "lperf: started.");

    return 0;
}

static int
lstop(lua_State *L) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    if (sigaction(SIGPROF, &sa, NULL)) {
        skynet_error(NULL, "lperf: unregister SIGPROF failed");
    }

    struct itimerval timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    timer.it_value = timer.it_interval;
    int ret = setitimer(ITIMER_PROF, &timer, NULL);
    if (ret != 0) {
        skynet_error(NULL, "lperf: stop timer fail, %s", strerror(errno));
    }

    if (g_app) {
        g_app->save_records();
        delete g_app;
        g_app = NULL;
    } else {
        skynet_error(NULL, "lperf: stop without app");
    }

    skynet_error(NULL, "lperf: stopped.");

    return 0;
}


extern "C" int
luaopen_lperf(lua_State *L) {
    luaL_checkversion(L);
    luaL_Reg l[] = {
            {"start", lstart},
            {"stop",  lstop},
            {NULL, NULL},
    };
    luaL_newlib(L, l);
    return 1;
}