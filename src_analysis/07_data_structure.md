## 一些基本的数据结构

## list

/src/kernel/list.h

里面用了许多宏去简化操作

在demo中演示

这个用法很有意思，比如你要串起来一堆Task

那么你写task时, 把他写到成员变量就可了

```cpp
struct Task {
    struct list_head list;
    xxx;
    xxxx;
}
```

当添加的时候，只要把他这个成员函数串起来就可

```cpp
list_add_tail(&task->list, xxx);
```
