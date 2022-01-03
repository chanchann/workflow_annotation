## install

将项目生成的库文件、头文件、可执行文件或相关文件等安装到指定位置（系统目录，或发行包目录）

cmake中，这主要是通过install方法在CMakeLists.txt中配置，make install命令安装相关文件来实现的。

```
install(TARGETS MyLib
    EXPORT MyLibTargets 
    LIBRARY DESTINATION lib  # 动态库安装路径，可选
    ARCHIVE DESTINATION lib  # 静态库安装路径，可选
    RUNTIME DESTINATION bin  # 可执行文件安装路径，可选
    PUBLIC_HEADER DESTINATION include  # 头文件安装路径，可选
)
```

`DESTINATION` 路径可以set，根目录默认为CMAKE_INSTALL_PREFIX, 默认值为 `/usr/local`

安装完成后，希望可以通过find_package方法进行引用，我们拿workflow的cmake做例子来看看

## for find_package

1. 需要生成一个XXXConfigVersion.cmake, 声明版本信息

```cpp
# 写入库的版本信息
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    XXXConfigVersion.cmake
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY AnyNewerVersion  # 表示该函数库向下兼
)
```

2. 将前面EXPORT MyLibTargets的信息写入到MyLibTargets.cmake文件中

```
install(EXPORT MyLibTargets
    FILE MyLibTargets.cmake
    NAMESPACE XXX::
    DESTINATION lib/cmake/MyLib
```
)



## ref

https://zhuanlan.zhihu.com/p/102955723

https://github.com/BrightXiaoHan/CMakeTutorial

https://cmake.org/cmake/help/latest/command/install.html