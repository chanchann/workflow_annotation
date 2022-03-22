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

## 需求

我们要实现的 JSON 库，主要是完成 3 个需求：

1. 把 JSON 文本解析为一个树状数据结构（parse）。

2. 提供接口访问该数据结构（access）。

3. 把数据结构转换成 JSON 文本（stringify）。

## parse接口

我们先看接口

```c
/* 解析json文档产生json value。返回json value对象。返回NULL代表解析失败（格式不标准，嵌套过深，分配内存失败）
   @doc 文档字符串 */
json_value_t *json_value_parse(const char *doc);
```

首先，我们传入的是json字符串。

返回的是`json_value_t`

```c
typedef struct __json_value json_value_t;

struct __json_value
{
	int type;
	union
	{
		char *string;
		double number;
		json_object_t object;
		json_array_t array;
	} value;
};
```

可以看出，他主要有两个变量，一个是type，一个是他的值，这里用了union，不同类型的值有不同形式的value

从最上面的类型我们可以看到，这里有string, number(一般用double来表示)，array和object，还有null和boolean我们用type表示即可，不需要value。

## json_value_parse 实现

```c
json_value_t *json_value_parse(const char *doc)
{
	json_value_t *val;
	int ret;

	val = (json_value_t *)malloc(sizeof (json_value_t));
	if (!val)
		return NULL;

	while (isspace(*doc))
		doc++;

	ret = __parse_json_value(doc, &doc, 0, val);
	if (ret >= 0)
	{
		while (isspace(*doc))
			doc++;

		if (*doc)
		{
			__destroy_json_value(val);
			ret = -2;
		}
	}

	if (ret < 0)
	{
		free(val);
		return NULL;
	}

	return val;
}
```

根据[rfc7159](https://datatracker.ietf.org/doc/html/rfc7159)

```
JSON-text = ws value ws
```

中间valude我们先省略，首先首尾是有white space的，我们先跳过首部white space，对value进行解析

```c
while (isspace(*doc))
	doc++;
```

然后调用`ret = __parse_json_value(doc, &doc, 0, val);`, 这里就是对value的parse了，这也是我们json parser的核心

比起我们写编译原理，我们这里就简单很多了，不需要写分词器(tokenizer),只需检测第一个字符，便可以知道它是哪种类型的值

```
n ➔ null
t ➔ true
f ➔ false
" ➔ string
0-9/- ➔ number
[ ➔ array
{ ➔ object
```

```c
static int __parse_json_value(const char *cursor, const char **end,
							  int depth, json_value_t *val)
{
	int ret;

	switch (*cursor)
	{
	case '\"':
		// string

	case '-':
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		// number

	case '{':
		// object
	
	case '[':
		// array

	case 't':
		// true

	case 'f':
		// false

	case 'n':
		// null

	default:
		return -2;
	}

	return 0;
}
```

我们json一个整体肯定是一个object，因为总先看到一个`{`, 所有先进入`__parse_json_object`

关于object，我们之后再看，我们先只用知道，从`__parse_json_object`中，我们去解析内部的`members`, 调用`__parse_json_members`

注意，这里是`members`, 带一个 `s`, 因为一个object中可能有多个键值对，然后里面`__parse_json_member`去解析单个`member`

在 `:` 后面的解析中，解析kv中的value调用的也是`__parse_json_value`

所以说我们就是利用`__parse_json_value`来解析键值对的值

```c
static int __parse_json_value(const char *cursor, const char **end,
							  int depth, json_value_t *val)
{
	int ret;

	switch (*cursor)
	{
	...

	case 't':
		if (strncmp(cursor, "true", 4) != 0)
			return -2;

		*end = cursor + 4;
		val->type = JSON_VALUE_TRUE;
		break;

	case 'f':
		if (strncmp(cursor, "false", 5) != 0)
			return -2;

		*end = cursor + 5;
		val->type = JSON_VALUE_FALSE;
		break;

	case 'n':
		if (strncmp(cursor, "null", 4) != 0)
			return -2;

		*end = cursor + 4;
		val->type = JSON_VALUE_NULL;
		break;

	default:
		return -2;
	}

	return 0;
}
```

我们先看最简单的null和boolean，直接`strncmp`，就可以判定是否为什么类型了，然后cursor相应往后移

解析完之后

```c
// json_value_parse function
if (ret >= 0)
{
	while (isspace(*doc))
		doc++;

	if (*doc)
	{
		__destroy_json_value(val);
		ret = -2;
	}
}

if (ret < 0)
{
	free(val);
	return NULL;
}
```

我们还要检测后面的ws是否合理，所以先判断刚才解析是否正确，如果不成功直接释放返回null

如果成功了，我们看后面书否都是white space，如果不是，则destroy掉我们刚才解析的`json_value_t`

```c
static void __destroy_json_value(json_value_t *val)
{
	switch (val->type)
	{
	case JSON_VALUE_STRING:
		free(val->value.string);
		break;
	case JSON_VALUE_OBJECT:
		__destroy_json_members(&val->value.object);
		break;
	case JSON_VALUE_ARRAY:
		__destroy_json_elements(&val->value.array);
		break;
	}
}
```

根据类型不同，有不同的`destroy`方式。

这里就是parse的所有流程，非常简单。

解下来我们逐步分析下其他类型是如何解析。

## parse number

```
number = [ "-" ] int [ frac ] [ exp ]
int = "0" / digit1-9 *digit
frac = "." 1*digit
exp = ("e" / "E") ["-" / "+"] 1*digit
```

```c
// __parse_json_value
case '-':
case '0':
case '1':
case '2':
case '3':
case '4':
case '5':
case '6':
case '7':
case '8':
case '9':
	ret = __parse_json_number(cursor, end, &val->value.number);
	if (ret < 0)
		return ret;

	val->type = JSON_VALUE_NUMBER;
	break;
```

```c
static int __parse_json_number(const char *cursor, const char **end,
							   double *num)
{
	const char *p = cursor;

	if (*p == '-')
		p++;

	if (*p == '0' && (isdigit(p[1]) || p[1] == 'X' || p[1] == 'x'))
		return -2;

	*num = strtod(cursor, (char **)end);
	if (*end == cursor)
		return -2;

	return 0;
}
```

这里有必要说一下`strtod`，这个函数将C-string 转化为double类型。但是一些 JSON 不容许的格式，strtod() 也可转换，所以我们需要提前做格式校验。

具体参考[cpp ref](https://www.cplusplus.com/reference/cstdlib/strtod/)

```
for c99/c11

A valid floating point number for strtod using the "C" locale is formed by an optional sign character (+ or -), followed by one of:
- A sequence of digits, optionally containing a decimal-point character (.), optionally followed by an exponent part (an e or E character followed by an optional sign and a sequence of digits).
- A 0x or 0X prefix, then a sequence of hexadecimal digits (as in isxdigit) optionally containing a period which separates the whole and fractional number parts. Optionally followed by a power of 2 exponent (a p or P character followed by an optional sign and a sequence of hexadecimal digits).
- INF or INFINITY (ignoring case).
- NAN or NANsequence (ignoring case), where sequence is a sequence of characters, where each character is either an alphanumeric character (as in isalnum) or the underscore character (_).
```

我们如果先看到`-`,则跳过，如果后面的是0开头，后面还有数字, 则invalid，注意第二条`0x`, `0X` 也不满足要求，所以我们还要检查一下。

























