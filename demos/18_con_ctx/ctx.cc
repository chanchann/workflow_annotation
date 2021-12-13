/*
https://github.com/sogou/workflow/blob/master/docs/about-connection-context.md

需要维护连接状态的,我们需要把一段上下文和连接绑定。

http协议可以说是一种完全无连接状态的协议，http会话，是通过cookie来实现的

一般情况下只有server任务需要使用连接上下文，并且只需要在process函数内部使用，这也是最安全最简单的用法

任务在callback里也可以使用或修改连接上下文，只是使用的时候需要考虑并发的问题
*/


