local Skynet = require "skynet"
local LPerf = require 'lperf'

function calc21(n)
    local s = 0
    for i = 1, 1000 do
        local j = i*n
        s = s + j
    end
    return s
end

function calc22(n)
    local s = 0
    for i = 1, 2000 do
        local j = i*n
        s = s + j
    end
    return s
end

function calc23(n)
    local s = 0
    for i = 1, 3000 do
        local j = i*n
        s = s + j
    end
    return s
end

local func_map = {
    [1] = calc21,
    [2] = calc22,
    [3] = calc23,
}

function calc()
    local s = 0
    for i = 1, 500 do
        s = s + calc1(i)
    end
    return s
end

function calc1(n)
    local s = 0
    for i = 1, 1000 do
        local k = (i % 3) + 1
        local f = func_map[k]
        local j = i*n
        s = s + f(j)
    end
    return s
end

function init()
    LPerf.start("test_calc.pro")
    calc()
    LPerf.stop()
    Skynet.exit()
end

Skynet.start(init)