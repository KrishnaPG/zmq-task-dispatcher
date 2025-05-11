import zmq, { Subscriber } from "zeromq";
import ZMQRPC_Client from "./comm-zmq-rpc-client";

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
  immediate: true,
});
// Long-living sockets Bind, Short-living sockets connect.
// In this case, this app is short-living so, just connect.
await publisher.connect("tcp://localhost:5555");

// Listen to the notifications, acks from the Service
const subsciber = new zmq.Subscriber({
  linger: 0,
  immediate: true,
});
await subsciber.connect("tcp://localhost:5556");
subsciber.subscribe(""); // subscribe to all

console.log("Started work producer on tcp://localhost:5555");
console.log("Listening result receiver on tcp://localhost:5556");

const rpcClient = new ZMQRPC_Client(publisher, subsciber);

function createRequest(reqId: bigint, methodId: number) {
  const buf = Buffer.allocUnsafe(9); // Must be exactly 9 bytes
  buf.writeBigUInt64LE(reqId, 0);
  buf.writeUInt8(methodId, 8);
  return buf;
}

// sanity-checks
const msg = createRequest(BigInt(0x123456789abcdef0), 0x01);
console.assert(msg.length === 9, "Message buffer must be exactly 9 bytes");

/**
 * With Pub/Sub there is no reliable way to guarantee the message delivery.
 * Pub silently drops the messages, but returns success for send().
 * We have Wait for the Sub to acknowledge the message (in JSONRPC kind of style).
 */

let nCounter = BigInt(0),
  bIsWaitingForReceive = false;
// Send messages periodically
setInterval(async () => {
  nCounter++;
  try {
    // pipeline start
    let id = nCounter; //(BigInt(Date.now()) << 16n) | BigInt(Math.floor(Math.random() * 0xffffffff));
    const req1 = createRequest(id, 0);
    console.log(`Sending req ${req1}: ${id}: ${id.toString(16)}`);

    await rpcClient
      .sendRequest(id, req1)
      .then((results) => console.log(`received for ${id}: ${id.toString(16)} ${JSON.stringify(results)}`))
      .catch((ex) => console.error(`Error with ${req1}: ${id}: ${id.toString(16)}, ${ex.message}`));

  } catch (err: any) {
    console.error("Error", err.message);
  }
  
}, 1000);

const shutdown = () => {
  rpcClient.close();
  console.log("RPCClient closed");
  process.exit(0);
};

// Handle cleanup on exit
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
