## 使用find_package引入外部依赖包

## example

```cpp
find_package(CURL)
add_executable(curltest curltest.cc)
if(CURL_FOUND)
    target_include_directories(clib PRIVATE ${CURL_INCLUDE_DIR})
    target_link_libraries(curltest ${CURL_LIBRARY})
else(CURL_FOUND)
    message(FATAL_ERROR ”CURL library not found”)
endif(CURL_FOUND)
```

<LibaryName>_FOUND

<LibaryName>_INCLUDE_DIR or <LibaryName>_INCLUDES

<LibaryName>_LIBRARY or <LibaryName>_LIBRARIES

## 两种模式

Module模式 / Config模式

### Module模式

Find<LibraryName>.cmake

cmake官方为我们预定义了许多寻找依赖包的Module

path_to_your_cmake/share/cmake-<version>/Modules

我的是 `/usr/share/cmake-3.16/Modules/`

cmake --help-module-list

https://cmake.org/cmake/help/latest/manual/cmake-modules.7.html

Find<LibaryName>.cmake

除了在上树module中，还可以我们指定CMAKE_MODULE_PATH

```
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/)
```

这个文件负责找到库所在的路径，为我们的项目引入头文件路径和库文件路径

如果Module模式搜索失败，没有找到对应的Find<LibraryName>.cmake文件，则转入Config模式进行搜索

### Config模式

通过<LibraryName>Config.cmake or <lower-case-package-name>-config.cmake这两个文件来引入我们需要的库

比如我的 `gtest` 在 `/usr/local/lib/cmake/GTest`

```
GTestConfig.cmake         GTestTargets.cmake
GTestConfigVersion.cmake  GTestTargets-noconfig.cmake
```

`/usr/local/lib/cmake/<LibraryName>/` 正是find_package函数的搜索路径之一

```
<prefix>/(lib/<arch>|lib*|share)/cmake/<name>*/                
<prefix>/(lib/<arch>|lib*|share)/<name>*/                      
<prefix>/(lib/<arch>|lib*|share)/<name>*/(cmake|CMake)/        
<prefix>/<name>*/(lib/<arch>|lib*|share)/cmake/<name>*/        
<prefix>/<name>*/(lib/<arch>|lib*|share)/<name>*/               
<prefix>/<name>*/(lib/<arch>|lib*|share)/<name>*/(cmake|CMake)/ 
```

详细参考文档

https://cmake.org/cmake/help/latest/command/find_package.html#config-mode-search-procedure

对于原生支持Cmake编译和安装的库通常会安装Config模式的配置文件到对应目录

这个配置文件直接配置了头文件库文件的路径以及各种cmake变量供find_package使用。

而对于非由cmake编译的项目，我们通常会编写一个Find<LibraryName>.cmake

比如 https://github.com/wfrest/wfrest/blob/main/cmake/FindBrotli.cmake

## ref

https://zhuanlan.zhihu.com/p/97369704

https://github.com/BrightXiaoHan/CMakeTutorial/tree/master/FindPackage