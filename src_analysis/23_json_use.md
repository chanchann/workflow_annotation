# Workflow 源码解析 Json parser ：part2 使用

项目地址 : https://github.com/Barenboim/json-parser

我们在part1中我们讲了如何解析文本，这次我们先使用起来，这节看官方的demo : test.c

使用: `./parse_json < xxx.json`

先看主逻辑，main函数

```c
#define BUFSIZE		(64 * 1024 * 1024)

int main()
{
	static char buf[BUFSIZE];
	// 我们把文件重定向到了stdin中，从其中读到buf里去
	// 最大大小为64mb
	size_t n = fread(buf, 1, BUFSIZE, stdin);

	if (n > 0)
	{
		if (n == BUFSIZE)
		{
			fprintf(stderr, "File too large.\n");
			exit(1);
		}

		buf[n] = '\0';
	}
	else
	{
		perror("fread");
		exit(1);
	}

	// 然后我们内容解析，解析部分就是part1所讲
	json_value_t *val = json_value_parse(buf);
	// 我们成功就打印然后销毁
	if (val)
	{
		print_json_value(val, 0);
		json_value_destroy(val);
	}
	else
		fprintf(stderr, "Invalid JSON document.\n");

	return 0;
}
```

两个重要的接口

```
函数签名 : json_value_t *json_value_parse(const char *doc);
描述:
解析JSON文本(const char*)为一个Json value(json_value_t *).
如果失败(非法json， 嵌套太深， 内存分配错误)则返回NULL
```

我们解析出来的json_value_t *，还有一个销毁的接口

```
函数签名 : void json_value_destroy(json_value_t *val);
描述:
销毁JSON value
```

## json_value_destroy

```c
void json_value_destroy(json_value_t *val)
{
	__destroy_json_value(val);
	free(val);
}
```

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

根据不同的类型，有不同的数据结构，自然有不同的销毁方式

```c
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

1. string

string就是char *，我们直接简单的`free(val->value.string);`即可

2. object

```c
struct __json_object
{
	struct list_head head;
	struct rb_root root;
	int size;
};
```

他既挂在了list上，又挂在了rb tree上，我们遍历list每一个member

从list和rb tree上删除，然后去销毁member(嵌套的结构)并释放内存

我们知道，member是一个kv结构，key是一个柔性数组，所以key的大小是连同`__json_member`一起被分配的，所以也是随着`free(memb)`而销毁

因为value又是json value，所以我们深入递归进入嵌套去销毁

```c
struct __json_member
{
	struct list_head list;
	struct rb_node rb;
	json_value_t value;
	char name[1];
};
```

```c
static void __destroy_json_members(json_object_t *obj)
{
	struct list_head *pos, *tmp;
	json_member_t *memb;

	list_for_each_safe(pos, tmp, &obj->head)
	{
		memb = list_entry(pos, json_member_t, list);
		list_del(pos);
		rb_erase(&memb->rb, &obj->root);
		// 递归进入嵌套销毁
		__destroy_json_value(&memb->value);
		free(memb);
	}
}
```

3. array

```c
struct __json_array
{
	struct list_head head;
	int size;
};
```

array只是一个链表结构，所以比object更为简单，直接遍历去销毁

```c
static void __destroy_json_elements(json_array_t *arr)
{
	struct list_head *pos, *tmp;
	json_element_t *elem;

	list_for_each_safe(pos, tmp, &arr->head)
	{
		elem = list_entry(pos, json_element_t, list);
		// todo : 此处为何不list_del
		__destroy_json_value(&elem->value);
		free(elem);
	}
}
```

## json_value_type

```
函数签名: int json_value_type(const json_value_t *val);
描述:
获取json value的类型
JSON_VALUE_STRING: string
JSON_VALUE_NUMBER: number
JSON_VALUE_OBJECT: JSON object
JSON_VALUE_ARRAY: JSON array
JSON_VALUE_TRUE: true
JSON_VALUE_FALSE: false
JSON_VALUE_NULL: null
```

```c
struct __json_value
{
	int type;
	...
};
```

value专门有个字段记录类型

所以我们只需要取出type即可

```c
int json_value_type(const json_value_t *val)
{
	return val->type;
}
```

## print_json_value

我们格式化打印，根据不同的类型也有不同的输出

```c
void print_json_value(const json_value_t *val, int depth)
{
	switch (json_value_type(val))
	{
	case JSON_VALUE_STRING:
		print_json_string(json_value_string(val));
		break;
	case JSON_VALUE_NUMBER:
		print_json_number(json_value_number(val));
		break;
	case JSON_VALUE_OBJECT:
		print_json_object(json_value_object(val), depth);
		break;
	case JSON_VALUE_ARRAY:
		print_json_array(json_value_array(val), depth);
		break;
	case JSON_VALUE_TRUE:
		printf("true");
		break;
	case JSON_VALUE_FALSE:
		printf("false");
		break;
	case JSON_VALUE_NULL:
		printf("null");
		break;
	}
}
```

1. string

```
函数签名: const char *json_value_string(const json_value_t *val);
描述: 
获取Json string
如果val不是JSON_VALUE_STRING类型，那么就返回NULL
```

```c
const char *json_value_string(const json_value_t *val)
{
	if (val->type != JSON_VALUE_STRING)
		return NULL;

	return val->value.string;
}
```

这个逻辑很简单，我们看看如何打印

```c
void print_json_string(const char *str)
{
	printf("\"");
	while (*str)
	{
		switch (*str)
		{
		case '\r':
			printf("\\r");
			break;
		case '\n':
			printf("\\n");
			break;
		case '\f':
			printf("\\f");
			break;
		case '\b':
			printf("\\b");
			break;
		case '\"':
			printf("\\\"");
			break;
		case '\t':
			printf("\\t");
			break;
		case '\\':
			printf("\\\\");
			break;
		default:
			printf("%c", *str);
			break;
		}
		str++;
	}
	printf("\"");
}
```

其实就是两点，把前后的`"`打印和转义符打印出来。

2. number

```
函数签名: double json_value_number(const json_value_t *val);
描述: 
获取Json number
如果val不是JSON_VALUE_NUMBER类型，那么就返回NAN
```

```c
double json_value_number(const json_value_t *val)
{
	if (val->type != JSON_VALUE_NUMBER)
		return NAN;

	return val->value.number;
}
```

```c
void print_json_number(double number)
{
	long long integer = number;

	if (integer == number)
		printf("%lld", integer);
	else
		printf("%lf", number);
}
```

这里是一个有趣的trick, 可以判断这个number是不是有小数

3. object

```
函数签名: json_object_t *json_value_object(const json_value_t *val);
描述: 
获取Json object
如果val不是JSON_VALUE_OBJECT类型，那么就返回NULL
```

```c
json_object_t *json_value_object(const json_value_t *val)
{
	if (val->type != JSON_VALUE_OBJECT)
		return NULL;

	return (json_object_t *)&val->value.object;
}
```

这里有个参数depth，代表嵌套的深度，因为json是可以嵌套的结构

我们看看打印效果再来看代码

```
{"1" : {"2" : {"3" : null}}, "2" : true}

==> 打印

{
    "1": {
        "2": {
            "3": null
        }
    },
    "2": true
}
```

```c
void print_json_object(const json_object_t *obj, int depth)
{·
	const char *name;
	const json_value_t *val;
	int n = 0;
	int i;

	printf("{\n");
	json_object_for_each(name, val, obj)
	{
		if (n != 0)
			printf(",\n"); // 可见 "2": true , 第一个元素外，都要打个`,`然后提行
		n++;
		for (i = 0; i < depth + 1; i++)
			printf("    ");    // 深入一层，那就多一个\t的距离，表示层次感
		printf("\"%s\": ", name);
		print_json_value(val, depth + 1);   // 递归进去解析
	}

	printf("\n");
	for (i = 0; i < depth; i++)
		printf("    ");
	printf("}");
}
```

这里还有个比较重要的宏来遍历object

```c
#define json_object_for_each(name, val, obj) \
	for (name = NULL, val = NULL; \
		 name = json_object_next_name(name, obj), \
		 val = json_object_next_value(val, obj), val; )
```

就是不断找下一个kv对

4. array

```
函数签名: json_array_t *json_value_array(const json_value_t *val);
描述: 
获取Json array
如果val不是JSON_VALUE_ARRAY类型，那么就返回NULL
```

```c
json_array_t *json_value_array(const json_value_t *val)
{
	if (val->type != JSON_VALUE_ARRAY)
		return NULL;

	return (json_array_t *)&val->value.array;
}
```

我们可以先看看效果

```
[1,2,[3,4,5, [6,7]],null]

==> 打印

[
    1,
    2,
    [
        3,
        4,
        5,
        [
            6,
            7
        ]
    ],
    null
]
```

array也可以嵌套，和object一样，更为简单

```c
void print_json_array(const json_array_t *arr, int depth)
{
	const json_value_t *val;
	int n = 0;
	int i;

	printf("[\n");
	json_array_for_each(val, arr)
	{
		if (n != 0)
			printf(",\n");
		n++;
		for (i = 0; i < depth + 1; i++)
			printf("    ");
		print_json_value(val, depth + 1);
	}

	printf("\n");
	for (i = 0; i < depth; i++)
		printf("    ");
	printf("]");
}
```