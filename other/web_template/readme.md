## web template

template 还是很常见的，但是与以往不太一样了

以前的 template 是直接把处理好的数据塞进 html 里生成视图丢给客户端

现在的 template 仅用于一些不方便通过二次请求拿的数据，服务端会直接通过 template 塞到 html 里

比如私有化部署、海内外环境区分啥的，有的身份凭证也会通过 template 植入
