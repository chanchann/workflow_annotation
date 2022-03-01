# Json parser

## 类型

JSON 只包含 6 种数据类型

null: 表示为 null

boolean: 表示为 true 或 false

number: 一般的浮点数表示方式

string: 表示为 "..."

array: 表示为 [ ... ]

object: 表示为 { ... }

如果把 true 和 false 当作两个类型就是 7 种

所以我们在此定义

```c
#define JSON_VALUE_STRING	1
#define JSON_VALUE_NUMBER	2
#define JSON_VALUE_OBJECT	3
#define JSON_VALUE_ARRAY	4
#define JSON_VALUE_TRUE		5
#define JSON_VALUE_FALSE	6
#define JSON_VALUE_NULL		7
```

