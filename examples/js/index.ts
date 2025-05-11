import zmq, { Subscriber } from "zeromq";

// Message types matching C++ enum
const MessageType = {
  AUDIO: 0,
  VIDEO: 1,
  CONTROL: 2,
};

// Create PUB socket
const publisher = new zmq.Publisher({
  noDrop: true,
  sendTimeout: -1,
  linger: 0,
  immediate: true
});

// Long-living sockets Bind, Short-living sockets connect.
// In this case, this app is short-living so, just connect.
await publisher.connect("tcp://localhost:5555");
console.log("Started work producer on tcp://localhost:5555");

// Listen to the notifications, acks from the Service
const subsciber = new zmq.Subscriber({
  linger: 0,
  immediate: true,
});
await subsciber.connect("tcp://localhost:5556");
subsciber.subscribe(""); // subscribe to all


function createRequest(reqId: bigint, methodId: number) {
  const buf = Buffer.allocUnsafe(9); // Must be exactly 9 bytes
  buf.writeBigUInt64LE(reqId, 0);
  buf.writeUInt8(methodId, 8);
  return buf;
}

// sanity-checks
const msg = createRequest(BigInt(0x123456789abcdef0), 0x01);
console.assert(msg.length === 9, "Message buffer must be exactly 9 bytes");

function

/**
 * TODO: implement wait for the ack from the sub for the messages received.
 * With Pub/Sub there is no reliable way to guarantee the message delivery.
 * Pub silently drops the messages, but returns success for send().
 * We have Wait for the Sub to acknowledge the message (in JSONRPC kind of style).
 */

let nCounter = 0, bIsWaitingForReceive = false;
// Send messages periodically
setInterval(async () => {
  nCounter++;
  try {
    // pipeline start
    let id = (BigInt(Date.now()) << 16n) | BigInt(Math.floor(Math.random() * 0xffffffff));
    const req1 = createRequest(id, 0);
    await publisher
      .send(req1)
      .then(() => console.log(`Sent ${req1}: ${id}: ${id.toString(16)}`))
      .catch((ex) => console.error(`Failed to send ${req1}`));
    
    // Audio message: 1-byte type + 4-byte sample_rate + data
  //   const aMsg = `audio data ${nCounter}`;
  //   const audioBuf = Buffer.alloc(1 + 4 + 10);
  //   audioBuf.writeUInt8(MessageType.AUDIO, 0);
  //   audioBuf.writeInt32BE(44100, 1); // Sample rate
  //   audioBuf.write(aMsg, 5);
  //   await publisher
  //     .send(audioBuf)
  //     .then(() => console.log(`Sent ${aMsg}`))
  //     .catch((ex) => console.error(`Failed to send ${aMsg}`));

  //   // Video message: 1-byte type + 4-byte width + 4-byte height + data
  //   const vMsg = `video data ${nCounter}`;
  //   const videoBuf = Buffer.alloc(1 + 4 + 4 + 10);
  //   videoBuf.writeUInt8(MessageType.VIDEO, 0);
  //   videoBuf.writeInt32BE(1920, 1); // Width
  //   videoBuf.writeInt32BE(1080, 5); // Height
  //   videoBuf.write(vMsg, 9);
  //   await publisher
  //     .send(videoBuf)
  //     .then(() => console.log(`Sent ${vMsg}`))
  //     .catch((ex) => console.error(`Failed to send ${vMsg}`));

  //   // Control message: 1-byte type + command
  //   const controlBuf = Buffer.from([MessageType.CONTROL, ...Buffer.from(`play ${nCounter}`)]);
  //   publisher
  //     .send(controlBuf)
  //     .then(() => console.log(`Sent Control ${nCounter}`))
  //     .catch((ex) => console.error(`Failed to send Control ${nCounter}`));
  } catch (err: any) {
    console.error("Error", err.message);
  }

  // Check for incoming messages
  // if (!bIsWaitingForReceive) {
  //   bIsWaitingForReceive = true;
  //   const x = await subsciber.receive().then(() => {
  //     bIsWaitingForReceive = false;
  //   });
  //   console.log("received: ", x);
  // }

}, 1000);

  for await (const msg of subsciber) {
    console.log("Received message:", msg.toString());
  }

const shutdown = () => {
  publisher.close();
  subsciber.close();
  console.log("Publisher closed");
  process.exit(0);  
}

// Handle cleanup on exit
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
