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

核心点:

`<LibaryName>_FOUND`

`<LibaryName>_INCLUDE_DIR or <LibaryName>_INCLUDES`

`<LibaryName>_LIBRARY or <LibaryName>_LIBRARIES`

find_package在一些目录中查找CURL的配置文件。

找到后，find_package会将头文件目录设置到 `${CURL_INCLUDE_DIR}`中，将链接库设置到`${CURL_LIBRARY}`中。

## 两种模式

Module模式 / Config模式

### Module模式

`Find<LibraryName>.cmake`

cmake官方为我们预定义了许多寻找依赖包的Module

`path_to_your_cmake/share/cmake-<version>/Modules`

我的是 `/usr/share/cmake-3.16/Modules/`

查看有哪些:

cmake --help-module-list

https://cmake.org/cmake/help/latest/manual/cmake-modules.7.html

最为重要的是找到 `Find<LibaryName>.cmake`

除了在上树module中，还可以我们指定CMAKE_MODULE_PATH

```
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/)
```

在这个路径下，我们自己写`Find<LibaryName>.cmake`, 去找非cmake的项目，如何写可以参考[自己写find.cmake](./find_cmake.md)

比如 https://github.com/wfrest/wfrest/blob/main/cmake/FindBrotli.cmake

这个文件负责找到库所在的路径，为我们的项目引入头文件路径和库文件路径

如果Module模式搜索失败，没有找到对应的`Find<LibraryName>.cmake`文件，则转入Config模式进行搜索

### Config模式

通过`<LibraryName>Config.cmake` or `<lower-case-package-name>-config.cmake`这两个文件来引入我们需要的库

比如我的 `gtest` 在 `/usr/local/lib/cmake/GTest`

```
GTestConfig.cmake         GTestTargets.cmake
GTestConfigVersion.cmake  GTestTargets-noconfig.cmake
```

`/usr/local/lib/cmake/<LibraryName>/` 正是find_package函数的搜索路径之一

搜索路径

```
<package>_DIR
CMAKE_PREFIX_PATH
CMAKE_FRAMEWORK_PATH
CMAKE_APPBUNDLE_PATH
PATH
```

详细参考文档

https://cmake.org/cmake/help/latest/command/find_package.html#config-mode-search-procedure

对于原生支持Cmake编译和安装的库通常会安装Config模式的配置文件到对应目录

这个配置文件直接配置了头文件库文件的路径以及各种cmake变量供find_package使用。

## 搜索路径再总结

比较粗略常规的搜索路径:

cmake默认采取Module模式，如果Module模式未找到库，才会采取Config模式。

如果`XXX_DIR`路径下找不到`XXXConfig.cmake`文件 (优先级高)

则会找`/usr/local/lib/cmake/XXX/`中的`XXXConfig.cmake`文件

其他更为细致的搜索路径和规则见文档。

## ref

https://zhuanlan.zhihu.com/p/97369704

https://github.com/BrightXiaoHan/CMakeTutorial/tree/master/FindPackage

https://zhuanlan.zhihu.com/p/50829542