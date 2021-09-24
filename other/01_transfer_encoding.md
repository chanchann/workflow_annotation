## transfer encoding

1. Transfer-Encoding vs Content-Encoding

改变报文格式(可能变大)  vs 压缩编码优化传输

2. When use Content-Encoding

例如用 gzip 压缩文本文件，能大幅减小体积。内容编码通常是选择性的，例如 jpg / png 这类文件一般不开启，因为图片格式已经是高度压缩过的

3. Why use Transfer-Encoding

With chunked transfer encoding (CTE), the encoder sends data to the player in a series of chunks instead of waiting until the complete segment is availabl

4. Persistent Connection

HTTP/1.0 的持久连接机制是后来才引入的，通过 Connection: keep-alive 这个头部来实现，服务端和客户端都可以使用它告诉对方在发送完数据之后不需要断开 TCP 连接，以备后用。HTTP/1.1 则规定所有连接都必须是持久的，除非显式地在头部加上 Connection: close

Why mention Persistent Connection here ?

So we need Content-Length.

5. Why we need Content-Length.

6. If Content-Length != message length

7. Content-Length leads to what problem, how to solve it?

8. Transfer-Encoding: chunked

The transmission ends when a zero-length chunk is received.

```
HTTP/1.1 200 OK 
Content-Type: text/plain
Transfer-Encoding: chunked
5\r\n
Media\r\n
8\r\n
Services\r\n
4\r\n
Live\r\n
0\r\n
\r\n
```

## ref 

https://imququ.com/post/transfer-encoding-header-in-http.html

https://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html

https://learn.akamai.com/en-us/webhelp/media-services-live/media-services-live-encoder-compatibility-testing-and-qualification-guide-v4.0/GUID-A7D10A31-F4BC-49DD-92B2-8A5BA409BAFE.html

https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Transfer-Encoding