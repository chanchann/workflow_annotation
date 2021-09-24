## 通信大致流程

```
handle
├── Communicator::handle_incoming_request     [vim src/kernel/Communicator.cc +541]
├── Communicator::handle_incoming_reply       [vim src/kernel/Communicator.cc +619]
├── Communicator::handle_reply_result [vim src/kernel/Communicator.cc +712]
├── Communicator::handle_request_result       [vim src/kernel/Communicator.cc +774]
├── Communicator::handle_connect_result       [vim src/kernel/Communicator.cc +930]
├── Communicator::handle_sleep_result [vim src/kernel/Communicator.cc +1031]
├── Communicator::handle_aio_result   [vim src/kernel/Communicator.cc +1044]
├── Communicator::reply       [vim src/kernel/Communicator.cc +1641]
├── IOService::decref [vim src/kernel/IOService_thread.cc +114]
├── IOService::decref [vim src/kernel/IOService_linux.cc +286]
├── Executor::executor_thread_routine [vim src/kernel/Executor.cc +76]
├── Executor::executor_cancel_tasks   [vim src/kernel/Executor.cc +102]
└── handle    [vim src/factory/HttpTaskImpl.cc +840]
```

发来消息，则handle
