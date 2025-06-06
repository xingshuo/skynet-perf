# skynet-perf
skynet snlua服务性能分析工具
* What is skynet? https://github.com/cloudwu/skynet

## 特性
* 易使用，接口简单
* 轻量，可无缝对接lua5.4版本的skynet（lua5.3版本也只需修改少量代码适配，即可接入）
* 直观，可生成函数间调用关系视图及热点函数性能开销占比信息

## 注意事项
* 一个skynet进程，同一时间只能对一个服务进行性能分析采样
* 采样需要skynet源码snlua结构体包含activeL字段，因此其仓库git commit ID:eaa60ca8（包含）之后的版本可无缝接入
* 针对eaa60ca8之前版本skynet的适配方案：git apply [snlua53_set_activeL.diff](https://github.com/xingshuo/skynet-perf/blob/main/snlua53_set_activeL.diff#L1)
* 采样频率固定为50Hz，可通过PROF_HZ常量自行调整
* 编译lperf.so时，若未指定INCLUDE_DIR，则默认使用include/下的头文件进行编译

## 编译 && 运行示例
* 将编译后的skynet仓库连接到工程目录下
```bash
# ln -sf $YOUR_SKYNET_PATH skynet
```
* 编译lperf.so
```bash
# make INCLUDE_DIR=skynet/3rd/lua
```
* 编译解析器pprof
```bash
# go get github.com/goccy/go-graphviz
# go build pprof.go
```
* 运行测试用例
```bash
# ./skynet/skynet test/calcsvr_config
```

## 分析示例采样结果
* 以文本形式查看所有热点函数（叶节点）性能开销占比（降序）
```bash
# ./pprof -i test_calc.pro -text
```
输出示例：
```
1th	    51%	    function 'calc23'
2th	    30%	    function 'calc22'
3th	    17%	    function 'calc21'
```
* 生成函数间调用关系及调用开销占比信息的png/svg图片
```bash
# ./pprof -i test_calc.pro -png test_calc.png
# ./pprof -i test_calc.pro -svg test_calc.svg
```
输出示例：
![image](test/test_calc.png)
* 生成采样文件的源信息明细（通常用于调试）
```bash
# ./pprof -i test_calc.pro -info test_calc.txt
```
输出示例：
```bash
# cat test_calc.txt
FuncId	:	FuncName
3	:	function <./skynet/lualib/skynet.lua:278>
4	:	upvalue 'skynet.lua(1026):f'
5	:	function 'skynet.init_service'
6	:	function <./skynet/lualib/skynet.lua:1010>
7	:	function 'init'
8	:	function 'calc'
9	:	function 'calc1'
10	:	function 'calc23'
11	:	function 'calc22'
12	:	function 'calc21'
--------------------------------------------------------------------
Count	:	Backtrace
270	:	3 -> 4 -> 5 -> 6 -> 7 -> 8 -> 9 -> 10
159	:	3 -> 4 -> 5 -> 6 -> 7 -> 8 -> 9 -> 11
91	:	3 -> 4 -> 5 -> 6 -> 7 -> 8 -> 9 -> 12
```

## 参考
* https://github.com/esrrhs/pLua
* https://github.com/google/pprof

## 其他扩展库
* https://github.com/xingshuo/skynet-ext
* https://github.com/xingshuo/skynet-mprof
* https://github.com/xingshuo/skynet-observer