import PendingRequests, { type TReqID } from "./pending-requests";
import Tracker from "@fict/utils/tracker";
import zmq, { Subscriber, Publisher, type MessageLike } from "zeromq";
import type { IStats } from "./stats";
import Stats from "./stats";

export interface TReqObj {
  id: TReqID;
  method: string;
  params?: object;
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
  id?: TReqID; // when stream is present, this may be undefined or null
  ack?: boolean | object; // when ack is true, result will come later
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
  m_logger: Console;

  constructor(
    pub: Publisher,
    sub: Subscriber,
    logger: Console = console,
    onNotification: (response: IRPCResponse) => void = () => {}
  ) {
    this.m_Publisher = pub;
    this.m_Subscriber = sub;
    this.m_logger = logger;
    this.listen_to_server(onNotification); // start listening to replies
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

    //this._send({ method: "rpc.cancel", id });
  }

  cancelStreamRequest(id: TReqID, bIgnoreResponse: boolean = false): void {
    if (!id) return;

    if (bIgnoreResponse) {
      this.m_activeStreams.delete(id);
    }

    //this._send({ method: "rpc.cancel", id });
  }

  private async listen_to_server(onNotification: (response: IRPCResponse) => void) {
    while (this.m_bShouldExit == false) {
      await this.m_Subscriber
        .receive()
        .then((messages: zmq.Message[]) => {
          messages.forEach((message) => {
            const response = JSON.parse(message.toString("utf-8")) as IRPCResponse;

            if (!response.id) {
              // this is either a stream or server-notification
              this._handleSSE(response, onNotification);
              return this.m_stats.onReceived(message);
            }

            // this is either an ack, error or result
            const t = this.m_pendingReq.get(BigInt(response.id));
            if (t) {
              this.m_pendingReq.remove(response.id);
              if (response.error)
                // some error happened, no result/ack fields will be available
                t.cancel(response.error);
              else if (response.ack) {
                // this is acknowledgement, real result will come later
                this.m_pendingReq.add(response.id, t); // reinsert into Q at the end
                this.m_logger.debug(`Ack received for ${response.id}`);
              }
              // no error, not an ack - consider this as result
              else t.finish(response.result);
            } else {
              this.m_logger.log("Unexpected Reply from server: ", response);
            }

            // record the stats
            this.m_stats.onReceived(message, t);
          });
        })
        .catch((ex) => this.m_logger.error(ex.message));
    }
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
