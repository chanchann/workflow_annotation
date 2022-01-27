## CORS

CORS由一系列传输的HTTP头组成，这些HTTP头有两个作用，

1：用于阻止还是允许浏览器向其他域名发起请求；

2：用于接受还是拒绝其他域名返回的响应数据；

跨域是浏览器正确做出请求，服务器也能正确做出响应，是浏览器拒绝接收跨域服务器返回的数据

###

很多人也认为使用CORS解决跨域很简单，只需要在服务器添加响应头 “ Access-Control-Allow-Origin ：* ” 就可以了，

在CORS中，所有的跨域请求被分为了两种类型，一种是简单请求，一种是复杂请求 (严格来说应该叫‘需预检请求’)；

简单请求与普通的ajax请求无异；

但复杂请求，必须在正式发送请求前先发送一个OPTIONS方法的请求已得到服务器的同意，若没有得到服务器的同意，浏览器不会发送正式请求；

## Conclusion

出于安全性的考虑，浏览器引入同源策略;

CORS本质上就是使用各种头信息来是浏览器与服务器之间进行身份认证实现跨域数据共享；

CORS中最常使用的响应头为 Access-Control-Allow-Origin、Access-Control-Allow-Headers、Access-Control-Expose-Headers;

CORS中最常使用的请求头为 Origin、Access-Control-Request-Headers、Access-Control-Request-Method;

## 简单类型的请求

1：请求方法必须是 GET、HEAD、POST中的一种，其他方法不行；

2：请求头类型只能是 Accept、Accept-Language、Content-Language、Content-Type，添加其他额外请求头不行；

3：请求头 Content-Type 如果有，值只能是 text/plain、multipart/form-data、application/x-www-form-urlencoded 中的一种，其他值不行；

4：请求中的任意 XMLHttpRequestUpload 对象均没有注册任何事件监听器；

5：请求中没有使用 ReadableStream 对象

## ref

https://zhuanlan.zhihu.com/p/53996160

https://www.ruanyifeng.com/blog/2016/04/cors.html

