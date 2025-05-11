import now from "@fict/utils/now";
import Tracker from "@fict/utils/tracker";
import { RPCError, ErrorFmt } from "@fict/utils/jsonrpc";

export type TReqID = string | bigint;

export default class PendingRequests {
  private q: Map<TReqID, Tracker>;
  private timeoutMS: number;
  private timer: NodeJS.Timeout | null;

  constructor(timeoutMS: number = 30000) {
    this.q = new Map();
    this.timeoutMS = timeoutMS;
    this.timer = null;
  }

  add(id: TReqID, t: Tracker): void {
    this.q.set(id, t);
    if (!this.timer) {
      this.timer = setInterval(() => this.clearExpiredRequests(), 250 + this.timeoutMS / 3);
    }
  }

  get(id: TReqID): Tracker | undefined {
    return this.q.get(id);
  }

  remove(id: TReqID): void {
    this.q.delete(id);
    if (this.q.size <= 0 && this.timer) {
      clearInterval(this.timer);
      this.timer = null;
    }
  }

  private clearExpiredRequests(): void {
    const currentTime = now();

    for (const [id, t] of this.q) {
      if (t.timeSpent() < this.timeoutMS) {
        break; // Subsequent requests will be newer (Map maintains insertion order)
      }

      t.cancel(RPCError(ErrorFmt.timeout("Request timed out"), id));
      this.remove(id);
    }
  }
}
