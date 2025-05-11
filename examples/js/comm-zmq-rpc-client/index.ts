import PendingRequests, { type TReqID } from "./pending-requests";
import Tracker from "@fict/utils/tracker";
import zmq, { Subscriber, Publisher, type MessageLike } from "zeromq";
import type { IStats } from "./stats";
import Stats from "./stats";

export interface TReqObj {
  method: string;
  params?: object;
  id?: TReqID;
}

export interface TStreamReqObj extends TReqObj {
  onStreamResponse: TStreamResponseCB;
}

export type TCancelCB = (bNoThrow: boolean) => void;
export type TStreamResponseCB = (data: any, cancelCB: TCancelCB) => void;

export interface IActiveStream {
  t: Tracker;
  onStreamResponse: TStreamResponseCB;
}

export interface IRPCError {
  code?: number;
  message?: string;
  title?: string;
  data?: any;
}

export interface IRPCResponse {
  jsonrpc: "2.0";
  id?: TReqID;
  result?: any;
  error?: IRPCError;
  stream?: {
    id: TReqID;
    data?: any;
  };
}

export interface IRPCRequest {
  jsonrpc: "2.0";
  id: TReqID;
  method: string;
  params?: object;
  options?: object;
}

export default class ZMQRPC_Client {
  m_Publisher: Publisher;
  m_Subscriber: Subscriber;
  m_pendingReq: PendingRequests = new PendingRequests();
  m_activeStreams: Map<TReqID, IActiveStream> = new Map();
  m_bShouldExit: boolean = false;
  m_stats: Stats = new Stats();

  constructor(pub: Publisher, sub: Subscriber) {
    this.m_Publisher = pub;
    this.m_Subscriber = sub;
    while (this.m_bShouldExit == false) {
      this.m_Subscriber.receive().then((messages: zmq.Message[]) => {
        messages.forEach((message) => {
          const response = JSON.parse(message.toString("utf-8")) as IRPCResponse;
          if (!response.id) {
            this._handleSSE(response, onNotification);
            return this.m_stats.onReceived(message);
          }
          const t = this.m_pendingReq.get(response.id);
          if (t) {
            response.error ? t.cancel(response.error) : t.finish(response.result);
            this.m_pendingReq.remove(response.id);
          } else {
            this.logger.log("Unexpected Reply from server: ", response);
          }
          this.m_stats.onReceived(message, t);
        });
      });
    }
  }

  close() {
    this.m_bShouldExit = true;
    this.m_Publisher.close();
    this.m_Subscriber.close();
  }

  sendRequest(id: TReqID, req: zmq.Message) {
    const t = new Tracker();
    this.m_pendingReq.add(id, t);
    return Promise.all([this.m_Publisher.send(req).then(() => this.m_stats.onSent(req)), t.p]);
  }

  cancelRequest(id: TReqID, bIgnoreResponse: boolean = false): void {
    if (!id) return;

    const t = this.m_pendingReq.get(id);
    if (!t) return;

    if (bIgnoreResponse) {
      this.m_pendingReq.remove(id);
    }

    this._send({ method: "rpc.cancel", id });
  }

  cancelStreamRequest(id: TReqID, bIgnoreResponse: boolean = false): void {
    if (!id) return;

    if (bIgnoreResponse) {
      this.m_activeStreams.delete(id);
    }

    this._send({ method: "rpc.cancel", id });
  }

  private _handleSSE(response: IRPCResponse, onNotification: (response: IRPCResponse) => void): void {
    if (!response.stream) {
      return onNotification(response);
    }

    const {
      stream: { id, data },
      result,
      error,
    } = response;
    const activeStream = id ? this.m_activeStreams.get(id) : undefined;

    if (!id || !activeStream) {
      return onNotification(response);
    }

    if (data) {
      return activeStream.onStreamResponse(data, (bNoThrow) => this.cancelStreamRequest(id, bNoThrow));
    }

    const t = activeStream.t;
    if (error) {
      t.cancel(error);
    } else if (result) {
      t.finish(result);
    } else {
      onNotification(response);
    }
  }
}

// export class ZMQRPC_PUB {
//     private readonly codec: typeof CBORX;
//     private rid: number;
//     private readonly pendingReq: PendingRequests;
//     private readonly activeStreams: Map<string | number, IActiveStream>;
//     private readonly stats: IStats;
//     private readonly logger: Console;

//     constructor(
//         url: string,
//         {
//             reqTimeOutMS,
//             codec = CBORX,
//             onNotification = () => {},
//             logger = console,
//         }: {
//             reqTimeOutMS?: number;
//             codec?: typeof CBORX;
//             onNotification?: (response: RPCResponse) => void;
//             logger?: Console;
//         } = {}
//     ) {
//         super(url);
//         this.binaryType = "arraybuffer";
//         this.codec = codec;
//         this.rid = 1;
//         this.pendingReq = new PendingRequests(reqTimeOutMS);
//         this.activeStreams = new Map();
//         this.stats = new Stats();
//         this.logger = logger;

//         this.addEventListener("message", (event: MessageEvent) => {
//             try {
//                 const response = this.codec.decode(
//                     new Uint8Array(event.data as ArrayBuffer)
//                 ) as RPCResponse;

//                 if (!response.id) {
//                     this._handleSSE(response, onNotification);
//                     return this.stats.onReceived(new Uint8Array(event.data as ArrayBuffer));
//                 }

//                 const t = this.pendingReq.get(response.id);
//                 if (t) {
//                     response.error
//                         ? t.cancel(response.error)
//                         : t.finish(response.result);
//                     this.pendingReq.remove(response.id);
//                 } else {
//                     this.logger.log("Unexpected Reply from server: ", response);
//                 }
//                 this.stats.onReceived(new Uint8Array(event.data as ArrayBuffer), t;
//             } catch (error) {
//                 this.logger.error("Error processing message:", error);
//             }
//         });
//     }

//     private _handleSSE(response: RPCResponse, onNotification: (response: RPCResponse) => void): void {
//         if (!response.stream) {
//             return onNotification(response);
//         }

//         const { stream: { id, data }, result, error } = response;
//         const activeStream = id ? this.activeStreams.get(id) : undefined;

//         if (!id || !activeStream) {
//             return onNotification(response);
//         }

//         if (data) {
//             return activeStream.onStreamResponse(data, (bNoThrow) =>
//                 this.cancelStreamRequest(id, bNoThrow)
//             );
//         }

//         const t = activeStream.t;
//         if (error) {
//             t.cancel(error);
//         } else if (result) {
//             t.finish(result);
//         } else {
//             onNotification(response);
//         }
//     }

//     private _send(props: TReqObj & { jsonrpc?: string }): void {
//         if (this.readyState !== WebSocket.OPEN) {
//             throw new Error("WebSocket is not open");
//         }

//         const buf = this.codec.encode({ jsonrpc: "2.0", ...props });
//         super.send(buf);
//         this.stats.onSent(buf);
//     }

//     sendRequest(req: TReqObj): Promise<any> {
//         const id = req.id || this.rid++;
//         const t = new Tracker();

//         this.pendingReq.add(id, t);
//         this._send({ ...req, id });

//         return t.p;
//     }

//     sendStreamRequest(req: TStreamReqObj): Promise<any> {
//         const id = req.id || this.rid++;
//         const t = new Tracker();

//         this.activeStreams.set(id, { t, onStreamResponse: req.onStreamResponse });
//         this._send({ ...req, id, options: { stream: true } });

//         return t.p;
//     }

//     cancelRequest(id: string | number, bIgnoreResponse: boolean = false): void {
//         if (!id) return;

//         const t = this.pendingReq.get(id);
//         if (!t) return;

//         if (bIgnoreResponse) {
//             this.pendingReq.remove(id);
//         }

//         this._send({ method: "rpc.cancel", id });
//     }

//     cancelStreamRequest(id: string | number, bIgnoreResponse: boolean = false): void {
//         if (!id) return;

//         if (bIgnoreResponse) {
//             this.activeStreams.delete(id);
//         }

//         this._send({ method: "rpc.cancel", id });
//     }
// }
