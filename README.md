# sqs-dispatch

Pulls messages from SQS and executes the specified command

## Build

```
$ make
```

## Run

```
$  ./build/make/sqs-dispatch
  --help                    show the help message
  -E [ --endpoint-url ] arg endpoint URL
  -Q [ --queue-url ] arg    sqs queue URL (required)
  -e [ --exec ] arg         exec arguments. Use {}.messageId and {}.body to get
                            the message.
```

## Example

For local development, you can use `localstack` to simulate an AWS environment.

```
# Start localstack
$ localstack start

# Make a fake account for localstack
$ aws configure --profile localstack
AWS Access Key ID [None]: 123
AWS Secret Access Key [None]: 456
Default region name [None]: us-east-1
Default output format [None]:
```

You can then create some SQS queues
```
$ aws sqs --endpoint-url http://localhost:4566 --profile localstack create-queue --queue-name example-queue
{
    "QueueUrl": "http://localhost:4566/000000000000/example-queue"
}
```

You can then send some messages to the queue
```
$ aws sqs --endpoint-url http://localhost:4566 --profile localstack send-message --queue-url "http://localhost:4566/000000000000/example-queue" --message-body "Message 1"
{
    "MD5OfMessageBody": "68390233272823b7adf13a1db79b2cd7",
    "MessageId": "acff6ac6-745f-4b4c-9516-470c19df636b"
}
$ aws sqs --endpoint-url http://localhost:4566 --profile localstack send-message --queue-url "http://localhost:4566/000000000000/example-queue" --message-body "Message 2"
{
    "MD5OfMessageBody": "88ef8f31ed540f1c4c03d5fdb06a7935",
    "MessageId": "6f85497f-a2a6-4202-bbc2-9fa54e9e3bfc"
}
```

When you run sqs-dispatch
```
$ ./build/make/sqs-dispatch -E http://localhost:4956 -Q "http://localhost:4566/000000000000/example-queue" -e "echo" -e "I got a message: " -e "{}.messageId" -e "{}.body"
app: 3.01278s
Received message:
  MessageId: acff6ac6-745f-4b4c-9516-470c19df636b
  ReceiptHandle: ZDc2ZGY1NGItY2VjZC00NzUzLTkyOGUtMGY0MzlmNWE3MDI2IGFybjphd3M6c3FzOnVzLWVhc3QtMTowMDAwMDAwMDAwMDA6ZXhhbXBsZS1xdWV1ZSBhY2ZmNmFjNi03NDVmLTRiNGMtOTUxNi00NzBjMTlkZjYzNmIgMTY4NTg1NDc2MS4yMDM3MDE3
  Body: Message 1

Received message:
  MessageId: 6f85497f-a2a6-4202-bbc2-9fa54e9e3bfc
  ReceiptHandle: ZGZhMGE5ZTctNmFkNi00YWI0LThiNzYtZWQzOTJjMGE0NjBkIGFybjphd3M6c3FzOnVzLWVhc3QtMTowMDAwMDAwMDAwMDA6ZXhhbXBsZS1xdWV1ZSA2Zjg1NDk3Zi1hMmE2LTQyMDItYmJjMi05ZmE1NGU5ZTNiZmMgMTY4NTg1NDc2MS4yMDM3OTY0
  Body: Message 2

cmd returned I got a message: 6f85497f-a2a6-4202-bbc2-9fa54e9e3bfc Message 2

cmd returned I got a message: acff6ac6-745f-4b4c-9516-470c19df636b Message 1

ready idx = 0
ret 0 = 1
ret 1 = 1
receive: 0.023598s
No messages received from queue http://localhost:4566/000000000000/example-queue
receive: 0.0114496s
No messages received from queue http://localhost:4566/000000000000/example-queue
receive: 0.0110188s
No messages received from queue http://localhost:4566/000000000000/example-queue
receive: 0.00908429s
No messages received from queue http://localhost:4566/000000000000/example-queue
receive: 0.00987379s
```